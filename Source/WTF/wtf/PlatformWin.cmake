list(APPEND WTF_SOURCES
    threads/win/BinarySemaphoreWin.cpp

    win/MainThreadWin.cpp
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
