set(WTF_HEADERS
    ASCIICType.h
    Assertions.h
    Atomics.h
    AutomaticThread.h
    BackwardsGraph.h
    Bag.h
    BagToHashMap.h
    BitVector.h
    Bitmap.h
    Brigand.h
    BubbleSort.h
    BumpPointerAllocator.h
    ByteOrder.h
    CPUTime.h
    CagedPtr.h
    ClockType.h
    CompilationThread.h
    Compiler.h
    Condition.h
    CrossThreadCopier.h
    CrossThreadTask.h
    CryptographicUtilities.h
    CryptographicallyRandomNumber.h
    CurrentTime.h
    DataLog.h
    DateMath.h
    Dominators.h
    DecimalNumber.h
    DeferrableRefCounted.h
    Deque.h
    DisallowCType.h
    DoublyLinkedList.h
    FastMalloc.h
    FastTLS.h
    FeatureDefines.h
    FilePrintStream.h
    FlipBytes.h
    Forward.h
    FunctionDispatcher.h
    GetPtr.h
    Gigacage.h
    GlobalVersion.h
    GraphNodeWorklist.h
    GregorianDateTime.h
    HashCountedSet.h
    Hasher.h
    HashFunctions.h
    HashIterators.h
    HashMap.h
    HashMethod.h
    HashSet.h
    HashTable.h
    HashTraits.h
    HexNumber.h
    IndexMap.h
    IndexSet.h
    IndexSparseSet.h
    IndexedContainerIterator.h
    IteratorAdaptors.h
    IteratorRange.h
    ListHashSet.h
    Liveness.h
    Lock.h
    LockAlgorithmInlines.h
    LockAlgorithm.h
    LockedPrintStream.h
    Locker.h
    LocklessBag.h
    LoggingHashID.h
    LoggingHashMap.h
    LoggingHashSet.h
    LoggingHashTraits.h
    MD5.h
    MainThread.h
    MallocPtr.h
    MathExtras.h
    MediaTime.h
    MemoryFootprint.h
    MemoryPressureHandler.h
    MessageQueue.h
    MetaAllocator.h
    MetaAllocatorHandle.h
    NaturalLoops.h
    MonotonicTime.h
    Noncopyable.h
    NumberOfCores.h
    OSAllocator.h
    OSRandomSource.h
    OptionSet.h
    OrderMaker.h
    PageAllocation.h
    PageBlock.h
    PageReservation.h
    ParallelHelperPool.h
    ParallelJobs.h
    ParallelJobsGeneric.h
    ParallelJobsLibdispatch.h
    ParallelJobsOpenMP.h
    ParallelVectorIterator.h
    ParkingLot.h
    Platform.h
    PlatformRegisters.h
    PrintStream.h
    ProcessID.h
    RAMSize.h
    RandomDevice.h
    RandomNumber.h
    RandomNumberSeed.h
    Range.h
    RangeSet.h
    RawPointer.h
    ReadWriteLock.h
    RecursiveLockAdapter.h
    RedBlackTree.h
    Ref.h
    RefCounted.h
    RefCountedLeakCounter.h
    RefCounter.h
    RefPtr.h
    RetainPtr.h
    RunLoop.h
    SHA1.h
    SharedTask.h
    SaturatedArithmetic.h
    ScopedLambda.h
    Seconds.h
    SegmentedVector.h
    SmallPtrSet.h
    StackBounds.h
    StackStats.h
    StackTrace.h
    StaticConstructors.h
    StdLibExtras.h
    Stopwatch.h
    StringExtras.h
    StringPrintStream.h
    SystemFree.h
    SystemTracing.h
    ThreadGroup.h
    ThreadMessage.h
    ThreadSafeRefCounted.h
    ThreadSpecific.h
    Threading.h
    ThreadingPrimitives.h
    TimeWithDynamicClockType.h
    TinyPtrSet.h
    UUID.h
    UniqueRef.h
    VMTags.h
    ValueCheck.h
    Variant.h
    Vector.h
    VectorTraits.h
    WallTime.h
    WeakPtr.h
    WordLock.h
    WorkQueue.h
    dtoa.h

    dtoa/bignum-dtoa.h
    dtoa/bignum.h
    dtoa/cached-powers.h
    dtoa/diy-fp.h
    dtoa/double-conversion.h
    dtoa/double.h
    dtoa/fast-dtoa.h
    dtoa/fixed-dtoa.h
    dtoa/strtod.h
    dtoa/utils.h

    text/AtomicString.h
    text/AtomicStringImpl.h
    text/AtomicStringTable.h
    text/Base64.h
    text/CString.h
    text/IntegerToStringConversion.h
    text/LChar.h
    text/LineBreakIteratorPoolICU.h
    text/StringBuffer.h
    text/StringCommon.h
    text/StringHash.h
    text/StringImpl.h
    text/StringView.h
    text/SymbolImpl.h
    text/SymbolRegistry.h
    text/TextBreakIterator.h
    text/TextBreakIteratorInternalICU.h
    text/TextStream.h
    text/UniquedStringImpl.h
    text/ValueToString.h
    text/WTFString.h

    text/icu/UTextProvider.h
    text/icu/UTextProviderLatin1.h
    text/icu/UTextProviderUTF16.h

    threads/BinarySemaphore.h
    threads/Signals.h

    unicode/CharacterNames.h
    unicode/Collator.h
    unicode/UTF8.h
)

set(WTF_SOURCES
    Assertions.cpp
    Atomics.cpp
    AutomaticThread.cpp
    BitVector.cpp
    CPUTime.cpp
    ClockType.cpp
    CompilationThread.cpp
    CrossThreadCopier.cpp
    CryptographicUtilities.cpp
    CryptographicallyRandomNumber.cpp
    CurrentTime.cpp
    DataLog.cpp
    DateMath.cpp
    DecimalNumber.cpp
    FastBitVector.cpp
    FastMalloc.cpp
    FilePrintStream.cpp
    FunctionDispatcher.cpp
    Gigacage.cpp
    GlobalVersion.cpp
    GregorianDateTime.cpp
    HashTable.cpp
    Lock.cpp
    LockedPrintStream.cpp
    MD5.cpp
    MainThread.cpp
    MediaTime.cpp
    MemoryPressureHandler.cpp
    MetaAllocator.cpp
    MonotonicTime.cpp
    NumberOfCores.cpp
    OSRandomSource.cpp
    PageBlock.cpp
    ParallelHelperPool.cpp
    ParallelJobsGeneric.cpp
    ParkingLot.cpp
    PrintStream.cpp
    RAMSize.cpp
    RandomDevice.cpp
    RandomNumber.cpp
    ReadWriteLock.cpp
    RefCountedLeakCounter.cpp
    RunLoop.cpp
    SHA1.cpp
    Seconds.cpp
    SixCharacterHash.cpp
    StackBounds.cpp
    StackStats.cpp
    StackTrace.cpp
    StringPrintStream.cpp
    ThreadGroup.cpp
    ThreadMessage.cpp
    Threading.cpp
    TimeWithDynamicClockType.cpp
    UUID.cpp
    WallTime.cpp
    WordLock.cpp
    WorkQueue.cpp
    dtoa.cpp

    dtoa/bignum-dtoa.cc
    dtoa/bignum.cc
    dtoa/cached-powers.cc
    dtoa/diy-fp.cc
    dtoa/double-conversion.cc
    dtoa/fast-dtoa.cc
    dtoa/fixed-dtoa.cc
    dtoa/strtod.cc

    persistence/Coders.cpp
    persistence/Decoder.cpp
    persistence/Encoder.cpp

    text/AtomicString.cpp
    text/AtomicStringImpl.cpp
    text/AtomicStringTable.cpp
    text/Base64.cpp
    text/CString.cpp
    text/StringBuilder.cpp
    text/StringImpl.cpp
    text/StringView.cpp
    text/SymbolImpl.cpp
    text/SymbolRegistry.cpp
    text/TextBreakIterator.cpp
    text/TextStream.cpp
    text/WTFString.cpp

    text/icu/UTextProvider.cpp
    text/icu/UTextProviderLatin1.cpp
    text/icu/UTextProviderUTF16.cpp

    threads/BinarySemaphore.cpp
    threads/Signals.cpp

    unicode/UTF8.cpp
)

set(WTF_INCLUDE_DIRECTORIES
    "${BMALLOC_DIR}"
    "${WTF_DIR}"
    "${CMAKE_BINARY_DIR}"
    "${DERIVED_SOURCES_DIR}"
    "${THIRDPARTY_DIR}"
)

set(WTF_PRIVATE_INCLUDE_DIRECTORIES
    "${WTF_DIR}/wtf"
    "${WTF_DIR}/wtf/dtoa"
    "${WTF_DIR}/wtf/persistence"
    "${WTF_DIR}/wtf/text"
    "${WTF_DIR}/wtf/text/icu"
    "${WTF_DIR}/wtf/threads"
    "${WTF_DIR}/wtf/unicode"
)

set(WTF_LIBRARIES
    ${CMAKE_DL_LIBS}
)

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    list(APPEND WTF_HEADERS
        spi/darwin/CommonCryptoSPI.h
    )
    list(APPEND WTF_INCLUDE_DIRECTORIES
        "${WTF_DIR}/wtf/spi/darwin"
    )
endif ()

if (NOT USE_SYSTEM_MALLOC)
    list(APPEND WTF_LIBRARIES bmalloc)
endif ()

list(APPEND WTF_SOURCES
    unicode/icu/CollatorICU.cpp
)
set(WTF_SYSTEM_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
)
list(APPEND WTF_LIBRARIES
    ${ICU_DATA_LIBRARIES}
    ${ICU_I18N_LIBRARIES}
    ${ICU_LIBRARIES}
)

if (WIN32)
    list(APPEND WTF_SOURCES
        OSAllocatorWin.cpp
        ThreadSpecificWin.cpp
        ThreadingWin.cpp
    )
    list(APPEND WTF_LIBRARIES
        winmm
    )
else ()
    list(APPEND WTF_SOURCES
        OSAllocatorPosix.cpp
        ThreadingPthreads.cpp
    )
endif ()

WEBKIT_FRAMEWORK_DECLARE(WTF)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_WRAP_SOURCELIST(${WTF_SOURCES})
WEBKIT_FRAMEWORK(WTF)

if (MSVC)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

    add_custom_command(
        TARGET WTF
        PRE_BUILD
        COMMAND ${PERL_EXECUTABLE} ${WEBKIT_LIBRARIES_DIR}/tools/scripts/auto-version.pl ${DERIVED_SOURCES_DIR}
        VERBATIM)

    add_custom_command(
        TARGET WTF
        POST_BUILD
        COMMAND ${PERL_EXECUTABLE} ${WEBKIT_LIBRARIES_DIR}/tools/scripts/version-stamp.pl ${DERIVED_SOURCES_DIR} $<TARGET_FILE:WTF>
        VERBATIM)
endif ()
