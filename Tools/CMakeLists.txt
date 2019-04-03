set_property(DIRECTORY . PROPERTY FOLDER "Tools")
include(frameworks/WTF)

# Individual subdirectories are added in the port's cmake file
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

if (ENABLE_API_TESTS)
    add_subdirectory(TestWebKitAPI)
endif ()
