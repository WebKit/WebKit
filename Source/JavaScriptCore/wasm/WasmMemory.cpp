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

#if ENABLE(WEBASSEMBLY)

#include "Options.h"
#include "WasmFaultSignalHandler.h"
#include "WasmInstance.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/DataLog.h>
#include <wtf/Gigacage.h>
#include <wtf/Lock.h>
#include <wtf/Platform.h>
#include <wtf/PrintStream.h>
#include <wtf/RAMSize.h>
#include <wtf/SafeStrerror.h>
#include <wtf/StdSet.h>
#include <wtf/Vector.h>

#include <cstring>
#include <limits>
#include <mutex>

namespace JSC { namespace Wasm {

// FIXME: We could be smarter about memset / mmap / madvise. https://bugs.webkit.org/show_bug.cgi?id=170343
// FIXME: Give up some of the cached fast memories if the GC determines it's easy to get them back, and they haven't been used in a while. https://bugs.webkit.org/show_bug.cgi?id=170773
// FIXME: Limit slow memory size. https://bugs.webkit.org/show_bug.cgi?id=170825

namespace {

constexpr bool verbose = false;

NEVER_INLINE NO_RETURN_DUE_TO_CRASH void webAssemblyCouldntGetFastMemory() { CRASH(); }

template<typename Func>
static bool tryAllocate(VM& vm, const Func& allocate)
{
    unsigned numTries = 2;
    bool done = false;
    for (unsigned i = 0; i < numTries && !done; ++i) {
        switch (allocate()) {
        case BufferMemoryResult::Success:
            done = true;
            break;
        case BufferMemoryResult::SuccessAndNotifyMemoryPressure:
            vm.heap.collectAsync(CollectionScope::Full);
            done = true;
            break;
        case BufferMemoryResult::SyncTryToReclaimMemory:
            if (i + 1 == numTries)
                break;
            vm.heap.collectSync(CollectionScope::Full);
            break;
        }
    }
    return done;
}

} // anonymous namespace


Memory::Memory()
    : m_handle(adoptRef(*new BufferMemoryHandle(nullptr, 0, 0, PageCount(0), PageCount(0), MemorySharingMode::Default, MemoryMode::BoundsChecking)))
{
}

Memory::Memory(PageCount initial, PageCount maximum, MemorySharingMode sharingMode, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
    : m_handle(adoptRef(*new BufferMemoryHandle(nullptr, 0, 0, initial, maximum, sharingMode, MemoryMode::BoundsChecking)))
    , m_growSuccessCallback(WTFMove(growSuccessCallback))
{
    ASSERT(!initial.bytes());
    ASSERT(mode() == MemoryMode::BoundsChecking);
    dataLogLnIf(verbose, "Memory::Memory allocating ", *this);
    ASSERT(!memory());
}

Memory::Memory(Ref<BufferMemoryHandle>&& handle, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
    : m_handle(WTFMove(handle))
    , m_growSuccessCallback(WTFMove(growSuccessCallback))
{
    dataLogLnIf(verbose, "Memory::Memory allocating ", *this);
}

Memory::Memory(Ref<BufferMemoryHandle>&& handle, Ref<SharedArrayBufferContents>&& shared, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
    : m_handle(WTFMove(handle))
    , m_shared(WTFMove(shared))
    , m_growSuccessCallback(WTFMove(growSuccessCallback))
{
    dataLogLnIf(verbose, "Memory::Memory allocating ", *this);
}

Ref<Memory> Memory::create()
{
    return adoptRef(*new Memory());
}

Ref<Memory> Memory::create(Ref<BufferMemoryHandle>&& handle, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
{
    return adoptRef(*new Memory(WTFMove(handle), WTFMove(growSuccessCallback)));
}

Ref<Memory> Memory::create(Ref<SharedArrayBufferContents>&& shared, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
{
    RefPtr<BufferMemoryHandle> handle = shared->memoryHandle();
    ASSERT(handle);
    return adoptRef(*new Memory(handle.releaseNonNull(), WTFMove(shared), WTFMove(growSuccessCallback)));
}

Ref<Memory> Memory::createZeroSized(MemorySharingMode sharingMode, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
{
    return adoptRef(*new Memory(PageCount(0), PageCount(0), sharingMode, WTFMove(growSuccessCallback)));
}

RefPtr<Memory> Memory::tryCreate(VM& vm, PageCount initial, PageCount maximum, MemorySharingMode sharingMode, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
{
    ASSERT(initial);
    RELEASE_ASSERT(!maximum || maximum >= initial); // This should be guaranteed by our caller.

    const uint64_t initialBytes = initial.bytes();
    const uint64_t maximumBytes = maximum ? maximum.bytes() : 0;

    if (initialBytes > MAX_ARRAY_BUFFER_SIZE)
        return nullptr; // Client will throw OOMError.

    if (maximum && !maximumBytes) {
        // User specified a zero maximum, initial size must also be zero.
        RELEASE_ASSERT(!initialBytes);
        return createZeroSized(sharingMode, WTFMove(growSuccessCallback));
    }
    
    bool done = tryAllocate(vm,
        [&] () -> BufferMemoryResult::Kind {
            return BufferMemoryManager::singleton().tryAllocatePhysicalBytes(initialBytes);
        });
    if (!done)
        return nullptr;
        
#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
    char* fastMemory = nullptr;
    if (Options::useWebAssemblyFastMemory()) {
        tryAllocate(vm,
            [&] () -> BufferMemoryResult::Kind {
                auto result = BufferMemoryManager::singleton().tryAllocateFastMemory();
                fastMemory = bitwise_cast<char*>(result.basePtr);
                return result.kind;
            });
    }
    
    if (fastMemory) {
        constexpr bool readable = false;
        constexpr bool writable = false;
        if (!OSAllocator::protect(fastMemory + initialBytes, BufferMemoryHandle::fastMappedBytes() - initialBytes, readable, writable)) {
#if OS(WINDOWS)
            dataLogLn("mprotect failed: ", static_cast<int>(GetLastError()));
#else
            dataLogLn("mprotect failed: ", safeStrerror(errno).data());
#endif
            RELEASE_ASSERT_NOT_REACHED();
        }

        switch (sharingMode) {
        case MemorySharingMode::Default: {
            return Memory::create(adoptRef(*new BufferMemoryHandle(fastMemory, initialBytes, BufferMemoryHandle::fastMappedBytes(), initial, maximum, MemorySharingMode::Default, MemoryMode::Signaling)), WTFMove(growSuccessCallback));
        }
        case MemorySharingMode::Shared: {
            auto handle = adoptRef(*new BufferMemoryHandle(fastMemory, initialBytes, BufferMemoryHandle::fastMappedBytes(), initial, maximum, MemorySharingMode::Shared, MemoryMode::Signaling));
            void* memory = handle->memory();
            size_t size = handle->size();
            auto content = SharedArrayBufferContents::create(memory, size, maximumBytes, WTFMove(handle), nullptr, SharedArrayBufferContents::Mode::WebAssembly);
            return Memory::create(WTFMove(content), WTFMove(growSuccessCallback));
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
#endif

    if (UNLIKELY(Options::crashIfWebAssemblyCantFastMemory()))
        webAssemblyCouldntGetFastMemory();

    switch (sharingMode) {
    case MemorySharingMode::Default: {
        if (!initialBytes)
            return adoptRef(new Memory(initial, maximum, MemorySharingMode::Default, WTFMove(growSuccessCallback)));

        void* slowMemory = Gigacage::tryAllocateZeroedVirtualPages(Gigacage::Primitive, initialBytes);
        if (!slowMemory) {
            BufferMemoryManager::singleton().freePhysicalBytes(initialBytes);
            return nullptr;
        }
        return Memory::create(adoptRef(*new BufferMemoryHandle(slowMemory, initialBytes, initialBytes, initial, maximum, MemorySharingMode::Default, MemoryMode::BoundsChecking)), WTFMove(growSuccessCallback));
    }
    case MemorySharingMode::Shared: {
        char* slowMemory = nullptr;
        tryAllocate(vm,
            [&] () -> BufferMemoryResult::Kind {
                auto result = BufferMemoryManager::singleton().tryAllocateGrowableBoundsCheckingMemory(maximumBytes);
                slowMemory = bitwise_cast<char*>(result.basePtr);
                return result.kind;
            });
        if (!slowMemory) {
            BufferMemoryManager::singleton().freePhysicalBytes(initialBytes);
            return nullptr;
        }

        constexpr bool readable = false;
        constexpr bool writable = false;
        if (!OSAllocator::protect(slowMemory + initialBytes, maximumBytes - initialBytes, readable, writable)) {
#if OS(WINDOWS)
            dataLogLn("mprotect failed: ", static_cast<int>(GetLastError()));
#else
            dataLogLn("mprotect failed: ", safeStrerror(errno).data());
#endif
            RELEASE_ASSERT_NOT_REACHED();
        }

        auto handle = adoptRef(*new BufferMemoryHandle(slowMemory, initialBytes, maximumBytes, initial, maximum, MemorySharingMode::Shared, MemoryMode::BoundsChecking));
        void* memory = handle->memory();
        size_t size = handle->size();
        auto content = SharedArrayBufferContents::create(memory, size, maximumBytes, WTFMove(handle), nullptr, SharedArrayBufferContents::Mode::WebAssembly);
        return Memory::create(WTFMove(content), WTFMove(growSuccessCallback));
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

Memory::~Memory() = default;

bool Memory::addressIsInGrowableOrFastMemory(void* address)
{
    return BufferMemoryManager::singleton().isInGrowableOrFastMemory(address);
}

Expected<PageCount, GrowFailReason> Memory::growShared(VM& vm, PageCount delta)
{
#if !ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
    // Shared memory requires signaling memory which is not available on ARMv7 or others
    // yet. In order to get more of the test suite to run, we can still use
    // a shared mmeory by using bounds checking, but we cannot grow it safely
    // in case it's used by multiple threads. Once the signal handler are
    // available, this can be relaxed.
    return makeUnexpected(GrowFailReason::GrowSharedUnavailable);
#endif

    PageCount oldPageCount;
    PageCount newPageCount;
    Expected<int64_t, GrowFailReason> result;
    {
        std::optional<Locker<Lock>> locker;
        // m_shared may not be exist, if this is zero byte memory with zero byte maximum size.
        if (m_shared)
            locker.emplace(m_shared->memoryHandle()->lock());

        oldPageCount = PageCount::fromBytes(size());
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
        RELEASE_ASSERT(m_shared);
        RELEASE_ASSERT(desiredSize <= MAX_ARRAY_BUFFER_SIZE);
        RELEASE_ASSERT(desiredSize > size());
        result = m_shared->grow(locker.value(), vm, desiredSize);
    }
    if (!result)
        return makeUnexpected(result.error());

    m_growSuccessCallback(GrowSuccessTag, oldPageCount, newPageCount);
    // Update cache for instance
    for (auto& instance : m_instances) {
        if (auto strongReference = instance.get())
            strongReference->updateCachedMemory();
    }
    return oldPageCount;
}

Expected<PageCount, GrowFailReason> Memory::grow(VM& vm, PageCount delta)
{
    if (!delta.isValid())
        return makeUnexpected(GrowFailReason::InvalidDelta);

    if (sharingMode() == MemorySharingMode::Shared)
        return growShared(vm, delta);

    ASSERT(!m_shared);
    const PageCount oldPageCount = PageCount::fromBytes(size());
    const PageCount newPageCount = oldPageCount + delta;
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
        bool allocationSuccess = tryAllocate(vm,
            [&] () -> BufferMemoryResult::Kind {
                return BufferMemoryManager::singleton().tryAllocatePhysicalBytes(desiredSize);
            });
        if (!allocationSuccess)
            return makeUnexpected(GrowFailReason::OutOfMemory);

        RELEASE_ASSERT(maximum().bytes() != 0);

        void* newMemory = Gigacage::tryAllocateZeroedVirtualPages(Gigacage::Primitive, desiredSize);
        if (!newMemory)
            return makeUnexpected(GrowFailReason::OutOfMemory);

        memcpy(newMemory, memory(), size());
        auto newHandle = adoptRef(*new BufferMemoryHandle(newMemory, desiredSize, desiredSize, initial(), maximum(), sharingMode(), MemoryMode::BoundsChecking));
        m_handle = WTFMove(newHandle);

        ASSERT(memory() == newMemory);
        return success();
    }
#if ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
    case MemoryMode::Signaling: {
        size_t extraBytes = desiredSize - size();
        RELEASE_ASSERT(extraBytes);
        bool allocationSuccess = tryAllocate(vm,
            [&] () -> BufferMemoryResult::Kind {
                return BufferMemoryManager::singleton().tryAllocatePhysicalBytes(extraBytes);
            });
        if (!allocationSuccess)
            return makeUnexpected(GrowFailReason::OutOfMemory);

        void* memory = this->memory();
        RELEASE_ASSERT(memory);

        // Signaling memory must have been pre-allocated virtually.
        uint8_t* startAddress = static_cast<uint8_t*>(memory) + size();
        
        dataLogLnIf(verbose, "Marking WebAssembly memory's ", RawPointer(memory), " as read+write in range [", RawPointer(startAddress), ", ", RawPointer(startAddress + extraBytes), ")");
        constexpr bool readable = true;
        constexpr bool writable = true;
        if (!OSAllocator::protect(startAddress, extraBytes, readable, writable)) {
#if OS(WINDOWS)
            dataLogLn("mprotect failed: ", static_cast<int>(GetLastError()));
#else
            dataLogLn("mprotect failed: ", safeStrerror(errno).data());
#endif
            RELEASE_ASSERT_NOT_REACHED();
        }

        m_handle->updateSize(desiredSize);
        return success();
    }
#endif
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
    // Source and destination areas might overlap, so using memmove.
    memmove(base + dstAddress, base + srcAddress, count);
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

void Memory::registerInstance(Instance& instance)
{
    size_t count = m_instances.size();
    for (size_t index = 0; index < count; index++) {
        if (m_instances.at(index).get() == nullptr) {
            m_instances.at(index) = { instance };
            return;
        }
    }
    m_instances.append({ instance });
}

void Memory::dump(PrintStream& out) const
{
    auto handle = m_handle.copyRef();
    out.print("Memory at ", RawPointer(handle->memory()), ", size ", handle->size(), "B capacity ", handle->mappedCapacity(), "B, initial ", handle->initial(), " maximum ", handle->maximum(), " mode ", handle->mode(), " sharingMode ", handle->sharingMode());
}

} // namespace JSC

} // namespace Wasm

#endif // ENABLE(WEBASSEMBLY)
