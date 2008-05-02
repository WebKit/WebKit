/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All rights reserved.
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
#include "array_instance.h"

#include "PropertyNameArray.h"
#include <wtf/Assertions.h>

using std::min;

namespace KJS {

typedef HashMap<unsigned, JSValue*> SparseArrayValueMap;

struct ArrayStorage {
    unsigned m_numValuesInVector;
    SparseArrayValueMap* m_sparseValueMap;
    JSValue* m_vector[1];
};

// 0xFFFFFFFF is a bit weird -- is not an array index even though it's an integer
static const unsigned maxArrayIndex = 0xFFFFFFFEU;

// Our policy for when to use a vector and when to use a sparse map.
// For all array indices under sparseArrayCutoff, we always use a vector.
// When indices greater than sparseArrayCutoff are involved, we use a vector
// as long as it is 1/8 full. If more sparse than that, we use a map.
static const unsigned sparseArrayCutoff = 10000;
static const unsigned minDensityMultiplier = 8;

static const unsigned copyingSortCutoff = 50000;

const ClassInfo ArrayInstance::info = {"Array", 0, 0, 0};

static inline size_t storageSize(unsigned vectorLength)
{
    return sizeof(ArrayStorage) - sizeof(JSValue*) + vectorLength * sizeof(JSValue*);
}

static inline unsigned increasedVectorLength(unsigned newLength)
{
    return (newLength * 3 + 1) / 2;
}

static inline bool isDenseEnoughForVector(unsigned length, unsigned numValues)
{
    return length / minDensityMultiplier <= numValues;
}

ArrayInstance::ArrayInstance(JSObject* prototype, unsigned initialLength)
    : JSObject(prototype)
{
    unsigned initialCapacity = min(initialLength, sparseArrayCutoff);

    m_length = initialLength;
    m_vectorLength = initialCapacity;
    m_storage = static_cast<ArrayStorage*>(fastZeroedMalloc(storageSize(initialCapacity)));

    Heap::heap(this)->reportExtraMemoryCost(initialCapacity * sizeof(JSValue*));
}

ArrayInstance::ArrayInstance(JSObject* prototype, const List& list)
    : JSObject(prototype)
{
    unsigned length = list.size();

    m_length = length;
    m_vectorLength = length;

    ArrayStorage* storage = static_cast<ArrayStorage*>(fastMalloc(storageSize(length)));

    storage->m_numValuesInVector = length;
    storage->m_sparseValueMap = 0;

    size_t i = 0;
    List::const_iterator end = list.end();
    for (List::const_iterator it = list.begin(); it != end; ++it, ++i)
        storage->m_vector[i] = *it;

    m_storage = storage;

    // When the array is created non-empty, its cells are filled, so it's really no worse than
    // a property map. Therefore don't report extra memory cost.
}

ArrayInstance::~ArrayInstance()
{
    delete m_storage->m_sparseValueMap;
    fastFree(m_storage);
}

JSValue* ArrayInstance::getItem(unsigned i) const
{
    ASSERT(i <= maxArrayIndex);

    ArrayStorage* storage = m_storage;

    if (i < m_vectorLength) {
        JSValue* value = storage->m_vector[i];
        return value ? value : jsUndefined();
    }

    SparseArrayValueMap* map = storage->m_sparseValueMap;
    if (!map)
        return jsUndefined();

    JSValue* value = map->get(i);
    return value ? value : jsUndefined();
}

JSValue* ArrayInstance::lengthGetter(ExecState* exec, JSObject*, const Identifier&, const PropertySlot& slot)
{
    return jsNumber(exec, static_cast<ArrayInstance*>(slot.slotBase())->m_length);
}

ALWAYS_INLINE bool ArrayInstance::inlineGetOwnPropertySlot(ExecState* exec, unsigned i, PropertySlot& slot)
{
    ArrayStorage* storage = m_storage;

    if (i >= m_length) {
        if (i > maxArrayIndex)
            return getOwnPropertySlot(exec, Identifier::from(i), slot);
        return false;
    }

    if (i < m_vectorLength) {
        JSValue*& valueSlot = storage->m_vector[i];
        if (valueSlot) {
            slot.setValueSlot(this, &valueSlot);
            return true;
        }
    } else if (SparseArrayValueMap* map = storage->m_sparseValueMap) {
        if (i >= sparseArrayCutoff) {
            SparseArrayValueMap::iterator it = map->find(i);
            if (it != map->end()) {
                slot.setValueSlot(this, &it->second);
                return true;
            }
        }
    }

    return false;
}

bool ArrayInstance::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (propertyName == exec->propertyNames().length) {
        slot.setCustom(this, lengthGetter);
        return true;
    }

    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(&isArrayIndex);
    if (isArrayIndex)
        return inlineGetOwnPropertySlot(exec, i, slot);

    return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

bool ArrayInstance::getOwnPropertySlot(ExecState* exec, unsigned i, PropertySlot& slot)
{
    return inlineGetOwnPropertySlot(exec, i, slot);
}

// ECMA 15.4.5.1
void ArrayInstance::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(&isArrayIndex);
    if (isArrayIndex) {
        put(exec, i, value);
        return;
    }

    if (propertyName == exec->propertyNames().length) {
        unsigned newLength = value->toUInt32(exec);
        if (value->toNumber(exec) != static_cast<double>(newLength)) {
            throwError(exec, RangeError, "Invalid array length.");
            return;
        }
        setLength(newLength);
        return;
    }

    JSObject::put(exec, propertyName, value);
}

void ArrayInstance::put(ExecState* exec, unsigned i, JSValue* value)
{
    unsigned length = m_length;
    if (i >= length) {
        if (i > maxArrayIndex) {
            put(exec, Identifier::from(i), value);
            return;
        }
        length = i + 1;
        m_length = length;
    }

    ArrayStorage* storage = m_storage;

    if (i < m_vectorLength) {
        JSValue*& valueSlot = storage->m_vector[i];
        storage->m_numValuesInVector += !valueSlot;
        valueSlot = value;
        return;
    }

    if (i < sparseArrayCutoff) {
        increaseVectorLength(i + 1);
        storage = m_storage;
        ++storage->m_numValuesInVector;
        storage->m_vector[i] = value;
        return;
    }

    SparseArrayValueMap* map = storage->m_sparseValueMap;
    if (!map || map->isEmpty()) {
        if (isDenseEnoughForVector(i + 1, storage->m_numValuesInVector + 1)) {
            increaseVectorLength(i + 1);
            storage = m_storage;
            ++storage->m_numValuesInVector;
            storage->m_vector[i] = value;
            return;
        }
        if (!map) {
            map = new SparseArrayValueMap;
            storage->m_sparseValueMap = map;
        }
        map->set(i, value);
        return;
    }

    unsigned newNumValuesInVector = storage->m_numValuesInVector + 1;
    if (!isDenseEnoughForVector(i + 1, newNumValuesInVector)) {
        map->set(i, value);
        return;
    }

    unsigned newVectorLength = increasedVectorLength(i + 1);
    for (unsigned j = m_vectorLength; j < newVectorLength; ++j)
        newNumValuesInVector += map->contains(j);
    newNumValuesInVector -= map->contains(i);
    if (isDenseEnoughForVector(newVectorLength, newNumValuesInVector)) {
        unsigned proposedNewNumValuesInVector = newNumValuesInVector;
        while (true) {
            unsigned proposedNewVectorLength = increasedVectorLength(newVectorLength + 1);
            for (unsigned j = newVectorLength; j < proposedNewVectorLength; ++j)
                proposedNewNumValuesInVector += map->contains(j);
            if (!isDenseEnoughForVector(proposedNewVectorLength, proposedNewNumValuesInVector))
                break;
            newVectorLength = proposedNewVectorLength;
            newNumValuesInVector = proposedNewNumValuesInVector;
        }
    }

    storage = static_cast<ArrayStorage*>(fastRealloc(storage, storageSize(newVectorLength)));

    unsigned vectorLength = m_vectorLength;
    if (newNumValuesInVector == storage->m_numValuesInVector + 1) {
        for (unsigned j = vectorLength; j < newVectorLength; ++j)
            storage->m_vector[j] = 0;
        map->remove(i);
    } else {
        for (unsigned j = vectorLength; j < newVectorLength; ++j)
            storage->m_vector[j] = map->take(j);
    }

    storage->m_vector[i] = value;

    m_vectorLength = newVectorLength;
    storage->m_numValuesInVector = newNumValuesInVector;
}

bool ArrayInstance::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(&isArrayIndex);
    if (isArrayIndex)
        return deleteProperty(exec, i);

    if (propertyName == exec->propertyNames().length)
        return false;

    return JSObject::deleteProperty(exec, propertyName);
}

bool ArrayInstance::deleteProperty(ExecState* exec, unsigned i)
{
    ArrayStorage* storage = m_storage;

    if (i < m_vectorLength) {
        JSValue*& valueSlot = storage->m_vector[i];
        bool hadValue = valueSlot;
        valueSlot = 0;
        storage->m_numValuesInVector -= hadValue;
        return hadValue;
    }

    if (SparseArrayValueMap* map = storage->m_sparseValueMap) {
        if (i >= sparseArrayCutoff) {
            SparseArrayValueMap::iterator it = map->find(i);
            if (it != map->end()) {
                map->remove(it);
                return true;
            }
        }
    }

    if (i > maxArrayIndex)
        return deleteProperty(exec, Identifier::from(i));

    return false;
}

void ArrayInstance::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    // FIXME: Filling PropertyNameArray with an identifier for every integer
    // is incredibly inefficient for large arrays. We need a different approach.

    ArrayStorage* storage = m_storage;

    unsigned usedVectorLength = min(m_length, m_vectorLength);
    for (unsigned i = 0; i < usedVectorLength; ++i) {
        if (storage->m_vector[i])
            propertyNames.add(Identifier::from(i));
    }

    if (SparseArrayValueMap* map = storage->m_sparseValueMap) {
        SparseArrayValueMap::iterator end = map->end();
        for (SparseArrayValueMap::iterator it = map->begin(); it != end; ++it)
            propertyNames.add(Identifier::from(it->first));
    }

    JSObject::getPropertyNames(exec, propertyNames);
}

void ArrayInstance::increaseVectorLength(unsigned newLength)
{
    ArrayStorage* storage = m_storage;

    unsigned vectorLength = m_vectorLength;
    ASSERT(newLength > vectorLength);
    unsigned newVectorLength = increasedVectorLength(newLength);

    storage = static_cast<ArrayStorage*>(fastRealloc(storage, storageSize(newVectorLength)));
    m_vectorLength = newVectorLength;

    for (unsigned i = vectorLength; i < newVectorLength; ++i)
        storage->m_vector[i] = 0;

    m_storage = storage;
}

void ArrayInstance::setLength(unsigned newLength)
{
    ArrayStorage* storage = m_storage;

    unsigned length = m_length;

    if (newLength < length) {
        unsigned usedVectorLength = min(length, m_vectorLength);
        for (unsigned i = newLength; i < usedVectorLength; ++i) {
            JSValue*& valueSlot = storage->m_vector[i];
            bool hadValue = valueSlot;
            valueSlot = 0;
            storage->m_numValuesInVector -= hadValue;
        }

        if (SparseArrayValueMap* map = storage->m_sparseValueMap) {
            SparseArrayValueMap copy = *map;
            SparseArrayValueMap::iterator end = copy.end();
            for (SparseArrayValueMap::iterator it = copy.begin(); it != end; ++it) {
                if (it->first >= newLength)
                    map->remove(it->first);
            }
            if (map->isEmpty()) {
                delete map;
                storage->m_sparseValueMap = 0;
            }
        }
    }

    m_length = newLength;
}

void ArrayInstance::mark()
{
    JSObject::mark();

    ArrayStorage* storage = m_storage;

    unsigned usedVectorLength = min(m_length, m_vectorLength);
    for (unsigned i = 0; i < usedVectorLength; ++i) {
        JSValue* value = storage->m_vector[i];
        if (value && !value->marked())
            value->mark();
    }

    if (SparseArrayValueMap* map = storage->m_sparseValueMap) {
        SparseArrayValueMap::iterator end = map->end();
        for (SparseArrayValueMap::iterator it = map->begin(); it != end; ++it) {
            JSValue* value = it->second;
            if (!value->marked())
                value->mark();
        }
    }
}

static int compareByStringPairForQSort(const void* a, const void* b)
{
    const std::pair<JSValue*, UString>* va = static_cast<const std::pair<JSValue*, UString>*>(a);
    const std::pair<JSValue*, UString>* vb = static_cast<const std::pair<JSValue*, UString>*>(b);
    return compare(va->second, vb->second);
}

class ArraySortComparator {
public:
    ArraySortComparator(ExecState* exec) : m_exec(exec) {}

    bool operator()(JSValue* va, JSValue* vb)
    {
        ASSERT(!va->isUndefined());
        ASSERT(!vb->isUndefined());
        return compare(va->toString(m_exec), vb->toString(m_exec)) < 0;
    }

private:
    ExecState* m_exec;
};

void ArrayInstance::sort(ExecState* exec)
{
    unsigned lengthNotIncludingUndefined = compactForSorting();

    if (lengthNotIncludingUndefined < copyingSortCutoff) {
        // Converting JavaScript values to strings can be expensive, so we do it once up front and sort based on that.
        // This is a considerable improvement over doing it twice per comparison, though it requires a large temporary
        // buffer.  For large arrays, we fall back to a qsort on the JavaScriptValues to avoid creating copies.

        Vector<std::pair<JSValue*, UString> > values(lengthNotIncludingUndefined);
        for (size_t i = 0; i < lengthNotIncludingUndefined; i++) {
            JSValue* value = m_storage->m_vector[i];
            ASSERT(!value->isUndefined());
            values[i].first = value;
            values[i].second = value->toString(exec);
        }

        // FIXME: Since we sort by string value, a fast algorithm might be to use a radix sort. That would be O(N) rather
        // than O(N log N).

#if HAVE(MERGESORT)
        mergesort(values.begin(), values.size(), sizeof(std::pair<JSValue*, UString>), compareByStringPairForQSort);
#else
        qsort(values.begin(), values.size(), sizeof(std::pair<JSValue*, UString>), compareByStringPairForQSort);
#endif
        for (size_t i = 0; i < lengthNotIncludingUndefined; i++)
            m_storage->m_vector[i] = values[i].first;
        return;
    }

    std::sort(m_storage->m_vector, m_storage->m_vector + lengthNotIncludingUndefined, ArraySortComparator(exec));
}

struct CompareWithCompareFunctionArguments {
    CompareWithCompareFunctionArguments(ExecState* exec, JSObject* cf)
        : exec(exec)
        , compareFunction(cf)
        , globalThisValue(exec->globalThisValue())
    {
    }

    bool operator()(JSValue* va, JSValue* vb)
    {
        ASSERT(!va->isUndefined());
        ASSERT(!vb->isUndefined());

        List arguments;
        arguments.append(va);
        arguments.append(vb);
        double compareResult = compareFunction->call(exec, globalThisValue, arguments)->toNumber(exec);
        return compareResult < 0;
    }

    ExecState* exec;
    JSObject* compareFunction;
    JSObject* globalThisValue;
};

void ArrayInstance::sort(ExecState* exec, JSObject* compareFunction)
{
    size_t lengthNotIncludingUndefined = compactForSorting();

    // FIXME: A tree sort using a perfectly balanced tree (e.g. an AVL tree) could do an even
    // better job of minimizing compares.

    std::sort(m_storage->m_vector, m_storage->m_vector + lengthNotIncludingUndefined, CompareWithCompareFunctionArguments(exec, compareFunction));
}

unsigned ArrayInstance::compactForSorting()
{
    ArrayStorage* storage = m_storage;

    unsigned usedVectorLength = min(m_length, m_vectorLength);

    unsigned numDefined = 0;
    unsigned numUndefined = 0;

    for (; numDefined < usedVectorLength; ++numDefined) {
        JSValue* v = storage->m_vector[numDefined];
        if (!v || v->isUndefined())
            break;
    }
    for (unsigned i = numDefined; i < usedVectorLength; ++i) {
        if (JSValue* v = storage->m_vector[i]) {
            if (v->isUndefined())
                ++numUndefined;
            else
                storage->m_vector[numDefined++] = v;
        }
    }

    unsigned newUsedVectorLength = numDefined + numUndefined;

    if (SparseArrayValueMap* map = storage->m_sparseValueMap) {
        newUsedVectorLength += map->size();
        if (newUsedVectorLength > m_vectorLength) {
            increaseVectorLength(newUsedVectorLength);
            storage = m_storage;
        }

        SparseArrayValueMap::iterator end = map->end();
        for (SparseArrayValueMap::iterator it = map->begin(); it != end; ++it)
            storage->m_vector[numDefined++] = it->second;

        delete map;
        storage->m_sparseValueMap = 0;
    }

    for (unsigned i = numDefined; i < newUsedVectorLength; ++i)
        storage->m_vector[i] = jsUndefined();
    for (unsigned i = newUsedVectorLength; i < usedVectorLength; ++i)
        storage->m_vector[i] = 0;

    return numDefined;
}

}
