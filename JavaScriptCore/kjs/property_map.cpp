/*
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashTable.h>
#include <wtf/Vector.h>

using std::max;
using WTF::doubleHash;

#ifndef NDEBUG
#define DO_PROPERTYMAP_CONSTENCY_CHECK 0
#define DUMP_PROPERTYMAP_STATS 0
#else
#define DO_PROPERTYMAP_CONSTENCY_CHECK 0
#define DUMP_PROPERTYMAP_STATS 0
#endif

#define USE_SINGLE_ENTRY 1

// 2/28/2006 ggaren: command-line JS iBench says that USE_SINGLE_ENTRY is a
// 3.2% performance boost.

namespace KJS {

// Choose a number for the following so that most property maps are smaller,
// but it's not going to blow out the stack to allocate this number of pointers.
static const int smallMapThreshold = 1024;

// The point at which the function call overhead of the qsort implementation
// becomes small compared to the inefficiency of insertion sort.
static const unsigned tinyMapThreshold = 20;

#if DUMP_PROPERTYMAP_STATS

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

struct PropertyMapEntry {
    UString::Rep* key;
    JSValue* value;
    unsigned attributes;
    unsigned index;

    PropertyMapEntry(UString::Rep* k, JSValue* v, int a)
        : key(k), value(v), attributes(a), index(0)
    {
    }
};

// lastIndexUsed is an ever-increasing index used to identify the order items
// were inserted into the property map. It's required that getEnumerablePropertyNames
// return the properties in the order they were added for compatibility with other
// browsers' JavaScript implementations.
struct PropertyMapHashTable {
    unsigned sizeMask;
    unsigned size;
    unsigned keyCount;
    unsigned deletedSentinelCount;
    unsigned lastIndexUsed;
    unsigned entryIndicies[1];

    PropertyMapEntry* entries()
    {
        // The entries vector comes after the indices vector.
        // The 0th item in the entries vector is not really used; it has to
        // have a 0 in its key to allow the hash table lookup to handle deleted
        // sentinels without any special-case code, but the other fields are unused.
        return reinterpret_cast<PropertyMapEntry*>(&entryIndicies[size]);
    }

    static size_t allocationSize(unsigned size)
    {
        // We never let a hash table get more than half full,
        // So the number of indices we need is the size of the hash table.
        // But the number of entries is half that (plus one for the deleted sentinel).
        return sizeof(PropertyMapHashTable)
            + (size - 1) * sizeof(unsigned)
            + (1 + size / 2) * sizeof(PropertyMapEntry);
    }
};

static const unsigned emptyEntryIndex = 0;
static const unsigned deletedSentinelIndex = 1;

SavedProperties::SavedProperties()
    : count(0)
{
}

SavedProperties::~SavedProperties()
{
}

#if !DO_PROPERTYMAP_CONSTENCY_CHECK

inline void PropertyMap::checkConsistency()
{
}

#endif

PropertyMap::~PropertyMap()
{
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        if (m_singleEntryKey)
            m_singleEntryKey->deref();
#endif
        return;
    }
    
    unsigned entryCount = m_u.table->keyCount + m_u.table->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; i++) {
        if (UString::Rep* key = m_u.table->entries()[i].key)
            key->deref();
    }
    fastFree(m_u.table);
}

void PropertyMap::clear()
{
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        if (m_singleEntryKey) {
            m_singleEntryKey->deref();
            m_singleEntryKey = 0;
        }
#endif
        return;
    }

    unsigned entryCount = m_u.table->keyCount + m_u.table->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; i++) {
        if (UString::Rep* key = m_u.table->entries()[i].key)
            key->deref();
    }
    for (unsigned i = 0; i < m_u.table->size; i++)
        m_u.table->entryIndicies[i] = emptyEntryIndex;
    m_u.table->keyCount = 0;
    m_u.table->deletedSentinelCount = 0;
}

JSValue* PropertyMap::get(const Identifier& name, unsigned& attributes) const
{
    ASSERT(!name.isNull());
    
    UString::Rep* rep = name._ustring.rep();
    
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        if (rep == m_singleEntryKey) {
            attributes = m_singleEntryAttributes;
            return m_u.singleEntryValue;
        }
#endif
        return 0;
    }
    
    unsigned i = rep->computedHash();

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    unsigned entryIndex = m_u.table->entryIndicies[i & m_u.table->sizeMask];
    if (entryIndex == emptyEntryIndex)
        return 0;

    if (rep == m_u.table->entries()[entryIndex - 1].key) {
        attributes = m_u.table->entries()[entryIndex - 1].attributes;
        return m_u.table->entries()[entryIndex - 1].value;
    }

#if DUMP_PROPERTYMAP_STATS
    ++numCollisions;
#endif

    unsigned k = 1 | doubleHash(rep->computedHash());

    while (1) {
        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif

        entryIndex = m_u.table->entryIndicies[i & m_u.table->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return 0;

        if (rep == m_u.table->entries()[entryIndex - 1].key) {
            attributes = m_u.table->entries()[entryIndex - 1].attributes;
            return m_u.table->entries()[entryIndex - 1].value;
        }
    }
}

JSValue* PropertyMap::get(const Identifier& name) const
{
    ASSERT(!name.isNull());
    
    UString::Rep* rep = name._ustring.rep();
    
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        if (rep == m_singleEntryKey)
            return m_u.singleEntryValue;
#endif
        return 0;
    }
    
    unsigned i = rep->computedHash();

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    unsigned entryIndex = m_u.table->entryIndicies[i & m_u.table->sizeMask];
    if (entryIndex == emptyEntryIndex)
        return 0;

    if (rep == m_u.table->entries()[entryIndex - 1].key)
        return m_u.table->entries()[entryIndex - 1].value;

#if DUMP_PROPERTYMAP_STATS
    ++numCollisions;
#endif

    unsigned k = 1 | doubleHash(rep->computedHash());

    while (1) {
        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif

        entryIndex = m_u.table->entryIndicies[i & m_u.table->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return 0;

        if (rep == m_u.table->entries()[entryIndex - 1].key)
            return m_u.table->entries()[entryIndex - 1].value;
    }
}

JSValue** PropertyMap::getLocation(const Identifier& name)
{
    ASSERT(!name.isNull());
    
    UString::Rep* rep = name._ustring.rep();
    
    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        if (rep == m_singleEntryKey)
            return &m_u.singleEntryValue;
#endif
        return 0;
    }
    
    unsigned i = rep->computedHash();

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    unsigned entryIndex = m_u.table->entryIndicies[i & m_u.table->sizeMask];
    if (entryIndex == emptyEntryIndex)
        return 0;

    if (rep == m_u.table->entries()[entryIndex - 1].key)
        return &m_u.table->entries()[entryIndex - 1].value;

#if DUMP_PROPERTYMAP_STATS
    ++numCollisions;
#endif

    unsigned k = 1 | doubleHash(rep->computedHash());

    while (1) {
        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif

        entryIndex = m_u.table->entryIndicies[i & m_u.table->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return 0;

        if (rep == m_u.table->entries()[entryIndex - 1].key)
            return &m_u.table->entries()[entryIndex - 1].value;
    }
}

void PropertyMap::put(const Identifier& name, JSValue* value, unsigned attributes, bool checkReadOnly)
{
    ASSERT(!name.isNull());
    ASSERT(value);
    
    checkConsistency();

    UString::Rep* rep = name._ustring.rep();
    
#if USE_SINGLE_ENTRY
    if (!m_usingTable) {
        if (!m_singleEntryKey) {
            rep->ref();
            m_singleEntryKey = rep;
            m_u.singleEntryValue = value;
            m_singleEntryAttributes = static_cast<short>(attributes);
            checkConsistency();
            return;
        }
        if (rep == m_singleEntryKey && !(checkReadOnly && (m_singleEntryAttributes & ReadOnly))) {
            m_u.singleEntryValue = value;
            return;
        }
    }
#endif

    if (!m_usingTable || (m_u.table->keyCount + m_u.table->deletedSentinelCount) * 2 >= m_u.table->size)
        expand();

    // FIXME: Consider a fast case for tables with no deleted sentinels.

    unsigned i = rep->computedHash();
    unsigned k = 0;
    bool foundDeletedElement = false;
    unsigned deletedElementIndex = 0; // initialize to make the compiler happy

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    while (1) {
        unsigned entryIndex = m_u.table->entryIndicies[i & m_u.table->sizeMask];
        if (entryIndex == emptyEntryIndex)
            break;

        if (m_u.table->entries()[entryIndex - 1].key == rep) {
            if (checkReadOnly && (m_u.table->entries()[entryIndex - 1].attributes & ReadOnly)) 
                return;
            // Put a new value in an existing hash table entry.
            m_u.table->entries()[entryIndex - 1].value = value;
            // Attributes are intentionally not updated.
            return;
        } else if (entryIndex == deletedSentinelIndex) {
            // If we find a deleted-element sentinel, remember it for use later.
            if (!foundDeletedElement) {
                foundDeletedElement = true;
                deletedElementIndex = i;
            }
        }

        if (k == 0) {
            k = 1 | doubleHash(rep->computedHash());
#if DUMP_PROPERTYMAP_STATS
            ++numCollisions;
#endif
        }

        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif
    }

    // Figure out which entry to use.
    unsigned entryIndex = m_u.table->keyCount + m_u.table->deletedSentinelCount + 2;
    if (foundDeletedElement) {
        i = deletedElementIndex;
        --m_u.table->deletedSentinelCount;

        // Since we're not making the table bigger, we can't use the entry one past
        // the end that we were planning on using, so search backwards for the empty
        // slot that we can use. We know it will be there because we did at least one
        // deletion in the past that left an entry empty.
        while (m_u.table->entries()[--entryIndex - 1].key)
            ;
    }


    // Create a new hash table entry.
    m_u.table->entryIndicies[i & m_u.table->sizeMask] = entryIndex;

    // Create a new hash table entry.
    rep->ref();
    m_u.table->entries()[entryIndex - 1].key = rep;
    m_u.table->entries()[entryIndex - 1].value = value;
    m_u.table->entries()[entryIndex - 1].attributes = attributes;
    m_u.table->entries()[entryIndex - 1].index = ++m_u.table->lastIndexUsed;
    ++m_u.table->keyCount;

    checkConsistency();
}

void PropertyMap::insert(const Entry& entry)
{
    ASSERT(m_u.table);

    unsigned i = entry.key->computedHash();
    unsigned k = 0;

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    while (1) {
        unsigned entryIndex = m_u.table->entryIndicies[i & m_u.table->sizeMask];
        if (entryIndex == emptyEntryIndex)
            break;

        if (k == 0) {
            k = 1 | doubleHash(entry.key->computedHash());
#if DUMP_PROPERTYMAP_STATS
            ++numCollisions;
#endif
        }

        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif
    }

    unsigned entryIndex = m_u.table->keyCount + 2;
    m_u.table->entryIndicies[i & m_u.table->sizeMask] = entryIndex;
    m_u.table->entries()[entryIndex - 1] = entry;
    ++m_u.table->keyCount;
}

void PropertyMap::expand()
{
    if (!m_usingTable)
        createTable();
    else
        rehash(m_u.table->size * 2);
}

void PropertyMap::rehash()
{
    ASSERT(m_usingTable);
    ASSERT(m_u.table);
    ASSERT(m_u.table->size);
    rehash(m_u.table->size);
}

void PropertyMap::createTable()
{
    const unsigned newTableSize = 16;

    ASSERT(!m_usingTable);

    checkConsistency();

#if USE_SINGLE_ENTRY
    JSValue* oldSingleEntryValue = m_u.singleEntryValue;
#endif

    m_u.table = static_cast<Table*>(fastZeroedMalloc(Table::allocationSize(newTableSize)));
    m_u.table->size = newTableSize;
    m_u.table->sizeMask = newTableSize - 1;
    m_usingTable = true;

#if USE_SINGLE_ENTRY
    if (m_singleEntryKey) {
        insert(Entry(m_singleEntryKey, oldSingleEntryValue, m_singleEntryAttributes));
        m_singleEntryKey = 0;
    }
#endif

    checkConsistency();
}

void PropertyMap::rehash(unsigned newTableSize)
{
    ASSERT(!m_singleEntryKey);
    ASSERT(m_u.table);
    ASSERT(m_usingTable);

    checkConsistency();

    Table* oldTable = m_u.table;
    
    m_u.table = static_cast<Table*>(fastZeroedMalloc(Table::allocationSize(newTableSize)));
    m_u.table->size = newTableSize;
    m_u.table->sizeMask = newTableSize - 1;

    unsigned lastIndexUsed = 0;
    unsigned entryCount = oldTable->keyCount + oldTable->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; ++i) {
        if (oldTable->entries()[i].key) {
            lastIndexUsed = max(oldTable->entries()[i].index, lastIndexUsed);
            insert(oldTable->entries()[i]);
        }
    }
    m_u.table->lastIndexUsed = lastIndexUsed;

    fastFree(oldTable);

    checkConsistency();
}

void PropertyMap::remove(const Identifier& name)
{
    ASSERT(!name.isNull());
    
    checkConsistency();

    UString::Rep* rep = name._ustring.rep();

    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        if (rep == m_singleEntryKey) {
            m_singleEntryKey->deref();
            m_singleEntryKey = 0;
            checkConsistency();
        }
#endif
        return;
    }

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
    ++numRemoves;
#endif

    // Find the thing to remove.
    unsigned i = rep->computedHash();
    unsigned k = 0;
    unsigned entryIndex;
    UString::Rep* key = 0;
    while (1) {
        entryIndex = m_u.table->entryIndicies[i & m_u.table->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return;

        key = m_u.table->entries()[entryIndex - 1].key;
        if (rep == key)
            break;

        if (k == 0) {
            k = 1 | doubleHash(rep->computedHash());
#if DUMP_PROPERTYMAP_STATS
            ++numCollisions;
#endif
        }

        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif
    }

    // Replace this one element with the deleted sentinel. Also clear out
    // the entry so we can iterate all the entries as needed.
    m_u.table->entryIndicies[i & m_u.table->sizeMask] = deletedSentinelIndex;
    key->deref();
    m_u.table->entries()[entryIndex - 1].key = 0;
    m_u.table->entries()[entryIndex - 1].value = jsUndefined();
    m_u.table->entries()[entryIndex - 1].attributes = 0;
    ASSERT(m_u.table->keyCount >= 1);
    --m_u.table->keyCount;
    ++m_u.table->deletedSentinelCount;

    if (m_u.table->deletedSentinelCount * 4 >= m_u.table->size)
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

    unsigned entryCount = m_u.table->keyCount + m_u.table->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; i++) {
        JSValue* v = m_u.table->entries()[i].value;
        if (!v->marked())
            v->mark();
    }
}

static int comparePropertyMapEntryIndices(const void* a, const void* b)
{
    unsigned ia = static_cast<PropertyMapEntry* const*>(a)[0]->index;
    unsigned ib = static_cast<PropertyMapEntry* const*>(b)[0]->index;
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

    unsigned entryCount = m_u.table->keyCount + m_u.table->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; i++) {
        if (m_u.table->entries()[i].attributes & GetterSetter)
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

    if (m_u.table->keyCount < tinyMapThreshold) {
        Entry* a[tinyMapThreshold];
        int i = 0;
        unsigned entryCount = m_u.table->keyCount + m_u.table->deletedSentinelCount;
        for (unsigned k = 1; k <= entryCount; k++) {
            if (m_u.table->entries()[k].key && !(m_u.table->entries()[k].attributes & DontEnum)) {
                Entry* value = &m_u.table->entries()[k];
                int j;
                for (j = i - 1; j >= 0 && a[j]->index > value->index; --j)
                    a[j + 1] = a[j];
                a[j + 1] = value;
                ++i;
            }
        }
        for (int k = 0; k < i; ++k)
            propertyNames.add(Identifier(a[k]->key));
        return;
    }

    // Allocate a buffer to use to sort the keys.
    Vector<Entry*, smallMapThreshold> sortedEnumerables(m_u.table->keyCount);

    // Get pointers to the enumerable entries in the buffer.
    Entry** p = sortedEnumerables.data();
    unsigned entryCount = m_u.table->keyCount + m_u.table->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; i++) {
        if (m_u.table->entries()[i].key && !(m_u.table->entries()[i].attributes & DontEnum))
            *p++ = &m_u.table->entries()[i];
    }

    // Sort the entries by index.
    qsort(sortedEnumerables.data(), p - sortedEnumerables.data(), sizeof(Entry*), comparePropertyMapEntryIndices);

    // Put the keys of the sorted entries into the list.
    for (Entry** q = sortedEnumerables.data(); q != p; ++q)
        propertyNames.add(Identifier(q[0]->key));
}

void PropertyMap::save(SavedProperties& s) const
{
    unsigned count = 0;

    if (!m_usingTable) {
#if USE_SINGLE_ENTRY
        if (m_singleEntryKey && !(m_singleEntryAttributes & (ReadOnly | Function)))
            ++count;
#endif
    } else {
        unsigned entryCount = m_u.table->keyCount + m_u.table->deletedSentinelCount;
        for (unsigned i = 1; i <= entryCount; ++i)
            if (m_u.table->entries()[i].key && !(m_u.table->entries()[i].attributes & (ReadOnly | Function)))
                ++count;
    }

    s.properties.clear();
    s.count = count;

    if (count == 0)
        return;
    
    s.properties.set(new SavedProperty[count]);
    
    SavedProperty* prop = s.properties.get();
    
#if USE_SINGLE_ENTRY
    if (!m_usingTable) {
        prop->init(m_singleEntryKey, m_u.singleEntryValue, m_singleEntryAttributes);
        return;
    }
#endif

    // Save in the right order so we don't lose the order.
    // Another possibility would be to save the indices.

    // Allocate a buffer to use to sort the keys.
    Vector<Entry*, smallMapThreshold> sortedEntries(count);

    // Get pointers to the entries in the buffer.
    Entry** p = sortedEntries.data();
    unsigned entryCount = m_u.table->keyCount + m_u.table->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; ++i) {
        if (m_u.table->entries()[i].key && !(m_u.table->entries()[i].attributes & (ReadOnly | Function)))
            *p++ = &m_u.table->entries()[i];
    }
    ASSERT(p == sortedEntries.data() + count);

    // Sort the entries by index.
    qsort(sortedEntries.data(), p - sortedEntries.data(), sizeof(Entry*), comparePropertyMapEntryIndices);

    // Put the sorted entries into the saved properties list.
    for (Entry** q = sortedEntries.data(); q != p; ++q, ++prop) {
        Entry* e = *q;
        prop->init(e->key, e->value, e->attributes);
    }
}

void PropertyMap::restore(const SavedProperties& p)
{
    for (unsigned i = 0; i != p.count; ++i)
        put(Identifier(p.properties[i].name()), p.properties[i].value(), p.properties[i].attributes());
}

#if DO_PROPERTYMAP_CONSTENCY_CHECK

void PropertyMap::checkConsistency()
{
    if (!m_usingTable)
        return;

    ASSERT(m_u.table->size >= 16);
    ASSERT(m_u.table->sizeMask);
    ASSERT(m_u.table->size == m_u.table->sizeMask + 1);
    ASSERT(!(m_u.table->size & m_u.table->sizeMask));

    ASSERT(m_u.table->keyCount <= m_u.table->size / 2);
    ASSERT(m_u.table->deletedSentinelCount <= m_u.table->size / 4);

    ASSERT(m_u.table->keyCount + m_u.table->deletedSentinelCount <= m_u.table->size / 2);

    unsigned indexCount = 0;
    unsigned deletedIndexCount = 0;
    for (unsigned a = 0; a != m_u.table->size; ++a) {
        unsigned entryIndex = m_u.table->entryIndicies[a];
        if (entryIndex == emptyEntryIndex)
            continue;
        if (entryIndex == deletedSentinelIndex) {
            ++deletedIndexCount;
            continue;
        }
        ASSERT(entryIndex > deletedSentinelIndex);
        ASSERT(entryIndex - 1 <= m_u.table->keyCount + m_u.table->deletedSentinelCount);
        ++indexCount;

        for (unsigned b = a + 1; b != m_u.table->size; ++b)
            ASSERT(m_u.table->entryIndicies[b] != entryIndex);
    }
    ASSERT(indexCount == m_u.table->keyCount);
    ASSERT(deletedIndexCount == m_u.table->deletedSentinelCount);

    ASSERT(m_u.table->entries()[0].key == 0);

    unsigned nonEmptyEntryCount = 0;
    for (unsigned c = 1; c <= m_u.table->keyCount + m_u.table->deletedSentinelCount; ++c) {
        UString::Rep* rep = m_u.table->entries()[c].key;
        if (!rep) {
            ASSERT(m_u.table->entries()[c].value->isUndefined());
            continue;
        }
        ++nonEmptyEntryCount;
        unsigned i = rep->computedHash();
        unsigned k = 0;
        unsigned entryIndex;
        while (1) {
            entryIndex = m_u.table->entryIndicies[i & m_u.table->sizeMask];
            ASSERT(entryIndex != emptyEntryIndex);
            if (rep == m_u.table->entries()[entryIndex - 1].key)
                break;
            if (k == 0)
                k = 1 | doubleHash(rep->computedHash());
            i += k;
        }
        ASSERT(entryIndex == c + 1);
    }

    ASSERT(nonEmptyEntryCount == m_u.table->keyCount);
}

#endif // DO_PROPERTYMAP_CONSTENCY_CHECK

} // namespace KJS
