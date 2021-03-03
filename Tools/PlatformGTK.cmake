if (DEVELOPER_MODE)
    add_subdirectory(TestRunnerShared)
    add_subdirectory(WebKitTestRunner)
    add_subdirectory(ImageDiff)
    add_subdirectory(flatpak)

    if (ENABLE_API_TESTS)
        add_subdirectory(TestWebKitAPI/glib)
    endif ()
endif ()

if (ENABLE_MINIBROWSER)
  add_subdirectory(MiniBrowser/gtk)
endif ()
