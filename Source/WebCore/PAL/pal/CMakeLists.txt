set(PAL_PUBLIC_HEADERS
    ExportMacros.h
    FileSizeFormatter.h
    Gunzip.h
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

# PAL only uses WebKit::WTF but we need to determine where that is
# If WTF is statically linked into a shared JavaScriptCore then that should be
# linked otherwise it can be linked directly
if (NOT ${WTF_LIBRARY_TYPE} STREQUAL "SHARED" AND ${JavaScriptCore_LIBRARY_TYPE} STREQUAL "SHARED")
    set(PAL_LIBRARIES WebKit::JavaScriptCore)
else ()
    set(PAL_LIBRARIES WebKit::WTF)
endif ()

set(PAL_INTERFACE_LIBRARIES PAL)
set(PAL_INTERFACE_INCLUDE_DIRECTORIES ${PAL_FRAMEWORK_HEADERS_DIR})
set(PAL_INTERFACE_DEPENDENCIES PAL_CopyHeaders)

WEBKIT_FRAMEWORK_DECLARE(PAL)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_COPY_FILES(PAL_CopyHeaders
    DESTINATION ${PAL_FRAMEWORK_HEADERS_DIR}/pal
    FILES ${PAL_PUBLIC_HEADERS}
)

WEBKIT_WRAP_SOURCELIST(${PAL_SOURCES})
WEBKIT_FRAMEWORK(PAL)
WEBKIT_FRAMEWORK_TARGET(PAL)
