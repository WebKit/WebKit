set(PAL_PUBLIC_HEADERS
    ExportMacros.h
    FileSizeFormatter.h
    Gunzip.h
    HysteresisActivity.h
    Logging.h
    SessionID.h
    ThreadGlobalData.h

    crypto/CryptoDigest.h

    system/Clock.h
    system/ClockGeneric.h
    system/SleepDisabler.h
    system/Sound.h
    system/SystemSleepListener.h

    text/DecodeEscapeSequences.h
    text/EncodingTables.h
    text/KillRing.h
    text/TextCodec.h
    text/TextCodecASCIIFastPath.h
    text/TextCodecCJK.h
    text/TextCodecICU.h
    text/TextCodecLatin1.h
    text/TextCodecReplacement.h
    text/TextCodecSingleByte.h
    text/TextCodecUTF16.h
    text/TextCodecUTF8.h
    text/TextCodecUserDefined.h
    text/TextEncoding.h
    text/TextEncodingDetector.h
    text/TextEncodingRegistry.h
    text/UnencodableHandling.h
)

set(PAL_SOURCES
    FileSizeFormatter.cpp
    Logging.cpp
    PALTZoneImpls.cpp
    SessionID.cpp
    ThreadGlobalData.cpp

    system/SleepDisabler.cpp
    system/SystemSleepListener.cpp

    text/EncodingTables.cpp
    text/TextCodec.cpp
    text/TextCodecCJK.cpp
    text/TextCodecICU.cpp
    text/TextCodecLatin1.cpp
    text/TextCodecReplacement.cpp
    text/TextCodecSingleByte.cpp
    text/TextCodecUTF16.cpp
    text/TextCodecUTF8.cpp
    text/TextCodecUserDefined.cpp
    text/TextEncoding.cpp
    text/TextEncodingDetectorICU.cpp
    text/TextEncodingRegistry.cpp
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

set(PAL_FRAMEWORKS WTF)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND PAL_FRAMEWORKS bmalloc)
endif ()

set(PAL_INTERFACE_LIBRARIES PAL)
set(PAL_INTERFACE_INCLUDE_DIRECTORIES ${PAL_FRAMEWORK_HEADERS_DIR})
set(PAL_INTERFACE_DEPENDENCIES PAL_CopyHeaders)

WEBKIT_FRAMEWORK_DECLARE(PAL)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

if (PORT STREQUAL GTK OR PORT STREQUAL WPE)
    WEBKIT_ADD_TARGET_CXX_FLAGS(PAL
        -Wunsafe-buffer-usage
        -fsafe-buffer-usage-suggestions
    )
endif ()

WEBKIT_COPY_FILES(PAL_CopyHeaders
    DESTINATION ${PAL_FRAMEWORK_HEADERS_DIR}/pal
    FILES ${PAL_PUBLIC_HEADERS}
)

WEBKIT_FRAMEWORK(PAL)
