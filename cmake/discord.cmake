# - Try to find the DiscordGameSDK library
#
# Once done, this will define:
#
#  DiscordGameSDK_INCLUDE_DIR - the DiscordGameSDK include directory
#  DiscordGameSDK_LIBRARIES - The libraries needed to use DiscordGameSDK

include(FetchContent)

FetchContent_Declare(
	DiscordGameSDK
	URL https://dl-game-sdk.discordapp.net/2.5.6/discord_game_sdk.zip
)

if (NOT DiscordGameSDK_SDKROOT)
	FetchContent_GetProperties(DiscordGameSDK)
	if (NOT discordgamesdk_POPULATED)
		FetchContent_Populate(DiscordGameSDK)
		message (STATUS "POPULATE ${discordgamesdk_SOURCE_DIR}")
		set(DiscordGameSDK_SDKROOT "${discordgamesdk_SOURCE_DIR}")
	endif()
	message (STATUS "DiscordGameSDK_SDKROOT at ${DiscordGameSDK_SDKROOT}")
endif ()

if (NOT DiscordGameSDK_INCLUDE_DIR OR NOT DiscordGameSDK_LIBRARIES OR NOT DiscordGameSDK_SOURCES)
	file(GLOB DiscordGameSDK_CPP_SOURCES "${DiscordGameSDK_SDKROOT}/cpp/*.cpp")
	set (DiscordGameSDK_INCLUDE_DIR "${DiscordGameSDK_SDKROOT}/c")
	set (DiscordGameSDK_CPP_INCLUDE_DIR "${DiscordGameSDK_SDKROOT}/cpp")

	if (WIN32)
		message (STATUS "Using DiscordGameSDK library for Windows")
		if ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "x64")
			set (DiscordGameSDK_LIBRARY "${DiscordGameSDK_SDKROOT}/lib/x86_64/discord_game_sdk.dll.lib")
			set (DiscordGameSDK_LIBRARIES "${DiscordGameSDK_LIBRARY}")
			set (DiscordGameSDK_REDISTRIBUTABLE "${DiscordGameSDK_SDKROOT}/lib/x86_64/discord_game_sdk.dll")
		elseif ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "win32")
			set (DiscordGameSDK_LIBRARY "${DiscordGameSDK_SDKROOT}/lib/x86/discord_game_sdk.dll.lib")
			set (DiscordGameSDK_LIBRARIES "${DiscordGameSDK_LIBRARY}")
			set (DiscordGameSDK_REDISTRIBUTABLE "${DiscordGameSDK_SDKROOT}/lib/x86/discord_game_sdk.dll")
		endif()
	endif()
endif ()


message(STATUS "DiscordGameSDK_LIBRARIES ${DiscordGameSDK_LIBRARIES}")
add_library(DiscordGameSDK SHARED IMPORTED)
set_target_properties(DiscordGameSDK PROPERTIES IMPORTED_LOCATION "${DiscordGameSDK_REDISTRIBUTABLE}")
set_target_properties(DiscordGameSDK PROPERTIES IMPORTED_IMPLIB "${DiscordGameSDK_LIBRARY}")
target_include_directories(DiscordGameSDK INTERFACE "${DiscordGameSDK_INCLUDE_DIR}")

add_library(DiscordGameSDK_cpp STATIC ${DiscordGameSDK_CPP_SOURCES})
target_link_libraries(DiscordGameSDK_cpp DiscordGameSDK)
target_include_directories(DiscordGameSDK_cpp PUBLIC "${DiscordGameSDK_CPP_INCLUDE_DIR}")
add_library(DiscordGameSDK::cpp ALIAS DiscordGameSDK_cpp)
