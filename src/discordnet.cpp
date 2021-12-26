#include <array>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "cidr.h"

#include "guidparse.h"
#include "adapter.h"

// wtf Microsoft?
#ifdef SendMessage
#undef SendMessage
#endif

#include "discordnet.h"

using namespace lpvpn;
using namespace lpvpn::discordnet;

enum channel {
	data = 0,
	aux
};

bool operator<(const struct in_addr &a, const struct in_addr &b)
{
	return a.s_addr < b.s_addr;
}

struct IPMapping {
	cidr::CIDR baseAddr;
	std::vector<discord::NetworkPeerId> peers;
	std::vector<char> isEmpty;
	std::vector<uint32_t> freeList;
	std::unordered_map<discord::NetworkPeerId, uint32_t> ips;

	IPMapping(cidr::CIDR baseAddr) : baseAddr(baseAddr) {}

	void clear() {
		peers.clear();
		isEmpty.clear();
		freeList.clear();
		ips.clear();
	}

	bool hasPeer(discord::NetworkPeerId peer) {
		return ips.contains(peer);
	}

	std::pair<cidr::CIDR, bool> getIP(discord::NetworkPeerId peer) {
		assert(peers.size() == isEmpty.size());

		auto it = ips.find(peer);
		if (it != ips.end())
			return { baseAddr + it->second, false };

		if (freeList.empty()) {
			freeList.push_back(peers.size());
			peers.emplace_back();
			isEmpty.push_back(true);
		}
		auto pos = freeList.back();
		freeList.pop_back();
		peers[pos] = peer;
		isEmpty[pos] = false;
		ips.emplace_hint(it, peer, pos);
		return { baseAddr + pos, true };
	}

	std::optional<discord::NetworkPeerId> getPeer(struct in_addr ip) {
		return getPeer(cidr::CIDR{ ip, 0 });
	}

	std::optional<discord::NetworkPeerId> getPeer(cidr::CIDR ip) {
		assert(peers.size() == isEmpty.size());
		auto pos = ip - baseAddr;
		return (pos >= peers.size() || isEmpty[pos]) ? std::nullopt : std::optional(peers[pos]);
	}

	bool setPeer(discord::NetworkPeerId peer, cidr::CIDR ip) {
		bool hasPeer = ips.contains(peer);
		auto pos = ip - baseAddr;
		// Check if too many peers, something may be wrong
		if (pos > 128)
			return hasPeer;
		while (peers.size() < pos) {
			freeList.push_back(peers.size());
			peers.emplace_back();
			isEmpty.push_back(true);
		}
		if (peers.size() == pos) {
			peers.emplace_back();
			isEmpty.push_back(true);
		}
		peers[pos] = peer;
		isEmpty[pos] = false;
		ips.emplace(peer, pos);
		return hasPeer;
	}

	void removePeer(discord::NetworkPeerId peer) {
		assert(peers.size() == isEmpty.size());

		auto it = ips.find(peer);
		if (it == ips.end())
			return;

		auto pos = it->second;
		ips.erase(it);
		isEmpty[pos] = true;
		if (pos == peers.size() - 1) {
			while (isEmpty[isEmpty.size() - 1]) {
				isEmpty.pop_back();
				peers.pop_back();
			}
		} else {
			freeList.push_back(pos);
		}
	}

	template <typename Func>
	void forEachPeer(Func&& func) {
		assert(peers.size() == isEmpty.size());
		for (size_t i = 0; i < peers.size(); i++) {
			if (isEmpty[i])
				continue;
			func(peers[i]);
		}
	}
};


void DiscordNet::run(std::stop_token token)
{
	std::unique_ptr<lpvpn::adapter::Adapter> adapter = nullptr;

	hostCidr = hostCidr.getMasked() + 1;
	IPMapping ipMapping(hostCidr);

	connection = Connection::Disconnected;

	discord::NetworkPeerId peerId;
	std::string route;

	discord::Lobby lobby;
	discord::User user;
	discord::Core *_core{};
	std::shared_ptr<discord::Core> core;
	auto result = discord::Core::Create(DISCORD_APP_ID, DiscordCreateFlags_NoRequireDiscord, &_core);
	if (result != discord::Result::Ok) {
		std::lock_guard lg(mutex);
		exception = std::runtime_error("Cannot instantiate discord service. Make sure discord is running.");
		return;
	}
	core.reset(_core);

	auto &lm = core->LobbyManager();
	auto &um = core->UserManager();
	auto &am = core->ActivityManager();
	auto &om = core->OverlayManager();
	auto &nm = core->NetworkManager();

	nm.GetPeerId(&peerId); // get peer id.

	discord::Activity activity{};
	activity.SetApplicationId(DISCORD_APP_ID);

	// Refresh the ipMapping
	auto refreshIPs = [&]() {
		if (connection < Connection::WaitingForIP)
			return;
		ipMapping.clear();
		int32_t memberCount = 0;
		auto lobbyId = lobby.GetId();
		lm.MemberCount(lobbyId, &memberCount);
		for (int32_t i = 0; i < memberCount; i++) {
			discord::UserId dstUserId = 0;
			lm.GetMemberUserId(lobby.GetId(), i, &dstUserId);

			char buffer[4096] = { 0 };
			lm.GetMemberMetadataValue(lobbyId, dstUserId, "$net.peer_id", buffer);
			std::string memberPeerIdStr(buffer);
			memset(buffer, 0, sizeof(buffer));
			lm.GetMemberMetadataValue(lobbyId, dstUserId, "$net.ipv4", buffer);
			std::string memberIPStr(buffer);

			if (memberPeerIdStr.empty() || memberIPStr.empty()) {
				// peer not ready
				continue;
			}

			discord::NetworkPeerId memberPeerId = std::stoull(memberPeerIdStr);
			auto memberIP = cidr::parse(memberIPStr.c_str());
			if (memberIP.addr.S_un.S_addr == 0)
				continue;
			ipMapping.setPeer(memberPeerId, memberIP);
		}
	};

	// Handle the update of a peer
	auto updatePeer = [&](discord::LobbyId lobbyId, discord::UserId userId) {
		char buffer[4096] = { 0 };
		lm.GetMemberMetadataValue(lobbyId, userId, "$net.peer_id", buffer);
		std::string memberPeerIdStr(buffer);
		memset(buffer, 0, sizeof(buffer));
		lm.GetMemberMetadataValue(lobbyId, userId, "$net.route", buffer);
		std::string memberRoute(buffer);
		memset(buffer, 0, sizeof(buffer));
		lm.GetMemberMetadataValue(lobbyId, userId, "$net.ipv4", buffer);
		std::string memberIPStr(buffer);

		if (memberPeerIdStr.empty() || memberRoute.empty()) {
			// peer not ready
			return;
		}

		discord::NetworkPeerId memberPeerId = std::stoull(memberPeerIdStr);

		if (ipMapping.hasPeer(memberPeerId)) {
			// existing node, update peer and done
			nm.UpdatePeer(memberPeerId, memberRoute.c_str());
			return;
		}

		// new node
		nm.OpenPeer(memberPeerId, memberRoute.c_str());

		nm.OpenChannel(memberPeerId, channel::data, false);
		nm.OpenChannel(memberPeerId, channel::aux, true);

		if (memberIPStr.empty()) {
			if (connection == Connection::Hosting) {
				// issue new IP
				auto memberIP = ipMapping.getIP(memberPeerId).first;
				auto auxData = std::string("ipv4 ") + cidr::format(memberIP);
				nm.SendMessage(memberPeerId, channel::aux, (uint8_t*)auxData.c_str(), (uint32_t)auxData.size());
			} else {
				// peer not ready
				return;
			}
		} else {
			ipMapping.setPeer(memberPeerId, cidr::parse(memberIPStr.c_str()));
		}
	};

	// Disconnect from a lobby
	auto disconnectLobby = [&]() {
		if (connection == Connection::Disconnected)
			return;

		lm.DisconnectLobby(lobby.GetId(), [&](discord::Result result) {
			if (result != discord::Result::Ok) {
				{
					std::lock_guard lg(mutex);
					exception = std::runtime_error("Could not disconnect from lobby");
				}
				return;
			}
			connection = Connection::Disconnected;
			ipMapping.clear();
		});
	};

	// Host a lobby
	auto hostLobby = [&]() {
		hostCidr = hostCidr.getMasked() + 1;
		ipMapping = IPMapping(hostCidr);
		discord::LobbyTransaction lobbyTx{};
		lm.GetLobbyCreateTransaction(&lobbyTx);
		lobbyTx.SetCapacity(8);
		lobbyTx.SetType(discord::LobbyType::Private);
		lm.CreateLobby(
			lobbyTx, [&](discord::Result result, discord::Lobby const& l) {
				if (result != discord::Result::Ok)
				{
					std::lock_guard lg(mutex);
					exception = std::runtime_error("Cannot create lobby");
					return;
				}
				if (!adapter) {
					adapter = std::make_unique<lpvpn::adapter::Adapter>();
				}
				adapter->setIP(hostCidr);
				address = hostCidr;
				lobby = l;

				// self set peerId
				discord::LobbyMemberTransaction tx;
				lm.GetMemberUpdateTransaction(lobby.GetId(), user.GetId(), &tx);
				tx.SetMetadata("$net.peer_id", std::to_string(peerId).c_str());
				tx.SetMetadata("$net.route", route.c_str());
				tx.SetMetadata("$net.ipv4", cidr::format(hostCidr).c_str());
				lm.UpdateMember(lobby.GetId(), user.GetId(), tx, [](discord::Result result) {});

				char activitySecret[128] = { 0 };
				int32_t memberCount = 0;
				lm.MemberCount(lobby.GetId(), &memberCount);
				lm.GetLobbyActivitySecret(lobby.GetId(), activitySecret);

				activity.SetDetails("");
				activity.SetState("Hosting a VPN");

				activity.SetType(discord::ActivityType::Playing);
				activity.GetParty().GetSize().SetMaxSize(lobby.GetCapacity());
				activity.GetParty().GetSize().SetCurrentSize(memberCount);
				activity.GetParty().SetId(std::to_string(lobby.GetId()).c_str());
				activity.GetSecrets().SetJoin(activitySecret);
				am.UpdateActivity(activity, [&](discord::Result result) {});
				connection = Connection::Hosting;
			});
	};

	// nm callbacks
	nm.OnMessage.Connect([&](discord::NetworkPeerId peerId, discord::NetworkChannelId channelId, uint8_t* data, uint32_t dataLength) {
		if (channelId == channel::aux) {
			// aux channel
			std::string_view auxData((char*)data);
			if (auxData.find("ipv4 ") == 0) {
				try {
					auto addr = cidr::parse(auxData.substr(5).data());
					discord::LobbyMemberTransaction tx;
					lm.GetMemberUpdateTransaction(lobby.GetId(), user.GetId(), &tx);
					tx.SetMetadata("$net.ipv4", cidr::format(addr).c_str());
					lm.UpdateMember(lobby.GetId(), user.GetId(), tx, [](discord::Result result) {});

					hostCidr = addr.getMasked() + 1;
					ipMapping = IPMapping(hostCidr);
					if (!adapter)
						adapter = std::make_unique<lpvpn::adapter::Adapter>();
					adapter->setIP(addr);
					{
						std::lock_guard lg(mutex);
						this->address = addr;
					}
					connection = Connection::Connected;
				} catch (std::exception& e) {
					{
						std::lock_guard lg(mutex);
						exception = std::runtime_error("Cannot set IP on adapter: " + std::string(e.what()));
					}
					disconnectLobby();
					return;
				}
			}
		}
		else if (channelId == channel::data) {
			if (adapter == nullptr)
				return;
			receivedBytes += dataLength;
			adapter->write(std::span(data, dataLength));
		}
	});
	// nm callbacks
	nm.OnRouteUpdate.Connect([&](char const* newRoute) {
		route = newRoute;
		if (connection >= Connection::WaitingForIP) {
			discord::LobbyMemberTransaction tx;
			lm.GetMemberUpdateTransaction(lobby.GetId(), user.GetId(), &tx);
			tx.SetMetadata("$net.route", newRoute);
			lm.UpdateMember(lobby.GetId(), user.GetId(), tx, [](discord::Result result) {});
		}
	});

	// lm callbacks
	lm.OnLobbyDelete.Connect([&](discord::LobbyId lobbyId, discord::UserId userId) {
		connection = Connection::Disconnected;
		ipMapping.clear();
	});
	lm.OnMemberDisconnect.Connect([&](discord::LobbyId lobbyId, discord::UserId userId) {
		if (connection == Connection::Hosting) {
			int32_t memberCount = 0;
			lm.MemberCount(lobby.GetId(), &memberCount);
			activity.GetParty().GetSize().SetCurrentSize(memberCount);
			am.UpdateActivity(activity, [](discord::Result result) {});
			refreshIPs();
		}
	});
	lm.OnMemberConnect.Connect([&](discord::LobbyId lobbyId, discord::UserId userId) {
		if (connection == Connection::Hosting) {
			int32_t memberCount = 0;
			lm.MemberCount(lobby.GetId(), &memberCount);
			activity.GetParty().GetSize().SetCurrentSize(memberCount);
			am.UpdateActivity(activity, [](discord::Result result) {});
		}
	});
	lm.OnMemberUpdate.Connect(updatePeer);


	// um callbacks
	um.OnCurrentUserUpdate.Connect([&]() {
		um.GetCurrentUser(&user);
	});

	// am callbacks
	am.OnActivityJoin.Connect([&](discord::LobbySecret secret) {
		if (connection >= Connection::WaitingForIP) {
			std::lock_guard lg(mutex);
			message = "Cannot join, already in a lobby";
			return; // return if already connected;
		}
		lm.ConnectLobbyWithActivitySecret(secret, [&](discord::Result result, const discord::Lobby& l) {
			if (connection >= Connection::WaitingForIP)
				return;
			if (result != discord::Result::Ok) {
				{
					std::lock_guard lg(mutex);
					//message = "Cannot connect to lobby " + std::to_string((int)result);
				}
				return;
			}
			lobby = l;
			if (!adapter) {
				adapter = std::make_unique<lpvpn::adapter::Adapter>();
			}

			refreshIPs();

			// self set peerId
			discord::LobbyMemberTransaction tx;
			lm.GetMemberUpdateTransaction(lobby.GetId(), user.GetId(), &tx);
			tx.SetMetadata("$net.peer_id", std::to_string(peerId).c_str());
			tx.SetMetadata("$net.route", route.c_str());
			lm.UpdateMember(lobby.GetId(), user.GetId(), tx, [](discord::Result result) {});

			activity.SetDetails("");
			activity.SetState("Connected to a VPN");

			int32_t count = 0;
			lm.MemberCount(lobby.GetId(), &count);
			for (int32_t i = 0; i < count; i++) {
				discord::UserId memberUserId = 0;
				lm.GetMemberUserId(lobby.GetId(), i, &memberUserId);
				updatePeer(lobby.GetId(), memberUserId);
			}

			am.UpdateActivity(activity, [&](discord::Result result) {});
			connection = Connection::WaitingForIP;
		});
	});

	// Logic to run before blocking for packets
	std::function<bool()> preBlock = [&]() {
		if (connection == Connection::Disconnected) {
			ipMapping.clear();
		}

		nm.Flush();
		core->RunCallbacks();
		refreshIPs();

		auto t = task.load();
		if (t != Task::None) {
			switch (t) {
			case Task::DoInvite:
				if (connection >= Connection::Connected) {
					om.OpenActivityInvite(discord::ActivityActionType::Join, [](discord::Result result) {});
				}
				break;
			case Task::DoLeave:
				disconnectLobby();
				break;
			case Task::DoHost:
				hostLobby();
				break;
			default:
				break;
			}
			task.store(Task::None);
		}

		if (connection < Connection::Connected) {
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
			return true;
		}

		return false;
	};

	preBlock();
	// Send packets currently available then block once
	while (!token.stop_requested())
	{
		if (connection < Connection::Connected) {
			preBlock();
			continue;
		}
		auto packet = adapter->read(preBlock);

		if (packet.empty())
			continue;
		sentBytes += packet.size();
		if (packet.size() < 20 || packet[0] >> 4 != 4)
		{
			// skip small and non IPv4 packets
			continue;
		}
		// parse dstIP
		struct in_addr dstIP;
		memcpy(&dstIP, packet.data() + 16, 4);
			
		auto remotePeerId = ipMapping.getPeer(dstIP);
		if (!remotePeerId.has_value())
		{
			// This ip is not registered so we refresh IPs and try again
			refreshIPs();
			remotePeerId = ipMapping.getPeer(dstIP);
		}
		if (!remotePeerId.has_value())
		{
			// This IP does not exist
			continue;
		}

		// directly send to Peer if found in cache.
		nm.SendMessage(*remotePeerId, channel::data, (uint8_t*)packet.data(), packet.size());
	}
}

DiscordNet::DiscordNet()
{
	thread = std::jthread([&](std::stop_token token) { run(std::move(token)); });
}


DiscordNet::~DiscordNet()
{
}

