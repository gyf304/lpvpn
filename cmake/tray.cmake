include(FetchContent)

FetchContent_Declare(
	tray
	GIT_REPOSITORY https://github.com/meshsocket/tray
)

FetchContent_GetProperties(tray)
if(NOT tray_POPULATED)
	FetchContent_Populate(tray)
endif()

set(tray_INCLUDE_DIR "${tray_SOURCE_DIR}")
if (WIN32)
	add_definitions(-DTRAY_WINAPI=1)
endif()

add_library(tray INTERFACE)
target_include_directories(tray INTERFACE "${tray_INCLUDE_DIR}")
