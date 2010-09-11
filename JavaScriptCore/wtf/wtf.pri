# wtf - qmake build info

SOURCES += \
    wtf/Assertions.cpp \
    wtf/ByteArray.cpp \
    wtf/CurrentTime.cpp \
    wtf/DateMath.cpp \
    wtf/dtoa.cpp \
    wtf/FastMalloc.cpp \
    wtf/HashTable.cpp \
    wtf/MD5.cpp \
    wtf/MainThread.cpp \
    wtf/qt/MainThreadQt.cpp \
    wtf/qt/StringQt.cpp \
    wtf/qt/ThreadingQt.cpp \
    wtf/PageAllocation.cpp \
    wtf/RandomNumber.cpp \
    wtf/RefCountedLeakCounter.cpp \
    wtf/ThreadingNone.cpp \
    wtf/Threading.cpp \
    wtf/TypeTraits.cpp \
    wtf/WTFThreadData.cpp \
    wtf/text/AtomicString.cpp \
    wtf/text/CString.cpp \
    wtf/text/StringImpl.cpp \
    wtf/text/StringStatics.cpp \
    wtf/text/WTFString.cpp \
    wtf/unicode/CollatorDefault.cpp \
    wtf/unicode/icu/CollatorICU.cpp \
    wtf/unicode/UTF8.cpp

!contains(DEFINES, USE_SYSTEM_MALLOC) {
    SOURCES += wtf/TCSystemAlloc.cpp
}

