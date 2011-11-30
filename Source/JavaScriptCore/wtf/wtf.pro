# -------------------------------------------------------------------
# Project file for WTF
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = lib

DEFINES += BUILDING_WTF

load(wtf)


CONFIG += staticlib
# Don't use WTF as the target name. qmake would create a
# WTF.vcproj for msvc which already exists as a directory
TARGET = $$WTF_TARGET
DESTDIR = $$WTF_DESTDIR
QT += core
QT -= gui

*-g++*:QMAKE_CXXFLAGS_RELEASE -= -O2
*-g++*:QMAKE_CXXFLAGS_RELEASE += -O3

HEADERS += \
    Alignment.h \
    AlwaysInline.h \
    ArrayBuffer.h \
    ArrayBufferView.h \
    ASCIICType.h \
    Assertions.h \
    Atomics.h \
    AVLTree.h \
    Bitmap.h \
    BitVector.h \
    BloomFilter.h \
    BoundsCheckedPointer.h \
    BumpPointerAllocator.h \
    ByteArray.h \
    CheckedArithmetic.h \
    Compiler.h \
    CryptographicallyRandomNumber.h \
    CurrentTime.h \
    DateMath.h \
    DecimalNumber.h \
    Decoder.h \
    Deque.h \
    DisallowCType.h \
    dtoa.h \
    dtoa/bignum-dtoa.h \
    dtoa/bignum.h \
    dtoa/cached-powers.h \
    dtoa/diy-fp.h \
    dtoa/double-conversion.h \
    dtoa/double.h \
    dtoa/fast-dtoa.h \
    dtoa/fixed-dtoa.h \
    dtoa/strtod.h \
    dtoa/utils.h \
    DynamicAnnotations.h \
    Encoder.h \
    FastAllocBase.h \
    FastMalloc.h \
    FixedArray.h \
    Float32Array.h \
    Float64Array.h \
    Forward.h \
    GetPtr.h \
    HashCountedSet.h \
    HashFunctions.h \
    HashIterators.h \
    HashMap.h \
    HashSet.h \
    HashTable.h \
    HashTraits.h \
    HexNumber.h \
    Int16Array.h \
    Int32Array.h \
    Int8Array.h \
    ListHashSet.h \
    ListRefPtr.h \
    Locker.h \
    MainThread.h \
    MallocZoneSupport.h \
    MathExtras.h \
    MD5.h \
    MessageQueue.h \
    MetaAllocator.h \
    MetaAllocatorHandle.h \
    Noncopyable.h \
    NonCopyingSort.h \
    NotFound.h \
    NullPtr.h \
    OSAllocator.h \
    OSRandomSource.h \
    OwnArrayPtr.h \
    OwnFastMallocPtr.h \
    OwnPtr.h \
    OwnPtrCommon.h \
    PackedIntVector.h \
    PageAllocation.h \
    PageAllocationAligned.h \
    PageBlock.h \
    PageReservation.h \
    ParallelJobs.h \
    ParallelJobsGeneric.h \
    ParallelJobsLibdispatch.h \
    ParallelJobsOpenMP.h \
    PassOwnArrayPtr.h \
    PassOwnPtr.h \
    PassRefPtr.h \
    PassTraits.h \
    Platform.h \
    PossiblyNull.h \
    qt/UtilsQt.h \
    RandomNumber.h \
    RandomNumberSeed.h \
    RedBlackTree.h \
    RefCounted.h \
    RefCountedLeakCounter.h \
    RefPtr.h \
    RefPtrHashMap.h \
    RetainPtr.h \
    SHA1.h \
    Spectrum.h \
    StackBounds.h \
    StaticConstructors.h \
    StdLibExtras.h \
    StringExtras.h \
    StringHasher.h \
    TCPackedCache.h \
    TCSpinLock.h \
    TCSystemAlloc.h \
    text/AtomicString.h \
    text/AtomicStringHash.h \
    text/AtomicStringImpl.h \
    text/CString.h \
    text/StringBuffer.h \
    text/StringBuilder.h \
    text/StringConcatenate.h \
    text/StringHash.h \
    text/StringImpl.h \
    text/StringOperators.h \
    text/TextPosition.h \
    text/WTFString.h \
    Threading.h \
    ThreadingPrimitives.h \
    ThreadRestrictionVerifier.h \
    ThreadSafeRefCounted.h \
    ThreadSpecific.h \
    TypeTraits.h \
    Uint16Array.h \
    Uint32Array.h \
    Uint8Array.h \
    unicode/CharacterNames.h \
    unicode/Collator.h \
    unicode/icu/UnicodeIcu.h \
    unicode/qt4/UnicodeQt4.h \
    unicode/ScriptCodesFromICU.h \
    unicode/Unicode.h \
    unicode/UnicodeMacrosFromICU.h \
    unicode/UTF8.h \
    UnusedParam.h \
    ValueCheck.h \
    Vector.h \
    VectorTraits.h \
    VMTags.h \
    WTFThreadData.h


unix: HEADERS += ThreadIdentifierDataPthreads.h

SOURCES += \
    ArrayBuffer.cpp \
    ArrayBufferView.cpp \
    Assertions.cpp \
    BitVector.cpp \
    ByteArray.cpp \
    CryptographicallyRandomNumber.cpp \
    CurrentTime.cpp \
    DateMath.cpp \
    DecimalNumber.cpp \
    dtoa.cpp \
    dtoa/bignum-dtoa.cc \
    dtoa/bignum.cc \
    dtoa/cached-powers.cc \
    dtoa/diy-fp.cc \
    dtoa/double-conversion.cc \
    dtoa/fast-dtoa.cc \
    dtoa/fixed-dtoa.cc \
    dtoa/strtod.cc \
    FastMalloc.cpp \
    gobject/GOwnPtr.cpp \
    gobject/GRefPtr.cpp \
    HashTable.cpp \
    MD5.cpp \
    MainThread.cpp \
    MetaAllocator.cpp \
    NullPtr.cpp \
    OSRandomSource.cpp \
    qt/MainThreadQt.cpp \
    qt/StringQt.cpp \
    PageAllocationAligned.cpp \
    PageBlock.cpp \
    ParallelJobsGeneric.cpp \
    RandomNumber.cpp \
    RefCountedLeakCounter.cpp \
    SHA1.cpp \
    StackBounds.cpp \
    TCSystemAlloc.cpp \
    Threading.cpp \
    TypeTraits.cpp \
    WTFThreadData.cpp \
    text/AtomicString.cpp \
    text/CString.cpp \
    text/StringBuilder.cpp \
    text/StringImpl.cpp \
    text/StringStatics.cpp \
    text/WTFString.cpp \
    unicode/CollatorDefault.cpp \
    unicode/icu/CollatorICU.cpp \
    unicode/UTF8.cpp

unix: SOURCES += \
    OSAllocatorPosix.cpp \
    ThreadIdentifierDataPthreads.cpp \
    ThreadingPthreads.cpp

win*|wince*: SOURCES += \
    OSAllocatorWin.cpp \
    ThreadSpecificWin.cpp \
    ThreadingWin.cpp

*sh4* {
    QMAKE_CXXFLAGS += -mieee -w
    QMAKE_CFLAGS   += -mieee -w
}

lessThan(QT_GCC_MAJOR_VERSION, 5) {
    # GCC 4.5 and before
    lessThan(QT_GCC_MINOR_VERSION, 6) {
        # Disable C++0x mode in JSC for those who enabled it in their Qt's mkspec.
        *-g++*:QMAKE_CXXFLAGS -= -std=c++0x -std=gnu++0x
    }

    # GCC 4.6 and after.
    greaterThan(QT_GCC_MINOR_VERSION, 5) {
        if (!contains(QMAKE_CXXFLAGS, -std=c++0x) && !contains(QMAKE_CXXFLAGS, -std=gnu++0x)) {
            # We need to deactivate those warnings because some names conflicts with upcoming c++0x types (e.g.nullptr).
            QMAKE_CFLAGS_WARN_ON += -Wno-c++0x-compat
            QMAKE_CXXFLAGS_WARN_ON += -Wno-c++0x-compat
            QMAKE_CFLAGS += -Wno-c++0x-compat
            QMAKE_CXXFLAGS += -Wno-c++0x-compat
        }
    }
}
