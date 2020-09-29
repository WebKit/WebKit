/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "DeferGC.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArrayBuffer.h"
#include "JSCellInlines.h"
#include "JSGenericTypedArrayView.h"
#include "TypeError.h"
#include "TypedArrays.h"
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
    JSGlobalObject* globalObject, Structure* structure, unsigned length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ConstructionContext context(vm, structure, length, sizeof(typename Adaptor::Type));
    if (!context) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    JSGenericTypedArrayView* result =
        new (NotNull, allocateCell<JSGenericTypedArrayView>(vm.heap))
        JSGenericTypedArrayView(vm, context);
    result->finishCreation(vm);
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::createWithFastVector(
    JSGlobalObject* globalObject, Structure* structure, unsigned length, void* vector)
{
    VM& vm = globalObject->vm();
    ConstructionContext context(structure, length, vector);
    RELEASE_ASSERT(context);
    JSGenericTypedArrayView* result =
        new (NotNull, allocateCell<JSGenericTypedArrayView>(vm.heap))
        JSGenericTypedArrayView(vm, context);
    result->finishCreation(vm);
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::createUninitialized(JSGlobalObject* globalObject, Structure* structure, unsigned length)
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
        new (NotNull, allocateCell<JSGenericTypedArrayView>(vm.heap))
        JSGenericTypedArrayView(vm, context);
    result->finishCreation(vm);
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::create(
    JSGlobalObject* globalObject, Structure* structure, RefPtr<ArrayBuffer>&& buffer,
    unsigned byteOffset, unsigned length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    size_t size = sizeof(typename Adaptor::Type);
    ASSERT(buffer);
    if (!ArrayBufferView::verifySubRangeLength(*buffer, byteOffset, length, size)) {
        throwException(globalObject, scope, createRangeError(globalObject, "Length out of range of buffer"));
        return nullptr;
    }
    if (!ArrayBufferView::verifyByteOffsetAlignment(byteOffset, size)) {
        throwException(globalObject, scope, createRangeError(globalObject, "Byte offset is not aligned"));
        return nullptr;
    }
    ConstructionContext context(vm, structure, WTFMove(buffer), byteOffset, length);
    ASSERT(context);
    JSGenericTypedArrayView* result =
        new (NotNull, allocateCell<JSGenericTypedArrayView>(vm.heap))
        JSGenericTypedArrayView(vm, context);
    result->finishCreation(vm);
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::create(
    VM& vm, Structure* structure, RefPtr<typename Adaptor::ViewType>&& impl)
{
    ConstructionContext context(vm, structure, impl->possiblySharedBuffer(), impl->byteOffset(), impl->length());
    ASSERT(context);
    JSGenericTypedArrayView* result =
        new (NotNull, allocateCell<JSGenericTypedArrayView>(vm.heap))
        JSGenericTypedArrayView(vm, context);
    result->finishCreation(vm);
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::create(
    Structure* structure, JSGlobalObject* globalObject,
    RefPtr<typename Adaptor::ViewType>&& impl)
{
    return create(globalObject->vm(), structure, WTFMove(impl));
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::validateRange(
    JSGlobalObject* globalObject, unsigned offset, unsigned length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (canAccessRangeQuickly(offset, length))
        return true;
    
    throwException(globalObject, scope, createRangeError(globalObject, "Range consisting of offset and length are out of bounds"));
    return false;
}

template<typename Adaptor>
template<typename OtherAdaptor>
bool JSGenericTypedArrayView<Adaptor>::setWithSpecificType(
    JSGlobalObject* globalObject, unsigned offset, JSGenericTypedArrayView<OtherAdaptor>* other,
    unsigned otherOffset, unsigned length, CopyType type)
{
    // Handle the hilarious case: the act of getting the length could have resulted
    // in neutering. Well, no. That'll never happen because there cannot be
    // side-effects on getting the length of a typed array. But predicting where there
    // are, or aren't, side-effects is a fool's game so we resort to this cheap
    // check. Worst case, if we're wrong, people start seeing less things get copied
    // but we won't have a security vulnerability.
    length = std::min(length, other->length());

    RELEASE_ASSERT(other->canAccessRangeQuickly(otherOffset, length));
    if (!validateRange(globalObject, offset, length))
        return false;
    
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
        || (elementSize == otherElementSize && vector() <= other->vector())
        || type == CopyType::LeftToRight) {
        for (unsigned i = 0; i < length; ++i) {
            setIndexQuicklyToNativeValue(
                offset + i, OtherAdaptor::template convertTo<Adaptor>(
                    other->getIndexQuicklyAsNativeValue(i + otherOffset)));
        }
        return true;
    }

    // Now we either have (2B) or (3) - so first we try to cover (2B).
    if (elementSize == otherElementSize) {
        for (unsigned i = length; i--;) {
            setIndexQuicklyToNativeValue(
                offset + i, OtherAdaptor::template convertTo<Adaptor>(
                    other->getIndexQuicklyAsNativeValue(i + otherOffset)));
        }
        return true;
    }
    
    // Fail: we need an intermediate transfer buffer (i.e. case (3)).
    Vector<typename Adaptor::Type, 32> transferBuffer(length);
    for (unsigned i = length; i--;) {
        transferBuffer[i] = OtherAdaptor::template convertTo<Adaptor>(
            other->getIndexQuicklyAsNativeValue(i + otherOffset));
    }
    for (unsigned i = length; i--;)
        setIndexQuicklyToNativeValue(offset + i, transferBuffer[i]);
    
    return true;
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::set(
    JSGlobalObject* globalObject, unsigned offset, JSObject* object, unsigned objectOffset, unsigned length, CopyType type)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    const ClassInfo* ci = object->classInfo(vm);
    if (ci->typedArrayStorageType == Adaptor::typeValue) {
        // The super fast case: we can just memcpy since we're the same type.
        JSGenericTypedArrayView* other = jsCast<JSGenericTypedArrayView*>(object);
        length = std::min(length, other->length());
        
        RELEASE_ASSERT(other->canAccessRangeQuickly(objectOffset, length));
        bool success = validateRange(globalObject, offset, length);
        EXCEPTION_ASSERT(!scope.exception() == success);
        if (!success)
            return false;

        memmove(typedVector() + offset, other->typedVector() + objectOffset, length * elementSize);
        return true;
    }
    
    switch (ci->typedArrayStorageType) {
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
    case NotTypedArray:
    case TypeDataView: {
        bool success = validateRange(globalObject, offset, length);
        EXCEPTION_ASSERT(!scope.exception() == success);
        if (!success)
            return false;

        // We could optimize this case. But right now, we don't.
        for (unsigned i = 0; i < length; ++i) {
            JSValue value = object->get(globalObject, i + objectOffset);
            RETURN_IF_EXCEPTION(scope, false);
            bool success = setIndex(globalObject, offset + i, value);
            EXCEPTION_ASSERT(!scope.exception() || !success);
            if (!success)
                return false;
        }
        return true;
    } }
    
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

template<typename Adaptor>
RefPtr<typename Adaptor::ViewType> JSGenericTypedArrayView<Adaptor>::possiblySharedTypedImpl()
{
    return Adaptor::ViewType::tryCreate(possiblySharedBuffer(), byteOffset(), length());
}

template<typename Adaptor>
RefPtr<typename Adaptor::ViewType> JSGenericTypedArrayView<Adaptor>::unsharedTypedImpl()
{
    return Adaptor::ViewType::tryCreate(unsharedBuffer(), byteOffset(), length());
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

    if (Optional<uint32_t> index = parseIndex(propertyName)) {
        if (thisObject->isNeutered()) {
            slot.setCustom(thisObject, static_cast<unsigned>(PropertyAttribute::None), throwNeuteredTypedArrayTypeError);
            return true;
        }

        if (thisObject->canGetIndexQuickly(index.value())) {
            slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::DontDelete), thisObject->getIndexQuickly(index.value()));
            return true;
        }

        return false;
    }

    if (isCanonicalNumericIndexString(propertyName)) {
        if (thisObject->isNeutered()) {
            slot.setCustom(thisObject, static_cast<unsigned>(PropertyAttribute::None), throwNeuteredTypedArrayTypeError);
            return true;
        }

        return false;
    }

    return Base::getOwnPropertySlot(thisObject, globalObject, propertyName, slot);
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::put(
    JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value,
    PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);

    // https://tc39.github.io/ecma262/#sec-integer-indexed-exotic-objects-set-p-v-receiver
    // Ignore the receiver even if the receiver is altered to non base value.
    // 9.4.5.5-2-b-i Return ? IntegerIndexedElementSet(O, numericIndex, V).
    if (Optional<uint32_t> index = parseIndex(propertyName))
        RELEASE_AND_RETURN(scope, putByIndex(thisObject, globalObject, index.value(), value, slot.isStrictMode()));

    if (isCanonicalNumericIndexString(propertyName)) {
        if (thisObject->isNeutered())
            throwTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);
        return false;
    }

    RELEASE_AND_RETURN(scope, Base::put(thisObject, globalObject, propertyName, value, slot));
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::defineOwnProperty(
    JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName,
    const PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);

    if (Optional<uint32_t> index = parseIndex(propertyName)) {
        auto throwTypeErrorIfNeeded = [&] (const char* errorMessage) -> bool {
            if (shouldThrow)
                throwTypeError(globalObject, scope, makeString(errorMessage, *index));
            return false;
        };

        if (index.value() >= thisObject->m_length)
            return false;

        if (descriptor.isAccessorDescriptor())
            return throwTypeErrorIfNeeded("Attempting to store accessor property on a typed array at index: ");

        if (descriptor.configurable())
            return throwTypeErrorIfNeeded("Attempting to configure non-configurable property on a typed array at index: ");

        if (descriptor.enumerablePresent() && !descriptor.enumerable())
            return throwTypeErrorIfNeeded("Attempting to store non-enumerable property on a typed array at index: ");

        if (descriptor.writablePresent() && !descriptor.writable())
            return throwTypeErrorIfNeeded("Attempting to store non-writable property on a typed array at index: ");

        if (descriptor.value())
            RELEASE_AND_RETURN(scope, thisObject->putByIndex(thisObject, globalObject, index.value(), descriptor.value(), shouldThrow));

        return true;
    }

    if (isCanonicalNumericIndexString(propertyName))
        return false;

    RELEASE_AND_RETURN(scope, Base::defineOwnProperty(thisObject, globalObject, propertyName, descriptor, shouldThrow));
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::deleteProperty(
    JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);

    if (parseIndex(propertyName)) {
        if (thisObject->isNeutered())
            return typeError(globalObject, scope, true, typedArrayBufferHasBeenDetachedErrorMessage);
        return false;
    }
    
    return Base::deleteProperty(thisObject, globalObject, propertyName, slot);
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::getOwnPropertySlotByIndex(
    JSObject* object, JSGlobalObject*, unsigned propertyName, PropertySlot& slot)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);

    if (thisObject->isNeutered()) {
        slot.setCustom(thisObject, static_cast<unsigned>(PropertyAttribute::None), throwNeuteredTypedArrayTypeError);
        return true;
    }

    if (!thisObject->canGetIndexQuickly(propertyName))
        return false;
    
    slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::DontDelete), thisObject->getIndexQuickly(propertyName));
    return true;
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::putByIndex(
    JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName, JSValue value, bool)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);
    return thisObject->setIndex(globalObject, propertyName, value);
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::deletePropertyByIndex(
    JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName)
{
    VM& vm = globalObject->vm();
    return JSCell::deleteProperty(cell, globalObject, Identifier::from(vm, propertyName));
}

template<typename Adaptor>
void JSGenericTypedArrayView<Adaptor>::getOwnPropertyNames(
    JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& array, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);

    if (array.includeStringProperties()) {
        for (unsigned i = 0; i < thisObject->m_length; ++i)
            array.add(Identifier::from(vm, i));
    }
    
    return Base::getOwnPropertyNames(object, globalObject, array, mode);
}

template<typename Adaptor>
size_t JSGenericTypedArrayView<Adaptor>::estimatedSize(JSCell* cell, VM& vm)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);

    if (thisObject->m_mode == OversizeTypedArray)
        return Base::estimatedSize(thisObject, vm) + thisObject->byteSize();
    if (thisObject->m_mode == FastTypedArray && thisObject->hasVector())
        return Base::estimatedSize(thisObject, vm) + thisObject->byteSize();

    return Base::estimatedSize(thisObject, vm);
}

template<typename Adaptor>
void JSGenericTypedArrayView<Adaptor>::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    TypedArrayMode mode;
    void* vector;
    size_t byteSize;
    
    {
        auto locker = holdLock(thisObject->cellLock());
        mode = thisObject->m_mode;
        vector = thisObject->vector();
        byteSize = thisObject->byteSize();
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
        break;
        
    case DataViewMode:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

} // namespace JSC
