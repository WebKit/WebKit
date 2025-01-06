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

if (${CMAKE_GENERATOR} MATCHES "Visual Studio")
    # Set the debugger working directory for Visual Studio
    set_target_properties(jsc PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

    # Set the startup target to JSC if WebCore disabled
    if (NOT ENABLE_WEBCORE)
        set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT jsc)
    endif ()
endif ()

list(APPEND jsc_LIBRARIES ${MEMORY_EXTRA_LIB})

if (DEVELOPER_MODE)
    list(APPEND testapi_LIBRARIES ${MEMORY_EXTRA_LIB})
    list(APPEND testmasm_LIBRARIES ${MEMORY_EXTRA_LIB})
    list(APPEND testRegExp_LIBRARIES ${MEMORY_EXTRA_LIB})
    list(APPEND testb3_LIBRARIES ${MEMORY_EXTRA_LIB})
    list(APPEND testair_LIBRARIES ${MEMORY_EXTRA_LIB})
    list(APPEND testdfg_LIBRARIES ${MEMORY_EXTRA_LIB})
endif ()
