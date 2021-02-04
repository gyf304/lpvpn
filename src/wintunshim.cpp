#include <MemoryModule.h>

#include "wintunshim.h"
#include "rc.h"

WINTUN_CREATE_ADAPTER_FUNC WintunCreateAdapter;
WINTUN_DELETE_ADAPTER_FUNC WintunDeleteAdapter;
WINTUN_DELETE_POOL_DRIVER_FUNC WintunDeletePoolDriver;
WINTUN_ENUM_ADAPTERS_FUNC WintunEnumAdapters;
WINTUN_FREE_ADAPTER_FUNC WintunFreeAdapter;
WINTUN_OPEN_ADAPTER_FUNC WintunOpenAdapter;
WINTUN_GET_ADAPTER_LUID_FUNC WintunGetAdapterLUID;
WINTUN_GET_ADAPTER_NAME_FUNC WintunGetAdapterName;
WINTUN_SET_ADAPTER_NAME_FUNC WintunSetAdapterName;
WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC WintunGetRunningDriverVersion;
WINTUN_SET_LOGGER_FUNC WintunSetLogger;
WINTUN_START_SESSION_FUNC WintunStartSession;
WINTUN_END_SESSION_FUNC WintunEndSession;
WINTUN_GET_READ_WAIT_EVENT_FUNC WintunGetReadWaitEvent;
WINTUN_RECEIVE_PACKET_FUNC WintunReceivePacket;
WINTUN_RELEASE_RECEIVE_PACKET_FUNC WintunReleaseReceivePacket;
WINTUN_ALLOCATE_SEND_PACKET_FUNC WintunAllocateSendPacket;
WINTUN_SEND_PACKET_FUNC WintunSendPacket;

HMODULE
InitializeWintun(void)
{
    auto file = lpvpn::fs.open("wintun.dll");
    auto Wintun = MemoryLoadLibrary(file.begin());

	if (!Wintun)
		return NULL;
#define LOAD(Name, Type) ((Name = (Type)MemoryGetProcAddress(Wintun, #Name)) == NULL)
	if (
		LOAD(WintunCreateAdapter, WINTUN_CREATE_ADAPTER_FUNC) ||
		LOAD(WintunDeleteAdapter, WINTUN_DELETE_ADAPTER_FUNC) ||
		LOAD(WintunDeletePoolDriver, WINTUN_DELETE_POOL_DRIVER_FUNC) ||
		LOAD(WintunEnumAdapters, WINTUN_ENUM_ADAPTERS_FUNC) ||
		LOAD(WintunFreeAdapter, WINTUN_FREE_ADAPTER_FUNC) ||
		LOAD(WintunOpenAdapter, WINTUN_OPEN_ADAPTER_FUNC) ||
		LOAD(WintunGetAdapterLUID, WINTUN_GET_ADAPTER_LUID_FUNC) ||
		LOAD(WintunGetAdapterName, WINTUN_GET_ADAPTER_NAME_FUNC) ||
		LOAD(WintunSetAdapterName, WINTUN_SET_ADAPTER_NAME_FUNC) ||
		LOAD(WintunGetRunningDriverVersion, WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC) ||
		LOAD(WintunSetLogger, WINTUN_SET_LOGGER_FUNC) ||
		LOAD(WintunStartSession, WINTUN_START_SESSION_FUNC) ||
		LOAD(WintunEndSession, WINTUN_END_SESSION_FUNC) ||
		LOAD(WintunGetReadWaitEvent, WINTUN_GET_READ_WAIT_EVENT_FUNC) ||
		LOAD(WintunReceivePacket, WINTUN_RECEIVE_PACKET_FUNC) ||
		LOAD(WintunReleaseReceivePacket, WINTUN_RELEASE_RECEIVE_PACKET_FUNC) ||
		LOAD(WintunAllocateSendPacket, WINTUN_ALLOCATE_SEND_PACKET_FUNC) ||
		LOAD(WintunSendPacket, WINTUN_SEND_PACKET_FUNC))
#undef LOAD
	{
		DWORD LastError = GetLastError();
		MemoryFreeLibrary(Wintun);
		SetLastError(LastError);
		return NULL;
	}
	return (HMODULE) Wintun;
}
