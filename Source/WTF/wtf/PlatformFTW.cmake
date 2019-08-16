list(APPEND WTF_PUBLIC_HEADERS
    text/win/WCharStringExtras.h

    win/DbgHelperWin.h
    win/GDIObject.h
    win/SoftLinking.h
    win/Win32Handle.h
)

set(WTF_LIBRARIES PUBLIC ${WTF_LIBRARIES})
list(APPEND WTF_LIBRARIES
    PRIVATE Dbghelp # For symbol handling <dbghelp.h>
    PRIVATE Shlwapi # For file system operations <shlwapi.h>
    PUBLIC  Winmm   # For multimedia functions, also used in <timeapi.h>
)

list(APPEND WTF_SOURCES
    text/win/StringWin.cpp
    text/win/TextBreakIteratorInternalICUWin.cpp

    win/CPUTimeWin.cpp
    win/DbgHelperWin.cpp
    win/FileSystemWin.cpp
    win/LanguageWin.cpp
    win/MainThreadWin.cpp
    win/MemoryFootprintWin.cpp
    win/MemoryPressureHandlerWin.cpp
    win/OSAllocatorWin.cpp
    win/PathWalker.cpp
    win/RunLoopWin.cpp
    win/ThreadSpecificWin.cpp
    win/ThreadingWin.cpp
    win/WorkQueueWin.cpp
)

if (USE_CF)
    list(APPEND WTF_PUBLIC_HEADERS
        cf/CFURLExtras.h
        cf/TypeCastsCF.h

        text/cf/TextBreakIteratorCF.h
    )
    list(APPEND WTF_SOURCES
        cf/CFURLExtras.cpp
        cf/FileSystemCF.cpp
        cf/URLCF.cpp

        text/cf/AtomStringImplCF.cpp
        text/cf/StringCF.cpp
        text/cf/StringImplCF.cpp
        text/cf/StringViewCF.cpp
    )

    list(APPEND WTF_LIBRARIES ${COREFOUNDATION_LIBRARY})
endif ()

set(WTF_OUTPUT_NAME WTF${DEBUG_SUFFIX})
