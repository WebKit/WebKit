list(REMOVE_ITEM jsc_SOURCES ../jsc.cpp)
list(APPEND jsc_SOURCES
    ${JAVASCRIPTCORE_DIR}/shell/playstation/TestShell.cpp
    ${JAVASCRIPTCORE_DIR}/shell/playstation/Initializer.cpp
)

# Get the necessary wrappers for C functions to make jsc shell
# able to properly run tests. Depending on version, first try
# using find_package for new version and if that doesn't work
# fallback to using find_library for older versions.
find_package(libtestwrappers)
if (TARGET libtestwrappers::testwrappers)
    list(APPEND jsc_LIBRARIES libtestwrappers::testwrappers)
else ()
    find_library(LIBTESTWRAPPERS testwrappers PATHS ${WEBKIT_LIBRARIES_DIR}/lib)
    set(PLAYSTATION_jsc_WRAP fopen getcwd chdir main)
    list(APPEND jsc_LIBRARIES ${LIBTESTWRAPPERS})
endif ()

set(PLAYSTATION_jsc_PROCESS_NAME "JSCShell")
set(PLAYSTATION_jsc_MAIN_THREAD_NAME "JSCShell")

# Set the debugger working directory for Visual Studio
if (${CMAKE_GENERATOR} MATCHES "Visual Studio")
    set_target_properties(jsc PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif ()
