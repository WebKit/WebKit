/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#include "JSCInlines.h"
#include "JSWebAssemblyInstance.h"
#include "Options.h"
#include "WasmFaultSignalHandler.h"
#include "WeakGCSetInlines.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/DataLog.h>
#include <wtf/Gigacage.h>
#include <wtf/Lock.h>
#include <wtf/Platform.h>
#include <wtf/PrintStream.h>
#include <wtf/RAMSize.h>
#include <wtf/SafeStrerror.h>
#include <wtf/StdSet.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

#include <cstring>
#include <limits>
#include <mutex>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED_TEMPLATE(MemoryJSWebAssemblyInstanceWeakCGSet, Wasm::Memory::JSWebAssemblyInstanceWeakCGSet);

namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Memory);

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

Memory::Memory(VM& vm)
    : m_handle(adoptRef(*new BufferMemoryHandle(BufferMemoryHandle::nullBasePointer(), 0, 0, PageCount(0), PageCount(0), MemorySharingMode::Default, MemoryMode::BoundsChecking)))
    , m_instances(vm)
{
}

Memory::Memory(VM& vm, PageCount initial, PageCount maximum, MemorySharingMode sharingMode, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
    : m_handle(adoptRef(*new BufferMemoryHandle(BufferMemoryHandle::nullBasePointer(), 0, 0, initial, maximum, sharingMode, MemoryMode::BoundsChecking)))
    , m_growSuccessCallback(WTFMove(growSuccessCallback))
    , m_instances(vm)
{
    ASSERT(!initial.bytes());
    ASSERT(mode() == MemoryMode::BoundsChecking);
    dataLogLnIf(verbose, "Memory::Memory allocating ", *this);
    ASSERT(basePointer());
}

Memory::Memory(VM& vm, Ref<BufferMemoryHandle>&& handle, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
    : m_handle(WTFMove(handle))
    , m_growSuccessCallback(WTFMove(growSuccessCallback))
    , m_instances(vm)
{
    dataLogLnIf(verbose, "Memory::Memory allocating ", *this);
}

Memory::Memory(VM& vm, Ref<BufferMemoryHandle>&& handle, Ref<SharedArrayBufferContents>&& shared, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
    : m_handle(WTFMove(handle))
    , m_shared(WTFMove(shared))
    , m_growSuccessCallback(WTFMove(growSuccessCallback))
    , m_instances(vm)
{
    dataLogLnIf(verbose, "Memory::Memory allocating ", *this);
}

Ref<Memory> Memory::create(VM& vm)
{
    return adoptRef(*new Memory(vm));
}

Ref<Memory> Memory::create(VM& vm, Ref<BufferMemoryHandle>&& handle, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
{
    return adoptRef(*new Memory(vm, WTFMove(handle), WTFMove(growSuccessCallback)));
}

Ref<Memory> Memory::create(VM& vm, Ref<SharedArrayBufferContents>&& shared, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
{
    RefPtr<BufferMemoryHandle> handle = shared->memoryHandle();
    ASSERT(handle);
    return adoptRef(*new Memory(vm, handle.releaseNonNull(), WTFMove(shared), WTFMove(growSuccessCallback)));
}

Ref<Memory> Memory::createZeroSized(VM& vm, MemorySharingMode sharingMode, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback)
{
    return adoptRef(*new Memory(vm, PageCount(0), PageCount(0), sharingMode, WTFMove(growSuccessCallback)));
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
        return createZeroSized(vm, sharingMode, WTFMove(growSuccessCallback));
    }
    
    bool done = tryAllocate(vm,
        [&] () -> BufferMemoryResult::Kind {
            return BufferMemoryManager::singleton().tryAllocatePhysicalBytes(initialBytes);
        });
    if (!done)
        return nullptr;
        
    char* fastMemory = nullptr;
    if (Options::useWasmFastMemory()) {
#if CPU(ADDRESS32)
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("32-bit platforms don't support fast memory.");
#endif
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
        OSAllocator::protect(fastMemory + initialBytes, BufferMemoryHandle::fastMappedBytes() - initialBytes, readable, writable);
        switch (sharingMode) {
        case MemorySharingMode::Default: {
            return Memory::create(vm, adoptRef(*new BufferMemoryHandle(fastMemory, initialBytes, BufferMemoryHandle::fastMappedBytes(), initial, maximum, MemorySharingMode::Default, MemoryMode::Signaling)), WTFMove(growSuccessCallback));
        }
        case MemorySharingMode::Shared: {
            auto handle = adoptRef(*new BufferMemoryHandle(fastMemory, initialBytes, BufferMemoryHandle::fastMappedBytes(), initial, maximum, MemorySharingMode::Shared, MemoryMode::Signaling));
            auto span = handle->mutableSpan();
            auto content = SharedArrayBufferContents::create(span, maximumBytes, WTFMove(handle), nullptr, SharedArrayBufferContents::Mode::WebAssembly);
            return Memory::create(vm, WTFMove(content), WTFMove(growSuccessCallback));
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }

    if (UNLIKELY(Options::crashIfWasmCantFastMemory()))
        webAssemblyCouldntGetFastMemory();

    switch (sharingMode) {
    case MemorySharingMode::Default: {
        if (!initialBytes)
            return adoptRef(new Memory(vm, initial, maximum, MemorySharingMode::Default, WTFMove(growSuccessCallback)));

        void* slowMemory = Gigacage::tryAllocateZeroedVirtualPages(Gigacage::Primitive, initialBytes);
        if (!slowMemory) {
            BufferMemoryManager::singleton().freePhysicalBytes(initialBytes);
            return nullptr;
        }
        return Memory::create(vm, adoptRef(*new BufferMemoryHandle(slowMemory, initialBytes, initialBytes, initial, maximum, MemorySharingMode::Default, MemoryMode::BoundsChecking)), WTFMove(growSuccessCallback));
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
        OSAllocator::protect(slowMemory + initialBytes, maximumBytes - initialBytes, readable, writable);

        auto handle = adoptRef(*new BufferMemoryHandle(slowMemory, initialBytes, maximumBytes, initial, maximum, MemorySharingMode::Shared, MemoryMode::BoundsChecking));
        auto span = handle->mutableSpan();
        auto content = SharedArrayBufferContents::create(span, maximumBytes, WTFMove(handle), nullptr, SharedArrayBufferContents::Mode::WebAssembly);
        return Memory::create(vm, WTFMove(content), WTFMove(growSuccessCallback));
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
        if (auto* strongReference = static_cast<JSWebAssemblyInstance*>(instance.get()))
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
                static_cast<JSC::JSWebAssemblyInstance*>(instance.get())->updateCachedMemory();
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

        memcpy(newMemory, basePointer(), size());
        auto newHandle = adoptRef(*new BufferMemoryHandle(newMemory, desiredSize, desiredSize, initial(), maximum(), sharingMode(), MemoryMode::BoundsChecking));
        m_handle = WTFMove(newHandle);

        ASSERT(basePointer() == newMemory);
        return success();
    }
    case MemoryMode::Signaling: {
        size_t extraBytes = desiredSize - size();
        RELEASE_ASSERT(extraBytes);
        bool allocationSuccess = tryAllocate(vm,
            [&] () -> BufferMemoryResult::Kind {
                return BufferMemoryManager::singleton().tryAllocatePhysicalBytes(extraBytes);
            });
        if (!allocationSuccess)
            return makeUnexpected(GrowFailReason::OutOfMemory);

        void* memory = this->basePointer();
        RELEASE_ASSERT(memory);

        // Signaling memory must have been pre-allocated virtually.
        uint8_t* startAddress = static_cast<uint8_t*>(memory) + size();
        
        dataLogLnIf(verbose, "Marking WebAssembly memory's ", RawPointer(memory), " as read+write in range [", RawPointer(startAddress), ", ", RawPointer(startAddress + extraBytes), ")");
        constexpr bool readable = true;
        constexpr bool writable = true;
        OSAllocator::protect(startAddress, extraBytes, readable, writable);
        m_handle->updateSize(desiredSize);
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

    memset(static_cast<uint8_t*>(basePointer()) + offset, targetValue, count);
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

    uint8_t* base = static_cast<uint8_t*>(basePointer());
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

    memcpy(static_cast<uint8_t*>(basePointer()) + offset, data, length);
    return true;
}

void Memory::registerInstance(JSWebAssemblyInstance& instance)
{
    auto result = m_instances.add(&instance);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void Memory::dump(PrintStream& out) const
{
    auto handle = m_handle.copyRef();
    out.print("Memory at ", RawPointer(handle->memory()), ", size ", handle->size(), "B capacity ", handle->mappedCapacity(), "B, initial ", handle->initial(), " maximum ", handle->maximum(), " mode ", handle->mode(), " sharingMode ", handle->sharingMode());
}

} // namespace Wasm

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
