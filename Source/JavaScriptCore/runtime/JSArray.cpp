/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "CopiedSpace.h"
#include "CopiedSpaceInlineMethods.h"
#include "CachedCall.h"
#include "Error.h"
#include "Executable.h"
#include "GetterSetter.h"
#include "PropertyNameArray.h"
#include <wtf/AVLTree.h>
#include <wtf/Assertions.h>
#include <wtf/OwnPtr.h>
#include <Operations.h>

using namespace std;
using namespace WTF;

namespace JSC {


ASSERT_CLASS_FITS_IN_CELL(JSArray);
ASSERT_HAS_TRIVIAL_DESTRUCTOR(JSArray);

// Overview of JSArray
//
// Properties of JSArray objects may be stored in one of three locations:
//   * The regular JSObject property map.
//   * A storage vector.
//   * A sparse map of array entries.
//
// Properties with non-numeric identifiers, with identifiers that are not representable
// as an unsigned integer, or where the value is greater than  MAX_ARRAY_INDEX
// (specifically, this is only one property - the value 0xFFFFFFFFU as an unsigned 32-bit
// integer) are not considered array indices and will be stored in the JSObject property map.
//
// All properties with a numeric identifer, representable as an unsigned integer i,
// where (i <= MAX_ARRAY_INDEX), are an array index and will be stored in either the
// storage vector or the sparse map.  An array index i will be handled in the following
// fashion:
//
//   * Where (i < MIN_SPARSE_ARRAY_INDEX) the value will be stored in the storage vector,
//     unless the array is in SparseMode in which case all properties go into the map.
//   * Where (MIN_SPARSE_ARRAY_INDEX <= i <= MAX_STORAGE_VECTOR_INDEX) the value will either
//     be stored in the storage vector or in the sparse array, depending on the density of
//     data that would be stored in the vector (a vector being used where at least
//     (1 / minDensityMultiplier) of the entries would be populated).
//   * Where (MAX_STORAGE_VECTOR_INDEX < i <= MAX_ARRAY_INDEX) the value will always be stored
//     in the sparse array.

// The definition of MAX_STORAGE_VECTOR_LENGTH is dependant on the definition storageSize
// function below - the MAX_STORAGE_VECTOR_LENGTH limit is defined such that the storage
// size calculation cannot overflow.  (sizeof(ArrayStorage) - sizeof(WriteBarrier<Unknown>)) +
// (vectorLength * sizeof(WriteBarrier<Unknown>)) must be <= 0xFFFFFFFFU (which is maximum value of size_t).
#define MAX_STORAGE_VECTOR_LENGTH static_cast<unsigned>((0xFFFFFFFFU - (sizeof(ArrayStorage) - sizeof(WriteBarrier<Unknown>))) / sizeof(WriteBarrier<Unknown>))

// These values have to be macros to be used in max() and min() without introducing
// a PIC branch in Mach-O binaries, see <rdar://problem/5971391>.
#define MIN_SPARSE_ARRAY_INDEX 10000U
#define MAX_STORAGE_VECTOR_INDEX (MAX_STORAGE_VECTOR_LENGTH - 1)
// 0xFFFFFFFF is a bit weird -- is not an array index even though it's an integer.
#define MAX_ARRAY_INDEX 0xFFFFFFFEU

// The value BASE_VECTOR_LEN is the maximum number of vector elements we'll allocate
// for an array that was created with a sepcified length (e.g. a = new Array(123))
#define BASE_VECTOR_LEN 4U
    
// The upper bound to the size we'll grow a zero length array when the first element
// is added.
#define FIRST_VECTOR_GROW 4U

// Our policy for when to use a vector and when to use a sparse map.
// For all array indices under MIN_SPARSE_ARRAY_INDEX, we always use a vector.
// When indices greater than MIN_SPARSE_ARRAY_INDEX are involved, we use a vector
// as long as it is 1/8 full. If more sparse than that, we use a map.
static const unsigned minDensityMultiplier = 8;

const ClassInfo JSArray::s_info = {"Array", &JSNonFinalObject::s_info, 0, 0, CREATE_METHOD_TABLE(JSArray)};

// We keep track of the size of the last array after it was grown.  We use this
// as a simple heuristic for as the value to grow the next array from size 0.
// This value is capped by the constant FIRST_VECTOR_GROW defined above.
static unsigned lastArraySize = 0;

static inline bool isDenseEnoughForVector(unsigned length, unsigned numValues)
{
    return length <= MIN_SPARSE_ARRAY_INDEX || length / minDensityMultiplier <= numValues;
}

static bool reject(ExecState* exec, bool throwException, const char* message)
{
    if (throwException)
        throwTypeError(exec, message);
    return false;
}

#if !CHECK_ARRAY_CONSISTENCY

inline void JSArray::checkConsistency(ConsistencyCheckType)
{
}

#endif

void JSArray::finishCreation(JSGlobalData& globalData, unsigned initialLength)
{
    Base::finishCreation(globalData);
    ASSERT(inherits(&s_info));

    unsigned initialVectorLength = BASE_VECTOR_LEN;
    unsigned initialStorageSize = storageSize(initialVectorLength);

    void* newStorage = 0;
    if (!globalData.heap.tryAllocateStorage(initialStorageSize, &newStorage))
        CRASH();
    
    m_storage = static_cast<ArrayStorage*>(newStorage);
    m_storage->m_allocBase = m_storage;
    m_storage->m_length = initialLength;
    m_vectorLength = initialVectorLength;
    m_storage->m_numValuesInVector = 0;
#if CHECK_ARRAY_CONSISTENCY
    m_storage->m_inCompactInitialization = false;
#endif

    checkConsistency();
}

JSArray* JSArray::tryFinishCreationUninitialized(JSGlobalData& globalData, unsigned initialLength)
{
    Base::finishCreation(globalData);
    ASSERT(inherits(&s_info));

    // Check for lengths larger than we can handle with a vector.
    if (initialLength > MAX_STORAGE_VECTOR_LENGTH)
        return 0;

    unsigned initialVectorLength = max(initialLength, BASE_VECTOR_LEN);
    unsigned initialStorageSize = storageSize(initialVectorLength);

    void* newStorage = 0;
    if (!globalData.heap.tryAllocateStorage(initialStorageSize, &newStorage))
        CRASH();
    
    m_storage = static_cast<ArrayStorage*>(newStorage);
    m_storage->m_allocBase = m_storage;
    m_storage->m_length = initialLength;
    m_vectorLength = initialVectorLength;
    m_storage->m_numValuesInVector = initialLength;

#if CHECK_ARRAY_CONSISTENCY
    m_storage->m_initializationIndex = 0;
    m_storage->m_inCompactInitialization = true;
#endif

    return this;
}

// This function can be called multiple times on the same object.
void JSArray::finalize(JSCell* cell)
{
    JSArray* thisObject = jsCast<JSArray*>(cell);
    thisObject->checkConsistency(DestructorConsistencyCheck);
    thisObject->deallocateSparseMap();
}

inline SparseArrayValueMap::AddResult SparseArrayValueMap::add(JSArray* array, unsigned i)
{
    SparseArrayEntry entry;
    entry.setWithoutWriteBarrier(jsUndefined());

    AddResult result = m_map.add(i, entry);
    size_t capacity = m_map.capacity();
    if (capacity != m_reportedCapacity) {
        Heap::heap(array)->reportExtraMemoryCost((capacity - m_reportedCapacity) * (sizeof(unsigned) + sizeof(WriteBarrier<Unknown>)));
        m_reportedCapacity = capacity;
    }
    return result;
}

inline void SparseArrayValueMap::put(ExecState* exec, JSArray* array, unsigned i, JSValue value, bool shouldThrow)
{
    AddResult result = add(array, i);
    SparseArrayEntry& entry = result.iterator->second;

    // To save a separate find & add, we first always add to the sparse map.
    // In the uncommon case that this is a new property, and the array is not
    // extensible, this is not the right thing to have done - so remove again.
    if (result.isNewEntry && !array->isExtensible()) {
        remove(result.iterator);
        if (shouldThrow)
            throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
        return;
    }

    if (!(entry.attributes & Accessor)) {
        if (entry.attributes & ReadOnly) {
            if (shouldThrow)
                throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
            return;
        }

        entry.set(exec->globalData(), array, value);
        return;
    }

    JSValue accessor = entry.Base::get();
    ASSERT(accessor.isGetterSetter());
    JSObject* setter = asGetterSetter(accessor)->setter();
    
    if (!setter) {
        if (shouldThrow)
            throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
        return;
    }

    CallData callData;
    CallType callType = setter->methodTable()->getCallData(setter, callData);
    MarkedArgumentBuffer args;
    args.append(value);
    call(exec, setter, callType, callData, array, args);
}

inline bool SparseArrayValueMap::putDirect(ExecState* exec, JSArray* array, unsigned i, JSValue value, bool shouldThrow)
{
    AddResult result = add(array, i);
    SparseArrayEntry& entry = result.iterator->second;

    // To save a separate find & add, we first always add to the sparse map.
    // In the uncommon case that this is a new property, and the array is not
    // extensible, this is not the right thing to have done - so remove again.
    if (result.isNewEntry && !array->isExtensible()) {
        remove(result.iterator);
        return reject(exec, shouldThrow, "Attempting to define property on object that is not extensible.");
    }

    entry.attributes = 0;
    entry.set(exec->globalData(), array, value);
    return true;
}

inline void SparseArrayEntry::get(PropertySlot& slot) const
{
    JSValue value = Base::get();
    ASSERT(value);

    if (LIKELY(!value.isGetterSetter())) {
        slot.setValue(value);
        return;
    }

    JSObject* getter = asGetterSetter(value)->getter();
    if (!getter) {
        slot.setUndefined();
        return;
    }

    slot.setGetterSlot(getter);
}

inline void SparseArrayEntry::get(PropertyDescriptor& descriptor) const
{
    descriptor.setDescriptor(Base::get(), attributes);
}

inline JSValue SparseArrayEntry::get(ExecState* exec, JSArray* array) const
{
    JSValue result = Base::get();
    ASSERT(result);

    if (LIKELY(!result.isGetterSetter()))
        return result;

    JSObject* getter = asGetterSetter(result)->getter();
    if (!getter)
        return jsUndefined();

    CallData callData;
    CallType callType = getter->methodTable()->getCallData(getter, callData);
    return call(exec, getter, callType, callData, array, exec->emptyList());
}

inline JSValue SparseArrayEntry::getNonSparseMode() const
{
    ASSERT(!attributes);
    return Base::get();
}

inline void SparseArrayValueMap::visitChildren(SlotVisitor& visitor)
{
    iterator end = m_map.end();
    for (iterator it = m_map.begin(); it != end; ++it)
        visitor.append(&it->second);
}

void JSArray::allocateSparseMap(JSGlobalData& globalData)
{
    m_sparseValueMap = new SparseArrayValueMap;
    globalData.heap.addFinalizer(this, finalize);
}

void JSArray::deallocateSparseMap()
{
    delete m_sparseValueMap;
    m_sparseValueMap = 0;
}

void JSArray::enterDictionaryMode(JSGlobalData& globalData)
{
    ArrayStorage* storage = m_storage;
    SparseArrayValueMap* map = m_sparseValueMap;

    if (!map) {
        allocateSparseMap(globalData);
        map = m_sparseValueMap;
    }

    if (map->sparseMode())
        return;

    map->setSparseMode();

    unsigned usedVectorLength = min(storage->m_length, m_vectorLength);
    for (unsigned i = 0; i < usedVectorLength; ++i) {
        JSValue value = storage->m_vector[i].get();
        // This will always be a new entry in the map, so no need to check we can write,
        // and attributes are default so no need to set them.
        if (value)
            map->add(this, i).iterator->second.set(globalData, this, value);
    }

    void* newRawStorage = 0;
    if (!globalData.heap.tryAllocateStorage(storageSize(0), &newRawStorage))
        CRASH();
    
    ArrayStorage* newStorage = static_cast<ArrayStorage*>(newRawStorage);
    memcpy(newStorage, m_storage, storageSize(0));
    newStorage->m_allocBase = newStorage;
    m_storage = newStorage;
    m_indexBias = 0;
    m_vectorLength = 0;
}

void JSArray::putDescriptor(ExecState* exec, SparseArrayEntry* entryInMap, PropertyDescriptor& descriptor, PropertyDescriptor& oldDescriptor)
{
    if (descriptor.isDataDescriptor()) {
        if (descriptor.value())
            entryInMap->set(exec->globalData(), this, descriptor.value());
        else if (oldDescriptor.isAccessorDescriptor())
            entryInMap->set(exec->globalData(), this, jsUndefined());
        entryInMap->attributes = descriptor.attributesOverridingCurrent(oldDescriptor) & ~Accessor;
        return;
    }

    if (descriptor.isAccessorDescriptor()) {
        JSObject* getter = 0;
        if (descriptor.getterPresent())
            getter = descriptor.getterObject();
        else if (oldDescriptor.isAccessorDescriptor())
            getter = oldDescriptor.getterObject();
        JSObject* setter = 0;
        if (descriptor.setterPresent())
            setter = descriptor.setterObject();
        else if (oldDescriptor.isAccessorDescriptor())
            setter = oldDescriptor.setterObject();

        GetterSetter* accessor = GetterSetter::create(exec);
        if (getter)
            accessor->setGetter(exec->globalData(), getter);
        if (setter)
            accessor->setSetter(exec->globalData(), setter);

        entryInMap->set(exec->globalData(), this, accessor);
        entryInMap->attributes = descriptor.attributesOverridingCurrent(oldDescriptor) & ~ReadOnly;
        return;
    }

    ASSERT(descriptor.isGenericDescriptor());
    entryInMap->attributes = descriptor.attributesOverridingCurrent(oldDescriptor);
}

// Defined in ES5.1 8.12.9
bool JSArray::defineOwnNumericProperty(ExecState* exec, unsigned index, PropertyDescriptor& descriptor, bool throwException)
{
    ASSERT(index != 0xFFFFFFFF);

    if (!inSparseMode()) {
        // Fast case: we're putting a regular property to a regular array
        // FIXME: this will pessimistically assume that if attributes are missing then they'll default to false
        // – however if the property currently exists missing attributes will override from their current 'true'
        // state (i.e. defineOwnProperty could be used to set a value without needing to entering 'SparseMode').
        if (!descriptor.attributes()) {
            ASSERT(!descriptor.isAccessorDescriptor());
            return putDirectIndex(exec, index, descriptor.value(), throwException);
        }

        enterDictionaryMode(exec->globalData());
    }

    SparseArrayValueMap* map = m_sparseValueMap;
    ASSERT(map);

    // 1. Let current be the result of calling the [[GetOwnProperty]] internal method of O with property name P.
    SparseArrayValueMap::AddResult result = map->add(this, index);
    SparseArrayEntry* entryInMap = &result.iterator->second;

    // 2. Let extensible be the value of the [[Extensible]] internal property of O.
    // 3. If current is undefined and extensible is false, then Reject.
    // 4. If current is undefined and extensible is true, then
    if (result.isNewEntry) {
        if (!isExtensible()) {
            map->remove(result.iterator);
            return reject(exec, throwException, "Attempting to define property on object that is not extensible.");
        }

        // 4.a. If IsGenericDescriptor(Desc) or IsDataDescriptor(Desc) is true, then create an own data property
        // named P of object O whose [[Value]], [[Writable]], [[Enumerable]] and [[Configurable]] attribute values
        // are described by Desc. If the value of an attribute field of Desc is absent, the attribute of the newly
        // created property is set to its default value.
        // 4.b. Else, Desc must be an accessor Property Descriptor so, create an own accessor property named P of
        // object O whose [[Get]], [[Set]], [[Enumerable]] and [[Configurable]] attribute values are described by
        // Desc. If the value of an attribute field of Desc is absent, the attribute of the newly created property
        // is set to its default value.
        // 4.c. Return true.

        PropertyDescriptor defaults;
        entryInMap->setWithoutWriteBarrier(jsUndefined());
        entryInMap->attributes = DontDelete | DontEnum | ReadOnly;
        entryInMap->get(defaults);

        putDescriptor(exec, entryInMap, descriptor, defaults);
        if (index >= m_storage->m_length)
            m_storage->m_length = index + 1;
        return true;
    }

    // 5. Return true, if every field in Desc is absent.
    // 6. Return true, if every field in Desc also occurs in current and the value of every field in Desc is the same value as the corresponding field in current when compared using the SameValue algorithm (9.12).
    PropertyDescriptor current;
    entryInMap->get(current);
    if (descriptor.isEmpty() || descriptor.equalTo(exec, current))
        return true;

    // 7. If the [[Configurable]] field of current is false then
    if (!current.configurable()) {
        // 7.a. Reject, if the [[Configurable]] field of Desc is true.
        if (descriptor.configurablePresent() && descriptor.configurable())
            return reject(exec, throwException, "Attempting to change configurable attribute of unconfigurable property.");
        // 7.b. Reject, if the [[Enumerable]] field of Desc is present and the [[Enumerable]] fields of current and Desc are the Boolean negation of each other.
        if (descriptor.enumerablePresent() && current.enumerable() != descriptor.enumerable())
            return reject(exec, throwException, "Attempting to change enumerable attribute of unconfigurable property.");
    }

    // 8. If IsGenericDescriptor(Desc) is true, then no further validation is required.
    if (!descriptor.isGenericDescriptor()) {
        // 9. Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) have different results, then
        if (current.isDataDescriptor() != descriptor.isDataDescriptor()) {
            // 9.a. Reject, if the [[Configurable]] field of current is false.
            if (!current.configurable())
                return reject(exec, throwException, "Attempting to change access mechanism for an unconfigurable property.");
            // 9.b. If IsDataDescriptor(current) is true, then convert the property named P of object O from a
            // data property to an accessor property. Preserve the existing values of the converted property‘s
            // [[Configurable]] and [[Enumerable]] attributes and set the rest of the property‘s attributes to
            // their default values.
            // 9.c. Else, convert the property named P of object O from an accessor property to a data property.
            // Preserve the existing values of the converted property‘s [[Configurable]] and [[Enumerable]]
            // attributes and set the rest of the property‘s attributes to their default values.
        } else if (current.isDataDescriptor() && descriptor.isDataDescriptor()) {
            // 10. Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both true, then
            // 10.a. If the [[Configurable]] field of current is false, then
            if (!current.configurable() && !current.writable()) {
                // 10.a.i. Reject, if the [[Writable]] field of current is false and the [[Writable]] field of Desc is true.
                if (descriptor.writable())
                    return reject(exec, throwException, "Attempting to change writable attribute of unconfigurable property.");
                // 10.a.ii. If the [[Writable]] field of current is false, then
                // 10.a.ii.1. Reject, if the [[Value]] field of Desc is present and SameValue(Desc.[[Value]], current.[[Value]]) is false.
                if (descriptor.value() && !sameValue(exec, descriptor.value(), current.value()))
                    return reject(exec, throwException, "Attempting to change value of a readonly property.");
            }
            // 10.b. else, the [[Configurable]] field of current is true, so any change is acceptable.
        } else {
            ASSERT(current.isAccessorDescriptor() && current.getterPresent() && current.setterPresent());
            // 11. Else, IsAccessorDescriptor(current) and IsAccessorDescriptor(Desc) are both true so, if the [[Configurable]] field of current is false, then
            if (!current.configurable()) {
                // 11.i. Reject, if the [[Set]] field of Desc is present and SameValue(Desc.[[Set]], current.[[Set]]) is false.
                if (descriptor.setterPresent() && descriptor.setter() != current.setter())
                    return reject(exec, throwException, "Attempting to change the setter of an unconfigurable property.");
                // 11.ii. Reject, if the [[Get]] field of Desc is present and SameValue(Desc.[[Get]], current.[[Get]]) is false.
                if (descriptor.getterPresent() && descriptor.getter() != current.getter())
                    return reject(exec, throwException, "Attempting to change the getter of an unconfigurable property.");
            }
        }
    }

    // 12. For each attribute field of Desc that is present, set the correspondingly named attribute of the property named P of object O to the value of the field.
    putDescriptor(exec, entryInMap, descriptor, current);
    // 13. Return true.
    return true;
}

void JSArray::setLengthWritable(ExecState* exec, bool writable)
{
    ASSERT(isLengthWritable() || !writable);
    if (!isLengthWritable() || writable)
        return;

    enterDictionaryMode(exec->globalData());

    SparseArrayValueMap* map = m_sparseValueMap;
    ASSERT(map);
    map->setLengthIsReadOnly();
}

// Defined in ES5.1 15.4.5.1
bool JSArray::defineOwnProperty(JSObject* object, ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor, bool throwException)
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
    bool isArrayIndex;
    // a. Let index be ToUint32(P).
    unsigned index = propertyName.toArrayIndex(isArrayIndex);
    if (isArrayIndex) {
        // b. Reject if index >= oldLen and oldLenDesc.[[Writable]] is false.
        if (index >= array->length() && !array->isLengthWritable())
            return reject(exec, throwException, "Attempting to define numeric property on array with non-writable length property.");
        // c. Let succeeded be the result of calling the default [[DefineOwnProperty]] internal method (8.12.9) on A passing P, Desc, and false as arguments.
        // d. Reject if succeeded is false.
        // e. If index >= oldLen
        // e.i. Set oldLenDesc.[[Value]] to index + 1.
        // e.ii. Call the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length", oldLenDesc, and false as arguments. This call will always return true.
        // f. Return true.
        return array->defineOwnNumericProperty(exec, index, descriptor, throwException);
    }

    return JSObject::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
}

bool JSArray::getOwnPropertySlotByIndex(JSCell* cell, ExecState* exec, unsigned i, PropertySlot& slot)
{
    JSArray* thisObject = jsCast<JSArray*>(cell);
    ArrayStorage* storage = thisObject->m_storage;

    if (i >= storage->m_length) {
        if (i > MAX_ARRAY_INDEX)
            return thisObject->methodTable()->getOwnPropertySlot(thisObject, exec, Identifier::from(exec, i), slot);
        return false;
    }

    if (i < thisObject->m_vectorLength) {
        JSValue value = storage->m_vector[i].get();
        if (value) {
            slot.setValue(value);
            return true;
        }
    } else if (SparseArrayValueMap* map = thisObject->m_sparseValueMap) {
        SparseArrayValueMap::iterator it = map->find(i);
        if (it != map->notFound()) {
            it->second.get(slot);
            return true;
        }
    }

    return JSObject::getOwnPropertySlot(thisObject, exec, Identifier::from(exec, i), slot);
}

bool JSArray::getOwnPropertySlot(JSCell* cell, ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    JSArray* thisObject = jsCast<JSArray*>(cell);
    if (propertyName == exec->propertyNames().length) {
        slot.setValue(jsNumber(thisObject->length()));
        return true;
    }

    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(isArrayIndex);
    if (isArrayIndex)
        return JSArray::getOwnPropertySlotByIndex(thisObject, exec, i, slot);

    return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

bool JSArray::getOwnPropertyDescriptor(JSObject* object, ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    JSArray* thisObject = jsCast<JSArray*>(object);
    if (propertyName == exec->propertyNames().length) {
        descriptor.setDescriptor(jsNumber(thisObject->length()), thisObject->isLengthWritable() ? DontDelete | DontEnum : DontDelete | DontEnum | ReadOnly);
        return true;
    }

    ArrayStorage* storage = thisObject->m_storage;
    
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(isArrayIndex);
    if (isArrayIndex) {
        if (i >= storage->m_length)
            return false;
        if (i < thisObject->m_vectorLength) {
            WriteBarrier<Unknown>& value = storage->m_vector[i];
            if (value) {
                descriptor.setDescriptor(value.get(), 0);
                return true;
            }
        } else if (SparseArrayValueMap* map = thisObject->m_sparseValueMap) {
            SparseArrayValueMap::iterator it = map->find(i);
            if (it != map->notFound()) {
                it->second.get(descriptor);
                return true;
            }
        }
    }
    return JSObject::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor);
}

// ECMA 15.4.5.1
void JSArray::put(JSCell* cell, ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    JSArray* thisObject = jsCast<JSArray*>(cell);
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(isArrayIndex);
    if (isArrayIndex) {
        putByIndex(thisObject, exec, i, value, slot.isStrictMode());
        return;
    }

    if (propertyName == exec->propertyNames().length) {
        unsigned newLength = value.toUInt32(exec);
        if (value.toNumber(exec) != static_cast<double>(newLength)) {
            throwError(exec, createRangeError(exec, "Invalid array length"));
            return;
        }
        thisObject->setLength(exec, newLength, slot.isStrictMode());
        return;
    }

    JSObject::put(thisObject, exec, propertyName, value, slot);
}

void JSArray::putByIndex(JSCell* cell, ExecState* exec, unsigned i, JSValue value, bool shouldThrow)
{
    JSArray* thisObject = jsCast<JSArray*>(cell);
    thisObject->checkConsistency();

    ArrayStorage* storage = thisObject->m_storage;

    // Fast case - store to the vector.
    if (i < thisObject->m_vectorLength) {
        WriteBarrier<Unknown>& valueSlot = storage->m_vector[i];
        unsigned length = storage->m_length;

        // Update m_length & m_numValuesInVector as necessary.
        if (i >= length) {
            length = i + 1;
            storage->m_length = length;
            ++storage->m_numValuesInVector;
        } else if (!valueSlot)
            ++storage->m_numValuesInVector;

        valueSlot.set(exec->globalData(), thisObject, value);
        thisObject->checkConsistency();
        return;
    }

    // Handle 2^32-1 - this is not an array index (see ES5.1 15.4), and is treated as a regular property.
    if (UNLIKELY(i > MAX_ARRAY_INDEX)) {
        PutPropertySlot slot(shouldThrow);
        thisObject->methodTable()->put(thisObject, exec, Identifier::from(exec, i), value, slot);
        return;
    }

    // For all other cases, call putByIndexBeyondVectorLength.
    thisObject->putByIndexBeyondVectorLength(exec, i, value, shouldThrow);
    thisObject->checkConsistency();
}

void JSArray::putByIndexBeyondVectorLength(ExecState* exec, unsigned i, JSValue value, bool shouldThrow)
{
    JSGlobalData& globalData = exec->globalData();

    // i should be a valid array index that is outside of the current vector.
    ASSERT(i >= m_vectorLength);
    ASSERT(i <= MAX_ARRAY_INDEX);

    ArrayStorage* storage = m_storage;
    SparseArrayValueMap* map = m_sparseValueMap;

    // First, handle cases where we don't currently have a sparse map.
    if (LIKELY(!map)) {
        // If the array is not extensible, we should have entered dictionary mode, and created the spare map.
        ASSERT(isExtensible());
    
        // Update m_length if necessary.
        if (i >= storage->m_length)
            storage->m_length = i + 1;

        // Check that it is sensible to still be using a vector, and then try to grow the vector.
        if (LIKELY((isDenseEnoughForVector(i, storage->m_numValuesInVector)) && increaseVectorLength(globalData, i + 1))) {
            // success! - reread m_storage since it has likely been reallocated, and store to the vector.
            storage = m_storage;
            storage->m_vector[i].set(globalData, this, value);
            ++storage->m_numValuesInVector;
            return;
        }
        // We don't want to, or can't use a vector to hold this property - allocate a sparse map & add the value.
        allocateSparseMap(exec->globalData());
        map = m_sparseValueMap;
        map->put(exec, this, i, value, shouldThrow);
        return;
    }

    // Update m_length if necessary.
    unsigned length = storage->m_length;
    if (i >= length) {
        // Prohibit growing the array if length is not writable.
        if (map->lengthIsReadOnly() || !isExtensible()) {
            if (shouldThrow)
                throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
            return;
        }
        length = i + 1;
        storage->m_length = length;
    }

    // We are currently using a map - check whether we still want to be doing so.
    // We will continue  to use a sparse map if SparseMode is set, a vector would be too sparse, or if allocation fails.
    unsigned numValuesInArray = storage->m_numValuesInVector + map->size();
    if (map->sparseMode() || !isDenseEnoughForVector(length, numValuesInArray) || !increaseVectorLength(exec->globalData(), length)) {
        map->put(exec, this, i, value, shouldThrow);
        return;
    }

    // Reread m_storage afterincreaseVectorLength, update m_numValuesInVector.
    storage = m_storage;
    storage->m_numValuesInVector = numValuesInArray;

    // Copy all values from the map into the vector, and delete the map.
    WriteBarrier<Unknown>* vector = storage->m_vector;
    SparseArrayValueMap::const_iterator end = map->end();
    for (SparseArrayValueMap::const_iterator it = map->begin(); it != end; ++it)
        vector[it->first].set(globalData, this, it->second.getNonSparseMode());
    deallocateSparseMap();

    // Store the new property into the vector.
    WriteBarrier<Unknown>& valueSlot = vector[i];
    if (!valueSlot)
        ++storage->m_numValuesInVector;
    valueSlot.set(globalData, this, value);
}

bool JSArray::putDirectIndexBeyondVectorLength(ExecState* exec, unsigned i, JSValue value, bool shouldThrow)
{
    JSGlobalData& globalData = exec->globalData();

    // i should be a valid array index that is outside of the current vector.
    ASSERT(i >= m_vectorLength);
    ASSERT(i <= MAX_ARRAY_INDEX);

    ArrayStorage* storage = m_storage;
    SparseArrayValueMap* map = m_sparseValueMap;

    // First, handle cases where we don't currently have a sparse map.
    if (LIKELY(!map)) {
        // If the array is not extensible, we should have entered dictionary mode, and created the spare map.
        ASSERT(isExtensible());
    
        // Update m_length if necessary.
        if (i >= storage->m_length)
            storage->m_length = i + 1;

        // Check that it is sensible to still be using a vector, and then try to grow the vector.
        if (LIKELY((isDenseEnoughForVector(i, storage->m_numValuesInVector)) && increaseVectorLength(globalData, i + 1))) {
            // success! - reread m_storage since it has likely been reallocated, and store to the vector.
            storage = m_storage;
            storage->m_vector[i].set(globalData, this, value);
            ++storage->m_numValuesInVector;
            return true;
        }
        // We don't want to, or can't use a vector to hold this property - allocate a sparse map & add the value.
        allocateSparseMap(exec->globalData());
        map = m_sparseValueMap;
        return map->putDirect(exec, this, i, value, shouldThrow);
    }

    // Update m_length if necessary.
    unsigned length = storage->m_length;
    if (i >= length) {
        // Prohibit growing the array if length is not writable.
        if (map->lengthIsReadOnly())
            return reject(exec, shouldThrow, StrictModeReadonlyPropertyWriteError);
        if (!isExtensible())
            return reject(exec, shouldThrow, "Attempting to define property on object that is not extensible.");
        length = i + 1;
        storage->m_length = length;
    }

    // We are currently using a map - check whether we still want to be doing so.
    // We will continue  to use a sparse map if SparseMode is set, a vector would be too sparse, or if allocation fails.
    unsigned numValuesInArray = storage->m_numValuesInVector + map->size();
    if (map->sparseMode() || !isDenseEnoughForVector(length, numValuesInArray) || !increaseVectorLength(exec->globalData(), length))
        return map->putDirect(exec, this, i, value, shouldThrow);

    // Reread m_storage afterincreaseVectorLength, update m_numValuesInVector.
    storage = m_storage;
    storage->m_numValuesInVector = numValuesInArray;

    // Copy all values from the map into the vector, and delete the map.
    WriteBarrier<Unknown>* vector = storage->m_vector;
    SparseArrayValueMap::const_iterator end = map->end();
    for (SparseArrayValueMap::const_iterator it = map->begin(); it != end; ++it)
        vector[it->first].set(globalData, this, it->second.getNonSparseMode());
    deallocateSparseMap();

    // Store the new property into the vector.
    WriteBarrier<Unknown>& valueSlot = vector[i];
    if (!valueSlot)
        ++storage->m_numValuesInVector;
    valueSlot.set(globalData, this, value);
    return true;
}

bool JSArray::deleteProperty(JSCell* cell, ExecState* exec, const Identifier& propertyName)
{
    JSArray* thisObject = jsCast<JSArray*>(cell);
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(isArrayIndex);
    if (isArrayIndex)
        return thisObject->methodTable()->deletePropertyByIndex(thisObject, exec, i);

    if (propertyName == exec->propertyNames().length)
        return false;

    return JSObject::deleteProperty(thisObject, exec, propertyName);
}

bool JSArray::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned i)
{
    JSArray* thisObject = jsCast<JSArray*>(cell);
    thisObject->checkConsistency();

    if (i > MAX_ARRAY_INDEX)
        return thisObject->methodTable()->deleteProperty(thisObject, exec, Identifier::from(exec, i));

    ArrayStorage* storage = thisObject->m_storage;
    
    if (i < thisObject->m_vectorLength) {
        WriteBarrier<Unknown>& valueSlot = storage->m_vector[i];
        if (valueSlot) {
            valueSlot.clear();
            --storage->m_numValuesInVector;
        }
    } else if (SparseArrayValueMap* map = thisObject->m_sparseValueMap) {
        SparseArrayValueMap::iterator it = map->find(i);
        if (it != map->notFound()) {
            if (it->second.attributes & DontDelete)
                return false;
            map->remove(it);
        }
    }

    thisObject->checkConsistency();
    return true;
}

static int compareKeysForQSort(const void* a, const void* b)
{
    unsigned da = *static_cast<const unsigned*>(a);
    unsigned db = *static_cast<const unsigned*>(b);
    return (da > db) - (da < db);
}

void JSArray::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSArray* thisObject = jsCast<JSArray*>(object);
    // FIXME: Filling PropertyNameArray with an identifier for every integer
    // is incredibly inefficient for large arrays. We need a different approach,
    // which almost certainly means a different structure for PropertyNameArray.

    ArrayStorage* storage = thisObject->m_storage;
    
    unsigned usedVectorLength = min(storage->m_length, thisObject->m_vectorLength);
    for (unsigned i = 0; i < usedVectorLength; ++i) {
        if (storage->m_vector[i])
            propertyNames.add(Identifier::from(exec, i));
    }

    if (SparseArrayValueMap* map = thisObject->m_sparseValueMap) {
        Vector<unsigned> keys;
        keys.reserveCapacity(map->size());
        
        SparseArrayValueMap::const_iterator end = map->end();
        for (SparseArrayValueMap::const_iterator it = map->begin(); it != end; ++it) {
            if (mode == IncludeDontEnumProperties || !(it->second.attributes & DontEnum))
                keys.append(static_cast<unsigned>(it->first));
        }

        qsort(keys.begin(), keys.size(), sizeof(unsigned), compareKeysForQSort);
        for (unsigned i = 0; i < keys.size(); ++i)
            propertyNames.add(Identifier::from(exec, keys[i]));
    }

    if (mode == IncludeDontEnumProperties)
        propertyNames.add(exec->propertyNames().length);

    JSObject::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

ALWAYS_INLINE unsigned JSArray::getNewVectorLength(unsigned desiredLength)
{
    ASSERT(desiredLength <= MAX_STORAGE_VECTOR_LENGTH);

    unsigned increasedLength;
    unsigned maxInitLength = min(m_storage->m_length, 100000U);

    if (desiredLength < maxInitLength)
        increasedLength = maxInitLength;
    else if (!m_vectorLength)
        increasedLength = max(desiredLength, lastArraySize);
    else {
        // Mathematically equivalent to:
        //   increasedLength = (newLength * 3 + 1) / 2;
        // or:
        //   increasedLength = (unsigned)ceil(newLength * 1.5));
        // This form is not prone to internal overflow.
        increasedLength = desiredLength + (desiredLength >> 1) + (desiredLength & 1);
    }

    ASSERT(increasedLength >= desiredLength);

    lastArraySize = min(increasedLength, FIRST_VECTOR_GROW);

    return min(increasedLength, MAX_STORAGE_VECTOR_LENGTH);
}

bool JSArray::increaseVectorLength(JSGlobalData& globalData, unsigned newLength)
{
    // This function leaves the array in an internally inconsistent state, because it does not move any values from sparse value map
    // to the vector. Callers have to account for that, because they can do it more efficiently.
    if (newLength > MAX_STORAGE_VECTOR_LENGTH)
        return false;

    ArrayStorage* storage = m_storage;

    unsigned vectorLength = m_vectorLength;
    ASSERT(newLength > vectorLength);
    unsigned newVectorLength = getNewVectorLength(newLength);

    // Fast case - there is no precapacity. In these cases a realloc makes sense.
    if (LIKELY(!m_indexBias)) {
        void* newStorage = storage->m_allocBase;
        if (!globalData.heap.tryReallocateStorage(&newStorage, storageSize(vectorLength), storageSize(newVectorLength)))
            return false;

        storage = m_storage = reinterpret_cast_ptr<ArrayStorage*>(static_cast<char*>(newStorage));
        m_storage->m_allocBase = newStorage;
        ASSERT(m_storage->m_allocBase);

        m_vectorLength = newVectorLength;
        
        return true;
    }

    // Remove some, but not all of the precapacity. Atomic decay, & capped to not overflow array length.
    unsigned newIndexBias = min(m_indexBias >> 1, MAX_STORAGE_VECTOR_LENGTH - newVectorLength);
    // Calculate new stoarge capcity, allowing room for the pre-capacity.
    unsigned newStorageCapacity = newVectorLength + newIndexBias;
    void* newAllocBase = 0;
    if (!globalData.heap.tryAllocateStorage(storageSize(newStorageCapacity), &newAllocBase))    
        return false;
    // The sum of m_vectorLength and m_indexBias will never exceed MAX_STORAGE_VECTOR_LENGTH.
    ASSERT(m_vectorLength <= MAX_STORAGE_VECTOR_LENGTH && (MAX_STORAGE_VECTOR_LENGTH - m_vectorLength) >= m_indexBias);

    m_vectorLength = newVectorLength;
    m_indexBias = newIndexBias;
    m_storage = reinterpret_cast_ptr<ArrayStorage*>(reinterpret_cast<WriteBarrier<Unknown>*>(newAllocBase) + m_indexBias);

    // Copy the ArrayStorage header & current contents of the vector.
    memmove(m_storage, storage, storageSize(vectorLength));

    // Free the old allocation, update m_allocBase.
    m_storage->m_allocBase = newAllocBase;

    return true;
}

// This method makes room in the vector, but leaves the new space uncleared.
bool JSArray::unshiftCountSlowCase(JSGlobalData& globalData, unsigned count)
{
    // If not, we should have handled this on the fast path.
    ASSERT(count > m_indexBias);

    ArrayStorage* storage = m_storage;

    // Step 1:
    // Gather 4 key metrics:
    //  * usedVectorLength - how many entries are currently in the vector (conservative estimate - fewer may be in use in sparse vectors).
    //  * requiredVectorLength - how many entries are will there be in the vector, after allocating space for 'count' more.
    //  * currentCapacity - what is the current size of the vector, including any pre-capacity.
    //  * desiredCapacity - how large should we like to grow the vector to - based on 2x requiredVectorLength.

    unsigned length = storage->m_length;
    unsigned usedVectorLength = min(m_vectorLength, length);
    ASSERT(usedVectorLength <= MAX_STORAGE_VECTOR_LENGTH);
    // Check that required vector length is possible, in an overflow-safe fashion.
    if (count > MAX_STORAGE_VECTOR_LENGTH - usedVectorLength)
        return false;
    unsigned requiredVectorLength = usedVectorLength + count;
    ASSERT(requiredVectorLength <= MAX_STORAGE_VECTOR_LENGTH);
    // The sum of m_vectorLength and m_indexBias will never exceed MAX_STORAGE_VECTOR_LENGTH.
    ASSERT(m_vectorLength <= MAX_STORAGE_VECTOR_LENGTH && (MAX_STORAGE_VECTOR_LENGTH - m_vectorLength) >= m_indexBias);
    unsigned currentCapacity = m_vectorLength + m_indexBias;
    // The calculation of desiredCapacity won't overflow, due to the range of MAX_STORAGE_VECTOR_LENGTH.
    unsigned desiredCapacity = min(MAX_STORAGE_VECTOR_LENGTH, max(BASE_VECTOR_LEN, requiredVectorLength) << 1);

    // Step 2:
    // We're either going to choose to allocate a new ArrayStorage, or we're going to reuse the existing on.

    void* newAllocBase = 0;
    unsigned newStorageCapacity;
    // If the current storage array is sufficiently large (but not too large!) then just keep using it.
    if (currentCapacity > desiredCapacity && isDenseEnoughForVector(currentCapacity, requiredVectorLength)) {
        newAllocBase = storage->m_allocBase;
        newStorageCapacity = currentCapacity;
    } else {
        if (!globalData.heap.tryAllocateStorage(storageSize(desiredCapacity), &newAllocBase))
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
    if (length < m_vectorLength) {
        // Atomic decay, + the post-capacity cannot be greater than what is available.
        postCapacity = min((m_vectorLength - length) >> 1, newStorageCapacity - requiredVectorLength);
        // If we're moving contents within the same allocation, the post-capacity is being reduced.
        ASSERT(newAllocBase != storage->m_allocBase || postCapacity < m_vectorLength - length);
    }

    m_vectorLength = requiredVectorLength + postCapacity;
    m_indexBias = newStorageCapacity - m_vectorLength;
    m_storage = reinterpret_cast_ptr<ArrayStorage*>(reinterpret_cast<WriteBarrier<Unknown>*>(newAllocBase) + m_indexBias);

    // Step 4:
    // Copy array data / header into their new locations, clear post-capacity & free any old allocation.

    // If this is being moved within the existing buffer of memory, we are always shifting data
    // to the right (since count > m_indexBias). As such this memmove cannot trample the header.
    memmove(m_storage->m_vector + count, storage->m_vector, sizeof(WriteBarrier<Unknown>) * usedVectorLength);
    memmove(m_storage, storage, storageSize(0));

    // Are we copying into a new allocation?
    if (newAllocBase != m_storage->m_allocBase) {
        // Free the old allocation, update m_allocBase.
        m_storage->m_allocBase = newAllocBase;
    }

    return true;
}

bool JSArray::setLength(ExecState* exec, unsigned newLength, bool throwException)
{
    checkConsistency();

    ArrayStorage* storage = m_storage;
    unsigned length = storage->m_length;

    // If the length is read only then we enter sparse mode, so should enter the following 'if'.
    ASSERT(isLengthWritable() || m_sparseValueMap);

    if (SparseArrayValueMap* map = m_sparseValueMap) {
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
                        storage->m_length = index + 1;
                        return reject(exec, throwException, "Unable to delete property.");
                    }
                    map->remove(it);
                }
            } else {
                for (unsigned i = 0; i < keys.size(); ++i)
                    map->remove(keys[i]);
                if (map->isEmpty())
                    deallocateSparseMap();
            }
        }
    }

    if (newLength < length) {
        // Delete properties from the vector.
        unsigned usedVectorLength = min(length, m_vectorLength);
        for (unsigned i = newLength; i < usedVectorLength; ++i) {
            WriteBarrier<Unknown>& valueSlot = storage->m_vector[i];
            bool hadValue = valueSlot;
            valueSlot.clear();
            storage->m_numValuesInVector -= hadValue;
        }
    }

    storage->m_length = newLength;

    checkConsistency();
    return true;
}

JSValue JSArray::pop(ExecState* exec)
{
    checkConsistency();
    ArrayStorage* storage = m_storage;
    
    unsigned length = storage->m_length;
    if (!length) {
        if (!isLengthWritable())
            throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
        return jsUndefined();
    }

    unsigned index = length - 1;
    if (index < m_vectorLength) {
        WriteBarrier<Unknown>& valueSlot = storage->m_vector[index];
        if (valueSlot) {
            --storage->m_numValuesInVector;
            JSValue element = valueSlot.get();
            valueSlot.clear();
            
            ASSERT(isLengthWritable());
            storage->m_length = index;
            checkConsistency();
            return element;
        }
    }

    // Let element be the result of calling the [[Get]] internal method of O with argument indx.
    JSValue element = get(exec, index);
    if (exec->hadException())
        return jsUndefined();
    // Call the [[Delete]] internal method of O with arguments indx and true.
    deletePropertyByIndex(this, exec, index);
    // Call the [[Put]] internal method of O with arguments "length", indx, and true.
    setLength(exec, index, true);
    // Return element.
    checkConsistency();
    return element;
}

// Push & putIndex are almost identical, with two small differences.
//  - we always are writing beyond the current array bounds, so it is always necessary to update m_length & m_numValuesInVector.
//  - pushing to an array of length 2^32-1 stores the property, but throws a range error.
void JSArray::push(ExecState* exec, JSValue value)
{
    checkConsistency();
    ArrayStorage* storage = m_storage;

    // Fast case - push within vector, always update m_length & m_numValuesInVector.
    unsigned length = storage->m_length;
    if (length < m_vectorLength) {
        storage->m_vector[length].set(exec->globalData(), this, value);
        storage->m_length = length + 1;
        ++storage->m_numValuesInVector;
        checkConsistency();
        return;
    }

    // Pushing to an array of length 2^32-1 stores the property, but throws a range error.
    if (UNLIKELY(storage->m_length == 0xFFFFFFFFu)) {
        methodTable()->putByIndex(this, exec, storage->m_length, value, true);
        // Per ES5.1 15.4.4.7 step 6 & 15.4.5.1 step 3.d.
        if (!exec->hadException())
            throwError(exec, createRangeError(exec, "Invalid array length"));
        return;
    }

    // Handled the same as putIndex.
    putByIndexBeyondVectorLength(exec, storage->m_length, value, true);
    checkConsistency();
}

bool JSArray::shiftCount(ExecState*, unsigned count)
{
    ASSERT(count > 0);
    
    ArrayStorage* storage = m_storage;
    
    unsigned oldLength = storage->m_length;
    ASSERT(count <= oldLength);
    
    // If the array contains holes or is otherwise in an abnormal state,
    // use the generic algorithm in ArrayPrototype.
    if (oldLength != storage->m_numValuesInVector || inSparseMode())
        return false;

    if (!oldLength)
        return true;
    
    storage->m_numValuesInVector -= count;
    storage->m_length -= count;
    
    if (m_vectorLength) {
        count = min(m_vectorLength, (unsigned)count);
        
        m_vectorLength -= count;
        
        if (m_vectorLength) {
            char* newBaseStorage = reinterpret_cast<char*>(storage) + count * sizeof(WriteBarrier<Unknown>);
            memmove(newBaseStorage, storage, storageSize(0));
            m_storage = reinterpret_cast_ptr<ArrayStorage*>(newBaseStorage);

            m_indexBias += count;
        }
    }
    return true;
}

// Returns true if the unshift can be handled, false to fallback.    
bool JSArray::unshiftCount(ExecState* exec, unsigned count)
{
    ArrayStorage* storage = m_storage;
    unsigned length = storage->m_length;

    // If the array contains holes or is otherwise in an abnormal state,
    // use the generic algorithm in ArrayPrototype.
    if (length != storage->m_numValuesInVector || inSparseMode())
        return false;

    if (m_indexBias >= count) {
        m_indexBias -= count;
        char* newBaseStorage = reinterpret_cast<char*>(storage) - count * sizeof(WriteBarrier<Unknown>);
        memmove(newBaseStorage, storage, storageSize(0));
        m_storage = reinterpret_cast_ptr<ArrayStorage*>(newBaseStorage);
        m_vectorLength += count;
    } else if (!unshiftCountSlowCase(exec->globalData(), count)) {
        throwOutOfMemoryError(exec);
        return true;
    }

    WriteBarrier<Unknown>* vector = m_storage->m_vector;
    for (unsigned i = 0; i < count; i++)
        vector[i].clear();
    return true;
}

void JSArray::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSArray* thisObject = jsCast<JSArray*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    JSNonFinalObject::visitChildren(thisObject, visitor);

    if (thisObject->m_storage) {
        ArrayStorage* storage = thisObject->m_storage;
        void* baseStorage = storage->m_allocBase;

        visitor.copyAndAppend(reinterpret_cast<void**>(&baseStorage), storageSize(thisObject->m_vectorLength + thisObject->m_indexBias), storage->m_vector->slot(), thisObject->m_vectorLength);

        if (baseStorage != thisObject->m_storage->m_allocBase) {
            thisObject->m_storage = reinterpret_cast_ptr<ArrayStorage*>(static_cast<char*>(baseStorage) + sizeof(JSValue) * thisObject->m_indexBias);
            thisObject->m_storage->m_allocBase = baseStorage;
            ASSERT(thisObject->m_storage->m_allocBase);
        }
    }

    if (SparseArrayValueMap* map = thisObject->m_sparseValueMap)
        map->visitChildren(visitor);
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
    ASSERT(!inSparseMode());

    ArrayStorage* storage = m_storage;

    unsigned lengthNotIncludingUndefined = compactForSorting();
    ASSERT(!m_sparseValueMap);

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

    checkConsistency(SortConsistencyCheck);
}

void JSArray::sort(ExecState* exec)
{
    ASSERT(!inSparseMode());

    unsigned lengthNotIncludingUndefined = compactForSorting();
    ASSERT(!m_sparseValueMap);

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
        JSValue value = m_storage->m_vector[i].get();
        ASSERT(!value.isUndefined());
        values[i].first = value;
        isSortingPrimitiveValues = isSortingPrimitiveValues && value.isPrimitive();
    }

    // FIXME: The following loop continues to call toString on subsequent values even after
    // a toString call raises an exception.

    for (size_t i = 0; i < lengthNotIncludingUndefined; i++)
        values[i].second = values[i].first.toUStringInline(exec);

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
    if (m_vectorLength < lengthNotIncludingUndefined)
        increaseVectorLength(exec->globalData(), lengthNotIncludingUndefined);
    if (m_storage->m_length < lengthNotIncludingUndefined)
        m_storage->m_length = lengthNotIncludingUndefined;

    JSGlobalData& globalData = exec->globalData();
    for (size_t i = 0; i < lengthNotIncludingUndefined; i++)
        m_storage->m_vector[i].set(globalData, this, values[i].first);

    Heap::heap(this)->popTempSortVector(&values);
    
    checkConsistency(SortConsistencyCheck);
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
    ASSERT(!inSparseMode());

    checkConsistency();

    // FIXME: This ignores exceptions raised in the compare function or in toNumber.

    // The maximum tree depth is compiled in - but the caller is clearly up to no good
    // if a larger array is passed.
    ASSERT(m_storage->m_length <= static_cast<unsigned>(std::numeric_limits<int>::max()));
    if (m_storage->m_length > static_cast<unsigned>(std::numeric_limits<int>::max()))
        return;

    unsigned usedVectorLength = min(m_storage->m_length, m_vectorLength);
    unsigned nodeCount = usedVectorLength;

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
        if (numDefined > m_vectorLength)
            break;
        JSValue v = m_storage->m_vector[numDefined].get();
        if (!v || v.isUndefined())
            break;
        tree.abstractor().m_nodes[numDefined].value = v;
        tree.insert(numDefined);
    }
    for (unsigned i = numDefined; i < usedVectorLength; ++i) {
        if (i > m_vectorLength)
            break;
        JSValue v = m_storage->m_vector[i].get();
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

    ASSERT(!m_sparseValueMap);

    // The array size may have changed.  Figure out the new bounds.
    unsigned newestUsedVectorLength = min(m_storage->m_length, m_vectorLength);
    
    unsigned elementsToExtractThreshold = min(min(newestUsedVectorLength, numDefined), static_cast<unsigned>(tree.abstractor().m_nodes.size()));
    unsigned undefinedElementsThreshold = min(newestUsedVectorLength, newUsedVectorLength);
    unsigned clearElementsThreshold = min(newestUsedVectorLength, usedVectorLength);

    // Copy the values back into m_storage.
    AVLTree<AVLTreeAbstractorForArrayCompare, 44>::Iterator iter;
    iter.start_iter_least(tree);
    JSGlobalData& globalData = exec->globalData();
    for (unsigned i = 0; i < elementsToExtractThreshold; ++i) {
        m_storage->m_vector[i].set(globalData, this, tree.abstractor().m_nodes[*iter].value);
        ++iter;
    }

    // Put undefined values back in.
    for (unsigned i = elementsToExtractThreshold; i < undefinedElementsThreshold; ++i)
        m_storage->m_vector[i].setUndefined();

    // Ensure that unused values in the vector are zeroed out.
    for (unsigned i = undefinedElementsThreshold; i < clearElementsThreshold; ++i)
        m_storage->m_vector[i].clear();

    m_storage->m_numValuesInVector = undefinedElementsThreshold;

    checkConsistency(SortConsistencyCheck);
}

void JSArray::fillArgList(ExecState* exec, MarkedArgumentBuffer& args)
{
    ArrayStorage* storage = m_storage;

    WriteBarrier<Unknown>* vector = storage->m_vector;
    unsigned vectorEnd = min(storage->m_length, m_vectorLength);
    unsigned i = 0;
    for (; i < vectorEnd; ++i) {
        WriteBarrier<Unknown>& v = vector[i];
        if (!v)
            break;
        args.append(v.get());
    }

    for (; i < storage->m_length; ++i)
        args.append(get(exec, i));
}

void JSArray::copyToArguments(ExecState* exec, CallFrame* callFrame, uint32_t length)
{
    ASSERT(length == this->length());
    UNUSED_PARAM(length);
    unsigned i = 0;
    WriteBarrier<Unknown>* vector = m_storage->m_vector;
    unsigned vectorEnd = min(length, m_vectorLength);
    for (; i < vectorEnd; ++i) {
        WriteBarrier<Unknown>& v = vector[i];
        if (!v)
            break;
        callFrame->setArgument(i, v.get());
    }

    for (; i < length; ++i)
        callFrame->setArgument(i, get(exec, i));
}

unsigned JSArray::compactForSorting()
{
    ASSERT(!inSparseMode());

    checkConsistency();

    ArrayStorage* storage = m_storage;

    unsigned usedVectorLength = min(storage->m_length, m_vectorLength);

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

    ASSERT(!m_sparseValueMap);

    for (unsigned i = numDefined; i < newUsedVectorLength; ++i)
        storage->m_vector[i].setUndefined();
    for (unsigned i = newUsedVectorLength; i < usedVectorLength; ++i)
        storage->m_vector[i].clear();

    storage->m_numValuesInVector = newUsedVectorLength;

    checkConsistency(SortConsistencyCheck);

    return numDefined;
}

#if CHECK_ARRAY_CONSISTENCY

void JSArray::checkConsistency(ConsistencyCheckType type)
{
    ArrayStorage* storage = m_storage;

    ASSERT(!storage->m_inCompactInitialization);

    ASSERT(storage);
    if (type == SortConsistencyCheck)
        ASSERT(!m_sparseValueMap);

    unsigned numValuesInVector = 0;
    for (unsigned i = 0; i < m_vectorLength; ++i) {
        if (JSValue value = storage->m_vector[i].get()) {
            ASSERT(i < storage->m_length);
            if (type != DestructorConsistencyCheck)
                value.isUndefined(); // Likely to crash if the object was deallocated.
            ++numValuesInVector;
        } else {
            if (type == SortConsistencyCheck)
                ASSERT(i >= storage->m_numValuesInVector);
        }
    }
    ASSERT(numValuesInVector == storage->m_numValuesInVector);
    ASSERT(numValuesInVector <= storage->m_length);

    if (m_sparseValueMap) {
        SparseArrayValueMap::const_iterator end = m_sparseValueMap->end();
        for (SparseArrayValueMap::const_iterator it = m_sparseValueMap->begin(); it != end; ++it) {
            unsigned index = it->first;
            ASSERT(index < storage->m_length);
            ASSERT(index >= m_vectorLength);
            ASSERT(index <= MAX_ARRAY_INDEX);
            ASSERT(it->second);
            if (type != DestructorConsistencyCheck)
                it->second.getNonSparseMode().isUndefined(); // Likely to crash if the object was deallocated.
        }
    }
}

#endif

} // namespace JSC
