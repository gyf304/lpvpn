include(FetchContent)

FetchContent_Declare(
	utfcpp
	URL https://github.com/nemtrif/utfcpp/archive/v3.1.2.zip
)
FetchContent_GetProperties(utfcpp)
if(NOT utfcpp_POPULATED)
	FetchContent_Populate(utfcpp)
	set(UTF8_TESTS OFF CACHE INTERNAL "")
	set(UTF8_INSTALL OFF CACHE INTERNAL "")
	set(UTF8_SAMPLES OFF CACHE INTERNAL "")
endif()

add_subdirectory("${utfcpp_SOURCE_DIR}")
