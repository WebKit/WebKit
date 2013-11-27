# Nix uses a different way to set/unset default features based on WK bug 110717 (not yet landed).
# So the implementation, i.e. the next two macros, was moved to here to keep a minimal as possible
# interference on other ports until some day when/if bug 110717 get landed.

# Compute and set default values for feature defines
macro(WEBKIT_OPTION_DEFAULTS _port)
    # Use the preprocessor to get the default values for feature defines
    EXEC_PROGRAM("gcc -E -P -dM -DBUILDING_${_port}__ -I ${CMAKE_SOURCE_DIR}/Source/WTF ${CMAKE_SOURCE_DIR}/Source/WTF/wtf/Platform.h | grep '^#define ENABLE_\\w\\+ [01]$' | cut -d' ' -f2-3 | sort" OUTPUT_VARIABLE DEFINITIONS)
    string(REGEX MATCHALL "([^\n]+|[^\n]+$)" DEFINITIONS_LIST ${DEFINITIONS})

    foreach (DEFINITION ${DEFINITIONS_LIST})
        string(REGEX REPLACE "^([^ ]+) ([^ ]+)$" "\\1" DEFINITION_NAME "${DEFINITION}")
        string(REGEX REPLACE "^([^ ]+) ([^ ]+)$" "\\2" DEFINITION_VALUE "${DEFINITION}")
        WEBKIT_OPTION_DEFINE(${DEFINITION_NAME} "Toggle ${DEFINITION_NAME}" ${DEFINITION_VALUE})
        option(${DEFINITION_NAME} "Toggle ${DEFINITION_NAME}" ${DEFINITION_VALUE})
    endforeach ()
endmacro()

# Show WebKit options summary and add -DFOOBAR to each option with a value different from the default value.
macro(PROCESS_WEBKIT_OPTIONS)
    message(STATUS "Enabled features:")

    set(_MAX_FEATURE_LENGTH 0)
    foreach (_name ${_WEBKIT_AVAILABLE_OPTIONS})
        string(LENGTH ${_name} _NAME_LENGTH)
        if (_NAME_LENGTH GREATER _MAX_FEATURE_LENGTH)
            set(_MAX_FEATURE_LENGTH ${_NAME_LENGTH})
        endif ()
    endforeach ()

    set(_SHOULD_PRINT_POINTS OFF)
    foreach (_name ${_WEBKIT_AVAILABLE_OPTIONS})
        string(LENGTH ${_name} _NAME_LENGTH)

        set(_MESSAGE " ${_name} ")

        if (_SHOULD_PRINT_POINTS)
            foreach (IGNORE RANGE ${_NAME_LENGTH} ${_MAX_FEATURE_LENGTH})
                set(_MESSAGE "${_MESSAGE} ")
            endforeach ()
            set(_SHOULD_PRINT_POINTS OFF)
        else ()
            foreach (IGNORE RANGE ${_NAME_LENGTH} ${_MAX_FEATURE_LENGTH})
                set(_MESSAGE "${_MESSAGE}.")
            endforeach ()
            set(_SHOULD_PRINT_POINTS ON)
        endif ()

        if (${_name})
            list(APPEND FEATURE_DEFINES ${_name})
            set(FEATURE_DEFINES_WITH_SPACE_SEPARATOR "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR} ${_name}")
        endif ()

        set(_MESSAGE "${_MESSAGE} ${${_name}}")
        message(STATUS "${_MESSAGE}")
    endforeach ()

    # Add add_definitions to values different from the defaults
    foreach (_name ${_WEBKIT_AVAILABLE_OPTIONS})
        # Convert values from ON/OFF to 1/0 because
        # CMake says "1 EQUAL ON" is false.
        if (${_WEBKIT_AVAILABLE_OPTIONS_INITALVALUE_${_name}})
            set(_a 1)
        else ()
            set(_a 0)
        endif ()
        if (${${_name}})
            set(_b 1)
        else ()
            set(_b 0)
        endif ()
        if (NOT _a EQUAL _b)
            add_definitions("-D${_name}=${_b}")
        endif ()
    endforeach ()
endmacro()

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Use -femit-struct-debug-baseonly to reduce the size of WebCore static library
    set(CMAKE_CXX_FLAGS_DEBUG "-g -femit-struct-debug-baseonly" CACHE STRING "Flags used by the compiler during debug builds." FORCE)
endif ()

set(SHARED_CORE 0)
set(ENABLE_WEBKIT 0)
set(ENABLE_WEBKIT2 1)

set(WebKit2_OUTPUT_NAME WebKitNix)

add_definitions(-DBUILDING_NIX__=1)

find_package(Cairo 1.12.8 REQUIRED)
find_package(Fontconfig 2.8.0 REQUIRED)
find_package(Freetype REQUIRED)
find_package(GLIB 2.36.0 REQUIRED COMPONENTS gio gobject gmodule gthread)
find_package(HarfBuzz)
find_package(ICU REQUIRED)
find_package(JPEG REQUIRED)
find_package(LibXml2 2.6 REQUIRED)
find_package(LibXslt 1.1.7 REQUIRED)
find_package(PNG REQUIRED)
find_package(Sqlite REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

# Variable that must exists on CMake space
# to keep common CMake files working as desired for us
set(WTF_USE_ICU_UNICODE ON)
set(WTF_USE_LEVELDB ON)
set(ENABLE_API_TESTS ON)

WEBKIT_OPTION_DEFAULTS("NIX")
WEBKIT_OPTION_DEFINE(WTF_USE_OPENGL_ES_2 "Use EGL + OpenGLES2" OFF)
WEBKIT_OPTION_DEFINE(WTF_USE_CURL "Use libCurl as network backend" OFF)

if (WTF_USE_CURL)
    find_package(CURL REQUIRED)
    set(REQUIRED_NETWORK libcurl)
else ()
    find_package(LibSoup 2.42.0 REQUIRED)
    set(REQUIRED_NETWORK libsoup-2.4)
endif ()

if (WTF_USE_OPENGL_ES_2)
    find_package(OpenGLES2 REQUIRED)
    find_package(EGL REQUIRED)
    set(WTF_USE_EGL ON)
    add_definitions(-DWTF_USE_OPENGL_ES_2=1)
else ()
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    add_definitions(-DHAVE_GLX=1)
endif ()

if (NOT ENABLE_SVG)
    set(ENABLE_SVG_FONTS OFF)
endif ()

PROCESS_WEBKIT_OPTIONS()



