list(APPEND testmem_SOURCES
    ${JAVASCRIPTCORE_DIR}/shell/playstation/Initializer.cpp
)

# Get the necessary wrappers for C functions to make jsc shell
# able to properly run tests. Depending on version, first try
# using find_package for new version and if that doesn't work
# fallback to using find_library for older versions.
find_package(libtestwrappers)
list(APPEND testmem_LIBRARIES libtestwrappers::testwrappers)

set(PLAYSTATION_testmem_PROCESS_NAME "testmem")
set(PLAYSTATION_testmem_MAIN_THREAD_NAME "testmem")
set(PLAYSTATION_testmem_REQUIRED_SHARED_LIBRARIES libicu)

if (${CMAKE_GENERATOR} MATCHES "Visual Studio")
    # Set the debugger working directory for Visual Studio
    set_target_properties(testmem PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif ()

# For the simple version, we could use find_library in
# Source/cmake/OptionsPlayStation.cmake, make a target with
# something like add_library(showmem static IMPORTED) and
# then set the location with set_target_properties on the
# IMPORTED_LOCATION property, and then in the playstation
# config for testmem adding showmem to the libraries list.

find_library(SHOWMAP_LIB showmap)
list(APPEND testmem_LIBRARIES ${SHOWMAP_LIB})

find_path(SHOWMAP_INCLUDE_DIR NAMES showmap.h)
list(APPEND testmem_INCLUDE_DIRECTORIES ${SHOWMAP_INCLUDE_DIR})
