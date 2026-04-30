# Enable ccache by default, if installed. To disable it you can:
# if using script build-webkit: pass --no-use-ccache
# if using cmake: set environment variable WK_USE_CCACHE=NO
if (NOT "$ENV{WK_USE_CCACHE}" STREQUAL "NO" AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(CCACHE_FOUND ccache)
    if (CCACHE_FOUND)
        # Check if CXX_COMPILER is already a ccache symlink (some distros/brew setups do this).
        execute_process(COMMAND readlink -f ${CMAKE_CXX_COMPILER} RESULT_VARIABLE READLINK_RETCODE OUTPUT_VARIABLE REAL_CXX_PATH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        execute_process(COMMAND which ${CCACHE_FOUND} RESULT_VARIABLE WHICH_RETCODE OUTPUT_VARIABLE REAL_CCACHE_PATH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if (${WHICH_RETCODE} EQUAL 0 AND ${READLINK_RETCODE} EQUAL 0 AND "${REAL_CXX_PATH}" STREQUAL "${REAL_CCACHE_PATH}")
            message(STATUS "Enabling ccache: Compiler path already pointing to ccache. Not setting ccache prefix.")
        elseif (APPLE)
            # CCACHE_SLOPPINESS / CCACHE_BASEDIR set via set(ENV{...}) or the
            # preset's environment block only apply to the cmake process, not to
            # the ninja that later invokes ccache. Generate a launcher that
            # exports them at compile time so cache hashes are portable across
            # worktrees of the same checkout (-fdebug-prefix-map handles the
            # debug-info side; CCACHE_BASEDIR handles the command-line side).
            set(_ccache_launcher "${CMAKE_BINARY_DIR}/ccache-launcher")
            file(WRITE "${_ccache_launcher}"
                "#!/bin/sh\n"
                "export CCACHE_BASEDIR='${CMAKE_SOURCE_DIR}'\n"
                "export CCACHE_NOHASHDIR=true\n"
                "export CCACHE_SLOPPINESS='pch_defines,time_macros,include_file_mtime,include_file_ctime'\n"
                "exec '${CCACHE_FOUND}' \"$@\"\n")
            file(CHMOD "${_ccache_launcher}" PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
            message(STATUS "Enabling ccache: Setting ccache prefix via ${_ccache_launcher} (CCACHE_BASEDIR=${CMAKE_SOURCE_DIR}).")
            set(CMAKE_C_COMPILER_LAUNCHER "${_ccache_launcher}")
            set(CMAKE_CXX_COMPILER_LAUNCHER "${_ccache_launcher}")
            set(CMAKE_ASM_COMPILER_LAUNCHER "${_ccache_launcher}")
            set(CMAKE_OBJC_COMPILER_LAUNCHER "${_ccache_launcher}")
            set(CMAKE_OBJCXX_COMPILER_LAUNCHER "${_ccache_launcher}")
        else ()
            if (NOT DEFINED ENV{CCACHE_SLOPPINESS})
                set(ENV{CCACHE_SLOPPINESS} time_macros)
            endif ()
            message(STATUS "Enabling ccache: Setting ccache prefix for compiler.")
            set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_FOUND})
            set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_FOUND})
        endif ()
    else ()
        message(STATUS "Enabling ccache: Couldn't find ccache program. Not enabling it.")
    endif ()
endif ()

if (("$ENV{WEBKIT_USE_SCCACHE}" STREQUAL "1") OR DEFINED ENV{SCCACHE_REDIS} OR DEFINED ENV{SCCACHE_BUCKET}
    OR DEFINED ENV{SCCACHE_MEMCACHED} OR DEFINED ENV{SCCACHE_GCS_BUCKET} OR DEFINED ENV{SCCACHE_AZURE_CONNECTION_STRING})
    find_program(SCCACHE_FOUND sccache)
    if (SCCACHE_FOUND)
        message(STATUS "Enabling sccache as prefix for compiler.")
        set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_FOUND})
        set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_FOUND})
    endif ()
endif ()
