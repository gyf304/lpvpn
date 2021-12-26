#pragma once

#include <stdint.h>
#include <winsock2.h>

namespace cidr {
	struct CIDR {
		struct in_addr addr;
		uint8_t maskBits;
		struct in_addr mask() const;
		struct CIDR getMasked() const;
	};

	bool operator==(const CIDR &a, const CIDR &b);
	CIDR operator+(const CIDR &cidr, int offset);
	unsigned operator-(const CIDR& cidr1, const CIDR& cidr2);

	CIDR parse(const char *addr);
	std::string format(const CIDR &cidr);
}

