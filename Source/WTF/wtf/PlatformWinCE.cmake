list(APPEND WTF_SOURCES
    NullPtr.cpp
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
