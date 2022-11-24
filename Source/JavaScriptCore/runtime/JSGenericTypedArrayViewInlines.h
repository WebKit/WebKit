/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "ArrayBufferView.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArrayBuffer.h"
#include "JSCellInlines.h"
#include "JSGenericTypedArrayView.h"
#include "TypeError.h"
#include "TypedArrays.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace JSC {

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>::JSGenericTypedArrayView(
    VM& vm, ConstructionContext& context)
    : Base(vm, context)
{
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::create(
    JSGlobalObject* globalObject, Structure* structure, size_t length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ConstructionContext context(vm, structure, length, sizeof(typename Adaptor::Type));
    if (!context) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    JSGenericTypedArrayView* result =
        new (NotNull, allocateCell<JSGenericTypedArrayView>(vm))
        JSGenericTypedArrayView(vm, context);
    result->finishCreation(vm);
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::createWithFastVector(
    JSGlobalObject* globalObject, Structure* structure, size_t length, void* vector)
{
    VM& vm = globalObject->vm();
    ConstructionContext context(structure, length, vector);
    RELEASE_ASSERT(context);
    JSGenericTypedArrayView* result =
        new (NotNull, allocateCell<JSGenericTypedArrayView>(vm))
        JSGenericTypedArrayView(vm, context);
    result->finishCreation(vm);
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::createUninitialized(JSGlobalObject* globalObject, Structure* structure, size_t length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ConstructionContext context(
        vm, structure, length, sizeof(typename Adaptor::Type),
        ConstructionContext::DontInitialize);
    if (!context) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    JSGenericTypedArrayView* result =
        new (NotNull, allocateCell<JSGenericTypedArrayView>(vm))
        JSGenericTypedArrayView(vm, context);
    result->finishCreation(vm);
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::create(JSGlobalObject* globalObject, Structure* structure, RefPtr<ArrayBuffer>&& buffer, size_t byteOffset, std::optional<size_t> length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    size_t elementSize = sizeof(typename Adaptor::Type);
    ASSERT(buffer);
    if (buffer->isDetached()) {
        throwTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);
        return nullptr;
    }

    ASSERT(length || buffer->isResizableOrGrowableShared());

    if (!ArrayBufferView::verifySubRangeLength(*buffer, byteOffset, length.value_or(0), elementSize)) {
        throwException(globalObject, scope, createRangeError(globalObject, "Length out of range of buffer"_s));
        return nullptr;
    }

    if (!ArrayBufferView::verifyByteOffsetAlignment(byteOffset, elementSize)) {
        throwException(globalObject, scope, createRangeError(globalObject, "Byte offset is not aligned"_s));
        return nullptr;
    }

    ConstructionContext context(vm, structure, WTFMove(buffer), byteOffset, length);
    ASSERT(context);
    JSGenericTypedArrayView* result =
        new (NotNull, allocateCell<JSGenericTypedArrayView>(vm))
        JSGenericTypedArrayView(vm, context);
    result->finishCreation(vm);
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::create(VM& vm, Structure* structure, RefPtr<typename Adaptor::ViewType>&& impl)
{
    ConstructionContext context(vm, structure, impl->possiblySharedBuffer(), impl->byteOffsetRaw(), impl->isAutoLength() ? std::nullopt : std::optional { impl->lengthRaw() });
    ASSERT(context);
    JSGenericTypedArrayView* result =
        new (NotNull, allocateCell<JSGenericTypedArrayView>(vm))
        JSGenericTypedArrayView(vm, context);
    result->finishCreation(vm);
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::create(Structure* structure, JSGlobalObject* globalObject, RefPtr<typename Adaptor::ViewType>&& impl)
{
    return create(globalObject->vm(), structure, WTFMove(impl));
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::validateRange(
    JSGlobalObject* globalObject, size_t offset, size_t length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (canAccessRangeQuickly(offset, length))
        return true;
    
    throwException(globalObject, scope, createRangeError(globalObject, "Range consisting of offset and length are out of bounds"_s));
    return false;
}

template<typename Adaptor>
template<typename OtherAdaptor>
bool JSGenericTypedArrayView<Adaptor>::setWithSpecificType(
    JSGlobalObject* globalObject, size_t offset, JSGenericTypedArrayView<OtherAdaptor>* other,
    size_t otherOffset, size_t length, CopyType type)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Handle the hilarious case: the act of getting the length could have resulted
    // in detaching. Well, no. That'll never happen because there cannot be
    // side-effects on getting the length of a typed array. But predicting where there
    // are, or aren't, side-effects is a fool's game so we resort to this cheap
    // check. Worst case, if we're wrong, people start seeing less things get copied
    // but we won't have a security vulnerability.
    length = std::min(length, other->length());

    RELEASE_ASSERT(other->canAccessRangeQuickly(otherOffset, length));
    bool success = validateRange(globalObject, offset, length);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception() == success);
    if (!success)
        return false;

    if constexpr (Adaptor::contentType != OtherAdaptor::contentType) {
        throwTypeError(globalObject, scope, "Content types of source and destination typed arrays are different"_s);
        return false;
    }
    
    // This method doesn't support copying between the same array. Note that
    // set() will only call this if the types differ, which implicitly guarantees
    // that we can't be the same array. This is relevant because the way we detect
    // non-overlapping is by checking if either (a) either array doesn't have a
    // backing buffer or (b) the backing buffers are different, but that doesn't
    // catch the case where it's the *same* array - fortunately though, this code
    // path never needs to worry about that case.
    ASSERT(static_cast<JSCell*>(this) != static_cast<JSCell*>(other));
    
    // 1) If the two arrays are non-overlapping, we can copy in any order we like
    //    and we don't need an intermediate buffer. Arrays are definitely
    //    non-overlapping if either one of them has no backing buffer (that means
    //    that it *owns* its philosophical backing buffer) or if they have
    //    different backing buffers.
    // 2) If the two arrays overlap but have the same element size, we can do a
    //    memmove-like copy where we flip-flop direction based on which vector
    //    starts before the other:
    //    A) If the destination vector is before the source vector, then a forward
    //       copy is in order.
    //    B) If the destination vector is after the source vector, then a backward
    //       copy is in order.
    // 3) If we have different element sizes and there is a chance of overlap then
    //    we need an intermediate vector.
    
    // NB. Comparisons involving elementSize will be constant-folded by template
    // specialization.

    unsigned otherElementSize = sizeof(typename OtherAdaptor::Type);

    // Handle cases (1) and (2A).
    if (!hasArrayBuffer() || !other->hasArrayBuffer()
        || existingBuffer() != other->existingBuffer()
        || (elementSize == otherElementSize && (static_cast<void*>(typedVector() + offset) <= static_cast<void*>(other->typedVector() + otherOffset)))
        || type == CopyType::LeftToRight) {
        for (size_t i = 0; i < length; ++i) {
            setIndexQuicklyToNativeValue(
                offset + i, OtherAdaptor::template convertTo<Adaptor>(
                    other->getIndexQuicklyAsNativeValue(i + otherOffset)));
        }
        return true;
    }

    // Now we either have (2B) or (3) - so first we try to cover (2B).
    if (elementSize == otherElementSize) {
        for (size_t i = length; i--;) {
            setIndexQuicklyToNativeValue(
                offset + i, OtherAdaptor::template convertTo<Adaptor>(
                    other->getIndexQuicklyAsNativeValue(i + otherOffset)));
        }
        return true;
    }
    
    // Fail: we need an intermediate transfer buffer (i.e. case (3)).
    Vector<typename Adaptor::Type, 32> transferBuffer(length);
    for (size_t i = length; i--;) {
        transferBuffer[i] = OtherAdaptor::template convertTo<Adaptor>(
            other->getIndexQuicklyAsNativeValue(i + otherOffset));
    }
    for (size_t i = length; i--;)
        setIndexQuicklyToNativeValue(offset + i, transferBuffer[i]);
    
    return true;
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::setFromTypedArray(JSGlobalObject* globalObject, size_t offset, JSArrayBufferView* object, size_t objectOffset, size_t length, CopyType type)
{
    // https://tc39.es/proposal-resizablearraybuffer/#sec-settypedarrayfromtypedarray

    ASSERT(isTypedView(object->type()));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto memmoveFastPath = [&] (JSArrayBufferView* other) {
        // The super fast case: we can just memmove since we're the same underlying storage type.
        length = std::min(length, other->length());

        bool success = validateRange(globalObject, offset, length);
        EXCEPTION_ASSERT(!scope.exception() == success);
        if (!success)
            return false;

        RELEASE_ASSERT(JSC::elementSize(Adaptor::typeValue) == JSC::elementSize(other->type()));
        memmove(typedVector() + offset, bitwise_cast<typename Adaptor::Type*>(other->vector()) + objectOffset, length * elementSize);
        return true;
    };

    TypedArrayType typedArrayType = JSC::typedArrayType(object->type());
    if (typedArrayType == Adaptor::typeValue)
        return memmoveFastPath(jsCast<JSArrayBufferView*>(object));

    if (isSomeUint8(typedArrayType) && isSomeUint8(Adaptor::typeValue))
        return memmoveFastPath(jsCast<JSArrayBufferView*>(object));

    if (isInt(Adaptor::typeValue) && isInt(typedArrayType) && !isClamped(Adaptor::typeValue) && JSC::elementSize(Adaptor::typeValue) == JSC::elementSize(typedArrayType))
        return memmoveFastPath(jsCast<JSArrayBufferView*>(object));

    switch (typedArrayType) {
    case TypeInt8:
        RELEASE_AND_RETURN(scope, setWithSpecificType<Int8Adaptor>(
            globalObject, offset, jsCast<JSInt8Array*>(object), objectOffset, length, type));
    case TypeInt16:
        RELEASE_AND_RETURN(scope, setWithSpecificType<Int16Adaptor>(
            globalObject, offset, jsCast<JSInt16Array*>(object), objectOffset, length, type));
    case TypeInt32:
        RELEASE_AND_RETURN(scope, setWithSpecificType<Int32Adaptor>(
            globalObject, offset, jsCast<JSInt32Array*>(object), objectOffset, length, type));
    case TypeUint8:
        RELEASE_AND_RETURN(scope, setWithSpecificType<Uint8Adaptor>(
            globalObject, offset, jsCast<JSUint8Array*>(object), objectOffset, length, type));
    case TypeUint8Clamped:
        RELEASE_AND_RETURN(scope, setWithSpecificType<Uint8ClampedAdaptor>(
            globalObject, offset, jsCast<JSUint8ClampedArray*>(object), objectOffset, length, type));
    case TypeUint16:
        RELEASE_AND_RETURN(scope, setWithSpecificType<Uint16Adaptor>(
            globalObject, offset, jsCast<JSUint16Array*>(object), objectOffset, length, type));
    case TypeUint32:
        RELEASE_AND_RETURN(scope, setWithSpecificType<Uint32Adaptor>(
            globalObject, offset, jsCast<JSUint32Array*>(object), objectOffset, length, type));
    case TypeFloat32:
        RELEASE_AND_RETURN(scope, setWithSpecificType<Float32Adaptor>(
            globalObject, offset, jsCast<JSFloat32Array*>(object), objectOffset, length, type));
    case TypeFloat64:
        RELEASE_AND_RETURN(scope, setWithSpecificType<Float64Adaptor>(
            globalObject, offset, jsCast<JSFloat64Array*>(object), objectOffset, length, type));
    case TypeBigInt64:
        RELEASE_AND_RETURN(scope, setWithSpecificType<BigInt64Adaptor>(
            globalObject, offset, jsCast<JSBigInt64Array*>(object), objectOffset, length, type));
    case TypeBigUint64:
        RELEASE_AND_RETURN(scope, setWithSpecificType<BigUint64Adaptor>(
            globalObject, offset, jsCast<JSBigUint64Array*>(object), objectOffset, length, type));
    case NotTypedArray:
    case TypeDataView: {
        RELEASE_ASSERT_NOT_REACHED();
        return true;
    } }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::setFromArrayLike(JSGlobalObject* globalObject, size_t offset, JSObject* object, size_t objectOffset, size_t length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool success = validateRange(globalObject, offset, length);
    EXCEPTION_ASSERT(!scope.exception() == success);
    if (!success)
        return false;

    // It is not valid to ever call object->get() with an index of more than MAX_ARRAY_INDEX.
    // So we iterate in the optimized loop up to MAX_ARRAY_INDEX, then if there is anything to do beyond this, we rely on slower code.
    size_t safeUnadjustedLength = std::min(length, static_cast<size_t>(MAX_ARRAY_INDEX) + 1);
    size_t safeLength = objectOffset <= safeUnadjustedLength ? safeUnadjustedLength - objectOffset : 0;
    for (size_t i = 0; i < safeLength; ++i) {
        ASSERT(i + objectOffset <= MAX_ARRAY_INDEX);
        JSValue value = object->get(globalObject, static_cast<unsigned>(i + objectOffset));
        RETURN_IF_EXCEPTION(scope, false);
        bool success = setIndex(globalObject, offset + i, value);
        EXCEPTION_ASSERT(!scope.exception() || !success);
        if (!success)
            return false;
    }
    for (size_t i = safeLength; i < length; ++i) {
        JSValue value = object->get(globalObject, static_cast<uint64_t>(i + objectOffset));
        RETURN_IF_EXCEPTION(scope, false);
        bool success = setIndex(globalObject, offset + i, value);
        EXCEPTION_ASSERT(!scope.exception() || !success);
        if (!success)
            return false;
    }
    return true;
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::setFromArrayLike(JSGlobalObject* globalObject, size_t offset, JSValue sourceValue)
{
    // https://tc39.es/ecma262/#sec-settypedarrayfromarraylike

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (isDetached()) {
        throwTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);
        return false;
    }

    size_t targetLength = this->length();

    JSObject* source = sourceValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue sourceLengthValue = source->get(globalObject, vm.propertyNames->length);
    RETURN_IF_EXCEPTION(scope, { });
    size_t sourceLength = sourceLengthValue.toLength(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (offset > MAX_ARRAY_BUFFER_SIZE || !isSumSmallerThanOrEqual(sourceLength, offset, targetLength)) {
        throwRangeError(globalObject, scope, "Range consisting of offset and length are out of bounds"_s);
        return false;
    }

    // It is not valid to ever call source->get() with an index of more than MAX_ARRAY_INDEX.
    // So we iterate in the optimized loop up to MAX_ARRAY_INDEX, then if there is anything to do beyond this, we rely on slower code.
    size_t safeLength = std::min(sourceLength, static_cast<size_t>(MAX_ARRAY_INDEX) + 1);
    for (size_t i = 0; i < safeLength; ++i) {
        ASSERT(i <= MAX_ARRAY_INDEX);
        JSValue value = source->get(globalObject, static_cast<unsigned>(i));
        RETURN_IF_EXCEPTION(scope, false);
        bool success = setIndex(globalObject, offset + i, value);
        EXCEPTION_ASSERT(!scope.exception() || !success);
        if (UNLIKELY(!success))
            return false;
    }
    for (size_t i = safeLength; i < sourceLength; ++i) {
        JSValue value = source->get(globalObject, static_cast<uint64_t>(i));
        RETURN_IF_EXCEPTION(scope, false);
        bool success = setIndex(globalObject, offset + i, value);
        EXCEPTION_ASSERT(!scope.exception() || !success);
        if (UNLIKELY(!success))
            return false;
    }
    return true;
}

template<typename Adaptor>
RefPtr<typename Adaptor::ViewType> JSGenericTypedArrayView<Adaptor>::possiblySharedTypedImpl()
{
    return Adaptor::ViewType::tryCreate(possiblySharedBuffer(), byteOffsetRaw(), isAutoLength() ? std::nullopt : std::optional { lengthRaw() });
}

template<typename Adaptor>
RefPtr<typename Adaptor::ViewType> JSGenericTypedArrayView<Adaptor>::unsharedTypedImpl()
{
    return Adaptor::ViewType::tryCreate(unsharedBuffer(), byteOffsetRaw(), isAutoLength() ? std::nullopt : std::optional { lengthRaw() });
}

template<typename Adaptor>
ArrayBuffer* JSGenericTypedArrayView<Adaptor>::existingBuffer()
{
    return existingBufferInButterfly();
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::getOwnPropertySlot(
    JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);

    if (std::optional<uint32_t> index = parseIndex(propertyName))
        return getOwnPropertySlotByIndex(thisObject, globalObject, index.value(), slot);

    if (isCanonicalNumericIndexString(propertyName.uid()))
        return false;

    return Base::getOwnPropertySlot(thisObject, globalObject, propertyName, slot);
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::put(
    JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value,
    PutPropertySlot& slot)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);

    if (std::optional<uint32_t> index = parseIndex(propertyName))
        return putByIndex(thisObject, globalObject, index.value(), value, slot.isStrictMode());

    if (isCanonicalNumericIndexString(propertyName.uid())) {
        // Cases like '-0', '1.1', etc. are still obliged to give the RHS a chance to throw.
        toNativeFromValue<Adaptor>(globalObject, value);
        return true;
    }

    return Base::put(thisObject, globalObject, propertyName, value, slot);
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::defineOwnProperty(
    JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName,
    const PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);

    if (std::optional<uint32_t> index = parseIndex(propertyName)) {
        auto throwTypeErrorIfNeeded = [&] (const char* errorMessage) -> bool {
            if (shouldThrow)
                throwTypeError(globalObject, scope, makeString(errorMessage, *index));
            return false;
        };

        if (thisObject->isDetached())
            return typeError(globalObject, scope, shouldThrow, typedArrayBufferHasBeenDetachedErrorMessage);

        if (!thisObject->inBounds(index.value()))
            return throwTypeErrorIfNeeded("Attempting to store out-of-bounds property on a typed array at index: ");

        if (descriptor.isAccessorDescriptor())
            return throwTypeErrorIfNeeded("Attempting to store accessor property on a typed array at index: ");

        if (descriptor.configurablePresent() && !descriptor.configurable())
            return throwTypeErrorIfNeeded("Attempting to store non-configurable property on a typed array at index: ");

        if (descriptor.enumerablePresent() && !descriptor.enumerable())
            return throwTypeErrorIfNeeded("Attempting to store non-enumerable property on a typed array at index: ");

        if (descriptor.writablePresent() && !descriptor.writable())
            return throwTypeErrorIfNeeded("Attempting to store non-writable property on a typed array at index: ");

        scope.release();
        if (descriptor.value())
            thisObject->setIndex(globalObject, index.value(), descriptor.value());

        return true;
    }

    if (isCanonicalNumericIndexString(propertyName.uid()))
        return typeError(globalObject, scope, shouldThrow, "Attempting to store canonical numeric string property on a typed array"_s);

    RELEASE_AND_RETURN(scope, Base::defineOwnProperty(thisObject, globalObject, propertyName, descriptor, shouldThrow));
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::deleteProperty(
    JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);

    if (std::optional<uint32_t> index = parseIndex(propertyName))
        return deletePropertyByIndex(thisObject, globalObject, index.value());

    if (isCanonicalNumericIndexString(propertyName.uid()))
        return true;

    return Base::deleteProperty(thisObject, globalObject, propertyName, slot);
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::getOwnPropertySlotByIndex(
    JSObject* object, JSGlobalObject* globalObject, unsigned propertyName, PropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);

    if (thisObject->isDetached() || !thisObject->inBounds(propertyName))
        return false;

    JSValue value;
    if constexpr (Adaptor::canConvertToJSQuickly) {
        UNUSED_VARIABLE(scope);
        value = thisObject->getIndexQuickly(propertyName);
    } else {
        auto nativeValue = thisObject->getIndexQuicklyAsNativeValue(propertyName);
        value = Adaptor::toJSValue(globalObject, nativeValue);
        RETURN_IF_EXCEPTION(scope, false);
    }
    slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::None), value);
    return true;
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::putByIndex(
    JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName, JSValue value, bool)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);
    thisObject->setIndex(globalObject, propertyName, value);
    return true;
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::deletePropertyByIndex(
    JSCell* cell, JSGlobalObject*, unsigned propertyName)
{
    // Integer-indexed elements can't be deleted, so we must return false when the index is valid.
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);
    return thisObject->isDetached() || !thisObject->inBounds(propertyName);
}

template<typename Adaptor>
void JSGenericTypedArrayView<Adaptor>::getOwnPropertyNames(
    JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& array, DontEnumPropertiesMode mode)
{
    VM& vm = globalObject->vm();
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);

    if (array.includeStringProperties()) {
        uint64_t length = thisObject->length();
        for (uint64_t i = 0; i < length; ++i)
            array.add(Identifier::from(vm, i));
    }
    
    thisObject->getOwnNonIndexPropertyNames(globalObject, array, mode);
}

template<typename Adaptor>
size_t JSGenericTypedArrayView<Adaptor>::estimatedSize(JSCell* cell, VM& vm)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);

    if (thisObject->m_mode == OversizeTypedArray)
        return Base::estimatedSize(thisObject, vm) + thisObject->byteLengthRaw();
    if (thisObject->m_mode == FastTypedArray && thisObject->hasVector())
        return Base::estimatedSize(thisObject, vm) + thisObject->byteLengthRaw();

    return Base::estimatedSize(thisObject, vm);
}

template<typename Adaptor>
template<typename Visitor>
void JSGenericTypedArrayView<Adaptor>::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    TypedArrayMode mode;
    void* vector;
    size_t byteSize;
    
    {
        Locker locker { thisObject->cellLock() };
        mode = thisObject->m_mode;
        vector = thisObject->vector();
        byteSize = thisObject->byteLengthRaw();
    }
    
    switch (mode) {
    case FastTypedArray: {
        if (vector)
            visitor.markAuxiliary(vector);
        break;
    }
        
    case OversizeTypedArray: {
        visitor.reportExtraMemoryVisited(byteSize);
        break;
    }
        
    case WastefulTypedArray:
    case ResizableNonSharedWastefulTypedArray:
    case ResizableNonSharedAutoLengthWastefulTypedArray:
    case GrowableSharedWastefulTypedArray:
    case GrowableSharedAutoLengthWastefulTypedArray:
        break;
        
    case DataViewMode:
    case ResizableNonSharedDataViewMode:
    case ResizableNonSharedAutoLengthDataViewMode:
    case GrowableSharedDataViewMode:
    case GrowableSharedAutoLengthDataViewMode:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

DEFINE_VISIT_CHILDREN_WITH_MODIFIER(template<typename Adaptor>, JSGenericTypedArrayView<Adaptor>);

} // namespace JSC
