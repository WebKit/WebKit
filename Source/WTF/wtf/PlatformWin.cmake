if (WTF_CPU_X86_64)
    if (MSVC)
        add_custom_command(
            OUTPUT ${WTF_DERIVED_SOURCES_DIR}/AsmStubsMSVC64.obj
            MAIN_DEPENDENCY ${WTF_DIR}/wtf/win/AsmStubsMSVC64.asm
            COMMAND ml64 -nologo -c -Fo ${WTF_DERIVED_SOURCES_DIR}/AsmStubsMSVC64.obj ${WTF_DIR}/wtf/win/AsmStubsMSVC64.asm
            VERBATIM)

        list(APPEND WTF_SOURCES ${WTF_DERIVED_SOURCES_DIR}/AsmStubsMSVC64.obj)
    endif ()
endif ()

list(APPEND WTF_PUBLIC_HEADERS
    text/win/WCharStringExtras.h

    win/DbgHelperWin.h
    win/GDIObject.h
    win/SoftLinking.h
    win/Win32Handle.h
)

list(APPEND WTF_SOURCES
    generic/WorkQueueGeneric.cpp

    text/win/StringWin.cpp
    text/win/TextBreakIteratorInternalICUWin.cpp

    win/CPUTimeWin.cpp
    win/DbgHelperWin.cpp
    win/FileSystemWin.cpp
    win/LanguageWin.cpp
    win/LoggingWin.cpp
    win/MainThreadWin.cpp
    win/MemoryFootprintWin.cpp
    win/MemoryPressureHandlerWin.cpp
    win/OSAllocatorWin.cpp
    win/PathWalker.cpp
    win/RunLoopWin.cpp
    win/SignalsWin.cpp
    win/ThreadingWin.cpp
    win/Win32Handle.cpp
)

list(APPEND WTF_LIBRARIES
    DbgHelp
    shlwapi
    winmm
)
