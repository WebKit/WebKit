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

#ifndef JSGenericTypedArrayViewInlines_h
#define JSGenericTypedArrayViewInlines_h

#include "ArrayBufferView.h"
#include "DeferGC.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArrayBuffer.h"
#include "JSGenericTypedArrayView.h"
#include "Reject.h"
#include "TypedArrays.h"

namespace JSC {

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>::JSGenericTypedArrayView(
    VM& vm, ConstructionContext& context)
    : Base(vm, context)
{
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::create(
    ExecState* exec, Structure* structure, unsigned length)
{
    ConstructionContext context(exec->vm(), structure, length, sizeof(typename Adaptor::Type));
    if (!context) {
        exec->vm().throwException(exec, createOutOfMemoryError(exec));
        return 0;
    }
    JSGenericTypedArrayView* result =
        new (NotNull, allocateCell<JSGenericTypedArrayView>(exec->vm().heap))
        JSGenericTypedArrayView(exec->vm(), context);
    result->finishCreation(exec->vm());
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::createUninitialized(
    ExecState* exec, Structure* structure, unsigned length)
{
    ConstructionContext context(
        exec->vm(), structure, length, sizeof(typename Adaptor::Type),
        ConstructionContext::DontInitialize);
    if (!context) {
        exec->vm().throwException(exec, createOutOfMemoryError(exec));
        return 0;
    }
    JSGenericTypedArrayView* result =
        new (NotNull, allocateCell<JSGenericTypedArrayView>(exec->vm().heap))
        JSGenericTypedArrayView(exec->vm(), context);
    result->finishCreation(exec->vm());
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::create(
    ExecState* exec, Structure* structure, PassRefPtr<ArrayBuffer> passedBuffer,
    unsigned byteOffset, unsigned length)
{
    RefPtr<ArrayBuffer> buffer = passedBuffer;
    size_t size = sizeof(typename Adaptor::Type);
    if (!ArrayBufferView::verifySubRangeLength(buffer, byteOffset, length, size)) {
        exec->vm().throwException(exec, createRangeError(exec, "Length out of range of buffer"));
        return nullptr;
    }
    if (!ArrayBufferView::verifyByteOffsetAlignment(byteOffset, size)) {
        exec->vm().throwException(exec, createRangeError(exec, "Byte offset is not aligned"));
        return nullptr;
    }
    ConstructionContext context(exec->vm(), structure, buffer, byteOffset, length);
    ASSERT(context);
    JSGenericTypedArrayView* result =
        new (NotNull, allocateCell<JSGenericTypedArrayView>(exec->vm().heap))
        JSGenericTypedArrayView(exec->vm(), context);
    result->finishCreation(exec->vm());
    return result;
}

template<typename Adaptor>
JSGenericTypedArrayView<Adaptor>* JSGenericTypedArrayView<Adaptor>::create(
    VM& vm, Structure* structure, PassRefPtr<typename Adaptor::ViewType> impl)
{
    RefPtr<ArrayBuffer> buffer = impl->buffer();
    ConstructionContext context(vm, structure, buffer, impl->byteOffset(), impl->length());
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
    PassRefPtr<typename Adaptor::ViewType> impl)
{
    return create(globalObject->vm(), structure, impl);
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::validateRange(
    ExecState* exec, unsigned offset, unsigned length)
{
    if (canAccessRangeQuickly(offset, length))
        return true;
    
    exec->vm().throwException(exec, createRangeError(exec, "Range consisting of offset and length are out of bounds"));
    return false;
}

template<typename Adaptor>
template<typename OtherAdaptor>
bool JSGenericTypedArrayView<Adaptor>::setWithSpecificType(
    ExecState* exec, unsigned offset, JSGenericTypedArrayView<OtherAdaptor>* other,
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
    if (!validateRange(exec, offset, length))
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
    ExecState* exec, unsigned offset, JSObject* object, unsigned objectOffset, unsigned length, CopyType type)
{
    const ClassInfo* ci = object->classInfo();
    if (ci->typedArrayStorageType == Adaptor::typeValue) {
        // The super fast case: we can just memcpy since we're the same type.
        JSGenericTypedArrayView* other = jsCast<JSGenericTypedArrayView*>(object);
        length = std::min(length, other->length());
        
        RELEASE_ASSERT(other->canAccessRangeQuickly(objectOffset, length));
        if (!validateRange(exec, offset, length))
            return false;

        memmove(typedVector() + offset, other->typedVector() + objectOffset, other->byteLength());
        return true;
    }
    
    switch (ci->typedArrayStorageType) {
    case TypeInt8:
        return setWithSpecificType<Int8Adaptor>(
            exec, offset, jsCast<JSInt8Array*>(object), objectOffset, length, type);
    case TypeInt16:
        return setWithSpecificType<Int16Adaptor>(
            exec, offset, jsCast<JSInt16Array*>(object), objectOffset, length, type);
    case TypeInt32:
        return setWithSpecificType<Int32Adaptor>(
            exec, offset, jsCast<JSInt32Array*>(object), objectOffset, length, type);
    case TypeUint8:
        return setWithSpecificType<Uint8Adaptor>(
            exec, offset, jsCast<JSUint8Array*>(object), objectOffset, length, type);
    case TypeUint8Clamped:
        return setWithSpecificType<Uint8ClampedAdaptor>(
            exec, offset, jsCast<JSUint8ClampedArray*>(object), objectOffset, length, type);
    case TypeUint16:
        return setWithSpecificType<Uint16Adaptor>(
            exec, offset, jsCast<JSUint16Array*>(object), objectOffset, length, type);
    case TypeUint32:
        return setWithSpecificType<Uint32Adaptor>(
            exec, offset, jsCast<JSUint32Array*>(object), objectOffset, length, type);
    case TypeFloat32:
        return setWithSpecificType<Float32Adaptor>(
            exec, offset, jsCast<JSFloat32Array*>(object), objectOffset, length, type);
    case TypeFloat64:
        return setWithSpecificType<Float64Adaptor>(
            exec, offset, jsCast<JSFloat64Array*>(object), objectOffset, length, type);
    case NotTypedArray:
    case TypeDataView: {
        if (!validateRange(exec, offset, length))
            return false;

        // We could optimize this case. But right now, we don't.
        for (unsigned i = 0; i < length; ++i) {
            JSValue value = object->get(exec, i + objectOffset);
            if (!setIndex(exec, offset + i, value))
                return false;
        }
        return true;
    } }
    
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

template<typename Adaptor>
ArrayBuffer* JSGenericTypedArrayView<Adaptor>::existingBuffer()
{
    return existingBufferInButterfly();
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::getOwnPropertySlot(
    JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);

    Optional<uint32_t> index = parseIndex(propertyName);
    if (index && thisObject->canGetIndexQuickly(index.value())) {
        slot.setValue(thisObject, DontDelete | ReadOnly, thisObject->getIndexQuickly(index.value()));
        return true;
    }
    
    return Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::put(
    JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value,
    PutPropertySlot& slot)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);
    
    // https://tc39.github.io/ecma262/#sec-integer-indexed-exotic-objects-set-p-v-receiver
    // Ignore the receiver even if the receiver is altered to non base value.
    // 9.4.5.5-2-b-i Return ? IntegerIndexedElementSet(O, numericIndex, V).
    if (Optional<uint32_t> index = parseIndex(propertyName))
        return putByIndex(thisObject, exec, index.value(), value, slot.isStrictMode());
    
    return Base::put(thisObject, exec, propertyName, value, slot);
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::defineOwnProperty(
    JSObject* object, ExecState* exec, PropertyName propertyName,
    const PropertyDescriptor& descriptor, bool shouldThrow)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);
    
    // This is matching Firefox behavior. In particular, it rejects all attempts to
    // defineOwnProperty for indexed properties on typed arrays, even if they're out
    // of bounds.
    if (parseIndex(propertyName))
        return reject(exec, shouldThrow, "Attempting to write to a read-only typed array property.");
    
    return Base::defineOwnProperty(thisObject, exec, propertyName, descriptor, shouldThrow);
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::deleteProperty(
    JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);
    
    if (parseIndex(propertyName))
        return false;
    
    return Base::deleteProperty(thisObject, exec, propertyName);
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::getOwnPropertySlotByIndex(
    JSObject* object, ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);
    
    if (propertyName > MAX_ARRAY_INDEX) {
        return thisObject->methodTable()->getOwnPropertySlot(
            thisObject, exec, Identifier::from(exec, propertyName), slot);
    }
    
    if (!thisObject->canGetIndexQuickly(propertyName))
        return false;
    
    slot.setValue(thisObject, None, thisObject->getIndexQuickly(propertyName));
    return true;
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::putByIndex(
    JSCell* cell, ExecState* exec, unsigned propertyName, JSValue value, bool shouldThrow)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);
    
    if (propertyName > MAX_ARRAY_INDEX) {
        PutPropertySlot slot(JSValue(thisObject), shouldThrow);
        return thisObject->methodTable()->put(thisObject, exec, Identifier::from(exec, propertyName), value, slot);
    }
    
    return thisObject->setIndex(exec, propertyName, value);
}

template<typename Adaptor>
bool JSGenericTypedArrayView<Adaptor>::deletePropertyByIndex(
    JSCell* cell, ExecState* exec, unsigned propertyName)
{
    if (propertyName > MAX_ARRAY_INDEX) {
        return cell->methodTable()->deleteProperty(
            cell, exec, Identifier::from(exec, propertyName));
    }
    
    return false;
}

template<typename Adaptor>
void JSGenericTypedArrayView<Adaptor>::getOwnPropertyNames(
    JSObject* object, ExecState* exec, PropertyNameArray& array, EnumerationMode mode)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);

    if (array.includeStringProperties()) {
        for (unsigned i = 0; i < thisObject->m_length; ++i)
            array.add(Identifier::from(exec, i));
    }
    
    return Base::getOwnPropertyNames(object, exec, array, mode);
}

template<typename Adaptor>
size_t JSGenericTypedArrayView<Adaptor>::estimatedSize(JSCell* cell)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);

    if (thisObject->m_mode == OversizeTypedArray)
        return Base::estimatedSize(thisObject) + thisObject->byteSize();
    if (thisObject->m_mode == FastTypedArray && thisObject->m_vector)
        return Base::estimatedSize(thisObject) + thisObject->byteSize();

    return Base::estimatedSize(thisObject);
}

template<typename Adaptor>
void JSGenericTypedArrayView<Adaptor>::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);
    
    switch (thisObject->m_mode) {
    case FastTypedArray: {
        if (thisObject->m_vector)
            visitor.copyLater(thisObject, TypedArrayVectorCopyToken, thisObject->m_vector.get(), thisObject->byteSize());
        break;
    }
        
    case OversizeTypedArray: {
        visitor.reportExtraMemoryVisited(thisObject->byteSize());
        break;
    }
        
    case WastefulTypedArray:
        break;
        
    case DataViewMode:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    
    Base::visitChildren(thisObject, visitor);
}

template<typename Adaptor>
void JSGenericTypedArrayView<Adaptor>::copyBackingStore(
    JSCell* cell, CopyVisitor& visitor, CopyToken token)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(cell);
    
    if (token == TypedArrayVectorCopyToken
        && visitor.checkIfShouldCopy(thisObject->m_vector.get())) {
        ASSERT(thisObject->m_vector);
        void* oldVector = thisObject->vector();
        void* newVector = visitor.allocateNewSpace(thisObject->byteSize());
        memcpy(newVector, oldVector, thisObject->byteSize());
        thisObject->m_vector.setWithoutBarrier(static_cast<char*>(newVector));
        visitor.didCopy(oldVector, thisObject->byteSize());
    }
    
    Base::copyBackingStore(thisObject, visitor, token);
}

template<typename Adaptor>
ArrayBuffer* JSGenericTypedArrayView<Adaptor>::slowDownAndWasteMemory(JSArrayBufferView* object)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);
    
    // We play this game because we want this to be callable even from places that
    // don't have access to ExecState* or the VM, and we only allocate so little
    // memory here that it's not necessary to trigger a GC - just accounting what
    // we have done is good enough. The sort of bizarro exception to the "allocating
    // little memory" is when we transfer a backing buffer into the C heap; this
    // will temporarily get counted towards heap footprint (incorrectly, in the case
    // of adopting an oversize typed array) but we don't GC here anyway. That's
    // almost certainly fine. The worst case is if you created a ton of fast typed
    // arrays, and did nothing but caused all of them to slow down and waste memory.
    // In that case, your memory footprint will double before the GC realizes what's
    // up. But if you do *anything* to trigger a GC watermark check, it will know
    // that you *had* done those allocations and it will GC appropriately.
    Heap* heap = Heap::heap(thisObject);
    DeferGCForAWhile deferGC(*heap);
    
    ASSERT(!thisObject->hasIndexingHeader());

    size_t size = thisObject->byteSize();
    
    if (thisObject->m_mode == FastTypedArray
        && !thisObject->butterfly() && size >= sizeof(IndexingHeader)) {
        ASSERT(thisObject->m_vector);
        // Reuse already allocated memory if at all possible.
        thisObject->m_butterfly.setWithoutBarrier(
            bitwise_cast<IndexingHeader*>(thisObject->vector())->butterfly());
    } else {
        RELEASE_ASSERT(!thisObject->hasIndexingHeader());
        VM& vm = *heap->vm();
        thisObject->m_butterfly.set(vm, thisObject, Butterfly::createOrGrowArrayRight(
            thisObject->butterfly(), vm, thisObject, thisObject->structure(),
            thisObject->structure()->outOfLineCapacity(), false, 0, 0));
    }

    RefPtr<ArrayBuffer> buffer;
    
    switch (thisObject->m_mode) {
    case FastTypedArray:
        buffer = ArrayBuffer::create(thisObject->vector(), thisObject->byteLength());
        break;
        
    case OversizeTypedArray:
        // FIXME: consider doing something like "subtracting" from extra memory
        // cost, since right now this case will cause the GC to think that we reallocated
        // the whole buffer.
        buffer = ArrayBuffer::createAdopted(thisObject->vector(), thisObject->byteLength());
        break;
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    thisObject->butterfly()->indexingHeader()->setArrayBuffer(buffer.get());
    thisObject->m_vector.setWithoutBarrier(static_cast<char*>(buffer->data()));
    thisObject->m_mode = WastefulTypedArray;
    heap->addReference(thisObject, buffer.get());
    
    return buffer.get();
}

template<typename Adaptor>
PassRefPtr<ArrayBufferView>
JSGenericTypedArrayView<Adaptor>::getTypedArrayImpl(JSArrayBufferView* object)
{
    JSGenericTypedArrayView* thisObject = jsCast<JSGenericTypedArrayView*>(object);
    return thisObject->typedImpl();
}

} // namespace JSC

#endif // JSGenericTypedArrayViewInlines_h

