if (ENABLE_BUBBLEWRAP_SANDBOX)
    find_package(Libseccomp)
    if (NOT Libseccomp_FOUND)
        message(FATAL_ERROR "libseccomp is needed for ENABLE_BUBBLEWRAP_SANDBOX")
    endif ()

    if (NOT DEFINED BWRAP_EXECUTABLE)
        if (CMAKE_CROSSCOMPILING)
            message(FATAL_ERROR "bwrap executable version 0.3.1 or newer is needed for ENABLE_BUBBLEWRAP_SANDBOX. Unable to autodetect the path when cross-compiling. "
                                "Please define define the CMake variable BWRAP_EXECUTABLE with the run-time full-path to the 'bwrap' program.")
        else ()
            find_program(BWRAP_EXECUTABLE bwrap)
            if (NOT BWRAP_EXECUTABLE)
                message(FATAL_ERROR "bwrap executable is needed for ENABLE_BUBBLEWRAP_SANDBOX. "
                       "Either install it or use the CMake variable BWRAP_EXECUTABLE to define the runtime path.")
            endif ()
        endif ()
    endif ()

    if (NOT DEFINED DBUS_PROXY_EXECUTABLE)
        if (CMAKE_CROSSCOMPILING)
            message(FATAL_ERROR "xdg-dbus-proxy executable is needed for ENABLE_BUBBLEWRAP_SANDBOX.  Unable to autodetect the path when cross-compiling. "
                                "Please define define the CMake variable DBUS_PROXY_EXECUTABLE with the run-time full-path to the 'xdg-dbus-proxy' program.")
        else ()
            find_program(DBUS_PROXY_EXECUTABLE xdg-dbus-proxy)
            if (NOT DBUS_PROXY_EXECUTABLE)
                message(FATAL_ERROR "xdg-dbus-proxy executable not found and is needed for ENABLE_BUBBLEWRAP_SANDBOX. "
                       "Either install it or use the CMake variable DBUS_PROXY_EXECUTABLE to define the runtime path.")
            endif ()
        endif ()
    endif ()

    # Do some extra sanity checks
    if (NOT IS_ABSOLUTE "${BWRAP_EXECUTABLE}")
        message(FATAL_ERROR "The value for BWRAP_EXECUTABLE should be a full path.")
    endif ()
    if (NOT IS_ABSOLUTE "${DBUS_PROXY_EXECUTABLE}")
        message(FATAL_ERROR "The value for DBUS_PROXY_EXECUTABLE should be a full path.")
    endif ()
    # This version check can only be done when not cross-compiling
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
    endif ()

    add_definitions(-DBWRAP_EXECUTABLE="${BWRAP_EXECUTABLE}")
    add_definitions(-DDBUS_PROXY_EXECUTABLE="${DBUS_PROXY_EXECUTABLE}")
endif ()
