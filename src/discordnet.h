#pragma once

#include <atomic>
#include <thread>
#include <discord.h>
#include "adapter.h"

namespace meshsocket::discordnet {
class DiscordNet {
	std::thread thread;
	std::atomic<bool> interrupted = false;
public:
	DiscordNet(cidr::CIDR hostCidr, std::unique_ptr<adapter::Adapter> adapter);
	~DiscordNet();
};
}
