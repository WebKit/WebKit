add_subdirectory(DumpRenderTree)
add_subdirectory(ImageDiff)

if (ENABLE_WEBKIT_LEGACY)
    add_subdirectory(MiniBrowser/win)
endif ()
