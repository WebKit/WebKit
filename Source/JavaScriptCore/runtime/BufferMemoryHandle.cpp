/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "BufferMemoryHandle.h"

#include "Options.h"
#include "WasmFaultSignalHandler.h"
#include <cstring>
#include <limits>
#include <mutex>
#include <wtf/CheckedArithmetic.h>
#include <wtf/DataLog.h>
#include <wtf/Gigacage.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/OSAllocator.h>
#include <wtf/Platform.h>
#include <wtf/PrintStream.h>
#include <wtf/SafeStrerror.h>
#include <wtf/Vector.h>

namespace JSC {

// FIXME: We could be smarter about memset / mmap / madvise. https://bugs.webkit.org/show_bug.cgi?id=170343
// FIXME: Give up some of the cached fast memories if the GC determines it's easy to get them back, and they haven't been used in a while. https://bugs.webkit.org/show_bug.cgi?id=170773
// FIXME: Limit slow memory size. https://bugs.webkit.org/show_bug.cgi?id=170825

#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
size_t BufferMemoryHandle::fastMappedRedzoneBytes()
{
    return static_cast<size_t>(PageCount::pageSize) * Options::webAssemblyFastMemoryRedzonePages();
}

size_t BufferMemoryHandle::fastMappedBytes()
{
    static_assert(sizeof(uint64_t) == sizeof(size_t), "We rely on allowing the maximum size of Memory we map to be 2^32 + redzone which is larger than fits in a 32-bit integer that we'd pass to mprotect if this didn't hold.");
    return (static_cast<size_t>(1) << 32) + fastMappedRedzoneBytes();
}
#endif

ASCIILiteral BufferMemoryResult::toString(Kind kind)
{
    switch (kind) {
    case Success:
        return "Success"_s;
    case SuccessAndNotifyMemoryPressure:
        return "SuccessAndNotifyMemoryPressure"_s;
    case SyncTryToReclaimMemory:
        return "SyncTryToReclaimMemory"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

void BufferMemoryResult::dump(PrintStream& out) const
{
    out.print("{basePtr = ", RawPointer(basePtr), ", kind = ", toString(kind), "}");
}

#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
BufferMemoryResult BufferMemoryManager::tryAllocateFastMemory()
{
    BufferMemoryResult result = [&] {
        Locker locker { m_lock };
        if (m_fastMemories.size() >= m_maxFastMemoryCount)
            return BufferMemoryResult(nullptr, BufferMemoryResult::SyncTryToReclaimMemory);

        void* result = Gigacage::tryAllocateZeroedVirtualPages(Gigacage::Primitive, BufferMemoryHandle::fastMappedBytes());
        if (!result)
            return BufferMemoryResult(nullptr, BufferMemoryResult::SyncTryToReclaimMemory);

        m_fastMemories.append(result);

        return BufferMemoryResult(
            result,
            m_fastMemories.size() >= m_maxFastMemoryCount / 2 ? BufferMemoryResult::SuccessAndNotifyMemoryPressure : BufferMemoryResult::Success);
    }();

    dataLogLnIf(Options::logWebAssemblyMemory(), "Allocated virtual: ", result, "; state: ", *this);

    return result;
}

void BufferMemoryManager::freeFastMemory(void* basePtr)
{
    {
        Locker locker { m_lock };
        Gigacage::freeVirtualPages(Gigacage::Primitive, basePtr, BufferMemoryHandle::fastMappedBytes());
        m_fastMemories.removeFirst(basePtr);
    }

    dataLogLnIf(Options::logWebAssemblyMemory(), "Freed virtual; state: ", *this);
}
#endif

BufferMemoryResult BufferMemoryManager::tryAllocateGrowableBoundsCheckingMemory(size_t mappedCapacity)
{
    BufferMemoryResult result = [&] {
        Locker locker { m_lock };
        void* result = Gigacage::tryAllocateZeroedVirtualPages(Gigacage::Primitive, mappedCapacity);
        if (!result)
            return BufferMemoryResult(nullptr, BufferMemoryResult::SyncTryToReclaimMemory);

        m_growableBoundsCheckingMemories.insert(std::make_pair(bitwise_cast<uintptr_t>(result), mappedCapacity));

        return BufferMemoryResult(result, BufferMemoryResult::Success);
    }();

    dataLogLnIf(Options::logWebAssemblyMemory(), "Allocated virtual: ", result, "; state: ", *this);

    return result;
}

void BufferMemoryManager::freeGrowableBoundsCheckingMemory(void* basePtr, size_t mappedCapacity)
{
    {
        Locker locker { m_lock };
        Gigacage::freeVirtualPages(Gigacage::Primitive, basePtr, mappedCapacity);
        m_growableBoundsCheckingMemories.erase(std::make_pair(bitwise_cast<uintptr_t>(basePtr), mappedCapacity));
    }

    dataLogLnIf(Options::logWebAssemblyMemory(), "Freed virtual; state: ", *this);
}

bool BufferMemoryManager::isInGrowableOrFastMemory(void* address)
{
    // NOTE: This can be called from a signal handler, but only after we proved that we're in JIT code or WasmLLInt code.
    Locker locker { m_lock };
#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
    for (void* memory : m_fastMemories) {
        char* start = static_cast<char*>(memory);
        if (start <= address && address <= start + BufferMemoryHandle::fastMappedBytes())
            return true;
    }
#endif
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

// FIXME: Ideally, bmalloc would have this kind of mechanism. Then, we would just forward to that
// mechanism here.
BufferMemoryResult::Kind BufferMemoryManager::tryAllocatePhysicalBytes(size_t bytes)
{
    BufferMemoryResult::Kind result = [&] {
        Locker locker { m_lock };
        if (m_physicalBytes + bytes > memoryLimit())
            return BufferMemoryResult::SyncTryToReclaimMemory;

        m_physicalBytes += bytes;

        if (m_physicalBytes >= memoryLimit() / 2)
            return BufferMemoryResult::SuccessAndNotifyMemoryPressure;

        return BufferMemoryResult::Success;
    }();

    dataLogLnIf(Options::logWebAssemblyMemory(), "Allocated physical: ", bytes, ", ", BufferMemoryResult::toString(result), "; state: ", *this);

    return result;
}

void BufferMemoryManager::freePhysicalBytes(size_t bytes)
{
    {
        Locker locker { m_lock };
        m_physicalBytes -= bytes;
    }

    dataLogLnIf(Options::logWebAssemblyMemory(), "Freed physical: ", bytes, "; state: ", *this);
}

void BufferMemoryManager::dump(PrintStream& out) const
{
#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
    out.print("fast memories =  ", m_fastMemories.size(), "/", m_maxFastMemoryCount, ", bytes = ", m_physicalBytes, "/", memoryLimit());
#else
    out.print("fast memories = N.A., bytes = ", m_physicalBytes, "/", memoryLimit());
#endif
}

BufferMemoryManager& BufferMemoryManager::singleton()
{
    static std::once_flag onceFlag;
    static LazyNeverDestroyed<BufferMemoryManager> manager;
    std::call_once(onceFlag, []{
        manager.construct();
    });
    return manager.get();
}

BufferMemoryHandle::BufferMemoryHandle(void* memory, size_t size, size_t mappedCapacity, PageCount initial, PageCount maximum, MemorySharingMode sharingMode, MemoryMode mode)
    : m_sharingMode(sharingMode)
    , m_mode(mode)
    , m_memory(memory, mappedCapacity)
    , m_size(size)
    , m_mappedCapacity(mappedCapacity)
    , m_initial(initial)
    , m_maximum(maximum)
{
    if (sharingMode == MemorySharingMode::Default && mode == MemoryMode::BoundsChecking)
        ASSERT(mappedCapacity == size);
    else {
#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
        Wasm::activateSignalingMemory();
#endif
    }
}

void* BufferMemoryHandle::nullBasePointer()
{
    static void* result = nullptr;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&]() {
#if GIGACAGE_ENABLED
        if (Gigacage::isEnabled(Gigacage::Primitive)) {
            result = Gigacage::basePtr(Gigacage::Primitive);
            return;
        }
#endif
        result = fastAlignedMalloc(PageCount::pageSize, PageCount::pageSize);
        WTF::fastDecommitAlignedMemory(result, PageCount::pageSize);
    });
    return result;
}

BufferMemoryHandle::~BufferMemoryHandle()
{
    if (m_memory) {
        void* memory = this->memory();
        BufferMemoryManager::singleton().freePhysicalBytes(m_size);
        switch (m_mode) {
#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
        case MemoryMode::Signaling: {
            // nullBasePointer's zero-sized memory is not used for MemoryMode::Signaling.
            constexpr bool readable = true;
            constexpr bool writable = true;
            if (!OSAllocator::protect(memory, BufferMemoryHandle::fastMappedBytes(), readable, writable)) {
#if OS(WINDOWS)
                dataLogLn("mprotect failed: ", static_cast<int>(GetLastError()));
#else
                dataLogLn("mprotect failed: ", safeStrerror(errno).data());
#endif
                RELEASE_ASSERT_NOT_REACHED();
            }
            BufferMemoryManager::singleton().freeFastMemory(memory);
            break;
        }
#endif
        case MemoryMode::BoundsChecking: {
            switch (m_sharingMode) {
            case MemorySharingMode::Default: {
                if (memory == nullBasePointer() && !m_size)
                    return;
                Gigacage::freeVirtualPages(Gigacage::Primitive, memory, m_size);
                break;
            }
            case MemorySharingMode::Shared: {
                if (memory == nullBasePointer() && !m_mappedCapacity) {
                    ASSERT(!m_size);
                    return;
                }
                constexpr bool readable = true;
                constexpr bool writable = true;
                if (!OSAllocator::protect(memory, m_mappedCapacity, readable, writable)) {
#if OS(WINDOWS)
                    dataLogLn("mprotect failed: ", static_cast<int>(GetLastError()));
#else
                    dataLogLn("mprotect failed: ", safeStrerror(errno).data());
#endif
                    RELEASE_ASSERT_NOT_REACHED();
                }
                BufferMemoryManager::singleton().freeGrowableBoundsCheckingMemory(memory, m_mappedCapacity);
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
NEVER_INLINE void* BufferMemoryHandle::memory() const
{
    ASSERT(m_memory.getMayBeNull(m_mappedCapacity) == m_memory.getUnsafe());
    return m_memory.getMayBeNull(m_mappedCapacity);
}

} // namespace JSC
