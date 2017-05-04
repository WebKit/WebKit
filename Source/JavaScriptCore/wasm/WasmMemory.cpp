/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WasmMemory.h"

#if ENABLE(WEBASSEMBLY)

#include "VM.h"
#include "WasmThunks.h"

#include <atomic>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Platform.h>
#include <wtf/PrintStream.h>
#include <wtf/VMTags.h>

namespace JSC { namespace Wasm {

// FIXME: We could be smarter about memset / mmap / madvise. https://bugs.webkit.org/show_bug.cgi?id=170343
// FIXME: Give up some of the cached fast memories if the GC determines it's easy to get them back, and they haven't been used in a while. https://bugs.webkit.org/show_bug.cgi?id=170773
// FIXME: Limit slow memory size. https://bugs.webkit.org/show_bug.cgi?id=170825

namespace {
constexpr bool verbose = false;

NEVER_INLINE NO_RETURN_DUE_TO_CRASH void webAssemblyCouldntGetFastMemory() { CRASH(); }
NEVER_INLINE NO_RETURN_DUE_TO_CRASH void webAssemblyCouldntUnmapMemory() { CRASH(); }
NEVER_INLINE NO_RETURN_DUE_TO_CRASH void webAssemblyCouldntUnprotectMemory() { CRASH(); }

void* mmapBytes(size_t bytes)
{
#if OS(DARWIN)
    int fd = VM_TAG_FOR_WEBASSEMBLY_MEMORY;
#else
    int fd = -1;
#endif

    void* location = mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANON, fd, 0);
    return location == MAP_FAILED ? nullptr : location;
}

void munmapBytes(void* memory, size_t size)
{
    if (UNLIKELY(munmap(memory, size)))
        webAssemblyCouldntUnmapMemory();
}

void zeroAndUnprotectBytes(void* start, size_t bytes)
{
    if (bytes) {
        dataLogLnIf(verbose, "Zeroing and unprotecting ", bytes, " from ", RawPointer(start));
        // FIXME: We could be smarter about memset / mmap / madvise. Here, we may not need to act synchronously, or maybe we can memset+unprotect smaller ranges of memory (which would pay off if not all the writable memory was actually physically backed: memset forces physical backing only to unprotect it right after). https://bugs.webkit.org/show_bug.cgi?id=170343
        memset(start, 0, bytes);
        if (UNLIKELY(mprotect(start, bytes, PROT_NONE)))
            webAssemblyCouldntUnprotectMemory();
    }
}

// Allocate fast memories very early at program startup and cache them. The fast memories use significant amounts of virtual uncommitted address space, reducing the likelihood that we'll obtain any if we wait to allocate them.
// We still try to allocate fast memories at runtime, and will cache them when relinquished up to the preallocation limit.
// Note that this state is per-process, not per-VM.
// We use simple static globals which don't allocate to avoid early fragmentation and to keep management to the bare minimum. We avoid locking because fast memories use segfault signal handling to handle out-of-bounds accesses. This requires identifying if the faulting address is in a fast memory range, which should avoid acquiring a lock lest the actual signal was caused by this very code while it already held the lock.
// Speed and contention don't really matter here, but simplicity does. We therefore use straightforward FIFOs for our cache, and linear traversal for the list of currently active fast memories.
constexpr size_t fastMemoryCacheHardLimit { 16 };
constexpr size_t fastMemoryAllocationSoftLimit { 32 }; // Prevents filling up the virtual address space.
static_assert(fastMemoryAllocationSoftLimit >= fastMemoryCacheHardLimit, "The cache shouldn't be bigger than the total number we'll ever allocate");
size_t fastMemoryPreallocateCount { 0 };
std::atomic<void*> fastMemoryCache[fastMemoryCacheHardLimit] = { ATOMIC_VAR_INIT(nullptr) };
std::atomic<void*> currentlyActiveFastMemories[fastMemoryAllocationSoftLimit] = { ATOMIC_VAR_INIT(nullptr) };
std::atomic<size_t> currentlyAllocatedFastMemories = ATOMIC_VAR_INIT(0);
std::atomic<size_t> observedMaximumFastMemory = ATOMIC_VAR_INIT(0);
std::atomic<size_t> currentSlowMemoryCapacity = ATOMIC_VAR_INIT(0);

size_t fastMemoryAllocatedBytesSoftLimit()
{
    return fastMemoryAllocationSoftLimit * Memory::fastMappedBytes();
}

void* tryGetCachedFastMemory()
{
    for (unsigned idx = 0; idx < fastMemoryPreallocateCount; ++idx) {
        if (void* previous = fastMemoryCache[idx].exchange(nullptr, std::memory_order_acq_rel))
            return previous;
    }
    return nullptr;
}

bool tryAddToCachedFastMemory(void* memory)
{
    for (unsigned i = 0; i < fastMemoryPreallocateCount; ++i) {
        void* expected = nullptr;
        if (fastMemoryCache[i].compare_exchange_strong(expected, memory, std::memory_order_acq_rel)) {
            dataLogLnIf(verbose, "Cached fast memory ", RawPointer(memory));
            return true;
        }
    }
    return false;
}

bool tryAddToCurrentlyActiveFastMemories(void* memory)
{
    for (size_t idx = 0; idx < fastMemoryAllocationSoftLimit; ++idx) {
        void* expected = nullptr;
        if (currentlyActiveFastMemories[idx].compare_exchange_strong(expected, memory, std::memory_order_acq_rel))
            return true;
    }
    return false;
}

void removeFromCurrentlyActiveFastMemories(void* memory)
{
    for (size_t idx = 0; idx < fastMemoryAllocationSoftLimit; ++idx) {
        void* expected = memory;
        if (currentlyActiveFastMemories[idx].compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel))
            return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void* tryGetFastMemory(VM& vm)
{
    void* memory = nullptr;

    if (LIKELY(Options::useWebAssemblyFastMemory())) {
        memory = tryGetCachedFastMemory();
        if (memory)
            dataLogLnIf(verbose, "tryGetFastMemory re-using ", RawPointer(memory));
        else if (currentlyAllocatedFastMemories.load(std::memory_order_acquire) >= 1) {
            // No memory was available in the cache, but we know there's at least one currently live. Maybe GC will find a free one.
            // FIXME collectSync(Full) and custom eager destruction of wasm memories could be better. For now use collectNow. Also, nothing tells us the current VM is holding onto fast memories. https://bugs.webkit.org/show_bug.cgi?id=170748
            dataLogLnIf(verbose, "tryGetFastMemory waiting on GC and retrying");
            vm.heap.collectNow(Sync, CollectionScope::Full);
            memory = tryGetCachedFastMemory();
            dataLogLnIf(verbose, "tryGetFastMemory waited on GC and retried ", memory? "successfully" : "unseccessfully");
        }

        // The soft limit is inherently racy because checking+allocation isn't atomic. Exceeding it slightly is fine.
        bool atAllocationSoftLimit = currentlyAllocatedFastMemories.load(std::memory_order_acquire) >= fastMemoryAllocationSoftLimit;
        dataLogLnIf(verbose && atAllocationSoftLimit, "tryGetFastMemory reached allocation soft limit of ", fastMemoryAllocationSoftLimit);

        if (!memory && !atAllocationSoftLimit) {
            memory = mmapBytes(Memory::fastMappedBytes());
            if (memory) {
                size_t currentlyAllocated = 1 + currentlyAllocatedFastMemories.fetch_add(1, std::memory_order_acq_rel);
                size_t currentlyObservedMaximum = observedMaximumFastMemory.load(std::memory_order_acquire);
                if (currentlyAllocated > currentlyObservedMaximum) {
                    size_t expected = currentlyObservedMaximum;
                    bool success = observedMaximumFastMemory.compare_exchange_strong(expected, currentlyAllocated, std::memory_order_acq_rel);
                    if (success)
                        dataLogLnIf(verbose, "tryGetFastMemory currently observed maximum is now ", currentlyAllocated);
                    else
                        // We lost the update race, but the counter is monotonic so the winner must have updated the value to what we were going to update it to, or multiple winners did so.
                        ASSERT(expected >= currentlyAllocated);
                }
                dataLogLnIf(verbose, "tryGetFastMemory allocated ", RawPointer(memory), ", currently allocated is ", currentlyAllocated);
            }
        }
    }

    if (memory) {
        if (UNLIKELY(!tryAddToCurrentlyActiveFastMemories(memory))) {
            // We got a memory, but reached the allocation soft limit *and* all of the allocated memories are active, none are cached. That's a bummer, we have to get rid of our memory. We can't just hold on to it because the list of active fast memories must be precise.
            dataLogLnIf(verbose, "tryGetFastMemory found a fast memory but had to give it up");
            munmapBytes(memory, Memory::fastMappedBytes());
            currentlyAllocatedFastMemories.fetch_sub(1, std::memory_order_acq_rel);
            memory = nullptr;
        }
    }

    if (!memory) {
        dataLogLnIf(verbose, "tryGetFastMemory couldn't re-use or allocate a fast memory");
        if (UNLIKELY(Options::crashIfWebAssemblyCantFastMemory()))
            webAssemblyCouldntGetFastMemory();
    }

    return memory;
}

bool slowMemoryCapacitySoftMaximumExceeded()
{
    // The limit on slow memory capacity is arbitrary. Its purpose is to limit
    // virtual memory allocation. We choose to set the limit at the same virtual
    // memory limit imposed on fast memories.
    size_t maximum = fastMemoryAllocatedBytesSoftLimit();
    size_t currentCapacity = currentSlowMemoryCapacity.load(std::memory_order_acquire);
    if (UNLIKELY(currentCapacity > maximum)) {
        dataLogLnIf(verbose, "Slow memory capacity limit reached");
        return true;
    }
    return false;
}

void* tryGetSlowMemory(size_t bytes)
{
    if (slowMemoryCapacitySoftMaximumExceeded())
        return nullptr;
    void* memory = mmapBytes(bytes);
    if (memory)
        currentSlowMemoryCapacity.fetch_add(bytes, std::memory_order_acq_rel);
    dataLogLnIf(memory && verbose, "Obtained slow memory ", RawPointer(memory), " with capacity ", bytes);
    dataLogLnIf(!memory && verbose, "Failed obtaining slow memory with capacity ", bytes);
    return memory;
}

void relinquishMemory(void* memory, size_t writableSize, size_t mappedCapacity, MemoryMode mode)
{
    switch (mode) {
    case MemoryMode::Signaling: {
        RELEASE_ASSERT(Options::useWebAssemblyFastMemory());
        RELEASE_ASSERT(mappedCapacity == Memory::fastMappedBytes());

        // This memory cannot cause a trap anymore.
        removeFromCurrentlyActiveFastMemories(memory);

        // We may cache fast memories. Assuming we will, we have to reset them before inserting them into the cache.
        zeroAndUnprotectBytes(memory, writableSize);

        if (tryAddToCachedFastMemory(memory))
            return;

        dataLogLnIf(verbose, "relinquishMemory unable to cache fast memory, freeing instead ", RawPointer(memory));
        munmapBytes(memory, Memory::fastMappedBytes());
        currentlyAllocatedFastMemories.fetch_sub(1, std::memory_order_acq_rel);

        return;
    }

    case MemoryMode::BoundsChecking:
        dataLogLnIf(verbose, "relinquishFastMemory freeing slow memory ", RawPointer(memory));
        munmapBytes(memory, mappedCapacity);
        currentSlowMemoryCapacity.fetch_sub(mappedCapacity, std::memory_order_acq_rel);
        return;

    case MemoryMode::NumberOfMemoryModes:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

bool makeNewMemoryReadWriteOrRelinquish(void* memory, size_t initialBytes, size_t mappedCapacityBytes, MemoryMode mode)
{
    ASSERT(memory && initialBytes <= mappedCapacityBytes);
    if (initialBytes) {
        dataLogLnIf(verbose, "Marking WebAssembly memory's ", RawPointer(memory), "'s initial ", initialBytes, " bytes as read+write");
        if (mprotect(memory, initialBytes, PROT_READ | PROT_WRITE)) {
            const char* why = strerror(errno);
            dataLogLnIf(verbose, "Failed making memory ", RawPointer(memory), " readable and writable: ", why);
            relinquishMemory(memory, 0, mappedCapacityBytes, mode);
            return false;
        }
    }
    return true;
}

} // anonymous namespace


const char* makeString(MemoryMode mode)
{
    switch (mode) {
    case MemoryMode::BoundsChecking: return "BoundsChecking";
    case MemoryMode::Signaling: return "Signaling";
    case MemoryMode::NumberOfMemoryModes: break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return "";
}

void Memory::initializePreallocations()
{
    if (UNLIKELY(!Options::useWebAssemblyFastMemory()))
        return;

    // Races cannot occur in this function: it is only called at program initialization, before WebAssembly can be invoked.

    MonotonicTime startTime;
    if (verbose)
        startTime = MonotonicTime::now();

    const size_t desiredFastMemories = std::min<size_t>(Options::webAssemblyFastMemoryPreallocateCount(), fastMemoryCacheHardLimit);

    // Start off trying to allocate fast memories contiguously so they don't fragment each other. This can fail if the address space is otherwise fragmented. In that case, go for smaller contiguous allocations. We'll eventually get individual non-contiguous fast memories allocated, or we'll just be unable to fit a single one at which point we give up.
    auto allocateContiguousFastMemories = [&] (size_t numContiguous) -> bool {
        if (void *memory = mmapBytes(Memory::fastMappedBytes() * numContiguous)) {
            for (size_t subMemory = 0; subMemory < numContiguous; ++subMemory) {
                void* startAddress = reinterpret_cast<char*>(memory) + Memory::fastMappedBytes() * subMemory;
                bool inserted = false;
                for (size_t cacheEntry = 0; cacheEntry < fastMemoryCacheHardLimit; ++cacheEntry) {
                    if (fastMemoryCache[cacheEntry].load(std::memory_order_relaxed) == nullptr) {
                        fastMemoryCache[cacheEntry].store(startAddress, std::memory_order_relaxed);
                        inserted = true;
                        break;
                    }
                }
                RELEASE_ASSERT(inserted);
            }
            return true;
        }
        return false;
    };

    size_t fragments = 0;
    size_t numFastMemories = 0;
    size_t contiguousMemoryAllocationAttempt = desiredFastMemories;
    while (numFastMemories != desiredFastMemories && contiguousMemoryAllocationAttempt != 0) {
        if (allocateContiguousFastMemories(contiguousMemoryAllocationAttempt)) {
            numFastMemories += contiguousMemoryAllocationAttempt;
            contiguousMemoryAllocationAttempt = std::min(contiguousMemoryAllocationAttempt - 1, desiredFastMemories - numFastMemories);
        } else
            --contiguousMemoryAllocationAttempt;
        ++fragments;
    }

    fastMemoryPreallocateCount = numFastMemories;
    currentlyAllocatedFastMemories.store(fastMemoryPreallocateCount, std::memory_order_relaxed);
    observedMaximumFastMemory.store(fastMemoryPreallocateCount, std::memory_order_relaxed);

    if (verbose) {
        MonotonicTime endTime = MonotonicTime::now();

        for (size_t cacheEntry = 0; cacheEntry < fastMemoryPreallocateCount; ++cacheEntry) {
            void* startAddress = fastMemoryCache[cacheEntry].load(std::memory_order_relaxed);
            ASSERT(startAddress);
            dataLogLn("Pre-allocation of WebAssembly fast memory at ", RawPointer(startAddress));
        }

        dataLogLn("Pre-allocated ", fastMemoryPreallocateCount, " WebAssembly fast memories in ", fastMemoryPreallocateCount == 0 ? 0 : fragments, fragments == 1 ? " fragment, took " : " fragments, took ", endTime - startTime);
    }
}

Memory::Memory(PageCount initial, PageCount maximum)
    : m_initial(initial)
    , m_maximum(maximum)
{
    ASSERT(!initial.bytes());
    ASSERT(m_mode == MemoryMode::BoundsChecking);
    dataLogLnIf(verbose, "Memory::Memory allocating ", *this);
}

Memory::Memory(void* memory, PageCount initial, PageCount maximum, size_t mappedCapacity, MemoryMode mode)
    : m_memory(memory)
    , m_size(initial.bytes())
    , m_initial(initial)
    , m_maximum(maximum)
    , m_mappedCapacity(mappedCapacity)
    , m_mode(mode)
{
    dataLogLnIf(verbose, "Memory::Memory allocating ", *this);
}

RefPtr<Memory> Memory::create(VM& vm, PageCount initial, PageCount maximum)
{
    ASSERT(initial);
    RELEASE_ASSERT(!maximum || maximum >= initial); // This should be guaranteed by our caller.

    const size_t initialBytes = initial.bytes();
    const size_t maximumBytes = maximum ? maximum.bytes() : 0;
    size_t mappedCapacityBytes = 0;
    MemoryMode mode;

    // We need to be sure we have a stub prior to running code.
    if (UNLIKELY(!Thunks::singleton().stub(throwExceptionFromWasmThunkGenerator)))
        return nullptr;

    if (maximum && !maximumBytes) {
        // User specified a zero maximum, initial size must also be zero.
        RELEASE_ASSERT(!initialBytes);
        return adoptRef(new Memory(initial, maximum));
    }

    void* memory = nullptr;

    // First try fast memory, because they're fast. Fast memory is suitable for any initial / maximum.
    memory = tryGetFastMemory(vm);
    if (memory) {
        mappedCapacityBytes = Memory::fastMappedBytes();
        mode = MemoryMode::Signaling;
    }

    // If we can't get a fast memory but the user expressed the intent to grow memory up to a certain maximum then we should try to honor that desire. It'll mean that grow is more likely to succeed, and won't require remapping.
    if (!memory && maximum) {
        memory = tryGetSlowMemory(maximumBytes);
        if (memory) {
            mappedCapacityBytes = maximumBytes;
            mode = MemoryMode::BoundsChecking;
        }
    }

    // We're stuck with a slow memory which may be slower or impossible to grow.
    if (!memory) {
        memory = tryGetSlowMemory(initialBytes);
        if (memory) {
            mappedCapacityBytes = initialBytes;
            mode = MemoryMode::BoundsChecking;
        }
    }

    if (!memory)
        return nullptr;

    if (!makeNewMemoryReadWriteOrRelinquish(memory, initialBytes, mappedCapacityBytes, mode))
        return nullptr;

    return adoptRef(new Memory(memory, initial, maximum, mappedCapacityBytes, mode));
}

Memory::~Memory()
{
    if (m_memory) {
        dataLogLnIf(verbose, "Memory::~Memory ", *this);
        relinquishMemory(m_memory, m_size, m_mappedCapacity, m_mode);
    }
}

size_t Memory::fastMappedRedzoneBytes()
{
    return static_cast<size_t>(PageCount::pageSize) * Options::webAssemblyFastMemoryRedzonePages();
}

size_t Memory::fastMappedBytes()
{
    static_assert(sizeof(uint64_t) == sizeof(size_t), "We rely on allowing the maximum size of Memory we map to be 2^32 + redzone which is larger than fits in a 32-bit integer that we'd pass to mprotect if this didn't hold.");
    return static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + fastMappedRedzoneBytes();
}

size_t Memory::maxFastMemoryCount()
{
    // The order can be relaxed here because we provide a monotonically-increasing estimate. A concurrent observer could see a slightly out-of-date value but can't tell that they did.
    return observedMaximumFastMemory.load(std::memory_order_relaxed);
}

bool Memory::addressIsInActiveFastMemory(void* address)
{
    // This cannot race in any meaningful way: the thread which calls this function wants to know if a fault it received at a particular address is in a fast memory. That fast memory must therefore be active in that thread. It cannot be added or removed from the list of currently active fast memories. Other memories being added / removed concurrently are inconsequential.
    for (size_t idx = 0; idx < fastMemoryAllocationSoftLimit; ++idx) {
        char* start = static_cast<char*>(currentlyActiveFastMemories[idx].load(std::memory_order_acquire));
        if (start <= address && address <= start + fastMappedBytes())
            return true;
    }
    return false;
}

bool Memory::grow(PageCount newSize)
{
    RELEASE_ASSERT(newSize > PageCount::fromBytes(m_size));

    dataLogLnIf(verbose, "Memory::grow to ", newSize, " from ", *this);

    if (maximum() && newSize > maximum())
        return false;

    size_t desiredSize = newSize.bytes();

    switch (mode()) {
    case MemoryMode::BoundsChecking:
        RELEASE_ASSERT(maximum().bytes() != 0);
        break;
    case MemoryMode::Signaling:
        // Signaling memory must have been pre-allocated virtually.
        RELEASE_ASSERT(m_memory);
        break;
    case MemoryMode::NumberOfMemoryModes:
        RELEASE_ASSERT_NOT_REACHED();
    }

    if (m_memory && desiredSize <= m_mappedCapacity) {
        uint8_t* startAddress = static_cast<uint8_t*>(m_memory) + m_size;
        size_t extraBytes = desiredSize - m_size;
        RELEASE_ASSERT(extraBytes);
        dataLogLnIf(verbose, "Marking WebAssembly memory's ", RawPointer(m_memory), " as read+write in range [", RawPointer(startAddress), ", ", RawPointer(startAddress + extraBytes), ")");
        if (mprotect(startAddress, extraBytes, PROT_READ | PROT_WRITE)) {
            dataLogLnIf(verbose, "Memory::grow in-place failed ", *this);
            return false;
        }

        m_size = desiredSize;
        dataLogLnIf(verbose, "Memory::grow in-place ", *this);
        return true;
    }

    // Signaling memory can't grow past its already-mapped size.
    RELEASE_ASSERT(mode() != MemoryMode::Signaling);

    // Otherwise, let's try to make some new memory.
    // FIXME mremap would be nice https://bugs.webkit.org/show_bug.cgi?id=170557
    // FIXME should we over-allocate here? https://bugs.webkit.org/show_bug.cgi?id=170826
    void* newMemory = tryGetSlowMemory(desiredSize);
    if (!newMemory)
        return false;

    if (!makeNewMemoryReadWriteOrRelinquish(newMemory, desiredSize, desiredSize, mode()))
        return false;

    if (m_memory) {
        memcpy(newMemory, m_memory, m_size);
        relinquishMemory(m_memory, m_size, m_size, m_mode);
    }

    m_memory = newMemory;
    m_mappedCapacity = desiredSize;
    m_size = desiredSize;

    dataLogLnIf(verbose, "Memory::grow ", *this);
    return true;
}

void Memory::dump(PrintStream& out) const
{
    out.print("Memory at ", RawPointer(m_memory), ", size ", m_size, "B capacity ", m_mappedCapacity, "B, initial ", m_initial, " maximum ", m_maximum, " mode ", makeString(m_mode));
}

} // namespace JSC

} // namespace Wasm

#endif // ENABLE(WEBASSEMBLY)
