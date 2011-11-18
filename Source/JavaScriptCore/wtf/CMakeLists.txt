SET(WTF_HEADERS
    ASCIICType.h
    AVLTree.h
    Alignment.h
    AlwaysInline.h
    Assertions.h
    Atomics.h
    BitVector.h
    Bitmap.h
    BoundsCheckedPointer.h
    BumpPointerAllocator.h
    ByteArray.h
    Compiler.h
    Complex.h
    CryptographicallyRandomNumber.h
    CurrentTime.h
    DateMath.h
    DecimalNumber.h
    Decoder.h
    Deque.h
    DisallowCType.h
    DoublyLinkedList.h
    DynamicAnnotations.h
    Encoder.h
    FastAllocBase.h
    FastMalloc.h
    FixedArray.h
    Forward.h
    GetPtr.h
    HashCountedSet.h
    HashFunctions.h
    HashIterators.h
    HashMap.h
    HashSet.h
    HashTable.h
    HashTraits.h
    HexNumber.h
    ListHashSet.h
    ListRefPtr.h
    Locker.h
    MD5.h
    MainThread.h
    MallocZoneSupport.h
    MathExtras.h
    MessageQueue.h
    MetaAllocator.cpp
    MetaAllocator.h
    MetaAllocatorHandle.h
    NonCopyingSort.h
    ThreadRestrictionVerifier.h
    Noncopyable.h
    NotFound.h
    NullPtr.h
    OSAllocator.h
    OSRandomSource.h
    OwnArrayPtr.h
    OwnFastMallocPtr.h
    OwnPtr.h
    OwnPtrCommon.h
    PageAllocation.h
    PageAllocationAligned.h
    PageBlock.h
    PageReservation.h
    PassOwnArrayPtr.h
    PassOwnPtr.h
    PassRefPtr.h
    PassTraits.h
    ParallelJobs.h
    ParallelJobsGeneric.h
    ParallelJobsLibdispatch.h
    ParallelJobsOpenMP.h
    Platform.h
    PossiblyNull.h
    RandomNumber.h
    RandomNumberSeed.h
    RedBlackTree.h
    RefCounted.h
    RefCountedLeakCounter.h
    RefPtr.h
    RefPtrHashMap.h
    RetainPtr.h
    SegmentedVector.h
    SHA1.h
    StackBounds.h
    StaticConstructors.h
    StdLibExtras.h
    StringExtras.h
    StringHasher.h
    TCPackedCache.h
    TCPageMap.h
    TCSpinLock.h
    TCSystemAlloc.h
    ThreadIdentifierDataPthreads.h
    ThreadSafeRefCounted.h
    ThreadSpecific.h
    Threading.h
    ThreadingPrimitives.h
    TypeTraits.h
    UnusedParam.h
    VMTags.h
    ValueCheck.h
    Vector.h
    VectorTraits.h
    WTFThreadData.h
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
    text/CString.h
    text/StringBuffer.h
    text/StringHash.h
    text/StringImpl.h
    text/WTFString.h

    threads/BinarySemaphore.h

    unicode/CharacterNames.h
    unicode/Collator.h
    unicode/UTF8.h
    unicode/Unicode.h
)

SET(WTF_SOURCES
    Assertions.cpp
    BitVector.cpp
    ByteArray.cpp
    CryptographicallyRandomNumber.cpp
    CurrentTime.cpp
    DateMath.cpp
    DecimalNumber.cpp
    DynamicAnnotations.cpp
    FastMalloc.cpp
    HashTable.cpp
    MainThread.cpp
    MD5.cpp
    OSRandomSource.cpp
    PageAllocationAligned.cpp
    PageBlock.cpp
    ParallelJobsGeneric.cpp
    RandomNumber.cpp
    RefCountedLeakCounter.cpp
    SHA1.cpp
    StackBounds.cpp
    StringExtras.cpp
    Threading.cpp
    TypeTraits.cpp
    WTFThreadData.cpp
    dtoa.cpp

    dtoa/bignum-dtoa.cc
    dtoa/bignum.cc
    dtoa/cached-powers.cc
    dtoa/diy-fp.cc
    dtoa/double-conversion.cc
    dtoa/fast-dtoa.cc
    dtoa/fixed-dtoa.cc
    dtoa/strtod.cc

    text/AtomicString.cpp
    text/CString.cpp
    text/StringBuilder.cpp
    text/StringImpl.cpp
    text/StringStatics.cpp
    text/WTFString.cpp

    threads/BinarySemaphore.cpp

    unicode/UTF8.cpp
)

SET(WTF_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}"
    "${JAVASCRIPTCORE_DIR}/wtf"
    "${JAVASCRIPTCORE_DIR}/wtf/unicode"
    "${JAVASCRIPTCORE_DIR}/wtf/dtoa"
    "${JavaScriptCore_INCLUDE_DIRECTORIES}"
)

SET(WTF_LIBRARIES
)


IF (ENABLE_FAST_MALLOC)
    LIST(APPEND WTF_SOURCES
        TCSystemAlloc.cpp
    )
ELSE ()
    ADD_DEFINITIONS(-DUSE_SYSTEM_MALLOC=1)
ENDIF()


SET(WTF_PORT_FLAGS )
INCLUDE_IF_EXISTS(${JAVASCRIPTCORE_DIR}/wtf/Platform${PORT}.cmake)

LIST(APPEND WTF_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}/ThirdParty"
)

WEBKIT_WRAP_SOURCELIST(${WTF_SOURCES})
INCLUDE_DIRECTORIES(${WTF_INCLUDE_DIRECTORIES})
ADD_DEFINITIONS(-DBUILDING_WTF)
ADD_LIBRARY(${WTF_LIBRARY_NAME} ${WTF_LIBRARY_TYPE} ${WTF_HEADERS} ${WTF_SOURCES})
TARGET_LINK_LIBRARIES(${WTF_LIBRARY_NAME} ${WTF_LIBRARIES})

IF (WTF_LINK_FLAGS)
    ADD_TARGET_PROPERTIES(${WTF_LIBRARY_NAME} LINK_FLAGS "${WTF_LINK_FLAGS}")
ENDIF ()

IF (SHARED_CORE)
    SET_TARGET_PROPERTIES(${WTF_LIBRARY_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
    INSTALL(TARGETS ${WTF_LIBRARY_NAME} DESTINATION lib)
ENDIF ()
