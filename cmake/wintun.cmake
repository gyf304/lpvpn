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

if ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "AMD64")
	set (Wintun_REDISTRIBUTABLE "${wintun_SOURCE_DIR}/bin/amd64/wintun.dll")
elseif ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86")
	set (Wintun_REDISTRIBUTABLE "${wintun_SOURCE_DIR}/bin/x86/wintun.dll")
elseif ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "ARM")
	set (Wintun_REDISTRIBUTABLE "${wintun_SOURCE_DIR}/bin/arm/wintun.dll")
elseif ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "ARM64")
	set (Wintun_REDISTRIBUTABLE "${wintun_SOURCE_DIR}/bin/arm64/wintun.dll")
endif()

add_library(Wintun INTERFACE)
target_include_directories(Wintun INTERFACE "${Wintun_INCLUDE_DIR}")
