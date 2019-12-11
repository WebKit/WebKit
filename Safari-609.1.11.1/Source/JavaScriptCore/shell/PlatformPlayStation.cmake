list(REMOVE_ITEM jsc_SOURCES ../jsc.cpp)
list(APPEND jsc_SOURCES
    ${JAVASCRIPTCORE_DIR}/shell/playstation/TestShell.cpp
    ${JAVASCRIPTCORE_DIR}/shell/playstation/Initializer.cpp
)

find_library(LIBTESTWRAPPERS testwrappers PATHS ${WEBKIT_LIBRARIES_DIR}/lib)

set(PLAYSTATION_jsc_PROCESS_NAME "JSCShell")
set(PLAYSTATION_jsc_MAIN_THREAD_NAME "JSCShell")
set(PLAYSTATION_jsc_WRAP fopen getcwd chdir main)
list(APPEND jsc_LIBRARIES ${LIBTESTWRAPPERS})
