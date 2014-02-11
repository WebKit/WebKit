/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "JSArrayBufferView.h"

#include "JSArrayBuffer.h"
#include "JSCInlines.h"
#include "Reject.h"

namespace JSC {

const ClassInfo JSArrayBufferView::s_info = {
    "ArrayBufferView", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSArrayBufferView)
};

JSArrayBufferView::ConstructionContext::ConstructionContext(
    VM& vm, Structure* structure, uint32_t length, uint32_t elementSize,
    InitializationMode mode)
    : m_structure(0)
    , m_length(length)
    , m_butterfly(0)
{
    if (length <= fastSizeLimit) {
        // Attempt GC allocation.
        void* temp = 0;
        size_t size = sizeOf(length, elementSize);
        // CopiedSpace only allows non-zero size allocations.
        if (size && !vm.heap.tryAllocateStorage(0, size, &temp))
            return;

        m_structure = structure;
        m_vector = temp;
        m_mode = FastTypedArray;

#if USE(JSVALUE32_64)
        if (mode == ZeroFill) {
            uint64_t* asWords = static_cast<uint64_t*>(m_vector);
            for (unsigned i = size / sizeof(uint64_t); i--;)
                asWords[i] = 0;
        }
#endif // USE(JSVALUE32_64)
        
        return;
    }

    // Don't allow a typed array to use more than 2GB.
    if (length > static_cast<unsigned>(INT_MAX) / elementSize)
        return;
    
    if (mode == ZeroFill) {
        if (!tryFastCalloc(length, elementSize).getValue(m_vector))
            return;
    } else {
        if (!tryFastMalloc(length * elementSize).getValue(m_vector))
            return;
    }
    
    vm.heap.reportExtraMemoryCost(static_cast<size_t>(length) * elementSize);
    
    m_structure = structure;
    m_mode = OversizeTypedArray;
}

JSArrayBufferView::ConstructionContext::ConstructionContext(
    VM& vm, Structure* structure, PassRefPtr<ArrayBuffer> arrayBuffer,
    unsigned byteOffset, unsigned length)
    : m_structure(structure)
    , m_vector(static_cast<uint8_t*>(arrayBuffer->data()) + byteOffset)
    , m_length(length)
    , m_mode(WastefulTypedArray)
{
    IndexingHeader indexingHeader;
    indexingHeader.setArrayBuffer(arrayBuffer.get());
    m_butterfly = Butterfly::create(vm, 0, 0, 0, true, indexingHeader, 0);
}

JSArrayBufferView::ConstructionContext::ConstructionContext(
    Structure* structure, PassRefPtr<ArrayBuffer> arrayBuffer,
    unsigned byteOffset, unsigned length, DataViewTag)
    : m_structure(structure)
    , m_vector(static_cast<uint8_t*>(arrayBuffer->data()) + byteOffset)
    , m_length(length)
    , m_mode(DataViewMode)
    , m_butterfly(0)
{
}

JSArrayBufferView::JSArrayBufferView(VM& vm, ConstructionContext& context)
    : Base(vm, context.structure(), context.butterfly())
    , m_vector(context.vector())
    , m_length(context.length())
    , m_mode(context.mode())
{
}

void JSArrayBufferView::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    switch (m_mode) {
    case FastTypedArray:
        return;
    case OversizeTypedArray:
        vm.heap.addFinalizer(this, finalize);
        return;
    case WastefulTypedArray:
        vm.heap.addReference(this, butterfly()->indexingHeader()->arrayBuffer());
        return;
    case DataViewMode:
        ASSERT(!butterfly());
        vm.heap.addReference(this, jsCast<JSDataView*>(this)->buffer());
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool JSArrayBufferView::getOwnPropertySlot(
    JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSArrayBufferView* thisObject = jsCast<JSArrayBufferView*>(object);
    if (propertyName == exec->propertyNames().byteOffset) {
        slot.setValue(thisObject, DontDelete | ReadOnly, jsNumber(thisObject->byteOffset()));
        return true;
    }
    
    if (propertyName == exec->propertyNames().buffer) {
        slot.setValue(
            thisObject, DontDelete | ReadOnly, exec->vm().m_typedArrayController->toJS(
                exec, thisObject->globalObject(), thisObject->buffer()));
        return true;
    }
    
    return Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

void JSArrayBufferView::put(
    JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value,
    PutPropertySlot& slot)
{
    JSArrayBufferView* thisObject = jsCast<JSArrayBufferView*>(cell);
    if (propertyName == exec->propertyNames().byteLength
        || propertyName == exec->propertyNames().byteOffset
        || propertyName == exec->propertyNames().buffer) {
        reject(exec, slot.isStrictMode(), "Attempting to write to read-only typed array property.");
        return;
    }
    
    Base::put(thisObject, exec, propertyName, value, slot);
}

bool JSArrayBufferView::defineOwnProperty(
    JSObject* object, ExecState* exec, PropertyName propertyName,
    const PropertyDescriptor& descriptor, bool shouldThrow)
{
    JSArrayBufferView* thisObject = jsCast<JSArrayBufferView*>(object);
    if (propertyName == exec->propertyNames().byteLength
        || propertyName == exec->propertyNames().byteOffset
        || propertyName == exec->propertyNames().buffer)
        return reject(exec, shouldThrow, "Attempting to define read-only typed array property.");
    
    return Base::defineOwnProperty(thisObject, exec, propertyName, descriptor, shouldThrow);
}

bool JSArrayBufferView::deleteProperty(
    JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    JSArrayBufferView* thisObject = jsCast<JSArrayBufferView*>(cell);
    if (propertyName == exec->propertyNames().byteLength
        || propertyName == exec->propertyNames().byteOffset
        || propertyName == exec->propertyNames().buffer)
        return false;
    
    return Base::deleteProperty(thisObject, exec, propertyName);
}

void JSArrayBufferView::getOwnNonIndexPropertyNames(
    JSObject* object, ExecState* exec, PropertyNameArray& array, EnumerationMode mode)
{
    JSArrayBufferView* thisObject = jsCast<JSArrayBufferView*>(object);
    
    // length/byteOffset/byteLength are DontEnum, at least in Firefox.
    if (mode == IncludeDontEnumProperties) {
        array.add(exec->propertyNames().byteOffset);
        array.add(exec->propertyNames().byteLength);
        array.add(exec->propertyNames().buffer);
    }
    
    Base::getOwnNonIndexPropertyNames(thisObject, exec, array, mode);
}

void JSArrayBufferView::finalize(JSCell* cell)
{
    JSArrayBufferView* thisObject = static_cast<JSArrayBufferView*>(cell);
    ASSERT(thisObject->m_mode == OversizeTypedArray || thisObject->m_mode == WastefulTypedArray);
    if (thisObject->m_mode == OversizeTypedArray)
        fastFree(thisObject->m_vector);
}

} // namespace JSC

