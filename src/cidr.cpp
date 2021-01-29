#include <stdexcept>
#include <string>
#include <stdio.h>

#include "cidr.h"

#pragma comment(lib, "Ws2_32.lib")

using namespace cidr;

struct in_addr CIDR::mask() const
{
	uint64_t mask = ~(((uint64_t)1 << (32 - maskBits)) - 1);
	struct in_addr addr;
	addr.S_un.S_addr = htonl((uint32_t)mask);
	return addr;
}

bool cidr::operator==(const CIDR &a, const CIDR &b) {
	return a.addr.S_un.S_addr == b.addr.S_un.S_addr && a.maskBits == b.maskBits;
}

CIDR cidr::operator+(const CIDR &cidr, int offset) {
	CIDR result = cidr;
	auto addr = ntohl(cidr.addr.S_un.S_addr);
	auto mask = ntohl(cidr.mask().S_un.S_addr);
	auto newaddr = addr + offset;
	if ((addr & mask) != (newaddr & mask)) {
		throw std::range_error("resultant CIDR out of range");
	}
	result.addr.S_un.S_addr = htonl(newaddr);
	return result;
}

CIDR cidr::parse(const char *addr) {
	CIDR cidr = {0};
	unsigned long a, b, c, d, e;
	int result = sscanf(addr, "%u.%u.%u.%u/%u", &a, &b, &c, &d, &e);
	if (result != 5) {
		return cidr;
	}
	if (a > 255 || b > 255 || c > 255 || d > 255 || e > 32) {
		return cidr;
	}
	cidr.addr.S_un.S_addr = htonl(a << 24 | b << 16 | c << 8 | d);
	cidr.maskBits = (uint8_t) e;
	return cidr;
}

std::string cidr::format(CIDR &cidr) {
	std::string addr(inet_ntoa(cidr.addr));
	return addr + "/" + std::to_string(cidr.maskBits);
}
