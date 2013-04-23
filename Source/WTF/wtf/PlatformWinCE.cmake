list(APPEND WTF_SOURCES
    OSAllocatorWin.cpp
    ThreadingWin.cpp
    ThreadSpecificWin.cpp

    threads/win/BinarySemaphoreWin.cpp

    win/MainThreadWin.cpp
    win/OwnPtrWin.cpp
)

list(APPEND WTF_LIBRARIES
    mmtimer
)
