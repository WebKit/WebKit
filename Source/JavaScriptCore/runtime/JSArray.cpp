/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

#include "ArrayPrototypeInlines.h"
#include "JSArrayInlines.h"
#include "JSCInlines.h"
#include "PropertyNameArray.h"
#include "TypeError.h"
#include <wtf/Assertions.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

const ASCIILiteral LengthExceededTheMaximumArrayLengthError { "Length exceeded the maximum array length"_s };

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSArray);

const ClassInfo JSArray::s_info = { "Array"_s, &JSNonFinalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSArray) };

JSArray* JSArray::tryCreateUninitializedRestricted(ObjectInitializationScope& scope, GCDeferralContext* deferralContext, Structure* structure, unsigned initialLength)
{
    VM& vm = scope.vm();

    if (initialLength > MAX_STORAGE_VECTOR_LENGTH) [[unlikely]]
        return nullptr;

    unsigned outOfLineStorage = structure->outOfLineCapacity();
    Butterfly* butterfly;
    IndexingType indexingType = structure->indexingType();
    if (!hasAnyArrayStorage(indexingType)) [[likely]] {
        ASSERT(
            hasUndecided(indexingType)
            || hasInt32(indexingType)
            || hasDouble(indexingType)
            || hasContiguous(indexingType));

        unsigned vectorLength = Butterfly::optimalContiguousVectorLength(structure, initialLength);
        void* temp = vm.auxiliarySpace().allocate(
            vm,
            Butterfly::totalSize(0, outOfLineStorage, true, vectorLength * sizeof(EncodedJSValue)),
            deferralContext, AllocationFailureMode::ReturnNull);
        if (!temp) [[unlikely]]
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
        void* temp = vm.auxiliarySpace().allocate(
            vm,
            Butterfly::totalSize(indexBias, outOfLineStorage, true, ArrayStorage::sizeFor(vectorLength)),
            deferralContext, AllocationFailureMode::ReturnNull);
        if (!temp) [[unlikely]]
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

    scope.notifyAllocated(result);
    return result;
}

void JSArray::eagerlyInitializeButterfly(ObjectInitializationScope& scope, JSArray* array, unsigned initialLength)
{
    Structure* structure = array->structure();
    IndexingType indexingType = structure->indexingType();
    Butterfly* butterfly = array->butterfly();

    // This function only serves as a companion to tryCreateUninitializedRestricted()
    // in the event that we really can't defer initialization of the butterfly after all.
    // tryCreateUninitializedRestricted() already initialized the elements between
    // initialLength and vector length. We just need to do 0 - initialLength.
    // ObjectInitializationScope::notifyInitialized() will verify that all elements are
    // initialized.
    if (!hasAnyArrayStorage(indexingType)) [[likely]] {
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

// https://tc39.es/ecma262/#sec-array-exotic-objects-defineownproperty-p-desc
bool JSArray::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool throwException)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArray* array = jsCast<JSArray*>(object);

    // 2. If P is "length", then
    // https://tc39.es/ecma262/#sec-arraysetlength
    if (propertyName == vm.propertyNames->length) {
        // FIXME: Nothing prevents this from being called on a RuntimeArray, and the length function will always return 0 in that case.
        unsigned newLength = array->length();
        if (descriptor.value()) {
            newLength = descriptor.value().toUInt32(globalObject);
            RETURN_IF_EXCEPTION(scope, false);
            double valueAsNumber = descriptor.value().toNumber(globalObject);
            RETURN_IF_EXCEPTION(scope, false);
            if (valueAsNumber != static_cast<double>(newLength)) {
                throwRangeError(globalObject, scope, "Invalid array length"_s);
                return false;
            }
        }

        // OrdinaryDefineOwnProperty (https://tc39.es/ecma262/#sec-validateandapplypropertydescriptor) at steps 1.a, 11.a, and 15 is now performed:
        // 4. If current.[[Configurable]] is false, then
        // 4.a. If Desc.[[Configurable]] is present and its value is true, return false.
        if (descriptor.configurablePresent() && descriptor.configurable())
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeConfigurabilityError);
        // 4.b. If Desc.[[Enumerable]] is present and SameValue(Desc.[[Enumerable]], current.[[Enumerable]]) is false, return false.
        if (descriptor.enumerablePresent() && descriptor.enumerable())
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeEnumerabilityError);
        // 6. Else if SameValue(IsDataDescriptor(current), IsDataDescriptor(Desc)) is false, then
        // 6.a. If current.[[Configurable]] is false, return false.
        if (descriptor.isAccessorDescriptor())
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeAccessMechanismError);
        // 7. Else if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both true, then
        // 7.a. If current.[[Configurable]] is false and current.[[Writable]] is false, then
        if (!array->isLengthWritable()) {
            // 7.a.i. If Desc.[[Writable]] is present and Desc.[[Writable]] is true, return false.
            // This check is unaffected by steps 13-14 of ArraySetLength as they change non-writable descriptors only.
            if (descriptor.writablePresent() && descriptor.writable())
                return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeWritabilityError);
            // 7.a.ii. If Desc.[[Value]] is present and SameValue(Desc.[[Value]], current.[[Value]]) is false, return false.
            // This check also covers step 12 of ArraySetLength, which is only reachable if newLen < oldLen.
            if (newLength != array->length())
                return typeError(globalObject, scope, throwException, ReadonlyPropertyChangeError);
        }

        // setLength() clears indices >= newLength and sets correct "length" value if [[Delete]] fails (step 17.b.i)
        bool success = true;
        if (newLength != array->length()) {
            success = array->setLength(globalObject, newLength, throwException);
            EXCEPTION_ASSERT(!scope.exception() || !success);
        }
        if (descriptor.writablePresent())
            array->setLengthWritable(globalObject, descriptor.writable());
        return success;
    }

    // 4. Else if P is an array index (15.4), then
    // a. Let index be ToUint32(P).
    if (std::optional<uint32_t> optionalIndex = parseIndex(propertyName)) {
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

// https://tc39.es/ecma262/#sec-array-exotic-objects-defineownproperty-p-desc
bool JSArray::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArray* thisObject = jsCast<JSArray*>(cell);
    thisObject->ensureWritable(vm);

    if (propertyName == vm.propertyNames->length) {
        if (!thisObject->isLengthWritable()) {
            if (slot.isStrictMode())
                throwTypeError(globalObject, scope, "Array length is not writable"_s);
            return false;
        }

        if (slot.thisValue() != thisObject) [[unlikely]]
            RELEASE_AND_RETURN(scope, JSObject::definePropertyOnReceiver(globalObject, propertyName, value, slot));

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

bool JSArray::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    VM& vm = globalObject->vm();
    JSArray* thisObject = jsCast<JSArray*>(cell);

    if (propertyName == vm.propertyNames->length)
        return false;

    return JSObject::deleteProperty(thisObject, globalObject, propertyName, slot);
}

static int compareKeysForQSort(const void* a, const void* b)
{
    unsigned da = *static_cast<const unsigned*>(a);
    unsigned db = *static_cast<const unsigned*>(b);
    return (da > db) - (da < db);
}

void JSArray::getOwnSpecialPropertyNames(JSObject*, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, DontEnumPropertiesMode mode)
{
    VM& vm = globalObject->vm();
    if (mode == DontEnumPropertiesMode::Include)
        propertyNames.add(vm.propertyNames->length);
}

// This method makes room in the vector, but leaves the new space for count slots uncleared.
bool JSArray::unshiftCountSlowCase(const AbstractLocker&, VM& vm, DeferGC&, bool addToFront, unsigned count)
{
    ASSERT(cellLock().isLocked());

    ArrayStorage* storage = ensureArrayStorage(vm);
    Butterfly* butterfly = storage->butterfly();
    Structure* structure = this->structure();
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

    {
        // When moving Butterfly's head to adjust property-storage, we must take a structure lock.
        // Otherwise, concurrent JIT compiler accesses to a property storage which is half-baked due to move for shift / unshift.
        // If the butterfly is newly allocated one, we do not need to take a lock since this is not changing the old butterfly.
        ConcurrentJSLocker structureLock(allocatedNewStorage ? nullptr : &structure->lock());
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
    }

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

bool JSArray::fastFill(VM& vm, unsigned startIndex, unsigned endIndex, JSValue value)
{
    if (isCopyOnWrite(indexingMode()))
        convertFromCopyOnWrite(vm);

    IndexingType type = indexingType();
    if (!(type & IsArray) || hasAnyArrayStorage(type))
        return false;
    IndexingType nextType = leastUpperBoundOfIndexingTypeAndValue(type, value);
    if (hasArrayStorage(nextType))
        return false;
    convertToIndexingTypeIfNeeded(vm, nextType);

    ASSERT(nextType == indexingType());

    // There is a chance that endIndex is beyond the length. If it is, let's just fail.
    if (endIndex > this->butterfly()->publicLength())
        return false;

    if (nextType == ArrayWithDouble) {
        auto* data = butterfly()->contiguousDouble().data();
        double pattern = value.asNumber();
#if OS(DARWIN)
        memset_pattern8(data + startIndex, &pattern, sizeof(double) * (endIndex - startIndex));
#else
        std::fill(data + startIndex, data + endIndex, pattern);
#endif
    } else if (nextType == ArrayWithInt32) {
        auto* data = butterfly()->contiguous().data();
        auto pattern = std::bit_cast<const WriteBarrier<Unknown>>(JSValue::encode(value));
#if OS(DARWIN)
        memset_pattern8(data + startIndex, &pattern, sizeof(JSValue) * (endIndex - startIndex));
#else
        std::fill(data + startIndex, data + endIndex, pattern);
#endif
        vm.writeBarrier(this);
    } else {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=283786
        auto contiguousStorage = butterfly()->contiguous();
        for (unsigned i = startIndex; i < endIndex; ++i)
            contiguousStorage.at(this, i).setWithoutWriteBarrier(value);
        vm.writeBarrier(this);
    }

    return true;
}

JSArray* JSArray::fastToReversed(JSGlobalObject* globalObject, uint64_t length)
{
    ASSERT(length <= std::numeric_limits<uint32_t>::max());

    VM& vm = globalObject->vm();

    auto sourceType = indexingType();
    switch (sourceType) {
    case ArrayWithInt32:
    case ArrayWithContiguous:
    case ArrayWithDouble: {
        if (length > this->butterfly()->vectorLength()) [[unlikely]]
            return nullptr;
        if (holesMustForwardToPrototype()) [[unlikely]]
            return nullptr;

        IndexingType resultType = sourceType;
        if (sourceType == ArrayWithDouble) {
            auto* buffer = this->butterfly()->contiguousDouble().data();
            if (containsHole(buffer, length)) [[unlikely]]
                resultType = ArrayWithContiguous;
        } else if (sourceType == ArrayWithInt32) {
            auto* buffer = this->butterfly()->contiguousInt32().data();
            if (containsHole(buffer, length)) [[unlikely]]
                resultType = ArrayWithContiguous;
        }

        Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(resultType);
        IndexingType indexingType = resultStructure->indexingType();
        if (hasAnyArrayStorage(indexingType)) [[unlikely]]
            return nullptr;
        ASSERT(!globalObject->isHavingABadTime());

        auto vectorLength = Butterfly::optimalContiguousVectorLength(resultStructure, length);
        void* memory = vm.auxiliarySpace().allocate(
            vm,
            Butterfly::totalSize(0, 0, true, vectorLength * sizeof(EncodedJSValue)),
            nullptr, AllocationFailureMode::ReturnNull);
        if (!memory) [[unlikely]]
            return nullptr;
        auto* butterfly = Butterfly::fromBase(memory, 0, 0);
        butterfly->setVectorLength(vectorLength);
        butterfly->setPublicLength(length);

        if (hasDouble(indexingType)) {
            ASSERT(!containsHole(this->butterfly()->contiguousDouble().data(), length));
            auto* sourceBuffer = this->butterfly()->contiguousDouble().data();
            auto* resultBuffer = butterfly->contiguousDouble().data();
            copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(resultBuffer, 0, sourceBuffer, 0, length, sourceType);
            std::reverse(resultBuffer, resultBuffer + length);
        } else if (hasInt32(indexingType)) {
            ASSERT(!containsHole(this->butterfly()->contiguous().data(), length));
            auto* sourceBuffer = this->butterfly()->contiguous().data();
            auto* resultBuffer = butterfly->contiguous().data();
            copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(resultBuffer, 0, sourceBuffer, 0, length, sourceType);
            std::reverse(resultBuffer, resultBuffer + length);
        } else {
            auto* resultBuffer = butterfly->contiguous().data();
            if (sourceType == ArrayWithDouble) {
                auto* sourceBuffer = this->butterfly()->contiguousDouble().data();
                copyArrayElements<ArrayFillMode::Undefined, NeedsGCSafeOps::No>(resultBuffer, 0, sourceBuffer, 0, length, sourceType);
            } else {
                auto* sourceBuffer = this->butterfly()->contiguous().data();
                copyArrayElements<ArrayFillMode::Undefined, NeedsGCSafeOps::No>(resultBuffer, 0, sourceBuffer, 0, length, sourceType);
            }
            std::reverse(resultBuffer, resultBuffer + length);
        }
        Butterfly::clearOptimalVectorLengthGap(resultType, butterfly, vectorLength, length);
        return createWithButterfly(vm, nullptr, resultStructure, butterfly);
    }
    case ArrayWithArrayStorage: {
        auto& storage = *this->butterfly()->arrayStorage();
        if (storage.m_sparseMap.get())
            return nullptr;
        if (length > storage.vectorLength())
            return nullptr;
        if (storage.hasHoles())
            return nullptr;

        Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);
        if (hasAnyArrayStorage(resultStructure->indexingType())) [[unlikely]]
            return nullptr;

        ASSERT(!globalObject->isHavingABadTime());
        ObjectInitializationScope scope(vm);
        JSArray* resultArray = JSArray::tryCreateUninitializedRestricted(scope, resultStructure, length);
        if (!resultArray) [[unlikely]]
            return nullptr;
        gcSafeMemcpy(resultArray->butterfly()->contiguous().data(), this->butterfly()->arrayStorage()->m_vector, sizeof(JSValue) * static_cast<uint32_t>(length));
        ASSERT(resultArray->butterfly()->publicLength() == length);

        auto data = resultArray->butterfly()->contiguous().data();
        std::reverse(data, data + length);
        vm.writeBarrier(resultArray);

        return resultArray;
    }
    default:
        return nullptr;
    }
}

JSArray* JSArray::fastWith(JSGlobalObject* globalObject, uint32_t index, JSValue value, uint64_t length)
{
    ASSERT(length <= std::numeric_limits<uint32_t>::max());

    VM& vm = globalObject->vm();

    auto sourceType = indexingType();
    switch (sourceType) {
    case ArrayWithInt32:
    case ArrayWithContiguous:
    case ArrayWithDouble: {
        if (length > this->butterfly()->vectorLength()) [[unlikely]]
            return nullptr;
        if (holesMustForwardToPrototype()) [[unlikely]]
            return nullptr;

        IndexingType resultType = leastUpperBoundOfIndexingTypeAndValue(sourceType, value);
        if (sourceType == ArrayWithDouble) {
            auto* buffer = this->butterfly()->contiguousDouble().data();
            if (containsHole(buffer, length)) [[unlikely]]
                resultType = ArrayWithContiguous;
        } else if (sourceType == ArrayWithInt32) {
            auto* buffer = this->butterfly()->contiguousInt32().data();
            if (containsHole(buffer, length)) [[unlikely]]
                resultType = ArrayWithContiguous;
        }

        Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(resultType);
        IndexingType indexingType = resultStructure->indexingType();
        if (hasAnyArrayStorage(indexingType)) [[unlikely]]
            return nullptr;
        ASSERT(!globalObject->isHavingABadTime());

        auto vectorLength = Butterfly::optimalContiguousVectorLength(resultStructure, length);
        void* memory = vm.auxiliarySpace().allocate(
            vm,
            Butterfly::totalSize(0, 0, true, vectorLength * sizeof(EncodedJSValue)),
            nullptr, AllocationFailureMode::ReturnNull);
        if (!memory) [[unlikely]]
            return nullptr;
        auto* butterfly = Butterfly::fromBase(memory, 0, 0);
        butterfly->setVectorLength(vectorLength);
        butterfly->setPublicLength(length);

        if (hasDouble(indexingType)) {
            auto* resultBuffer = butterfly->contiguousDouble().data();
            if (sourceType == ArrayWithDouble) {
                auto* sourceBuffer = this->butterfly()->contiguousDouble().data();
                copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(resultBuffer, 0, sourceBuffer, 0, length, sourceType);
            } else {
                ASSERT(sourceType == ArrayWithInt32);
                auto* sourceBuffer = this->butterfly()->contiguous().data();
                copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(resultBuffer, 0, sourceBuffer, 0, length, sourceType);
            }
            resultBuffer[index] = value.asNumber();
        } else if (hasInt32(indexingType)) {
            ASSERT(sourceType == ArrayWithInt32);
            auto* sourceBuffer = this->butterfly()->contiguous().data();
            auto* resultBuffer = butterfly->contiguous().data();
            copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(resultBuffer, 0, sourceBuffer, 0, length, sourceType);
            resultBuffer[index].setWithoutWriteBarrier(value);
        } else {
            auto* resultBuffer = butterfly->contiguous().data();
            if (sourceType == ArrayWithDouble) {
                auto* sourceBuffer = this->butterfly()->contiguousDouble().data();
                copyArrayElements<ArrayFillMode::Undefined, NeedsGCSafeOps::No>(resultBuffer, 0, sourceBuffer, 0, length, sourceType);
            } else {
                auto* sourceBuffer = this->butterfly()->contiguous().data();
                copyArrayElements<ArrayFillMode::Undefined, NeedsGCSafeOps::No>(resultBuffer, 0, sourceBuffer, 0, length, sourceType);
            }
            resultBuffer[index].setWithoutWriteBarrier(value);
        }

        Butterfly::clearOptimalVectorLengthGap(indexingType, butterfly, vectorLength, length);
        return createWithButterfly(vm, nullptr, resultStructure, butterfly);
    }
    case ArrayWithArrayStorage: {
        auto& storage = *this->butterfly()->arrayStorage();
        if (storage.m_sparseMap.get())
            return nullptr;
        if (length > storage.vectorLength())
            return nullptr;
        if (storage.hasHoles())
            return nullptr;

        Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);
        if (hasAnyArrayStorage(resultStructure->indexingType())) [[unlikely]]
            return nullptr;

        ASSERT(!globalObject->isHavingABadTime());
        ObjectInitializationScope scope(vm);
        JSArray* resultArray = JSArray::tryCreateUninitializedRestricted(scope, resultStructure, length);
        if (!resultArray) [[unlikely]]
            return nullptr;
        gcSafeMemcpy(resultArray->butterfly()->contiguous().data(), this->butterfly()->arrayStorage()->m_vector, sizeof(JSValue) * static_cast<uint32_t>(length));
        ASSERT(resultArray->butterfly()->publicLength() == length);

        resultArray->butterfly()->contiguous().at(resultArray, index).setWithoutWriteBarrier(value);
        vm.writeBarrier(resultArray);

        return resultArray;
    }
    default:
        return nullptr;
    }
}

std::optional<bool> JSArray::fastIncludes(JSGlobalObject* globalObject, JSValue searchElement, uint64_t index64, uint64_t length64)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool canDoFastPath = this->canDoFastIndexedAccess()
        && this->getArrayLength() == length64 // The effects in getting `index` could have changed the length of this array.
        && static_cast<uint32_t>(index64) == index64;
    if (!canDoFastPath)
        return std::nullopt;

    uint32_t length = static_cast<uint32_t>(length64);
    uint32_t index = static_cast<uint32_t>(index64);

    switch (this->indexingType()) {
    case ArrayWithInt32: {
        auto& butterfly = *this->butterfly();
        auto data = butterfly.contiguous().data();

        int32_t int32Value = 0;
        if (searchElement.isInt32AsAnyInt())
            int32Value = searchElement.asInt32AsAnyInt();
        else if (searchElement.isUndefined()) [[unlikely]]
            return containsHole(data, length64);
        else if (!searchElement.isNumber() || searchElement.asNumber() != 0.0) [[unlikely]]
            return false;

        EncodedJSValue encodedSearchElement = JSValue::encode(jsNumber(int32Value));
        auto* result = WTF::find64(std::bit_cast<const uint64_t*>(data + index), encodedSearchElement, length - index);
        return static_cast<bool>(result);
    }
    case ArrayWithContiguous: {
        auto& butterfly = *this->butterfly();
        auto data = butterfly.contiguous().data();

        if (searchElement.isObject()) {
            auto* result = std::bit_cast<const WriteBarrier<Unknown>*>(WTF::find64(std::bit_cast<const uint64_t*>(data + index), JSValue::encode(searchElement), length - index));
            if (result)
                return true;
            return false;
        }

        bool searchElementIsUndefined = searchElement.isUndefined();
        for (; index < length; ++index) {
            JSValue value = data[index].get();
            if (!value) {
                if (searchElementIsUndefined)
                    return true;
                continue;
            }
            bool isEqual = sameValueZero(globalObject, searchElement, value);
            RETURN_IF_EXCEPTION(scope, { });
            if (isEqual)
                return true;
        }
        return false;
    }
    case ALL_DOUBLE_INDEXING_TYPES: {
        auto& butterfly = *this->butterfly();
        auto data = butterfly.contiguousDouble().data();

        if (searchElement.isUndefined())
            return containsHole(data, length64);
        if (!searchElement.isNumber())
            return false;

        double searchNumber = searchElement.asNumber();
        for (; index < length; ++index) {
            if (data[index] == searchNumber)
                return true;
        }
        return false;
    }
    default:
        return std::nullopt;
    }
}

bool JSArray::fastCopyWithin(JSGlobalObject* globalObject, uint64_t from64, uint64_t to64, uint64_t count64, uint64_t length64)
{
    VM& vm = globalObject->vm();

    uint32_t from = static_cast<uint32_t>(from64);
    uint32_t to = static_cast<uint32_t>(to64);
    uint32_t count = static_cast<uint32_t>(count64);
    uint32_t length = static_cast<uint32_t>(length64);

    ASSERT(from + count <= length);
    ASSERT(to + count <= length);

    bool canDoFastPath = this->canDoFastIndexedAccess()
        && this->getArrayLength() == length
        && from64 == from
        && to64 == to
        && count64 == count;

    if (!canDoFastPath)
        return false;

    if (isCopyOnWrite(indexingMode()))
        convertFromCopyOnWrite(vm);

    auto type = this->indexingType();
    switch (type) {
    case ArrayWithInt32:
    case ArrayWithContiguous: {
        auto data = this->butterfly()->contiguous().data();

        if (containsHole(data, length))
            return false;

        std::span<WriteBarrier<Unknown>> destination { data + to, count };
        std::span<const WriteBarrier<Unknown>> source { data + from, count };

        if (type == ArrayWithInt32)
            memmoveSpan(destination, source);
        else {
            ASSERT(type == ArrayWithContiguous);
            gcSafeMemmove(destination.data(), source.data(), count * sizeof(JSValue));
            vm.writeBarrier(this);
        }
        return true;
    }
    case ArrayWithDouble: {
        auto data = this->butterfly()->contiguousDouble().data();

        if (containsHole(data, length))
            return false;

        std::span<double> destination { data + to, count };
        std::span<double> source { data + from, count };

        memmoveSpan(destination, source);
        return true;
    }
    case ArrayWithArrayStorage: {
        auto& storage = *this->butterfly()->arrayStorage();
        if (storage.m_sparseMap.get())
            return false;
        if (length > storage.vectorLength())
            return false;
        if (storage.hasHoles())
            return false;
        ASSERT(!globalObject->isHavingABadTime());

        auto vector = this->butterfly()->arrayStorage()->m_vector;
        gcSafeMemmove(vector + to, vector + from, count * sizeof(JSValue));
        vm.writeBarrier(this);

        return true;
    }
    default:
        return false;
    }
}

JSArray* JSArray::fastToSpliced(JSGlobalObject* globalObject, CallFrame* callFrame, uint64_t length, uint64_t newLength, uint64_t start, uint64_t deleteCount, uint64_t insertCount)
{
    VM& vm = globalObject->vm();

    IndexingType sourceType = indexingType();
    switch (sourceType) {
    case ArrayWithInt32:
    case ArrayWithContiguous:
    case ArrayWithDouble: {
        if (newLength > MAX_STORAGE_VECTOR_LENGTH) [[unlikely]]
            return nullptr;

        if (newLength >= MIN_SPARSE_ARRAY_INDEX) [[unlikely]]
            return nullptr;

        if (length > this->butterfly()->vectorLength()) [[unlikely]]
            return nullptr;

        if (hasDouble(sourceType)) {
            if (containsHole(this->butterfly()->contiguousDouble().data(), static_cast<uint32_t>(length)))
                return nullptr;
        } else if (containsHole(this->butterfly()->contiguous().data(), static_cast<uint32_t>(length)))
            return nullptr;

        IndexingType insertedItemsIndexingType = sourceType;
        for (uint64_t i = 0; i < insertCount; i++)
            insertedItemsIndexingType = leastUpperBoundOfIndexingTypeAndValue(insertedItemsIndexingType, callFrame->uncheckedArgument(i + 2));

        Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(insertedItemsIndexingType);
        IndexingType resultIndexingType = resultStructure->indexingType();

        if (hasAnyArrayStorage(resultIndexingType)) [[unlikely]]
            return nullptr;
        ASSERT(!globalObject->isHavingABadTime());

        auto vectorLength = Butterfly::optimalContiguousVectorLength(resultStructure, newLength);
        void* memory = vm.auxiliarySpace().allocate(
            vm,
            Butterfly::totalSize(0, 0, true, vectorLength * sizeof(EncodedJSValue)),
            nullptr, AllocationFailureMode::ReturnNull);
        if (!memory) [[unlikely]]
            return nullptr;
        auto* resultButterfly = Butterfly::fromBase(memory, 0, 0);
        resultButterfly->setVectorLength(vectorLength);
        resultButterfly->setPublicLength(newLength);

        auto copyArrayPrefixElements = [&]<typename T, typename U>(T* resultBuffer, U* sourceBuffer) {
            copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(resultBuffer, 0, sourceBuffer, 0, start, sourceType);
        };
        auto copyArraySuffixElements = [&]<typename T, typename U>(T* resultBuffer, U* sourceBuffer) {
            copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(resultBuffer, start + insertCount, sourceBuffer, start + deleteCount, length - start - deleteCount, sourceType);
        };

        auto* sourceButterfly = this->butterfly();
        if (hasDouble(resultIndexingType)) {
            ASSERT(sourceType == ArrayWithInt32 || sourceType == ArrayWithDouble);
            double* resultBuffer = resultButterfly->contiguousDouble().data();
            if (sourceType == ArrayWithDouble)
                copyArrayPrefixElements(resultBuffer, sourceButterfly->contiguousDouble().data());
            else
                copyArrayPrefixElements(resultBuffer, sourceButterfly->contiguous().data());
            for (uint64_t i = 0; i < insertCount; ++i) {
                JSValue value = callFrame->uncheckedArgument(i + 2);
                ASSERT(value.isNumber());
                resultButterfly->contiguousDouble().atUnsafe(start + i) = value.asNumber();
            }
            if (sourceType == ArrayWithDouble)
                copyArraySuffixElements(resultBuffer, sourceButterfly->contiguousDouble().data());
            else
                copyArraySuffixElements(resultBuffer, sourceButterfly->contiguous().data());
        } else if (hasInt32(resultIndexingType) || hasContiguous(resultIndexingType)) {
            auto* resultBuffer = resultButterfly->contiguous().data();
            if (sourceType == ArrayWithDouble)
                copyArrayPrefixElements(resultBuffer, sourceButterfly->contiguousDouble().data());
            else
                copyArrayPrefixElements(resultBuffer, sourceButterfly->contiguous().data());
            for (uint64_t i = 0; i < insertCount; ++i) {
                JSValue value = callFrame->uncheckedArgument(i + 2);
                resultButterfly->contiguous().atUnsafe(start + i).setWithoutWriteBarrier(value);
            }
            if (sourceType == ArrayWithDouble)
                copyArraySuffixElements(resultBuffer, sourceButterfly->contiguousDouble().data());
            else
                copyArraySuffixElements(resultBuffer, sourceButterfly->contiguous().data());
        } else
            RELEASE_ASSERT_NOT_REACHED();

        Butterfly::clearOptimalVectorLengthGap(resultIndexingType, resultButterfly, vectorLength, newLength);
        return createWithButterfly(vm, nullptr, resultStructure, resultButterfly);
    }
    default: {
        return nullptr;
    }
    }
}

JSString* JSArray::fastToString(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = this->length();

    StringRecursionChecker checker(globalObject, this);
    EXCEPTION_ASSERT(!scope.exception() || checker.earlyReturnValue());
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return jsEmptyString(vm);

    if (canUseFastArrayJoin(this)) [[likely]] {
        const LChar comma = ',';

        bool isCoW = isCopyOnWrite(this->indexingMode());
        JSCellButterfly* immutableButterfly = nullptr;
        if (isCoW) {
            immutableButterfly = JSCellButterfly::fromButterfly(this->butterfly());
            auto iter = vm.heap.immutableButterflyToStringCache.find(immutableButterfly);
            if (iter != vm.heap.immutableButterflyToStringCache.end())
                return iter->value;
        }

        bool sawHoles = false;
        bool genericCase = false;
        JSString* result = fastArrayJoin(globalObject, this, span(comma), length, sawHoles, genericCase);
        RETURN_IF_EXCEPTION(scope, { });

        if (!sawHoles && !genericCase && result && isCoW) {
            ASSERT(JSCellButterfly::fromButterfly(this->butterfly()) == immutableButterfly);
            vm.heap.immutableButterflyToStringCache.add(immutableButterfly, jsCast<JSString*>(result));
        }

        return result;
    }

    JSStringJoiner joiner(","_s);
    for (unsigned i = 0; i < length; ++i) {
        JSValue element = this->tryGetIndexQuickly(i);
        if (!element) {
            element = this->get(globalObject, i);
            RETURN_IF_EXCEPTION(scope, { });
        }
        joiner.append(globalObject, element);
        RETURN_IF_EXCEPTION(scope, { });
    }

    RELEASE_AND_RETURN(scope, joiner.join(globalObject));
}

static uint64_t calculateFlattenedLength(JSGlobalObject* globalObject, JSArray* sourceArray, uint64_t sourceLength, uint64_t depth)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!vm.isSafeToRecurseSoft()) [[unlikely]] {
        throwStackOverflowError(globalObject, scope);
        return std::numeric_limits<uint64_t>::max();
    }

    CheckedUint64 resultLength = 0;
    auto lengthExceeded = [&]() -> bool {
        return resultLength.hasOverflowed() || resultLength > maxSafeIntegerAsUInt64();
    };

    IndexingType sourceType = sourceArray->indexingType();
    switch (sourceType) {
    case ArrayWithInt32: {
        auto* sourceBuffer = sourceArray->butterfly()->contiguous().data();
        for (uint64_t i = 0; i < sourceLength; ++i) {
            JSValue element = sourceBuffer[i].get();
            if (!element) [[unlikely]]
                continue;
            resultLength++;
            if (lengthExceeded()) [[unlikely]] {
                throwTypeError(globalObject, scope, "flatten array exceeds 2**52 - 1");
                return std::numeric_limits<uint64_t>::max();
            }
        }
        break;
    }
    case ArrayWithContiguous: {
        auto* sourceBuffer = sourceArray->butterfly()->contiguous().data();
        for (uint64_t i = 0; i < sourceLength; ++i) {
            JSValue element = sourceBuffer[i].get();
            if (!element) [[unlikely]]
                continue;
            if (depth > 0 && isJSArray(element)) {
                JSArray* elementArray = jsCast<JSArray*>(element);
                uint64_t newDepth = (depth == std::numeric_limits<uint64_t>::max()) ? depth : depth - 1;
                uint64_t flatLength = calculateFlattenedLength(globalObject, elementArray, elementArray->length(), newDepth);
                RETURN_IF_EXCEPTION(scope, flatLength);
                if (flatLength == std::numeric_limits<uint64_t>::max()) [[unlikely]]
                    return flatLength;
                resultLength += flatLength;
                if (lengthExceeded()) [[unlikely]] {
                    throwTypeError(globalObject, scope, "flatten array exceeds 2**52 - 1");
                    return std::numeric_limits<uint64_t>::max();
                }
            } else {
                if (element.isObject()) {
                    auto elementObject = asObject(element);
                    if (elementObject->isProxy()) [[unlikely]]
                        return std::numeric_limits<uint64_t>::max();
                }
                resultLength++;
                if (lengthExceeded()) [[unlikely]] {
                    throwTypeError(globalObject, scope, "flatten array exceeds 2**52 - 1");
                    return std::numeric_limits<uint64_t>::max();
                }
            }
        }
        break;
    }
    case ArrayWithDouble: {
        auto* sourceBuffer = sourceArray->butterfly()->contiguousDouble().data();
        for (uint64_t i = 0; i < sourceLength; ++i) {
            double value = sourceBuffer[i];
            if (std::isnan(value)) [[unlikely]]
                continue;
            resultLength++;
            if (lengthExceeded()) [[unlikely]] {
                throwTypeError(globalObject, scope, "flatten array exceeds 2**52 - 1");
                return std::numeric_limits<uint64_t>::max();
            }
        }
        break;
    }
    default:
        return std::numeric_limits<uint64_t>::max();
    }

    return resultLength;
}

template<typename T>
static uint64_t fastFlatIntoBuffer(JSGlobalObject* globalObject, T* resultBuffer, uint64_t& resultIndex, JSArray* sourceArray, uint64_t sourceLength, uint64_t depth, uint64_t vectorLength)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!vm.isSafeToRecurseSoft()) [[unlikely]] {
        throwStackOverflowError(globalObject, scope);
        return std::numeric_limits<uint64_t>::max();
    }

    IndexingType sourceType = sourceArray->indexingType();

    switch (sourceType) {
    case ArrayWithInt32: {
        auto* sourceBuffer = sourceArray->butterfly()->contiguous().data();
        for (uint64_t i = 0; i < sourceLength; ++i) {
            if (resultIndex >= vectorLength) [[unlikely]]
                return std::numeric_limits<uint64_t>::max();
            JSValue element = sourceBuffer[i].get();
            if (!element) [[unlikely]]
                continue;
            if constexpr (std::is_same_v<T, double>)
                resultBuffer[resultIndex] = element.asNumber();
            else
                resultBuffer[resultIndex].setWithoutWriteBarrier(element);
            ++resultIndex;
        }
        break;
    }
    case ArrayWithContiguous: {
        auto* sourceBuffer = sourceArray->butterfly()->contiguous().data();
        for (uint64_t i = 0; i < sourceLength; ++i) {
            if (resultIndex >= vectorLength) [[unlikely]]
                return std::numeric_limits<uint64_t>::max();
            JSValue element = sourceBuffer[i].get();
            if (!element) [[unlikely]]
                continue;
            if (depth > 0 && isJSArray(element)) {
                JSArray* elementArray = jsCast<JSArray*>(element);
                uint64_t newDepth = (depth == std::numeric_limits<uint64_t>::max()) ? depth : depth - 1;
                resultIndex = fastFlatIntoBuffer(globalObject, resultBuffer, resultIndex, elementArray, elementArray->length(), newDepth, vectorLength);
                RETURN_IF_EXCEPTION(scope, resultIndex);
                if (resultIndex == std::numeric_limits<uint64_t>::max())
                    return std::numeric_limits<uint64_t>::max();
            } else {
                if constexpr (std::is_same_v<T, double>)
                    resultBuffer[resultIndex] = element.asNumber();
                else
                    resultBuffer[resultIndex].setWithoutWriteBarrier(element);
                ++resultIndex;
            }
        }
        break;
    }
    case ArrayWithDouble: {
        auto* sourceBuffer = sourceArray->butterfly()->contiguousDouble().data();
        for (uint64_t i = 0; i < sourceLength; ++i) {
            if (resultIndex >= vectorLength) [[unlikely]]
                return std::numeric_limits<uint64_t>::max();
            double value = sourceBuffer[i];
            if (std::isnan(value)) [[unlikely]]
                continue;
            if constexpr (std::is_same_v<T, double>)
                resultBuffer[resultIndex] = value;
            else
                resultBuffer[resultIndex].setWithoutWriteBarrier(JSValue(value));
            ++resultIndex;
        }
        break;
    }
    default: {
        RELEASE_ASSERT_NOT_REACHED();
        return std::numeric_limits<uint64_t>::max();
    }
    }
    return resultIndex;
}

JSArray* JSArray::fastFlat(JSGlobalObject* globalObject, uint64_t depth, uint64_t length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    IndexingType sourceType = this->indexingType();

    switch (sourceType) {
    case ArrayWithInt32:
    case ArrayWithDouble:
    case ArrayWithContiguous: {
        if (length > this->butterfly()->vectorLength()) [[unlikely]]
            return nullptr;

        if (holesMustForwardToPrototype()) [[unlikely]]
            return nullptr;

        uint64_t flattenedLength = calculateFlattenedLength(globalObject, this, length, depth);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (flattenedLength == std::numeric_limits<uint64_t>::max()) [[unlikely]]
            return nullptr;

        Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(sourceType);

        IndexingType indexingType = resultStructure->indexingType();
        if (hasAnyArrayStorage(indexingType)) [[unlikely]]
            return nullptr;
        ASSERT(!globalObject->isHavingABadTime());

        auto vectorLength = Butterfly::optimalContiguousVectorLength(resultStructure, flattenedLength);

        void* memory = vm.auxiliarySpace().allocate(
            vm,
            Butterfly::totalSize(0, 0, true, vectorLength * sizeof(EncodedJSValue)),
            nullptr, AllocationFailureMode::ReturnNull);
        if (!memory) [[unlikely]]
            return nullptr;

        auto* butterfly = Butterfly::fromBase(memory, 0, 0);
        butterfly->setVectorLength(vectorLength);
        butterfly->setPublicLength(flattenedLength);

        uint64_t resultIndex = 0;
        if (indexingType == ArrayWithDouble) {
            auto* resultBuffer = butterfly->contiguousDouble().data();
            resultIndex = fastFlatIntoBuffer(globalObject, resultBuffer, resultIndex, this, length, depth, vectorLength);
        } else {
            auto* resultBuffer = butterfly->contiguous().data();
            resultIndex = fastFlatIntoBuffer(globalObject, resultBuffer, resultIndex, this, length, depth, vectorLength);
        }
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (resultIndex == std::numeric_limits<uint64_t>::max())
            return nullptr;

        Butterfly::clearOptimalVectorLengthGap(indexingType, butterfly, vectorLength, resultIndex);
        return createWithButterfly(vm, nullptr, resultStructure, butterfly);
    }
    default: {
        return nullptr;
    }
    }
}

bool JSArray::appendMemcpy(JSGlobalObject* globalObject, VM& vm, unsigned startIndex, IndexingType otherType, std::span<const EncodedJSValue> values)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (isCopyOnWrite(indexingMode()))
        convertFromCopyOnWrite(vm);

    IndexingType type = indexingType();
    bool allowPromotion = false;
    IndexingType copyType = mergeIndexingTypeForCopying(otherType, allowPromotion);
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

    if (values.size() >= MIN_SPARSE_ARRAY_INDEX)
        return false;

    CheckedUint32 checkedNewLength = startIndex;
    checkedNewLength += values.size();

    if (checkedNewLength.hasOverflowed()) {
        throwException(globalObject, scope, createRangeError(globalObject, LengthExceededTheMaximumArrayLengthError));
        return false;
    }
    unsigned newLength = checkedNewLength;

    if (newLength >= MIN_SPARSE_ARRAY_INDEX)
        return false;

    if (!ensureLength(vm, newLength)) {
        throwOutOfMemoryError(globalObject, scope);
        return false;
    }
    ASSERT(copyType == indexingType());

    if (otherType == ArrayWithUndecided) [[unlikely]] {
        auto* butterfly = this->butterfly();
        if (type == ArrayWithDouble) {
            for (unsigned i = startIndex; i < newLength; ++i)
                butterfly->contiguousDouble().at(this, i) = PNaN;
        } else {
            for (unsigned i = startIndex; i < newLength; ++i)
                butterfly->contiguousInt32().at(this, i).setWithoutWriteBarrier(JSValue());
        }
    } else if (type == ArrayWithDouble) {
        auto data = butterfly()->contiguousDouble().data();
        unsigned index = startIndex;
        for (EncodedJSValue encodedDouble : values)
            data[index++] = JSValue::decode(encodedDouble).asNumber();
    } else if (type == ArrayWithInt32)
        memcpy(butterfly()->contiguous().data() + startIndex, std::bit_cast<const WriteBarrier<Unknown>*>(values.data()), sizeof(JSValue) * values.size());
    else {
        gcSafeMemcpy(butterfly()->contiguous().data() + startIndex, std::bit_cast<const WriteBarrier<Unknown>*>(values.data()), sizeof(JSValue) * values.size());
        vm.writeBarrier(this);
    }

    return true;
}

bool JSArray::appendMemcpy(JSGlobalObject* globalObject, VM& vm, unsigned startIndex, JSC::JSArray* otherArray)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!canFastAppend(otherArray))
        return false;

    IndexingType type = indexingType();
    IndexingType otherType = otherArray->indexingType();
    bool allowPromotion = false;
    IndexingType copyType = mergeIndexingTypeForCopying(otherType, allowPromotion);
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
    CheckedUint32 checkedNewLength = startIndex;
    checkedNewLength += otherLength;

    if (checkedNewLength.hasOverflowed()) {
        throwException(globalObject, scope, createRangeError(globalObject, LengthExceededTheMaximumArrayLengthError));
        return false;
    }
    unsigned newLength = checkedNewLength;

    if (newLength >= MIN_SPARSE_ARRAY_INDEX)
        return false;

    if (!ensureLength(vm, newLength)) {
        throwOutOfMemoryError(globalObject, scope);
        return false;
    }
    ASSERT(copyType == indexingType());

    if (otherType == ArrayWithUndecided) [[unlikely]] {
        auto* butterfly = this->butterfly();
        if (type == ArrayWithDouble) {
            for (unsigned i = startIndex; i < newLength; ++i)
                butterfly->contiguousDouble().at(this, i) = PNaN;
        } else {
            for (unsigned i = startIndex; i < newLength; ++i)
                butterfly->contiguousInt32().at(this, i).setWithoutWriteBarrier(JSValue());
        }
    } else if (type == ArrayWithDouble) {
        // Double array storage do not need to be safe against GC since they are not scanned.
        memcpy(butterfly()->contiguousDouble().data() + startIndex, otherArray->butterfly()->contiguousDouble().data(), sizeof(double) * otherLength);
    } else if (type == ArrayWithInt32)
        memcpy(butterfly()->contiguous().data() + startIndex, otherArray->butterfly()->contiguous().data(), sizeof(JSValue) * otherLength);
    else {
        gcSafeMemcpy(butterfly()->contiguous().data() + startIndex, otherArray->butterfly()->contiguous().data(), sizeof(JSValue) * otherLength);
        vm.writeBarrier(this);
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
        [[fallthrough]];

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

JSArray* JSArray::fastSlice(JSGlobalObject* globalObject, JSObject* source, uint64_t startIndex, uint64_t count)
{
    VM& vm = globalObject->vm();

    Structure* sourceStructure = source->structure();
    if (sourceStructure->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero()) {
        // We do not need to have ClonedArgumentsType here since it does not have interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero.
        switch (source->type()) {
        case DirectArgumentsType:
            return DirectArguments::fastSlice(globalObject, jsCast<DirectArguments*>(source), startIndex, count);
        default:
            return nullptr;
        }
        return nullptr;
    }

    auto arrayType = source->indexingType() | IsArray;
    switch (arrayType) {
    case ArrayWithDouble:
    case ArrayWithInt32:
    case ArrayWithContiguous: {
        if (count >= MIN_SPARSE_ARRAY_INDEX || sourceStructure->holesMustForwardToPrototype(source))
            return nullptr;

        if (startIndex + count > source->butterfly()->vectorLength())
            return nullptr;

        Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(arrayType);
        IndexingType indexingType = resultStructure->indexingType();
        if (hasAnyArrayStorage(indexingType)) [[unlikely]]
            return nullptr;

        if (isCopyOnWrite(source->indexingMode())) {
            if (!startIndex && count == source->butterfly()->publicLength())
                return JSArray::createWithButterfly(vm, nullptr, globalObject->originalArrayStructureForIndexingType(source->indexingMode()), source->butterfly());
        }

        ASSERT(!globalObject->isHavingABadTime());
        if (count > MAX_STORAGE_VECTOR_LENGTH) [[unlikely]]
            return nullptr;

        ASSERT(!resultStructure->outOfLineCapacity()); // JSArray's initial Structure should not have any properties.
        uint32_t initialLength = static_cast<uint32_t>(count);
        unsigned vectorLength = Butterfly::optimalContiguousVectorLength(resultStructure, initialLength);
        void* memory = vm.auxiliarySpace().allocate(vm, Butterfly::totalSize(0, 0, true, vectorLength * sizeof(EncodedJSValue)), nullptr, AllocationFailureMode::ReturnNull);
        if (!memory) [[unlikely]]
            return nullptr;

        auto* butterfly = Butterfly::fromBase(memory, 0, 0);
        butterfly->setVectorLength(vectorLength);
        butterfly->setPublicLength(initialLength);
        // We initialize Butterfly first before setting it to JSArray. In that case, butterfly is not scannoed so that we can safely use memcpy here.
        memcpy(butterfly->contiguous().data(), source->butterfly()->contiguous().data() + startIndex, sizeof(JSValue) * initialLength);

        Butterfly::clearOptimalVectorLengthGap(indexingType, butterfly, vectorLength, initialLength);
        return createWithButterfly(vm, nullptr, resultStructure, butterfly);
    }
    case ArrayWithArrayStorage: {
        if (count >= MIN_SPARSE_ARRAY_INDEX || sourceStructure->holesMustForwardToPrototype(source))
            return nullptr;

        if (startIndex + count > source->butterfly()->arrayStorage()->vectorLength())
            return nullptr;

        Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);
        if (hasAnyArrayStorage(resultStructure->indexingType())) [[unlikely]]
            return nullptr;

        ASSERT(!globalObject->isHavingABadTime());
        ObjectInitializationScope scope(vm);
        JSArray* resultArray = JSArray::tryCreateUninitializedRestricted(scope, resultStructure, static_cast<uint32_t>(count));
        if (!resultArray) [[unlikely]]
            return nullptr;
        gcSafeMemcpy(resultArray->butterfly()->contiguous().data(), source->butterfly()->arrayStorage()->m_vector + startIndex, sizeof(JSValue) * static_cast<uint32_t>(count));
        ASSERT(resultArray->butterfly()->publicLength() == count);
        return resultArray;
    }
    case ArrayWithUndecided: {
        if (count)
            return nullptr;
        return constructEmptyArray(globalObject, nullptr);
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
    
    AssertNoGC assertNoGC;
    Locker locker { cellLock() };
    
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
        {
            // When moving Butterfly's head to adjust property-storage, we must take a structure lock.
            // Otherwise, concurrent JIT compiler accesses to a property storage which is half-baked due to move for shift / unshift.
            Structure* structure = this->structure();
            ConcurrentJSLocker structureLock(structure->lock());
            // Adjust the Butterfly and the index bias. We only need to do this here because we're changing
            // the start of the Butterfly, which needs to point at the first indexed property in the used
            // portion of the vector.
            Butterfly* butterfly = this->butterfly()->shift(structure, count);
            storage = butterfly->arrayStorage();
            storage->m_indexBias += count;

            // Since we're consuming part of the vector by moving its beginning to the left,
            // we need to modify the vector length appropriately.
            storage->setVectorLength(vectorLength - count);
            setButterfly(vm, butterfly);
        }
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

bool JSArray::shiftCountWithAnyIndexingType(JSGlobalObject* globalObject, unsigned& startIndex, unsigned count, unsigned shiftThreshold)
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
        if (oldLength - (startIndex + count) >= MIN_SPARSE_ARRAY_INDEX || oldLength > shiftThreshold)
            return shiftCountWithArrayStorage(vm, startIndex, count, ensureArrayStorage(vm));

        // Storing to a hole is fine since we're still having a good time. But reading from a hole
        // is totally not fine, since we might have to read from the proto chain.
        // We have to check for holes before we start moving things around so that we don't get halfway
        // through shifting and then realize we should have been in ArrayStorage mode.
        unsigned end = oldLength - count;
        unsigned moveCount = end - startIndex;
        if (moveCount) {
            if (holesMustForwardToPrototype()) [[unlikely]] {
                for (unsigned i = startIndex; i < end; ++i) {
                    JSValue v = butterfly->contiguous().at(this, i + count).get();
                    if (!v) [[unlikely]] {
                        startIndex = i;
                        return shiftCountWithArrayStorage(vm, startIndex, count, ensureArrayStorage(vm));
                    }
                    butterfly->contiguous().at(this, i).setWithoutWriteBarrier(v);
                }
            } else {
                gcSafeMemmove(butterfly->contiguous().data() + startIndex,
                    butterfly->contiguous().data() + startIndex + count,
                    sizeof(JSValue) * moveCount);
            }
        }

        for (unsigned i = end; i < oldLength; ++i)
            butterfly->contiguous().at(this, i).clear();

        butterfly->setPublicLength(oldLength - count);

        // Our memmoving of values around in the array could have concealed some of them from
        // the collector. Let's make sure that the collector scans this object again.
        if (indexingType == ArrayWithContiguous)
            vm.writeBarrier(this);

        return true;
    }
        
    case ArrayWithDouble: {
        unsigned oldLength = butterfly->publicLength();
        RELEASE_ASSERT(count <= oldLength);
        
        // We may have to walk the entire array to do the shift. We're willing to do
        // so only if it's not horribly slow.
        if (oldLength - (startIndex + count) >= MIN_SPARSE_ARRAY_INDEX || oldLength > shiftThreshold)
            return shiftCountWithArrayStorage(vm, startIndex, count, ensureArrayStorage(vm));

        // Storing to a hole is fine since we're still having a good time. But reading from a hole 
        // is totally not fine, since we might have to read from the proto chain.
        // We have to check for holes before we start moving things around so that we don't get halfway 
        // through shifting and then realize we should have been in ArrayStorage mode.
        unsigned end = oldLength - count;
        unsigned moveCount = end - startIndex;
        if (moveCount) {
            if (holesMustForwardToPrototype()) [[unlikely]] {
                for (unsigned i = startIndex; i < end; ++i) {
                    double v = butterfly->contiguousDouble().at(this, i + count);
                    if (v != v) [[unlikely]] {
                        startIndex = i;
                        return shiftCountWithArrayStorage(vm, startIndex, count, ensureArrayStorage(vm));
                    }
                    butterfly->contiguousDouble().at(this, i) = v;
                }
            } else {
                gcSafeMemmove(butterfly->contiguousDouble().data() + startIndex,
                    butterfly->contiguousDouble().data() + startIndex + count,
                    sizeof(double) * moveCount);
            }
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
    DeferGC deferGC(vm);
    Locker locker { cellLock() };
    
    if (moveFront && storage->m_indexBias >= count) {
        // When moving Butterfly's head to adjust property-storage, we must take a structure lock.
        // Otherwise, concurrent JIT compiler accesses to a property storage which is half-baked due to move for shift / unshift.
        Structure* structure = this->structure();
        ConcurrentJSLocker structureLock(structure->lock());
        Butterfly* newButterfly = storage->butterfly()->unshift(structure, count);

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
        unsigned moveCount = oldLength - startIndex;
        if (moveCount >= MIN_SPARSE_ARRAY_INDEX)
            RELEASE_AND_RETURN(scope, unshiftCountWithArrayStorage(globalObject, startIndex, count, ensureArrayStorage(vm)));

        CheckedUint32 checkedLength(oldLength);
        checkedLength += count;
        if (checkedLength.hasOverflowed()) {
            throwOutOfMemoryError(globalObject, scope);
            return true;
        }
        unsigned newLength = checkedLength;
        if (newLength > MAX_STORAGE_VECTOR_LENGTH)
            return false;

        // FIXME: If we create a new butterfly, we should move elements at the same time.
        if (!ensureLength(vm, newLength)) {
            throwOutOfMemoryError(globalObject, scope);
            return true;
        }
        butterfly = this->butterfly();

        // We have to check for holes before we start moving things around so that we don't get halfway 
        // through shifting and then realize we should have been in ArrayStorage mode.
        if (moveCount) {
            if (holesMustForwardToPrototype()) [[unlikely]] {
                auto* buffer = butterfly->contiguous().data() + startIndex;
                if (containsHole(buffer, moveCount)) [[unlikely]]
                    RELEASE_AND_RETURN(scope, unshiftCountWithArrayStorage(globalObject, startIndex, count, ensureArrayStorage(vm)));
            }

            gcSafeMemmove(butterfly->contiguous().data() + startIndex + count, butterfly->contiguous().data() + startIndex, moveCount * sizeof(EncodedJSValue));
        }

        // Our memmoving of values around in the array could have concealed some of them from
        // the collector. Let's make sure that the collector scans this object again.
        vm.writeBarrier(this);
        
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
        unsigned moveCount = oldLength - startIndex;
        if (moveCount >= MIN_SPARSE_ARRAY_INDEX)
            RELEASE_AND_RETURN(scope, unshiftCountWithArrayStorage(globalObject, startIndex, count, ensureArrayStorage(vm)));

        CheckedUint32 checkedLength(oldLength);
        checkedLength += count;
        if (checkedLength.hasOverflowed()) {
            throwOutOfMemoryError(globalObject, scope);
            return true;
        }
        unsigned newLength = checkedLength;
        if (newLength > MAX_STORAGE_VECTOR_LENGTH)
            return false;

        // FIXME: If we create a new butterfly, we should move elements at the same time.
        if (!ensureLength(vm, newLength)) {
            throwOutOfMemoryError(globalObject, scope);
            return true;
        }
        butterfly = this->butterfly();
        
        // We have to check for holes before we start moving things around so that we don't get halfway 
        // through shifting and then realize we should have been in ArrayStorage mode.
        if (moveCount) {
            if (holesMustForwardToPrototype()) [[unlikely]] {
                for (unsigned i = oldLength; i-- > startIndex;) {
                    double v = butterfly->contiguousDouble().at(this, i);
                    if (v != v) [[unlikely]]
                        RELEASE_AND_RETURN(scope, unshiftCountWithArrayStorage(globalObject, startIndex, count, ensureArrayStorage(vm)));
                }
            }

            gcSafeMemmove(butterfly->contiguousDouble().data() + startIndex + count, butterfly->contiguousDouble().data() + startIndex, moveCount * sizeof(double));
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
        vector = nullptr;
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
        vector = nullptr;
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
        vector = nullptr;
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
        vector = nullptr;
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
    Structure* structure = this->structure();
    // This is the fast case. Many arrays will be an original array.
    if (globalObject->isOriginalArrayStructure(structure))
        return true;

    if (structure->mayInterceptIndexedAccesses())
        return false;

    if (getPrototypeDirect() != globalObject->arrayPrototype())
        return false;

    if (getDirectOffset(vm, vm.propertyNames->iteratorSymbol) != invalidOffset)
        return false;

    return true;
}

bool JSArray::isToPrimitiveFastAndNonObservable()
{
    JSGlobalObject* globalObject = this->globalObject();
    if (!globalObject->arrayPrototypeChainIsSane()) [[unlikely]]
        return false;
    if (!globalObject->arrayToStringWatchpointSet().isStillValid()) [[unlikely]]
        return false;
    if (!globalObject->arraySymbolToPrimitiveWatchpointSet().isStillValid()) [[unlikely]]
        return false;
    if (!globalObject->arrayJoinWatchpointSet().isStillValid()) [[unlikely]]
        return false;

    Structure* structure = this->structure();
    return globalObject->isOriginalArrayStructure(structure);
}

template<AllocationFailureMode failureMode>
inline JSArray* constructArray(ObjectInitializationScope& scope, Structure* arrayStructure, unsigned length)
{
    JSArray* array = JSArray::tryCreateUninitializedRestricted(scope, arrayStructure, length);

    // FIXME: we should probably throw an out of memory error here, but
    // when making this change we should check that all clients of this
    // function will correctly handle an exception being thrown from here.
    // https://bugs.webkit.org/show_bug.cgi?id=169786
    if constexpr (failureMode == AllocationFailureMode::Assert)
        RELEASE_ASSERT(array);
    else if (!array)
        return nullptr;

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

    JSArray* array = constructArray<AllocationFailureMode::Assert>(scope, arrayStructure, length);
    for (unsigned i = 0; i < length; ++i)
        array->initializeIndex(scope, i, values.at(i));
    return array;
}

JSArray* constructArray(JSGlobalObject* globalObject, Structure* arrayStructure, const JSValue* values, unsigned length)
{
    VM& vm = globalObject->vm();
    ObjectInitializationScope scope(vm);

    JSArray* array = constructArray<AllocationFailureMode::Assert>(scope, arrayStructure, length);
    for (unsigned i = 0; i < length; ++i)
        array->initializeIndex(scope, i, values[i]);
    return array;
}

JSArray* constructArrayNegativeIndexed(JSGlobalObject* globalObject, Structure* arrayStructure, const JSValue* values, unsigned length)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    ObjectInitializationScope scope(vm);

    JSArray* array = constructArray<AllocationFailureMode::ReturnNull>(scope, arrayStructure, length);
    if (!array) [[unlikely]] {
        throwOutOfMemoryError(globalObject, throwScope);
        return nullptr;
    }

    for (int i = 0; i < static_cast<int>(length); ++i)
        array->initializeIndex(scope, i, values[-i]);
    return array;
}

template<>
void clearElement(double& element)
{
    element = PNaN;
}

template<ArrayFillMode fillMode>
JSArray* tryCloneArrayFromFast(JSGlobalObject* globalObject, JSValue arrayValue)
{
    ASSERT(isJSArray(arrayValue));

    VM& vm = globalObject->vm();

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* array = jsCast<JSArray*>(arrayValue);
    if (!array->isIteratorProtocolFastAndNonObservable()) [[unlikely]]
        return nullptr;

    IndexingType sourceType = array->indexingType();
    if (shouldUseSlowPut(sourceType) || sourceType == ArrayClass) [[unlikely]]
        return nullptr;

    Butterfly* butterfly= array->butterfly();
    unsigned resultSize = butterfly->publicLength();
    if (hasAnyArrayStorage(sourceType) || resultSize >= MIN_SPARSE_ARRAY_INDEX) [[unlikely]] {
        JSArray* result = constructEmptyArray(globalObject, nullptr, resultSize);
        RETURN_IF_EXCEPTION(scope, { });

        scope.release();
        moveArrayElements<fillMode>(globalObject, vm, result, 0, array, resultSize);
        return result;
    }

    ASSERT(sourceType == ArrayWithDouble || sourceType == ArrayWithInt32 || sourceType == ArrayWithContiguous || sourceType == ArrayWithUndecided);

    if (!globalObject->isHavingABadTime()) [[likely]] {
        if constexpr (fillMode == ArrayFillMode::Empty) {
            if (isCopyOnWrite(array->indexingMode()))
                return JSArray::createWithButterfly(vm, nullptr, globalObject->originalArrayStructureForIndexingType(array->indexingMode()), array->butterfly());
        }

        if (!resultSize)
            RELEASE_AND_RETURN(scope, constructEmptyArray(globalObject, nullptr));
    }

    IndexingType resultType = sourceType;
    if constexpr (fillMode == ArrayFillMode::Undefined)  {
        if (sourceType == ArrayWithDouble) {
            double* buffer = butterfly->contiguousDouble().data();
            if (containsHole(buffer, resultSize)) [[unlikely]]
                resultType = ArrayWithContiguous;
        } else if (sourceType == ArrayWithInt32) {
            auto* buffer = butterfly->contiguous().data();
            if (containsHole(buffer, resultSize)) [[unlikely]]
                resultType = ArrayWithContiguous;
        } else if (sourceType == ArrayWithUndecided && resultSize)
            resultType = ArrayWithContiguous;
    }

    Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(resultType);
    if (hasAnyArrayStorage(resultStructure->indexingType())) [[unlikely]]
        return nullptr;

    ASSERT(!globalObject->isHavingABadTime());
    auto vectorLength = Butterfly::optimalContiguousVectorLength(resultStructure, resultSize);
    if (vectorLength > MAX_STORAGE_VECTOR_LENGTH) [[unlikely]]
        return { };

    ASSERT(!resultStructure->outOfLineCapacity());
    void* memory = vm.auxiliarySpace().allocate(vm, Butterfly::totalSize(0, 0, true, vectorLength * sizeof(EncodedJSValue)), nullptr, AllocationFailureMode::ReturnNull);
    if (!memory) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }
    auto* resultButterfly = Butterfly::fromBase(memory, 0, 0);
    resultButterfly->setVectorLength(vectorLength);
    resultButterfly->setPublicLength(resultSize);

    switch (resultType) {
    case ArrayWithUndecided:
        if constexpr (fillMode == ArrayFillMode::Empty) {
            auto* buffer = resultButterfly->contiguous().data();
            copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(buffer, 0, butterfly->contiguous().data(), 0, resultSize, ArrayWithUndecided);
            break;
        }
        ASSERT(!resultSize);
        break;
    case ArrayWithDouble: {
        ASSERT(sourceType == ArrayWithDouble);
        double* buffer = resultButterfly->contiguousDouble().data();
        copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(buffer, 0, butterfly->contiguousDouble().data(), 0, resultSize, ArrayWithDouble);
        break;
    }
    case ArrayWithInt32: {
        ASSERT(sourceType == ArrayWithInt32);
        auto* buffer = resultButterfly->contiguous().data();
        copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(buffer, 0, butterfly->contiguous().data(), 0, resultSize, ArrayWithInt32);
        break;
    }
    case ArrayWithContiguous: {
        auto* buffer = resultButterfly->contiguous().data();
        if (sourceType == ArrayWithDouble)
            copyArrayElements<fillMode, NeedsGCSafeOps::No>(buffer, 0, butterfly->contiguousDouble().data(), 0, resultSize, ArrayWithDouble);
        else
            copyArrayElements<fillMode, NeedsGCSafeOps::No>(buffer, 0, butterfly->contiguous().data(), 0, resultSize, sourceType);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    Butterfly::clearOptimalVectorLengthGap(resultType, resultButterfly, vectorLength, resultSize);
    return JSArray::createWithButterfly(vm, nullptr, resultStructure, resultButterfly);
}

template JSArray* tryCloneArrayFromFast<ArrayFillMode::Undefined>(JSGlobalObject*, JSValue);
template JSArray* tryCloneArrayFromFast<ArrayFillMode::Empty>(JSGlobalObject*, JSValue);

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
