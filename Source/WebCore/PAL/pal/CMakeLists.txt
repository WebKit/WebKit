set(PAL_PUBLIC_HEADERS
    ExportMacros.h
    FileSizeFormatter.h
    HysteresisActivity.h
    Logging.h
    SessionID.h
    ThreadGlobalData.h

    crypto/CryptoDigest.h
    crypto/CryptoTypes.h

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

list(APPEND PAL_UNIFIED_SOURCE_LIST_FILES
    "Sources.txt"
)

set(PAL_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${PAL_DERIVED_SOURCES_DIR}"
    # Exclude PAL source dirs from Swift -- the source tree contains module.modulemap
    # which conflicts with the installed copy. Use generator expressions to include
    # them only for C/C++/ObjC.
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:${PAL_DIR}>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:${PAL_DIR}/pal>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:${PAL_DIR}/pal/crypto>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:${PAL_DIR}/pal/system>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:${PAL_DIR}/pal/text>"
)

set(PAL_FRAMEWORKS
    WTF
    bmalloc
)
if (USE_APPLE_INTERNAL_SDK)
    list(APPEND PAL_FRAMEWORKS WebKitAdditions)
endif ()

set(PAL_INTERFACE_LIBRARIES PAL)
set(PAL_INTERFACE_INCLUDE_DIRECTORIES ${PAL_FRAMEWORK_HEADERS_DIR})
set(PAL_INTERFACE_DEPENDENCIES PAL_CopyHeaders)

WEBKIT_FRAMEWORK_DECLARE(PAL)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
WEBKIT_ADD_TARGET_UNSAFE_BUFFER_WARNINGS(PAL)

if (SWIFT_REQUIRED AND APPLE)
    set(PAL_SWIFT_INTEROP_SOURCES
        crypto/CryptoAlgorithmAESGCMCocoa.cpp
        crypto/CryptoAlgorithmAESKWCocoaBridging.cpp
        crypto/CryptoAlgorithmEd25519CocoaBridging.cpp
        crypto/CryptoAlgorithmHKDFCocoaBridging.cpp
        crypto/CryptoAlgorithmHMACCocoaBridging.cpp
        crypto/CryptoAlgorithmX25519CocoaBridging.cpp
        crypto/CryptoEDKeyBridging.cpp
        crypto/PlatformECKey.cpp

        crypto/commoncrypto/CryptoDigestCommonCrypto.cpp
    )
    foreach (_f IN LISTS PAL_SWIFT_INTEROP_SOURCES)
        cmake_path(GET _f FILENAME _fn)
        list(APPEND PAL_UNIFIED_SOURCE_EXCLUDES "(^|/)${_fn}( |$)")
    endforeach ()
endif ()

WEBKIT_COMPUTE_SOURCES(PAL)

if (DEFINED PAL_SWIFT_INTEROP_SOURCES)
    list(REMOVE_ITEM PAL_SOURCES ${PAL_SWIFT_INTEROP_SOURCES})
    list(APPEND PAL_INTERFACE_LIBRARIES "$<TARGET_OBJECTS:PAL_SwiftInterop>")
endif ()

WEBKIT_COPY_FILES(PAL_CopyHeaders
    DESTINATION ${PAL_HEADERS_DIR}
    FILES ${PAL_PUBLIC_HEADERS}
)

if (SWIFT_REQUIRED AND APPLE)
    target_sources(PAL PRIVATE
        ${PAL_DIR}/pal/pal.swift
        ${PAL_DIR}/pal/system/cocoa/RegexHelper.swift
        ${PAL_DIR}/pal/crypto/CryptoKitShim.swift
        ${PAL_DIR}/pal/crypto/WTFExtras.swift
    )

    # FIXME(rdar://177840132): swiftc -emit-dependencies records imported
    # clang modulemaps, but sometimes skips individual C++ headers.
    set(PAL_SWIFT_INTEROP_HEADERS ${PAL_PUBLIC_HEADERS})

    WEBKIT_SETUP_SWIFT_AND_GENERATE_SWIFT_CPP_INTEROP_HEADER(PAL pal
        "${PAL_HEADERS_DIR}"
        "PALSwift-Generated.h")

    WEBKIT_TARGET_ADD_SWIFT_OPTIONS(PAL
        -library-level spi
        # SafeInteropWrappers imports a lifetimebound std::span return as a Swift Span,
        # which is what lets the byte views hand Swift a lifetime-dependent view rather
        # than a raw pointer.
        -enable-experimental-feature SafeInteropWrappers
        -Xfrontend -clang-header-expose-decls=has-expose-attr-or-stdlib
        -Xcc -I${PAL_FRAMEWORK_HEADERS_DIR}
        -Xcc -I${CMAKE_BINARY_DIR}/WTF/Headers
        -Xcc -I${CMAKE_BINARY_DIR}/ICU/Headers
        -Xcc -I${CMAKE_BINARY_DIR}/bmalloc/Headers
        -Xcc -I${CMAKE_BINARY_DIR}
        -Xcc -I${PAL_DERIVED_SOURCES_DIR}
    )
    if (NOT APPLE)
        # On Apple platforms the interop macro already adds this; elsewhere PAL's
        # Swift still extends its underlying module and needs it explicitly.
        WEBKIT_TARGET_ADD_SWIFT_OPTIONS(PAL -import-underlying-module)
    endif ()
endif ()

WEBKIT_FRAMEWORK(PAL)
WEBKIT_ADD_PREFIX_HEADER(PAL PALPrefix.h PREFIX_LANGUAGES CXX OBJCXX)

if (DEFINED PAL_SWIFT_INTEROP_SOURCES)
    WEBKIT_DEFINE_SUBTARGET(PAL_SwiftInterop PAL ${PAL_SWIFT_INTEROP_SOURCES})
    target_precompile_headers(PAL_SwiftInterop REUSE_FROM PAL)
    add_dependencies(PAL_SwiftInterop PAL_CopyHeaders)
endif ()

