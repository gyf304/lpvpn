cmake_minimum_required(VERSION 3.16)

project(lpvpn)

message(STATUS "Platform: ${PLATFORM}")
message(STATUS "Generator Platform: ${CMAKE_GENERATOR_PLATFORM}")

set(DISCORD_APP_ID 801910914370240553)

set(CMAKE_CXX_STANDARD 17)
include(FetchContent)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tmp")

include(cmake/discord.cmake)
include(cmake/wintun.cmake)
include(cmake/wxWidgets.cmake)
include(cmake/CMakeRC.cmake)
include(cmake/MemoryModule.cmake)

if (WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()


file(GLOB_RECURSE RESOURCES RELATIVE ${CMAKE_SOURCE_DIR} "resources/cmrc/*")
cmrc_add_resource_library(lpvpn-resources ALIAS lpvpn::rc NAMESPACE lpvpn)
cmrc_add_resources(lpvpn-resources WHENCE resources/cmrc ${RESOURCES})
cmrc_add_resources(lpvpn-resources WHENCE ${DiscordGameSDK_REDISTRIBUTABLE_DIR} ${DiscordGameSDK_REDISTRIBUTABLE})
cmrc_add_resources(lpvpn-resources WHENCE ${Wintun_REDISTRIBUTABLE_DIR} ${Wintun_REDISTRIBUTABLE})

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.cpp")
add_executable(lpvpn ${SOURCES} ${DiscordGameSDK_CPP_SOURCES} resources/windows/app.rc)
target_include_directories(lpvpn PRIVATE ${DiscordGameSDK_CPP_INCLUDE_DIR})

target_compile_definitions(
	lpvpn PUBLIC
	DISCORD_APP_ID=${DISCORD_APP_ID}
)

if (WIN32)
	set_target_properties(lpvpn PROPERTIES
		LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /SUBSYSTEM:WINDOWS"
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	)
endif()

target_link_libraries(lpvpn PRIVATE wxmono lpvpn::rc Wintun DiscordGameSDK MemoryModule)

# copy LICENSE
add_custom_command(
	TARGET lpvpn POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_SOURCE_DIR}/LICENSE"
			$<TARGET_FILE_DIR:lpvpn>
)
