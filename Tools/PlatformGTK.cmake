if (DEVELOPER_MODE)
    add_subdirectory(WebKitTestRunner)
    add_subdirectory(ImageDiff)

    if (ENABLE_API_TESTS)
        add_subdirectory(TestWebKitAPI/glib)
    endif ()

    if (ENABLE_NETSCAPE_PLUGIN_API)
        add_subdirectory(DumpRenderTree/TestNetscapePlugIn)
    endif ()
endif ()

if (ENABLE_MINIBROWSER)
  add_subdirectory(MiniBrowser/gtk)
endif ()
