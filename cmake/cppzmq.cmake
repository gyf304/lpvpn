include(FetchContent)

FetchContent_Declare(
    cppzmq
    URL https://github.com/zeromq/cppzmq/archive/v4.7.1.zip
)

FetchContent_GetProperties(cppzmq)
if (NOT cppzmq_POPULATED)
	FetchContent_Populate(cppzmq)
endif()

set(CPPZMQ_BUILD_TESTS OFF CACHE INTERNAL "")

add_subdirectory("${cppzmq_SOURCE_DIR}")
