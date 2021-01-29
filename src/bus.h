#pragma once

#ifdef max
#undef max
#endif

#ifdef _WIN32
#pragma warning(disable:4267)
#endif
#include <zmq_addon.hpp>
#ifdef _WIN32
#pragma warning(default:4267)
#endif

namespace meshsocket::bus {
class Bus {
	std::map<std::string, std::function<void(const std::string &msg)>> subscriptions;
	zmq::socket_t pubSock;
	zmq::socket_t subSock;
public:
	Bus();
	~Bus();

	void subscribe(const std::string &chan, std::function<void(const std::string &msg)> f);
	void subscribe(const char *chan, std::function<void(const std::string &msg)> f);

	void publish(const std::string &chan, const std::string &msg) ;
	void publish(const char *chan, const char *msg);

	void runCallbacks(bool blocking = false);
};
}