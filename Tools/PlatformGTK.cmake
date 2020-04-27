if (DEVELOPER_MODE)
    if (NOT USE_GTK4)
        add_subdirectory(WebKitTestRunner)
    endif ()

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
