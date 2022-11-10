/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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
#include "ArrayBuffer.h"

#include "JSArrayBufferView.h"
#include "JSCellInlines.h"
#include <wtf/Gigacage.h>
#include <wtf/SafeStrerror.h>

namespace JSC {
namespace ArrayBufferInternal {
static constexpr bool verbose = false;
}

Ref<SharedTask<void(void*)>> ArrayBuffer::primitiveGigacageDestructor()
{
    static LazyNeverDestroyed<Ref<SharedTask<void(void*)>>> destructor;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        destructor.construct(createSharedTask<void(void*)>([] (void* p) { Gigacage::free(Gigacage::Primitive, p); }));
    });
    return destructor.get().copyRef();
}

void ArrayBufferContents::tryAllocate(size_t numElements, unsigned elementByteSize, std::optional<size_t> maxByteLength, InitializationPolicy policy)
{
    CheckedSize sizeInBytes = numElements;
    sizeInBytes *= elementByteSize;
    if (sizeInBytes.hasOverflowed() || sizeInBytes.value() > MAX_ARRAY_BUFFER_SIZE || (maxByteLength && sizeInBytes.value() > maxByteLength.value())) {
        reset();
        return;
    }

    size_t allocationSize = sizeInBytes.value();
    if (!allocationSize)
        allocationSize = 1; // Make sure malloc actually allocates something, but not too much. We use null to mean that the buffer is detached.

    void* data = Gigacage::tryMalloc(Gigacage::Primitive, allocationSize);
    m_data = DataType(data, maxByteLength.value_or(sizeInBytes.value()));
    if (!data) {
        reset();
        return;
    }
    
    if (policy == InitializationPolicy::ZeroInitialize)
        memset(data, 0, allocationSize);

    m_sizeInBytes = sizeInBytes.value();
    RELEASE_ASSERT(m_sizeInBytes <= MAX_ARRAY_BUFFER_SIZE);
    m_maxByteLength = maxByteLength.value_or(m_sizeInBytes);
    m_hasMaxByteLength = !!maxByteLength;
    m_destructor = ArrayBuffer::primitiveGigacageDestructor();
}

void ArrayBufferContents::makeShared()
{
    m_shared = SharedArrayBufferContents::create(data(), sizeInBytes(), maxByteLength(), nullptr, WTFMove(m_destructor), SharedArrayBufferContents::Mode::Default);
    m_destructor = nullptr;
}

void ArrayBufferContents::copyTo(ArrayBufferContents& other)
{
    ASSERT(!other.m_data);
    other.tryAllocate(m_sizeInBytes, sizeof(char), std::nullopt, ArrayBufferContents::InitializationPolicy::DontInitialize);
    if (!other.m_data)
        return;
    memcpy(other.data(), data(), m_sizeInBytes);
    other.m_sizeInBytes = m_sizeInBytes;
    RELEASE_ASSERT(other.m_sizeInBytes <= MAX_ARRAY_BUFFER_SIZE);
}

void ArrayBufferContents::shareWith(ArrayBufferContents& other)
{
    ASSERT(!other.m_data);
    ASSERT(m_shared);
    other.m_data = m_data;
    other.m_destructor = nullptr;
    other.m_shared = m_shared;
    other.m_sizeInBytes = m_sizeInBytes;
    other.m_maxByteLength = m_maxByteLength;
    RELEASE_ASSERT(other.m_sizeInBytes <= MAX_ARRAY_BUFFER_SIZE);
}

Ref<ArrayBuffer> ArrayBuffer::create(size_t numElements, unsigned elementByteSize)
{
    auto buffer = tryCreate(numElements, elementByteSize);
    if (!buffer)
        CRASH();
    return buffer.releaseNonNull();
}

Ref<ArrayBuffer> ArrayBuffer::create(ArrayBuffer& other)
{
    return ArrayBuffer::create(other.data(), other.byteLength());
}

Ref<ArrayBuffer> ArrayBuffer::create(const void* source, size_t byteLength)
{
    auto buffer = tryCreate(source, byteLength);
    if (!buffer)
        CRASH();
    return buffer.releaseNonNull();
}

Ref<ArrayBuffer> ArrayBuffer::create(ArrayBufferContents&& contents)
{
    return adoptRef(*new ArrayBuffer(WTFMove(contents)));
}

// FIXME: We cannot use this except if the memory comes from the cage.
// Current this is only used from:
// - JSGenericTypedArrayView<>::slowDownAndWasteMemory. But in that case, the memory should have already come
//   from the cage.
Ref<ArrayBuffer> ArrayBuffer::createAdopted(const void* data, size_t byteLength)
{
    ASSERT(!Gigacage::isEnabled() || (Gigacage::contains(data) && Gigacage::contains(static_cast<const uint8_t*>(data) + byteLength - 1)));
    return createFromBytes(data, byteLength, ArrayBuffer::primitiveGigacageDestructor());
}

// FIXME: We cannot use this except if the memory comes from the cage.
// Currently this is only used from:
// - The C API. We could support that by either having the system switch to a mode where typed arrays are no
//   longer caged, or we could introduce a new set of typed array types that are uncaged and get accessed
//   differently.
// - WebAssembly. Wasm should allocate from the cage.
Ref<ArrayBuffer> ArrayBuffer::createFromBytes(const void* data, size_t byteLength, ArrayBufferDestructorFunction&& destructor)
{
    if (data && !Gigacage::isCaged(Gigacage::Primitive, data))
        Gigacage::disablePrimitiveGigacage();
    
    ArrayBufferContents contents(const_cast<void*>(data), byteLength, std::nullopt, WTFMove(destructor));
    return create(WTFMove(contents));
}

Ref<ArrayBuffer> ArrayBuffer::createShared(Ref<SharedArrayBufferContents>&& shared)
{
    void* memory = shared->data();
    size_t sizeInBytes = shared->sizeInBytes(std::memory_order_seq_cst);
    ArrayBufferContents contents(memory, sizeInBytes, shared->maxByteLength(), WTFMove(shared));
    return create(WTFMove(contents));
}

RefPtr<ArrayBuffer> ArrayBuffer::tryCreate(size_t numElements, unsigned elementByteSize, std::optional<size_t> maxByteLength)
{
    return tryCreate(numElements, elementByteSize, maxByteLength, ArrayBufferContents::InitializationPolicy::ZeroInitialize);
}

RefPtr<ArrayBuffer> ArrayBuffer::tryCreate(ArrayBuffer& other)
{
    return tryCreate(other.data(), other.byteLength());
}

RefPtr<ArrayBuffer> ArrayBuffer::tryCreate(const void* source, size_t byteLength)
{
    ArrayBufferContents contents;
    contents.tryAllocate(byteLength, 1, std::nullopt, ArrayBufferContents::InitializationPolicy::DontInitialize);
    if (!contents.m_data)
        return nullptr;
    return createInternal(WTFMove(contents), source, byteLength);
}

Ref<ArrayBuffer> ArrayBuffer::createUninitialized(size_t numElements, unsigned elementByteSize)
{
    return create(numElements, elementByteSize, ArrayBufferContents::InitializationPolicy::DontInitialize);
}

RefPtr<ArrayBuffer> ArrayBuffer::tryCreateUninitialized(size_t numElements, unsigned elementByteSize)
{
    return tryCreate(numElements, elementByteSize, std::nullopt, ArrayBufferContents::InitializationPolicy::DontInitialize);
}

Ref<ArrayBuffer> ArrayBuffer::create(size_t numElements, unsigned elementByteSize, ArrayBufferContents::InitializationPolicy policy)
{
    auto buffer = tryCreate(numElements, elementByteSize, std::nullopt, policy);
    if (!buffer)
        CRASH();
    return buffer.releaseNonNull();
}

Ref<ArrayBuffer> ArrayBuffer::createInternal(ArrayBufferContents&& contents, const void* source, size_t byteLength)
{
    auto buffer = adoptRef(*new ArrayBuffer(WTFMove(contents)));
    if (byteLength) {
        ASSERT(source);
        memcpy(buffer->data(), source, byteLength);
    }
    return buffer;
}

RefPtr<ArrayBuffer> ArrayBuffer::tryCreate(size_t numElements, unsigned elementByteSize, std::optional<size_t> maxByteLength, ArrayBufferContents::InitializationPolicy policy)
{
    ArrayBufferContents contents;
    contents.tryAllocate(numElements, elementByteSize, maxByteLength, policy);
    if (!contents.m_data)
        return nullptr;
    return adoptRef(*new ArrayBuffer(WTFMove(contents)));
}

ArrayBuffer::ArrayBuffer(ArrayBufferContents&& contents)
    : m_contents(WTFMove(contents))
{
}

size_t ArrayBuffer::clampValue(double x, size_t left, size_t right)
{
    ASSERT(left <= right);
    if (x < left)
        x = left;
    if (right < x)
        x = right;
    return x;
}

size_t ArrayBuffer::clampIndex(double index) const
{
    size_t currentLength = byteLength();
    if (index < 0)
        index = currentLength + index;
    return clampValue(index, 0, currentLength);
}

RefPtr<ArrayBuffer> ArrayBuffer::slice(double begin, double end) const
{
    return sliceWithClampedIndex(clampIndex(begin), clampIndex(end));
}

RefPtr<ArrayBuffer> ArrayBuffer::slice(double begin) const
{
    return sliceWithClampedIndex(clampIndex(begin), byteLength());
}

RefPtr<ArrayBuffer> ArrayBuffer::sliceWithClampedIndex(size_t begin, size_t end) const
{
    size_t size = begin <= end ? end - begin : 0;
    auto result = ArrayBuffer::tryCreate(static_cast<const char*>(data()) + begin, size);
    if (result)
        result->setSharingMode(sharingMode());
    return result;
}

void ArrayBuffer::makeShared()
{
    m_contents.makeShared();
    m_locked = true;
    ASSERT(!isDetached());
}

void ArrayBuffer::makeWasmMemory()
{
    m_locked = true;
    m_isWasmMemory = true;
}

void ArrayBuffer::setSharingMode(ArrayBufferSharingMode newSharingMode)
{
    if (newSharingMode == sharingMode())
        return;
    RELEASE_ASSERT(!isShared()); // Cannot revert sharing.
    RELEASE_ASSERT(newSharingMode == ArrayBufferSharingMode::Shared);
    makeShared();
}

bool ArrayBuffer::shareWith(ArrayBufferContents& result)
{
    if (!m_contents.m_data || !isShared()) {
        result.m_data = nullptr;
        return false;
    }
    
    m_contents.shareWith(result);
    return true;
}

bool ArrayBuffer::transferTo(VM& vm, ArrayBufferContents& result)
{
    Ref<ArrayBuffer> protect(*this);

    if (!m_contents.m_data) {
        result.m_data = nullptr;
        return false;
    }
    
    if (isShared()) {
        m_contents.shareWith(result);
        return true;
    }

    bool isDetachable = !m_pinCount && !m_locked;

    if (!isDetachable) {
        m_contents.copyTo(result);
        if (!result.m_data)
            return false;
        return true;
    }

    result = WTFMove(m_contents);
    notifyDetaching(vm);
    return true;
}

// We allow detaching wasm memory ArrayBuffers even though they are locked.
void ArrayBuffer::detach(VM& vm)
{
    ASSERT(isWasmMemory());
    ArrayBufferContents unused = WTFMove(m_contents);
    notifyDetaching(vm);
}

void ArrayBuffer::notifyDetaching(VM& vm)
{
    for (size_t i = numberOfIncomingReferences(); i--;) {
        JSCell* cell = incomingReferenceAt(i);
        if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(cell))
            view->detach();
    }
    m_detachingWatchpointSet.fireAll(vm, "Array buffer was detached");
}

Expected<void, GrowFailReason> ArrayBuffer::grow(VM& vm, size_t newByteLength)
{
    auto shared = m_contents.m_shared;
    if (!shared)
        return makeUnexpected(GrowFailReason::GrowSharedUnavailable);
    return shared->grow(vm, newByteLength);
}

template<typename Func>
static bool tryAllocate(VM& vm, const Func& allocate)
{
    unsigned numTries = 2;
    bool success = false;
    for (unsigned i = 0; i < numTries && !success; ++i) {
        switch (allocate()) {
        case BufferMemoryResult::Success:
            success = true;
            break;
        case BufferMemoryResult::SuccessAndNotifyMemoryPressure:
            vm.heap.collectAsync(CollectionScope::Full);
            success = true;
            break;
        case BufferMemoryResult::SyncTryToReclaimMemory:
            if (i + 1 == numTries)
                break;
            vm.heap.collectSync(CollectionScope::Full);
            break;
        }
    }
    return success;
}

RefPtr<ArrayBuffer> ArrayBuffer::tryCreateShared(VM& vm, size_t numElements, unsigned elementByteSize, size_t maxByteLength)
{
    CheckedSize sizeInBytes = numElements;
    sizeInBytes *= elementByteSize;
    if (sizeInBytes.hasOverflowed() || sizeInBytes.value() > MAX_ARRAY_BUFFER_SIZE || (sizeInBytes.value() > maxByteLength))
        return nullptr;

    size_t initialBytes = roundUpToMultipleOf<PageCount::pageSize>(sizeInBytes.value());
    if (!initialBytes)
        initialBytes = PageCount::pageSize; // Make sure malloc actually allocates something, but not too much. We use null to mean that the buffer is detached.
    size_t maximumBytes = roundUpToMultipleOf<PageCount::pageSize>(maxByteLength);
    if (!maximumBytes)
        maximumBytes = PageCount::pageSize; // Make sure malloc actually allocates something, but not too much. We use null to mean that the buffer is detached.

    bool done = tryAllocate(vm,
        [&] () -> BufferMemoryResult::Kind {
            return BufferMemoryManager::singleton().tryAllocatePhysicalBytes(initialBytes);
        });
    if (!done)
        return nullptr;

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
        dataLog("mprotect failed: ", safeStrerror(errno).data(), "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }

    auto handle = adoptRef(*new BufferMemoryHandle(slowMemory, initialBytes, maximumBytes, PageCount::fromBytes(initialBytes), PageCount::fromBytes(maximumBytes), MemorySharingMode::Shared, MemoryMode::BoundsChecking));
    auto content = SharedArrayBufferContents::create(handle->memory(), sizeInBytes.value(), maxByteLength, WTFMove(handle), nullptr, SharedArrayBufferContents::Mode::Default);
    return createShared(WTFMove(content));
}

Expected<void, GrowFailReason> SharedArrayBufferContents::grow(VM& vm, size_t newByteLength)
{
    if (!m_hasMaxByteLength)
        return makeUnexpected(GrowFailReason::GrowSharedUnavailable);
    ASSERT(m_memoryHandle);
    return grow(Locker { m_memoryHandle->lock() }, vm, newByteLength);
}

Expected<void, GrowFailReason> SharedArrayBufferContents::grow(const AbstractLocker&, VM& vm, size_t newByteLength)
{
    // Keep in mind that newByteLength may not be page-size-aligned.
    size_t sizeInBytes = m_sizeInBytes.load(std::memory_order_seq_cst);
    if (sizeInBytes > newByteLength || m_maxByteLength < newByteLength)
        return makeUnexpected(GrowFailReason::InvalidGrowSize);

    if (sizeInBytes == newByteLength)
        return { };

    auto newPageCount = PageCount::fromBytesWithRoundUp(newByteLength);
    auto oldPageCount = PageCount::fromBytes(m_memoryHandle->size()); // MemoryHandle's size is always page-size aligned.
    if (newPageCount.bytes() > MAX_ARRAY_BUFFER_SIZE)
        return makeUnexpected(GrowFailReason::WouldExceedMaximum);

    if (newPageCount != oldPageCount) {
        ASSERT(m_memoryHandle->maximum() >= newPageCount);
        size_t desiredSize = newPageCount.bytes();
        RELEASE_ASSERT(desiredSize <= MAX_ARRAY_BUFFER_SIZE);
        RELEASE_ASSERT(desiredSize > m_memoryHandle->size());

        size_t extraBytes = desiredSize - m_memoryHandle->size();
        RELEASE_ASSERT(extraBytes);
        bool allocationSuccess = tryAllocate(vm,
            [&] () -> BufferMemoryResult::Kind {
                return BufferMemoryManager::singleton().tryAllocatePhysicalBytes(extraBytes);
            });
        if (!allocationSuccess)
            return makeUnexpected(GrowFailReason::OutOfMemory);

        void* memory = m_memoryHandle->memory();
        RELEASE_ASSERT(memory);

        // Signaling memory must have been pre-allocated virtually.
        uint8_t* startAddress = static_cast<uint8_t*>(memory) + m_memoryHandle->size();

        dataLogLnIf(ArrayBufferInternal::verbose, "Marking memory's ", RawPointer(memory), " as read+write in range [", RawPointer(startAddress), ", ", RawPointer(startAddress + extraBytes), ")");
        constexpr bool readable = true;
        constexpr bool writable = true;
        if (!OSAllocator::protect(startAddress, extraBytes, readable, writable)) {
            dataLog("mprotect failed: ", safeStrerror(errno).data(), "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        m_memoryHandle->growToSize(desiredSize);
    }

    growToSize(newByteLength);
    return { };
}

ASCIILiteral errorMesasgeForTransfer(ArrayBuffer* buffer)
{
    ASSERT(buffer->isLocked());
    if (buffer->isShared())
        return "Cannot transfer a SharedArrayBuffer"_s;
    if (buffer->isWasmMemory())
        return "Cannot transfer a WebAssembly.Memory"_s;
    return "Cannot transfer an ArrayBuffer whose backing store has been accessed by the JavaScriptCore C API"_s;
}

} // namespace JSC

