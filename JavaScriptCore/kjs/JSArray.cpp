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
#include "JSArray.h"

#include "ArrayPrototype.h"
#include "PropertyNameArray.h"
#include <wtf/AVLTree.h>
#include <wtf/Assertions.h>

#define CHECK_ARRAY_CONSISTENCY 0

using namespace std;

namespace KJS {

typedef HashMap<unsigned, JSValue*> SparseArrayValueMap;

struct ArrayStorage {
    unsigned m_numValuesInVector;
    SparseArrayValueMap* m_sparseValueMap;
    void* lazyCreationData; // An JSArray subclass can use this to fill the vector lazily.
    JSValue* m_vector[1];
};

// 0xFFFFFFFF is a bit weird -- is not an array index even though it's an integer.
static const unsigned maxArrayIndex = 0xFFFFFFFEU;

// Our policy for when to use a vector and when to use a sparse map.
// For all array indices under sparseArrayCutoff, we always use a vector.
// When indices greater than sparseArrayCutoff are involved, we use a vector
// as long as it is 1/8 full. If more sparse than that, we use a map.
// This value has to be a macro to be used in max() and min() without introducing
// a PIC branch in Mach-O binaries, see <rdar://problem/5971391>.
#define sparseArrayCutoff 10000U
static const unsigned minDensityMultiplier = 8;

const ClassInfo JSArray::info = {"Array", 0, 0, 0};

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

#if !CHECK_ARRAY_CONSISTENCY

inline void JSArray::checkConsistency(ConsistencyCheckType)
{
}

#endif

JSArray::JSArray(JSObject* prototype, unsigned initialLength)
    : JSObject(prototype)
{
    unsigned initialCapacity = min(initialLength, sparseArrayCutoff);

    m_length = initialLength;
    m_vectorLength = initialCapacity;
    m_storage = static_cast<ArrayStorage*>(fastZeroedMalloc(storageSize(initialCapacity)));

    Heap::heap(this)->reportExtraMemoryCost(initialCapacity * sizeof(JSValue*));

    checkConsistency();
}

JSArray::JSArray(JSObject* prototype, const ArgList& list)
    : JSObject(prototype)
{
    unsigned length = list.size();

    m_length = length;
    m_vectorLength = length;

    ArrayStorage* storage = static_cast<ArrayStorage*>(fastMalloc(storageSize(length)));

    storage->m_numValuesInVector = length;
    storage->m_sparseValueMap = 0;

    size_t i = 0;
    ArgList::const_iterator end = list.end();
    for (ArgList::const_iterator it = list.begin(); it != end; ++it, ++i)
        storage->m_vector[i] = *it;

    m_storage = storage;

    // When the array is created non-empty, its cells are filled, so it's really no worse than
    // a property map. Therefore don't report extra memory cost.

    checkConsistency();
}

JSArray::~JSArray()
{
    checkConsistency(DestructorConsistencyCheck);

    delete m_storage->m_sparseValueMap;
    fastFree(m_storage);
}

JSValue* JSArray::getItem(unsigned i) const
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

JSValue* JSArray::lengthGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return jsNumber(exec, static_cast<JSArray*>(slot.slotBase())->m_length);
}

ALWAYS_INLINE bool JSArray::inlineGetOwnPropertySlot(ExecState* exec, unsigned i, PropertySlot& slot)
{
    ArrayStorage* storage = m_storage;

    if (UNLIKELY(i >= m_length)) {
        if (i > maxArrayIndex)
            return getOwnPropertySlot(exec, Identifier::from(exec, i), slot);
        return false;
    }

    if (i < m_vectorLength) {
        JSValue*& valueSlot = storage->m_vector[i];
        if (valueSlot) {
            slot.setValueSlot(&valueSlot);
            return true;
        }
    } else if (SparseArrayValueMap* map = storage->m_sparseValueMap) {
        if (i >= sparseArrayCutoff) {
            SparseArrayValueMap::iterator it = map->find(i);
            if (it != map->end()) {
                slot.setValueSlot(&it->second);
                return true;
            }
        }
    }

    return false;
}

bool JSArray::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
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

bool JSArray::getOwnPropertySlot(ExecState* exec, unsigned i, PropertySlot& slot)
{
    return inlineGetOwnPropertySlot(exec, i, slot);
}

// ECMA 15.4.5.1
void JSArray::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
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

void JSArray::put(ExecState* exec, unsigned i, JSValue* value)
{
    checkConsistency();

    unsigned length = m_length;
    if (i >= length) {
        if (i > maxArrayIndex) {
            put(exec, Identifier::from(exec, i), value);
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
        checkConsistency();
        return;
    }

    SparseArrayValueMap* map = storage->m_sparseValueMap;

    if (i >= sparseArrayCutoff) {
        // We miss some cases where we could compact the storage, such as a large array that is being filled from the end
        // (which will only be compacted as we reach indices that are less than cutoff) - but this makes the check much faster.
        if (!isDenseEnoughForVector(i + 1, storage->m_numValuesInVector + 1)) {
            if (!map) {
                map = new SparseArrayValueMap;
                storage->m_sparseValueMap = map;
            }
            map->set(i, value);
            return;
        }
    }

    // We have decided that we'll put the new item into the vector.
    // Fast case is when there is no sparse map, so we can increase the vector size without moving values from it.
    if (!map || map->isEmpty()) {
        increaseVectorLength(i + 1);
        storage = m_storage;
        ++storage->m_numValuesInVector;
        storage->m_vector[i] = value;
        checkConsistency();
        return;
    }

    // Decide how many values it would be best to move from the map.
    unsigned newNumValuesInVector = storage->m_numValuesInVector + 1;
    unsigned newVectorLength = increasedVectorLength(i + 1);
    for (unsigned j = max(m_vectorLength, sparseArrayCutoff); j < newVectorLength; ++j)
        newNumValuesInVector += map->contains(j);
    if (i >= sparseArrayCutoff)
        newNumValuesInVector -= map->contains(i);
    if (isDenseEnoughForVector(newVectorLength, newNumValuesInVector)) {
        unsigned proposedNewNumValuesInVector = newNumValuesInVector;
        while (true) {
            unsigned proposedNewVectorLength = increasedVectorLength(newVectorLength + 1);
            for (unsigned j = max(newVectorLength, sparseArrayCutoff); j < proposedNewVectorLength; ++j)
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
        if (i > sparseArrayCutoff)
            map->remove(i);
    } else {
        for (unsigned j = vectorLength; j < max(vectorLength, sparseArrayCutoff); ++j)
            storage->m_vector[j] = 0;
        for (unsigned j = max(vectorLength, sparseArrayCutoff); j < newVectorLength; ++j)
            storage->m_vector[j] = map->take(j);
    }

    storage->m_vector[i] = value;

    m_vectorLength = newVectorLength;
    storage->m_numValuesInVector = newNumValuesInVector;

    m_storage = storage;

    checkConsistency();
}

bool JSArray::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(&isArrayIndex);
    if (isArrayIndex)
        return deleteProperty(exec, i);

    if (propertyName == exec->propertyNames().length)
        return false;

    return JSObject::deleteProperty(exec, propertyName);
}

bool JSArray::deleteProperty(ExecState* exec, unsigned i)
{
    checkConsistency();

    ArrayStorage* storage = m_storage;

    if (i < m_vectorLength) {
        JSValue*& valueSlot = storage->m_vector[i];
        bool hadValue = valueSlot;
        valueSlot = 0;
        storage->m_numValuesInVector -= hadValue;
        checkConsistency();
        return hadValue;
    }

    if (SparseArrayValueMap* map = storage->m_sparseValueMap) {
        if (i >= sparseArrayCutoff) {
            SparseArrayValueMap::iterator it = map->find(i);
            if (it != map->end()) {
                map->remove(it);
                checkConsistency();
                return true;
            }
        }
    }

    checkConsistency();

    if (i > maxArrayIndex)
        return deleteProperty(exec, Identifier::from(exec, i));

    return false;
}

void JSArray::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    // FIXME: Filling PropertyNameArray with an identifier for every integer
    // is incredibly inefficient for large arrays. We need a different approach,
    // which almost certainly means a different structure for PropertyNameArray.

    ArrayStorage* storage = m_storage;

    unsigned usedVectorLength = min(m_length, m_vectorLength);
    for (unsigned i = 0; i < usedVectorLength; ++i) {
        if (storage->m_vector[i])
            propertyNames.add(Identifier::from(exec, i));
    }

    if (SparseArrayValueMap* map = storage->m_sparseValueMap) {
        SparseArrayValueMap::iterator end = map->end();
        for (SparseArrayValueMap::iterator it = map->begin(); it != end; ++it)
            propertyNames.add(Identifier::from(exec, it->first));
    }

    JSObject::getPropertyNames(exec, propertyNames);
}

bool JSArray::increaseVectorLength(unsigned newLength)
{
    // This function leaves the array in an internally inconsistent state, because it does not move any values from sparse value map
    // to the vector. Callers have to account for that, because they can do it more efficiently.

    ArrayStorage* storage = m_storage;

    unsigned vectorLength = m_vectorLength;
    ASSERT(newLength > vectorLength);
    unsigned newVectorLength = increasedVectorLength(newLength);

    storage = static_cast<ArrayStorage*>(fastRealloc(storage, storageSize(newVectorLength)));
    if (!storage)
        return false;

    m_vectorLength = newVectorLength;

    for (unsigned i = vectorLength; i < newVectorLength; ++i)
        storage->m_vector[i] = 0;

    m_storage = storage;
    return true;
}

void JSArray::setLength(unsigned newLength)
{
    checkConsistency();

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

    checkConsistency();
}

void JSArray::mark()
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

typedef std::pair<JSValue*, UString> ArrayQSortPair;

static int compareByStringPairForQSort(const void* a, const void* b)
{
    const ArrayQSortPair* va = static_cast<const ArrayQSortPair*>(a);
    const ArrayQSortPair* vb = static_cast<const ArrayQSortPair*>(b);
    return compare(va->second, vb->second);
}

void JSArray::sort(ExecState* exec)
{
    unsigned lengthNotIncludingUndefined = compactForSorting();
    if (m_storage->m_sparseValueMap) {
        exec->setException(Error::create(exec, GeneralError, "Out of memory"));
        return;
    }

    if (!lengthNotIncludingUndefined)
        return;

    // Converting JavaScript values to strings can be expensive, so we do it once up front and sort based on that.
    // This is a considerable improvement over doing it twice per comparison, though it requires a large temporary
    // buffer. Besides, this protects us from crashing if some objects have custom toString methods that return
    // random or otherwise changing results, effectively making compare function inconsistent.

    Vector<ArrayQSortPair> values(lengthNotIncludingUndefined);
    if (!values.begin()) {
        exec->setException(Error::create(exec, GeneralError, "Out of memory"));
        return;
    }

    for (size_t i = 0; i < lengthNotIncludingUndefined; i++) {
        JSValue* value = m_storage->m_vector[i];
        ASSERT(!value->isUndefined());
        values[i].first = value;
    }

    // FIXME: While calling these toString functions, the array could be mutated.
    // In that case, objects pointed to by values in this vector might get garbage-collected!

    // FIXME: The following loop continues to call toString on subsequent values even after
    // a toString call raises an exception.

    for (size_t i = 0; i < lengthNotIncludingUndefined; i++)
        values[i].second = values[i].first->toString(exec);

    if (exec->hadException())
        return;

    // FIXME: Since we sort by string value, a fast algorithm might be to use a radix sort. That would be O(N) rather
    // than O(N log N).

#if HAVE(MERGESORT)
    mergesort(values.begin(), values.size(), sizeof(ArrayQSortPair), compareByStringPairForQSort);
#else
    // FIXME: The qsort library function is likely to not be a stable sort.
    // ECMAScript-262 does not specify a stable sort, but in practice, browsers perform a stable sort.
    qsort(values.begin(), values.size(), sizeof(ArrayQSortPair), compareByStringPairForQSort);
#endif

    // FIXME: If the toString function changed the length of the array, this might be
    // modifying the vector incorrectly.

    for (size_t i = 0; i < lengthNotIncludingUndefined; i++)
        m_storage->m_vector[i] = values[i].first;

    checkConsistency(SortConsistencyCheck);
}

struct AVLTreeNodeForArrayCompare {
    JSValue* value;

    // Child pointers.  The high bit of gt is robbed and used as the
    // balance factor sign.  The high bit of lt is robbed and used as
    // the magnitude of the balance factor.
    int32_t gt;
    int32_t lt;
};

struct AVLTreeAbstractorForArrayCompare {
    typedef int32_t handle; // Handle is an index into m_nodes vector.
    typedef JSValue* key;
    typedef int32_t size;

    Vector<AVLTreeNodeForArrayCompare> m_nodes;
    ExecState* m_exec;
    JSValue* m_compareFunction;
    CallType m_compareCallType;
    const CallData* m_compareCallData;
    JSValue* m_globalThisValue;

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
        ASSERT(!va->isUndefined());
        ASSERT(!vb->isUndefined());

        if (m_exec->hadException())
            return 1;

        ArgList arguments;
        arguments.append(va);
        arguments.append(vb);
        double compareResult = call(m_exec, m_compareFunction, m_compareCallType, *m_compareCallData, m_globalThisValue, arguments)->toNumber(m_exec);
        return (compareResult < 0) ? -1 : 1; // Not passing equality through, because we need to store all values, even if equivalent.
    }

    int compare_key_node(key k, handle h) { return compare_key_key(k, m_nodes[h].value); }
    int compare_node_node(handle h1, handle h2) { return compare_key_key(m_nodes[h1].value, m_nodes[h2].value); }

    static handle null() { return 0x7FFFFFFF; }
};

void JSArray::sort(ExecState* exec, JSValue* compareFunction, CallType callType, const CallData& callData)
{
    checkConsistency();

    // FIXME: This ignores exceptions raised in the compare function or in toNumber.

    // The maximum tree depth is compiled in - but the caller is clearly up to no good
    // if a larger array is passed.
    ASSERT(m_length <= static_cast<unsigned>(std::numeric_limits<int>::max()));
    if (m_length > static_cast<unsigned>(std::numeric_limits<int>::max()))
        return;

    if (!m_length)
        return;

    unsigned usedVectorLength = min(m_length, m_vectorLength);

    AVLTree<AVLTreeAbstractorForArrayCompare, 44> tree; // Depth 44 is enough for 2^31 items
    tree.abstractor().m_exec = exec;
    tree.abstractor().m_compareFunction = compareFunction;
    tree.abstractor().m_compareCallType = callType;
    tree.abstractor().m_compareCallData = &callData;
    tree.abstractor().m_globalThisValue = exec->globalThisValue();
    tree.abstractor().m_nodes.resize(usedVectorLength + (m_storage->m_sparseValueMap ? m_storage->m_sparseValueMap->size() : 0));

    if (!tree.abstractor().m_nodes.begin()) {
        exec->setException(Error::create(exec, GeneralError, "Out of memory"));
        return;
    }

    // FIXME: If the compare function modifies the array, the vector, map, etc. could be modified
    // right out from under us while we're building the tree here.

    unsigned numDefined = 0;
    unsigned numUndefined = 0;

    // Iterate over the array, ignoring missing values, counting undefined ones, and inserting all other ones into the tree.
    for (; numDefined < usedVectorLength; ++numDefined) {
        JSValue* v = m_storage->m_vector[numDefined];
        if (!v || v->isUndefined())
            break;
        tree.abstractor().m_nodes[numDefined].value = v;
        tree.insert(numDefined);
    }
    for (unsigned i = numDefined; i < usedVectorLength; ++i) {
        if (JSValue* v = m_storage->m_vector[i]) {
            if (v->isUndefined())
                ++numUndefined;
            else {
                tree.abstractor().m_nodes[numDefined].value = v;
                tree.insert(numDefined);
                ++numDefined;
            }
        }
    }

    unsigned newUsedVectorLength = numDefined + numUndefined;

    if (SparseArrayValueMap* map = m_storage->m_sparseValueMap) {
        newUsedVectorLength += map->size();
        if (newUsedVectorLength > m_vectorLength) {
            if (!increaseVectorLength(newUsedVectorLength)) {
                exec->setException(Error::create(exec, GeneralError, "Out of memory"));
                return;
            }
        }

        SparseArrayValueMap::iterator end = map->end();
        for (SparseArrayValueMap::iterator it = map->begin(); it != end; ++it) {
            tree.abstractor().m_nodes[numDefined].value = it->second;
            tree.insert(numDefined);
            ++numDefined;
        }

        delete map;
        m_storage->m_sparseValueMap = 0;
    }

    ASSERT(tree.abstractor().m_nodes.size() >= numDefined);

    // FIXME: If the compare function changed the length of the array, the following might be
    // modifying the vector incorrectly.

    // Copy the values back into m_storage.
    AVLTree<AVLTreeAbstractorForArrayCompare, 44>::Iterator iter;
    iter.start_iter_least(tree);
    for (unsigned i = 0; i < numDefined; ++i) {
        m_storage->m_vector[i] = tree.abstractor().m_nodes[*iter].value;
        ++iter;
    }

    // Put undefined values back in.
    for (unsigned i = numDefined; i < newUsedVectorLength; ++i)
        m_storage->m_vector[i] = jsUndefined();

    // Ensure that unused values in the vector are zeroed out.
    for (unsigned i = newUsedVectorLength; i < usedVectorLength; ++i)
        m_storage->m_vector[i] = 0;

    m_storage->m_numValuesInVector = newUsedVectorLength;

    checkConsistency(SortConsistencyCheck);
}

unsigned JSArray::compactForSorting()
{
    checkConsistency();

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
            if (!increaseVectorLength(newUsedVectorLength))
                return 0;
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

    storage->m_numValuesInVector = newUsedVectorLength;

    checkConsistency(SortConsistencyCheck);

    return numDefined;
}

void* JSArray::lazyCreationData()
{
    return m_storage->lazyCreationData;
}

void JSArray::setLazyCreationData(void* d)
{
    m_storage->lazyCreationData = d;
}

#if CHECK_ARRAY_CONSISTENCY

void JSArray::checkConsistency(ConsistencyCheckType type)
{
    ASSERT(m_storage);
    if (type == SortConsistencyCheck)
        ASSERT(!m_storage->m_sparseValueMap);

    unsigned numValuesInVector = 0;
    for (unsigned i = 0; i < m_vectorLength; ++i) {
        if (JSValue* value = m_storage->m_vector[i]) {
            ASSERT(i < m_length);
            if (type != DestructorConsistencyCheck)
                value->type(); // Likely to crash if the object was deallocated.
            ++numValuesInVector;
        } else {
            if (type == SortConsistencyCheck)
                ASSERT(i >= m_storage->m_numValuesInVector);
        }
    }
    ASSERT(numValuesInVector == m_storage->m_numValuesInVector);

    if (m_storage->m_sparseValueMap) {
        SparseArrayValueMap::iterator end = m_storage->m_sparseValueMap->end();
        for (SparseArrayValueMap::iterator it = m_storage->m_sparseValueMap->begin(); it != end; ++it) {
            unsigned index = it->first;
            ASSERT(index < m_length);
            ASSERT(index >= m_vectorLength);
            ASSERT(index <= maxArrayIndex);
            ASSERT(it->second);
            if (type != DestructorConsistencyCheck)
                it->second->type(); // Likely to crash if the object was deallocated.
        }
    }
}

#endif

JSArray* constructEmptyArray(ExecState* exec)
{
    return new (exec) JSArray(exec->lexicalGlobalObject()->arrayPrototype(), 0);
}

JSArray* constructEmptyArray(ExecState* exec, unsigned initialLength)
{
    return new (exec) JSArray(exec->lexicalGlobalObject()->arrayPrototype(), initialLength);
}

JSArray* constructArray(ExecState* exec, JSValue* singleItemValue)
{
    ArgList values;
    values.append(singleItemValue);
    return new (exec) JSArray(exec->lexicalGlobalObject()->arrayPrototype(), values);
}

JSArray* constructArray(ExecState* exec, const ArgList& values)
{
    return new (exec) JSArray(exec->lexicalGlobalObject()->arrayPrototype(), values);
}

}
