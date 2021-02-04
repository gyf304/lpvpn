include(FetchContent)

FetchContent_Declare(
    Wintun
    URL https://www.wintun.net/builds/wintun-0.10.zip
)

FetchContent_GetProperties(Wintun)
if (NOT wintun_POPULATED)
	FetchContent_Populate(Wintun)
endif()

set (Wintun_INCLUDE_DIR "${wintun_SOURCE_DIR}/include")

if ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "x64")
	set (Wintun_REDISTRIBUTABLE "${wintun_SOURCE_DIR}/bin/amd64/wintun.dll")
	set (Wintun_REDISTRIBUTABLE_DIR "${wintun_SOURCE_DIR}/bin/amd64")
elseif ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "win32")
	set (Wintun_REDISTRIBUTABLE "${wintun_SOURCE_DIR}/bin/x86/wintun.dll")
	set (Wintun_REDISTRIBUTABLE_DIR "${wintun_SOURCE_DIR}/bin/x86")
endif()

add_library(Wintun INTERFACE)
target_include_directories(Wintun INTERFACE "${Wintun_INCLUDE_DIR}")
