/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "property_map.h"

#include "object.h"
#include "protect.h"
#include "PropertyNameArray.h"
#include <algorithm>
#include <wtf/FastMalloc.h>
#include <wtf/Vector.h>

using std::max;

#define DEBUG_PROPERTIES 0
#define DO_CONSISTENCY_CHECK 0
#define DUMP_STATISTICS 0
#define USE_SINGLE_ENTRY 1

// 2/28/2006 ggaren: super accurate JS iBench says that USE_SINGLE_ENTRY is a
// 3.2% performance boost.

#if !DO_CONSISTENCY_CHECK
#define checkConsistency() ((void)0)
#endif

namespace KJS {

// Choose a number for the following so that most property maps are smaller,
// but it's not going to blow out the stack to allocate this number of pointers.
const int smallMapThreshold = 1024;

#if DUMP_STATISTICS

static int numProbes;
static int numCollisions;
static int numRehashes;
static int numRemoves;

struct PropertyMapStatisticsExitLogger { ~PropertyMapStatisticsExitLogger(); };

static PropertyMapStatisticsExitLogger logger;

PropertyMapStatisticsExitLogger::~PropertyMapStatisticsExitLogger()
{
    printf("\nKJS::PropertyMap statistics\n\n");
    printf("%d probes\n", numProbes);
    printf("%d collisions (%.1f%%)\n", numCollisions, 100.0 * numCollisions / numProbes);
    printf("%d rehashes\n", numRehashes);
    printf("%d removes\n", numRemoves);
}

#endif

// lastIndexUsed is an ever-increasing index used to identify the order items
// were inserted into the property map. It's vital that getEnumerablePropertyNames
// return the properties in the order they were added for compatibility with other
// browsers' JavaScript implementations.
struct PropertyMapHashTable
{
    int sizeMask;
    int size;
    int keyCount;
    int sentinelCount;
    int lastIndexUsed;
    PropertyMapHashTableEntry entries[1];
};

class SavedProperty {
public:
    Identifier key;
    ProtectedPtr<JSValue> value;
    int attributes;
};

SavedProperties::SavedProperties() : _count(0) { }
SavedProperties::~SavedProperties() { }

// Algorithm concepts from Algorithms in C++, Sedgewick.

// This is a method rather than a variable to work around <rdar://problem/4462053>
static inline UString::Rep* deletedSentinel() { return reinterpret_cast<UString::Rep*>(0x1); }

// Returns true if the key is not null or the deleted sentinel, false otherwise
static inline bool isValid(UString::Rep* key)
{
    return !!(reinterpret_cast<uintptr_t>(key) & ~0x1);
}

PropertyMap::~PropertyMap()
{
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        UString::Rep* key = m_singleEntryKey;
        if (key)
            key->deref();
#endif
        return;
    }
    
    int minimumKeysToProcess = m_u.table->keyCount + m_u.table->sentinelCount;
    Entry *entries = m_u.table->entries;
    for (int i = 0; i < minimumKeysToProcess; i++) {
        UString::Rep *key = entries[i].key;
        if (key) {
            if (key != deletedSentinel())
                key->deref();
        } else
            ++minimumKeysToProcess;
    }
    fastFree(m_u.table);
}

void PropertyMap::clear()
{
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        UString::Rep* key = m_singleEntryKey;
        if (key) {
            key->deref();
            m_singleEntryKey = 0;
        }
#endif
        return;
    }

    int size = m_u.table->size;
    Entry *entries = m_u.table->entries;
    for (int i = 0; i < size; i++) {
        UString::Rep *key = entries[i].key;
        if (isValid(key)) {
            key->deref();
            entries[i].key = 0;
            entries[i].value = 0;
        }
    }
    m_u.table->keyCount = 0;
    m_u.table->sentinelCount = 0;
}

JSValue *PropertyMap::get(const Identifier &name, unsigned &attributes) const
{
    assert(!name.isNull());
    
    UString::Rep *rep = name._ustring.rep();
    
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        UString::Rep* key = m_singleEntryKey;
        if (rep == key) {
            attributes = m_singleEntryAttributes;
            return m_u.singleEntryValue;
        }
#endif
        return 0;
    }
    
    unsigned h = rep->hash();
    int sizeMask = m_u.table->sizeMask;
    Entry *entries = m_u.table->entries;
    int i = h & sizeMask;
    int k = 0;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += entries[i].key && entries[i].key != rep;
#endif
    while (UString::Rep *key = entries[i].key) {
        if (rep == key) {
            attributes = entries[i].attributes;
            return entries[i].value;
        }
        if (k == 0)
            k = 1 | (h % sizeMask);
        i = (i + k) & sizeMask;
#if DUMP_STATISTICS
        ++numRehashes;
#endif
    }
    return 0;
}

JSValue *PropertyMap::get(const Identifier &name) const
{
    assert(!name.isNull());
    
    UString::Rep *rep = name._ustring.rep();

    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        UString::Rep *key = m_singleEntryKey;
        if (rep == key)
            return m_u.singleEntryValue;
#endif
        return 0;
    }
    
    unsigned h = rep->hash();
    int sizeMask = m_u.table->sizeMask;
    Entry *entries = m_u.table->entries;
    int i = h & sizeMask;
    int k = 0;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += entries[i].key && entries[i].key != rep;
#endif
    while (UString::Rep *key = entries[i].key) {
        if (rep == key)
            return entries[i].value;
        if (k == 0)
            k = 1 | (h % sizeMask);
        i = (i + k) & sizeMask;
#if DUMP_STATISTICS
        ++numRehashes;
#endif
    }
    return 0;
}

JSValue **PropertyMap::getLocation(const Identifier &name)
{
    assert(!name.isNull());
    
    UString::Rep *rep = name._ustring.rep();

    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        UString::Rep *key = m_singleEntryKey;
        if (rep == key)
            return &m_u.singleEntryValue;
#endif
        return 0;
    }
    
    unsigned h = rep->hash();
    int sizeMask = m_u.table->sizeMask;
    Entry *entries = m_u.table->entries;
    int i = h & sizeMask;
    int k = 0;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += entries[i].key && entries[i].key != rep;
#endif
    while (UString::Rep *key = entries[i].key) {
        if (rep == key)
            return &entries[i].value;
        if (k == 0)
            k = 1 | (h % sizeMask);
        i = (i + k) & sizeMask;
#if DUMP_STATISTICS
        ++numRehashes;
#endif
    }
    return 0;
}

#if DEBUG_PROPERTIES
static void printAttributes(int attributes)
{
    if (attributes == 0)
        printf("None");
    else {
        if (attributes & ReadOnly)
            printf("ReadOnly ");
        if (attributes & DontEnum)
            printf("DontEnum ");
        if (attributes & DontDelete)
            printf("DontDelete ");
        if (attributes & Internal)
            printf("Internal ");
        if (attributes & Function)
            printf("Function ");
    }
}
#endif

void PropertyMap::put(const Identifier &name, JSValue *value, int attributes, bool roCheck)
{
    assert(!name.isNull());
    assert(value != 0);
    
    checkConsistency();

    UString::Rep *rep = name._ustring.rep();
    
#if DEBUG_PROPERTIES
    printf("adding property %s, attributes = 0x%08x (", name.ascii(), attributes);
    printAttributes(attributes);
    printf(")\n");
#endif
    
#if USE_SINGLE_ENTRY
    if (!m_usingTable) {
        UString::Rep *key = m_singleEntryKey;
        if (key) {
            if (rep == key && !(roCheck && (m_singleEntryAttributes & ReadOnly))) {
                m_u.singleEntryValue = value;
                return;
            }
        } else {
            rep->ref();
            m_singleEntryKey = rep;
            m_u.singleEntryValue = value;
            m_singleEntryAttributes = static_cast<short>(attributes);
            checkConsistency();
            return;
        }
    }
#endif

    if (!m_usingTable || m_u.table->keyCount * 2 >= m_u.table->size)
        expand();
    
    unsigned h = rep->hash();
    int sizeMask = m_u.table->sizeMask;
    Entry *entries = m_u.table->entries;
    int i = h & sizeMask;
    int k = 0;
    bool foundDeletedElement = false;
    int deletedElementIndex = 0;    /* initialize to make the compiler happy */
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += entries[i].key && entries[i].key != rep;
#endif
    while (UString::Rep *key = entries[i].key) {
        if (rep == key) {
            if (roCheck && (m_u.table->entries[i].attributes & ReadOnly)) 
                return;
            // Put a new value in an existing hash table entry.
            entries[i].value = value;
            // Attributes are intentionally not updated.
            return;
        }
        // If we find the deleted-element sentinel, remember it for use later.
        if (key == deletedSentinel() && !foundDeletedElement) {
            foundDeletedElement = true;
            deletedElementIndex = i;
        }
        if (k == 0)
            k = 1 | (h % sizeMask);
        i = (i + k) & sizeMask;
#if DUMP_STATISTICS
        ++numRehashes;
#endif
    }

    // Use either the deleted element or the 0 at the end of the chain.
    if (foundDeletedElement) {
        i = deletedElementIndex;
        --m_u.table->sentinelCount;
    }

    // Create a new hash table entry.
    rep->ref();
    entries[i].key = rep;
    entries[i].value = value;
    entries[i].attributes = static_cast<short>(attributes);
    entries[i].index = ++m_u.table->lastIndexUsed;
    ++m_u.table->keyCount;

    checkConsistency();
}

void PropertyMap::insert(UString::Rep *key, JSValue *value, int attributes, int index)
{
    assert(m_u.table);

    unsigned h = key->hash();
    int sizeMask = m_u.table->sizeMask;
    Entry *entries = m_u.table->entries;
    int i = h & sizeMask;
    int k = 0;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += entries[i].key && entries[i].key != key;
#endif
    while (entries[i].key) {
        assert(entries[i].key != deletedSentinel());
        if (k == 0)
            k = 1 | (h % sizeMask);
        i = (i + k) & sizeMask;
#if DUMP_STATISTICS
        ++numRehashes;
#endif
    }
    
    entries[i].key = key;
    entries[i].value = value;
    entries[i].attributes = static_cast<short>(attributes);
    entries[i].index = index;
}

void PropertyMap::expand()
{
    Table *oldTable = m_u.table;
    int oldTableSize = m_usingTable ? oldTable->size : 0;    
    rehash(oldTableSize ? oldTableSize * 2 : 16);
}

void PropertyMap::rehash()
{
    assert(m_u.table);
    assert(m_u.table->size);
    rehash(m_u.table->size);
}

void PropertyMap::rehash(int newTableSize)
{
    checkConsistency();

#if USE_SINGLE_ENTRY
    JSValue* oldSingleEntryValue = m_u.singleEntryValue;
#endif

    Table *oldTable = m_usingTable ? m_u.table : 0;
    int oldTableSize = m_usingTable ? oldTable->size : 0;
    int oldTableKeyCount = m_usingTable ? oldTable->keyCount : 0;
    
    m_u.table = (Table *)fastCalloc(1, sizeof(Table) + (newTableSize - 1) * sizeof(Entry) );
    m_u.table->size = newTableSize;
    m_u.table->sizeMask = newTableSize - 1;
    m_u.table->keyCount = oldTableKeyCount;
    m_usingTable = true;

#if USE_SINGLE_ENTRY
    UString::Rep* key = m_singleEntryKey;
    if (key) {
        insert(key, oldSingleEntryValue, m_singleEntryAttributes, 0);
        m_singleEntryKey = 0;
        // update the count, because single entries don't count towards
        // the table key count
        ++m_u.table->keyCount;
        assert(m_u.table->keyCount == 1);
    }
#endif
    
    int lastIndexUsed = 0;
    for (int i = 0; i != oldTableSize; ++i) {
        Entry &entry = oldTable->entries[i];
        UString::Rep *key = entry.key;
        if (isValid(key)) {
            int index = entry.index;
            lastIndexUsed = max(index, lastIndexUsed);
            insert(key, entry.value, entry.attributes, index);
        }
    }
    m_u.table->lastIndexUsed = lastIndexUsed;

    fastFree(oldTable);

    checkConsistency();
}

void PropertyMap::remove(const Identifier &name)
{
    assert(!name.isNull());
    
    checkConsistency();

    UString::Rep *rep = name._ustring.rep();

    UString::Rep *key;

    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        key = m_singleEntryKey;
        if (rep == key) {
            key->deref();
            m_singleEntryKey = 0;
            checkConsistency();
        }
#endif
        return;
    }

    // Find the thing to remove.
    unsigned h = rep->hash();
    int sizeMask = m_u.table->sizeMask;
    Entry *entries = m_u.table->entries;
    int i = h & sizeMask;
    int k = 0;
#if DUMP_STATISTICS
    ++numProbes;
    ++numRemoves;
    numCollisions += entries[i].key && entries[i].key != rep;
#endif
    while ((key = entries[i].key)) {
        if (rep == key)
            break;
        if (k == 0)
            k = 1 | (h % sizeMask);
        i = (i + k) & sizeMask;
#if DUMP_STATISTICS
        ++numRehashes;
#endif
    }
    if (!key)
        return;
    
    // Replace this one element with the deleted sentinel. Also set value to 0 and attributes to DontEnum
    // to help callers that iterate all keys not have to check for the sentinel.
    key->deref();
    key = deletedSentinel();
    entries[i].key = key;
    entries[i].value = 0;
    entries[i].attributes = DontEnum;
    assert(m_u.table->keyCount >= 1);
    --m_u.table->keyCount;
    ++m_u.table->sentinelCount;
    
    if (m_u.table->sentinelCount * 4 >= m_u.table->size)
        rehash();

    checkConsistency();
}

void PropertyMap::mark() const
{
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        if (m_singleEntryKey) {
            JSValue* v = m_u.singleEntryValue;
            if (!v->marked())
                v->mark();
        }
#endif
        return;
    }

    int minimumKeysToProcess = m_u.table->keyCount;
    Entry *entries = m_u.table->entries;
    for (int i = 0; i < minimumKeysToProcess; i++) {
        JSValue *v = entries[i].value;
        if (v) {
            if (!v->marked())
                v->mark();
        } else {
            ++minimumKeysToProcess;
        }
    }
}

static int comparePropertyMapEntryIndices(const void *a, const void *b)
{
    int ia = static_cast<PropertyMapHashTableEntry * const *>(a)[0]->index;
    int ib = static_cast<PropertyMapHashTableEntry * const *>(b)[0]->index;
    if (ia < ib)
        return -1;
    if (ia > ib)
        return +1;
    return 0;
}

bool PropertyMap::containsGettersOrSetters() const
{
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        return !!(m_singleEntryAttributes & GetterSetter);
#else
        return false;
#endif
    }

    for (int i = 0; i != m_u.table->size; ++i) {
        if (m_u.table->entries[i].attributes & GetterSetter)
            return true;
    }
    
    return false;
}

void PropertyMap::getEnumerablePropertyNames(PropertyNameArray& propertyNames) const
{
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        UString::Rep* key = m_singleEntryKey;
        if (key && !(m_singleEntryAttributes & DontEnum))
            propertyNames.add(Identifier(key));
#endif
        return;
    }

    // Allocate a buffer to use to sort the keys.
    Vector<Entry*, smallMapThreshold> sortedEnumerables(m_u.table->keyCount);

    // Get pointers to the enumerable entries in the buffer.
    Entry** p = sortedEnumerables.data();
    int size = m_u.table->size;
    Entry* entries = m_u.table->entries;
    for (int i = 0; i != size; ++i) {
        Entry* e = &entries[i];
        if (e->key && !(e->attributes & DontEnum))
            *p++ = e;
    }

    // Sort the entries by index.
    qsort(sortedEnumerables.data(), p - sortedEnumerables.data(), sizeof(Entry*), comparePropertyMapEntryIndices);

    // Put the keys of the sorted entries into the list.
    for (Entry** q = sortedEnumerables.data(); q != p; ++q)
        propertyNames.add(Identifier(q[0]->key));
}

void PropertyMap::getSparseArrayPropertyNames(PropertyNameArray& propertyNames) const
{
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        UString::Rep *key = m_singleEntryKey;
        if (key) {
            UString k(key);
            bool fitsInUInt32;
            k.toUInt32(&fitsInUInt32);
            if (fitsInUInt32)
                propertyNames.add(Identifier(key));
        }
#endif
        return;
    }

    int size = m_u.table->size;
    Entry *entries = m_u.table->entries;
    for (int i = 0; i != size; ++i) {
        UString::Rep *key = entries[i].key;
        if (isValid(key)) {
            UString k(key);
            bool fitsInUInt32;
            k.toUInt32(&fitsInUInt32);
            if (fitsInUInt32)
                propertyNames.add(Identifier(key));
        }
    }
}

void PropertyMap::save(SavedProperties &p) const
{
    int count = 0;

    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        if (m_singleEntryKey && !(m_singleEntryAttributes & (ReadOnly | Function)))
            ++count;
#endif
    } else {
        int size = m_u.table->size;
        Entry *entries = m_u.table->entries;
        for (int i = 0; i != size; ++i)
            if (isValid(entries[i].key) && !(entries[i].attributes & (ReadOnly | Function)))
                ++count;
    }

    p._properties.clear();
    p._count = count;

    if (count == 0)
        return;
    
    p._properties.set(new SavedProperty [count]);
    
    SavedProperty *prop = p._properties.get();
    
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        if (m_singleEntryKey && !(m_singleEntryAttributes & (ReadOnly | Function))) {
            prop->key = Identifier(m_singleEntryKey);
            prop->value = m_u.singleEntryValue;
            prop->attributes = m_singleEntryAttributes;
            ++prop;
        }
#endif
    } else {
        // Save in the right order so we don't lose the order.
        // Another possibility would be to save the indices.

        // Allocate a buffer to use to sort the keys.
        Vector<Entry*, smallMapThreshold> sortedEntries(count);

        // Get pointers to the entries in the buffer.
        Entry** p = sortedEntries.data();
        int size = m_u.table->size;
        Entry* entries = m_u.table->entries;
        for (int i = 0; i != size; ++i) {
            Entry *e = &entries[i];
            if (isValid(e->key) && !(e->attributes & (ReadOnly | Function)))
                *p++ = e;
        }
        assert(p - sortedEntries.data() == count);

        // Sort the entries by index.
        qsort(sortedEntries.data(), p - sortedEntries.data(), sizeof(Entry*), comparePropertyMapEntryIndices);

        // Put the sorted entries into the saved properties list.
        for (Entry** q = sortedEntries.data(); q != p; ++q, ++prop) {
            Entry* e = *q;
            prop->key = Identifier(e->key);
            prop->value = e->value;
            prop->attributes = e->attributes;
        }
    }
}

void PropertyMap::restore(const SavedProperties &p)
{
    for (int i = 0; i != p._count; ++i)
        put(p._properties[i].key, p._properties[i].value, p._properties[i].attributes);
}

#if DO_CONSISTENCY_CHECK

void PropertyMap::checkConsistency()
{
    if (!m_usingTable)
        return;

    int count = 0;
    int sentinelCount = 0;
    for (int j = 0; j != m_u.table->size; ++j) {
        UString::Rep *rep = m_u.table->entries[j].key;
        if (!rep)
            continue;
        if (rep == deletedSentinel()) {
            ++sentinelCount;
            continue;
        }
        unsigned h = rep->hash();
        int i = h & m_u.table->sizeMask;
        int k = 0;
        while (UString::Rep *key = m_u.table->entries[i].key) {
            if (rep == key)
                break;
            if (k == 0)
                k = 1 | (h % m_u.table->sizeMask);
            i = (i + k) & m_u.table->sizeMask;
        }
        assert(i == j);
        ++count;
    }
    assert(count == m_u.table->keyCount);
    assert(sentinelCount == m_u.table->sentinelCount);
    assert(m_u.table->size >= 16);
    assert(m_u.table->sizeMask);
    assert(m_u.table->size == m_u.table->sizeMask + 1);
}

#endif // DO_CONSISTENCY_CHECK

} // namespace KJS
