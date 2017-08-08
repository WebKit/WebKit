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
#include <wtf/Gigacage.h>
#include <wtf/Lock.h>
#include <wtf/Platform.h>
#include <wtf/PrintStream.h>
#include <wtf/RAMSize.h>

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
        SuccessAndAsyncGC,
        SyncGCAndRetry
    };
    
    static const char* toString(Kind kind)
    {
        switch (kind) {
        case Success:
            return "Success";
        case SuccessAndAsyncGC:
            return "SuccessAndAsyncGC";
        case SyncGCAndRetry:
            return "SyncGCAndRetry";
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
        : m_maxCount(Options::maxNumWebAssemblyFastMemories())
    {
    }
    
    MemoryResult tryAllocateVirtualPages()
    {
        MemoryResult result = [&] {
            auto holder = holdLock(m_lock);
            if (m_memories.size() >= m_maxCount)
                return MemoryResult(nullptr, MemoryResult::SyncGCAndRetry);
            
            void* result = Gigacage::tryAllocateVirtualPages(Gigacage::Primitive, Memory::fastMappedBytes());
            if (!result)
                return MemoryResult(nullptr, MemoryResult::SyncGCAndRetry);
            
            m_memories.append(result);
            
            return MemoryResult(
                result,
                m_memories.size() >= m_maxCount / 2 ? MemoryResult::SuccessAndAsyncGC : MemoryResult::Success);
        }();
        
        if (Options::logWebAssemblyMemory())
            dataLog("Allocated virtual: ", result, "; state: ", *this, "\n");
        
        return result;
    }
    
    void freeVirtualPages(void* basePtr)
    {
        {
            auto holder = holdLock(m_lock);
            Gigacage::freeVirtualPages(Gigacage::Primitive, basePtr, Memory::fastMappedBytes());
            m_memories.removeFirst(basePtr);
        }
        
        if (Options::logWebAssemblyMemory())
            dataLog("Freed virtual; state: ", *this, "\n");
    }
    
    bool containsAddress(void* address)
    {
        // NOTE: This can be called from a signal handler, but only after we proved that we're in JIT code.
        auto holder = holdLock(m_lock);
        for (void* memory : m_memories) {
            char* start = static_cast<char*>(memory);
            if (start <= address && address <= start + Memory::fastMappedBytes())
                return true;
        }
        return false;
    }
    
    // FIXME: Ideally, bmalloc would have this kind of mechanism. Then, we would just forward to that
    // mechanism here.
    MemoryResult::Kind tryAllocatePhysicalBytes(size_t bytes)
    {
        MemoryResult::Kind result = [&] {
            auto holder = holdLock(m_lock);
            if (m_physicalBytes + bytes > ramSize())
                return MemoryResult::SyncGCAndRetry;
            
            m_physicalBytes += bytes;
            
            if (m_physicalBytes >= ramSize() / 2)
                return MemoryResult::SuccessAndAsyncGC;
            
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
        out.print("memories =  ", m_memories.size(), "/", m_maxCount, ", bytes = ", m_physicalBytes, "/", ramSize());
    }
    
private:
    Lock m_lock;
    unsigned m_maxCount { 0 };
    Vector<void*> m_memories;
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
bool tryAndGC(VM& vm, const Func& allocate)
{
    unsigned numTries = 2;
    bool done = false;
    for (unsigned i = 0; i < numTries && !done; ++i) {
        switch (allocate()) {
        case MemoryResult::Success:
            done = true;
            break;
        case MemoryResult::SuccessAndAsyncGC:
            vm.heap.collectAsync(CollectionScope::Full);
            done = true;
            break;
        case MemoryResult::SyncGCAndRetry:
            if (i + 1 == numTries)
                break;
            vm.heap.collectSync(CollectionScope::Full);
            break;
        }
    }
    return done;
}

} // anonymous namespace

const char* makeString(MemoryMode mode)
{
    switch (mode) {
    case MemoryMode::BoundsChecking: return "BoundsChecking";
    case MemoryMode::Signaling: return "Signaling";
    }
    RELEASE_ASSERT_NOT_REACHED();
    return "";
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

static void commitZeroPages(void* startAddress, size_t sizeInBytes)
{
    bool writable = true;
    bool executable = false;
#if OS(LINUX)
    // In Linux, MADV_DONTNEED clears backing pages with zero. Be Careful that MADV_DONTNEED shows different semantics in different OSes.
    // For example, FreeBSD does not clear backing pages immediately.
    while (madvise(startAddress, sizeInBytes, MADV_DONTNEED) == -1 && errno == EAGAIN) { }
    OSAllocator::commit(startAddress, sizeInBytes, writable, executable);
#else
    OSAllocator::commit(startAddress, sizeInBytes, writable, executable);
    memset(startAddress, 0, sizeInBytes);
#endif
}

RefPtr<Memory> Memory::create(VM& vm, PageCount initial, PageCount maximum)
{
    ASSERT(initial);
    RELEASE_ASSERT(!maximum || maximum >= initial); // This should be guaranteed by our caller.

    const size_t initialBytes = initial.bytes();
    const size_t maximumBytes = maximum ? maximum.bytes() : 0;

    // We need to be sure we have a stub prior to running code.
    if (UNLIKELY(!Thunks::singleton().stub(throwExceptionFromWasmThunkGenerator)))
        return nullptr;

    if (maximum && !maximumBytes) {
        // User specified a zero maximum, initial size must also be zero.
        RELEASE_ASSERT(!initialBytes);
        return adoptRef(new Memory(initial, maximum));
    }
    
    bool done = tryAndGC(
        vm,
        [&] () -> MemoryResult::Kind {
            return memoryManager().tryAllocatePhysicalBytes(initialBytes);
        });
    if (!done)
        return nullptr;
        
    char* fastMemory = nullptr;
    if (Options::useWebAssemblyFastMemory()) {
        tryAndGC(
            vm,
            [&] () -> MemoryResult::Kind {
                auto result = memoryManager().tryAllocateVirtualPages();
                fastMemory = bitwise_cast<char*>(result.basePtr);
                return result.kind;
            });
    }
    
    if (fastMemory) {
        
        if (mprotect(fastMemory + initialBytes, Memory::fastMappedBytes() - initialBytes, PROT_NONE)) {
            dataLog("mprotect failed: ", strerror(errno), "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        commitZeroPages(fastMemory, initialBytes);
        
        return adoptRef(new Memory(fastMemory, initial, maximum, Memory::fastMappedBytes(), MemoryMode::Signaling));
    }
    
    if (UNLIKELY(Options::crashIfWebAssemblyCantFastMemory()))
        webAssemblyCouldntGetFastMemory();

    if (!initialBytes)
        return adoptRef(new Memory(initial, maximum));
    
    void* slowMemory = Gigacage::tryAlignedMalloc(Gigacage::Primitive, WTF::pageSize(), initialBytes);
    if (!slowMemory) {
        memoryManager().freePhysicalBytes(initialBytes);
        return nullptr;
    }
    memset(slowMemory, 0, initialBytes);
    return adoptRef(new Memory(slowMemory, initial, maximum, initialBytes, MemoryMode::BoundsChecking));
}

Memory::~Memory()
{
    if (m_memory) {
        memoryManager().freePhysicalBytes(m_size);
        switch (m_mode) {
        case MemoryMode::Signaling:
            mprotect(m_memory, Memory::fastMappedBytes(), PROT_READ | PROT_WRITE);
            memoryManager().freeVirtualPages(m_memory);
            break;
        case MemoryMode::BoundsChecking:
            Gigacage::alignedFree(Gigacage::Primitive, m_memory);
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
    return memoryManager().containsAddress(address);
}

bool Memory::grow(VM& vm, PageCount newSize)
{
    RELEASE_ASSERT(newSize > PageCount::fromBytes(m_size));

    dataLogLnIf(verbose, "Memory::grow to ", newSize, " from ", *this);

    if (maximum() && newSize > maximum())
        return false;

    size_t desiredSize = newSize.bytes();
    RELEASE_ASSERT(desiredSize > m_size);
    size_t extraBytes = desiredSize - m_size;
    RELEASE_ASSERT(extraBytes);
    bool success = tryAndGC(
        vm,
        [&] () -> MemoryResult::Kind {
            return memoryManager().tryAllocatePhysicalBytes(extraBytes);
        });
    if (!success)
        return false;
        
    switch (mode()) {
    case MemoryMode::BoundsChecking: {
        RELEASE_ASSERT(maximum().bytes() != 0);
        
        void* newMemory = Gigacage::tryAlignedMalloc(Gigacage::Primitive, WTF::pageSize(), desiredSize);
        if (!newMemory)
            return false;
        memcpy(newMemory, m_memory, m_size);
        memset(static_cast<char*>(newMemory) + m_size, 0, desiredSize - m_size);
        if (m_memory)
            Gigacage::alignedFree(Gigacage::Primitive, m_memory);
        m_memory = newMemory;
        m_mappedCapacity = desiredSize;
        m_size = desiredSize;
        return true;
    }
    case MemoryMode::Signaling: {
        RELEASE_ASSERT(m_memory);
        // Signaling memory must have been pre-allocated virtually.
        uint8_t* startAddress = static_cast<uint8_t*>(m_memory) + m_size;
        
        dataLogLnIf(verbose, "Marking WebAssembly memory's ", RawPointer(m_memory), " as read+write in range [", RawPointer(startAddress), ", ", RawPointer(startAddress + extraBytes), ")");
        if (mprotect(startAddress, extraBytes, PROT_READ | PROT_WRITE)) {
            dataLogLnIf(verbose, "Memory::grow in-place failed ", *this);
            return false;
        }
        commitZeroPages(startAddress, extraBytes);
        m_size = desiredSize;
        return true;
    } }
    
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

void Memory::dump(PrintStream& out) const
{
    out.print("Memory at ", RawPointer(m_memory), ", size ", m_size, "B capacity ", m_mappedCapacity, "B, initial ", m_initial, " maximum ", m_maximum, " mode ", makeString(m_mode));
}

} // namespace JSC

} // namespace Wasm

#endif // ENABLE(WEBASSEMBLY)
