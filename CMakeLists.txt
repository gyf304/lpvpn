cmake_minimum_required(VERSION 3.11)
project(MeshSocket)

set(DISCORD_APP_ID 801910914370240553)

set(CMAKE_CXX_STANDARD 17)
include(FetchContent)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(cmake/discord.cmake)
include(cmake/tray.cmake)
include(cmake/utfcpp.cmake)
include(cmake/wintun.cmake)
include(cmake/zeromq.cmake)
include(cmake/cppzmq.cmake)

if (WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.cpp")
add_executable(MeshSocket ${SOURCES})

# copy discord SDK dll
add_custom_command(
	TARGET MeshSocket POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${DiscordGameSDK_REDISTRIBUTABLE}
			$<TARGET_FILE_DIR:MeshSocket>
)

# copy Wintun dll
add_custom_command(
	TARGET MeshSocket POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${Wintun_REDISTRIBUTABLE}
			$<TARGET_FILE_DIR:MeshSocket>
)

# copy icon
add_custom_command(
	TARGET MeshSocket POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_SOURCE_DIR}/resources/icon.ico
			$<TARGET_FILE_DIR:MeshSocket>
)

target_compile_definitions(
	MeshSocket PUBLIC
	DISCORD_APP_ID=${DISCORD_APP_ID}
)

if (WIN32)
	set_target_properties(MeshSocket PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /SUBSYSTEM:WINDOWS")
	# set_target_properties(MeshSocket PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
endif()

target_link_libraries(MeshSocket 
	PRIVATE cppzmq-static DiscordGameSDK::cpp utf8::cpp tray Wintun
)
