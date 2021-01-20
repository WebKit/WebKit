/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "WasmInstance.h"

#if ENABLE(WEBASSEMBLY)

#include "Options.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/DataLog.h>
#include <wtf/Gigacage.h>
#include <wtf/Lock.h>
#include <wtf/Platform.h>
#include <wtf/PrintStream.h>
#include <wtf/RAMSize.h>
#include <wtf/StdSet.h>
#include <wtf/Vector.h>

#include <cstring>
#include <mutex>

namespace JSC { namespace Wasm {

// FIXME: We could be smarter about memset / mmap / madvise. https://bugs.webkit.org/show_bug.cgi?id=170343
// FIXME: Give up some of the cached fast memories if the GC determines it's easy to get them back, and they haven't been used in a while. https://bugs.webkit.org/show_bug.cgi?id=170773
// FIXME: Limit slow memory size. https://bugs.webkit.org/show_bug.cgi?id=170825

namespace {

constexpr bool verbose = false;

NEVER_INLINE NO_RETURN_DUE_TO_CRASH void webAssemblyCouldntGetFastMemory() { CRASH(); }

struct MemoryResult {
    enum Kind {
        Success,
        SuccessAndNotifyMemoryPressure,
        SyncTryToReclaimMemory
    };

    static const char* toString(Kind kind)
    {
        switch (kind) {
        case Success:
            return "Success";
        case SuccessAndNotifyMemoryPressure:
            return "SuccessAndNotifyMemoryPressure";
        case SyncTryToReclaimMemory:
            return "SyncTryToReclaimMemory";
        }
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
    
    MemoryResult() { }
    
    MemoryResult(void* basePtr, Kind kind)
        : basePtr(basePtr)
        , kind(kind)
    {
    }
    
    void dump(PrintStream& out) const
    {
        out.print("{basePtr = ", RawPointer(basePtr), ", kind = ", toString(kind), "}");
    }
    
    void* basePtr;
    Kind kind;
};

class MemoryManager {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(MemoryManager);
public:
    MemoryManager()
        : m_maxFastMemoryCount(Options::maxNumWebAssemblyFastMemories())
    {
    }
    
    MemoryResult tryAllocateFastMemory()
    {
        MemoryResult result = [&] {
            auto holder = holdLock(m_lock);
            if (m_fastMemories.size() >= m_maxFastMemoryCount)
                return MemoryResult(nullptr, MemoryResult::SyncTryToReclaimMemory);
            
            void* result = Gigacage::tryAllocateZeroedVirtualPages(Gigacage::Primitive, Memory::fastMappedBytes());
            if (!result)
                return MemoryResult(nullptr, MemoryResult::SyncTryToReclaimMemory);
            
            m_fastMemories.append(result);
            
            return MemoryResult(
                result,
                m_fastMemories.size() >= m_maxFastMemoryCount / 2 ? MemoryResult::SuccessAndNotifyMemoryPressure : MemoryResult::Success);
        }();
        
        dataLogLnIf(Options::logWebAssemblyMemory(), "Allocated virtual: ", result, "; state: ", *this);
        
        return result;
    }
    
    void freeFastMemory(void* basePtr)
    {
        {
            auto holder = holdLock(m_lock);
            Gigacage::freeVirtualPages(Gigacage::Primitive, basePtr, Memory::fastMappedBytes());
            m_fastMemories.removeFirst(basePtr);
        }
        
        dataLogLnIf(Options::logWebAssemblyMemory(), "Freed virtual; state: ", *this);
    }

    MemoryResult tryAllocateGrowableBoundsCheckingMemory(size_t mappedCapacity)
    {
        MemoryResult result = [&] {
            auto holder = holdLock(m_lock);
            void* result = Gigacage::tryAllocateZeroedVirtualPages(Gigacage::Primitive, mappedCapacity);
            if (!result)
                return MemoryResult(nullptr, MemoryResult::SyncTryToReclaimMemory);

            m_growableBoundsCheckingMemories.insert(std::make_pair(bitwise_cast<uintptr_t>(result), mappedCapacity));

            return MemoryResult(result, MemoryResult::Success);
        }();

        dataLogLnIf(Options::logWebAssemblyMemory(), "Allocated virtual: ", result, "; state: ", *this);

        return result;
    }

    void freeGrowableBoundsCheckingMemory(void* basePtr, size_t mappedCapacity)
    {
        {
            auto holder = holdLock(m_lock);
            Gigacage::freeVirtualPages(Gigacage::Primitive, basePtr, mappedCapacity);
            m_growableBoundsCheckingMemories.erase(std::make_pair(bitwise_cast<uintptr_t>(basePtr), mappedCapacity));
        }

        dataLogLnIf(Options::logWebAssemblyMemory(), "Freed virtual; state: ", *this);
    }

    bool isInGrowableOrFastMemory(void* address)
    {
        // NOTE: This can be called from a signal handler, but only after we proved that we're in JIT code or WasmLLInt code.
        auto holder = holdLock(m_lock);
        for (void* memory : m_fastMemories) {
            char* start = static_cast<char*>(memory);
            if (start <= address && address <= start + Memory::fastMappedBytes())
                return true;
        }
        uintptr_t addressValue = bitwise_cast<uintptr_t>(address);
        auto iterator = std::upper_bound(m_growableBoundsCheckingMemories.begin(), m_growableBoundsCheckingMemories.end(), std::make_pair(addressValue, 0),
            [](std::pair<uintptr_t, size_t> a, std::pair<uintptr_t, size_t> b) {
                return (a.first + a.second) < (b.first + b.second);
            });
        if (iterator != m_growableBoundsCheckingMemories.end()) {
            // Since we never have overlapped range in m_growableBoundsCheckingMemories, just checking one lower-bound range is enough.
            if (iterator->first <= addressValue && addressValue < (iterator->first + iterator->second))
                return true;
        }
        return false;
    }

    // We allow people to "commit" more wasm memory than there is on the system since most of the time
    // people don't actually write to most of that memory. There is some chance that this gets us
    // JetSammed but that's possible anyway.
    inline size_t memoryLimit() const { return ramSize() * 3; }

    // FIXME: Ideally, bmalloc would have this kind of mechanism. Then, we would just forward to that
    // mechanism here.
    MemoryResult::Kind tryAllocatePhysicalBytes(size_t bytes)
    {
        MemoryResult::Kind result = [&] {
            auto holder = holdLock(m_lock);
            if (m_physicalBytes + bytes > memoryLimit())
                return MemoryResult::SyncTryToReclaimMemory;
            
            m_physicalBytes += bytes;
            
            if (m_physicalBytes >= memoryLimit() / 2)
                return MemoryResult::SuccessAndNotifyMemoryPressure;
            
            return MemoryResult::Success;
        }();
        
        dataLogLnIf(Options::logWebAssemblyMemory(), "Allocated physical: ", bytes, ", ", MemoryResult::toString(result), "; state: ", *this);
        
        return result;
    }
    
    void freePhysicalBytes(size_t bytes)
    {
        {
            auto holder = holdLock(m_lock);
            m_physicalBytes -= bytes;
        }
        
        dataLogLnIf(Options::logWebAssemblyMemory(), "Freed physical: ", bytes, "; state: ", *this);
    }
    
    void dump(PrintStream& out) const
    {
        out.print("fast memories =  ", m_fastMemories.size(), "/", m_maxFastMemoryCount, ", bytes = ", m_physicalBytes, "/", memoryLimit());
    }
    
private:
    Lock m_lock;
    unsigned m_maxFastMemoryCount { 0 };
    Vector<void*> m_fastMemories;
    StdSet<std::pair<uintptr_t, size_t>> m_growableBoundsCheckingMemories;
    size_t m_physicalBytes { 0 };
};

static MemoryManager& memoryManager()
{
    static std::once_flag onceFlag;
    static MemoryManager* manager;
    std::call_once(
        onceFlag,
        [] {
            manager = new MemoryManager();
        });
    return *manager;
}

template<typename Func>
bool tryAllocate(const Func& allocate, const WTF::Function<void(Memory::NotifyPressure)>& notifyMemoryPressure, const WTF::Function<void(Memory::SyncTryToReclaim)>& syncTryToReclaimMemory)
{
    unsigned numTries = 2;
    bool done = false;
    for (unsigned i = 0; i < numTries && !done; ++i) {
        switch (allocate()) {
        case MemoryResult::Success:
            done = true;
            break;
        case MemoryResult::SuccessAndNotifyMemoryPressure:
            if (notifyMemoryPressure)
                notifyMemoryPressure(Memory::NotifyPressureTag);
            done = true;
            break;
        case MemoryResult::SyncTryToReclaimMemory:
            if (i + 1 == numTries)
                break;
            if (syncTryToReclaimMemory)
                syncTryToReclaimMemory(Memory::SyncTryToReclaimTag);
            break;
        }
    }
    return done;
}

} // anonymous namespace


MemoryHandle::MemoryHandle(void* memory, size_t size, size_t mappedCapacity, PageCount initial, PageCount maximum, MemorySharingMode sharingMode, MemoryMode mode)
    : m_memory(memory, mappedCapacity)
    , m_size(size)
    , m_mappedCapacity(mappedCapacity)
    , m_initial(initial)
    , m_maximum(maximum)
    , m_sharingMode(sharingMode)
    , m_mode(mode)
{
#if ASSERT_ENABLED
    if (sharingMode == MemorySharingMode::Default && mode == MemoryMode::BoundsChecking)
        ASSERT(mappedCapacity == size);
#endif
}

MemoryHandle::~MemoryHandle()
{
    if (m_memory) {
        void* memory = this->memory();
        memoryManager().freePhysicalBytes(m_size);
        switch (m_mode) {
        case MemoryMode::Signaling:
            if (mprotect(memory, Memory::fastMappedBytes(), PROT_READ | PROT_WRITE)) {
                dataLog("mprotect failed: ", strerror(errno), "\n");
                RELEASE_ASSERT_NOT_REACHED();
            }
            memoryManager().freeFastMemory(memory);
            break;
        case MemoryMode::BoundsChecking: {
            switch (m_sharingMode) {
            case MemorySharingMode::Default:
                Gigacage::freeVirtualPages(Gigacage::Primitive, memory, m_size);
                break;
            case MemorySharingMode::Shared: {
                if (mprotect(memory, m_mappedCapacity, PROT_READ | PROT_WRITE)) {
                    dataLog("mprotect failed: ", strerror(errno), "\n");
                    RELEASE_ASSERT_NOT_REACHED();
                }
                memoryManager().freeGrowableBoundsCheckingMemory(memory, m_mappedCapacity);
                break;
            }
            }
            break;
        }
        }
    }
}

// FIXME: ARM64E clang has a bug and inlining this function makes optimizer run forever.
// For now, putting NEVER_INLINE to suppress inlining of this.
NEVER_INLINE void* MemoryHandle::memory() const
{
    ASSERT(m_memory.getMayBeNull(m_mappedCapacity) == m_memory.getUnsafe());
    return m_memory.getMayBeNull(m_mappedCapacity);
}

Memory::Memory()
    : m_handle(adoptRef(*new MemoryHandle(nullptr, 0, 0, PageCount(0), PageCount(0), MemorySharingMode::Default, MemoryMode::BoundsChecking)))
{
}

Memory::Memory(PageCount initial, PageCount maximum, MemorySharingMode sharingMode, Function<void(NotifyPressure)>&& notifyMemoryPressure, Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
    : m_handle(adoptRef(*new MemoryHandle(nullptr, 0, 0, initial, maximum, sharingMode, MemoryMode::BoundsChecking)))
    , m_notifyMemoryPressure(WTFMove(notifyMemoryPressure))
    , m_syncTryToReclaimMemory(WTFMove(syncTryToReclaimMemory))
    , m_growSuccessCallback(WTFMove(growSuccessCallback))
{
    ASSERT(!initial.bytes());
    ASSERT(mode() == MemoryMode::BoundsChecking);
    dataLogLnIf(verbose, "Memory::Memory allocating ", *this);
    ASSERT(!memory());
}

Memory::Memory(Ref<MemoryHandle>&& handle, Function<void(NotifyPressure)>&& notifyMemoryPressure, Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
    : m_handle(WTFMove(handle))
    , m_notifyMemoryPressure(WTFMove(notifyMemoryPressure))
    , m_syncTryToReclaimMemory(WTFMove(syncTryToReclaimMemory))
    , m_growSuccessCallback(WTFMove(growSuccessCallback))
{
    dataLogLnIf(verbose, "Memory::Memory allocating ", *this);
}

Ref<Memory> Memory::create()
{
    return adoptRef(*new Memory());
}

Ref<Memory> Memory::create(Ref<MemoryHandle>&& handle, WTF::Function<void(NotifyPressure)>&& notifyMemoryPressure, WTF::Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
{
    return adoptRef(*new Memory(WTFMove(handle), WTFMove(notifyMemoryPressure), WTFMove(syncTryToReclaimMemory), WTFMove(growSuccessCallback)));
}

RefPtr<Memory> Memory::tryCreate(PageCount initial, PageCount maximum, MemorySharingMode sharingMode, WTF::Function<void(NotifyPressure)>&& notifyMemoryPressure, WTF::Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
{
    ASSERT(initial);
    RELEASE_ASSERT(!maximum || maximum >= initial); // This should be guaranteed by our caller.

    const size_t initialBytes = initial.bytes();
    const size_t maximumBytes = maximum ? maximum.bytes() : 0;

    if (initialBytes > MAX_ARRAY_BUFFER_SIZE)
        return nullptr; // Client will throw OOMError.

    if (maximum && !maximumBytes) {
        // User specified a zero maximum, initial size must also be zero.
        RELEASE_ASSERT(!initialBytes);
        return adoptRef(new Memory(initial, maximum, sharingMode, WTFMove(notifyMemoryPressure), WTFMove(syncTryToReclaimMemory), WTFMove(growSuccessCallback)));
    }
    
    bool done = tryAllocate(
        [&] () -> MemoryResult::Kind {
            return memoryManager().tryAllocatePhysicalBytes(initialBytes);
        }, notifyMemoryPressure, syncTryToReclaimMemory);
    if (!done)
        return nullptr;
        
    char* fastMemory = nullptr;
    if (Options::useWebAssemblyFastMemory()) {
        tryAllocate(
            [&] () -> MemoryResult::Kind {
                auto result = memoryManager().tryAllocateFastMemory();
                fastMemory = bitwise_cast<char*>(result.basePtr);
                return result.kind;
            }, notifyMemoryPressure, syncTryToReclaimMemory);
    }
    
    if (fastMemory) {
        if (mprotect(fastMemory + initialBytes, Memory::fastMappedBytes() - initialBytes, PROT_NONE)) {
            dataLog("mprotect failed: ", strerror(errno), "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        return Memory::create(adoptRef(*new MemoryHandle(fastMemory, initialBytes, Memory::fastMappedBytes(), initial, maximum, sharingMode, MemoryMode::Signaling)), WTFMove(notifyMemoryPressure), WTFMove(syncTryToReclaimMemory), WTFMove(growSuccessCallback));
    }
    
    if (UNLIKELY(Options::crashIfWebAssemblyCantFastMemory()))
        webAssemblyCouldntGetFastMemory();

    if (!initialBytes)
        return adoptRef(new Memory(initial, maximum, sharingMode, WTFMove(notifyMemoryPressure), WTFMove(syncTryToReclaimMemory), WTFMove(growSuccessCallback)));

    switch (sharingMode) {
    case MemorySharingMode::Default: {
        void* slowMemory = Gigacage::tryAllocateZeroedVirtualPages(Gigacage::Primitive, initialBytes);
        if (!slowMemory) {
            memoryManager().freePhysicalBytes(initialBytes);
            return nullptr;
        }
        return Memory::create(adoptRef(*new MemoryHandle(slowMemory, initialBytes, initialBytes, initial, maximum, sharingMode, MemoryMode::BoundsChecking)), WTFMove(notifyMemoryPressure), WTFMove(syncTryToReclaimMemory), WTFMove(growSuccessCallback));
    }
    case MemorySharingMode::Shared: {
        char* slowMemory = nullptr;
        tryAllocate(
            [&] () -> MemoryResult::Kind {
                auto result = memoryManager().tryAllocateGrowableBoundsCheckingMemory(maximumBytes);
                slowMemory = bitwise_cast<char*>(result.basePtr);
                return result.kind;
            }, notifyMemoryPressure, syncTryToReclaimMemory);
        if (!slowMemory) {
            memoryManager().freePhysicalBytes(initialBytes);
            return nullptr;
        }

        if (mprotect(slowMemory + initialBytes, maximumBytes - initialBytes, PROT_NONE)) {
            dataLog("mprotect failed: ", strerror(errno), "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        return Memory::create(adoptRef(*new MemoryHandle(slowMemory, initialBytes, maximumBytes, initial, maximum, sharingMode, MemoryMode::BoundsChecking)), WTFMove(notifyMemoryPressure), WTFMove(syncTryToReclaimMemory), WTFMove(growSuccessCallback));
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

Memory::~Memory() = default;

size_t Memory::fastMappedRedzoneBytes()
{
    return static_cast<size_t>(PageCount::pageSize) * Options::webAssemblyFastMemoryRedzonePages();
}

size_t Memory::fastMappedBytes()
{
    static_assert(sizeof(uint64_t) == sizeof(size_t), "We rely on allowing the maximum size of Memory we map to be 2^32 + redzone which is larger than fits in a 32-bit integer that we'd pass to mprotect if this didn't hold.");
    return (static_cast<size_t>(1) << 32) + fastMappedRedzoneBytes();
}

bool Memory::addressIsInGrowableOrFastMemory(void* address)
{
    return memoryManager().isInGrowableOrFastMemory(address);
}

Expected<PageCount, Memory::GrowFailReason> Memory::growShared(PageCount delta)
{
    Wasm::PageCount oldPageCount;
    Wasm::PageCount newPageCount;
    auto result = ([&]() -> Expected<PageCount, Memory::GrowFailReason> {
        auto locker = holdLock(m_handle->lock());

        oldPageCount = sizeInPages();
        newPageCount = oldPageCount + delta;
        if (!newPageCount || !newPageCount.isValid())
            return makeUnexpected(GrowFailReason::InvalidGrowSize);
        if (newPageCount.bytes() > MAX_ARRAY_BUFFER_SIZE)
            return makeUnexpected(GrowFailReason::OutOfMemory);

        if (!delta.pageCount())
            return oldPageCount;

        dataLogLnIf(verbose, "Memory::grow(", delta, ") to ", newPageCount, " from ", *this);
        RELEASE_ASSERT(newPageCount > PageCount::fromBytes(size()));

        if (maximum() && newPageCount > maximum())
            return makeUnexpected(GrowFailReason::WouldExceedMaximum);

        size_t desiredSize = newPageCount.bytes();
        RELEASE_ASSERT(desiredSize <= MAX_ARRAY_BUFFER_SIZE);
        RELEASE_ASSERT(desiredSize > size());

        // If the memory is MemorySharingMode::Shared, we already allocated enough virtual address space even if the memory is bound-checking mode. We perform mprotect to extend.
        size_t extraBytes = desiredSize - size();
        RELEASE_ASSERT(extraBytes);
        bool allocationSuccess = tryAllocate(
            [&] () -> MemoryResult::Kind {
                return memoryManager().tryAllocatePhysicalBytes(extraBytes);
            }, [](Wasm::Memory::NotifyPressure) { }, [](Memory::SyncTryToReclaim) { });
        if (!allocationSuccess)
            return makeUnexpected(GrowFailReason::OutOfMemory);

        void* memory = this->memory();
        RELEASE_ASSERT(memory);

        // Signaling memory must have been pre-allocated virtually.
        uint8_t* startAddress = static_cast<uint8_t*>(memory) + size();

        dataLogLnIf(verbose, "Marking WebAssembly memory's ", RawPointer(memory), " as read+write in range [", RawPointer(startAddress), ", ", RawPointer(startAddress + extraBytes), ")");
        if (mprotect(startAddress, extraBytes, PROT_READ | PROT_WRITE)) {
            dataLog("mprotect failed: ", strerror(errno), "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        m_handle->growToSize(desiredSize);
        return oldPageCount;
    }());
    if (result)
        m_growSuccessCallback(GrowSuccessTag, oldPageCount, newPageCount);
    return result;
}

Expected<PageCount, Memory::GrowFailReason> Memory::grow(PageCount delta)
{
    if (!delta.isValid())
        return makeUnexpected(GrowFailReason::InvalidDelta);

    if (sharingMode() == MemorySharingMode::Shared)
        return growShared(delta);

    const Wasm::PageCount oldPageCount = sizeInPages();
    const Wasm::PageCount newPageCount = oldPageCount + delta;
    if (!newPageCount || !newPageCount.isValid())
        return makeUnexpected(GrowFailReason::InvalidGrowSize);
    if (newPageCount.bytes() > MAX_ARRAY_BUFFER_SIZE)
        return makeUnexpected(GrowFailReason::OutOfMemory);

    auto success = [&] () {
        m_growSuccessCallback(GrowSuccessTag, oldPageCount, newPageCount);
        // Update cache for instance
        for (auto& instance : m_instances) {
            if (instance.get() != nullptr)
                instance.get()->updateCachedMemory();
        }
        return oldPageCount;
    };

    if (delta.pageCount() == 0)
        return success();

    dataLogLnIf(verbose, "Memory::grow(", delta, ") to ", newPageCount, " from ", *this);
    RELEASE_ASSERT(newPageCount > PageCount::fromBytes(size()));

    if (maximum() && newPageCount > maximum())
        return makeUnexpected(GrowFailReason::WouldExceedMaximum);

    size_t desiredSize = newPageCount.bytes();
    RELEASE_ASSERT(desiredSize <= MAX_ARRAY_BUFFER_SIZE);
    RELEASE_ASSERT(desiredSize > size());
    switch (mode()) {
    case MemoryMode::BoundsChecking: {
        bool allocationSuccess = tryAllocate(
            [&] () -> MemoryResult::Kind {
                return memoryManager().tryAllocatePhysicalBytes(desiredSize);
            }, m_notifyMemoryPressure, m_syncTryToReclaimMemory);
        if (!allocationSuccess)
            return makeUnexpected(GrowFailReason::OutOfMemory);

        RELEASE_ASSERT(maximum().bytes() != 0);

        void* newMemory = Gigacage::tryAllocateZeroedVirtualPages(Gigacage::Primitive, desiredSize);
        if (!newMemory)
            return makeUnexpected(GrowFailReason::OutOfMemory);

        memcpy(newMemory, memory(), size());
        auto newHandle = adoptRef(*new MemoryHandle(newMemory, desiredSize, desiredSize, initial(), maximum(), sharingMode(), MemoryMode::BoundsChecking));
        m_handle = WTFMove(newHandle);

        ASSERT(memory() == newMemory);
        return success();
    }
    case MemoryMode::Signaling: {
        size_t extraBytes = desiredSize - size();
        RELEASE_ASSERT(extraBytes);
        bool allocationSuccess = tryAllocate(
            [&] () -> MemoryResult::Kind {
                return memoryManager().tryAllocatePhysicalBytes(extraBytes);
            }, m_notifyMemoryPressure, m_syncTryToReclaimMemory);
        if (!allocationSuccess)
            return makeUnexpected(GrowFailReason::OutOfMemory);

        void* memory = this->memory();
        RELEASE_ASSERT(memory);

        // Signaling memory must have been pre-allocated virtually.
        uint8_t* startAddress = static_cast<uint8_t*>(memory) + size();
        
        dataLogLnIf(verbose, "Marking WebAssembly memory's ", RawPointer(memory), " as read+write in range [", RawPointer(startAddress), ", ", RawPointer(startAddress + extraBytes), ")");
        if (mprotect(startAddress, extraBytes, PROT_READ | PROT_WRITE)) {
            dataLog("mprotect failed: ", strerror(errno), "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        m_handle->growToSize(desiredSize);
        return success();
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
    return oldPageCount;
}

bool Memory::fill(uint32_t offset, uint8_t targetValue, uint32_t count)
{
    if (sumOverflows<uint32_t>(offset, count))
        return false;

    if (offset + count > m_handle->size())
        return false;

    memset(reinterpret_cast<uint8_t*>(memory()) + offset, targetValue, count);
    return true;
}

bool Memory::copy(uint32_t dstAddress, uint32_t srcAddress, uint32_t count)
{
    if (sumOverflows<uint32_t>(dstAddress, count) || sumOverflows<uint32_t>(srcAddress, count))
        return false;

    const uint32_t lastDstAddress = dstAddress + count;
    const uint32_t lastSrcAddress = srcAddress + count;

    if (lastDstAddress > size() || lastSrcAddress > size())
        return false;

    if (!count)
        return true;

    uint8_t* base = reinterpret_cast<uint8_t*>(memory());
    memcpy(base + dstAddress, base + srcAddress, count);
    return true;
}

bool Memory::init(uint32_t offset, const uint8_t* data, uint32_t length)
{
    if (sumOverflows<uint32_t>(offset, length))
        return false;

    if (offset + length > m_handle->size())
        return false;

    if (!length)
        return true;

    memcpy(reinterpret_cast<uint8_t*>(memory()) + offset, data, length);
    return true;
}

void Memory::registerInstance(Instance* instance)
{
    size_t count = m_instances.size();
    for (size_t index = 0; index < count; index++) {
        if (m_instances.at(index).get() == nullptr) {
            m_instances.at(index) = makeWeakPtr(*instance);
            return;
        }
    }
    m_instances.append(makeWeakPtr(*instance));
}

void Memory::dump(PrintStream& out) const
{
    auto handle = m_handle.copyRef();
    out.print("Memory at ", RawPointer(handle->memory()), ", size ", handle->size(), "B capacity ", handle->mappedCapacity(), "B, initial ", handle->initial(), " maximum ", handle->maximum(), " mode ", makeString(handle->mode()), " sharingMode ", makeString(handle->sharingMode()));
}

} // namespace JSC

} // namespace Wasm

#endif // ENABLE(WEBASSEMBLY)
