#pragma once

#include <queue>
#include <optional>

#include <atomic>
#include <thread>
#include <discord.h>
#include "adapter.h"

#ifdef ERROR
#undef ERROR
#endif

namespace lpvpn::discordnet {

enum DiscordNetOutMessageType {
	INFO = 0,
	WARNING,
	ERROR,
	IPV4_ADDR,
	UPLOAD_BANDWIDTH,
	DOWNLOAD_BANDWIDTH
};

enum DiscordNetInMessageType {
	INVITE = 0
};

struct DiscordNetOutMessage {
	DiscordNetOutMessageType type;
	std::string message;
};

struct DiscordNetInMessage {
	DiscordNetInMessageType type;
	std::string message;
};


class DiscordNet {
	std::thread thread;
	std::atomic<bool> interrupted = false;
	std::mutex mutex;

	cidr::CIDR hostCidr;
	std::optional<cidr::CIDR> address;
	std::optional<std::exception> exception;
	std::optional<std::string> message;

	bool host;
	bool shouldInvite = false;
	std::atomic<uint64_t> sentBytes = 0;
	std::atomic<uint64_t> receivedBytes = 0;

	void run();
public:
	DiscordNet();
	DiscordNet(cidr::CIDR hostCidr);
	~DiscordNet();

	size_t getSentBytes();
	size_t getReceivedBytes();

	std::optional<cidr::CIDR> getAddress();
	std::optional<std::exception> getException();
	std::optional<std::string> getMessage();

	void invite();
};

}
