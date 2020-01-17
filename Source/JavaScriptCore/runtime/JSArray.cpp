/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
 *  Copyright (C) 2003 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "JSArray.h"

#include "ArrayPrototype.h"
#include "ButterflyInlines.h"
#include "CodeBlock.h"
#include "Error.h"
#include "GetterSetter.h"
#include "IndexingHeaderInlines.h"
#include "JSArrayInlines.h"
#include "JSCInlines.h"
#include "PropertyNameArray.h"
#include "TypeError.h"
#include <wtf/Assertions.h>

namespace JSC {

const ASCIILiteral LengthExceededTheMaximumArrayLengthError { "Length exceeded the maximum array length"_s };

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSArray);

const ClassInfo JSArray::s_info = {"Array", &JSNonFinalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSArray)};

JSArray* JSArray::tryCreateUninitializedRestricted(ObjectInitializationScope& scope, GCDeferralContext* deferralContext, Structure* structure, unsigned initialLength)
{
    VM& vm = scope.vm();

    if (UNLIKELY(initialLength > MAX_STORAGE_VECTOR_LENGTH))
        return nullptr;

    unsigned outOfLineStorage = structure->outOfLineCapacity();
    Butterfly* butterfly;
    IndexingType indexingType = structure->indexingType();
    if (LIKELY(!hasAnyArrayStorage(indexingType))) {
        ASSERT(
            hasUndecided(indexingType)
            || hasInt32(indexingType)
            || hasDouble(indexingType)
            || hasContiguous(indexingType));

        unsigned vectorLength = Butterfly::optimalContiguousVectorLength(structure, initialLength);
        void* temp = vm.jsValueGigacageAuxiliarySpace.allocateNonVirtual(
            vm,
            Butterfly::totalSize(0, outOfLineStorage, true, vectorLength * sizeof(EncodedJSValue)),
            deferralContext, AllocationFailureMode::ReturnNull);
        if (UNLIKELY(!temp))
            return nullptr;
        butterfly = Butterfly::fromBase(temp, 0, outOfLineStorage);
        butterfly->setVectorLength(vectorLength);
        butterfly->setPublicLength(initialLength);
        if (hasDouble(indexingType)) {
            for (unsigned i = initialLength; i < vectorLength; ++i)
                butterfly->contiguousDouble().atUnsafe(i) = PNaN;
        } else {
            for (unsigned i = initialLength; i < vectorLength; ++i)
                butterfly->contiguous().atUnsafe(i).clear();
        }
    } else {
        ASSERT(
            indexingType == ArrayWithSlowPutArrayStorage
            || indexingType == ArrayWithArrayStorage);
        static constexpr unsigned indexBias = 0;
        unsigned vectorLength = ArrayStorage::optimalVectorLength(indexBias, structure, initialLength);
        void* temp = vm.jsValueGigacageAuxiliarySpace.allocateNonVirtual(
            vm,
            Butterfly::totalSize(indexBias, outOfLineStorage, true, ArrayStorage::sizeFor(vectorLength)),
            deferralContext, AllocationFailureMode::ReturnNull);
        if (UNLIKELY(!temp))
            return nullptr;
        butterfly = Butterfly::fromBase(temp, indexBias, outOfLineStorage);
        *butterfly->indexingHeader() = indexingHeaderForArrayStorage(initialLength, vectorLength);
        ArrayStorage* storage = butterfly->arrayStorage();
        storage->m_indexBias = indexBias;
        storage->m_sparseMap.clear();
        storage->m_numValuesInVector = initialLength;
        for (unsigned i = initialLength; i < vectorLength; ++i)
            storage->m_vector[i].clear();
    }

    JSArray* result = createWithButterfly(vm, deferralContext, structure, butterfly);

    const bool createUninitialized = true;
    scope.notifyAllocated(result, createUninitialized);
    return result;
}

void JSArray::eagerlyInitializeButterfly(ObjectInitializationScope& scope, JSArray* array, unsigned initialLength)
{
    Structure* structure = array->structure(scope.vm());
    IndexingType indexingType = structure->indexingType();
    Butterfly* butterfly = array->butterfly();

    // This function only serves as a companion to tryCreateUninitializedRestricted()
    // in the event that we really can't defer initialization of the butterfly after all.
    // tryCreateUninitializedRestricted() already initialized the elements between
    // initialLength and vector length. We just need to do 0 - initialLength.
    // ObjectInitializationScope::notifyInitialized() will verify that all elements are
    // initialized.
    if (LIKELY(!hasAnyArrayStorage(indexingType))) {
        if (hasDouble(indexingType)) {
            for (unsigned i = 0; i < initialLength; ++i)
                butterfly->contiguousDouble().atUnsafe(i) = PNaN;
        } else {
            for (unsigned i = 0; i < initialLength; ++i)
                butterfly->contiguous().atUnsafe(i).clear();
        }
    } else {
        ArrayStorage* storage = butterfly->arrayStorage();
        for (unsigned i = 0; i < initialLength; ++i)
            storage->m_vector[i].clear();
    }
    scope.notifyInitialized(array);
}

void JSArray::setLengthWritable(JSGlobalObject* globalObject, bool writable)
{
    ASSERT(isLengthWritable() || !writable);
    if (!isLengthWritable() || writable)
        return;

    enterDictionaryIndexingMode(globalObject->vm());

    SparseArrayValueMap* map = arrayStorage()->m_sparseMap.get();
    ASSERT(map);
    map->setLengthIsReadOnly();
}

// Defined in ES5.1 15.4.5.1
bool JSArray::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool throwException)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArray* array = jsCast<JSArray*>(object);

    // 3. If P is "length", then
    if (propertyName == vm.propertyNames->length) {
        // All paths through length definition call the default [[DefineOwnProperty]], hence:
        // from ES5.1 8.12.9 7.a.
        if (descriptor.configurablePresent() && descriptor.configurable())
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeConfigurabilityError);
        // from ES5.1 8.12.9 7.b.
        if (descriptor.enumerablePresent() && descriptor.enumerable())
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeEnumerabilityError);

        // a. If the [[Value]] field of Desc is absent, then
        // a.i. Return the result of calling the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length", Desc, and Throw as arguments.
        if (descriptor.isAccessorDescriptor())
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeAccessMechanismError);
        // from ES5.1 8.12.9 10.a.
        if (!array->isLengthWritable() && descriptor.writablePresent() && descriptor.writable())
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeWritabilityError);
        // This descriptor is either just making length read-only, or changing nothing!
        if (!descriptor.value()) {
            if (descriptor.writablePresent())
                array->setLengthWritable(globalObject, descriptor.writable());
            return true;
        }
        
        // b. Let newLenDesc be a copy of Desc.
        // c. Let newLen be ToUint32(Desc.[[Value]]).
        unsigned newLen = descriptor.value().toUInt32(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        // d. If newLen is not equal to ToNumber( Desc.[[Value]]), throw a RangeError exception.
        double valueAsNumber = descriptor.value().toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        if (newLen != valueAsNumber) {
            JSC::throwException(globalObject, scope, createRangeError(globalObject, "Invalid array length"_s));
            return false;
        }

        // Based on SameValue check in 8.12.9, this is always okay.
        // FIXME: Nothing prevents this from being called on a RuntimeArray, and the length function will always return 0 in that case.
        if (newLen == array->length()) {
            if (descriptor.writablePresent())
                array->setLengthWritable(globalObject, descriptor.writable());
            return true;
        }

        // e. Set newLenDesc.[[Value] to newLen.
        // f. If newLen >= oldLen, then
        // f.i. Return the result of calling the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length", newLenDesc, and Throw as arguments.
        // g. Reject if oldLenDesc.[[Writable]] is false.
        if (!array->isLengthWritable())
            return typeError(globalObject, scope, throwException, ReadonlyPropertyChangeError);
        
        // h. If newLenDesc.[[Writable]] is absent or has the value true, let newWritable be true.
        // i. Else,
        // i.i. Need to defer setting the [[Writable]] attribute to false in case any elements cannot be deleted.
        // i.ii. Let newWritable be false.
        // i.iii. Set newLenDesc.[[Writable] to true.
        // j. Let succeeded be the result of calling the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length", newLenDesc, and Throw as arguments.
        // k. If succeeded is false, return false.
        // l. While newLen < oldLen repeat,
        // l.i. Set oldLen to oldLen – 1.
        // l.ii. Let deleteSucceeded be the result of calling the [[Delete]] internal method of A passing ToString(oldLen) and false as arguments.
        // l.iii. If deleteSucceeded is false, then
        bool success = array->setLength(globalObject, newLen, throwException);
        EXCEPTION_ASSERT(!scope.exception() || !success);
        if (!success) {
            // 1. Set newLenDesc.[[Value] to oldLen+1.
            // 2. If newWritable is false, set newLenDesc.[[Writable] to false.
            // 3. Call the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length", newLenDesc, and false as arguments.
            // 4. Reject.
            if (descriptor.writablePresent())
                array->setLengthWritable(globalObject, descriptor.writable());
            return false;
        }

        // m. If newWritable is false, then
        // i. Call the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length",
        //    Property Descriptor{[[Writable]]: false}, and false as arguments. This call will always
        //    return true.
        if (descriptor.writablePresent())
            array->setLengthWritable(globalObject, descriptor.writable());
        // n. Return true.
        return true;
    }

    // 4. Else if P is an array index (15.4), then
    // a. Let index be ToUint32(P).
    if (Optional<uint32_t> optionalIndex = parseIndex(propertyName)) {
        // b. Reject if index >= oldLen and oldLenDesc.[[Writable]] is false.
        uint32_t index = optionalIndex.value();
        // FIXME: Nothing prevents this from being called on a RuntimeArray, and the length function will always return 0 in that case.
        if (index >= array->length() && !array->isLengthWritable())
            return typeError(globalObject, scope, throwException, "Attempting to define numeric property on array with non-writable length property."_s);
        // c. Let succeeded be the result of calling the default [[DefineOwnProperty]] internal method (8.12.9) on A passing P, Desc, and false as arguments.
        // d. Reject if succeeded is false.
        // e. If index >= oldLen
        // e.i. Set oldLenDesc.[[Value]] to index + 1.
        // e.ii. Call the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length", oldLenDesc, and false as arguments. This call will always return true.
        // f. Return true.
        RELEASE_AND_RETURN(scope, array->defineOwnIndexedProperty(globalObject, index, descriptor, throwException));
    }

    RELEASE_AND_RETURN(scope, array->JSObject::defineOwnNonIndexProperty(globalObject, propertyName, descriptor, throwException));
}

bool JSArray::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = globalObject->vm();
    JSArray* thisObject = jsCast<JSArray*>(object);
    if (propertyName == vm.propertyNames->length) {
        unsigned attributes = thisObject->isLengthWritable() ? PropertyAttribute::DontDelete | PropertyAttribute::DontEnum : PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly;
        slot.setValue(thisObject, attributes, jsNumber(thisObject->length()));
        return true;
    }

    return JSObject::getOwnPropertySlot(thisObject, globalObject, propertyName, slot);
}

// ECMA 15.4.5.1
bool JSArray::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArray* thisObject = jsCast<JSArray*>(cell);

    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        RELEASE_AND_RETURN(scope, ordinarySetSlow(globalObject, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode()));

    thisObject->ensureWritable(vm);

    if (propertyName == vm.propertyNames->length) {
        if (!thisObject->isLengthWritable()) {
            if (slot.isStrictMode())
                throwTypeError(globalObject, scope, "Array length is not writable"_s);
            return false;
        }

        unsigned newLength = value.toUInt32(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        double valueAsNumber = value.toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        if (valueAsNumber != static_cast<double>(newLength)) {
            throwException(globalObject, scope, createRangeError(globalObject, "Invalid array length"_s));
            return false;
        }
        RELEASE_AND_RETURN(scope, thisObject->setLength(globalObject, newLength, slot.isStrictMode()));
    }

    RELEASE_AND_RETURN(scope, JSObject::put(thisObject, globalObject, propertyName, value, slot));
}

bool JSArray::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName)
{
    VM& vm = globalObject->vm();
    JSArray* thisObject = jsCast<JSArray*>(cell);

    if (propertyName == vm.propertyNames->length)
        return false;

    return JSObject::deleteProperty(thisObject, globalObject, propertyName);
}

static int compareKeysForQSort(const void* a, const void* b)
{
    unsigned da = *static_cast<const unsigned*>(a);
    unsigned db = *static_cast<const unsigned*>(b);
    return (da > db) - (da < db);
}

void JSArray::getOwnNonIndexPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    JSArray* thisObject = jsCast<JSArray*>(object);

    if (mode.includeDontEnumProperties())
        propertyNames.add(vm.propertyNames->length);

    JSObject::getOwnNonIndexPropertyNames(thisObject, globalObject, propertyNames, mode);
}

// This method makes room in the vector, but leaves the new space for count slots uncleared.
bool JSArray::unshiftCountSlowCase(const AbstractLocker&, VM& vm, DeferGC&, bool addToFront, unsigned count)
{
    ASSERT(cellLock().isLocked());

    ArrayStorage* storage = ensureArrayStorage(vm);
    Butterfly* butterfly = storage->butterfly();
    Structure* structure = this->structure(vm);
    unsigned propertyCapacity = structure->outOfLineCapacity();
    unsigned propertySize = structure->outOfLineSize();
    
    // If not, we should have handled this on the fast path.
    ASSERT(!addToFront || count > storage->m_indexBias);

    // Step 1:
    // Gather 4 key metrics:
    //  * usedVectorLength - how many entries are currently in the vector (conservative estimate - fewer may be in use in sparse vectors).
    //  * requiredVectorLength - how many entries are will there be in the vector, after allocating space for 'count' more.
    //  * currentCapacity - what is the current size of the vector, including any pre-capacity.
    //  * desiredCapacity - how large should we like to grow the vector to - based on 2x requiredVectorLength.

    unsigned length = storage->length();
    unsigned oldVectorLength = storage->vectorLength();
    unsigned usedVectorLength = std::min(oldVectorLength, length);
    ASSERT(usedVectorLength <= MAX_STORAGE_VECTOR_LENGTH);
    // Check that required vector length is possible, in an overflow-safe fashion.
    if (count > MAX_STORAGE_VECTOR_LENGTH - usedVectorLength)
        return false;
    unsigned requiredVectorLength = usedVectorLength + count;
    ASSERT(requiredVectorLength <= MAX_STORAGE_VECTOR_LENGTH);
    // The sum of m_vectorLength and m_indexBias will never exceed MAX_STORAGE_VECTOR_LENGTH.
    ASSERT(storage->vectorLength() <= MAX_STORAGE_VECTOR_LENGTH && (MAX_STORAGE_VECTOR_LENGTH - storage->vectorLength()) >= storage->m_indexBias);
    unsigned currentCapacity = storage->vectorLength() + storage->m_indexBias;
    // The calculation of desiredCapacity won't overflow, due to the range of MAX_STORAGE_VECTOR_LENGTH.
    // FIXME: This code should be fixed to avoid internal fragmentation. It's not super high
    // priority since increaseVectorLength() will "fix" any mistakes we make, but it would be cool
    // to get this right eventually.
    unsigned desiredCapacity = std::min(MAX_STORAGE_VECTOR_LENGTH, std::max(BASE_ARRAY_STORAGE_VECTOR_LEN, requiredVectorLength) << 1);

    // Step 2:
    // We're either going to choose to allocate a new ArrayStorage, or we're going to reuse the existing one.

    void* newAllocBase = nullptr;
    unsigned newStorageCapacity;
    bool allocatedNewStorage;
    // If the current storage array is sufficiently large (but not too large!) then just keep using it.
    if (currentCapacity > desiredCapacity && isDenseEnoughForVector(currentCapacity, requiredVectorLength)) {
        newAllocBase = butterfly->base(structure);
        newStorageCapacity = currentCapacity;
        allocatedNewStorage = false;
    } else {
        const unsigned preCapacity = 0;
        Butterfly* newButterfly = Butterfly::tryCreateUninitialized(vm, this, preCapacity, propertyCapacity, true, ArrayStorage::sizeFor(desiredCapacity));
        if (!newButterfly)
            return false;
        newAllocBase = newButterfly->base(preCapacity, propertyCapacity);
        newStorageCapacity = desiredCapacity;
        allocatedNewStorage = true;
    }

    // Step 3:
    // Work out where we're going to move things to.

    // Determine how much of the vector to use as pre-capacity, and how much as post-capacity.
    // If we're adding to the end, we'll add all the new space to the end.
    // If the vector had no free post-capacity (length >= m_vectorLength), don't give it any.
    // If it did, we calculate the amount that will remain based on an atomic decay - leave the
    // vector with half the post-capacity it had previously.
    unsigned postCapacity = 0;
    if (!addToFront)
        postCapacity = newStorageCapacity - requiredVectorLength;
    else if (length < storage->vectorLength()) {
        // Atomic decay, + the post-capacity cannot be greater than what is available.
        postCapacity = std::min((storage->vectorLength() - length) >> 1, newStorageCapacity - requiredVectorLength);
        // If we're moving contents within the same allocation, the post-capacity is being reduced.
        ASSERT(newAllocBase != butterfly->base(structure) || postCapacity < storage->vectorLength() - length);
    }

    unsigned newVectorLength = requiredVectorLength + postCapacity;
    RELEASE_ASSERT(newVectorLength <= MAX_STORAGE_VECTOR_LENGTH);
    unsigned preCapacity = newStorageCapacity - newVectorLength;

    Butterfly* newButterfly = Butterfly::fromBase(newAllocBase, preCapacity, propertyCapacity);

    if (addToFront) {
        ASSERT(count + usedVectorLength <= newVectorLength);
        gcSafeMemmove(newButterfly->arrayStorage()->m_vector + count, storage->m_vector, sizeof(JSValue) * usedVectorLength);
        gcSafeMemmove(newButterfly->propertyStorage() - propertySize, butterfly->propertyStorage() - propertySize, sizeof(JSValue) * propertySize + sizeof(IndexingHeader) + ArrayStorage::sizeFor(0));

        // We don't need to zero the pre-capacity for the concurrent GC because it is not available to use as property storage.
        gcSafeZeroMemory(static_cast<JSValue*>(newButterfly->base(0, propertyCapacity)), (propertyCapacity - propertySize) * sizeof(JSValue));

        if (allocatedNewStorage) {
            // We will set the vectorLength to newVectorLength. We populated requiredVectorLength
            // (usedVectorLength + count), which is less. Clear the difference.
            for (unsigned i = requiredVectorLength; i < newVectorLength; ++i)
                newButterfly->arrayStorage()->m_vector[i].clear();
        }
    } else if ((newAllocBase != butterfly->base(structure)) || (preCapacity != storage->m_indexBias)) {
        gcSafeMemmove(newButterfly->propertyStorage() - propertyCapacity, butterfly->propertyStorage() - propertyCapacity, sizeof(JSValue) * propertyCapacity + sizeof(IndexingHeader) + ArrayStorage::sizeFor(0));
        gcSafeMemmove(newButterfly->arrayStorage()->m_vector, storage->m_vector, sizeof(JSValue) * usedVectorLength);
        
        for (unsigned i = requiredVectorLength; i < newVectorLength; i++)
            newButterfly->arrayStorage()->m_vector[i].clear();
    }

    newButterfly->arrayStorage()->setVectorLength(newVectorLength);
    newButterfly->arrayStorage()->m_indexBias = preCapacity;
    
    setButterfly(vm, newButterfly);

    return true;
}

bool JSArray::setLengthWithArrayStorage(JSGlobalObject* globalObject, unsigned newLength, bool throwException, ArrayStorage* storage)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = storage->length();
    
    // If the length is read only then we enter sparse mode, so should enter the following 'if'.
    ASSERT(isLengthWritable() || storage->m_sparseMap);

    if (SparseArrayValueMap* map = storage->m_sparseMap.get()) {
        // Fail if the length is not writable.
        if (map->lengthIsReadOnly())
            return typeError(globalObject, scope, throwException, ReadonlyPropertyWriteError);

        if (newLength < length) {
            // Copy any keys we might be interested in into a vector.
            Vector<unsigned, 0, UnsafeVectorOverflow> keys;
            keys.reserveInitialCapacity(std::min(map->size(), static_cast<size_t>(length - newLength)));
            SparseArrayValueMap::const_iterator end = map->end();
            for (SparseArrayValueMap::const_iterator it = map->begin(); it != end; ++it) {
                unsigned index = static_cast<unsigned>(it->key);
                if (index < length && index >= newLength)
                    keys.append(index);
            }

            // Check if the array is in sparse mode. If so there may be non-configurable
            // properties, so we have to perform deletion with caution, if not we can
            // delete values in any order.
            if (map->sparseMode()) {
                qsort(keys.begin(), keys.size(), sizeof(unsigned), compareKeysForQSort);
                unsigned i = keys.size();
                while (i) {
                    unsigned index = keys[--i];
                    SparseArrayValueMap::iterator it = map->find(index);
                    ASSERT(it != map->notFound());
                    if (it->value.attributes() & PropertyAttribute::DontDelete) {
                        storage->setLength(index + 1);
                        return typeError(globalObject, scope, throwException, UnableToDeletePropertyError);
                    }
                    map->remove(it);
                }
            } else {
                for (unsigned i = 0; i < keys.size(); ++i)
                    map->remove(keys[i]);
                if (map->isEmpty())
                    deallocateSparseIndexMap();
            }
        }
    }

    if (newLength < length) {
        // Delete properties from the vector.
        unsigned usedVectorLength = std::min(length, storage->vectorLength());
        for (unsigned i = newLength; i < usedVectorLength; ++i) {
            WriteBarrier<Unknown>& valueSlot = storage->m_vector[i];
            bool hadValue = !!valueSlot;
            valueSlot.clear();
            storage->m_numValuesInVector -= hadValue;
        }
    }

    storage->setLength(newLength);

    return true;
}

bool JSArray::appendMemcpy(JSGlobalObject* globalObject, VM& vm, unsigned startIndex, JSC::JSArray* otherArray)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!canFastCopy(vm, otherArray))
        return false;

    IndexingType type = indexingType();
    IndexingType otherType = otherArray->indexingType();
    IndexingType copyType = mergeIndexingTypeForCopying(otherType);
    if (type == ArrayWithUndecided && copyType != NonArray) {
        if (copyType == ArrayWithInt32)
            convertUndecidedToInt32(vm);
        else if (copyType == ArrayWithDouble)
            convertUndecidedToDouble(vm);
        else if (copyType == ArrayWithContiguous)
            convertUndecidedToContiguous(vm);
        else {
            ASSERT(copyType == ArrayWithUndecided);
            return true;
        }
    } else if (type != copyType)
        return false;

    unsigned otherLength = otherArray->length();
    Checked<unsigned, RecordOverflow> checkedNewLength = startIndex;
    checkedNewLength += otherLength;

    unsigned newLength;
    if (checkedNewLength.safeGet(newLength) == CheckedState::DidOverflow) {
        throwException(globalObject, scope, createRangeError(globalObject, LengthExceededTheMaximumArrayLengthError));
        return false;
    }

    if (newLength >= MIN_SPARSE_ARRAY_INDEX)
        return false;

    if (!ensureLength(vm, newLength)) {
        throwOutOfMemoryError(globalObject, scope);
        return false;
    }
    ASSERT(copyType == indexingType());

    if (UNLIKELY(otherType == ArrayWithUndecided)) {
        auto* butterfly = this->butterfly();
        if (type == ArrayWithDouble) {
            for (unsigned i = startIndex; i < newLength; ++i)
                butterfly->contiguousDouble().at(this, i) = PNaN;
        } else {
            for (unsigned i = startIndex; i < newLength; ++i)
                butterfly->contiguousInt32().at(this, i).setWithoutWriteBarrier(JSValue());
        }
    } else if (type == ArrayWithDouble)
        gcSafeMemcpy(butterfly()->contiguousDouble().data() + startIndex, otherArray->butterfly()->contiguousDouble().data(), sizeof(JSValue) * otherLength);
    else {
        gcSafeMemcpy(butterfly()->contiguous().data() + startIndex, otherArray->butterfly()->contiguous().data(), sizeof(JSValue) * otherLength);
        vm.heap.writeBarrier(this);
    }

    return true;
}

bool JSArray::setLength(JSGlobalObject* globalObject, unsigned newLength, bool throwException)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Butterfly* butterfly = this->butterfly();
    switch (indexingMode()) {
    case ArrayClass:
        if (!newLength)
            return true;
        if (newLength >= MIN_SPARSE_ARRAY_INDEX) {
            RELEASE_AND_RETURN(scope, setLengthWithArrayStorage(
                globalObject, newLength, throwException,
                ensureArrayStorage(vm)));
        }
        createInitialUndecided(vm, newLength);
        return true;

    case CopyOnWriteArrayWithInt32:
    case CopyOnWriteArrayWithDouble:
    case CopyOnWriteArrayWithContiguous:
        if (newLength == butterfly->publicLength())
            return true;
        convertFromCopyOnWrite(vm);
        butterfly = this->butterfly();
        FALLTHROUGH;

    case ArrayWithUndecided:
    case ArrayWithInt32:
    case ArrayWithDouble:
    case ArrayWithContiguous: {
        if (newLength == butterfly->publicLength())
            return true;
        if (newLength > MAX_STORAGE_VECTOR_LENGTH // This check ensures that we can do fast push.
            || (newLength >= MIN_SPARSE_ARRAY_INDEX
                && !isDenseEnoughForVector(newLength, countElements()))) {
            RELEASE_AND_RETURN(scope, setLengthWithArrayStorage(
                globalObject, newLength, throwException,
                ensureArrayStorage(vm)));
        }
        if (newLength > butterfly->publicLength()) {
            if (!ensureLength(vm, newLength)) {
                throwOutOfMemoryError(globalObject, scope);
                return false;
            }
            return true;
        }

        unsigned lengthToClear = butterfly->publicLength() - newLength;
        unsigned costToAllocateNewButterfly = 64; // a heuristic.
        if (lengthToClear > newLength && lengthToClear > costToAllocateNewButterfly) {
            reallocateAndShrinkButterfly(vm, newLength);
            return true;
        }

        if (indexingType() == ArrayWithDouble) {
            for (unsigned i = butterfly->publicLength(); i-- > newLength;)
                butterfly->contiguousDouble().at(this, i) = PNaN;
        } else {
            for (unsigned i = butterfly->publicLength(); i-- > newLength;)
                butterfly->contiguous().at(this, i).clear();
        }
        butterfly->setPublicLength(newLength);
        return true;
    }
        
    case ArrayWithArrayStorage:
    case ArrayWithSlowPutArrayStorage:
        RELEASE_AND_RETURN(scope, setLengthWithArrayStorage(globalObject, newLength, throwException, arrayStorage()));
        
    default:
        CRASH();
        return false;
    }
}

JSValue JSArray::pop(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ensureWritable(vm);

    Butterfly* butterfly = this->butterfly();

    switch (indexingType()) {
    case ArrayClass:
        return jsUndefined();
        
    case ArrayWithUndecided:
        if (!butterfly->publicLength())
            return jsUndefined();
        // We have nothing but holes. So, drop down to the slow version.
        break;
        
    case ArrayWithInt32:
    case ArrayWithContiguous: {
        unsigned length = butterfly->publicLength();
        
        if (!length--)
            return jsUndefined();
        
        RELEASE_ASSERT(length < butterfly->vectorLength());
        JSValue value = butterfly->contiguous().at(this, length).get();
        if (value) {
            butterfly->contiguous().at(this, length).clear();
            butterfly->setPublicLength(length);
            return value;
        }
        break;
    }
        
    case ArrayWithDouble: {
        unsigned length = butterfly->publicLength();
        
        if (!length--)
            return jsUndefined();
        
        RELEASE_ASSERT(length < butterfly->vectorLength());
        double value = butterfly->contiguousDouble().at(this, length);
        if (value == value) {
            butterfly->contiguousDouble().at(this, length) = PNaN;
            butterfly->setPublicLength(length);
            return JSValue(JSValue::EncodeAsDouble, value);
        }
        break;
    }
        
    case ARRAY_WITH_ARRAY_STORAGE_INDEXING_TYPES: {
        ArrayStorage* storage = butterfly->arrayStorage();
    
        unsigned length = storage->length();
        if (!length) {
            if (!isLengthWritable())
                throwTypeError(globalObject, scope, ReadonlyPropertyWriteError);
            return jsUndefined();
        }

        unsigned index = length - 1;
        if (index < storage->vectorLength()) {
            WriteBarrier<Unknown>& valueSlot = storage->m_vector[index];
            if (valueSlot) {
                --storage->m_numValuesInVector;
                JSValue element = valueSlot.get();
                valueSlot.clear();
            
                RELEASE_ASSERT(isLengthWritable());
                storage->setLength(index);
                return element;
            }
        }
        break;
    }
        
    default:
        CRASH();
        return JSValue();
    }
    
    unsigned index = getArrayLength() - 1;
    // Let element be the result of calling the [[Get]] internal method of O with argument indx.
    JSValue element = get(globalObject, index);
    RETURN_IF_EXCEPTION(scope, JSValue());
    // Call the [[Delete]] internal method of O with arguments indx and true.
    bool success = deletePropertyByIndex(this, globalObject, index);
    RETURN_IF_EXCEPTION(scope, JSValue());
    if (!success) {
        throwTypeError(globalObject, scope, UnableToDeletePropertyError);
        return jsUndefined();
    }
    // Call the [[Put]] internal method of O with arguments "length", indx, and true.
    scope.release();
    setLength(globalObject, index, true);
    // Return element.
    return element;
}

// Push & putIndex are almost identical, with two small differences.
//  - we always are writing beyond the current array bounds, so it is always necessary to update m_length & m_numValuesInVector.
//  - pushing to an array of length 2^32-1 stores the property, but throws a range error.
NEVER_INLINE void JSArray::push(JSGlobalObject* globalObject, JSValue value)
{
    pushInline(globalObject, value);
}

JSArray* JSArray::fastSlice(JSGlobalObject* globalObject, unsigned startIndex, unsigned count)
{
    VM& vm = globalObject->vm();

    ensureWritable(vm);

    auto arrayType = indexingMode();
    switch (arrayType) {
    case ArrayWithDouble:
    case ArrayWithInt32:
    case ArrayWithContiguous: {
        if (count >= MIN_SPARSE_ARRAY_INDEX || structure(vm)->holesMustForwardToPrototype(vm, this))
            return nullptr;

        Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(arrayType);
        if (UNLIKELY(hasAnyArrayStorage(resultStructure->indexingType())))
            return nullptr;

        ASSERT(!globalObject->isHavingABadTime());
        ObjectInitializationScope scope(vm);
        JSArray* resultArray = JSArray::tryCreateUninitializedRestricted(scope, resultStructure, count);
        if (UNLIKELY(!resultArray))
            return nullptr;

        auto& resultButterfly = *resultArray->butterfly();
        if (arrayType == ArrayWithDouble)
            gcSafeMemcpy(resultButterfly.contiguousDouble().data(), butterfly()->contiguousDouble().data() + startIndex, sizeof(JSValue) * count);
        else
            gcSafeMemcpy(resultButterfly.contiguous().data(), butterfly()->contiguous().data() + startIndex, sizeof(JSValue) * count);

        ASSERT(resultButterfly.publicLength() == count);
        return resultArray;
    }
    default:
        return nullptr;
    }
}

bool JSArray::shiftCountWithArrayStorage(VM& vm, unsigned startIndex, unsigned count, ArrayStorage* storage)
{
    unsigned oldLength = storage->length();
    RELEASE_ASSERT(count <= oldLength);
    
    // If the array contains holes or is otherwise in an abnormal state,
    // use the generic algorithm in ArrayPrototype.
    if (storage->hasHoles() 
        || hasSparseMap() 
        || shouldUseSlowPut(indexingType())) {
        return false;
    }

    if (!oldLength)
        return true;
    
    unsigned length = oldLength - count;
    
    storage->m_numValuesInVector -= count;
    storage->setLength(length);
    
    unsigned vectorLength = storage->vectorLength();
    if (!vectorLength)
        return true;
    
    if (startIndex >= vectorLength)
        return true;
    
    DisallowGC disallowGC;
    auto locker = holdLock(cellLock());
    
    if (startIndex + count > vectorLength)
        count = vectorLength - startIndex;
    
    unsigned usedVectorLength = std::min(vectorLength, oldLength);
    
    unsigned numElementsBeforeShiftRegion = startIndex;
    unsigned firstIndexAfterShiftRegion = startIndex + count;
    unsigned numElementsAfterShiftRegion = usedVectorLength - firstIndexAfterShiftRegion;
    ASSERT(numElementsBeforeShiftRegion + count + numElementsAfterShiftRegion == usedVectorLength);

    // The point of this comparison seems to be to minimize the amount of elements that have to 
    // be moved during a shift operation.
    if (numElementsBeforeShiftRegion < numElementsAfterShiftRegion) {
        // The number of elements before the shift region is less than the number of elements
        // after the shift region, so we move the elements before to the right.
        if (numElementsBeforeShiftRegion) {
            RELEASE_ASSERT(count + startIndex <= vectorLength);
            gcSafeMemmove(storage->m_vector + count,
                storage->m_vector,
                sizeof(JSValue) * startIndex);
        }
        // Adjust the Butterfly and the index bias. We only need to do this here because we're changing
        // the start of the Butterfly, which needs to point at the first indexed property in the used
        // portion of the vector.
        Butterfly* butterfly = this->butterfly()->shift(structure(vm), count);
        storage = butterfly->arrayStorage();
        storage->m_indexBias += count;

        // Since we're consuming part of the vector by moving its beginning to the left,
        // we need to modify the vector length appropriately.
        storage->setVectorLength(vectorLength - count);
        setButterfly(vm, butterfly);
    } else {
        // The number of elements before the shift region is greater than or equal to the number 
        // of elements after the shift region, so we move the elements after the shift region to the left.
        gcSafeMemmove(storage->m_vector + startIndex,
            storage->m_vector + firstIndexAfterShiftRegion,
            sizeof(JSValue) * numElementsAfterShiftRegion);

        // Clear the slots of the elements we just moved.
        unsigned startOfEmptyVectorTail = usedVectorLength - count;
        for (unsigned i = startOfEmptyVectorTail; i < usedVectorLength; ++i)
            storage->m_vector[i].clear();
        // We don't modify the index bias or the Butterfly pointer in this case because we're not changing 
        // the start of the Butterfly, which needs to point at the first indexed property in the used 
        // portion of the vector. We also don't modify the vector length because we're not actually changing
        // its length; we're just using less of it.
    }
    
    return true;
}

bool JSArray::shiftCountWithAnyIndexingType(JSGlobalObject* globalObject, unsigned& startIndex, unsigned count)
{
    VM& vm = globalObject->vm();
    RELEASE_ASSERT(count > 0);

    ensureWritable(vm);

    Butterfly* butterfly = this->butterfly();
    
    auto indexingType = this->indexingType();
    switch (indexingType) {
    case ArrayClass:
        return true;
        
    case ArrayWithUndecided:
        // Don't handle this because it's confusing and it shouldn't come up.
        return false;
        
    case ArrayWithInt32:
    case ArrayWithContiguous: {
        unsigned oldLength = butterfly->publicLength();
        RELEASE_ASSERT(count <= oldLength);
        
        // We may have to walk the entire array to do the shift. We're willing to do
        // so only if it's not horribly slow.
        if (oldLength - (startIndex + count) >= MIN_SPARSE_ARRAY_INDEX)
            return shiftCountWithArrayStorage(vm, startIndex, count, ensureArrayStorage(vm));

        // Storing to a hole is fine since we're still having a good time. But reading from a hole 
        // is totally not fine, since we might have to read from the proto chain.
        // We have to check for holes before we start moving things around so that we don't get halfway 
        // through shifting and then realize we should have been in ArrayStorage mode.
        unsigned end = oldLength - count;
        if (this->structure(vm)->holesMustForwardToPrototype(vm, this)) {
            for (unsigned i = startIndex; i < end; ++i) {
                JSValue v = butterfly->contiguous().at(this, i + count).get();
                if (UNLIKELY(!v)) {
                    startIndex = i;
                    return shiftCountWithArrayStorage(vm, startIndex, count, ensureArrayStorage(vm));
                }
                butterfly->contiguous().at(this, i).setWithoutWriteBarrier(v);
            }
        } else {
            gcSafeMemmove(butterfly->contiguous().data() + startIndex, 
                butterfly->contiguous().data() + startIndex + count, 
                sizeof(JSValue) * (end - startIndex));
        }

        for (unsigned i = end; i < oldLength; ++i)
            butterfly->contiguous().at(this, i).clear();

        butterfly->setPublicLength(oldLength - count);

        // Our memmoving of values around in the array could have concealed some of them from
        // the collector. Let's make sure that the collector scans this object again.
        if (indexingType == ArrayWithContiguous)
            vm.heap.writeBarrier(this);

        return true;
    }
        
    case ArrayWithDouble: {
        unsigned oldLength = butterfly->publicLength();
        RELEASE_ASSERT(count <= oldLength);
        
        // We may have to walk the entire array to do the shift. We're willing to do
        // so only if it's not horribly slow.
        if (oldLength - (startIndex + count) >= MIN_SPARSE_ARRAY_INDEX)
            return shiftCountWithArrayStorage(vm, startIndex, count, ensureArrayStorage(vm));

        // Storing to a hole is fine since we're still having a good time. But reading from a hole 
        // is totally not fine, since we might have to read from the proto chain.
        // We have to check for holes before we start moving things around so that we don't get halfway 
        // through shifting and then realize we should have been in ArrayStorage mode.
        unsigned end = oldLength - count;
        if (this->structure(vm)->holesMustForwardToPrototype(vm, this)) {
            for (unsigned i = startIndex; i < end; ++i) {
                double v = butterfly->contiguousDouble().at(this, i + count);
                if (UNLIKELY(v != v)) {
                    startIndex = i;
                    return shiftCountWithArrayStorage(vm, startIndex, count, ensureArrayStorage(vm));
                }
                butterfly->contiguousDouble().at(this, i) = v;
            }
        } else {
            gcSafeMemmove(butterfly->contiguousDouble().data() + startIndex,
                butterfly->contiguousDouble().data() + startIndex + count,
                sizeof(JSValue) * (end - startIndex));
        }
        for (unsigned i = end; i < oldLength; ++i)
            butterfly->contiguousDouble().at(this, i) = PNaN;
        
        butterfly->setPublicLength(oldLength - count);
        return true;
    }
        
    case ArrayWithArrayStorage:
    case ArrayWithSlowPutArrayStorage:
        return shiftCountWithArrayStorage(vm, startIndex, count, arrayStorage());
        
    default:
        CRASH();
        return false;
    }
}

// Returns true if the unshift can be handled, false to fallback.    
bool JSArray::unshiftCountWithArrayStorage(JSGlobalObject* globalObject, unsigned startIndex, unsigned count, ArrayStorage* storage)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = storage->length();

    RELEASE_ASSERT(startIndex <= length);

    // If the array contains holes or is otherwise in an abnormal state,
    // use the generic algorithm in ArrayPrototype.
    if (storage->hasHoles() || storage->inSparseMode() || shouldUseSlowPut(indexingType()))
        return false;

    bool moveFront = !startIndex || startIndex < length / 2;

    unsigned vectorLength = storage->vectorLength();

    // Need to have GC deferred around the unshiftCountSlowCase(), since that leaves the butterfly in
    // a weird state: some parts of it will be left uninitialized, which we will fill in here.
    DeferGC deferGC(vm.heap);
    auto locker = holdLock(cellLock());
    
    if (moveFront && storage->m_indexBias >= count) {
        Butterfly* newButterfly = storage->butterfly()->unshift(structure(vm), count);
        storage = newButterfly->arrayStorage();
        storage->m_indexBias -= count;
        storage->setVectorLength(vectorLength + count);
        setButterfly(vm, newButterfly);
    } else if (!moveFront && vectorLength - length >= count)
        storage = storage->butterfly()->arrayStorage();
    else if (unshiftCountSlowCase(locker, vm, deferGC, moveFront, count))
        storage = arrayStorage();
    else {
        throwOutOfMemoryError(globalObject, scope);
        return true;
    }

    WriteBarrier<Unknown>* vector = storage->m_vector;

    if (startIndex) {
        if (moveFront)
            gcSafeMemmove(vector, vector + count, startIndex * sizeof(JSValue));
        else if (length - startIndex)
            gcSafeMemmove(vector + startIndex + count, vector + startIndex, (length - startIndex) * sizeof(JSValue));
    }

    for (unsigned i = 0; i < count; i++)
        vector[i + startIndex].clear();
    
    return true;
}

bool JSArray::unshiftCountWithAnyIndexingType(JSGlobalObject* globalObject, unsigned startIndex, unsigned count)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ensureWritable(vm);

    Butterfly* butterfly = this->butterfly();
    
    switch (indexingType()) {
    case ArrayClass:
    case ArrayWithUndecided:
        // We could handle this. But it shouldn't ever come up, so we won't.
        return false;

    case ArrayWithInt32:
    case ArrayWithContiguous: {
        unsigned oldLength = butterfly->publicLength();
        
        // We may have to walk the entire array to do the unshift. We're willing to do so
        // only if it's not horribly slow.
        if (oldLength - startIndex >= MIN_SPARSE_ARRAY_INDEX)
            RELEASE_AND_RETURN(scope, unshiftCountWithArrayStorage(globalObject, startIndex, count, ensureArrayStorage(vm)));

        Checked<unsigned, RecordOverflow> checkedLength(oldLength);
        checkedLength += count;
        unsigned newLength;
        if (CheckedState::DidOverflow == checkedLength.safeGet(newLength)) {
            throwOutOfMemoryError(globalObject, scope);
            return true;
        }
        if (newLength > MAX_STORAGE_VECTOR_LENGTH)
            return false;
        if (!ensureLength(vm, newLength)) {
            throwOutOfMemoryError(globalObject, scope);
            return true;
        }
        butterfly = this->butterfly();

        // We have to check for holes before we start moving things around so that we don't get halfway 
        // through shifting and then realize we should have been in ArrayStorage mode.
        for (unsigned i = oldLength; i-- > startIndex;) {
            JSValue v = butterfly->contiguous().at(this, i).get();
            if (UNLIKELY(!v))
                RELEASE_AND_RETURN(scope, unshiftCountWithArrayStorage(globalObject, startIndex, count, ensureArrayStorage(vm)));
        }

        for (unsigned i = oldLength; i-- > startIndex;) {
            JSValue v = butterfly->contiguous().at(this, i).get();
            ASSERT(v);
            butterfly->contiguous().at(this, i + count).setWithoutWriteBarrier(v);
        }
        
        // Our memmoving of values around in the array could have concealed some of them from
        // the collector. Let's make sure that the collector scans this object again.
        vm.heap.writeBarrier(this);
        
        // NOTE: we're leaving being garbage in the part of the array that we shifted out
        // of. This is fine because the caller is required to store over that area, and
        // in contiguous mode storing into a hole is guaranteed to behave exactly the same
        // as storing over an existing element.
        
        return true;
    }
        
    case ArrayWithDouble: {
        unsigned oldLength = butterfly->publicLength();
        
        // We may have to walk the entire array to do the unshift. We're willing to do so
        // only if it's not horribly slow.
        if (oldLength - startIndex >= MIN_SPARSE_ARRAY_INDEX)
            RELEASE_AND_RETURN(scope, unshiftCountWithArrayStorage(globalObject, startIndex, count, ensureArrayStorage(vm)));

        Checked<unsigned, RecordOverflow> checkedLength(oldLength);
        checkedLength += count;
        unsigned newLength;
        if (CheckedState::DidOverflow == checkedLength.safeGet(newLength)) {
            throwOutOfMemoryError(globalObject, scope);
            return true;
        }
        if (newLength > MAX_STORAGE_VECTOR_LENGTH)
            return false;
        if (!ensureLength(vm, newLength)) {
            throwOutOfMemoryError(globalObject, scope);
            return true;
        }
        butterfly = this->butterfly();
        
        // We have to check for holes before we start moving things around so that we don't get halfway 
        // through shifting and then realize we should have been in ArrayStorage mode.
        for (unsigned i = oldLength; i-- > startIndex;) {
            double v = butterfly->contiguousDouble().at(this, i);
            if (UNLIKELY(v != v))
                RELEASE_AND_RETURN(scope, unshiftCountWithArrayStorage(globalObject, startIndex, count, ensureArrayStorage(vm)));
        }

        for (unsigned i = oldLength; i-- > startIndex;) {
            double v = butterfly->contiguousDouble().at(this, i);
            ASSERT(v == v);
            butterfly->contiguousDouble().at(this, i + count) = v;
        }
        
        // NOTE: we're leaving being garbage in the part of the array that we shifted out
        // of. This is fine because the caller is required to store over that area, and
        // in contiguous mode storing into a hole is guaranteed to behave exactly the same
        // as storing over an existing element.
        
        return true;
    }
        
    case ArrayWithArrayStorage:
    case ArrayWithSlowPutArrayStorage:
        RELEASE_AND_RETURN(scope, unshiftCountWithArrayStorage(globalObject, startIndex, count, arrayStorage()));
        
    default:
        CRASH();
        return false;
    }
}

void JSArray::fillArgList(JSGlobalObject* globalObject, MarkedArgumentBuffer& args)
{
    unsigned i = 0;
    unsigned vectorEnd;
    WriteBarrier<Unknown>* vector;

    Butterfly* butterfly = this->butterfly();
    
    switch (indexingType()) {
    case ArrayClass:
        return;
        
    case ArrayWithUndecided: {
        vector = 0;
        vectorEnd = 0;
        break;
    }
        
    case ArrayWithInt32:
    case ArrayWithContiguous: {
        vectorEnd = butterfly->publicLength();
        vector = butterfly->contiguous().data();
        break;
    }
        
    case ArrayWithDouble: {
        vector = 0;
        vectorEnd = 0;
        for (; i < butterfly->publicLength(); ++i) {
            double v = butterfly->contiguousDouble().at(this, i);
            if (v != v)
                break;
            args.append(JSValue(JSValue::EncodeAsDouble, v));
        }
        break;
    }
    
    case ARRAY_WITH_ARRAY_STORAGE_INDEXING_TYPES: {
        ArrayStorage* storage = butterfly->arrayStorage();
        
        vector = storage->m_vector;
        vectorEnd = std::min(storage->length(), storage->vectorLength());
        break;
    }
        
    default:
        CRASH();
#if COMPILER_QUIRK(CONSIDERS_UNREACHABLE_CODE)
        vector = 0;
        vectorEnd = 0;
        break;
#endif
    }
    
    for (; i < vectorEnd; ++i) {
        WriteBarrier<Unknown>& v = vector[i];
        if (!v)
            break;
        args.append(v.get());
    }

    // FIXME: What prevents this from being called with a RuntimeArray? The length function will always return 0 in that case.
    for (; i < length(); ++i)
        args.append(get(globalObject, i));
}

void JSArray::copyToArguments(JSGlobalObject* globalObject, JSValue* firstElementDest, unsigned offset, unsigned length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned i = offset;
    WriteBarrier<Unknown>* vector;
    unsigned vectorEnd;
    length += offset; // We like to think of the length as being our length, rather than the output length.

    // FIXME: What prevents this from being called with a RuntimeArray? The length function will always return 0 in that case.
    ASSERT(length == this->length());

    Butterfly* butterfly = this->butterfly();
    switch (indexingType()) {
    case ArrayClass:
        return;
        
    case ArrayWithUndecided: {
        vector = 0;
        vectorEnd = 0;
        break;
    }
        
    case ArrayWithInt32:
    case ArrayWithContiguous: {
        vector = butterfly->contiguous().data();
        vectorEnd = butterfly->publicLength();
        break;
    }
        
    case ArrayWithDouble: {
        vector = 0;
        vectorEnd = 0;
        for (; i < butterfly->publicLength(); ++i) {
            ASSERT(i < butterfly->vectorLength());
            double v = butterfly->contiguousDouble().at(this, i);
            if (v != v)
                break;
            firstElementDest[i - offset] = JSValue(JSValue::EncodeAsDouble, v);
        }
        break;
    }
        
    case ARRAY_WITH_ARRAY_STORAGE_INDEXING_TYPES: {
        ArrayStorage* storage = butterfly->arrayStorage();
        vector = storage->m_vector;
        vectorEnd = std::min(length, storage->vectorLength());
        break;
    }
        
    default:
        CRASH();
#if COMPILER_QUIRK(CONSIDERS_UNREACHABLE_CODE)
        vector = 0;
        vectorEnd = 0;
        break;
#endif
    }
    
    for (; i < vectorEnd; ++i) {
        WriteBarrier<Unknown>& v = vector[i];
        if (!v)
            break;
        firstElementDest[i - offset] = v.get();
    }
    
    for (; i < length; ++i) {
        firstElementDest[i - offset] = get(globalObject, i);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

bool JSArray::isIteratorProtocolFastAndNonObservable()
{
    JSGlobalObject* globalObject = this->globalObject();
    if (!globalObject->isArrayPrototypeIteratorProtocolFastAndNonObservable())
        return false;

    VM& vm = globalObject->vm();
    Structure* structure = this->structure(vm);
    // This is the fast case. Many arrays will be an original array.
    if (globalObject->isOriginalArrayStructure(structure))
        return true;

    if (structure->mayInterceptIndexedAccesses())
        return false;

    if (getPrototypeDirect(vm) != globalObject->arrayPrototype())
        return false;

    if (getDirectOffset(vm, vm.propertyNames->iteratorSymbol) != invalidOffset)
        return false;

    return true;
}

inline JSArray* constructArray(ObjectInitializationScope& scope, Structure* arrayStructure, unsigned length)
{
    JSArray* array = JSArray::tryCreateUninitializedRestricted(scope, arrayStructure, length);

    // FIXME: we should probably throw an out of memory error here, but
    // when making this change we should check that all clients of this
    // function will correctly handle an exception being thrown from here.
    // https://bugs.webkit.org/show_bug.cgi?id=169786
    RELEASE_ASSERT(array);

    // FIXME: We only need this for subclasses of Array because we might need to allocate a new structure to change
    // indexing types while initializing. If this triggered a GC then we might scan our currently uninitialized
    // array and crash. https://bugs.webkit.org/show_bug.cgi?id=186811
    if (!arrayStructure->globalObject()->isOriginalArrayStructure(arrayStructure))
        JSArray::eagerlyInitializeButterfly(scope, array, length);

    return array;
}

JSArray* constructArray(JSGlobalObject* globalObject, Structure* arrayStructure, const ArgList& values)
{
    VM& vm = globalObject->vm();
    unsigned length = values.size();
    ObjectInitializationScope scope(vm);

    JSArray* array = constructArray(scope, arrayStructure, length);
    for (unsigned i = 0; i < length; ++i)
        array->initializeIndex(scope, i, values.at(i));
    return array;
}

JSArray* constructArray(JSGlobalObject* globalObject, Structure* arrayStructure, const JSValue* values, unsigned length)
{
    VM& vm = globalObject->vm();
    ObjectInitializationScope scope(vm);

    JSArray* array = constructArray(scope, arrayStructure, length);
    for (unsigned i = 0; i < length; ++i)
        array->initializeIndex(scope, i, values[i]);
    return array;
}

JSArray* constructArrayNegativeIndexed(JSGlobalObject* globalObject, Structure* arrayStructure, const JSValue* values, unsigned length)
{
    VM& vm = globalObject->vm();
    ObjectInitializationScope scope(vm);

    JSArray* array = constructArray(scope, arrayStructure, length);
    for (int i = 0; i < static_cast<int>(length); ++i)
        array->initializeIndex(scope, i, values[-i]);
    return array;
}

} // namespace JSC
