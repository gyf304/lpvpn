#pragma once

#include <stdint.h>
#include <winsock2.h>

namespace cidr {
	struct CIDR {
		struct in_addr addr;
		uint8_t maskBits;
		struct in_addr mask() const; 
	};

	bool operator==(const CIDR &a, const CIDR &b);
	CIDR operator+(const CIDR &cidr, int offset);

	CIDR parse(const char *addr);
	std::string format(CIDR &cidr);
}

