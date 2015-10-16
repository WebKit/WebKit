/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
static EncodedJSValue constructGenericTypedArrayViewFromIterator(ExecState* exec, Structure* structure, JSValue iterator)
{
    if (!iterator.isObject())
        return JSValue::encode(throwTypeError(exec, "Symbol.Iterator for the first argument did not return an object."));

    MarkedArgumentBuffer storage;
    while (true) {
        JSValue next = iteratorStep(exec, iterator);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        if (next.isFalse())
            break;

        JSValue nextItem = iteratorValue(exec, next);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        storage.append(nextItem);
    }

    ViewClass* result = ViewClass::createUninitialized(exec, structure, storage.size());
    if (!result) {
        ASSERT(exec->hadException());
        return JSValue::encode(jsUndefined());
    }

    for (unsigned i = 0; i < storage.size(); ++i) {
        if (!result->setIndex(exec, i, storage.at(i))) {
            ASSERT(exec->hadException());
            return JSValue::encode(jsUndefined());
        }
    }

    return JSValue::encode(result);
}

template<typename ViewClass>
static EncodedJSValue JSC_HOST_CALL constructGenericTypedArrayView(ExecState* exec)
{
    Structure* structure =
        asInternalFunction(exec->callee())->globalObject()->typedArrayStructure(
            ViewClass::TypedArrayStorageType);

    VM& vm = exec->vm();

    if (!exec->argumentCount()) {
        if (ViewClass::TypedArrayStorageType == TypeDataView)
            return throwVMError(exec, createTypeError(exec, "DataView constructor requires at least one argument."));
        
        // Even though the documentation doesn't say so, it's correct to say
        // "new Int8Array()". This is the same as allocating an array of zero
        // length.
        return JSValue::encode(ViewClass::create(exec, structure, 0));
    }
    
    if (JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(exec->argument(0))) {
        RefPtr<ArrayBuffer> buffer = jsBuffer->impl();
        
        unsigned offset = (exec->argumentCount() > 1) ? exec->uncheckedArgument(1).toUInt32(exec) : 0;
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        unsigned length = 0;
        if (exec->argumentCount() > 2) {
            length = exec->uncheckedArgument(2).toUInt32(exec);
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
        } else {
            if ((buffer->byteLength() - offset) % ViewClass::elementSize)
                return throwVMError(exec, createRangeError(exec, "ArrayBuffer length minus the byteOffset is not a multiple of the element size"));
            length = (buffer->byteLength() - offset) / ViewClass::elementSize;
        }
        return JSValue::encode(ViewClass::create(exec, structure, buffer, offset, length));
    }
    
    if (ViewClass::TypedArrayStorageType == TypeDataView)
        return throwVMError(exec, createTypeError(exec, "Expected ArrayBuffer for the first argument."));
    
    // For everything but DataView, we allow construction with any of:
    // - Another array. This creates a copy of the of that array.
    // - An integer. This creates a new typed array of that length and zero-initializes it.
    
    if (JSObject* object = jsDynamicCast<JSObject*>(exec->uncheckedArgument(0))) {
        PropertySlot lengthSlot(object);
        object->getPropertySlot(exec, vm.propertyNames->length, lengthSlot);

        if (!isTypedView(object->classInfo()->typedArrayStorageType)) {
            JSValue iteratorFunc = object->get(exec, vm.propertyNames->iteratorSymbol);
            if (exec->hadException())
                return JSValue::encode(jsUndefined());

            // We would like not use the iterator as it is painfully slow. Fortunately, unless
            // 1) The iterator is not a known iterator.
            // 2) The base object does not have a length getter.
            // 3) Bad times are being had.
            // it should not be observable that we do not use the iterator.

            if (!iteratorFunc.isUndefined()
                && (iteratorFunc != exec->lexicalGlobalObject()->arrayProtoValuesFunction()
                    || lengthSlot.isAccessor() || lengthSlot.isCustom()
                    || exec->lexicalGlobalObject()->isHavingABadTime())) {

                    CallData callData;
                    CallType callType = getCallData(iteratorFunc, callData);
                    if (callType == CallTypeNone)
                        return JSValue::encode(throwTypeError(exec, "Symbol.Iterator for the first argument cannot be called."));

                    ArgList arguments;
                    JSValue iterator = call(exec, iteratorFunc, callType, callData, object, arguments);
                    if (exec->hadException())
                        return JSValue::encode(jsUndefined());

                    return constructGenericTypedArrayViewFromIterator<ViewClass>(exec, structure, iterator);

            }
        }

        unsigned length = lengthSlot.getValue(exec, vm.propertyNames->length).toUInt32(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        
        ViewClass* result = ViewClass::createUninitialized(exec, structure, length);
        if (!result) {
            ASSERT(exec->hadException());
            return JSValue::encode(jsUndefined());
        }
        
        if (!result->set(exec, object, 0, length))
            return JSValue::encode(jsUndefined());
        
        return JSValue::encode(result);
    }
    
    int length;
    if (exec->uncheckedArgument(0).isInt32())
        length = exec->uncheckedArgument(0).asInt32();
    else if (!exec->uncheckedArgument(0).isNumber())
        return throwVMError(exec, createTypeError(exec, "Invalid array length argument"));
    else {
        length = static_cast<int>(exec->uncheckedArgument(0).asNumber());
        if (length != exec->uncheckedArgument(0).asNumber())
            return throwVMError(exec, createTypeError(exec, "Invalid array length argument (fractional lengths not allowed)"));
    }

    if (length < 0)
        return throwVMError(exec, createRangeError(exec, "Requested length is negative"));
    return JSValue::encode(ViewClass::create(exec, structure, length));
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
