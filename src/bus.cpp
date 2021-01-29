#include <map>
#include <functional>
#include <thread>
#include <atomic>

#include <iostream>

#include "bus.h"

using namespace meshsocket::bus;

std::atomic<uint32_t> clientsCount = 0;
bool initialized = false;
zmq::context_t ctx;
zmq::socket_t xpubSock;
zmq::socket_t xsubSock;
std::thread proxyThread;

void proxy()
{
	try {
		zmq::proxy(xsubSock, xpubSock);
	} catch (std::exception &e) {
		(void) e;
	}
}

meshsocket::bus::Bus::Bus()
{
	if (clientsCount++ == 0)
	{
		ctx = zmq::context_t(0);

		xpubSock = zmq::socket_t(ctx, zmq::socket_type::xpub);
		xsubSock = zmq::socket_t(ctx, zmq::socket_type::xsub);
		
		xpubSock.bind("inproc://xpub");
		xsubSock.bind("inproc://xsub");

		proxyThread = std::thread(proxy);
	}

	pubSock = zmq::socket_t(ctx, zmq::socket_type::pub);
	subSock = zmq::socket_t(ctx, zmq::socket_type::sub);
	pubSock.connect("inproc://xsub");
	subSock.connect("inproc://xpub");
}

meshsocket::bus::Bus::~Bus()
{
	if (--clientsCount == 0)
	{
		ctx.shutdown();
		proxyThread.join();
	}
}

void meshsocket::bus::Bus::subscribe(const std::string &chan, std::function<void(const std::string &msg)> f)
{
	subscriptions[chan] = f;
	subSock.set(zmq::sockopt::subscribe, chan);
}

void meshsocket::bus::Bus::subscribe(const char *chan, std::function<void(const std::string &msg)> f)
{
	subscribe(std::string(chan), f);
}

void meshsocket::bus::Bus::publish(const std::string &chan, const std::string &msg)
{
	pubSock.send(zmq::message_t(chan), zmq::send_flags::sndmore);
	pubSock.send(zmq::message_t(msg), zmq::send_flags::none);
}

void meshsocket::bus::Bus::publish(const char *chan, const char *msg)
{
	publish(std::string(chan), std::string(msg));
}

void meshsocket::bus::Bus::runCallbacks(bool blocking)
{
	std::vector<zmq::message_t> recvMsgs;
	while (true)
	{
		auto ret = zmq::recv_multipart(subSock, std::back_inserter(recvMsgs), blocking ? zmq::recv_flags::none : zmq::recv_flags::dontwait);
		if (!ret)
		{
			return;
		}
		if (recvMsgs.size() < 2)
		{
			continue;
		}
		auto chan = recvMsgs[0].to_string();
		auto msg = recvMsgs[1].to_string();
		if (subscriptions.count(chan))
		{
			subscriptions[chan](msg);
		}
	}
}
