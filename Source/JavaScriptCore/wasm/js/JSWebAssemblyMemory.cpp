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
#include "JSWebAssemblyMemory.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"

#include "ArrayBuffer.h"
#include "JSArrayBuffer.h"

namespace JSC {

const ClassInfo JSWebAssemblyMemory::s_info = { "WebAssembly.Memory", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyMemory) };

JSWebAssemblyMemory* JSWebAssemblyMemory::create(ExecState* exec, VM& vm, Structure* structure)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* globalObject = exec->lexicalGlobalObject();

    auto exception = [&] (JSObject* error) {
        throwException(exec, throwScope, error);
        return nullptr;
    };

    if (!globalObject->webAssemblyEnabled())
        return exception(createEvalError(exec, globalObject->webAssemblyDisabledErrorMessage()));

    auto* memory = new (NotNull, allocateCell<JSWebAssemblyMemory>(vm.heap)) JSWebAssemblyMemory(vm, structure);
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

JSArrayBuffer* JSWebAssemblyMemory::buffer(VM& vm, JSGlobalObject* globalObject)
{
    if (m_bufferWrapper)
        return m_bufferWrapper.get();

    // We can't use a ref here since it doesn't have a copy constructor...
    Ref<Wasm::Memory> protectedMemory = m_memory.get();
    auto destructor = [protectedMemory = WTFMove(protectedMemory)] (void*) { };
    m_buffer = ArrayBuffer::createFromBytes(memory().memory(), memory().size(), WTFMove(destructor));
    m_buffer->makeWasmMemory();
    m_bufferWrapper.set(vm, this, JSArrayBuffer::create(vm, globalObject->arrayBufferStructure(ArrayBufferSharingMode::Default), m_buffer.get()));
    RELEASE_ASSERT(m_bufferWrapper);
    return m_bufferWrapper.get();
}

Wasm::PageCount JSWebAssemblyMemory::grow(VM& vm, ExecState* exec, uint32_t delta)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto grown = memory().grow(Wasm::PageCount(delta));
    if (!grown) {
        switch (grown.error()) {
        case Wasm::Memory::GrowFailReason::InvalidDelta:
            throwException(exec, throwScope, createRangeError(exec, "WebAssembly.Memory.grow expects the delta to be a valid page count"_s));
            break;
        case Wasm::Memory::GrowFailReason::InvalidGrowSize:
            throwException(exec, throwScope, createRangeError(exec, "WebAssembly.Memory.grow expects the grown size to be a valid page count"_s));
            break;
        case Wasm::Memory::GrowFailReason::WouldExceedMaximum:
            throwException(exec, throwScope, createRangeError(exec, "WebAssembly.Memory.grow would exceed the memory's declared maximum size"_s));
            break;
        case Wasm::Memory::GrowFailReason::OutOfMemory:
            throwException(exec, throwScope, createOutOfMemoryError(exec));
            break;
        }
        return Wasm::PageCount();
    }

    return grown.value();
}

void JSWebAssemblyMemory::growSuccessCallback(VM& vm, Wasm::PageCount oldPageCount, Wasm::PageCount newPageCount)
{
    // We need to clear out the old array buffer because it might now be pointing to stale memory.
    // Neuter the old array.
    if (m_buffer) {
        m_buffer->neuter(vm);
        m_buffer = nullptr;
        m_bufferWrapper.clear();
    }
    
    memory().check();
    
    vm.heap.reportExtraMemoryAllocated(newPageCount.bytes() - oldPageCount.bytes());
}

void JSWebAssemblyMemory::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    vm.heap.reportExtraMemoryAllocated(memory().size());
}

void JSWebAssemblyMemory::destroy(JSCell* cell)
{
    auto memory = static_cast<JSWebAssemblyMemory*>(cell);
    ASSERT(memory->classInfo() == info());

    memory->JSWebAssemblyMemory::~JSWebAssemblyMemory();
}

void JSWebAssemblyMemory::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<JSWebAssemblyMemory*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_bufferWrapper);
    visitor.reportExtraMemoryVisited(thisObject->memory().size());
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
