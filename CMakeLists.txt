cmake_minimum_required(VERSION 3.11)

project(lpvpn)

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

if (WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.cpp")
add_executable(lpvpn ${SOURCES})

file(GLOB_RECURSE RESOURCES RELATIVE ${CMAKE_SOURCE_DIR} "resources/*")
cmrc_add_resource_library(lpvpn-resources ALIAS lpvpn::rc NAMESPACE lpvpn WHENCE resources ${RESOURCES})

# copy discord SDK dll
add_custom_command(
	TARGET lpvpn POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${DiscordGameSDK_REDISTRIBUTABLE}
			$<TARGET_FILE_DIR:lpvpn>
)

# copy Wintun dll
add_custom_command(
	TARGET lpvpn POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${Wintun_REDISTRIBUTABLE}
			$<TARGET_FILE_DIR:lpvpn>
)

# copy nsis resources
file(GLOB_RECURSE NSIS_SOURCES  "${CMAKE_SOURCE_DIR}/nsis/*")
foreach(NSIS_SOURCE ${NSIS_SOURCES})
	add_custom_command(
		TARGET lpvpn POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
				${NSIS_SOURCE}
				$<TARGET_FILE_DIR:lpvpn>
	)
endforeach(NSIS_SOURCE)

target_compile_definitions(
	lpvpn PUBLIC
	DISCORD_APP_ID=${DISCORD_APP_ID}
)

if (WIN32)
	set_target_properties(lpvpn PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /SUBSYSTEM:WINDOWS")
endif()

target_link_libraries(lpvpn 
	PRIVATE wxmono lpvpn::rc DiscordGameSDK::cpp Wintun
)

add_custom_target(lpvpn-standalone ALL
	COMMAND makensis lpvpn-standalone.nsi
	WORKING_DIRECTORY $<TARGET_FILE_DIR:lpvpn>
	DEPENDS lpvpn
)
