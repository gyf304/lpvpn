include(FetchContent)

FetchContent_Declare(
	wxWidgets
	URL https://github.com/wxWidgets/wxWidgets/releases/download/v3.1.4/wxWidgets-3.1.4.zip
)
FetchContent_GetProperties(wxWidgets)
if(NOT wxwidgets_POPULATED)
	FetchContent_Populate(wxwidgets)
endif()

set(wxBUILD_MONOLITHIC ON)
set(wxBUILD_SHARED OFF)
set(wxBUILD_USE_STATIC_RUNTIME ON)
set(wxBUILD_MSVC_MULTIPROC ON)
add_subdirectory("${wxwidgets_SOURCE_DIR}")
