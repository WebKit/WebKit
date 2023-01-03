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

namespace JSC {

const ClassInfo JSWebAssemblyMemory::s_info = { "WebAssembly.Memory"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyMemory) };

JSWebAssemblyMemory* JSWebAssemblyMemory::tryCreate(JSGlobalObject* globalObject, VM& vm, Structure* structure)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto exception = [&] (JSObject* error) {
        throwException(globalObject, throwScope, error);
        return nullptr;
    };

    if (!globalObject->webAssemblyEnabled())
        return exception(createEvalError(globalObject, globalObject->webAssemblyDisabledErrorMessage()));

    auto* memory = new (NotNull, allocateCell<JSWebAssemblyMemory>(vm)) JSWebAssemblyMemory(vm, structure);
    memory->finishCreation(vm);
    return memory;
}
    
void JSWebAssemblyMemory::adopt(Ref<Wasm::Memory>&& memory)
{
    m_memory.swap(memory);
    ASSERT(m_memory->refCount() == 1);
    m_memory->check();
}

Structure* JSWebAssemblyMemory::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSWebAssemblyMemory::JSWebAssemblyMemory(VM& vm, Structure* structure)
    : Base(vm, structure)
    , m_memory(Wasm::Memory::create())
{
}

JSArrayBuffer* JSWebAssemblyMemory::buffer(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

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
        m_buffer = ArrayBuffer::createFromBytes(memory, size, WTFMove(destructor));
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

PageCount JSWebAssemblyMemory::grow(VM& vm, JSGlobalObject* globalObject, uint32_t delta)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto grown = memory().grow(vm, PageCount(delta));
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
    // We need to clear out the old array buffer because it might now be pointing to stale memory.
    if (m_buffer) {
        if (m_memory->sharingMode() == MemorySharingMode::Default)
            m_buffer->detach(vm);
        m_buffer = nullptr;
        m_bufferWrapper.clear();
    }
    
    memory().check();
    
    vm.heap.reportExtraMemoryAllocated(newPageCount.bytes() - oldPageCount.bytes());
}

void JSWebAssemblyMemory::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    vm.heap.reportExtraMemoryAllocated(memory().size());
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

#endif // ENABLE(WEBASSEMBLY)
