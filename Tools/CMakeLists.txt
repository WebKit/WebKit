set_property(DIRECTORY . PROPERTY FOLDER "Tools")

# Individual subdirectories are added in the port's cmake file
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

if (ENABLE_LAYOUT_TESTS)
    add_subdirectory(TestRunnerShared)

    if (ENABLE_IMAGE_DIFF)
        add_subdirectory(ImageDiff)
    endif ()

    if (ENABLE_WEBKIT_LEGACY)
        add_subdirectory(DumpRenderTree)
    endif ()

    if (ENABLE_WEBKIT)
        add_subdirectory(WebKitTestRunner)
    endif ()
endif ()

if (ENABLE_API_TESTS)
    add_subdirectory(TestWebKitAPI)
endif ()
