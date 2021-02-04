
include(FetchContent)

FetchContent_Declare(
	MemoryModule
	URL https://github.com/fancycode/MemoryModule/archive/v0_0_4.zip
)
FetchContent_GetProperties(MemoryModule)
if(NOT memorymodule_POPULATED)
	FetchContent_Populate(memorymodule)
endif()

set(MemoryModule_INCLUDE_DIR "${memorymodule_SOURCE_DIR}")

add_library(MemoryModule STATIC "${memorymodule_SOURCE_DIR}/MemoryModule.c")
target_include_directories(MemoryModule PUBLIC "${MemoryModule_INCLUDE_DIR}")
