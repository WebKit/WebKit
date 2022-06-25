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
    if ("${WPE_COG_REPO}" STREQUAL "")
        set(WPE_COG_REPO "https://github.com/Igalia/cog.git")
    endif ()
    if ("${WPE_COG_TAG}" STREQUAL "")
        set(WPE_COG_TAG "origin/master")
    endif ()
    # TODO Use GIT_REMOTE_UPDATE_STRATEGY with 3.18 to allow switching between
    # conflicting branches without having to delete the repo
    ExternalProject_Add(cog
        GIT_REPOSITORY "${WPE_COG_REPO}"
        GIT_TAG "${WPE_COG_TAG}"
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/Tools/wpe/cog"
        CMAKE_ARGS "-DCOG_PLATFORM_GTK4=ON" "-DCOG_PLATFORM_HEADLESS=ON" "-DCOG_PLATFORM_X11=ON" "-DUSE_SOUP2=${USE_SOUP2}"
        INSTALL_COMMAND "")
    ExternalProject_Add_StepDependencies(cog build WebKit)
endif ()
