/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyMemory.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"

#include "ArrayBuffer.h"
#include "JSArrayBuffer.h"
#include "ObjectConstructor.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

const ClassInfo JSWebAssemblyMemory::s_info = { "WebAssembly.Memory"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyMemory) };

JSWebAssemblyMemory* JSWebAssemblyMemory::create(VM& vm, Structure* structure)
{
    auto* memory = new (NotNull, allocateCell<JSWebAssemblyMemory>(vm)) JSWebAssemblyMemory(vm, structure);
    memory->finishCreation(vm);
    return memory;
}
    
void JSWebAssemblyMemory::adopt(Ref<Wasm::Memory>&& memory)
{
    m_memory.swap(memory);
    ASSERT(m_memory->refCount() == 1);
    m_memory->checkLifetime();
}

Structure* JSWebAssemblyMemory::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSWebAssemblyMemory::JSWebAssemblyMemory(VM& vm, Structure* structure)
    : Base(vm, structure)
    , m_memory(Wasm::Memory::create(vm))
{
}

void JSWebAssemblyMemory::associateArrayBuffer(JSGlobalObject* globalObject, bool shouldBeFixedLength)
{
    ASSERT(!m_buffer);
    ASSERT(!m_bufferWrapper);
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (m_memory->sharingMode() == MemorySharingMode::Shared && m_memory->shared())
        m_buffer = ArrayBuffer::createShared(*m_memory->shared(), shouldBeFixedLength);
    else {
        Ref<BufferMemoryHandle> protectedHandle = m_memory->handle();
        void* data = m_memory->basePointer();
        size_t size = m_memory->size();
        ASSERT(data);
        if (shouldBeFixedLength) {
            auto destructor = createSharedTask<void(void*)>([protectedHandle = WTFMove(protectedHandle)] (void*) { });
            m_buffer = ArrayBuffer::createFromBytes({ static_cast<const uint8_t*>(data), size }, WTFMove(destructor));
        } else {
            // The determination of maxByteLength of a resizable non-shared array buffer may change in
            // https://webassembly.github.io/threads/js-api/index.html#create-a-resizable-memory-buffer
            // Currently we are implementing the behavior expected by WPT tests,
            // so that maxByteLength is 2^32 if the memory has no user-defined max size.
#if USE(LARGE_TYPED_ARRAYS)
            // If sizeof(size_t) == 8 we use the proper spec value because it's representable.
            constexpr size_t defaultMaxByteLengthIfMemoryHasNoMax = 65536ULL * 65536ULL;
#else
            // If sizeof(size_t) == 4, compute the largest page-aligned size that fits within MAX_ARRAY_BUFFER_SIZE.
            uint32_t maxPages = MAX_ARRAY_BUFFER_SIZE / PageCount::pageSize;
            const size_t defaultMaxByteLengthIfMemoryHasNoMax = static_cast<size_t>(PageCount(maxPages).bytes());
#endif
            PageCount memoryMax = m_memory->maximum();
            size_t maxByteLength = memoryMax.isValid() ? memoryMax.bytes() : defaultMaxByteLengthIfMemoryHasNoMax;
            ArrayBufferContents contents(data, size, maxByteLength, WTFMove(protectedHandle));
            m_buffer = ArrayBuffer::create(WTFMove(contents));
        }
        if (m_memory->sharingMode() == MemorySharingMode::Shared)
            m_buffer->makeShared();
    }
    m_buffer->makeWasmMemory();
    if (m_buffer->isResizableNonShared())
        m_buffer->setAssociatedWasmMemory(m_memory.ptr());

    auto* arrayBuffer = JSArrayBuffer::create(vm, globalObject->arrayBufferStructure(m_buffer->sharingMode()), m_buffer.get());
    if (m_memory->sharingMode() == MemorySharingMode::Shared) {
        objectConstructorFreeze(globalObject, arrayBuffer);
        RETURN_IF_EXCEPTION(throwScope, void());
    }

    m_bufferWrapper.set(vm, this, arrayBuffer);
    RELEASE_ASSERT(m_bufferWrapper);
}

void JSWebAssemblyMemory::disassociateArrayBuffer(VM& vm)
{
    ASSERT(m_buffer);
    if (!m_buffer->isShared())
        m_buffer->detach(vm);
    m_buffer->setAssociatedWasmMemory(nullptr);
    m_buffer = nullptr;
    m_bufferWrapper.clear();
}

// https://webassembly.github.io/threads/js-api/index.html#dom-memory-buffer
JSArrayBuffer* JSWebAssemblyMemory::buffer(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (Options::useWasmMemoryToBufferAPIs()) {
        if (auto* wrapper = m_bufferWrapper.get()) {
            // If SharedArrayBuffer's underlying memory is grown by another thread, we must refresh.
            if (wrapper->impl()->byteLength() != memory().size())
                disassociateArrayBuffer(vm);
        }

        if (!m_buffer) {
            associateArrayBuffer(globalObject, true);
            RETURN_IF_EXCEPTION(throwScope, { });
        }

        RELEASE_ASSERT(m_bufferWrapper);
        return m_bufferWrapper.get();
    }

    // Historical behavior prior to the resizable SAB change follows
    // Remove when the feature is permanent

    auto* wrapper = m_bufferWrapper.get();
    if (wrapper) {
        // If SharedArrayBuffer's underlying memory is not grown, we continue using cached wrapper.
        if (wrapper->impl()->byteLength() == memory().size())
            return wrapper;
    }

    if (m_memory->sharingMode() == MemorySharingMode::Shared && m_memory->shared()) {
        m_buffer = ArrayBuffer::createShared(*m_memory->shared());
        m_buffer->makeWasmMemory();
    } else {
        Ref<BufferMemoryHandle> protectedHandle = m_memory->handle();
        void* memory = m_memory->basePointer();
        size_t size = m_memory->size();
        ASSERT(memory);
        auto destructor = createSharedTask<void(void*)>([protectedHandle = WTFMove(protectedHandle)] (void*) { });
        m_buffer = ArrayBuffer::createFromBytes({ static_cast<const uint8_t*>(memory), size }, WTFMove(destructor));
        m_buffer->makeWasmMemory();
        if (m_memory->sharingMode() == MemorySharingMode::Shared)
            m_buffer->makeShared();
    }

    auto* arrayBuffer = JSArrayBuffer::create(vm, globalObject->arrayBufferStructure(m_buffer->sharingMode()), m_buffer.get());
    if (m_memory->sharingMode() == MemorySharingMode::Shared) {
        objectConstructorFreeze(globalObject, arrayBuffer);
        RETURN_IF_EXCEPTION(throwScope, { });
    }

    m_bufferWrapper.set(vm, this, arrayBuffer);
    RELEASE_ASSERT(m_bufferWrapper);
    return m_bufferWrapper.get();

}

// https://webassembly.github.io/threads/js-api/index.html#dom-memory-tofixedlengthbuffer
JSArrayBuffer* JSWebAssemblyMemory::toFixedLengthBuffer(JSGlobalObject* globalObject)
{
    ASSERT(Options::useWasmMemoryToBufferAPIs());
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (!m_buffer) {
        associateArrayBuffer(globalObject, true);
        RETURN_IF_EXCEPTION(throwScope, { });
    } else if (!m_buffer->isFixedLength()) {
        disassociateArrayBuffer(vm);
        associateArrayBuffer(globalObject, true);
        RETURN_IF_EXCEPTION(throwScope, { });
    }

    RELEASE_ASSERT(m_bufferWrapper);
    return m_bufferWrapper.get();
}

// https://webassembly.github.io/threads/js-api/index.html#dom-memory-toresizablebuffer
JSArrayBuffer* JSWebAssemblyMemory::toResizableBuffer(JSGlobalObject* globalObject)
{
    ASSERT(Options::useWasmMemoryToBufferAPIs());
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (!m_buffer) {
        associateArrayBuffer(globalObject, false);
        RETURN_IF_EXCEPTION(throwScope, { });
    } else if (m_buffer->isFixedLength()) {
        disassociateArrayBuffer(vm);
        associateArrayBuffer(globalObject, false);
        RETURN_IF_EXCEPTION(throwScope, { });
    }

    RELEASE_ASSERT(m_bufferWrapper);
    return m_bufferWrapper.get();
}

PageCount JSWebAssemblyMemory::grow(VM& vm, JSGlobalObject* globalObject, uint32_t delta)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto grown = memory().grow(vm, PageCount(delta)); // calls growSuccessCallback() after growing
    if (!grown) {
        switch (grown.error()) {
        case GrowFailReason::InvalidDelta:
            throwException(globalObject, throwScope, createRangeError(globalObject, "WebAssembly.Memory.grow expects the delta to be a valid page count"_s));
            break;
        case GrowFailReason::InvalidGrowSize:
            throwException(globalObject, throwScope, createRangeError(globalObject, "WebAssembly.Memory.grow expects the grown size to be a valid page count"_s));
            break;
        case GrowFailReason::WouldExceedMaximum:
            throwException(globalObject, throwScope, createRangeError(globalObject, "WebAssembly.Memory.grow would exceed the memory's declared maximum size"_s));
            break;
        case GrowFailReason::OutOfMemory:
            throwException(globalObject, throwScope, createOutOfMemoryError(globalObject));
            break;
        case GrowFailReason::GrowSharedUnavailable:
            throwException(globalObject, throwScope, createRangeError(globalObject, "WebAssembly.Memory.grow for shared memory is unavailable"_s));
            break;
        }
        return PageCount();
    }

    return grown.value();
}

JSObject* JSWebAssemblyMemory::type(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();

    PageCount minimum = m_memory->initial();
    PageCount maximum = m_memory->maximum();

    JSObject* result;
    if (maximum.isValid()) {
        result = constructEmptyObject(globalObject, globalObject->objectPrototype(), 3);
        result->putDirect(vm, Identifier::fromString(vm, "maximum"_s), jsNumber(maximum.pageCount()));
    } else
        result = constructEmptyObject(globalObject, globalObject->objectPrototype(), 2);

    result->putDirect(vm, Identifier::fromString(vm, "minimum"_s), jsNumber(minimum.pageCount()));
    result->putDirect(vm, Identifier::fromString(vm, "shared"_s), jsBoolean(m_memory->sharingMode() == MemorySharingMode::Shared));

    return result;
}


void JSWebAssemblyMemory::growSuccessCallback(VM& vm, PageCount oldPageCount, PageCount newPageCount)
{
    if (m_buffer) {
        if (Options::useWasmMemoryToBufferAPIs()) {
            // https://webassembly.github.io/threads/js-api/index.html#refresh-the-memory-buffer
            // Fixed length buffers are "refreshed" by discarding them, so an updated one is created lazily.
            // Shared growable buffers are always fresh because growing is handled by their SharedArrayBufferContents.
            // Non-shared resizable buffers need to be refreshed explicitly.
            if (m_buffer->isFixedLength())
                disassociateArrayBuffer(vm);
            else if (!m_buffer->isShared())
                m_buffer->refreshAfterWasmMemoryGrow(m_memory.ptr());
        } else {
            // historical behavior before the SAB feature:
            // clear out the old array buffer because it might now be pointing to stale memory.
            if (m_memory->sharingMode() == MemorySharingMode::Default)
                m_buffer->detach(vm);
            m_buffer = nullptr;
            m_bufferWrapper.clear();
        }
    }

    
    memory().checkLifetime();
    
    vm.heap.reportExtraMemoryAllocated(this, newPageCount.bytes() - oldPageCount.bytes());
}

void JSWebAssemblyMemory::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    vm.heap.reportExtraMemoryAllocated(this, memory().size());
}

void JSWebAssemblyMemory::destroy(JSCell* cell)
{
    auto memory = static_cast<JSWebAssemblyMemory*>(cell);
    memory->JSWebAssemblyMemory::~JSWebAssemblyMemory();
}

template<typename Visitor>
void JSWebAssemblyMemory::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSWebAssemblyMemory*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_bufferWrapper);
    visitor.reportExtraMemoryVisited(thisObject->memory().size());
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyMemory);

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
