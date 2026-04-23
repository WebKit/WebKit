# Enable ccache by default, if installed. To disable it you can:
# if using script build-webkit: pass --no-use-ccache
# if using cmake: set environment variable WK_USE_CCACHE=NO
if (NOT "$ENV{WK_USE_CCACHE}" STREQUAL "NO")
    find_program(CCACHE_FOUND ccache)
    if (CCACHE_FOUND)
        if (NOT DEFINED ENV{CCACHE_SLOPPINESS})
            if (APPLE)
                # Apple SDK headers change mtime/ctime across Xcode updates without content
                # changes; include file_mtime/ctime sloppiness avoids whole-cache invalidation.
                set(ENV{CCACHE_SLOPPINESS} "pch_defines,time_macros,include_file_mtime,include_file_ctime")
            else ()
                set(ENV{CCACHE_SLOPPINESS} time_macros)
            endif ()
        endif ()
        # Check if CXX_COMPILER is already a ccache symlink (some distros/brew setups do this).
        execute_process(COMMAND readlink -f ${CMAKE_CXX_COMPILER} RESULT_VARIABLE READLINK_RETCODE OUTPUT_VARIABLE REAL_CXX_PATH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        execute_process(COMMAND which ${CCACHE_FOUND} RESULT_VARIABLE WHICH_RETCODE OUTPUT_VARIABLE REAL_CCACHE_PATH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if (${WHICH_RETCODE} EQUAL 0 AND ${READLINK_RETCODE} EQUAL 0 AND "${REAL_CXX_PATH}" STREQUAL "${REAL_CCACHE_PATH}")
            message(STATUS "Enabling ccache: Compiler path already pointing to ccache. Not setting ccache prefix.")
        else ()
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
        # sccache does not understand PCH compilation steps — it caches the
        # binary .pch/.gch output and later feeds it back as source, causing
        # "not valid UTF-8" errors.
        set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
    endif ()
endif ()
