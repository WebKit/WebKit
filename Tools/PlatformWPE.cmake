if (ENABLE_API_TESTS OR ENABLE_LAYOUT_TESTS OR ENABLE_MINIBROWSER)
    add_subdirectory(wpe/backends)
endif ()

if (ENABLE_LAYOUT_TESTS)
    add_subdirectory(ImageDiff)
    add_subdirectory(TestRunnerShared)
    add_subdirectory(WebKitTestRunner)
endif ()

if (DEVELOPER_MODE)
    add_subdirectory(flatpak)
endif ()

if (ENABLE_API_TESTS)
    add_subdirectory(TestWebKitAPI/glib)
endif ()

if (ENABLE_MINIBROWSER)
    add_subdirectory(MiniBrowser/wpe)
endif ()

if (ENABLE_COG)
    include(ExternalProject)
    if ("${WPE_COG_PLATFORMS}" STREQUAL "")
        set(WPE_COG_PLATFORMS "drm,headless,gtk4,x11,wayland")
    elseif ("${WPE_COG_PLATFORMS}" STREQUAL "none")
        set(WPE_COG_PLATFORMS "")
    endif ()
    if (DEFINED ENV{PKG_CONFIG_PATH})
        set(WPE_COG_PKG_CONFIG_PATH ${CMAKE_BINARY_DIR}:$ENV{PKG_CONFIG_PATH})
    else ()
        set(WPE_COG_PKG_CONFIG_PATH ${CMAKE_BINARY_DIR})
    endif ()
    if ("${WPE_COG_REPO}" STREQUAL "")
        set(WPE_COG_REPO "https://github.com/Igalia/cog.git")
    endif ()
    if ("${WPE_COG_TAG}" STREQUAL "")
        set(WPE_COG_TAG "origin/master")
    endif ()
    # TODO Use GIT_REMOTE_UPDATE_STRATEGY with 3.18 to allow switching between
    # conflicting branches without having to delete the repo

    # Convert a few options to their Meson equivalents
    if (USE_SOUP2)
        set(COG_MESON_SOUP2 enabled)
    else ()
        set(COG_MESON_SOUP2 disabled)
    endif ()

    string(TOLOWER "${CMAKE_BUILD_TYPE}" COG_MESON_BUILDTYPE)
    if (COG_MESON_BUILDTYPE STREQUAL "relwithdebinfo")
        set(COG_MESON_BUILDTYPE debugoptimized)
    elseif (COG_MESON_BUILDTYPE STREQUAL "minsizerel")
        set(COG_MESON_BUILDTYPE minsize)
    elseif (NOT (COG_MESON_BUILDTYPE STREQUAL release OR COG_MESON_BUILDTYPE STREQUAL debug))
        set(COG_MESON_BUILDTYPE debugoptimized)
    endif ()

    ExternalProject_Add(cog
        GIT_REPOSITORY "${WPE_COG_REPO}"
        GIT_TAG "${WPE_COG_TAG}"
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/Tools/wpe/cog"
        BUILD_IN_SOURCE FALSE
        CONFIGURE_COMMAND
            meson setup <BINARY_DIR> <SOURCE_DIR>
            --buildtype ${COG_MESON_BUILDTYPE}
            --pkg-config-path ${WPE_COG_PKG_CONFIG_PATH}
            -Dsoup2=${COG_MESON_SOUP2}
            -Dplatforms=${WPE_COG_PLATFORMS}
        BUILD_COMMAND
            meson compile -C <BINARY_DIR>
        INSTALL_COMMAND "")
    ExternalProject_Add_StepDependencies(cog build WebKit)
endif ()
