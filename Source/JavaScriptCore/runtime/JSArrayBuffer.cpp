/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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
#include "JSArrayBuffer.h"

#include "JSCInlines.h"
#include "TypeError.h"
#include "TypedArrayController.h"

namespace JSC {

const ClassInfo JSArrayBuffer::s_info = {
    "ArrayBuffer", &Base::s_info, 0, CREATE_METHOD_TABLE(JSArrayBuffer)};

JSArrayBuffer::JSArrayBuffer(VM& vm, Structure* structure, PassRefPtr<ArrayBuffer> arrayBuffer)
    : Base(vm, structure)
    , m_impl(arrayBuffer.get())
{
}

void JSArrayBuffer::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    // This probably causes GCs in the various VMs to overcount the impact of the array buffer.
    vm.heap.addReference(this, m_impl);
    vm.m_typedArrayController->registerWrapper(globalObject, m_impl, this);
}

JSArrayBuffer* JSArrayBuffer::create(
    VM& vm, Structure* structure, PassRefPtr<ArrayBuffer> passedBuffer)
{
    RefPtr<ArrayBuffer> buffer = passedBuffer;
    JSArrayBuffer* result =
        new (NotNull, allocateCell<JSArrayBuffer>(vm.heap))
        JSArrayBuffer(vm, structure, buffer);
    result->finishCreation(vm, structure->globalObject());
    return result;
}

Structure* JSArrayBuffer::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info(),
        NonArray);
}

bool JSArrayBuffer::isShared() const
{
    return m_impl->isShared();
}

ArrayBufferSharingMode JSArrayBuffer::sharingMode() const
{
    return m_impl->sharingMode();
}

size_t JSArrayBuffer::estimatedSize(JSCell* cell)
{
    JSArrayBuffer* thisObject = jsCast<JSArrayBuffer*>(cell);
    size_t bufferEstimatedSize = thisObject->impl()->gcSizeEstimateInBytes();
    return Base::estimatedSize(cell) + bufferEstimatedSize;
}

bool JSArrayBuffer::getOwnPropertySlot(
    JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSArrayBuffer* thisObject = jsCast<JSArrayBuffer*>(object);
    
    if (propertyName == exec->propertyNames().byteLength) {
        slot.setValue(thisObject, DontDelete | ReadOnly, jsNumber(thisObject->impl()->byteLength()));
        return true;
    }
    
    return Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

bool JSArrayBuffer::put(
    JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value,
    PutPropertySlot& slot)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSArrayBuffer* thisObject = jsCast<JSArrayBuffer*>(cell);

    if (UNLIKELY(isThisValueAltered(slot, thisObject))) {
        scope.release();
        return ordinarySetSlow(exec, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode());
    }
    
    if (propertyName == vm.propertyNames->byteLength)
        return typeError(exec, scope, slot.isStrictMode(), ASCIILiteral("Attempting to write to a read-only array buffer property."));

    scope.release();
    return Base::put(thisObject, exec, propertyName, value, slot);
}

bool JSArrayBuffer::defineOwnProperty(
    JSObject* object, ExecState* exec, PropertyName propertyName,
    const PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSArrayBuffer* thisObject = jsCast<JSArrayBuffer*>(object);
    
    if (propertyName == vm.propertyNames->byteLength)
        return typeError(exec, scope, shouldThrow, ASCIILiteral("Attempting to define read-only array buffer property."));

    scope.release();
    return Base::defineOwnProperty(thisObject, exec, propertyName, descriptor, shouldThrow);
}

bool JSArrayBuffer::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    JSArrayBuffer* thisObject = jsCast<JSArrayBuffer*>(cell);
    
    if (propertyName == exec->propertyNames().byteLength)
        return false;
    
    return Base::deleteProperty(thisObject, exec, propertyName);
}

void JSArrayBuffer::getOwnNonIndexPropertyNames(
    JSObject* object, ExecState* exec, PropertyNameArray& array, EnumerationMode mode)
{
    JSArrayBuffer* thisObject = jsCast<JSArrayBuffer*>(object);
    
    if (mode.includeDontEnumProperties())
        array.add(exec->propertyNames().byteLength);
    
    Base::getOwnNonIndexPropertyNames(thisObject, exec, array, mode);
}

} // namespace JSC

