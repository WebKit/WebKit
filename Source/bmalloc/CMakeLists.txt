set_property(DIRECTORY . PROPERTY FOLDER "bmalloc")

set(bmalloc_PRIVATE_INCLUDE_DIRECTORIES
    "${BMALLOC_DIR}"
)

set(bmalloc_SOURCES
    bmalloc/AllIsoHeaps.cpp
    bmalloc/Allocator.cpp
    bmalloc/AvailableMemory.cpp
    bmalloc/Cache.cpp
    bmalloc/CryptoRandom.cpp
    bmalloc/Deallocator.cpp
    bmalloc/DebugHeap.cpp
    bmalloc/Environment.cpp
    bmalloc/FreeList.cpp
    bmalloc/Gigacage.cpp
    bmalloc/Heap.cpp
    bmalloc/HeapConstants.cpp
    bmalloc/HeapKind.cpp
    bmalloc/IsoHeapImpl.cpp
    bmalloc/IsoPage.cpp
    bmalloc/IsoSharedHeap.cpp
    bmalloc/IsoSharedPage.cpp
    bmalloc/IsoTLS.cpp
    bmalloc/IsoTLSEntry.cpp
    bmalloc/IsoTLSLayout.cpp
    bmalloc/LargeMap.cpp
    bmalloc/Logging.cpp
    bmalloc/Mutex.cpp
    bmalloc/ObjectType.cpp
    bmalloc/ObjectTypeTable.cpp
    bmalloc/PerProcess.cpp
    bmalloc/Scavenger.cpp
    bmalloc/bmalloc.cpp
)

set(bmalloc_PUBLIC_HEADERS
    bmalloc/Algorithm.h
    bmalloc/AllIsoHeaps.h
    bmalloc/AllIsoHeapsInlines.h
    bmalloc/Allocator.h
    bmalloc/AvailableMemory.h
    bmalloc/BAssert.h
    bmalloc/BCompiler.h
    bmalloc/BExport.h
    bmalloc/BInline.h
    bmalloc/BMalloced.h
    bmalloc/BPlatform.h
    bmalloc/BVMTags.h
    bmalloc/Bits.h
    bmalloc/BulkDecommit.h
    bmalloc/BumpAllocator.h
    bmalloc/BumpRange.h
    bmalloc/Cache.h
    bmalloc/Chunk.h
    bmalloc/CryptoRandom.h
    bmalloc/Deallocator.h
    bmalloc/DebugHeap.h
    bmalloc/DeferredDecommit.h
    bmalloc/DeferredDecommitInlines.h
    bmalloc/DeferredTrigger.h
    bmalloc/DeferredTriggerInlines.h
    bmalloc/EligibilityResult.h
    bmalloc/EligibilityResultInlines.h
    bmalloc/Environment.h
    bmalloc/FailureAction.h
    bmalloc/FixedVector.h
    bmalloc/FreeList.h
    bmalloc/FreeListInlines.h
    bmalloc/Gigacage.h
    bmalloc/GigacageConfig.h
    bmalloc/GigacageKind.h
    bmalloc/Heap.h
    bmalloc/HeapConstants.h
    bmalloc/HeapKind.h
    bmalloc/IsoAllocator.h
    bmalloc/IsoAllocatorInlines.h
    bmalloc/IsoConfig.h
    bmalloc/IsoDeallocator.h
    bmalloc/IsoDeallocatorInlines.h
    bmalloc/IsoDirectory.h
    bmalloc/IsoDirectoryInlines.h
    bmalloc/IsoDirectoryPage.h
    bmalloc/IsoDirectoryPageInlines.h
    bmalloc/IsoHeap.h
    bmalloc/IsoHeapImpl.h
    bmalloc/IsoHeapImplInlines.h
    bmalloc/IsoHeapInlines.h
    bmalloc/IsoPage.h
    bmalloc/IsoPageInlines.h
    bmalloc/IsoPageTrigger.h
    bmalloc/IsoSharedConfig.h
    bmalloc/IsoSharedHeap.h
    bmalloc/IsoSharedHeapInlines.h
    bmalloc/IsoSharedPage.h
    bmalloc/IsoSharedPageInlines.h
    bmalloc/IsoTLS.h
    bmalloc/IsoTLSAllocatorEntry.h
    bmalloc/IsoTLSAllocatorEntryInlines.h
    bmalloc/IsoTLSDeallocatorEntry.h
    bmalloc/IsoTLSDeallocatorEntryInlines.h
    bmalloc/IsoTLSEntry.h
    bmalloc/IsoTLSEntryInlines.h
    bmalloc/IsoTLSInlines.h
    bmalloc/IsoTLSLayout.h
    bmalloc/LargeMap.h
    bmalloc/LargeRange.h
    bmalloc/LineMetadata.h
    bmalloc/List.h
    bmalloc/Logging.h
    bmalloc/Map.h
    bmalloc/Mutex.h
    bmalloc/Object.h
    bmalloc/ObjectType.h
    bmalloc/ObjectTypeTable.h
    bmalloc/Packed.h
    bmalloc/PerHeapKind.h
    bmalloc/PerProcess.h
    bmalloc/PerThread.h
    bmalloc/PhysicalPageMap.h
    bmalloc/ProcessCheck.h
    bmalloc/Range.h
    bmalloc/Scavenger.h
    bmalloc/ScopeExit.h
    bmalloc/Sizes.h
    bmalloc/SmallLine.h
    bmalloc/SmallPage.h
    bmalloc/StaticPerProcess.h
    bmalloc/StdLibExtras.h
    bmalloc/Syscall.h
    bmalloc/VMAllocate.h
    bmalloc/Vector.h
    bmalloc/Zone.h
    bmalloc/bmalloc.h
)

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    list(APPEND bmalloc_SOURCES
        bmalloc/Zone.cpp
    )
endif ()

set(bmalloc_LIBRARIES
    ${CMAKE_DL_LIBS}
)

set(bmalloc_INTERFACE_LIBRARIES bmalloc)
set(bmalloc_INTERFACE_INCLUDE_DIRECTORIES ${bmalloc_FRAMEWORK_HEADERS_DIR})
set(bmalloc_INTERFACE_DEPENDENCIES bmalloc_CopyHeaders)

WEBKIT_FRAMEWORK_DECLARE(bmalloc)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_COPY_FILES(bmalloc_CopyHeaders
    DESTINATION ${bmalloc_FRAMEWORK_HEADERS_DIR}
    FILES ${bmalloc_PUBLIC_HEADERS}
)

WEBKIT_WRAP_SOURCELIST(${bmalloc_SOURCES})
WEBKIT_FRAMEWORK(bmalloc)
WEBKIT_FRAMEWORK_TARGET(bmalloc)

# Only build mbmalloc on platforms that MallocBench supports
if (DEVELOPER_MODE AND (APPLE OR HAVE_MALLOC_TRIM))
    add_library(mbmalloc SHARED bmalloc/mbmalloc.cpp)
    target_include_directories(mbmalloc PRIVATE ${bmalloc_PRIVATE_INCLUDE_DIRECTORIES})
    target_link_libraries(mbmalloc Threads::Threads bmalloc)
    set_target_properties(mbmalloc PROPERTIES COMPILE_DEFINITIONS "BUILDING_mbmalloc")
endif ()
