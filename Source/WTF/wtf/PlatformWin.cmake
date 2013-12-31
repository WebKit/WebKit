list(APPEND WTF_SOURCES
    win/MainThreadWin.cpp
    win/RunLoopWin.cpp
)

if (WINCE)
    list(APPEND WTF_LIBRARIES
        mmtimer
    )
else ()
    list(APPEND WTF_LIBRARIES
        winmm
    )
endif ()
