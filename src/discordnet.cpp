#include <array>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <map>
#include <set>

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
	return a.S_un.S_addr < b.S_un.S_addr;
}

void DiscordNet::run()
{
	std::unique_ptr<lpvpn::adapter::Adapter> adapter = nullptr;

	std::map<struct in_addr, discord::NetworkPeerId> ipMapping;
	std::set<discord::NetworkPeerId> peers;

	bool connected = false;
	cidr::CIDR currentAddr = hostCidr;

	discord::NetworkPeerId peerId;
	std::string route;

	discord::Lobby lobby;
	discord::User user;
	discord::Core *_core{};
	std::shared_ptr<discord::Core> core;
	auto result = discord::Core::Create(DISCORD_APP_ID, DiscordCreateFlags_Default, &_core);
	core.reset(_core);
	if (!core)
	{
		exception = std::runtime_error("Cannot instantiate discord service");
		interrupted = true;
		return;
	}

	auto &lm = core->LobbyManager();
	auto &um = core->UserManager();
	auto &am = core->ActivityManager();
	auto &om = core->OverlayManager();
	auto &nm = core->NetworkManager();

	nm.GetPeerId(&peerId); // get peer id.

	discord::Activity activity{};
	activity.SetApplicationId(DISCORD_APP_ID);

	auto updatePeer = [&](discord::LobbyId lobbyId, discord::UserId userId) {
		char _memberPeerId[4096] = { 0 };
		char _memberRoute[4096] = { 0 };

		lm.GetMemberMetadataValue(lobbyId, userId, "$net.peer_id", _memberPeerId);
		lm.GetMemberMetadataValue(lobbyId, userId, "$net.route", _memberRoute);

		std::string memberPeerIdStr(_memberPeerId);
		std::string memberRoute(_memberRoute);

		if (memberPeerIdStr.empty() || memberRoute.empty()) {
			// peer not ready
			return;
		}

		discord::NetworkPeerId memberPeerId = std::stoull(memberPeerIdStr);

		if (peers.count(memberPeerId)) {
			// existing node, update peer and done
			nm.UpdatePeer(memberPeerId, memberRoute.c_str());
			return;
		}

		// new node
		peers.emplace(memberPeerId);
		nm.OpenPeer(memberPeerId, memberRoute.c_str());

		nm.OpenChannel(memberPeerId, channel::data, false);
		nm.OpenChannel(memberPeerId, channel::aux,  true);

		if (!host) {
			return;
		}

		// issue IP
		auto auxData = std::string("ipv4 ") + cidr::format(currentAddr);
		nm.SendMessage(memberPeerId, channel::aux, (uint8_t *)auxData.c_str(), (uint32_t)auxData.size());
		nm.Flush();
		currentAddr = currentAddr + 1;
	};

	// nm callbacks
	nm.OnRouteUpdate.Connect([&](char const *newRoute) {
		if (!connected) {
			route = std::string(newRoute);
			return;
		}
		discord::LobbyMemberTransaction tx;
		lm.GetMemberUpdateTransaction(lobby.GetId(), user.GetId(), &tx);
		tx.SetMetadata("$net.route", newRoute);
		lm.UpdateMember(lobby.GetId(), user.GetId(), tx, [](discord::Result result) {});
	});

	nm.OnMessage.Connect([&](discord::NetworkPeerId peerId, discord::NetworkChannelId channelId, uint8_t *data, uint32_t dataLength) {
		if (channelId == channel::aux) {
			// aux channel
			std::string auxData((char *)data);
			if (auxData.find("ipv4 ") == 0) {
				auto addr = cidr::parse(auxData.substr(5).c_str());
				try {
					adapter->setIP(addr);
					this->address = addr;
				} catch (std::exception &e) {
					exception = std::runtime_error("Cannot set IP on adapter: " + std::string(e.what()));
					interrupted = true;
					return;
				}
			}
		} else if (channelId == channel::data) {
			if (adapter == nullptr) {
				return;
			}
			receivedBytes += dataLength;
			adapter->write(std::vector<uint8_t>(data, data + dataLength));
		}
	});

	// lm callbacks
	lm.OnLobbyDelete.Connect([&](discord::LobbyId lobbyId, std::uint32_t reason) {
		interrupted = true;
	});

	lm.OnMemberDisconnect.Connect([&](discord::LobbyId lobbyId, discord::UserId userId) {
		if (host)
		{
			int32_t memberCount = 0;
			lm.MemberCount(lobby.GetId(), &memberCount);
			activity.GetParty().GetSize().SetCurrentSize(memberCount);
			am.UpdateActivity(activity, [](discord::Result result) {});
		}
	});

	lm.OnMemberConnect.Connect([&](discord::LobbyId lobbyId, discord::UserId userId) {
		if (host)
		{
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

	am.OnActivityJoin.Connect([&](discord::LobbySecret secret) {
		if (connected)
		{
			exception = std::runtime_error("Cannot join, already connected");
			return; // return if already connected;
		}
		connected = true;
		lm.ConnectLobbyWithActivitySecret(secret, [&](discord::Result result, const discord::Lobby &l) {
			if (result != discord::Result::Ok)
			{
				exception = std::runtime_error("Cannot connect to lobby");
				interrupted = true;
				return;
			}
			lobby = l;
			adapter = std::make_unique<lpvpn::adapter::Adapter>();

			// connect to peers
			int32_t count = 0;
			lm.MemberCount(lobby.GetId(), &count);
			for (int32_t i = 0; i < count; i++) {
				discord::UserId memberUserId = 0;
				lm.GetMemberUserId(lobby.GetId(), i, &memberUserId);
				updatePeer(lobby.GetId(), memberUserId);
			}

			// self set peerId
			discord::LobbyMemberTransaction tx;
			lm.GetMemberUpdateTransaction(lobby.GetId(), user.GetId(), &tx);
			tx.SetMetadata("$net.peer_id", std::to_string(peerId).c_str());
			tx.SetMetadata("$net.route", route.c_str());
			lm.UpdateMember(lobby.GetId(), user.GetId(), tx, [](discord::Result result) {});

			activity.SetDetails("");
			activity.SetState("Connected to a VPN");

			am.UpdateActivity(activity, [&](discord::Result result) {});
		});
	});

	if (host) {
		discord::LobbyTransaction lobbyTx{};
		lm.GetLobbyCreateTransaction(&lobbyTx);
		lobbyTx.SetCapacity(8);
		lobbyTx.SetType(discord::LobbyType::Private);
		lm.CreateLobby(
			lobbyTx, [&](discord::Result result, discord::Lobby const &l) {
				if (result != discord::Result::Ok)
				{
					exception = std::runtime_error("Cannot create lobby");
					interrupted = true;
					return;
				}
				connected = true;
				adapter = std::make_unique<lpvpn::adapter::Adapter>();
				lobby = l;

				// self set peerId
				discord::LobbyMemberTransaction tx;
				lm.GetMemberUpdateTransaction(lobby.GetId(), user.GetId(), &tx);
				tx.SetMetadata("$net.peer_id", std::to_string(peerId).c_str());
				tx.SetMetadata("$net.route", route.c_str());
				lm.UpdateMember(lobby.GetId(), user.GetId(), tx, [](discord::Result result) {});

				char activitySecret[128] = {0};
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
			});
	}

	while (!interrupted)
	{
		core->RunCallbacks();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		if (!connected || adapter == nullptr) {
			continue;
		}

		if (shouldInvite) {
			shouldInvite = false;
			om.OpenActivityInvite(discord::ActivityActionType::Join, [](discord::Result result) {});
		}

		// send packets
		std::vector<uint8_t> packet;
		do
		{
			packet = adapter->read();
			sentBytes += packet.size();
			if (packet.size() < 20 || packet[0] >> 4 != 4)
			{
				// skip small and non IPv4 packets
				continue;
			}
			// parse dstIP
			struct in_addr dstIP;
			for (int i = 0; i < 4; i++)
			{
				uint8_t *addr = (uint8_t *)&dstIP;
				addr[i] = packet[16 + i];
			}
			if (ipMapping.count(dstIP))
			{
				// directly send to Peer if found in cache.
				auto &remotePeerId = ipMapping[dstIP];
				nm.SendMessage(remotePeerId, channel::data, &packet[0], (uint32_t)packet.size());
			}
			else
			{
				// otherwise broadcast
				int32_t memberCount = 0;
				lm.MemberCount(lobby.GetId(), &memberCount);
				for (int32_t i = 0; i < memberCount; i++)
				{
					discord::UserId dstUserId = 0;
					lm.GetMemberUserId(lobby.GetId(), i, &dstUserId);
					if (dstUserId != user.GetId())
					{
						for (auto &remotePeerId : peers) {
							if (remotePeerId == peerId) {
								continue;
							}
							nm.SendMessage(remotePeerId, channel::data, &packet[0], (uint32_t)packet.size());
						}
					}
				}
			}
		} while (packet.size());
		nm.Flush();
	};
}

DiscordNet::DiscordNet(cidr::CIDR hostCidr)
{
	this->hostCidr = hostCidr;
	this->host = true;
	thread = std::thread(&DiscordNet::run, this);
}

DiscordNet::DiscordNet()
{
	this->host = false;
	thread = std::thread(&DiscordNet::run, this);
}

DiscordNet::~DiscordNet()
{
	interrupted = true;
	thread.join();
}

size_t DiscordNet::getSentBytes() {
	return sentBytes;
}

size_t DiscordNet::getReceivedBytes() {
	return receivedBytes;
}

std::optional<cidr::CIDR> DiscordNet::getAddress() {
	return address;
}

std::optional<std::exception> DiscordNet::getException() {
	return exception;
}

void DiscordNet::invite() {
	shouldInvite = true;
}
