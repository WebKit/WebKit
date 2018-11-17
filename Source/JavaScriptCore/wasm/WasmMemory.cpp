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
#include <wtf/DataLog.h>
#include <wtf/Gigacage.h>
#include <wtf/Lock.h>
#include <wtf/OSAllocator.h>
#include <wtf/PageBlock.h>
#include <wtf/Platform.h>
#include <wtf/PrintStream.h>
#include <wtf/RAMSize.h>
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
        
        if (Options::logWebAssemblyMemory())
            dataLog("Allocated virtual: ", result, "; state: ", *this, "\n");
        
        return result;
    }
    
    void freeFastMemory(void* basePtr)
    {
        {
            auto holder = holdLock(m_lock);
            Gigacage::freeVirtualPages(Gigacage::Primitive, basePtr, Memory::fastMappedBytes());
            m_fastMemories.removeFirst(basePtr);
        }
        
        if (Options::logWebAssemblyMemory())
            dataLog("Freed virtual; state: ", *this, "\n");
    }
    
    bool isAddressInFastMemory(void* address)
    {
        // NOTE: This can be called from a signal handler, but only after we proved that we're in JIT code.
        auto holder = holdLock(m_lock);
        for (void* memory : m_fastMemories) {
            char* start = static_cast<char*>(memory);
            if (start <= address && address <= start + Memory::fastMappedBytes())
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
        
        if (Options::logWebAssemblyMemory())
            dataLog("Allocated physical: ", bytes, ", ", MemoryResult::toString(result), "; state: ", *this, "\n");
        
        return result;
    }
    
    void freePhysicalBytes(size_t bytes)
    {
        {
            auto holder = holdLock(m_lock);
            m_physicalBytes -= bytes;
        }
        
        if (Options::logWebAssemblyMemory())
            dataLog("Freed physical: ", bytes, "; state: ", *this, "\n");
    }
    
    void dump(PrintStream& out) const
    {
        out.print("fast memories =  ", m_fastMemories.size(), "/", m_maxFastMemoryCount, ", bytes = ", m_physicalBytes, "/", memoryLimit());
    }
    
private:
    Lock m_lock;
    unsigned m_maxFastMemoryCount { 0 };
    Vector<void*> m_fastMemories;
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

Memory::Memory()
{
}

Memory::Memory(PageCount initial, PageCount maximum, Function<void(NotifyPressure)>&& notifyMemoryPressure, Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
    : m_initial(initial)
    , m_maximum(maximum)
    , m_notifyMemoryPressure(WTFMove(notifyMemoryPressure))
    , m_syncTryToReclaimMemory(WTFMove(syncTryToReclaimMemory))
    , m_growSuccessCallback(WTFMove(growSuccessCallback))
{
    ASSERT(!initial.bytes());
    ASSERT(m_mode == MemoryMode::BoundsChecking);
    dataLogLnIf(verbose, "Memory::Memory allocating ", *this);
}

Memory::Memory(void* memory, PageCount initial, PageCount maximum, size_t mappedCapacity, MemoryMode mode, Function<void(NotifyPressure)>&& notifyMemoryPressure, Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
    : m_memory(memory)
    , m_size(initial.bytes())
    , m_initial(initial)
    , m_maximum(maximum)
    , m_mappedCapacity(mappedCapacity)
    , m_mode(mode)
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

RefPtr<Memory> Memory::tryCreate(PageCount initial, PageCount maximum, WTF::Function<void(NotifyPressure)>&& notifyMemoryPressure, WTF::Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
{
    ASSERT(initial);
    RELEASE_ASSERT(!maximum || maximum >= initial); // This should be guaranteed by our caller.

    const size_t initialBytes = initial.bytes();
    const size_t maximumBytes = maximum ? maximum.bytes() : 0;

    RELEASE_ASSERT(initialBytes <= MAX_ARRAY_BUFFER_SIZE);

    if (maximum && !maximumBytes) {
        // User specified a zero maximum, initial size must also be zero.
        RELEASE_ASSERT(!initialBytes);
        return adoptRef(new Memory(initial, maximum, WTFMove(notifyMemoryPressure), WTFMove(syncTryToReclaimMemory), WTFMove(growSuccessCallback)));
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

        return adoptRef(new Memory(fastMemory, initial, maximum, Memory::fastMappedBytes(), MemoryMode::Signaling, WTFMove(notifyMemoryPressure), WTFMove(syncTryToReclaimMemory), WTFMove(growSuccessCallback)));
    }
    
    if (UNLIKELY(Options::crashIfWebAssemblyCantFastMemory()))
        webAssemblyCouldntGetFastMemory();

    if (!initialBytes)
        return adoptRef(new Memory(initial, maximum, WTFMove(notifyMemoryPressure), WTFMove(syncTryToReclaimMemory), WTFMove(growSuccessCallback)));
    
    void* slowMemory = Gigacage::tryAllocateZeroedVirtualPages(Gigacage::Primitive, initialBytes);
    if (!slowMemory) {
        memoryManager().freePhysicalBytes(initialBytes);
        return nullptr;
    }
    return adoptRef(new Memory(slowMemory, initial, maximum, initialBytes, MemoryMode::BoundsChecking, WTFMove(notifyMemoryPressure), WTFMove(syncTryToReclaimMemory), WTFMove(growSuccessCallback)));
}

Memory::~Memory()
{
    if (m_memory) {
        memoryManager().freePhysicalBytes(m_size);
        switch (m_mode) {
        case MemoryMode::Signaling:
            if (mprotect(m_memory, Memory::fastMappedBytes(), PROT_READ | PROT_WRITE)) {
                dataLog("mprotect failed: ", strerror(errno), "\n");
                RELEASE_ASSERT_NOT_REACHED();
            }
            memoryManager().freeFastMemory(m_memory);
            break;
        case MemoryMode::BoundsChecking:
            Gigacage::freeVirtualPages(Gigacage::Primitive, m_memory, m_size);
            break;
        }
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

bool Memory::addressIsInActiveFastMemory(void* address)
{
    return memoryManager().isAddressInFastMemory(address);
}

Expected<PageCount, Memory::GrowFailReason> Memory::grow(PageCount delta)
{
    const Wasm::PageCount oldPageCount = sizeInPages();

    if (!delta.isValid())
        return makeUnexpected(GrowFailReason::InvalidDelta);
    
    const Wasm::PageCount newPageCount = oldPageCount + delta;
    // FIXME: Creating a wasm memory that is bigger than the ArrayBuffer limit but smaller than the spec limit should throw
    // OOME not RangeError
    // https://bugs.webkit.org/show_bug.cgi?id=191776
    if (!newPageCount || !newPageCount.isValid() || newPageCount.bytes() >= MAX_ARRAY_BUFFER_SIZE)
        return makeUnexpected(GrowFailReason::InvalidGrowSize);

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
    RELEASE_ASSERT(newPageCount > PageCount::fromBytes(m_size));

    if (maximum() && newPageCount > maximum())
        return makeUnexpected(GrowFailReason::WouldExceedMaximum);

    size_t desiredSize = newPageCount.bytes();
    RELEASE_ASSERT(desiredSize <= MAX_ARRAY_BUFFER_SIZE);
    RELEASE_ASSERT(desiredSize > m_size);
    size_t extraBytes = desiredSize - m_size;
    RELEASE_ASSERT(extraBytes);
    bool allocationSuccess = tryAllocate(
        [&] () -> MemoryResult::Kind {
            return memoryManager().tryAllocatePhysicalBytes(extraBytes);
        }, m_notifyMemoryPressure, m_syncTryToReclaimMemory);
    if (!allocationSuccess)
        return makeUnexpected(GrowFailReason::OutOfMemory);

    switch (mode()) {
    case MemoryMode::BoundsChecking: {
        RELEASE_ASSERT(maximum().bytes() != 0);

        void* newMemory = Gigacage::tryAllocateZeroedVirtualPages(Gigacage::Primitive, desiredSize);
        if (!newMemory)
            return makeUnexpected(GrowFailReason::OutOfMemory);

        memcpy(newMemory, m_memory, m_size);
        if (m_memory)
            Gigacage::freeVirtualPages(Gigacage::Primitive, m_memory, m_size);
        m_memory = newMemory;
        m_mappedCapacity = desiredSize;
        m_size = desiredSize;
        return success();
    }
    case MemoryMode::Signaling: {
        RELEASE_ASSERT(m_memory);
        // Signaling memory must have been pre-allocated virtually.
        uint8_t* startAddress = static_cast<uint8_t*>(m_memory) + m_size;
        
        dataLogLnIf(verbose, "Marking WebAssembly memory's ", RawPointer(m_memory), " as read+write in range [", RawPointer(startAddress), ", ", RawPointer(startAddress + extraBytes), ")");
        if (mprotect(startAddress, extraBytes, PROT_READ | PROT_WRITE)) {
            dataLog("mprotect failed: ", strerror(errno), "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
        m_size = desiredSize;
        return success();
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
    return oldPageCount;
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
    out.print("Memory at ", RawPointer(m_memory), ", size ", m_size, "B capacity ", m_mappedCapacity, "B, initial ", m_initial, " maximum ", m_maximum, " mode ", makeString(m_mode));
}

} // namespace JSC

} // namespace Wasm

#endif // ENABLE(WEBASSEMBLY)
