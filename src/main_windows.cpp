#ifdef _WIN32

#include <windows.h>
#include <tchar.h>
#include <thread>
#include <vector>
#include <string>

#define UTF_CPP_CPLUSPLUS (201103L)
#include <utf8.h>

#include "main.h"

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
			PSTR lpCmdLine, int nCmdShow)
{
	auto cmdLine = GetCommandLineW();
	int argc = 0;
	auto argvw = CommandLineToArgvW(cmdLine, &argc);

	std::vector<std::string> argvconv;

	for (int i = 0; i < argc; i++)
	{
		auto argw = argvw[i];
		auto arg = utf8::utf16to8(std::u16string((const char16_t *)argw));
		argvconv.push_back(arg);
	}

	char **argv = new char *[argc];

	for (int i = 0; i < argc; i++)
	{
		argv[i] = argvconv[i].data();
	}

	int retCode = 0;
	try {
		retCode = app_main(argc, argv);
	} catch (std::exception &e) {
		alert(e.what());
	}
	

	delete[] argv;

	return retCode;
}

int main(int argc, char **argv) {
	int retCode = 0;
	try {
		retCode = app_main(argc, argv);
	} catch (std::exception &e) {
		alert(e.what());
	}
	return retCode;
}

void alert(std::string msg)
{
	auto u16msg = utf8::utf8to16(msg);
	MessageBoxW(NULL, (wchar_t *)u16msg.data(), NULL, MB_OK);
}

void open_url(std::string url)
{
	auto u16url = utf8::utf8to16(url);
	ShellExecuteW(0, 0, (wchar_t *)u16url.data(), 0, 0, SW_SHOW);
}

#endif
