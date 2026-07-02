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

if (SWIFT_REQUIRED AND NOT (PORT STREQUAL GTK OR PORT STREQUAL WPE))
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
    PRUNE_STALE
    DESTINATION ${PAL_FRAMEWORK_HEADERS_DIR}/pal
    FILES ${PAL_PUBLIC_HEADERS}
)

if (SWIFT_REQUIRED AND NOT (PORT STREQUAL GTK OR PORT STREQUAL WPE))
    target_sources(PAL PRIVATE
        ${PAL_DIR}/pal/pal.swift
        ${PAL_DIR}/pal/crypto/CryptoKit+UnsafeOverlays.swift
        ${PAL_DIR}/pal/crypto/CryptoTypes.swift
        ${PAL_DIR}/pal/system/cocoa/RegexHelper.swift
        ${PAL_DIR}/pal/crypto/CryptoKitShim.swift
    )

    if (APPLE)
        # Swift compiles transitively load system frameworks whose PCM builds
        # need our project -Xcc flags. Explicit modules let libSwiftScan
        # pre-build PCMs with the right context.
        set(PAL_SWIFT_EXPLICIT_MODULE_BUILD TRUE)
    endif ()

    # FIXME(rdar://177840132): swiftc -emit-dependencies records imported
    # clang modulemaps, but sometimes skips individual C++ headers.
    set(PAL_SWIFT_INTEROP_HEADERS ${PAL_PUBLIC_HEADERS})

    WEBKIT_SETUP_SWIFT_AND_GENERATE_SWIFT_CPP_INTEROP_HEADER(PAL pal
        "${PAL_FRAMEWORK_HEADERS_DIR}/pal"
        "PALSwift-Generated.h")

    # SHELL: keeps multi-token groups (-swift-version 6, -Xcc -I<path>) together;
    # without it CMake deduplicates repeated -Xcc tokens, leaving -I paths unguarded.
    # https://bugs.webkit.org/show_bug.cgi?id=312105
    target_compile_options(PAL PRIVATE
        "$<$<COMPILE_LANGUAGE:Swift>:-import-underlying-module>"
        "$<$<COMPILE_LANGUAGE:Swift>:-strict-memory-safety>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-swift-version 6>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${PAL_FRAMEWORK_HEADERS_DIR}>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${CMAKE_BINARY_DIR}/WTF/Headers>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${CMAKE_BINARY_DIR}/ICU/Headers>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${CMAKE_BINARY_DIR}/bmalloc/Headers>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${CMAKE_BINARY_DIR}>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${PAL_DERIVED_SOURCES_DIR}>"
    )
endif ()

WEBKIT_FRAMEWORK(PAL)
WEBKIT_ADD_PREFIX_HEADER(PAL PALPrefix.h PREFIX_LANGUAGES CXX OBJCXX)

if (DEFINED PAL_SWIFT_INTEROP_SOURCES)
    WEBKIT_DEFINE_SUBTARGET(PAL_SwiftInterop PAL ${PAL_SWIFT_INTEROP_SOURCES})
    target_precompile_headers(PAL_SwiftInterop REUSE_FROM PAL)
    add_dependencies(PAL_SwiftInterop PAL_CopyHeaders)
endif ()

# Must come after WEBKIT_FRAMEWORK() which adds -warnings-as-errors.
target_compile_options(PAL PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -no-warnings-as-errors>")
