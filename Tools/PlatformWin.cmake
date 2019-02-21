add_subdirectory(ImageDiff)

if (ENABLE_WEBKIT_LEGACY)
    add_subdirectory(DumpRenderTree)
    add_subdirectory(MiniBrowser/win)
endif ()

if (ENABLE_WEBKIT)
    add_subdirectory(WebKitTestRunner)
endif ()
