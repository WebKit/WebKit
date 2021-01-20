if (ENABLE_BUBBLEWRAP_SANDBOX)
    find_program(BWRAP_EXECUTABLE bwrap)
    if (NOT BWRAP_EXECUTABLE)
        message(FATAL_ERROR "bwrap executable is needed for ENABLE_BUBBLEWRAP_SANDBOX")
    endif ()

    find_package(Libseccomp)
    if (NOT Libseccomp_FOUND)
        message(FATAL_ERROR "libseccomp is needed for ENABLE_BUBBLEWRAP_SANDBOX")
    endif ()

    find_program(DBUS_PROXY_EXECUTABLE xdg-dbus-proxy)
    if (NOT DBUS_PROXY_EXECUTABLE)
        message(FATAL_ERROR "xdg-dbus-proxy not found and is needed for ENABLE_BUBBLEWRAP_SANDBOX")
    endif ()

    if (NOT CMAKE_CROSSCOMPILING)
        execute_process(
            COMMAND "${BWRAP_EXECUTABLE}" --version
            RESULT_VARIABLE BWRAP_RET
            OUTPUT_VARIABLE BWRAP_OUTPUT
        )
        if (BWRAP_RET)
            message(FATAL_ERROR "Failed to run ${BWRAP_EXECUTABLE}")
        endif ()
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" BWRAP_VERSION "${BWRAP_OUTPUT}")
        if (NOT "${BWRAP_VERSION}" VERSION_GREATER_EQUAL "0.3.1")
            message(FATAL_ERROR "bwrap must be >= 0.3.1 but ${BWRAP_VERSION} found")
        endif ()
    elseif (NOT SILENCE_CROSS_COMPILATION_NOTICES)
        message(NOTICE
            "***--------------------------------------------------------***\n"
            "***  Cannot check Bubblewrap version when cross-compiling. ***\n"
            "***  The target system MUST have version 0.3.1 or newer.   ***\n"
            "***  Use the BWRAP_EXECUTABLE and DBUS_PROXY_EXECUTABLE    ***\n"
            "***  variables to set the run-time paths for the 'bwrap'   ***\n"
            "***  and 'xdg-dbus-proxy' programs.                        ***\n"
            "***--------------------------------------------------------***"
        )
    endif ()

    add_definitions(-DBWRAP_EXECUTABLE="${BWRAP_EXECUTABLE}")
    add_definitions(-DDBUS_PROXY_EXECUTABLE="${DBUS_PROXY_EXECUTABLE}")
endif ()
