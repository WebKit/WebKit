set(wrapper_DEFINITIONS USE_CONSOLE_ENTRY_POINT)

WEBKIT_WRAP_EXECUTABLE(jsc
    SOURCES DLLLauncherMain.cpp
    LIBRARIES shlwapi
)
target_compile_definitions(jsc PRIVATE ${wrapper_DEFINITIONS})

if (DEVELOPER_MODE)
    WEBKIT_WRAP_EXECUTABLE(testapi
        SOURCES DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(testapi PRIVATE ${wrapper_DEFINITIONS})

    WEBKIT_WRAP_EXECUTABLE(testRegExp
        SOURCES DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(testRegExp PRIVATE ${wrapper_DEFINITIONS})

    WEBKIT_WRAP_EXECUTABLE(testmasm
        SOURCES DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(testmasm PRIVATE ${wrapper_DEFINITIONS})

    WEBKIT_WRAP_EXECUTABLE(testb3
        SOURCES DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(testb3 PRIVATE ${wrapper_DEFINITIONS})

    WEBKIT_WRAP_EXECUTABLE(testair
        SOURCES DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(testair PRIVATE ${wrapper_DEFINITIONS})

    WEBKIT_WRAP_EXECUTABLE(testdfg
        SOURCES DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(testdfg PRIVATE ${wrapper_DEFINITIONS})
endif ()
