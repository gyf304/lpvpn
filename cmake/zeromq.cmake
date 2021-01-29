include(FetchContent)

FetchContent_Declare(
	zeromq
	URL https://github.com/zeromq/libzmq/releases/download/v4.3.4/zeromq-4.3.4.zip
)
FetchContent_GetProperties(zeromq)
if(NOT zeromq_POPULATED)
	FetchContent_Populate(zeromq)
endif()

set(ENABLE_CURVE OFF CACHE INTERNAL "")
set(BUILD_TESTS OFF CACHE INTERNAL "")

add_subdirectory("${zeromq_SOURCE_DIR}")
