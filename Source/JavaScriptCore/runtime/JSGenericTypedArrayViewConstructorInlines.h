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
#include "JSTypedArrays.h"
#include "MathCommon.h"
#include "StructureCreateInlines.h"
#include <cstddef>
#include <optional>
#include <wtf/Assertions.h>
#include <wtf/text/ASCIILiteral.h>

namespace JSC {

template<typename ViewClass>
JSGenericTypedArrayViewConstructor<ViewClass>::JSGenericTypedArrayViewConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callConstructor(), constructConstructor())
{
}

template<typename ViewClass>
void JSGenericTypedArrayViewConstructor<ViewClass>::finishCreation(VM& vm, JSGlobalObject* globalObject, JSObject* prototype, const String& name)
{
    Base::finishCreation(vm, ViewClass::TypedArrayStorageType == TypeDataView ? 1 : 3, name, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->BYTES_PER_ELEMENT, jsNumber(ViewClass::elementSize), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly | PropertyAttribute::DontDelete);

    if constexpr (std::is_same_v<ViewClass, JSUint8Array>) {
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("fromBase64"_s, uint8ArrayConstructorFromBase64, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("fromHex"_s, uint8ArrayConstructorFromHex, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    }
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
        if (storage.hasOverflowed()) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return;
        }
    });
    RETURN_IF_EXCEPTION(scope, nullptr);

    ViewClass* result = ViewClass::createUninitialized(globalObject, structure, storage.size());
    EXCEPTION_ASSERT(!!scope.exception() == !result);
    if (!result) [[unlikely]]
        return nullptr;

    for (unsigned i = 0; i < storage.size(); ++i) {
        bool success = result->setIndex(globalObject, i, storage.at(i));
        EXCEPTION_ASSERT(scope.exception() || success);
        if (!success)
            return nullptr;
    }

    return result;
}

constinit const ASCIILiteral typedArrayErrorMessageBufferIsAlreadyDetached = "Buffer is already detached"_s;
constinit const ASCIILiteral typedArrayErrorMessageByteOffsetExceedSourceBufferByteLength = "byteOffset exceeds source ArrayBuffer byteLength"_s;

template<typename ViewClass>
inline JSObject* constructGenericTypedArrayViewWithArrayBuffer(JSGlobalObject* globalObject, Structure* structure, JSArrayBuffer* jsBuffer, size_t offset, std::optional<size_t> lengthOpt)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RefPtr<ArrayBuffer> buffer = jsBuffer->impl();
    if (buffer->isDetached()) {
        throwTypeError(globalObject, scope, typedArrayErrorMessageBufferIsAlreadyDetached);
        return nullptr;
    }

    std::optional<size_t> length;
    if (lengthOpt)
        length = lengthOpt;
    else {
        size_t byteLength = buffer->byteLength();
        if (buffer->isResizableOrGrowableShared()) {
            if (offset > byteLength) [[unlikely]] {
                throwRangeError(globalObject, scope, typedArrayErrorMessageByteOffsetExceedSourceBufferByteLength);
                return nullptr;
            }
        } else {
            if ((byteLength - offset) % ViewClass::elementSize) [[unlikely]] {
                throwRangeError(globalObject, scope, "ArrayBuffer length minus the byteOffset is not a multiple of the element size"_s);
                return nullptr;
            }
            length = (byteLength - offset) / ViewClass::elementSize;
        }
    }

    RELEASE_AND_RETURN(scope, ViewClass::create(globalObject, structure, WTF::move(buffer), offset, length));
}

template<typename ViewClass>
inline JSObject* constructGenericTypedArrayViewWithArguments(JSGlobalObject* globalObject, Structure* structure, JSValue firstValue, size_t offset, std::optional<size_t> lengthOpt)
{
    static_assert(ViewClass::TypedArrayStorageType != TypeDataView);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // https://tc39.es/ecma262/#sec-initializetypedarrayfromarraybuffer
    if (JSArrayBuffer* jsBuffer = dynamicDowncast<JSArrayBuffer>(firstValue))
        RELEASE_AND_RETURN(scope, constructGenericTypedArrayViewWithArrayBuffer<ViewClass>(globalObject, structure, jsBuffer, offset, lengthOpt));

    ASSERT(!offset);

    // For everything but DataView, we allow construction with any of:
    // - Another array. This creates a copy of the of that array.
    // - A primitive. This creates a new typed array of that length and zero-initializes it.

    if (JSObject* object = dynamicDowncast<JSObject>(firstValue)) {
        size_t length;

        // https://tc39.es/proposal-resizablearraybuffer/#sec-initializetypedarrayfromtypedarray
        if (isTypedView(object->type())) {
            auto* view = uncheckedDowncast<JSArrayBufferView>(object);

            length = view->length();

            ViewClass* result = ViewClass::createUninitialized(globalObject, structure, length);
            EXCEPTION_ASSERT(!!scope.exception() == !result);
            if (!result) [[unlikely]]
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
        if (JSArray* array = dynamicDowncast<JSArray>(object); array && isJSArray(array) && array->isIteratorProtocolFastAndNonObservable()) [[likely]]
            length = array->length();
        else {
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
            // 4) The iterator protocol is still intact.
            // it should not be observable that we do not use the iterator.

            if (!iteratorFunc.isUndefinedOrNull()
                && object->realmMayBeNull()
                && (iteratorFunc != object->realm()->arrayProtoValuesFunction()
                    || lengthSlot.isAccessor() || lengthSlot.isCustom() || lengthSlot.isTaintedByOpaqueObject()
                    || hasAnyArrayStorage(object->indexingType())
                    || !object->realm()->arrayIteratorProtocolWatchpointSet().isStillValid())) {
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

        ViewClass* result = ViewClass::createUninitialized(globalObject, structure, length);
        EXCEPTION_ASSERT(!!scope.exception() == !result);
        if (!result) [[unlikely]]
            return nullptr;

        scope.release();
        if (!result->setFromArrayLike(globalObject, 0, object, 0, length))
            return nullptr;
        return result;
    }

    ASSERT(!offset && lengthOpt.has_value());
    ASSERT(!firstValue.isObject());

    size_t length = lengthOpt.value();
    RELEASE_AND_RETURN(scope, ViewClass::create(globalObject, structure, length));
}

template<typename ViewClass>
inline JSObject* operationConstructGenericTypedArrayViewWithOneArgumentImpl(JSGlobalObject* globalObject, Structure* structure, JSValue firstValue)
{
    static_assert(ViewClass::TypedArrayStorageType != TypeDataView);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    std::optional<size_t> length;
    if (!firstValue.isObject()) {
        // the step 9 of https://tc39.es/ecma262/2026/#sec-typedarray
        length = firstValue.toIndex(globalObject, "length"_s);
        RETURN_IF_EXCEPTION(scope, { });
    }

    RELEASE_AND_RETURN(scope, constructGenericTypedArrayViewWithArguments<ViewClass>(globalObject, structure, firstValue, 0,  length));
}

// This is equivalent to https://tc39.es/ecma262/#sec-typedarray
template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue constructGenericTypedArrayViewImpl(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    static_assert(ViewClass::TypedArrayStorageType != TypeDataView);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());

    size_t argCount = callFrame->argumentCount();

    if (!argCount) {
        Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, typedArrayStructureWithTypedArrayType<ViewClass::TypedArrayStorageType>, newTarget, callFrame->jsCallee());
        RETURN_IF_EXCEPTION(scope, { });

        RELEASE_AND_RETURN(scope, JSValue::encode(ViewClass::create(globalObject, structure, 0)));
    }

    Structure* structure = nullptr;
    JSValue firstValue = callFrame->uncheckedArgument(0);
    size_t offset = 0;
    std::optional<size_t> length;
    if (auto* arrayBuffer = dynamicDowncast<JSArrayBuffer>(firstValue)) {
        if (arrayBuffer->isResizableOrGrowableShared()) {
            structure = JSC_GET_DERIVED_STRUCTURE(vm, resizableOrGrowableSharedTypedArrayStructureWithTypedArrayType<ViewClass::TypedArrayStorageType>, newTarget, callFrame->jsCallee());
            RETURN_IF_EXCEPTION(scope, { });
        } else {
            structure = JSC_GET_DERIVED_STRUCTURE(vm, typedArrayStructureWithTypedArrayType<ViewClass::TypedArrayStorageType>, newTarget, callFrame->jsCallee());
            RETURN_IF_EXCEPTION(scope, { });
        }

        if (argCount > 1) {
            offset = callFrame->uncheckedArgument(1).toIndex(globalObject, "byteOffset"_s);
            RETURN_IF_EXCEPTION(scope, { });

            if (offset % ViewClass::elementSize) [[unlikely]]
                return throwVMRangeError(globalObject, scope, "byteOffset modulo TypedArray.BYTES_PER_ELEMENT must be 0"_s);
        }

        if (argCount > 2) {
            // If the length value is present but undefined, treat it as missing.
            JSValue lengthValue = callFrame->uncheckedArgument(2);
            if (!lengthValue.isUndefined()) {
                length = lengthValue.toIndex(globalObject, "length"_s);
                RETURN_IF_EXCEPTION(scope, encodedJSValue());
            }
        }
    } else {
        if (!firstValue.isObject()) {
            // the step 9 of https://tc39.es/ecma262/2026/#sec-typedarray
            length = firstValue.toIndex(globalObject, "length"_s);
            RETURN_IF_EXCEPTION(scope, { });
        }

        structure = JSC_GET_DERIVED_STRUCTURE(vm, typedArrayStructureWithTypedArrayType<ViewClass::TypedArrayStorageType>, newTarget, callFrame->jsCallee());
        RETURN_IF_EXCEPTION(scope, { });
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(constructGenericTypedArrayViewWithArguments<ViewClass>(globalObject, structure, firstValue, offset, length)));
}

static EncodedJSValue constructDataViewImpl(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // [`DataView`][dataview] and [the other `%TypedArray%` constructors][typedarray] take almost same arguments and behave very similarly
    // but the spec has a different abstract operation steps.
    // It makes a difference for an order of checking their arguments and throwing errors in the detail.
    // And its difference comes with an user observable behavior in a corner case.
    // We should have a separate code path among them to implement the spec correctly & simply.
    //
    // [dataview]: https://tc39.es/ecma262/#sec-dataview-buffer-byteoffset-bytelength
    // [typedarray]: https://tc39.es/ecma262/#sec-typedarray

    size_t argCount = callFrame->argumentCount();
    if (!argCount) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "DataView constructor requires at least one argument."_s);

    ASSERT(argCount > 0);

    JSValue firstValue = callFrame->uncheckedArgument(0);
    JSArrayBuffer* arrayBuffer = dynamicDowncast<JSArrayBuffer>(firstValue);
    if (!arrayBuffer) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Expected ArrayBuffer for the first argument."_s);

    RefPtr<ArrayBuffer> buffer = arrayBuffer->impl();

    size_t offset = 0;
    if (argCount > 1) {
        offset = callFrame->uncheckedArgument(1).toIndex(globalObject, "byteOffset"_s);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (buffer->isDetached()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, typedArrayErrorMessageBufferIsAlreadyDetached);

    size_t bufferByteLength = buffer->byteLength();
    if (offset > bufferByteLength) [[unlikely]]
        return throwVMRangeError(globalObject, scope, typedArrayErrorMessageByteOffsetExceedSourceBufferByteLength);

    std::optional<size_t> length { };
    if (argCount > 2) {
        // If the length value is present but undefined, treat it as missing.
        if (JSValue lengthValue = callFrame->uncheckedArgument(2); !lengthValue.isUndefined()) [[likely]] {
            size_t viewByteLength = lengthValue.toIndex(globalObject, "byteLength"_s);
            RETURN_IF_EXCEPTION(scope, { });

            // Accroding to the spec (April 24, 2026),
            // https://tc39.es/ecma262/#sec-dataview-buffer-byteoffset-bytelength defines as the step 9-b that
            // we should throw RangeError rather even if ToIndex(byteLength) happens to detach the buffer as:
            // As user observable behavior, the sequence would be:
            //
            //  9-a: Let viewByteLength be ? ToIndex(byteLength): the weird object can detach the buffer at here.
            //  9-b: If `(offset + viewByteLength) > bufferByteLength`, throw RangeError. <- here.
            //  11: If the buffer is detached, throw TypeError.
            ASSERT(offset <= maxSafeInteger());
            ASSERT(viewByteLength <= maxSafeInteger());
            if ((offset + viewByteLength) > bufferByteLength) [[unlikely]]
                return throwVMRangeError(globalObject, scope, arrayBufferViewErrorMessageOutOfRangeOfBuffer);

            length = viewByteLength;
        }
    }

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = nullptr;
    if (arrayBuffer->isResizableOrGrowableShared()) {
        structure = JSC_GET_DERIVED_STRUCTURE(vm, resizableOrGrowableSharedTypedArrayStructureWithTypedArrayType<JSDataView::TypedArrayStorageType>, newTarget, callFrame->jsCallee());
        RETURN_IF_EXCEPTION(scope, { });
    } else {
        structure = JSC_GET_DERIVED_STRUCTURE(vm, typedArrayStructureWithTypedArrayType<JSDataView::TypedArrayStorageType>, newTarget, callFrame->jsCallee());
        RETURN_IF_EXCEPTION(scope, { });
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(constructGenericTypedArrayViewWithArrayBuffer<JSDataView>(globalObject, structure, arrayBuffer, offset, length)));
}

// This is equivalent to https://tc39.es/ecma262/#sec-dataview-buffer-byteoffset-bytelength
template<>
ALWAYS_INLINE EncodedJSValue constructGenericTypedArrayViewImpl<JSDataView>(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    EncodedJSValue dataview = constructDataViewImpl(globalObject, callFrame);
    RELEASE_AND_RETURN(scope, dataview);
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue callGenericTypedArrayViewImpl(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, ViewClass::info()->className));
}

} // namespace JSC
