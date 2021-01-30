#pragma once
#include "cidr.h"

namespace lpvpn::adapter {
	class Adapter {
		std::shared_ptr<void> impl;
	public:
		Adapter();
		void setIP(cidr::CIDR &addr);
		void write(std::vector<uint8_t> &data);
		std::vector<uint8_t> read();
	};
}
