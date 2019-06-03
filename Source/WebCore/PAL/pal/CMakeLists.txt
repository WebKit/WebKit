set(PAL_PUBLIC_HEADERS
    ExportMacros.h
    FileSizeFormatter.h
    HysteresisActivity.h
    LogMacros.h
    Logging.h
    SessionID.h

    crypto/CryptoDigest.h

    system/Clock.h
    system/ClockGeneric.h
    system/SleepDisabler.h
    system/Sound.h
    system/SystemSleepListener.h

    text/KillRing.h
    text/UnencodableHandling.h
)

set(PAL_SOURCES
    FileSizeFormatter.cpp
    Logging.cpp
    SessionID.cpp

    system/SleepDisabler.cpp
    system/SystemSleepListener.cpp
)

set(PAL_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${PAL_DERIVED_SOURCES_DIR}"
    "${PAL_DIR}"
    "${PAL_DIR}/pal"
    "${PAL_DIR}/pal/crypto"
    "${PAL_DIR}/pal/system"
    "${PAL_DIR}/pal/text"
)

set(PAL_LIBRARIES
    WebKit::JavaScriptCore
)

WEBKIT_FRAMEWORK_DECLARE(PAL)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_WRAP_SOURCELIST(${PAL_SOURCES})
WEBKIT_FRAMEWORK(PAL)

WEBKIT_MAKE_FORWARDING_HEADERS(PAL
    TARGET_NAME PALFrameworkHeaders
    DESTINATION ${PAL_FRAMEWORK_HEADERS_DIR}/pal
    FILES ${PAL_PUBLIC_HEADERS}
)
