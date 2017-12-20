list(APPEND WTF_HEADERS
    cf/TypeCastsCF.h
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

set(WTF_PRE_BUILD_COMMAND "${CMAKE_BINARY_DIR}/DerivedSources/WTF/preBuild.cmd")
file(WRITE "${WTF_PRE_BUILD_COMMAND}" "@xcopy /y /s /d /f \"${WTF_DIR}/wtf/*.h\" \"${FORWARDING_HEADERS_DIR}/wtf\" >nul 2>nul\n@xcopy /y /s /d /f \"${DERIVED_SOURCES_DIR}/WTF/*.h\" \"${FORWARDING_HEADERS_DIR}/wtf\" >nul 2>nul\n")
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_DIR}/wtf)

set(WTF_OUTPUT_NAME WTF${DEBUG_SUFFIX})
