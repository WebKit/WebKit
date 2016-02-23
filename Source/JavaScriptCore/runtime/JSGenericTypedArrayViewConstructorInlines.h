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

#ifndef JSGenericTypedArrayViewConstructorInlines_h
#define JSGenericTypedArrayViewConstructorInlines_h

#include "Error.h"
#include "IteratorOperations.h"
#include "JSArrayBuffer.h"
#include "JSCJSValueInlines.h"
#include "JSDataView.h"
#include "JSGenericTypedArrayViewConstructor.h"
#include "JSGlobalObject.h"
#include "StructureInlines.h"

namespace JSC {

template<typename ViewClass>
JSGenericTypedArrayViewConstructor<ViewClass>::JSGenericTypedArrayViewConstructor(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

template<typename ViewClass>
void JSGenericTypedArrayViewConstructor<ViewClass>::finishCreation(VM& vm, JSGlobalObject* globalObject, JSObject* prototype, const String& name, FunctionExecutable* privateAllocator)
{
    Base::finishCreation(vm, name);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(3), DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->BYTES_PER_ELEMENT, jsNumber(ViewClass::elementSize), DontEnum | ReadOnly | DontDelete);

    if (privateAllocator)
        putDirectBuiltinFunction(vm, globalObject, vm.propertyNames->allocateTypedArrayPrivateName, privateAllocator, DontEnum | DontDelete | ReadOnly);
}

template<typename ViewClass>
JSGenericTypedArrayViewConstructor<ViewClass>*
JSGenericTypedArrayViewConstructor<ViewClass>::create(
    VM& vm, JSGlobalObject* globalObject, Structure* structure, JSObject* prototype,
    const String& name, FunctionExecutable* privateAllocator)
{
    JSGenericTypedArrayViewConstructor* result =
        new (NotNull, allocateCell<JSGenericTypedArrayViewConstructor>(vm.heap))
        JSGenericTypedArrayViewConstructor(vm, structure);
    result->finishCreation(vm, globalObject, prototype, name, privateAllocator);
    return result;
}

template<typename ViewClass>
Structure* JSGenericTypedArrayViewConstructor<ViewClass>::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

template<typename ViewClass>
static JSObject* constructGenericTypedArrayViewFromIterator(ExecState* exec, Structure* structure, JSValue iterator)
{
    if (!iterator.isObject())
        return throwTypeError(exec, "Symbol.Iterator for the first argument did not return an object.");

    MarkedArgumentBuffer storage;
    while (true) {
        JSValue next = iteratorStep(exec, iterator);
        if (exec->hadException())
            return nullptr;

        if (next.isFalse())
            break;

        JSValue nextItem = iteratorValue(exec, next);
        if (exec->hadException())
            return nullptr;

        storage.append(nextItem);
    }

    ViewClass* result = ViewClass::createUninitialized(exec, structure, storage.size());
    if (!result) {
        ASSERT(exec->hadException());
        return nullptr;
    }

    for (unsigned i = 0; i < storage.size(); ++i) {
        if (!result->setIndex(exec, i, storage.at(i))) {
            ASSERT(exec->hadException());
            return nullptr;
        }
    }

    return result;
}

template<typename ViewClass>
static JSObject* constructGenericTypedArrayViewWithArguments(ExecState* exec, Structure* structure, EncodedJSValue firstArgument, unsigned offset, Optional<unsigned> lengthOpt)
{
    JSValue firstValue = JSValue::decode(firstArgument);
    VM& vm = exec->vm();

    if (JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(firstValue)) {
        RefPtr<ArrayBuffer> buffer = jsBuffer->impl();
        unsigned length = 0;

        if (lengthOpt)
            length = lengthOpt.value();
        else {
            if ((buffer->byteLength() - offset) % ViewClass::elementSize)
                return throwRangeError(exec, "ArrayBuffer length minus the byteOffset is not a multiple of the element size");
            length = (buffer->byteLength() - offset) / ViewClass::elementSize;

        }

        return ViewClass::create(exec, structure, buffer, offset, length);
    }
    ASSERT(!offset && !lengthOpt);
    
    if (ViewClass::TypedArrayStorageType == TypeDataView)
        return throwTypeError(exec, "Expected ArrayBuffer for the first argument.");
    
    // For everything but DataView, we allow construction with any of:
    // - Another array. This creates a copy of the of that array.
    // - An integer. This creates a new typed array of that length and zero-initializes it.

    if (JSObject* object = jsDynamicCast<JSObject*>(firstValue)) {
        unsigned length;

        if (isTypedView(object->classInfo()->typedArrayStorageType))
            length = jsCast<JSArrayBufferView*>(object)->length();
        else {
            PropertySlot lengthSlot(object, PropertySlot::InternalMethodType::Get);
            object->getPropertySlot(exec, vm.propertyNames->length, lengthSlot);

            JSValue iteratorFunc = object->get(exec, vm.propertyNames->iteratorSymbol);
            if (exec->hadException())
                return nullptr;

            // We would like not use the iterator as it is painfully slow. Fortunately, unless
            // 1) The iterator is not a known iterator.
            // 2) The base object does not have a length getter.
            // 3) The base object might have indexed getters.
            // it should not be observable that we do not use the iterator.

            if (!iteratorFunc.isUndefined()
                && (iteratorFunc != object->globalObject()->arrayProtoValuesFunction()
                    || lengthSlot.isAccessor() || lengthSlot.isCustom()
                    || hasAnyArrayStorage(object->indexingType()))) {

                    CallData callData;
                    CallType callType = getCallData(iteratorFunc, callData);
                    if (callType == CallTypeNone)
                        return throwTypeError(exec, "Symbol.Iterator for the first argument cannot be called.");

                    ArgList arguments;
                    JSValue iterator = call(exec, iteratorFunc, callType, callData, object, arguments);
                    if (exec->hadException())
                        return nullptr;

                    return constructGenericTypedArrayViewFromIterator<ViewClass>(exec, structure, iterator);
            }

            length = lengthSlot.isUnset() ? 0 : lengthSlot.getValue(exec, vm.propertyNames->length).toUInt32(exec);
            if (exec->hadException())
                return nullptr;
        }

        
        ViewClass* result = ViewClass::createUninitialized(exec, structure, length);
        if (!result) {
            ASSERT(exec->hadException());
            return nullptr;
        }
        
        if (!result->set(exec, object, 0, length))
            return nullptr;
        
        return result;
    }
    
    int length;
    if (firstValue.isInt32())
        length = firstValue.asInt32();
    else if (!firstValue.isNumber())
        return throwTypeError(exec, "Invalid array length argument");
    else {
        length = static_cast<int>(firstValue.asNumber());
        if (length != firstValue.asNumber())
            return throwTypeError(exec, "Invalid array length argument (fractional lengths not allowed)");
    }

    if (length < 0)
        return throwRangeError(exec, "Requested length is negative");
    return ViewClass::create(exec, structure, length);
}

template<typename ViewClass>
static EncodedJSValue JSC_HOST_CALL constructGenericTypedArrayView(ExecState* exec)
{
    Structure* structure = InternalFunction::createSubclassStructure(exec, exec->newTarget(), asInternalFunction(exec->callee())->globalObject()->typedArrayStructure(ViewClass::TypedArrayStorageType));
    if (exec->hadException())
        return JSValue::encode(JSValue());

    size_t argCount = exec->argumentCount();

    if (!argCount) {
        if (ViewClass::TypedArrayStorageType == TypeDataView)
            return throwVMError(exec, createTypeError(exec, "DataView constructor requires at least one argument."));

        return JSValue::encode(ViewClass::create(exec, structure, 0));
    }

    JSValue firstValue = exec->uncheckedArgument(0);
    unsigned offset = 0;
    Optional<unsigned> length = Nullopt;
    if (jsDynamicCast<JSArrayBuffer*>(firstValue) && argCount > 1) {
        offset = exec->uncheckedArgument(1).toUInt32(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        if (argCount > 2) {
            length = exec->uncheckedArgument(2).toUInt32(exec);
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
        }

    }

    return JSValue::encode(constructGenericTypedArrayViewWithArguments<ViewClass>(exec, structure, JSValue::encode(firstValue), offset, length));
}

template<typename ViewClass>
ConstructType JSGenericTypedArrayViewConstructor<ViewClass>::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructGenericTypedArrayView<ViewClass>;
    return ConstructTypeHost;
}

template<typename ViewClass>
CallType JSGenericTypedArrayViewConstructor<ViewClass>::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = constructGenericTypedArrayView<ViewClass>;
    return CallTypeNone;
}

} // namespace JSC

#endif // JSGenericTypedArrayViewConstructorInlines_h
