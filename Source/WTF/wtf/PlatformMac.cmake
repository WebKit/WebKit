find_library(COCOA_LIBRARY Cocoa)
find_library(COREFOUNDATION_LIBRARY CoreFoundation)
find_library(READLINE_LIBRARY Readline)
list(APPEND WTF_LIBRARIES
    ${COREFOUNDATION_LIBRARY}
    ${COCOA_LIBRARY}
    ${READLINE_LIBRARY}
)

list(APPEND WTF_PUBLIC_HEADERS
    WeakObjCPtr.h

    cf/CFURLExtras.h
    cf/TypeCastsCF.h

    cocoa/Entitlements.h
    cocoa/NSURLExtras.h
    cocoa/SoftLinking.h

    darwin/WeakLinking.h

    mac/AppKitCompatibilityDeclarations.h

    spi/cf/CFBundleSPI.h
    spi/cf/CFStringSPI.h

    spi/cocoa/CFXPCBridgeSPI.h
    spi/cocoa/SecuritySPI.h
    spi/cocoa/objcSPI.h

    spi/darwin/SandboxSPI.h
    spi/darwin/XPCSPI.h
    spi/darwin/dyldSPI.h

    spi/mac/MetadataSPI.h

    text/cf/TextBreakIteratorCF.h
)

list(APPEND WTF_SOURCES
    BlockObjCExceptions.mm

    cf/CFURLExtras.cpp
    cf/FileSystemCF.cpp
    cf/LanguageCF.cpp
    cf/RunLoopCF.cpp
    cf/RunLoopTimerCF.cpp
    cf/SchedulePairCF.cpp
    cf/URLCF.cpp

    cocoa/AutodrainedPool.cpp
    cocoa/CPUTimeCocoa.cpp
    cocoa/Entitlements.mm
    cocoa/FileSystemCocoa.mm
    cocoa/MachSendRight.cpp
    cocoa/MainThreadCocoa.mm
    cocoa/MemoryFootprintCocoa.cpp
    cocoa/MemoryPressureHandlerCocoa.mm
    cocoa/NSURLExtras.mm
    cocoa/URLCocoa.mm
    cocoa/WorkQueueCocoa.cpp

    mac/DeprecatedSymbolsUsedBySafari.mm
    mac/FileSystemMac.mm
    mac/SchedulePairMac.mm

    posix/EnvironmentPOSIX.cpp
    posix/FileSystemPOSIX.cpp
    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    text/cf/AtomicStringImplCF.cpp
    text/cf/StringCF.cpp
    text/cf/StringImplCF.cpp
    text/cf/StringViewCF.cpp

    text/cocoa/StringCocoa.mm
    text/cocoa/StringImplCocoa.mm
    text/cocoa/StringViewCocoa.mm
    text/cocoa/TextBreakIteratorInternalICUCocoa.cpp
)

list(APPEND WTF_PRIVATE_INCLUDE_DIRECTORIES
    ${DERIVED_SOURCES_WTF_DIR}
)

file(COPY mac/MachExceptions.defs DESTINATION ${DERIVED_SOURCES_WTF_DIR})
file(COPY "${WTF_DIR}/icu/unicode" DESTINATION ${DERIVED_SOURCES_WTF_DIR})

add_custom_command(
    OUTPUT
        ${DERIVED_SOURCES_WTF_DIR}/MachExceptionsServer.h
        ${DERIVED_SOURCES_WTF_DIR}/mach_exc.h
        ${DERIVED_SOURCES_WTF_DIR}/mach_excServer.c
        ${DERIVED_SOURCES_WTF_DIR}/mach_excUser.c
    MAIN_DEPENDENCY mac/MachExceptions.defs
    WORKING_DIRECTORY ${DERIVED_SOURCES_WTF_DIR}
    COMMAND mig -sheader MachExceptionsServer.h MachExceptions.defs
    VERBATIM)
list(APPEND WTF_SOURCES
    ${DERIVED_SOURCES_WTF_DIR}/mach_excServer.c
    ${DERIVED_SOURCES_WTF_DIR}/mach_excUser.c
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebKitLegacy DIRECTORIES ${WebKitLegacy_FORWARDING_HEADERS_DIRECTORIES} FILES ${WebKitLegacy_FORWARDING_HEADERS_FILES})
WEBKIT_CREATE_FORWARDING_HEADERS(WebKit DIRECTORIES ${FORWARDING_HEADERS_DIR}/WebKitLegacy)
