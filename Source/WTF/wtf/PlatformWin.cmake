list(APPEND WTF_HEADERS
    cf/TypeCastsCF.h
    text/win/WCharStringExtras.h
)

list(APPEND WTF_SOURCES
    text/win/TextBreakIteratorInternalICUWin.cpp

    win/CPUTimeWin.cpp
    win/LanguageWin.cpp
    win/MainThreadWin.cpp
    win/MemoryFootprintWin.cpp
    win/MemoryPressureHandlerWin.cpp
    win/RunLoopWin.cpp
    win/WorkItemContext.cpp
    win/WorkQueueWin.cpp
)

if (USE_CF)
    list(APPEND WTF_SOURCES
        text/cf/AtomicStringImplCF.cpp
        text/cf/StringCF.cpp
        text/cf/StringImplCF.cpp
        text/cf/StringViewCF.cpp
    )

    list(APPEND WTF_LIBRARIES ${COREFOUNDATION_LIBRARY})
endif ()

set(WTF_FORWARDING_HEADERS_DIRECTORIES
    .
    cf
    dtoa
    generic
    persistence
    spi
    text
    text/cf
    text/icu
    text/win
    threads
    unicode
    win
)
WEBKIT_MAKE_FORWARDING_HEADERS(WTF
    DESTINATION ${FORWARDING_HEADERS_DIR}/wtf
    DIRECTORIES ${WTF_FORWARDING_HEADERS_DIRECTORIES})

set(WTF_OUTPUT_NAME WTF${DEBUG_SUFFIX})
