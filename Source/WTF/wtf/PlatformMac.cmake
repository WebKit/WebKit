find_library(COCOA_LIBRARY Cocoa)
find_library(COREFOUNDATION_LIBRARY CoreFoundation)
find_library(READLINE_LIBRARY Readline)
find_library(SECURITY_LIBRARY Security)
list(APPEND WTF_LIBRARIES
    ${COREFOUNDATION_LIBRARY}
    ${COCOA_LIBRARY}
    ${READLINE_LIBRARY}
    ${SECURITY_LIBRARY}
)

list(APPEND WTF_SOURCES
    BlockObjCExceptions.mm
    ProcessPrivilege.cpp
    TranslatedProcess.cpp

    cf/CFURLExtras.cpp
    cf/FileSystemCF.cpp
    cf/LanguageCF.cpp
    cf/RunLoopCF.cpp
    cf/SchedulePairCF.cpp
    cf/URLCF.cpp

    cocoa/AutodrainedPool.cpp
    cocoa/CrashReporter.cpp
    cocoa/Entitlements.mm
    cocoa/FileSystemCocoa.mm
    cocoa/LanguageCocoa.mm
    cocoa/LoggingCocoa.mm
    cocoa/MachSendRight.cpp
    cocoa/MainThreadCocoa.mm
    cocoa/MemoryFootprintCocoa.cpp
    cocoa/MemoryPressureHandlerCocoa.mm
    cocoa/NSURLExtras.mm
    cocoa/ResourceUsageCocoa.cpp
    cocoa/RuntimeApplicationChecksCocoa.cpp
    cocoa/SystemTracingCocoa.cpp
    cocoa/URLCocoa.mm
    cocoa/WorkQueueCocoa.cpp

    darwin/LibraryPathDiagnostics.mm

    mac/FileSystemMac.mm
    mac/SchedulePairMac.mm

    posix/CPUTimePOSIX.cpp
    posix/FileSystemPOSIX.cpp
    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    text/cf/AtomStringImplCF.cpp
    text/cf/StringCF.cpp
    text/cf/StringImplCF.cpp
    text/cf/StringViewCF.cpp

    text/cocoa/ASCIILiteralCocoa.mm
    text/cocoa/ContextualizedCFString.mm
    text/cocoa/ContextualizedNSString.mm
    text/cocoa/StringCocoa.mm
    text/cocoa/StringImplCocoa.mm
    text/cocoa/StringViewCocoa.mm
    text/cocoa/TextBreakIteratorInternalICUCocoa.cpp
)

file(COPY mac/MachExceptions.defs DESTINATION ${WTF_DERIVED_SOURCES_DIR})

add_custom_command(
    OUTPUT
        ${WTF_DERIVED_SOURCES_DIR}/MachExceptionsServer.h
        ${WTF_DERIVED_SOURCES_DIR}/mach_exc.h
        ${WTF_DERIVED_SOURCES_DIR}/mach_excServer.c
        ${WTF_DERIVED_SOURCES_DIR}/mach_excUser.c
    MAIN_DEPENDENCY mac/MachExceptions.defs
    WORKING_DIRECTORY ${WTF_DERIVED_SOURCES_DIR}
    COMMAND mig -DMACH_EXC_SERVER_TASKIDTOKEN_STATE -sheader MachExceptionsServer.h MachExceptions.defs
    VERBATIM)
list(APPEND WTF_SOURCES
    ${WTF_DERIVED_SOURCES_DIR}/mach_excServer.c
    ${WTF_DERIVED_SOURCES_DIR}/mach_excUser.c
)
