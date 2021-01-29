#include <array>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <map>
#include <atomic>

#include <tray.h>

#include "main.h"

#include "guidparse.h"
#include "adapter.h"
#include "bus.h"
#include "discordnet.h"

#if defined(_WIN32)
#define ICON_FILE "icon.ico"
#endif

#ifndef DEFAULT_CIDR
#define DEFAULT_CIDR "192.168.42.1/24"
#endif

using namespace meshsocket;

bus::Bus mainBus = bus::Bus();

void host_cb(struct tray_menu *item)
{
	mainBus.publish("host", "");
}

void invite_cb(struct tray_menu *item)
{
	mainBus.publish("invite", "");
}

void do_nothing_cb(struct tray_menu *item)
{
	// pass
}

void quit_cb(struct tray_menu *item)
{
	mainBus.publish("exit", "");
}

struct tray_menu tray_menu[] = {
	{"Status: Standing by", 1, 0, do_nothing_cb, NULL},
	{"Host", 0, 0, host_cb, NULL},
	{"Invite", 0, 0, invite_cb, NULL},
	{"Quit", 0, 0, quit_cb, NULL},
	{NULL, 0, 0, NULL, NULL}};

struct tray tray = {ICON_FILE, tray_menu};

int app_main(int argc, char **argv)
{
	std::string statusStr;
	bool interrupted = false;
	std::vector<std::string> args;
	for (int i = 0; i < argc; i++) {
		args.push_back(argv[i]);
	}

	tray_init(&tray);

	mainBus.subscribe("error", [&](const std::string &msg) {
		alert(msg);
		interrupted = true;
	});

	mainBus.subscribe("warning", [&](const std::string &msg) {
		alert(msg);
	});

	mainBus.subscribe("status", [&](const std::string &msg) {
		statusStr = "Status: " + msg;
		tray_menu[0].text = (char *) statusStr.c_str();
		tray_update(&tray);
	});

	mainBus.subscribe("exit", [&](const std::string &msg) {
		interrupted = true;
	});

	std::signal(SIGINT, [](int) {
		mainBus.publish("exit", "");
	});

	auto hostCidr = cidr::parse(DEFAULT_CIDR);
	auto adapter = std::make_unique<adapter::Adapter>();

	{ // run discordNet
		discordnet::DiscordNet discordNet(hostCidr, std::move(adapter));
		while (tray_loop(0) == 0 && !interrupted) {
			mainBus.runCallbacks();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	tray_exit();
	return 0;
}
