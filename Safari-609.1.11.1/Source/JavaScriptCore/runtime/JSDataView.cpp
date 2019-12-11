/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
#include "TypeError.h"

namespace JSC {

const ClassInfo JSDataView::s_info = {
    "DataView", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDataView)};

JSDataView::JSDataView(VM& vm, ConstructionContext& context, ArrayBuffer* buffer)
    : Base(vm, context)
    , m_buffer(buffer)
{
}

JSDataView* JSDataView::create(
    JSGlobalObject* globalObject, Structure* structure, RefPtr<ArrayBuffer>&& buffer,
    unsigned byteOffset, unsigned byteLength)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(buffer);
    if (!ArrayBufferView::verifySubRangeLength(*buffer, byteOffset, byteLength, sizeof(uint8_t))) {
        throwVMError(globalObject, scope, createRangeError(globalObject, "Length out of range of buffer"_s));
        return nullptr;
    }
    if (!ArrayBufferView::verifyByteOffsetAlignment(byteOffset, sizeof(uint8_t))) {
        throwException(globalObject, scope, createRangeError(globalObject, "Byte offset is not aligned"_s));
        return nullptr;
    }
    ConstructionContext context(
        structure, buffer.copyRef(), byteOffset, byteLength, ConstructionContext::DataView);
    ASSERT(context);
    JSDataView* result =
        new (NotNull, allocateCell<JSDataView>(vm.heap)) JSDataView(vm, context, buffer.get());
    result->finishCreation(vm);
    return result;
}

JSDataView* JSDataView::createUninitialized(JSGlobalObject*, Structure*, unsigned)
{
    UNREACHABLE_FOR_PLATFORM();
    return 0;
}

JSDataView* JSDataView::create(JSGlobalObject*, Structure*, unsigned)
{
    UNREACHABLE_FOR_PLATFORM();
    return 0;
}

bool JSDataView::set(JSGlobalObject*, unsigned, JSObject*, unsigned, unsigned)
{
    UNREACHABLE_FOR_PLATFORM();
    return false;
}

bool JSDataView::setIndex(JSGlobalObject*, unsigned, JSValue)
{
    UNREACHABLE_FOR_PLATFORM();
    return false;
}

RefPtr<DataView> JSDataView::possiblySharedTypedImpl()
{
    return DataView::create(possiblySharedBuffer(), byteOffset(), length());
}

RefPtr<DataView> JSDataView::unsharedTypedImpl()
{
    return DataView::create(unsharedBuffer(), byteOffset(), length());
}

bool JSDataView::getOwnPropertySlot(
    JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = globalObject->vm();
    JSDataView* thisObject = jsCast<JSDataView*>(object);
    if (propertyName == vm.propertyNames->byteLength) {
        slot.setValue(thisObject, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly, jsNumber(thisObject->m_length));
        return true;
    }
    if (propertyName == vm.propertyNames->byteOffset) {
        slot.setValue(thisObject, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly, jsNumber(thisObject->byteOffset()));
        return true;
    }

    return Base::getOwnPropertySlot(thisObject, globalObject, propertyName, slot);
}

bool JSDataView::put(
    JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value,
    PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSDataView* thisObject = jsCast<JSDataView*>(cell);

    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        RELEASE_AND_RETURN(scope, ordinarySetSlow(globalObject, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode()));

    if (propertyName == vm.propertyNames->byteLength
        || propertyName == vm.propertyNames->byteOffset)
        return typeError(globalObject, scope, slot.isStrictMode(), "Attempting to write to read-only typed array property."_s);

    RELEASE_AND_RETURN(scope, Base::put(thisObject, globalObject, propertyName, value, slot));
}

bool JSDataView::defineOwnProperty(
    JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName,
    const PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSDataView* thisObject = jsCast<JSDataView*>(object);
    if (propertyName == vm.propertyNames->byteLength
        || propertyName == vm.propertyNames->byteOffset)
        return typeError(globalObject, scope, shouldThrow, "Attempting to define read-only typed array property."_s);

    RELEASE_AND_RETURN(scope, Base::defineOwnProperty(thisObject, globalObject, propertyName, descriptor, shouldThrow));
}

bool JSDataView::deleteProperty(
    JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName)
{
    VM& vm = globalObject->vm();
    JSDataView* thisObject = jsCast<JSDataView*>(cell);
    if (propertyName == vm.propertyNames->byteLength
        || propertyName == vm.propertyNames->byteOffset)
        return false;

    return Base::deleteProperty(thisObject, globalObject, propertyName);
}

void JSDataView::getOwnNonIndexPropertyNames(
    JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& array, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    JSDataView* thisObject = jsCast<JSDataView*>(object);
    
    if (mode.includeDontEnumProperties()) {
        array.add(vm.propertyNames->byteOffset);
        array.add(vm.propertyNames->byteLength);
    }
    
    Base::getOwnNonIndexPropertyNames(thisObject, globalObject, array, mode);
}

Structure* JSDataView::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(DataViewType, StructureFlags), info(),
        NonArray);
}

} // namespace JSC

