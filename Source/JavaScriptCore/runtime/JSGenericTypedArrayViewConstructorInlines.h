/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#pragma once

#include "BuiltinNames.h"
#include "Error.h"
#include "IteratorOperations.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferPrototypeInlines.h"
#include "JSCJSValueInlines.h"
#include "JSDataView.h"
#include "JSGenericTypedArrayViewConstructor.h"
#include "JSGlobalObject.h"
#include "StructureInlines.h"

namespace JSC {

template<typename ViewClass>
JSGenericTypedArrayViewConstructor<ViewClass>::JSGenericTypedArrayViewConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callConstructor(), constructConstructor())
{
}

template<typename ViewClass>
void JSGenericTypedArrayViewConstructor<ViewClass>::finishCreation(VM& vm, JSGlobalObject*, JSObject* prototype, const String& name)
{
    Base::finishCreation(vm, ViewClass::TypedArrayStorageType == TypeDataView ? 1 : 3, name, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->BYTES_PER_ELEMENT, jsNumber(ViewClass::elementSize), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly | PropertyAttribute::DontDelete);
}

template<typename ViewClass>
JSGenericTypedArrayViewConstructor<ViewClass>*
JSGenericTypedArrayViewConstructor<ViewClass>::create(
    VM& vm, JSGlobalObject* globalObject, Structure* structure, JSObject* prototype,
    const String& name)
{
    JSGenericTypedArrayViewConstructor* result =
        new (NotNull, allocateCell<JSGenericTypedArrayViewConstructor>(vm.heap))
        JSGenericTypedArrayViewConstructor(vm, structure);
    result->finishCreation(vm, globalObject, prototype, name);
    return result;
}

template<typename ViewClass>
Structure* JSGenericTypedArrayViewConstructor<ViewClass>::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

template<typename ViewClass>
inline JSObject* constructGenericTypedArrayViewFromIterator(JSGlobalObject* globalObject, Structure* structure, JSObject* iterable, JSValue iteratorMethod)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    MarkedArgumentBuffer storage;
    forEachInIterable(*globalObject, iterable, iteratorMethod, [&] (VM&, JSGlobalObject&, JSValue value) {
        storage.append(value);
        if (UNLIKELY(storage.hasOverflowed())) {
            throwOutOfMemoryError(globalObject, scope);
            return;
        }
    });
    RETURN_IF_EXCEPTION(scope, nullptr);

    ViewClass* result = ViewClass::createUninitialized(globalObject, structure, storage.size());
    EXCEPTION_ASSERT(!!scope.exception() == !result);
    if (UNLIKELY(!result))
        return nullptr;

    for (unsigned i = 0; i < storage.size(); ++i) {
        bool success = result->setIndex(globalObject, i, storage.at(i));
        EXCEPTION_ASSERT(scope.exception() || success);
        if (!success)
            return nullptr;
    }

    return result;
}

inline JSArrayBuffer* constructCustomArrayBufferIfNeeded(JSGlobalObject* globalObject, JSArrayBufferView* view)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArrayBuffer* source = view->possiblySharedJSBuffer(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (source->isShared())
        return nullptr;

    Optional<JSValue> species = arrayBufferSpeciesConstructor(globalObject, source, ArrayBufferSharingMode::Default);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!species)
        return nullptr;

    if (!species->isConstructor(vm)) {
        throwTypeError(globalObject, scope, "species is not a constructor"_s);
        return nullptr;
    }

    JSValue prototype = species->get(globalObject, vm.propertyNames->prototype);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto buffer = ArrayBuffer::tryCreate(source->impl()->byteLength(), 1);
    if (!buffer) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    auto result = JSArrayBuffer::create(vm, getFunctionRealm(vm, asObject(species.value()))->arrayBufferStructure(ArrayBufferSharingMode::Default), WTFMove(buffer));
    if (prototype.isObject())
        result->setPrototypeDirect(vm, prototype);
    return result;
}

template<typename ViewClass>
inline JSObject* constructGenericTypedArrayViewWithArguments(JSGlobalObject* globalObject, Structure* structure, EncodedJSValue firstArgument, unsigned offset, Optional<unsigned> lengthOpt)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue firstValue = JSValue::decode(firstArgument);

    if (JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(vm, firstValue)) {
        RefPtr<ArrayBuffer> buffer = jsBuffer->impl();
        if (buffer->isDetached()) {
            throwTypeError(globalObject, scope, "Buffer is already detached"_s);
            return nullptr;
        }

        unsigned length = 0;
        if (lengthOpt)
            length = lengthOpt.value();
        else {
            if (UNLIKELY((buffer->byteLength() - offset) % ViewClass::elementSize)) {
                throwRangeError(globalObject, scope, "ArrayBuffer length minus the byteOffset is not a multiple of the element size"_s);
                return nullptr;
            }
            length = (buffer->byteLength() - offset) / ViewClass::elementSize;
        }

        RELEASE_AND_RETURN(scope, ViewClass::create(globalObject, structure, WTFMove(buffer), offset, length));
    }
    ASSERT(!offset && !lengthOpt);
    
    if (UNLIKELY(ViewClass::TypedArrayStorageType == TypeDataView)) {
        throwTypeError(globalObject, scope, "Expected ArrayBuffer for the first argument."_s);
        return nullptr;
    }
    
    // For everything but DataView, we allow construction with any of:
    // - Another array. This creates a copy of the of that array.
    // - A primitive. This creates a new typed array of that length and zero-initializes it.

    if (JSObject* object = jsDynamicCast<JSObject*>(vm, firstValue)) {
        unsigned length;
        JSArrayBuffer* customBuffer = nullptr;

        if (isTypedView(object->classInfo(vm)->typedArrayStorageType)) {
            auto* view = jsCast<JSArrayBufferView*>(object);

            customBuffer = constructCustomArrayBufferIfNeeded(globalObject, view);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (view->isDetached()) {
                throwTypeError(globalObject, scope, "Underlying ArrayBuffer has been detached from the view"_s);
                return nullptr;
            }

            length = view->length();
        } else {
            // This getPropertySlot operation should not be observed by the Proxy.
            // So we use VMInquiry. And purge the opaque object cases (proxy and namespace object) by isTaintedByOpaqueObject() guard.
            PropertySlot lengthSlot(object, PropertySlot::InternalMethodType::VMInquiry, &vm);
            object->getPropertySlot(globalObject, vm.propertyNames->length, lengthSlot);
            RETURN_IF_EXCEPTION(scope, nullptr);
            lengthSlot.disallowVMEntry.reset();

            JSValue iteratorFunc = object->get(globalObject, vm.propertyNames->iteratorSymbol);
            RETURN_IF_EXCEPTION(scope, nullptr);

            // We would like not use the iterator as it is painfully slow. Fortunately, unless
            // 1) The iterator is not a known iterator.
            // 2) The base object does not have a length getter.
            // 3) The base object might have indexed getters.
            // it should not be observable that we do not use the iterator.

            if (!iteratorFunc.isUndefinedOrNull()
                && (iteratorFunc != object->globalObject(vm)->arrayProtoValuesFunction()
                    || lengthSlot.isAccessor() || lengthSlot.isCustom() || lengthSlot.isTaintedByOpaqueObject()
                    || hasAnyArrayStorage(object->indexingType()))) {

                    RELEASE_AND_RETURN(scope, constructGenericTypedArrayViewFromIterator<ViewClass>(globalObject, structure, object, iteratorFunc));
            }

            if (lengthSlot.isUnset())
                length = 0;
            else {
                JSValue value = lengthSlot.getValue(globalObject, vm.propertyNames->length);
                RETURN_IF_EXCEPTION(scope, nullptr);
                length = value.toLength(globalObject);
                RETURN_IF_EXCEPTION(scope, nullptr);
            }
        }

        ViewClass* result = customBuffer
            ? ViewClass::create(globalObject, structure, customBuffer->impl(), 0, length)
            : ViewClass::createUninitialized(globalObject, structure, length);
        EXCEPTION_ASSERT(!!scope.exception() == !result);
        if (UNLIKELY(!result))
            return nullptr;

        scope.release();
        if (!result->set(globalObject, 0, object, 0, length))
            return nullptr;
        
        return result;
    }

    unsigned length = firstValue.toIndex(globalObject, "length");
    RETURN_IF_EXCEPTION(scope, nullptr);
    RELEASE_AND_RETURN(scope, ViewClass::create(globalObject, structure, length));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue constructGenericTypedArrayViewImpl(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = newTarget == callFrame->jsCallee()
        ? globalObject->typedArrayStructure(ViewClass::TypedArrayStorageType)
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->typedArrayStructure(ViewClass::TypedArrayStorageType));
    RETURN_IF_EXCEPTION(scope, { });

    size_t argCount = callFrame->argumentCount();

    if (!argCount) {
        if (ViewClass::TypedArrayStorageType == TypeDataView)
            return throwVMTypeError(globalObject, scope, "DataView constructor requires at least one argument."_s);

        RELEASE_AND_RETURN(scope, JSValue::encode(ViewClass::create(globalObject, structure, 0)));
    }

    JSValue firstValue = callFrame->uncheckedArgument(0);
    unsigned offset = 0;
    Optional<unsigned> length = WTF::nullopt;
    if (jsDynamicCast<JSArrayBuffer*>(vm, firstValue) && argCount > 1) {
        offset = callFrame->uncheckedArgument(1).toIndex(globalObject, "byteOffset");
        RETURN_IF_EXCEPTION(scope, encodedJSValue());

        if (argCount > 2) {
            // If the length value is present but undefined, treat it as missing.
            JSValue lengthValue = callFrame->uncheckedArgument(2);
            if (!lengthValue.isUndefined()) {
                length = lengthValue.toIndex(globalObject, ViewClass::TypedArrayStorageType == TypeDataView ? "byteLength" : "length");
                RETURN_IF_EXCEPTION(scope, encodedJSValue());
            }
        }
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(constructGenericTypedArrayViewWithArguments<ViewClass>(globalObject, structure, JSValue::encode(firstValue), offset, length)));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue callGenericTypedArrayViewImpl(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, ViewClass::info()->className));
}

} // namespace JSC
