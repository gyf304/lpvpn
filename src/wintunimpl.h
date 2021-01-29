#include <winsock2.h>
#include <Windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <wintun.h>

static WINTUN_CREATE_ADAPTER_FUNC WintunCreateAdapter;
static WINTUN_DELETE_ADAPTER_FUNC WintunDeleteAdapter;
static WINTUN_DELETE_POOL_DRIVER_FUNC WintunDeletePoolDriver;
static WINTUN_ENUM_ADAPTERS_FUNC WintunEnumAdapters;
static WINTUN_FREE_ADAPTER_FUNC WintunFreeAdapter;
static WINTUN_OPEN_ADAPTER_FUNC WintunOpenAdapter;
static WINTUN_GET_ADAPTER_LUID_FUNC WintunGetAdapterLUID;
static WINTUN_GET_ADAPTER_NAME_FUNC WintunGetAdapterName;
static WINTUN_SET_ADAPTER_NAME_FUNC WintunSetAdapterName;
static WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC WintunGetRunningDriverVersion;
static WINTUN_SET_LOGGER_FUNC WintunSetLogger;
static WINTUN_START_SESSION_FUNC WintunStartSession;
static WINTUN_END_SESSION_FUNC WintunEndSession;
static WINTUN_GET_READ_WAIT_EVENT_FUNC WintunGetReadWaitEvent;
static WINTUN_RECEIVE_PACKET_FUNC WintunReceivePacket;
static WINTUN_RELEASE_RECEIVE_PACKET_FUNC WintunReleaseReceivePacket;
static WINTUN_ALLOCATE_SEND_PACKET_FUNC WintunAllocateSendPacket;
static WINTUN_SEND_PACKET_FUNC WintunSendPacket;

static HMODULE
InitializeWintun(void)
{
	HMODULE Wintun =
		LoadLibraryExW(L"wintun.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!Wintun)
		return NULL;
#define LOAD(Name, Type) ((Name = (Type)GetProcAddress(Wintun, #Name)) == NULL)
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
		FreeLibrary(Wintun);
		SetLastError(LastError);
		return NULL;
	}
	return Wintun;
}
