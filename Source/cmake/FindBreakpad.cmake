pkg_check_modules(PC_BREAKPAD breakpad-client)

find_path(BREAKPAD_INCLUDE_DIRS
    NAMES client/linux/handler/exception_handler.h
    HINTS ${PC_BREAKPAD_INCLUDEDIR}
    HINTS ${PC_BREAKPAD_INCLUDE_DIRS}
)

find_library(BREAKPAD_LIBRARY
    NAMES breakpad_client
    HINTS ${PC_BREAKPAD_LIBRARY_DIRS}
          ${PC_BREAKPAD_LIBDIR}
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Breakpad REQUIRED_VARS BREAKPAD_INCLUDE_DIRS BREAKPAD_LIBRARY)

if (BREAKPAD_LIBRARY AND NOT TARGET Breakpad::Breakpad)
    add_library(Breakpad::Breakpad UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Breakpad::Breakpad PROPERTIES
        IMPORTED_LOCATION "${BREAKPAD_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${PC_BREAKPAD_CFLAGS_OTHER}"
        INTERFACE_INCLUDE_DIRECTORIES "${BREAKPAD_INCLUDE_DIRS}"
    )
endif ()

mark_as_advanced(BREAKPAD_INCLUDE_DIRS BREAKPAD_LIBRARY)
