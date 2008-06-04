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
#include <wtf/AVLTree.h>

using namespace std;

namespace KJS {

typedef HashMap<unsigned, JSValue*> SparseArrayValueMap;

struct ArrayStorage {
    unsigned m_numValuesInVector;
    SparseArrayValueMap* m_sparseValueMap;
    void* lazyCreationData; // An ArrayInstance subclass can use this to fill the vector lazily.
    JSValue* m_vector[1];
};

// 0xFFFFFFFF is a bit weird -- is not an array index even though it's an integer
static const unsigned maxArrayIndex = 0xFFFFFFFEU;

// Our policy for when to use a vector and when to use a sparse map.
// For all array indices under sparseArrayCutoff, we always use a vector.
// When indices greater than sparseArrayCutoff are involved, we use a vector
// as long as it is 1/8 full. If more sparse than that, we use a map.
// This value has to be a macro to be used in max() and min() without introducing
// a PIC branch in Mach-O binaries, see <rdar://problem/5971391>.
#define sparseArrayCutoff 10000U
static const unsigned minDensityMultiplier = 8;

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

    Collector::reportExtraMemoryCost(initialCapacity * sizeof(JSValue*));
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

JSValue* ArrayInstance::lengthGetter(ExecState*, const Identifier&, const PropertySlot& slot)
{
    return jsNumber(static_cast<ArrayInstance*>(slot.slotBase())->m_length);
}

ALWAYS_INLINE bool ArrayInstance::inlineGetOwnPropertySlot(ExecState* exec, unsigned i, PropertySlot& slot)
{
    ArrayStorage* storage = m_storage;

    if (UNLIKELY(i >= m_length)) {
        if (i > maxArrayIndex)
            return getOwnPropertySlot(exec, Identifier::from(i), slot);
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

bool ArrayInstance::increaseVectorLength(unsigned newLength)
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

void ArrayInstance::sort(ExecState* exec)
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

    Vector<std::pair<JSValue*, UString> > values(lengthNotIncludingUndefined);
    if (!values.begin()) {
        exec->setException(Error::create(exec, GeneralError, "Out of memory"));
        return;
    }

    for (size_t i = 0; i < lengthNotIncludingUndefined; i++) {
        JSValue* value = m_storage->m_vector[i];
        ASSERT(!value->isUndefined());
        values[i].first = value;
        values[i].second = value->toString(exec);
    }

    if (exec->hadException())
        return;

    // FIXME: Since we sort by string value, a fast algorithm might be to use a radix sort. That would be O(N) rather
    // than O(N log N).

#if HAVE(MERGESORT)
    mergesort(values.begin(), values.size(), sizeof(std::pair<JSValue*, UString>), compareByStringPairForQSort);
#else
    // FIXME: QSort may not be stable in some implementations. ECMAScript-262 does not require this, but in practice, browsers perform stable sort.
    qsort(values.begin(), values.size(), sizeof(std::pair<JSValue*, UString>), compareByStringPairForQSort);
#endif

    for (size_t i = 0; i < lengthNotIncludingUndefined; i++)
        m_storage->m_vector[i] = values[i].first;
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
    JSObject* m_compareFunction;
    JSObject* m_globalThisValue;

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
        }
    }

    int compare_key_key(key va, key vb)
    {
        ASSERT(!va->isUndefined());
        ASSERT(!vb->isUndefined());

        if (m_exec->hadException())
            return 1;

        List arguments;
        arguments.append(va);
        arguments.append(vb);
        double compareResult = m_compareFunction->callAsFunction(m_exec, m_globalThisValue, arguments)->toNumber(m_exec);
        return (compareResult < 0) ? -1 : 1; // Not passing equality through, because we need to store all values, even if equivalent.
    }

    int compare_key_node(key k, handle h) { return compare_key_key(k, m_nodes[h].value); }
    int compare_node_node(handle h1, handle h2) { return compare_key_key(m_nodes[h1].value, m_nodes[h2].value); }

    static handle null() { return 0x7FFFFFFF; }
};

void ArrayInstance::sort(ExecState* exec, JSObject* compareFunction)
{
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
    tree.abstractor().m_globalThisValue = exec->globalThisValue();
    tree.abstractor().m_nodes.resize(usedVectorLength + (m_storage->m_sparseValueMap ? m_storage->m_sparseValueMap->size() : 0));

    if (!tree.abstractor().m_nodes.begin()) {
        exec->setException(Error::create(exec, GeneralError, "Out of memory"));
        return;
    }

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

    return numDefined;
}

void* ArrayInstance::lazyCreationData()
{
    return m_storage->lazyCreationData;
}

void ArrayInstance::setLazyCreationData(void* d)
{
    m_storage->lazyCreationData = d;
}

}
