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
#include "JSDataView.h"

#include "ArrayBufferView.h"
#include "DataView.h"
#include "Error.h"
#include "JSCInlines.h"

namespace JSC {

const ClassInfo JSDataView::s_info = {
    "DataView", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSDataView)};

JSDataView::JSDataView(VM& vm, ConstructionContext& context, ArrayBuffer* buffer)
    : Base(vm, context)
    , m_buffer(buffer)
{
}

JSDataView* JSDataView::create(
    ExecState* exec, Structure* structure, PassRefPtr<ArrayBuffer> passedBuffer,
    unsigned byteOffset, unsigned byteLength)
{
    RefPtr<ArrayBuffer> buffer = passedBuffer;
    if (!ArrayBufferView::verifySubRange<uint8_t>(buffer, byteOffset, byteLength)) {
        throwVMError(
            exec, createRangeError(exec, "Byte offset and length out of range of buffer"));
        return 0;
    }
    VM& vm = exec->vm();
    ConstructionContext context(
        structure, buffer, byteOffset, byteLength, ConstructionContext::DataView);
    ASSERT(context);
    JSDataView* result =
        new (NotNull, allocateCell<JSDataView>(vm.heap)) JSDataView(vm, context, buffer.get());
    result->finishCreation(vm);
    return result;
}

JSDataView* JSDataView::createUninitialized(ExecState*, Structure*, unsigned)
{
    UNREACHABLE_FOR_PLATFORM();
    return 0;
}

JSDataView* JSDataView::create(ExecState*, Structure*, unsigned)
{
    UNREACHABLE_FOR_PLATFORM();
    return 0;
}

bool JSDataView::set(ExecState*, JSObject*, unsigned, unsigned)
{
    UNREACHABLE_FOR_PLATFORM();
    return false;
}

PassRefPtr<DataView> JSDataView::typedImpl()
{
    return DataView::create(buffer(), byteOffset(), length());
}

bool JSDataView::getOwnPropertySlot(
    JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSDataView* thisObject = jsCast<JSDataView*>(object);
    if (propertyName == exec->propertyNames().byteLength) {
        slot.setValue(thisObject, DontEnum | ReadOnly, jsNumber(thisObject->m_length));
        return true;
    }
    
    return Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

ArrayBuffer* JSDataView::slowDownAndWasteMemory(JSArrayBufferView*)
{
    UNREACHABLE_FOR_PLATFORM();
    return 0;
}

PassRefPtr<ArrayBufferView> JSDataView::getTypedArrayImpl(JSArrayBufferView* object)
{
    JSDataView* thisObject = jsCast<JSDataView*>(object);
    return thisObject->typedImpl();
}

Structure* JSDataView::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info(),
        NonArray);
}

} // namespace JSC

