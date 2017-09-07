find_library(COCOA_LIBRARY Cocoa)
find_library(COREFOUNDATION_LIBRARY CoreFoundation)
find_library(READLINE_LIBRARY Readline)
list(APPEND WTF_LIBRARIES
    ${COREFOUNDATION_LIBRARY}
    ${COCOA_LIBRARY}
    ${READLINE_LIBRARY}
)

list(APPEND WTF_SOURCES
    AutodrainedPoolMac.mm
    BlockObjCExceptions.mm
    RunLoopTimerCF.cpp
    SchedulePairCF.cpp
    SchedulePairMac.mm

    cf/LanguageCF.cpp
    cf/RunLoopCF.cpp

    text/mac/TextBreakIteratorInternalICUMac.mm

    cocoa/CPUTimeCocoa.mm
    cocoa/MemoryFootprintCocoa.cpp
    cocoa/MemoryPressureHandlerCocoa.mm
    cocoa/WorkQueueCocoa.cpp

    mac/DeprecatedSymbolsUsedBySafari.mm
    mac/MainThreadMac.mm

    text/cf/AtomicStringImplCF.cpp
    text/cf/StringCF.cpp
    text/cf/StringImplCF.cpp
    text/cf/StringViewCF.cpp

    text/mac/StringImplMac.mm
    text/mac/StringMac.mm
    text/mac/StringViewObjC.mm
)

list(APPEND WTF_INCLUDE_DIRECTORIES
    "${WTF_DIR}/icu"
    "${WTF_DIR}/wtf/spi/darwin"
    ${DERIVED_SOURCES_WTF_DIR}
)

file(COPY mac/MachExceptions.defs DESTINATION ${DERIVED_SOURCES_WTF_DIR})

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
