if (DEVELOPER_MODE OR ENABLE_MINIBROWSER)
    add_subdirectory(wpe/backends)
endif ()

if (DEVELOPER_MODE)
    add_subdirectory(ImageDiff)
    add_subdirectory(TestRunnerShared)
    add_subdirectory(WebKitTestRunner)

    if (ENABLE_API_TESTS)
        add_subdirectory(TestWebKitAPI/glib)
    endif ()
endif ()

if (ENABLE_MINIBROWSER)
    add_subdirectory(MiniBrowser/wpe)
endif ()
