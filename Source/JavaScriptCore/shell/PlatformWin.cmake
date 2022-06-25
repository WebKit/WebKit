set(wrapper_DEFINITIONS USE_CONSOLE_ENTRY_POINT)

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND wrapper_DEFINITIONS WIN_CAIRO)
endif ()

WEBKIT_WRAP_EXECUTABLE(jsc
    SOURCES DLLLauncherMain.cpp
    LIBRARIES shlwapi
)
target_compile_definitions(jsc PRIVATE ${wrapper_DEFINITIONS})
set(jsc_OUTPUT_NAME jsc${DEBUG_SUFFIX})

if (DEVELOPER_MODE)
    WEBKIT_WRAP_EXECUTABLE(testapi
        SOURCES DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(testapi PRIVATE ${wrapper_DEFINITIONS})
    set(testapi_OUTPUT_NAME testapi${DEBUG_SUFFIX})

    WEBKIT_WRAP_EXECUTABLE(testRegExp
        SOURCES DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(testRegExp PRIVATE ${wrapper_DEFINITIONS})
    set(testRegExp_OUTPUT_NAME testRegExp${DEBUG_SUFFIX})

    WEBKIT_WRAP_EXECUTABLE(testmasm
        SOURCES DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(testmasm PRIVATE ${wrapper_DEFINITIONS})
    set(testmasm_OUTPUT_NAME testmasm${DEBUG_SUFFIX})

    WEBKIT_WRAP_EXECUTABLE(testb3
        SOURCES DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(testb3 PRIVATE ${wrapper_DEFINITIONS})
    set(testb3_OUTPUT_NAME testb3${DEBUG_SUFFIX})

    WEBKIT_WRAP_EXECUTABLE(testair
        SOURCES DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(testair PRIVATE ${wrapper_DEFINITIONS})
    set(testair_OUTPUT_NAME testair${DEBUG_SUFFIX})

    WEBKIT_WRAP_EXECUTABLE(testdfg
        SOURCES DLLLauncherMain.cpp
        LIBRARIES shlwapi
    )
    target_compile_definitions(testdfg PRIVATE ${wrapper_DEFINITIONS})
    set(testdfg_OUTPUT_NAME testdfg${DEBUG_SUFFIX})
endif ()
