list(APPEND PAL_SOURCES
    crypto/win/CryptoDigestWin.cpp

    system/ClockGeneric.cpp

    system/win/SoundWin.cpp

    text/KillRing.cpp

    win/LoggingWin.cpp
)

list(APPEND PAL_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}/../include/private"
    "${DERIVED_SOURCES_PAL_DIR}"
)

if (${WTF_PLATFORM_WIN_CAIRO})
    include(PlatformWinCairo.cmake)
else ()
    include(PlatformAppleWin.cmake)
endif ()

set(PAL_OUTPUT_NAME PAL${DEBUG_SUFFIX})

# Generate PALHeaderDetection.h by PAL_PreBuild
add_custom_target(PAL_PreBuild SOURCES "${DERIVED_SOURCES_PAL_DIR}/PALHeaderDetection.h")
add_custom_command(
    OUTPUT "${DERIVED_SOURCES_PAL_DIR}/PALHeaderDetection.h"
    WORKING_DIRECTORY "${DERIVED_SOURCES_PAL_DIR}"
    COMMAND ${PYTHON_EXECUTABLE} ${PAL_DIR}/AVFoundationSupport.py ${WEBKIT_LIBRARIES_DIR} > PALHeaderDetection.h
    VERBATIM)
add_dependencies(PAL PAL_PreBuild)
