add_subdirectory(ImageDiff)

if (ENABLE_WEBKIT OR ENABLE_WEBKIT_LEGACY)
    add_subdirectory(MiniBrowser/win)
endif ()

add_subdirectory(TestRunnerShared)

if (ENABLE_WEBKIT_LEGACY)
    add_subdirectory(DumpRenderTree)
endif ()

if (ENABLE_WEBKIT)
    add_subdirectory(WebKitTestRunner)
endif ()
