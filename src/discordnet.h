#pragma once

#include <queue>
#include <optional>

#include <atomic>
#include <thread>
#include <discord.h>
#include "adapter.h"
#include "mutex.h"

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
public:
	std::jthread thread;

	enum class Task {
		None,
		DoInvite,
		DoHost,
		DoLeave
	};
	enum class Connection {
		Disconnected,
		WaitingForIP,
		Connected,
		Hosting
	};
	util::FastMutex mutex;
	std::atomic<Connection> connection;
	std::atomic<Task> task;

	cidr::CIDR hostCidr;
	std::optional<cidr::CIDR> address;
	std::optional<std::exception> exception;
	std::optional<std::string> message;

	std::atomic<uint64_t> sentBytes = 0;
	std::atomic<uint64_t> receivedBytes = 0;

	void run(std::stop_token token);

	DiscordNet();
	~DiscordNet();

	Connection getConnectionState() const { return connection.load(); }

	void invite() { auto exp = Task::None; task.compare_exchange_strong(exp, Task::DoInvite); }
	void host(cidr::CIDR hostCidr) { auto exp = Task::None; this->hostCidr = hostCidr; task.compare_exchange_strong(exp, Task::DoHost); }
	void leave() { auto exp = Task::None; task.compare_exchange_strong(exp, Task::DoLeave); }
};

}
