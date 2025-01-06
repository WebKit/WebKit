if (ENABLE_API_TESTS OR ENABLE_LAYOUT_TESTS OR ENABLE_MINIBROWSER)
    add_subdirectory(wpe/backends)
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
    find_program(MESON_EXE NAMES meson)
    execute_process(
        COMMAND "${MESON_EXE}" --version
        OUTPUT_VARIABLE MESON_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
        ERROR_QUIET
    )
    if ("${MESON_VERSION}" VERSION_GREATER 1.1.0)
        set(MESON_RECONFIGURE flag)
    else ()
        set(MESON_RECONFIGURE remove)
    endif ()

    if ("${WPE_COG_PLATFORMS}" STREQUAL "")
        set(WPE_COG_PLATFORMS "drm,headless,wayland,x11")
    elseif ("${WPE_COG_PLATFORMS}" STREQUAL "none")
        set(WPE_COG_PLATFORMS "")
    endif ()

    # Conditionally add 'GTK4'.
    find_package(GTK 4.0.0)
    if (GTK_FOUND)
        set(WPE_COG_PLATFORMS "${WPE_COG_PLATFORMS},gtk4")
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
        set(WPE_COG_TAG "edafaa079f1c5ebe201bac33ddf3c476479c3f1f")
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

    if (ENABLE_SANITIZERS)
        set(COG_MESON_SANITIZE_OPTION "${ENABLE_SANITIZERS}")
    else ()
        set(COG_MESON_SANITIZE_OPTION "none")
    endif ()

    ExternalProject_Add(cog
        GIT_REPOSITORY "${WPE_COG_REPO}"
        GIT_TAG "${WPE_COG_TAG}"
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/Tools/wpe/cog"
        BUILD_IN_SOURCE FALSE
        CONFIGURE_COMMAND
            ${CMAKE_SOURCE_DIR}/Tools/wpe/meson-wrapper
            ${MESON_RECONFIGURE}
            <BINARY_DIR> <SOURCE_DIR>
            --buildtype ${COG_MESON_BUILDTYPE}
            --pkg-config-path ${WPE_COG_PKG_CONFIG_PATH}
            -Dwpe_api=${WPE_API_VERSION}
            -Dplatforms=${WPE_COG_PLATFORMS}
            -Db_sanitize=${COG_MESON_SANITIZE_OPTION}
            -Dlibportal=auto
        BUILD_COMMAND
            meson compile -C <BINARY_DIR>
        INSTALL_COMMAND "")
    ExternalProject_Add_StepDependencies(cog build WebKit)
    ExternalProject_Add_StepDependencies(cog configure
        "${CMAKE_BINARY_DIR}/cmakeconfig.h"
        "${WPE_PKGCONFIG_FILE}"
    )
endif ()
