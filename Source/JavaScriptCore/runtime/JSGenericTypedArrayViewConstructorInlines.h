/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
        new (NotNull, allocateCell<JSGenericTypedArrayViewConstructor>(vm))
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

template<typename ViewClass>
inline JSObject* constructGenericTypedArrayViewWithArguments(JSGlobalObject* globalObject, Structure* structure, JSValue firstValue, size_t offset, std::optional<size_t> lengthOpt)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // https://tc39.es/proposal-resizablearraybuffer/#sec-initializetypedarrayfromarraybuffer
    if (JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(firstValue)) {
        RefPtr<ArrayBuffer> buffer = jsBuffer->impl();
        if (buffer->isDetached()) {
            throwTypeError(globalObject, scope, "Buffer is already detached"_s);
            return nullptr;
        }

        std::optional<size_t> length;
        if (lengthOpt)
            length = lengthOpt;
        else {
            size_t byteLength = buffer->byteLength();
            if (buffer->isResizableOrGrowableShared()) {
                if (UNLIKELY(offset > byteLength)) {
                    throwRangeError(globalObject, scope, "byteOffset exceeds source ArrayBuffer byteLength"_s);
                    return nullptr;
                }
            } else {
                if (UNLIKELY((byteLength - offset) % ViewClass::elementSize)) {
                    throwRangeError(globalObject, scope, "ArrayBuffer length minus the byteOffset is not a multiple of the element size"_s);
                    return nullptr;
                }
                length = (byteLength - offset) / ViewClass::elementSize;
            }
        }

        RELEASE_AND_RETURN(scope, ViewClass::create(globalObject, structure, WTFMove(buffer), offset, length));
    }
    ASSERT(!offset && !lengthOpt);

    if constexpr (ViewClass::TypedArrayStorageType == TypeDataView) {
        throwTypeError(globalObject, scope, "Expected ArrayBuffer for the first argument."_s);
        return nullptr;
    }
    
    // For everything but DataView, we allow construction with any of:
    // - Another array. This creates a copy of the of that array.
    // - A primitive. This creates a new typed array of that length and zero-initializes it.

    if (JSObject* object = jsDynamicCast<JSObject*>(firstValue)) {
        size_t length;

        // https://tc39.es/proposal-resizablearraybuffer/#sec-initializetypedarrayfromtypedarray
        if (isTypedView(object->type())) {
            auto* view = jsCast<JSArrayBufferView*>(object);

            length = view->length();

            ViewClass* result = ViewClass::createUninitialized(globalObject, structure, length);
            EXCEPTION_ASSERT(!!scope.exception() == !result);
            if (UNLIKELY(!result))
                return nullptr;

            IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
            if (isIntegerIndexedObjectOutOfBounds(view, getter)) {
                throwTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);
                return nullptr;
            }

            if (contentType(object->type()) != ViewClass::contentType) {
                throwTypeError(globalObject, scope, "Content types of source and new typed array are different"_s);
                return nullptr;
            }

            scope.release();
            if (!result->setFromTypedArray(globalObject, 0, view, 0, length, CopyType::Unobservable))
                return nullptr;
            return result;
        }

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
            && (iteratorFunc != object->globalObject()->arrayProtoValuesFunction()
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

        ViewClass* result = ViewClass::createUninitialized(globalObject, structure, length);
        EXCEPTION_ASSERT(!!scope.exception() == !result);
        if (UNLIKELY(!result))
            return nullptr;

        scope.release();
        if (!result->setFromArrayLike(globalObject, 0, object, 0, length))
            return nullptr;
        return result;
    }

    size_t length = firstValue.toTypedArrayIndex(globalObject, "length"_s);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RELEASE_AND_RETURN(scope, ViewClass::create(globalObject, structure, length));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue constructGenericTypedArrayViewImpl(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());

    size_t argCount = callFrame->argumentCount();

    if (!argCount) {
        Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, typedArrayStructureWithTypedArrayType<ViewClass::TypedArrayStorageType>, newTarget, callFrame->jsCallee());
        RETURN_IF_EXCEPTION(scope, { });

        if constexpr (ViewClass::TypedArrayStorageType == TypeDataView)
            return throwVMTypeError(globalObject, scope, "DataView constructor requires at least one argument."_s);

        RELEASE_AND_RETURN(scope, JSValue::encode(ViewClass::create(globalObject, structure, 0)));
    }

    Structure* structure = nullptr;
    JSValue firstValue = callFrame->uncheckedArgument(0);
    size_t offset = 0;
    std::optional<size_t> length;
    if (auto* arrayBuffer = jsDynamicCast<JSArrayBuffer*>(firstValue)) {
        if (arrayBuffer->isResizableOrGrowableShared()) {
            structure = JSC_GET_DERIVED_STRUCTURE(vm, resizableOrGrowableSharedTypedArrayStructureWithTypedArrayType<ViewClass::TypedArrayStorageType>, newTarget, callFrame->jsCallee());
            RETURN_IF_EXCEPTION(scope, { });
        } else {
            structure = JSC_GET_DERIVED_STRUCTURE(vm, typedArrayStructureWithTypedArrayType<ViewClass::TypedArrayStorageType>, newTarget, callFrame->jsCallee());
            RETURN_IF_EXCEPTION(scope, { });
        }

        if (argCount > 1) {
            offset = callFrame->uncheckedArgument(1).toTypedArrayIndex(globalObject, "byteOffset"_s);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());

            if (argCount > 2) {
                // If the length value is present but undefined, treat it as missing.
                JSValue lengthValue = callFrame->uncheckedArgument(2);
                if (!lengthValue.isUndefined()) {
                    length = lengthValue.toTypedArrayIndex(globalObject, ViewClass::TypedArrayStorageType == TypeDataView ? "byteLength"_s : "length"_s);
                    RETURN_IF_EXCEPTION(scope, encodedJSValue());
                }
            }
        }
    } else {
        structure = JSC_GET_DERIVED_STRUCTURE(vm, typedArrayStructureWithTypedArrayType<ViewClass::TypedArrayStorageType>, newTarget, callFrame->jsCallee());
        RETURN_IF_EXCEPTION(scope, { });
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(constructGenericTypedArrayViewWithArguments<ViewClass>(globalObject, structure, firstValue, offset, length)));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue callGenericTypedArrayViewImpl(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, ViewClass::info()->className));
}

} // namespace JSC
