if (ENABLE_MALLOC_HEAP_BREAKDOWN AND NOT APPLE)
    include(MallocHeapBreakdown.cmake)
endif ()

set(WTF_PUBLIC_HEADERS
    AbstractCanMakeCheckedPtr.h
    AbstractRefCounted.h
    AbstractRefCountedAndCanMakeWeakPtr.h
    AbstractThreadSafeRefCountedAndCanMakeWeakPtr.h
    AllocSpanMixin.h
    ASCIICType.h
    AccessibleAddress.h
    AggregateLogger.h
    AlignedStorage.h
    AnsiColors.h
    ApproximateTime.h
    ArgumentCoder.h
    Assertions.h
    Atomics.h
    AutodrainedPool.h
    AutomaticThread.h
    BackwardsGraph.h
    Bag.h
    BagToHashMap.h
    BitSet.h
    BitVector.h
    BlockObjCExceptions.h
    BlockPtr.h
    BloomFilter.h
    BooleanLattice.h
    Box.h
    BoxPtr.h
    Brigand.h
    BubbleSort.h
    BumpPointerAllocator.h
    ButterflyArray.h
    ByteOrder.h
    CPUTime.h
    CanMakeWeakPtr.h
    CagedPtr.h
    CagedUniquePtr.h
    CallbackAggregator.h
    CancellableTask.h
    CheckedArithmetic.h
    CheckedPtr.h
    CheckedRef.h
    ClockType.h
    CodePtr.h
    CommaPrinter.h
    CompactPointerTuple.h
    CompactPtr.h
    CompactRefPtr.h
    CompactRefPtrTuple.h
    CompactUniquePtrTuple.h
    CompactVariant.h
    CompactVariantOperations.h
    CompilationThread.h
    Compiler.h
    CompletionHandler.h
    ConcurrentBuffer.h
    ConcurrentPtrHashSet.h
    ConcurrentVector.h
    Condition.h
    ContinuousApproximateTime.h
    ContinuousTime.h
    CoroutineUtilities.h
    CountingLock.h
    CrossThreadCopier.h
    CrossThreadQueue.h
    CrossThreadTask.h
    CrossThreadTaskHandler.h
    CryptographicUtilities.h
    CryptographicallyRandomNumber.h
    DataLog.h
    DataMutex.h
    DataRef.h
    DateMath.h
    DebugHeap.h
    DebugUtilities.h
    DeferrableRefCounted.h
    Deque.h
    Dominators.h
    DoublyLinkedList.h
    EmbeddedFixedVector.h
    EnumClassOperatorOverloads.h
    EnumSet.h
    EnumTraits.h
    EnumeratedArray.h
    Expected.h
    ExperimentalFeatureNames.h
    ExportMacros.h
    FailureAction.h
    FastBitVector.h
    FastFloat.h
    FastMalloc.h
    FastTLS.h
    FileHandle.h
    FileLockMode.h
    FilePrintStream.h
    FileSystem.h
    FixedBitVector.h
    FixedVector.h
    FixedWidthDouble.h
    FlatteningVariantAdaptor.h
    FlipBytes.h
    Float16.h
    ForbidHeapAllocation.h
    Forward.h
    Function.h
    FunctionDispatcher.h
    FunctionPtr.h
    FunctionTraits.h
    GenericHashKey.h
    GenericTimeMixin.h
    GetPtr.h
    Gigacage.h
    GlobalVersion.h
    GraphNodeWorklist.h
    GregorianDateTime.h
    HashCountedSet.h
    HashFunctions.h
    HashIterators.h
    HashMap.h
    HashMethod.h
    HashSet.h
    HashTable.h
    HashTraits.h
    Hasher.h
    HexNumber.h
    Identified.h
    Indenter.h
    IndexedRange.h
    IndexKeyType.h
    IndexMap.h
    IndexSet.h
    IndexSparseSet.h
    IndexedContainerIterator.h
    InlineASM.h
    Insertion.h
    InstanceCounted.h
    Int128.h
    InterferenceGraph.h
    IntervalSet.h
    IsoMalloc.h
    IsoMallocInlines.h
    IterationStatus.h
    IteratorRange.h
    JSONValues.h
    JSValueMalloc.h
    KeyValuePair.h
    LEBDecoder.h
    LLVMProfilingUtils.h
    Language.h
    LazyRef.h
    LazyUniqueRef.h
    LikelyDenseUnsignedIntegerSet.h
    ListDump.h
    ListHashSet.h
    Liveness.h
    Lock.h
    LockAlgorithm.h
    LockAlgorithmInlines.h
    LockedPrintStream.h
    Locker.h
    LocklessBag.h
    LogChannels.h
    LogInitialization.h
    Logger.h
    LoggerHelper.h
    Logging.h
    LoggingAccumulator.h
    LoggingHashID.h
    LoggingHashMap.h
    LoggingHashSet.h
    LoggingHashTraits.h
    MachSendRight.h
    MainThread.h
    MainThreadData.h
    MainThreadDispatcher.h
    MallocCommon.h
    MallocPtr.h
    MallocSpan.h
    MappedFileData.h
    Markable.h
    MathExtras.h
    MediaTime.h
    MemoryFootprint.h
    MemoryPressureHandler.h
    MessageQueue.h
    MetaAllocator.h
    MetaAllocatorHandle.h
    MmapSpan.h
    MonotonicTime.h
    NakedPtr.h
    NativePromise.h
    NaturalLoops.h
    NeverDestroyed.h
    NoLock.h
    NoTailCalls.h
    NoVirtualDestructorBase.h
    Noncopyable.h
    Nonmovable.h
    NotFound.h
    NumberOfCores.h
    OSAllocator.h
    OSObjectPtr.h
    OSRandomSource.h
    ObjCRuntimeExtras.h
    ObjectIdentifier.h
    Observer.h
    OptionCountedSet.h
    OptionSet.h
    OptionalOrReference.h
    OptionSetHash.h
    OrderMaker.h
    OverflowPolicy.h
    Packed.h
    PackedRef.h
    PackedRefPtr.h
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
    PlatformCallingConventions.h
    PlatformCPU.h
    PlatformEnable.h
    PlatformEnableCocoa.h
    PlatformEnableGlib.h
    PlatformEnablePlayStation.h
    PlatformEnableWin.h
    PlatformHave.h
    PlatformLegacy.h
    PlatformOS.h
    PlatformRegisters.h
    PlatformUse.h
    PointerComparison.h
    PointerPreparations.h
    PreciseSum.h
    PrintStream.h
    PriorityQueue.h
    ProcessID.h
    ProcessPrivilege.h
    PtrTag.h
    RAMSize.h
    RandomDevice.h
    Range.h
    RangeSet.h
    RawHex.h
    RawPointer.h
    RawPtrTraits.h
    RawValueTraits.h
    ReadWriteLock.h
    RecursableLambda.h
    RecursiveLockAdapter.h
    RedBlackTree.h
    Ref.h
    RefCountable.h
    RefCountDebugger.h
    RefCounted.h
    RefCountedAndCanMakeWeakPtr.h
    RefCountedFixedVector.h
    RefCounter.h
    RefPtr.h
    RefTrackerMixin.h
    RefVector.h
    ReferenceWrapperVector.h
    ResourceUsage.h
    RetainPtr.h
    RetainReleaseSwift.h
    RobinHoodHashMap.h
    RobinHoodHashSet.h
    RobinHoodHashTable.h
    RunLoop.h
    RuntimeApplicationChecks.h
    SHA1.h
    SIMDHelpers.h
    SIMDUTF.h
    SafeStrerror.h
    SaturatedArithmetic.h
    SchedulePair.h
    Scope.h
    ScopedLambda.h
    ScopedPrintStream.h
    Seconds.h
    SegmentedVector.h
    SentinelLinkedList.h
    SequesteredAllocator.h
    SequesteredImmortalHeap.h
    SequesteredMalloc.h
    SetForScope.h
    SharedTask.h
    SignedPtr.h
    SimpleStats.h
    SingleRootGraph.h
    SingleThreadIntegralWrapper.h
    SinglyLinkedList.h
    SinglyLinkedListWithTail.h
    SixCharacterHash.h
    SmallMap.h
    SmallSet.h
    SoftLinking.h
    SortedArrayMap.h
    SpanningTree.h
    Spectrum.h
    StackBounds.h
    StackCheck.h
    StackPointer.h
    StackShot.h
    StackShotProfiler.h
    StackStats.h
    StackTrace.h
    StatisticsManager.h
    StdFilesystem.h
    StdIntExtras.h
    StdLibExtras.h
    StdList.h
    StdMap.h
    StdMultimap.h
    StdSet.h
    StdUnorderedMap.h
    StdUnorderedSet.h
    Stopwatch.h
    StreamBuffer.h
    StringHashDumpContext.h
    StringPrintStream.h
    StructDump.h
    SuspendableWorkQueue.h
    SwiftBridging.h
    SynchronizedFixedQueue.h
    SystemFree.h
    SystemMalloc.h
    SystemTracing.h
    TZoneMalloc.h
    TZoneMallocInlines.h
    TaggedArrayStoragePtr.h
    TaggedPtr.h
    ThreadAssertions.h
    ThreadGroup.h
    ThreadMessage.h
    ThreadSafetyAnalysis.h
    ThreadSafeRefCounted.h
    ThreadSafeRefCountedWithSuppressingSaferCPPChecking.h
    ThreadSafeWeakHashSet.h
    ThreadSafeWeakPtr.h
    ThreadSanitizerSupport.h
    ThreadSpecific.h
    Threading.h
    ThreadingPrimitives.h
    TimeWithDynamicClockType.h
    TimingScope.h
    TinyLRUCache.h
    TinyPtrSet.h
    TrailingArray.h
    TranslatedProcess.h
    TriState.h
    TypeCasts.h
    TypeTraits.h
    URL.h
    URLHelpers.h
    URLHash.h
    URLParser.h
    UUID.h
    UnalignedAccess.h
    Unexpected.h
    UniStdExtras.h
    UnionFind.h
    UniqueArray.h
    UniqueRef.h
    UniqueRefVector.h
    VMTags.h
    ValidatedReinterpretCast.h
    ValueCheck.h
    ValueOrReference.h
    Variant.h
    VariantExtras.h
    VariantList.h
    VariantListOperations.h
    Vector.h
    VectorTraits.h
    WTFConfig.h
    WTFProcess.h
    WTFSemaphore.h
    WallTime.h
    WasmSIMD128.h
    WeakHashCountedSet.h
    WeakHashMap.h
    WeakHashSet.h
    WeakListHashSet.h
    WeakObjCPtr.h
    WeakPtr.h
    WeakPtrFactory.h
    WeakPtrImpl.h
    WeakRandom.h
    WeakRandomNumber.h
    WeakRef.h
    WindowsExtras.h
    WordLock.h
    WorkQueue.h
    WorkerPool.h
    ZippedRange.h
    dtoa.h

    dragonbox/dragonbox.h
    dragonbox/dragonbox_to_chars.h
    dragonbox/ieee754_format.h

    dragonbox/detail/bits.h
    dragonbox/detail/cache_holder.h
    dragonbox/detail/decimal_fp.h
    dragonbox/detail/div.h
    dragonbox/detail/log.h
    dragonbox/detail/policy.h
    dragonbox/detail/policy_holder.h
    dragonbox/detail/util.h
    dragonbox/detail/wuint.h

    dtoa/bignum-dtoa.h
    dtoa/bignum.h
    dtoa/cached-powers.h
    dtoa/diy-fp.h
    dtoa/double-conversion.h
    dtoa/fast-dtoa.h
    dtoa/fixed-dtoa.h
    dtoa/ieee.h
    dtoa/strtod.h
    dtoa/utils.h

    fast_float/ascii_number.h
    fast_float/bigint.h
    fast_float/decimal_to_binary.h
    fast_float/digit_comparison.h
    fast_float/fast_float.h
    fast_float/fast_table.h
    fast_float/float_common.h
    fast_float/parse_number.h
    fast_float/simple_decimal_conversion.h

    persistence/PersistentCoders.h
    persistence/PersistentDecoder.h
    persistence/PersistentEncoder.h

    simde/simde.h
    simde/arm/neon.h
    simde/arm/sve.h
    simde/wasm/simd128.h

    simdutf/simdutf_impl.h

    text/ASCIIFastPath.h
    text/ASCIILiteral.h
    text/AdaptiveStringSearcher.h
    text/AtomString.h
    text/AtomStringHash.h
    text/AtomStringImpl.h
    text/AtomStringTable.h
    text/Base64.h
    text/CString.h
    text/CStringView.h
    text/CharacterProperties.h
    text/CodePointIterator.h
    text/ConversionMode.h
    text/EscapedFormsForJSON.h
    text/ExternalStringImpl.h
    text/FastCharacterComparison.h
    text/IntegerToStringConversion.h
    text/Latin1Character.h
    text/LineEnding.h
    text/MakeString.h
    text/NullTextBreakIterator.h
    text/OrdinalNumber.h
    text/ParsingUtilities.h
    text/StringBuffer.h
    text/StringBuilder.h
    text/StringBuilderInternals.h
    text/StringBuilderJSON.h
    text/StringCommon.h
    text/StringConcatenate.h
    text/StringConcatenateNumbers.h
    text/StringHash.h
    text/StringHasher.h
    text/StringHasherInlines.h
    text/SuperFastHash.h
    text/WYHash.h
    text/StringImpl.h
    text/StringParsingBuffer.h
    text/StringSearch.h
    text/StringToIntegerConversion.h
    text/StringView.h
    text/SymbolImpl.h
    text/SymbolRegistry.h
    text/TextBreakIterator.h
    text/TextBreakIteratorInternalICU.h
    text/TextPosition.h
    text/TextStream.h
    text/UTF8ConversionError.h
    text/UniquedStringImpl.h
    text/WTFString.h

    text/icu/TextBreakIteratorICU.h
    text/icu/UnicodeExtras.h
    text/icu/UTextProvider.h
    text/icu/UTextProviderLatin1.h
    text/icu/UTextProviderUTF16.h

    threads/BinarySemaphore.h
    threads/Signals.h

    unicode/CharacterNames.h
    unicode/Collator.h
    unicode/UTF8Conversion.h

    unicode/icu/ICUHelpers.h
)

set(WTF_SOURCES
    ASCIICType.cpp
    ApproximateTime.cpp
    Assertions.cpp
    AutomaticThread.cpp
    BitVector.cpp
    BloomFilter.cpp
    CPUTime.cpp
    ClockType.cpp
    CodePtr.cpp
    CompilationThread.cpp
    ConcurrentBuffer.cpp
    ConcurrentPtrHashSet.cpp
    ContinuousApproximateTime.cpp
    ContinuousTime.cpp
    CountingLock.cpp
    CrossThreadCopier.cpp
    CrossThreadTaskHandler.cpp
    CryptographicUtilities.cpp
    CryptographicallyRandomNumber.cpp
    CurrentTime.cpp
    DataLog.cpp
    DateMath.cpp
    DebugHeap.cpp
    EmbeddedFixedVector.cpp
    FastBitVector.cpp
    FastFloat.cpp
    FastMalloc.cpp
    FileHandle.cpp
    FilePrintStream.cpp
    FileSystem.cpp
    FunctionDispatcher.cpp
    Gigacage.cpp
    GlobalVersion.cpp
    GregorianDateTime.cpp
    HashTable.cpp
    HexNumber.cpp
    Int128.cpp
    JSONValues.cpp
    Language.cpp
    LikelyDenseUnsignedIntegerSet.cpp
    Lock.cpp
    LockedPrintStream.cpp
    LogChannels.cpp
    LogInitialization.cpp
    Logger.cpp
    Logging.cpp
    MainThread.cpp
    MainThreadDispatcher.cpp
    MallocCommon.cpp
    MediaTime.cpp
    MemoryPressureHandler.cpp
    MetaAllocator.cpp
    MonotonicTime.cpp
    NativePromise.cpp
    NumberOfCores.cpp
    OSRandomSource.cpp
    ObjectIdentifier.cpp
    PageBlock.cpp
    ParallelHelperPool.cpp
    ParallelJobsGeneric.cpp
    ParkingLot.cpp
    PreciseSum.cpp
    PrintStream.cpp
    ProcessPrivilege.cpp
    RAMSize.cpp
    RandomDevice.cpp
    ReadWriteLock.cpp
    RefCountDebugger.cpp
    RefTrackerMixin.cpp
    RunLoop.cpp
    RuntimeApplicationChecks.cpp
    SHA1.cpp
    SIMDUTF.cpp
    SafeStrerror.cpp
    Seconds.cpp
    SegmentedVector.cpp
    SequesteredAllocator.cpp
    SequesteredImmortalHeap.cpp
    SequesteredMalloc.cpp
    SixCharacterHash.cpp
    SmallSet.cpp
    StackBounds.cpp
    StackCheck.cpp
    StackPointer.cpp
    StackStats.cpp
    StackTrace.cpp
    StatisticsManager.cpp
    StringPrintStream.cpp
    SuspendableWorkQueue.cpp
    ThreadGroup.cpp
    ThreadMessage.cpp
    Threading.cpp
    TimeWithDynamicClockType.cpp
    TimingScope.cpp
    URL.cpp
    URLHelpers.cpp
    URLParser.cpp
    UUID.cpp
    UniqueArray.cpp
    Vector.cpp
    WTFAssertions.cpp
    WTFConfig.cpp
    WTFProcess.cpp
    WallTime.cpp
    WeakPtr.cpp
    WeakRandomNumber.cpp
    WordLock.cpp
    WorkQueue.cpp
    WorkerPool.cpp
    dtoa.cpp

    dragonbox/dragonbox_to_chars.cpp

    dtoa/bignum-dtoa.cc
    dtoa/bignum.cc
    dtoa/cached-powers.cc
    dtoa/diy-fp.cc
    dtoa/double-conversion.cc
    dtoa/fast-dtoa.cc
    dtoa/fixed-dtoa.cc
    dtoa/strtod.cc

    persistence/PersistentCoders.cpp
    persistence/PersistentDecoder.cpp
    persistence/PersistentEncoder.cpp

    text/ASCIILiteral.cpp
    text/AtomString.cpp
    text/AtomStringImpl.cpp
    text/AtomStringTable.cpp
    text/Base64.cpp
    text/CString.cpp
    text/CStringView.cpp
    text/ExternalStringImpl.cpp
    text/LineEnding.cpp
    text/StringBuffer.cpp
    text/StringBuilder.cpp
    text/StringBuilderJSON.cpp
    text/StringCommon.cpp
    text/StringImpl.cpp
    text/StringView.cpp
    text/SymbolImpl.cpp
    text/SymbolRegistry.cpp
    text/TextBreakIterator.cpp
    text/TextStream.cpp
    text/WTFString.cpp

    text/icu/UnicodeExtras.cpp
    text/icu/UTextProvider.cpp
    text/icu/UTextProviderLatin1.cpp
    text/icu/UTextProviderUTF16.cpp

    threads/BinarySemaphore.cpp
    threads/Signals.cpp

    unicode/CollatorDefault.cpp
    unicode/UTF8Conversion.cpp

    unicode/icu/CollatorICU.cpp
    unicode/icu/ICUHelpers.cpp
)

if (WIN32)
    list(APPEND WTF_PUBLIC_HEADERS
        text/win/WCharStringExtras.h

        win/DbgHelperWin.h
        win/GDIObject.h
        win/PathWalker.h
        win/SoftLinking.h
        win/Win32Handle.h
    )
elseif (APPLE)
    list(APPEND WTF_SOURCES
        darwin/OSLogPrintStream.mm
    )

    list(APPEND WTF_PUBLIC_HEADERS
        cf/CFURLExtras.h
        cf/NotificationCenterCF.h
        cf/TypeCastsCF.h
        cf/VectorCF.h

        cocoa/CrashReporter.h
        cocoa/Entitlements.h
        cocoa/NSURLExtras.h
        cocoa/RuntimeApplicationChecksCocoa.h
        cocoa/SoftLinking.h
        cocoa/TollFreeBridging.h
        cocoa/TypeCastsCocoa.h
        cocoa/VectorCocoa.h

        darwin/OSLogPrintStream.h
        darwin/WeakLinking.h
        darwin/XPCExtras.h

        spi/cf/CFBundleSPI.h
        spi/cf/CFStringSPI.h

        spi/cocoa/CFXPCBridgeSPI.h
        spi/cocoa/CrashReporterClientSPI.h
        spi/cocoa/IOSurfaceSPI.h
        spi/cocoa/MachVMSPI.h
        spi/cocoa/NSLocaleSPI.h
        spi/cocoa/NSObjCRuntimeSPI.h
        spi/cocoa/SecuritySPI.h
        spi/cocoa/objcSPI.h

        spi/darwin/ReasonSPI.h
        spi/darwin/CodeSignSPI.h
        spi/darwin/DataVaultSPI.h
        spi/darwin/OSVariantSPI.h
        spi/darwin/ProcessMemoryFootprint.h
        spi/darwin/SandboxSPI.h
        spi/darwin/XPCSPI.h
        spi/darwin/dyldSPI.h

        spi/mac/MetadataSPI.h

        text/cf/StringConcatenateCF.h
        text/cf/TextBreakIteratorCF.h
    )
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    list(APPEND WTF_PUBLIC_HEADERS
        linux/CurrentProcessMemoryStatus.h
        linux/ProcessMemoryFootprint.h
    )
endif ()

set(WTF_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${WTF_DERIVED_SOURCES_DIR}"
    "${WTF_DIR}"
    "${WTF_DIR}/wtf"
    "${WTF_DIR}/wtf/dtoa"
    "${WTF_DIR}/wtf/fast_float"
    "${WTF_DIR}/wtf/persistence"
    "${WTF_DIR}/wtf/simdutf"
    "${WTF_DIR}/wtf/text"
    "${WTF_DIR}/wtf/text/icu"
    "${WTF_DIR}/wtf/threads"
    "${WTF_DIR}/wtf/unicode"
)

set(WTF_LIBRARIES
    ${CMAKE_DL_LIBS}
    ICU::data
    ICU::i18n
    ICU::uc
)

if (NOT USE_SYSTEM_MALLOC)
    set(WTF_FRAMEWORKS bmalloc)
endif ()

if (ATOMICS_REQUIRE_LIBATOMIC)
    list(APPEND WTF_LIBRARIES atomic)
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    if (HAVE_STD_EXPERIMENTAL_FILESYSTEM)
        list(APPEND WTF_LIBRARIES stdc++fs)
    endif ()
endif ()

set(WTF_INTERFACE_LIBRARIES WTF)
set(WTF_INTERFACE_INCLUDE_DIRECTORIES ${WTF_FRAMEWORK_HEADERS_DIR})
set(WTF_INTERFACE_DEPENDENCIES WTF_CopyHeaders)

WEBKIT_FRAMEWORK_DECLARE(WTF)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

if (PORT STREQUAL GTK OR PORT STREQUAL WPE)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WTF
        -Wunsafe-buffer-usage
        -Wunsafe-buffer-usage-in-libc-call
        -fsafe-buffer-usage-suggestions
    )
endif ()

WEBKIT_COPY_FILES(WTF_CopyHeaders
    DESTINATION ${WTF_FRAMEWORK_HEADERS_DIR}/wtf
    FILES ${WTF_PUBLIC_HEADERS}
)

WEBKIT_FRAMEWORK(WTF)

if (MSVC)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
else ()
    # Prior to GCC 13, GCC has a bug that warning suppression pragma cannot be effective on preprocessor itself.
    # We need to disable preprocessor related warnings manually here.
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431
    set_source_files_properties(
        SIMDUTF.cpp
        text/Base64.cpp
        PROPERTIES COMPILE_FLAGS "-Wno-error=undef -Wno-undef")
endif ()

file(MAKE_DIRECTORY ${WTF_SCRIPTS_DIR})

message(STATUS "Copying generate-unified-source-bundles.rb to: ${WTF_SCRIPTS_DIR}")

file(COPY
    ../Scripts/generate-unified-source-bundles.rb
    DESTINATION ${WTF_SCRIPTS_DIR}
)

set(WTF_PREFERENCES
    ../Scripts/GeneratePreferences.rb
    ../Scripts/Preferences/UnifiedWebPreferences.yaml
)

WEBKIT_COPY_FILES(WTF_CopyPreferences
    DESTINATION ${WTF_SCRIPTS_DIR}
    FILES ${WTF_PREFERENCES}
)
