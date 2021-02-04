include(FetchContent)

FetchContent_Declare(
	CMakeRC
	GIT_REPOSITORY https://github.com/vector-of-bool/cmrc
)

FetchContent_GetProperties(CMakeRC)
if(NOT cmakerc_POPULATED)
	FetchContent_Populate(cmakerc)
endif()

add_subdirectory("${cmakerc_SOURCE_DIR}")
