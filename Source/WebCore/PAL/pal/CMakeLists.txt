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

set(PAL_INTERFACE_LIBRARIES PAL)
set(PAL_INTERFACE_INCLUDE_DIRECTORIES ${PAL_FRAMEWORK_HEADERS_DIR})
set(PAL_INTERFACE_DEPENDENCIES PAL_CopyHeaders)

WEBKIT_FRAMEWORK_DECLARE(PAL)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

if (SWIFT_REQUIRED AND NOT (PORT STREQUAL GTK OR PORT STREQUAL WPE))
    target_sources(PAL PRIVATE
        ${PAL_DIR}/pal/pal.swift
        ${PAL_DIR}/pal/crypto/CryptoKit+UnsafeOverlays.swift
        ${PAL_DIR}/pal/crypto/CryptoTypes.swift
        ${PAL_DIR}/pal/system/cocoa/RegexHelper.swift
        ${PAL_DIR}/pal/crypto/CryptoKitShim.swift
    )

    # Use -Xcc -I for ALL include paths (like Xcode does). Plain -I triggers
    # Swift's own module discovery which causes C++ stdlib module conflicts.
    set(PAL_SWIFT_INCLUDE_DIRECTORIES "")

    set(PAL_SWIFT_EXTRA_OPTIONS
        "-import-underlying-module"
        "-strict-memory-safety"
        "-swift-version" "6"
        "-Xcc" "-I${PAL_FRAMEWORK_HEADERS_DIR}"
        "-Xcc" "-I${CMAKE_BINARY_DIR}/WTF/Headers"
        "-Xcc" "-I${CMAKE_BINARY_DIR}/ICU/Headers"
        "-Xcc" "-I${CMAKE_BINARY_DIR}/bmalloc/Headers"
        "-Xcc" "-I${CMAKE_BINARY_DIR}"
        "-Xcc" "-I${PAL_DERIVED_SOURCES_DIR}"
    )

    WEBKIT_SETUP_SWIFT_AND_GENERATE_SWIFT_CPP_INTEROP_HEADER(PAL pal
        "${PAL_FRAMEWORK_HEADERS_DIR}/pal"
        "PALSwift-Generated.h")

    # Also pass these options to CMake's native Swift compilation (the macro
    # only adds them to the custom typecheck command). SHELL: keeps multi-token
    # groups (-swift-version 6, -Xcc -I<path>) together; without it CMake
    # deduplicates repeated -Xcc tokens, leaving -I paths unguarded.
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

if (PORT STREQUAL GTK OR PORT STREQUAL WPE)
    WEBKIT_ADD_TARGET_CXX_FLAGS(PAL
        -Wunsafe-buffer-usage
        -Wunsafe-buffer-usage-in-libc-call
        -fsafe-buffer-usage-suggestions
    )
endif ()

WEBKIT_COPY_FILES(PAL_CopyHeaders
    DESTINATION ${PAL_FRAMEWORK_HEADERS_DIR}/pal
    FILES ${PAL_PUBLIC_HEADERS}
)

WEBKIT_FRAMEWORK(PAL)

# PAL's Swift sources need the module.modulemap and headers to be installed
# before compilation (they import the underlying pal module). The CopyHeaders
# target is an interface dependency for downstream targets, but PAL itself
# also needs it.
if (SWIFT_REQUIRED AND NOT (PORT STREQUAL GTK OR PORT STREQUAL WPE))
    add_dependencies(PAL PAL_CopyHeaders WTF_CopyHeaders bmalloc_CopyHeaders)
endif ()
