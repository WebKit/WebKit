list(APPEND WTF_HEADERS
    unicode/wchar/UnicodeWchar.h
)

list(APPEND WTF_SOURCES
    NullPtr.cpp
    OSAllocatorWin.cpp
    ThreadingWin.cpp
    ThreadSpecificWin.cpp

    threads/win/BinarySemaphoreWin.cpp

    unicode/CollatorDefault.cpp
    unicode/wchar/UnicodeWchar.cpp

    win/MainThreadWin.cpp
    win/OwnPtrWin.cpp
)

list(APPEND WTF_LIBRARIES
    mmtimer
)
