#pragma once
#include "cidr.h"
#include <cassert>
#include <span>
#include <functional>

namespace lpvpn::adapter {
	struct Packet {
		void* sessionHandle = nullptr;
		uint8_t* packet = nullptr;
		size_t packetSize = 0;

		constexpr Packet() = default;
		constexpr Packet(void* sessionHandle, uint8_t* packet, size_t packetSize) : sessionHandle(sessionHandle), packet(packet), packetSize(packetSize) {}

		constexpr Packet(Packet&& p) noexcept {
			this->sessionHandle = p.sessionHandle;
			this->packet = p.packet;
			p.packet = nullptr;
		}
		Packet(const Packet&) = delete;
		~Packet();

		void release();

		Packet& operator=(Packet&& p) {
			this->release();
			this->sessionHandle = p.sessionHandle;
			this->packet = p.packet;
			p.packet = nullptr;
			return *this;
		}
		Packet& operator=(const Packet& p) = delete;

		uint8_t operator[](size_t ind) const { assert(ind < packetSize); return packet[ind]; }
		constexpr uint8_t* data() const { return packet; }
		constexpr size_t size() const { return packetSize; }
		constexpr bool empty() const { return size() == 0; }
	};

	class WintunAdapter;

	class Adapter {
		std::shared_ptr<WintunAdapter> impl;
	public:
		Adapter();
		void setIP(cidr::CIDR &addr);
		void write(std::span<uint8_t> data);
		Packet read(const std::function<bool()>& preBlock);
	};
}
