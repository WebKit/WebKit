/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008, 2009, 2012 Apple Inc. All rights reserved.
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
#include "ButterflyInlineMethods.h"
#include "CopiedSpace.h"
#include "CopiedSpaceInlineMethods.h"
#include "CachedCall.h"
#include "Error.h"
#include "Executable.h"
#include "GetterSetter.h"
#include "IndexingHeaderInlineMethods.h"
#include "PropertyNameArray.h"
#include "Reject.h"
#include "SparseArrayValueMapInlineMethods.h"
#include <wtf/AVLTree.h>
#include <wtf/Assertions.h>
#include <wtf/OwnPtr.h>
#include <Operations.h>

using namespace std;
using namespace WTF;

namespace JSC {


ASSERT_CLASS_FITS_IN_CELL(JSArray);
ASSERT_HAS_TRIVIAL_DESTRUCTOR(JSArray);

const ClassInfo JSArray::s_info = {"Array", &JSNonFinalObject::s_info, 0, 0, CREATE_METHOD_TABLE(JSArray)};

Butterfly* createArrayButterflyInDictionaryIndexingMode(JSGlobalData& globalData, unsigned initialLength)
{
    Butterfly* butterfly = Butterfly::create(
        globalData, 0, 0, true, IndexingHeader(), ArrayStorage::sizeFor(0));
    ArrayStorage* storage = butterfly->arrayStorage();
    storage->setLength(initialLength);
    storage->setVectorLength(0);
    storage->m_indexBias = 0;
    storage->m_sparseMap.clear();
    storage->m_numValuesInVector = 0;
#if CHECK_ARRAY_CONSISTENCY
    storage->m_initializationIndex = 0;
    storage->m_inCompactInitialization = 0;
#endif
    return butterfly;
}

void JSArray::setLengthWritable(ExecState* exec, bool writable)
{
    ASSERT(isLengthWritable() || !writable);
    if (!isLengthWritable() || writable)
        return;

    enterDictionaryIndexingMode(exec->globalData());

    SparseArrayValueMap* map = arrayStorage()->m_sparseMap.get();
    ASSERT(map);
    map->setLengthIsReadOnly();
}

// Defined in ES5.1 15.4.5.1
bool JSArray::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, PropertyDescriptor& descriptor, bool throwException)
{
    JSArray* array = jsCast<JSArray*>(object);

    // 3. If P is "length", then
    if (propertyName == exec->propertyNames().length) {
        // All paths through length definition call the default [[DefineOwnProperty]], hence:
        // from ES5.1 8.12.9 7.a.
        if (descriptor.configurablePresent() && descriptor.configurable())
            return reject(exec, throwException, "Attempting to change configurable attribute of unconfigurable property.");
        // from ES5.1 8.12.9 7.b.
        if (descriptor.enumerablePresent() && descriptor.enumerable())
            return reject(exec, throwException, "Attempting to change enumerable attribute of unconfigurable property.");

        // a. If the [[Value]] field of Desc is absent, then
        // a.i. Return the result of calling the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length", Desc, and Throw as arguments.
        if (descriptor.isAccessorDescriptor())
            return reject(exec, throwException, "Attempting to change access mechanism for an unconfigurable property.");
        // from ES5.1 8.12.9 10.a.
        if (!array->isLengthWritable() && descriptor.writablePresent() && descriptor.writable())
            return reject(exec, throwException, "Attempting to change writable attribute of unconfigurable property.");
        // This descriptor is either just making length read-only, or changing nothing!
        if (!descriptor.value()) {
            if (descriptor.writablePresent())
                array->setLengthWritable(exec, descriptor.writable());
            return true;
        }
        
        // b. Let newLenDesc be a copy of Desc.
        // c. Let newLen be ToUint32(Desc.[[Value]]).
        unsigned newLen = descriptor.value().toUInt32(exec);
        // d. If newLen is not equal to ToNumber( Desc.[[Value]]), throw a RangeError exception.
        if (newLen != descriptor.value().toNumber(exec)) {
            throwError(exec, createRangeError(exec, "Invalid array length"));
            return false;
        }

        // Based on SameValue check in 8.12.9, this is always okay.
        if (newLen == array->length()) {
            if (descriptor.writablePresent())
                array->setLengthWritable(exec, descriptor.writable());
            return true;
        }

        // e. Set newLenDesc.[[Value] to newLen.
        // f. If newLen >= oldLen, then
        // f.i. Return the result of calling the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length", newLenDesc, and Throw as arguments.
        // g. Reject if oldLenDesc.[[Writable]] is false.
        if (!array->isLengthWritable())
            return reject(exec, throwException, "Attempting to change value of a readonly property.");
        
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
        if (!array->setLength(exec, newLen, throwException)) {
            // 1. Set newLenDesc.[[Value] to oldLen+1.
            // 2. If newWritable is false, set newLenDesc.[[Writable] to false.
            // 3. Call the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length", newLenDesc, and false as arguments.
            // 4. Reject.
            if (descriptor.writablePresent())
                array->setLengthWritable(exec, descriptor.writable());
            return false;
        }

        // m. If newWritable is false, then
        // i. Call the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length",
        //    Property Descriptor{[[Writable]]: false}, and false as arguments. This call will always
        //    return true.
        if (descriptor.writablePresent())
            array->setLengthWritable(exec, descriptor.writable());
        // n. Return true.
        return true;
    }

    // 4. Else if P is an array index (15.4), then
    // a. Let index be ToUint32(P).
    unsigned index = propertyName.asIndex();
    if (index != PropertyName::NotAnIndex) {
        // b. Reject if index >= oldLen and oldLenDesc.[[Writable]] is false.
        if (index >= array->length() && !array->isLengthWritable())
            return reject(exec, throwException, "Attempting to define numeric property on array with non-writable length property.");
        // c. Let succeeded be the result of calling the default [[DefineOwnProperty]] internal method (8.12.9) on A passing P, Desc, and false as arguments.
        // d. Reject if succeeded is false.
        // e. If index >= oldLen
        // e.i. Set oldLenDesc.[[Value]] to index + 1.
        // e.ii. Call the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length", oldLenDesc, and false as arguments. This call will always return true.
        // f. Return true.
        return array->defineOwnIndexedProperty(exec, index, descriptor, throwException);
    }

    return array->JSObject::defineOwnNonIndexProperty(exec, propertyName, descriptor, throwException);
}

bool JSArray::getOwnPropertySlot(JSCell* cell, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSArray* thisObject = jsCast<JSArray*>(cell);
    if (propertyName == exec->propertyNames().length) {
        slot.setValue(jsNumber(thisObject->length()));
        return true;
    }

    return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

bool JSArray::getOwnPropertyDescriptor(JSObject* object, ExecState* exec, PropertyName propertyName, PropertyDescriptor& descriptor)
{
    JSArray* thisObject = jsCast<JSArray*>(object);
    if (propertyName == exec->propertyNames().length) {
        descriptor.setDescriptor(jsNumber(thisObject->length()), thisObject->isLengthWritable() ? DontDelete | DontEnum : DontDelete | DontEnum | ReadOnly);
        return true;
    }

    return JSObject::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor);
}

// ECMA 15.4.5.1
void JSArray::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    JSArray* thisObject = jsCast<JSArray*>(cell);

    if (propertyName == exec->propertyNames().length) {
        unsigned newLength = value.toUInt32(exec);
        if (value.toNumber(exec) != static_cast<double>(newLength)) {
            throwError(exec, createRangeError(exec, ASCIILiteral("Invalid array length")));
            return;
        }
        thisObject->setLength(exec, newLength, slot.isStrictMode());
        return;
    }

    JSObject::put(thisObject, exec, propertyName, value, slot);
}

bool JSArray::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    JSArray* thisObject = jsCast<JSArray*>(cell);

    if (propertyName == exec->propertyNames().length)
        return false;

    return JSObject::deleteProperty(thisObject, exec, propertyName);
}

static int compareKeysForQSort(const void* a, const void* b)
{
    unsigned da = *static_cast<const unsigned*>(a);
    unsigned db = *static_cast<const unsigned*>(b);
    return (da > db) - (da < db);
}

void JSArray::getOwnNonIndexPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSArray* thisObject = jsCast<JSArray*>(object);

    if (mode == IncludeDontEnumProperties)
        propertyNames.add(exec->propertyNames().length);

    JSObject::getOwnNonIndexPropertyNames(thisObject, exec, propertyNames, mode);
}

// This method makes room in the vector, but leaves the new space uncleared.
bool JSArray::unshiftCountSlowCase(JSGlobalData& globalData, unsigned count)
{
    ArrayStorage* storage = ensureArrayStorage(globalData);
    Butterfly* butterfly = storage->butterfly();
    unsigned propertyCapacity = structure()->outOfLineCapacity();
    unsigned propertySize = structure()->outOfLineSize();

    // If not, we should have handled this on the fast path.
    ASSERT(count > storage->m_indexBias);

    // Step 1:
    // Gather 4 key metrics:
    //  * usedVectorLength - how many entries are currently in the vector (conservative estimate - fewer may be in use in sparse vectors).
    //  * requiredVectorLength - how many entries are will there be in the vector, after allocating space for 'count' more.
    //  * currentCapacity - what is the current size of the vector, including any pre-capacity.
    //  * desiredCapacity - how large should we like to grow the vector to - based on 2x requiredVectorLength.

    unsigned length = storage->length();
    unsigned usedVectorLength = min(storage->vectorLength(), length);
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
    unsigned desiredCapacity = min(MAX_STORAGE_VECTOR_LENGTH, max(BASE_VECTOR_LEN, requiredVectorLength) << 1);

    // Step 2:
    // We're either going to choose to allocate a new ArrayStorage, or we're going to reuse the existing on.

    void* newAllocBase = 0;
    unsigned newStorageCapacity;
    // If the current storage array is sufficiently large (but not too large!) then just keep using it.
    if (currentCapacity > desiredCapacity && isDenseEnoughForVector(currentCapacity, requiredVectorLength)) {
        newAllocBase = butterfly->base(structure());
        newStorageCapacity = currentCapacity;
    } else {
        size_t newSize = Butterfly::totalSize(0, propertyCapacity, true, ArrayStorage::sizeFor(desiredCapacity));
        if (!globalData.heap.tryAllocateStorage(newSize, &newAllocBase))
            return false;
        newStorageCapacity = desiredCapacity;
    }

    // Step 3:
    // Work out where we're going to move things to.

    // Determine how much of the vector to use as pre-capacity, and how much as post-capacity.
    // If the vector had no free post-capacity (length >= m_vectorLength), don't give it any.
    // If it did, we calculate the amount that will remain based on an atomic decay - leave the
    // vector with half the post-capacity it had previously.
    unsigned postCapacity = 0;
    if (length < storage->vectorLength()) {
        // Atomic decay, + the post-capacity cannot be greater than what is available.
        postCapacity = min((storage->vectorLength() - length) >> 1, newStorageCapacity - requiredVectorLength);
        // If we're moving contents within the same allocation, the post-capacity is being reduced.
        ASSERT(newAllocBase != butterfly->base(structure()) || postCapacity < storage->vectorLength() - length);
    }
    
    unsigned newVectorLength = requiredVectorLength + postCapacity;
    unsigned newIndexBias = newStorageCapacity - newVectorLength;

    Butterfly* newButterfly = Butterfly::fromBase(newAllocBase, newIndexBias, propertyCapacity);
    
    memmove(newButterfly->arrayStorage()->m_vector + count, storage->m_vector, sizeof(JSValue) * usedVectorLength);
    memmove(newButterfly->propertyStorage() - propertySize, butterfly->propertyStorage() - propertySize, sizeof(JSValue) * propertySize + sizeof(IndexingHeader) + ArrayStorage::sizeFor(0));
    
    newButterfly->arrayStorage()->setVectorLength(newVectorLength);
    newButterfly->arrayStorage()->m_indexBias = newIndexBias;
    
    m_butterfly = newButterfly;

    return true;
}

bool JSArray::setLength(ExecState* exec, unsigned newLength, bool throwException)
{
    checkIndexingConsistency();

    ArrayStorage* storage = ensureArrayStorage(exec->globalData());
    unsigned length = storage->length();

    // If the length is read only then we enter sparse mode, so should enter the following 'if'.
    ASSERT(isLengthWritable() || storage->m_sparseMap);

    if (SparseArrayValueMap* map = storage->m_sparseMap.get()) {
        // Fail if the length is not writable.
        if (map->lengthIsReadOnly())
            return reject(exec, throwException, StrictModeReadonlyPropertyWriteError);

        if (newLength < length) {
            // Copy any keys we might be interested in into a vector.
            Vector<unsigned> keys;
            keys.reserveCapacity(min(map->size(), static_cast<size_t>(length - newLength)));
            SparseArrayValueMap::const_iterator end = map->end();
            for (SparseArrayValueMap::const_iterator it = map->begin(); it != end; ++it) {
                unsigned index = static_cast<unsigned>(it->first);
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
                    if (it->second.attributes & DontDelete) {
                        storage->setLength(index + 1);
                        return reject(exec, throwException, "Unable to delete property.");
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
        unsigned usedVectorLength = min(length, storage->vectorLength());
        for (unsigned i = newLength; i < usedVectorLength; ++i) {
            WriteBarrier<Unknown>& valueSlot = storage->m_vector[i];
            bool hadValue = valueSlot;
            valueSlot.clear();
            storage->m_numValuesInVector -= hadValue;
        }
    }

    storage->setLength(newLength);

    checkIndexingConsistency();
    return true;
}

JSValue JSArray::pop(ExecState* exec)
{
    checkIndexingConsistency();
    
    switch (structure()->indexingType()) {
    case ArrayClass:
        return jsUndefined();
        
    case ArrayWithArrayStorage: {
        ArrayStorage* storage = m_butterfly->arrayStorage();
    
        unsigned length = storage->length();
        if (!length) {
            if (!isLengthWritable())
                throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
            return jsUndefined();
        }

        unsigned index = length - 1;
        if (index < storage->vectorLength()) {
            WriteBarrier<Unknown>& valueSlot = storage->m_vector[index];
            if (valueSlot) {
                --storage->m_numValuesInVector;
                JSValue element = valueSlot.get();
                valueSlot.clear();
            
                ASSERT(isLengthWritable());
                storage->setLength(index);
                checkIndexingConsistency();
                return element;
            }
        }

        // Let element be the result of calling the [[Get]] internal method of O with argument indx.
        JSValue element = get(exec, index);
        if (exec->hadException())
            return jsUndefined();
        // Call the [[Delete]] internal method of O with arguments indx and true.
        if (!deletePropertyByIndex(this, exec, index)) {
            throwTypeError(exec, "Unable to delete property.");
            return jsUndefined();
        }
        // Call the [[Put]] internal method of O with arguments "length", indx, and true.
        setLength(exec, index, true);
        // Return element.
        checkIndexingConsistency();
        return element;
    }
        
    default:
        ASSERT_NOT_REACHED();
        return JSValue();
    }
}

// Push & putIndex are almost identical, with two small differences.
//  - we always are writing beyond the current array bounds, so it is always necessary to update m_length & m_numValuesInVector.
//  - pushing to an array of length 2^32-1 stores the property, but throws a range error.
void JSArray::push(ExecState* exec, JSValue value)
{
    checkIndexingConsistency();
    
    switch (structure()->indexingType()) {
    case ArrayClass: {
        putByIndexBeyondVectorLengthWithArrayStorage(exec, 0, value, true, createInitialArrayStorage(exec->globalData()));
        break;
    }
        
    case ArrayWithArrayStorage: {
        ArrayStorage* storage = m_butterfly->arrayStorage();

        // Fast case - push within vector, always update m_length & m_numValuesInVector.
        unsigned length = storage->length();
        if (length < storage->vectorLength()) {
            storage->m_vector[length].set(exec->globalData(), this, value);
            storage->setLength(length + 1);
            ++storage->m_numValuesInVector;
            checkIndexingConsistency();
            return;
        }

        // Pushing to an array of length 2^32-1 stores the property, but throws a range error.
        if (UNLIKELY(storage->length() == 0xFFFFFFFFu)) {
            methodTable()->putByIndex(this, exec, storage->length(), value, true);
            // Per ES5.1 15.4.4.7 step 6 & 15.4.5.1 step 3.d.
            if (!exec->hadException())
                throwError(exec, createRangeError(exec, "Invalid array length"));
            return;
        }

        // Handled the same as putIndex.
        putByIndexBeyondVectorLengthWithArrayStorage(exec, storage->length(), value, true, storage);
        checkIndexingConsistency();
        break;
    }
        
    default:
        ASSERT_NOT_REACHED();
    }
}

bool JSArray::shiftCount(ExecState* exec, unsigned count)
{
    ASSERT(count > 0);
    
    ArrayStorage* storage = ensureArrayStorage(exec->globalData());
    
    unsigned oldLength = storage->length();
    
    // If the array contains holes or is otherwise in an abnormal state,
    // use the generic algorithm in ArrayPrototype.
    if (oldLength != storage->m_numValuesInVector || inSparseIndexingMode())
        return false;

    if (!oldLength)
        return true;
    
    storage->m_numValuesInVector -= count;
    storage->setLength(oldLength - count);
    
    unsigned vectorLength = storage->vectorLength();
    if (vectorLength) {
        count = min(vectorLength, (unsigned)count);
        
        vectorLength -= count;
        storage->setVectorLength(vectorLength);
        
        if (vectorLength) {
            m_butterfly = m_butterfly->shift(structure(), count);
            storage = m_butterfly->arrayStorage();
            storage->m_indexBias += count;
        }
    }
    return true;
}

// Returns true if the unshift can be handled, false to fallback.    
bool JSArray::unshiftCount(ExecState* exec, unsigned count)
{
    ArrayStorage* storage = ensureArrayStorage(exec->globalData());
    unsigned length = storage->length();

    // If the array contains holes or is otherwise in an abnormal state,
    // use the generic algorithm in ArrayPrototype.
    if (length != storage->m_numValuesInVector || storage->inSparseMode())
        return false;

    if (storage->m_indexBias >= count) {
        m_butterfly = storage->butterfly()->unshift(structure(), count);
        storage = m_butterfly->arrayStorage();
        storage->m_indexBias -= count;
        storage->setVectorLength(storage->vectorLength() + count);
    } else if (!unshiftCountSlowCase(exec->globalData(), count)) {
        throwOutOfMemoryError(exec);
        return true;
    }

    WriteBarrier<Unknown>* vector = storage->m_vector;
    for (unsigned i = 0; i < count; i++)
        vector[i].clear();
    return true;
}

static int compareNumbersForQSort(const void* a, const void* b)
{
    double da = static_cast<const JSValue*>(a)->asNumber();
    double db = static_cast<const JSValue*>(b)->asNumber();
    return (da > db) - (da < db);
}

static int compareByStringPairForQSort(const void* a, const void* b)
{
    const ValueStringPair* va = static_cast<const ValueStringPair*>(a);
    const ValueStringPair* vb = static_cast<const ValueStringPair*>(b);
    return codePointCompare(va->second, vb->second);
}

void JSArray::sortNumeric(ExecState* exec, JSValue compareFunction, CallType callType, const CallData& callData)
{
    ASSERT(!inSparseIndexingMode());

    switch (structure()->indexingType()) {
    case ArrayClass:
        return;
        
    case ArrayWithArrayStorage: {
        unsigned lengthNotIncludingUndefined = compactForSorting(exec->globalData());
        ArrayStorage* storage = m_butterfly->arrayStorage();
        
        if (storage->m_sparseMap.get()) {
            throwOutOfMemoryError(exec);
            return;
        }
        
        if (!lengthNotIncludingUndefined)
            return;
        
        bool allValuesAreNumbers = true;
        size_t size = storage->m_numValuesInVector;
        for (size_t i = 0; i < size; ++i) {
            if (!storage->m_vector[i].isNumber()) {
                allValuesAreNumbers = false;
                break;
            }
        }
        
        if (!allValuesAreNumbers)
            return sort(exec, compareFunction, callType, callData);
        
        // For numeric comparison, which is fast, qsort is faster than mergesort. We
        // also don't require mergesort's stability, since there's no user visible
        // side-effect from swapping the order of equal primitive values.
        qsort(storage->m_vector, size, sizeof(WriteBarrier<Unknown>), compareNumbersForQSort);
        
        checkIndexingConsistency(SortConsistencyCheck);
        return;
    }
        
    default:
        ASSERT_NOT_REACHED();
    }
}

void JSArray::sort(ExecState* exec)
{
    ASSERT(!inSparseIndexingMode());
    
    switch (structure()->indexingType()) {
    case ArrayClass:
        return;
        
    case ArrayWithArrayStorage: {
        unsigned lengthNotIncludingUndefined = compactForSorting(exec->globalData());
        ArrayStorage* storage = m_butterfly->arrayStorage();
        if (storage->m_sparseMap.get()) {
            throwOutOfMemoryError(exec);
            return;
        }
        
        if (!lengthNotIncludingUndefined)
            return;
        
        // Converting JavaScript values to strings can be expensive, so we do it once up front and sort based on that.
        // This is a considerable improvement over doing it twice per comparison, though it requires a large temporary
        // buffer. Besides, this protects us from crashing if some objects have custom toString methods that return
        // random or otherwise changing results, effectively making compare function inconsistent.
        
        Vector<ValueStringPair> values(lengthNotIncludingUndefined);
        if (!values.begin()) {
            throwOutOfMemoryError(exec);
            return;
        }
        
        Heap::heap(this)->pushTempSortVector(&values);
        
        bool isSortingPrimitiveValues = true;
        for (size_t i = 0; i < lengthNotIncludingUndefined; i++) {
            JSValue value = storage->m_vector[i].get();
            ASSERT(!value.isUndefined());
            values[i].first = value;
            isSortingPrimitiveValues = isSortingPrimitiveValues && value.isPrimitive();
        }
        
        // FIXME: The following loop continues to call toString on subsequent values even after
        // a toString call raises an exception.
        
        for (size_t i = 0; i < lengthNotIncludingUndefined; i++)
            values[i].second = values[i].first.toWTFStringInline(exec);
        
        if (exec->hadException()) {
            Heap::heap(this)->popTempSortVector(&values);
            return;
        }
        
        // FIXME: Since we sort by string value, a fast algorithm might be to use a radix sort. That would be O(N) rather
        // than O(N log N).
        
#if HAVE(MERGESORT)
        if (isSortingPrimitiveValues)
            qsort(values.begin(), values.size(), sizeof(ValueStringPair), compareByStringPairForQSort);
        else
            mergesort(values.begin(), values.size(), sizeof(ValueStringPair), compareByStringPairForQSort);
#else
        // FIXME: The qsort library function is likely to not be a stable sort.
        // ECMAScript-262 does not specify a stable sort, but in practice, browsers perform a stable sort.
        qsort(values.begin(), values.size(), sizeof(ValueStringPair), compareByStringPairForQSort);
#endif
        
        // If the toString function changed the length of the array or vector storage,
        // increase the length to handle the orignal number of actual values.
        if (storage->vectorLength() < lengthNotIncludingUndefined) {
            increaseVectorLength(exec->globalData(), lengthNotIncludingUndefined);
            storage = m_butterfly->arrayStorage();
        }
        if (storage->length() < lengthNotIncludingUndefined)
            storage->setLength(lengthNotIncludingUndefined);
        
        JSGlobalData& globalData = exec->globalData();
        for (size_t i = 0; i < lengthNotIncludingUndefined; i++)
            storage->m_vector[i].set(globalData, this, values[i].first);
        
        Heap::heap(this)->popTempSortVector(&values);
        
        checkIndexingConsistency(SortConsistencyCheck);
        return;
    }
        
    default:
        ASSERT_NOT_REACHED();
    }
}

struct AVLTreeNodeForArrayCompare {
    JSValue value;

    // Child pointers.  The high bit of gt is robbed and used as the
    // balance factor sign.  The high bit of lt is robbed and used as
    // the magnitude of the balance factor.
    int32_t gt;
    int32_t lt;
};

struct AVLTreeAbstractorForArrayCompare {
    typedef int32_t handle; // Handle is an index into m_nodes vector.
    typedef JSValue key;
    typedef int32_t size;

    Vector<AVLTreeNodeForArrayCompare> m_nodes;
    ExecState* m_exec;
    JSValue m_compareFunction;
    CallType m_compareCallType;
    const CallData* m_compareCallData;
    OwnPtr<CachedCall> m_cachedCall;

    handle get_less(handle h) { return m_nodes[h].lt & 0x7FFFFFFF; }
    void set_less(handle h, handle lh) { m_nodes[h].lt &= 0x80000000; m_nodes[h].lt |= lh; }
    handle get_greater(handle h) { return m_nodes[h].gt & 0x7FFFFFFF; }
    void set_greater(handle h, handle gh) { m_nodes[h].gt &= 0x80000000; m_nodes[h].gt |= gh; }

    int get_balance_factor(handle h)
    {
        if (m_nodes[h].gt & 0x80000000)
            return -1;
        return static_cast<unsigned>(m_nodes[h].lt) >> 31;
    }

    void set_balance_factor(handle h, int bf)
    {
        if (bf == 0) {
            m_nodes[h].lt &= 0x7FFFFFFF;
            m_nodes[h].gt &= 0x7FFFFFFF;
        } else {
            m_nodes[h].lt |= 0x80000000;
            if (bf < 0)
                m_nodes[h].gt |= 0x80000000;
            else
                m_nodes[h].gt &= 0x7FFFFFFF;
        }
    }

    int compare_key_key(key va, key vb)
    {
        ASSERT(!va.isUndefined());
        ASSERT(!vb.isUndefined());

        if (m_exec->hadException())
            return 1;

        double compareResult;
        if (m_cachedCall) {
            m_cachedCall->setThis(jsUndefined());
            m_cachedCall->setArgument(0, va);
            m_cachedCall->setArgument(1, vb);
            compareResult = m_cachedCall->call().toNumber(m_cachedCall->newCallFrame(m_exec));
        } else {
            MarkedArgumentBuffer arguments;
            arguments.append(va);
            arguments.append(vb);
            compareResult = call(m_exec, m_compareFunction, m_compareCallType, *m_compareCallData, jsUndefined(), arguments).toNumber(m_exec);
        }
        return (compareResult < 0) ? -1 : 1; // Not passing equality through, because we need to store all values, even if equivalent.
    }

    int compare_key_node(key k, handle h) { return compare_key_key(k, m_nodes[h].value); }
    int compare_node_node(handle h1, handle h2) { return compare_key_key(m_nodes[h1].value, m_nodes[h2].value); }

    static handle null() { return 0x7FFFFFFF; }
};

void JSArray::sort(ExecState* exec, JSValue compareFunction, CallType callType, const CallData& callData)
{
    ASSERT(!inSparseIndexingMode());
    
    switch (structure()->indexingType()) {
    case ArrayClass:
        return;
        
    case ArrayWithArrayStorage: {
        ArrayStorage* storage = m_butterfly->arrayStorage();
        checkIndexingConsistency();
        
        // FIXME: This ignores exceptions raised in the compare function or in toNumber.
        
        // The maximum tree depth is compiled in - but the caller is clearly up to no good
        // if a larger array is passed.
        ASSERT(storage->length() <= static_cast<unsigned>(std::numeric_limits<int>::max()));
        if (storage->length() > static_cast<unsigned>(std::numeric_limits<int>::max()))
            return;
        
        unsigned usedVectorLength = min(storage->length(), storage->vectorLength());
        unsigned nodeCount = usedVectorLength + (storage->m_sparseMap ? storage->m_sparseMap->size() : 0);
        
        if (!nodeCount)
            return;
        
        AVLTree<AVLTreeAbstractorForArrayCompare, 44> tree; // Depth 44 is enough for 2^31 items
        tree.abstractor().m_exec = exec;
        tree.abstractor().m_compareFunction = compareFunction;
        tree.abstractor().m_compareCallType = callType;
        tree.abstractor().m_compareCallData = &callData;
        tree.abstractor().m_nodes.grow(nodeCount);
        
        if (callType == CallTypeJS)
            tree.abstractor().m_cachedCall = adoptPtr(new CachedCall(exec, jsCast<JSFunction*>(compareFunction), 2));
        
        if (!tree.abstractor().m_nodes.begin()) {
            throwOutOfMemoryError(exec);
            return;
        }
        
        // FIXME: If the compare function modifies the array, the vector, map, etc. could be modified
        // right out from under us while we're building the tree here.
        
        unsigned numDefined = 0;
        unsigned numUndefined = 0;
        
        // Iterate over the array, ignoring missing values, counting undefined ones, and inserting all other ones into the tree.
        for (; numDefined < usedVectorLength; ++numDefined) {
            JSValue v = storage->m_vector[numDefined].get();
            if (!v || v.isUndefined())
                break;
            tree.abstractor().m_nodes[numDefined].value = v;
            tree.insert(numDefined);
        }
        for (unsigned i = numDefined; i < usedVectorLength; ++i) {
            JSValue v = storage->m_vector[i].get();
            if (v) {
                if (v.isUndefined())
                    ++numUndefined;
                else {
                    tree.abstractor().m_nodes[numDefined].value = v;
                    tree.insert(numDefined);
                    ++numDefined;
                }
            }
        }
        
        unsigned newUsedVectorLength = numDefined + numUndefined;
        
        if (SparseArrayValueMap* map = storage->m_sparseMap.get()) {
            newUsedVectorLength += map->size();
            if (newUsedVectorLength > storage->vectorLength()) {
                // Check that it is possible to allocate an array large enough to hold all the entries.
                if ((newUsedVectorLength > MAX_STORAGE_VECTOR_LENGTH) || !increaseVectorLength(exec->globalData(), newUsedVectorLength)) {
                    throwOutOfMemoryError(exec);
                    return;
                }
                storage = m_butterfly->arrayStorage();
            }
            
            SparseArrayValueMap::const_iterator end = map->end();
            for (SparseArrayValueMap::const_iterator it = map->begin(); it != end; ++it) {
                tree.abstractor().m_nodes[numDefined].value = it->second.getNonSparseMode();
                tree.insert(numDefined);
                ++numDefined;
            }
            
            deallocateSparseIndexMap();
        }
        
        ASSERT(tree.abstractor().m_nodes.size() >= numDefined);
        
        // FIXME: If the compare function changed the length of the array, the following might be
        // modifying the vector incorrectly.
        
        // Copy the values back into m_storage.
        AVLTree<AVLTreeAbstractorForArrayCompare, 44>::Iterator iter;
        iter.start_iter_least(tree);
        JSGlobalData& globalData = exec->globalData();
        for (unsigned i = 0; i < numDefined; ++i) {
            storage->m_vector[i].set(globalData, this, tree.abstractor().m_nodes[*iter].value);
            ++iter;
        }
        
        // Put undefined values back in.
        for (unsigned i = numDefined; i < newUsedVectorLength; ++i)
            storage->m_vector[i].setUndefined();
        
        // Ensure that unused values in the vector are zeroed out.
        for (unsigned i = newUsedVectorLength; i < usedVectorLength; ++i)
            storage->m_vector[i].clear();
        
        storage->m_numValuesInVector = newUsedVectorLength;
        
        checkIndexingConsistency(SortConsistencyCheck);
        return;
    }
        
    default:
        ASSERT_NOT_REACHED();
    }
}

void JSArray::fillArgList(ExecState* exec, MarkedArgumentBuffer& args)
{
    switch (structure()->indexingType()) {
    case ArrayClass:
        return;
    
    case ArrayWithArrayStorage: {
        ArrayStorage* storage = m_butterfly->arrayStorage();
        
        WriteBarrier<Unknown>* vector = storage->m_vector;
        unsigned vectorEnd = min(storage->length(), storage->vectorLength());
        unsigned i = 0;
        for (; i < vectorEnd; ++i) {
            WriteBarrier<Unknown>& v = vector[i];
            if (!v)
                break;
            args.append(v.get());
        }
        
        for (; i < storage->length(); ++i)
            args.append(get(exec, i));
        return;
    }
        
    default:
        ASSERT_NOT_REACHED();
    }
}

void JSArray::copyToArguments(ExecState* exec, CallFrame* callFrame, uint32_t length)
{
    ASSERT(length == this->length());
    switch (structure()->indexingType()) {
    case ArrayClass:
        return;
        
    case ArrayWithArrayStorage: {
        ArrayStorage* storage = m_butterfly->arrayStorage();
        unsigned i = 0;
        WriteBarrier<Unknown>* vector = storage->m_vector;
        unsigned vectorEnd = min(length, storage->vectorLength());
        for (; i < vectorEnd; ++i) {
            WriteBarrier<Unknown>& v = vector[i];
            if (!v)
                break;
            callFrame->setArgument(i, v.get());
        }
        
        for (; i < length; ++i)
            callFrame->setArgument(i, get(exec, i));
        return;
    }
        
    default:
        ASSERT_NOT_REACHED();
    }
}

unsigned JSArray::compactForSorting(JSGlobalData& globalData)
{
    ASSERT(!inSparseIndexingMode());

    checkIndexingConsistency();
    
    switch (structure()->indexingType()) {
    case ArrayClass:
        return 0;

    case ArrayWithArrayStorage: {
        ArrayStorage* storage = m_butterfly->arrayStorage();
        
        unsigned usedVectorLength = min(storage->length(), storage->vectorLength());
        
        unsigned numDefined = 0;
        unsigned numUndefined = 0;
        
        for (; numDefined < usedVectorLength; ++numDefined) {
            JSValue v = storage->m_vector[numDefined].get();
            if (!v || v.isUndefined())
                break;
        }
        
        for (unsigned i = numDefined; i < usedVectorLength; ++i) {
            JSValue v = storage->m_vector[i].get();
            if (v) {
                if (v.isUndefined())
                    ++numUndefined;
                else
                    storage->m_vector[numDefined++].setWithoutWriteBarrier(v);
            }
        }
        
        unsigned newUsedVectorLength = numDefined + numUndefined;
        
        if (SparseArrayValueMap* map = storage->m_sparseMap.get()) {
            newUsedVectorLength += map->size();
            if (newUsedVectorLength > storage->vectorLength()) {
                // Check that it is possible to allocate an array large enough to hold all the entries - if not,
                // exception is thrown by caller.
                if ((newUsedVectorLength > MAX_STORAGE_VECTOR_LENGTH) || !increaseVectorLength(globalData, newUsedVectorLength))
                    return 0;
                
                storage = m_butterfly->arrayStorage();
            }
            
            SparseArrayValueMap::const_iterator end = map->end();
            for (SparseArrayValueMap::const_iterator it = map->begin(); it != end; ++it)
                storage->m_vector[numDefined++].setWithoutWriteBarrier(it->second.getNonSparseMode());
            
            deallocateSparseIndexMap();
        }
        
        for (unsigned i = numDefined; i < newUsedVectorLength; ++i)
            storage->m_vector[i].setUndefined();
        for (unsigned i = newUsedVectorLength; i < usedVectorLength; ++i)
            storage->m_vector[i].clear();
        
        storage->m_numValuesInVector = newUsedVectorLength;
        
        checkIndexingConsistency(SortConsistencyCheck);
        
        return numDefined;
    }
        
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}


} // namespace JSC
