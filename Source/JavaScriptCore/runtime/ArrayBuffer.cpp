/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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
#include "JSCInlines.h"
#include <wtf/Gigacage.h>

namespace JSC {

Ref<SharedTask<void(void*)>> ArrayBuffer::primitiveGigacageDestructor()
{
    static LazyNeverDestroyed<Ref<SharedTask<void(void*)>>> destructor;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        destructor.construct(createSharedTask<void(void*)>([] (void* p) { Gigacage::free(Gigacage::Primitive, p); }));
    });
    return destructor.get().copyRef();
}

SharedArrayBufferContents::SharedArrayBufferContents(void* data, unsigned size, ArrayBufferDestructorFunction&& destructor)
    : m_data(data, size)
    , m_destructor(WTFMove(destructor))
    , m_sizeInBytes(size)
{
}

SharedArrayBufferContents::~SharedArrayBufferContents()
{
    if (m_destructor) {
        // FIXME: we shouldn't use getUnsafe here https://bugs.webkit.org/show_bug.cgi?id=197698
        m_destructor->run(m_data.getUnsafe());
    }
}

ArrayBufferContents::ArrayBufferContents()
{
    reset();
}

ArrayBufferContents::ArrayBufferContents(ArrayBufferContents&& other)
{
    reset();
    other.transferTo(*this);
}

ArrayBufferContents::ArrayBufferContents(void* data, unsigned sizeInBytes, ArrayBufferDestructorFunction&& destructor)
    : m_data(data, sizeInBytes)
    , m_sizeInBytes(sizeInBytes)
{
    RELEASE_ASSERT(m_sizeInBytes <= MAX_ARRAY_BUFFER_SIZE);
    m_destructor = WTFMove(destructor);
}

ArrayBufferContents& ArrayBufferContents::operator=(ArrayBufferContents&& other)
{
    other.transferTo(*this);
    return *this;
}

ArrayBufferContents::~ArrayBufferContents()
{
    destroy();
}

void ArrayBufferContents::clear()
{
    destroy();
    reset();
}

void ArrayBufferContents::destroy()
{
    if (m_destructor) {
        // FIXME: We shouldn't use getUnsafe here: https://bugs.webkit.org/show_bug.cgi?id=197698
        m_destructor->run(m_data.getUnsafe());
    }
}

void ArrayBufferContents::reset()
{
    m_data = nullptr;
    m_destructor = nullptr;
    m_shared = nullptr;
    m_sizeInBytes = 0;
}

void ArrayBufferContents::tryAllocate(unsigned numElements, unsigned elementByteSize, InitializationPolicy policy)
{
    // Do not allow 31-bit overflow of the total size.
    if (numElements) {
        unsigned totalSize = numElements * elementByteSize;
        if (totalSize / numElements != elementByteSize || totalSize > MAX_ARRAY_BUFFER_SIZE) {
            reset();
            return;
        }
    }
    size_t sizeInBytes = static_cast<size_t>(numElements) * static_cast<size_t>(elementByteSize);
    size_t allocationSize = sizeInBytes;
    if (!allocationSize)
        allocationSize = 1; // Make sure malloc actually allocates something, but not too much. We use null to mean that the buffer is neutered.

    void* data = Gigacage::tryMalloc(Gigacage::Primitive, allocationSize);
    m_data = DataType(data, sizeInBytes);
    if (!data) {
        reset();
        return;
    }
    
    if (policy == ZeroInitialize)
        memset(data, 0, allocationSize);

    m_sizeInBytes = sizeInBytes;
    RELEASE_ASSERT(m_sizeInBytes <= MAX_ARRAY_BUFFER_SIZE);
    m_destructor = ArrayBuffer::primitiveGigacageDestructor();
}

void ArrayBufferContents::makeShared()
{
    m_shared = adoptRef(new SharedArrayBufferContents(data(), sizeInBytes(), WTFMove(m_destructor)));
    m_destructor = nullptr;
}

void ArrayBufferContents::transferTo(ArrayBufferContents& other)
{
    other.clear();
    other.m_data = m_data;
    other.m_sizeInBytes = m_sizeInBytes;
    RELEASE_ASSERT(other.m_sizeInBytes <= MAX_ARRAY_BUFFER_SIZE);
    other.m_destructor = WTFMove(m_destructor);
    other.m_shared = m_shared;
    reset();
}

void ArrayBufferContents::copyTo(ArrayBufferContents& other)
{
    ASSERT(!other.m_data);
    other.tryAllocate(m_sizeInBytes, sizeof(char), ArrayBufferContents::DontInitialize);
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
    RELEASE_ASSERT(other.m_sizeInBytes <= MAX_ARRAY_BUFFER_SIZE);
}

Ref<ArrayBuffer> ArrayBuffer::create(unsigned numElements, unsigned elementByteSize)
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

Ref<ArrayBuffer> ArrayBuffer::create(const void* source, unsigned byteLength)
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
Ref<ArrayBuffer> ArrayBuffer::createAdopted(const void* data, unsigned byteLength)
{
    return createFromBytes(data, byteLength, ArrayBuffer::primitiveGigacageDestructor());
}

// FIXME: We cannot use this except if the memory comes from the cage.
// Currently this is only used from:
// - The C API. We could support that by either having the system switch to a mode where typed arrays are no
//   longer caged, or we could introduce a new set of typed array types that are uncaged and get accessed
//   differently.
// - WebAssembly. Wasm should allocate from the cage.
Ref<ArrayBuffer> ArrayBuffer::createFromBytes(const void* data, unsigned byteLength, ArrayBufferDestructorFunction&& destructor)
{
    if (data && !Gigacage::isCaged(Gigacage::Primitive, data))
        Gigacage::disablePrimitiveGigacage();
    
    ArrayBufferContents contents(const_cast<void*>(data), byteLength, WTFMove(destructor));
    return create(WTFMove(contents));
}

RefPtr<ArrayBuffer> ArrayBuffer::tryCreate(unsigned numElements, unsigned elementByteSize)
{
    return tryCreate(numElements, elementByteSize, ArrayBufferContents::ZeroInitialize);
}

RefPtr<ArrayBuffer> ArrayBuffer::tryCreate(ArrayBuffer& other)
{
    return tryCreate(other.data(), other.byteLength());
}

RefPtr<ArrayBuffer> ArrayBuffer::tryCreate(const void* source, unsigned byteLength)
{
    ArrayBufferContents contents;
    contents.tryAllocate(byteLength, 1, ArrayBufferContents::DontInitialize);
    if (!contents.m_data)
        return nullptr;
    return createInternal(WTFMove(contents), source, byteLength);
}

Ref<ArrayBuffer> ArrayBuffer::createUninitialized(unsigned numElements, unsigned elementByteSize)
{
    return create(numElements, elementByteSize, ArrayBufferContents::DontInitialize);
}

RefPtr<ArrayBuffer> ArrayBuffer::tryCreateUninitialized(unsigned numElements, unsigned elementByteSize)
{
    return tryCreate(numElements, elementByteSize, ArrayBufferContents::DontInitialize);
}

Ref<ArrayBuffer> ArrayBuffer::create(unsigned numElements, unsigned elementByteSize, ArrayBufferContents::InitializationPolicy policy)
{
    auto buffer = tryCreate(numElements, elementByteSize, policy);
    if (!buffer)
        CRASH();
    return buffer.releaseNonNull();
}

Ref<ArrayBuffer> ArrayBuffer::createInternal(ArrayBufferContents&& contents, const void* source, unsigned byteLength)
{
    ASSERT(!byteLength || source);
    auto buffer = adoptRef(*new ArrayBuffer(WTFMove(contents)));
    memcpy(buffer->data(), source, byteLength);
    return buffer;
}

RefPtr<ArrayBuffer> ArrayBuffer::tryCreate(unsigned numElements, unsigned elementByteSize, ArrayBufferContents::InitializationPolicy policy)
{
    ArrayBufferContents contents;
    contents.tryAllocate(numElements, elementByteSize, policy);
    if (!contents.m_data)
        return nullptr;
    return adoptRef(*new ArrayBuffer(WTFMove(contents)));
}

ArrayBuffer::ArrayBuffer(ArrayBufferContents&& contents)
    : m_contents(WTFMove(contents))
    , m_pinCount(0)
    , m_isWasmMemory(false)
    , m_locked(false)
{
}

unsigned ArrayBuffer::clampValue(double x, unsigned left, unsigned right)
{
    ASSERT(left <= right);
    if (x < left)
        x = left;
    if (right < x)
        x = right;
    return x;
}

unsigned ArrayBuffer::clampIndex(double index) const
{
    unsigned currentLength = byteLength();
    if (index < 0)
        index = currentLength + index;
    return clampValue(index, 0, currentLength);
}

RefPtr<ArrayBuffer> ArrayBuffer::slice(double begin, double end) const
{
    return sliceImpl(clampIndex(begin), clampIndex(end));
}

RefPtr<ArrayBuffer> ArrayBuffer::slice(double begin) const
{
    return sliceImpl(clampIndex(begin), byteLength());
}

RefPtr<ArrayBuffer> ArrayBuffer::sliceImpl(unsigned begin, unsigned end) const
{
    unsigned size = begin <= end ? end - begin : 0;
    auto result = ArrayBuffer::tryCreate(static_cast<const char*>(data()) + begin, size);
    if (result)
        result->setSharingMode(sharingMode());
    return result;
}

void ArrayBuffer::makeShared()
{
    m_contents.makeShared();
    m_locked = true;
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

    bool isNeuterable = !m_pinCount && !m_locked;

    if (!isNeuterable) {
        m_contents.copyTo(result);
        if (!result.m_data)
            return false;
        return true;
    }

    m_contents.transferTo(result);
    notifyNeutering(vm);
    return true;
}

// We allow neutering wasm memory ArrayBuffers even though they are locked.
void ArrayBuffer::neuter(VM& vm)
{
    ASSERT(isWasmMemory());
    ArrayBufferContents unused;
    m_contents.transferTo(unused);
    notifyNeutering(vm);
}

void ArrayBuffer::notifyNeutering(VM& vm)
{
    for (size_t i = numberOfIncomingReferences(); i--;) {
        JSCell* cell = incomingReferenceAt(i);
        if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(vm, cell))
            view->neuter();
    }
    m_neuteringWatchpointSet.fireAll(vm, "Array buffer was neutered");
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

