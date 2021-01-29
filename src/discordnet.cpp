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
#include "main.h"

#include "guidparse.h"
#include "adapter.h"
#include "bus.h"

// wtf Microsoft?
#ifdef SendMessage
#undef SendMessage
#endif

#include "discordnet.h"

using namespace meshsocket;

enum channel {
	data = 0,
	aux
};

bool operator<(const struct in_addr &a, const struct in_addr &b)
{
	return a.S_un.S_addr < b.S_un.S_addr;
}

static void discordMain(cidr::CIDR hostCidr, std::unique_ptr<adapter::Adapter> adapter, std::atomic<bool> *interruptPtr)
{
	auto &interrupted = *interruptPtr;
	auto bus = bus::Bus();

	std::map<struct in_addr, discord::NetworkPeerId> ipMapping;
	std::set<discord::NetworkPeerId> peers;

	bool connected = false;
	bool isHost = false;
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
		bus.publish("error", "Cannot instantiate discord service");
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

		if (!isHost) {
			return;
		}

		// issue IP
		auto auxData = std::string("ipv4 ") + cidr::format(currentAddr);
		nm.SendMessage(memberPeerId, channel::aux, (uint8_t *)auxData.c_str(), auxData.size());
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
				} catch (std::exception &e) {
					bus.publish("error", "Cannot set IP on adapter: " + std::string(e.what()));
					interrupted = true;
					return;
				}
			}
		} else if (channelId == channel::data) {
			adapter->write(std::vector<uint8_t>(data, data + dataLength));
		}
	});

	// lm callbacks
	lm.OnLobbyDelete.Connect([&](discord::LobbyId lobbyId, std::uint32_t reason) {
		interrupted = true;
	});

	lm.OnMemberDisconnect.Connect([&](discord::LobbyId lobbyId, discord::UserId userId) {
		if (isHost)
		{
			int32_t memberCount = 0;
			lm.MemberCount(lobby.GetId(), &memberCount);
			activity.GetParty().GetSize().SetCurrentSize(memberCount);
			am.UpdateActivity(activity, [](discord::Result result) {});
			if (memberCount == 1) {
				bus.publish("status", "Hosting a VPN (1 Member)");
			} else {
				bus.publish("status", "Hosting a VPN (" + std::to_string(memberCount) + " Members)");
			}
		}
	});

	lm.OnMemberConnect.Connect([&](discord::LobbyId lobbyId, discord::UserId userId) {
		if (isHost)
		{
			int32_t memberCount = 0;
			lm.MemberCount(lobby.GetId(), &memberCount);
			activity.GetParty().GetSize().SetCurrentSize(memberCount);
			am.UpdateActivity(activity, [](discord::Result result) {});
			if (memberCount == 1) {
				bus.publish("status", "Hosting a VPN (1 Member)");
			} else {
				bus.publish("status", "Hosting a VPN (" + std::to_string(memberCount) + " Members)");
			}
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
			bus.publish("warning", "Cannot join, already connected");
			return; // return if already connected;
		}
		connected = true;
		isHost = false;
		lm.ConnectLobbyWithActivitySecret(secret, [&](discord::Result result, const discord::Lobby &l) {
			if (result != discord::Result::Ok)
			{
				bus.publish("error", "Cannot connect to lobby");
				interrupted = true;
				return;
			}
			lobby = l;

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
			bus.publish("status", "Connected");
			am.UpdateActivity(activity, [&](discord::Result result) {});
		});
	});

	bus.subscribe("host", [&](const std::string &msg) {
		if (connected)
		{
			if (isHost)
			{
				// if already connected, just run invite.
				om.OpenActivityInvite(discord::ActivityActionType::Join, [](discord::Result result) {});
			}
			else
			{
				bus.publish("warning", "You are already in a lobby.");
			}
			return;
		}
		// create lobby
		discord::LobbyTransaction lobbyTx{};
		lm.GetLobbyCreateTransaction(&lobbyTx);
		lobbyTx.SetCapacity(8);
		lobbyTx.SetType(discord::LobbyType::Private);
		lm.CreateLobby(
			lobbyTx, [&](discord::Result result, discord::Lobby const &l) {
				if (result != discord::Result::Ok)
				{
					bus.publish("error", "Cannot create lobby");
					interrupted = true;
					return;
				}
				connected = true;
				isHost = true;
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
				bus.publish("status", "Hosting a VPN (1 Member)");
				activity.SetType(discord::ActivityType::Playing);
				activity.GetParty().GetSize().SetMaxSize(lobby.GetCapacity());
				activity.GetParty().GetSize().SetCurrentSize(memberCount);
				activity.GetParty().SetId(std::to_string(lobby.GetId()).c_str());
				activity.GetSecrets().SetJoin(activitySecret);
				am.UpdateActivity(activity, [&](discord::Result result) {
					om.OpenActivityInvite(discord::ActivityActionType::Join, [](discord::Result result) {});
				});
			});
	});

	bus.subscribe("invite", [&](const std::string &msg) {
		if (!isHost || !connected)
		{
			bus.publish("warning", "You must be hosting a VPN to invite.");
			return;
		}
		om.OpenActivityInvite(discord::ActivityActionType::Join, [](discord::Result result) {});
	});

	while (!interrupted)
	{
		bus.runCallbacks();
		core->RunCallbacks();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		if (!connected) {
			continue;
		}

		// send packets
		std::vector<uint8_t> packet;
		do
		{
			packet = adapter->read();
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

meshsocket::discordnet::DiscordNet::DiscordNet(cidr::CIDR hostCidr, std::unique_ptr<adapter::Adapter> adapter)
{
	thread = std::thread(discordMain, hostCidr, std::move(adapter), &interrupted);
}

meshsocket::discordnet::DiscordNet::~DiscordNet()
{
	interrupted = true;
	thread.join();
}
