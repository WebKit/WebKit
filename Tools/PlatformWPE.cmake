if (DEVELOPER_MODE OR ENABLE_MINIBROWSER)
    add_subdirectory(wpe/backends)
endif ()

if (DEVELOPER_MODE)
    add_subdirectory(ImageDiff)
    add_subdirectory(TestRunnerShared)
    add_subdirectory(WebKitTestRunner)
    add_subdirectory(flatpak)

    if (ENABLE_API_TESTS)
        add_subdirectory(TestWebKitAPI/glib)
    endif ()
endif ()

if (ENABLE_MINIBROWSER)
    add_subdirectory(MiniBrowser/wpe)
endif ()

if (DEVELOPER_MODE AND ENABLE_COG)
    include(ExternalProject)
    ExternalProject_Add(cog
        GIT_REPOSITORY "https://github.com/Igalia/cog.git"
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/Tools/wpe/cog"
        INSTALL_COMMAND "")
    ExternalProject_Add_StepDependencies(cog build WebKit)
endif ()
