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
#include "PropertyMap.h"

#include "JSObject.h"
#include "PropertyNameArray.h"
#include "protect.h"
#include <algorithm>
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashTable.h>
#include <wtf/Vector.h>

using std::max;
using WTF::doubleHash;

#ifndef NDEBUG
#define DO_PROPERTYMAP_CONSTENCY_CHECK 0
#else
#define DO_PROPERTYMAP_CONSTENCY_CHECK 0
#endif

namespace JSC {

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

struct PropertyMapStatisticsExitLogger {
    ~PropertyMapStatisticsExitLogger();
};

static PropertyMapStatisticsExitLogger logger;

PropertyMapStatisticsExitLogger::~PropertyMapStatisticsExitLogger()
{
    printf("\nJSC::PropertyMap statistics\n\n");
    printf("%d probes\n", numProbes);
    printf("%d collisions (%.1f%%)\n", numCollisions, 100.0 * numCollisions / numProbes);
    printf("%d rehashes\n", numRehashes);
    printf("%d removes\n", numRemoves);
}

#endif

static const unsigned deletedSentinelIndex = 1;

#if !DO_PROPERTYMAP_CONSTENCY_CHECK

inline void PropertyMap::checkConsistency()
{
}

#endif

PropertyMap& PropertyMap::operator=(const PropertyMap& other)
{
    if (other.m_table) {
        size_t tableSize = Table::allocationSize(other.m_table->size);
        m_table = static_cast<Table*>(fastMalloc(tableSize));
        memcpy(m_table, other.m_table, tableSize);

        unsigned entryCount = m_table->keyCount + m_table->deletedSentinelCount;
        for (unsigned i = 1; i <= entryCount; ++i) {
            if (UString::Rep* key = m_table->entries()[i].key)
                key->ref();
        }
    }

    m_deletedOffsets = other.m_deletedOffsets;
    return *this;
}

PropertyMap::~PropertyMap()
{
    if (!m_table)
        return;

    unsigned entryCount = m_table->keyCount + m_table->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; i++) {
        if (UString::Rep* key = m_table->entries()[i].key)
            key->deref();
    }
    fastFree(m_table);
}

size_t PropertyMap::put(const Identifier& propertyName, unsigned attributes)
{
    ASSERT(!propertyName.isNull());
    ASSERT(get(propertyName) == WTF::notFound);

    checkConsistency();

    UString::Rep* rep = propertyName._ustring.rep();

    if (!m_table)
        createTable();

    // FIXME: Consider a fast case for tables with no deleted sentinels.

    unsigned i = rep->computedHash();
    unsigned k = 0;
    bool foundDeletedElement = false;
    unsigned deletedElementIndex = 0; // initialize to make the compiler happy

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    while (1) {
        unsigned entryIndex = m_table->entryIndices[i & m_table->sizeMask];
        if (entryIndex == emptyEntryIndex)
            break;

        if (entryIndex == deletedSentinelIndex) {
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
    unsigned entryIndex = m_table->keyCount + m_table->deletedSentinelCount + 2;
    if (foundDeletedElement) {
        i = deletedElementIndex;
        --m_table->deletedSentinelCount;

        // Since we're not making the table bigger, we can't use the entry one past
        // the end that we were planning on using, so search backwards for the empty
        // slot that we can use. We know it will be there because we did at least one
        // deletion in the past that left an entry empty.
        while (m_table->entries()[--entryIndex - 1].key) { }
    }

    // Create a new hash table entry.
    m_table->entryIndices[i & m_table->sizeMask] = entryIndex;

    // Create a new hash table entry.
    rep->ref();
    m_table->entries()[entryIndex - 1].key = rep;
    m_table->entries()[entryIndex - 1].attributes = attributes;
    m_table->entries()[entryIndex - 1].index = ++m_table->lastIndexUsed;

    unsigned newOffset;
    if (!m_deletedOffsets.isEmpty()) {
        newOffset = m_deletedOffsets.last();
        m_deletedOffsets.removeLast();
    } else
        newOffset = m_table->keyCount;
    m_table->entries()[entryIndex - 1].offset = newOffset;

    ++m_table->keyCount;

    if ((m_table->keyCount + m_table->deletedSentinelCount) * 2 >= m_table->size)
        expand();

    checkConsistency();
    return newOffset;
}

size_t PropertyMap::remove(const Identifier& propertyName)
{
    ASSERT(!propertyName.isNull());

    checkConsistency();

    UString::Rep* rep = propertyName._ustring.rep();

    if (!m_table)
        return WTF::notFound;

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
        entryIndex = m_table->entryIndices[i & m_table->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return WTF::notFound;

        key = m_table->entries()[entryIndex - 1].key;
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
    m_table->entryIndices[i & m_table->sizeMask] = deletedSentinelIndex;

    size_t offset = m_table->entries()[entryIndex - 1].offset;

    key->deref();
    m_table->entries()[entryIndex - 1].key = 0;
    m_table->entries()[entryIndex - 1].attributes = 0;
    m_table->entries()[entryIndex - 1].offset = 0;
    m_deletedOffsets.append(offset);

    ASSERT(m_table->keyCount >= 1);
    --m_table->keyCount;
    ++m_table->deletedSentinelCount;

    if (m_table->deletedSentinelCount * 4 >= m_table->size)
        rehash();

    checkConsistency();
    return offset;
}

size_t PropertyMap::get(const Identifier& propertyName, unsigned& attributes)
{
    ASSERT(!propertyName.isNull());

    if (!m_table)
        return WTF::notFound;

    UString::Rep* rep = propertyName._ustring.rep();

    unsigned i = rep->computedHash();

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    unsigned entryIndex = m_table->entryIndices[i & m_table->sizeMask];
    if (entryIndex == emptyEntryIndex)
        return WTF::notFound;

    if (rep == m_table->entries()[entryIndex - 1].key) {
        attributes = m_table->entries()[entryIndex - 1].attributes;
        return m_table->entries()[entryIndex - 1].offset;
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

        entryIndex = m_table->entryIndices[i & m_table->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return WTF::notFound;

        if (rep == m_table->entries()[entryIndex - 1].key) {
            attributes = m_table->entries()[entryIndex - 1].attributes;
            return m_table->entries()[entryIndex - 1].offset;
        }
    }
}

void PropertyMap::insert(const Entry& entry)
{
    ASSERT(m_table);

    unsigned i = entry.key->computedHash();
    unsigned k = 0;

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    while (1) {
        unsigned entryIndex = m_table->entryIndices[i & m_table->sizeMask];
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

    unsigned entryIndex = m_table->keyCount + 2;
    m_table->entryIndices[i & m_table->sizeMask] = entryIndex;
    m_table->entries()[entryIndex - 1] = entry;

    ++m_table->keyCount;
}

void PropertyMap::expand()
{
    ASSERT(m_table);
    rehash(m_table->size * 2);
}

void PropertyMap::createTable()
{
    const unsigned newTableSize = 16;

    ASSERT(!m_table);

    checkConsistency();

    m_table = static_cast<Table*>(fastZeroedMalloc(Table::allocationSize(newTableSize)));
    m_table->size = newTableSize;
    m_table->sizeMask = newTableSize - 1;

    checkConsistency();
}

void PropertyMap::rehash()
{
    ASSERT(m_table);
    ASSERT(m_table->size);
    rehash(m_table->size);
}

void PropertyMap::rehash(unsigned newTableSize)
{
    ASSERT(m_table);

    checkConsistency();

    Table* oldTable = m_table;

    m_table = static_cast<Table*>(fastZeroedMalloc(Table::allocationSize(newTableSize)));
    m_table->size = newTableSize;
    m_table->sizeMask = newTableSize - 1;

    unsigned lastIndexUsed = 0;
    unsigned entryCount = oldTable->keyCount + oldTable->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; ++i) {
        if (oldTable->entries()[i].key) {
            lastIndexUsed = max(oldTable->entries()[i].index, lastIndexUsed);
            insert(oldTable->entries()[i]);
        }
    }
    m_table->lastIndexUsed = lastIndexUsed;

    fastFree(oldTable);

    checkConsistency();
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

void PropertyMap::getEnumerablePropertyNames(PropertyNameArray& propertyNames) const
{
    if (!m_table)
        return;

    if (m_table->keyCount < tinyMapThreshold) {
        Entry* a[tinyMapThreshold];
        int i = 0;
        unsigned entryCount = m_table->keyCount + m_table->deletedSentinelCount;
        for (unsigned k = 1; k <= entryCount; k++) {
            if (m_table->entries()[k].key && !(m_table->entries()[k].attributes & DontEnum)) {
                Entry* value = &m_table->entries()[k];
                int j;
                for (j = i - 1; j >= 0 && a[j]->index > value->index; --j)
                    a[j + 1] = a[j];
                a[j + 1] = value;
                ++i;
            }
        }
        if (!propertyNames.size()) {
            for (int k = 0; k < i; ++k)
                propertyNames.addKnownUnique(a[k]->key);
        } else {
            for (int k = 0; k < i; ++k)
                propertyNames.add(a[k]->key);
        }

        return;
    }

    // Allocate a buffer to use to sort the keys.
    Vector<Entry*, smallMapThreshold> sortedEnumerables(m_table->keyCount);

    // Get pointers to the enumerable entries in the buffer.
    Entry** p = sortedEnumerables.data();
    unsigned entryCount = m_table->keyCount + m_table->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; i++) {
        if (m_table->entries()[i].key && !(m_table->entries()[i].attributes & DontEnum))
            *p++ = &m_table->entries()[i];
    }

    size_t enumerableCount = p - sortedEnumerables.data();
    // Sort the entries by index.
    qsort(sortedEnumerables.data(), enumerableCount, sizeof(Entry*), comparePropertyMapEntryIndices);
    sortedEnumerables.resize(enumerableCount);

    // Put the keys of the sorted entries into the list.
    if (!propertyNames.size()) {
        for (size_t i = 0; i < sortedEnumerables.size(); ++i)
            propertyNames.addKnownUnique(sortedEnumerables[i]->key);
    } else {
        for (size_t i = 0; i < sortedEnumerables.size(); ++i)
            propertyNames.add(sortedEnumerables[i]->key);
    }
}

#if DO_PROPERTYMAP_CONSTENCY_CHECK

void PropertyMap::checkConsistency()
{
    if (!m_table)
        return;

    ASSERT(m_table->size >= 16);
    ASSERT(m_table->sizeMask);
    ASSERT(m_table->size == m_table->sizeMask + 1);
    ASSERT(!(m_table->size & m_table->sizeMask));

    ASSERT(m_table->keyCount <= m_table->size / 2);
    ASSERT(m_table->deletedSentinelCount <= m_table->size / 4);

    ASSERT(m_table->keyCount + m_table->deletedSentinelCount <= m_table->size / 2);

    unsigned indexCount = 0;
    unsigned deletedIndexCount = 0;
    for (unsigned a = 0; a != m_table->size; ++a) {
        unsigned entryIndex = m_table->entryIndices[a];
        if (entryIndex == emptyEntryIndex)
            continue;
        if (entryIndex == deletedSentinelIndex) {
            ++deletedIndexCount;
            continue;
        }
        ASSERT(entryIndex > deletedSentinelIndex);
        ASSERT(entryIndex - 1 <= m_table->keyCount + m_table->deletedSentinelCount);
        ++indexCount;

        for (unsigned b = a + 1; b != m_table->size; ++b)
            ASSERT(m_table->entryIndices[b] != entryIndex);
    }
    ASSERT(indexCount == m_table->keyCount);
    ASSERT(deletedIndexCount == m_table->deletedSentinelCount);

    ASSERT(m_table->entries()[0].key == 0);

    unsigned nonEmptyEntryCount = 0;
    for (unsigned c = 1; c <= m_table->keyCount + m_table->deletedSentinelCount; ++c) {
        UString::Rep* rep = m_table->entries()[c].key;
        if (!rep)
            continue;
        ++nonEmptyEntryCount;
        unsigned i = rep->computedHash();
        unsigned k = 0;
        unsigned entryIndex;
        while (1) {
            entryIndex = m_table->entryIndices[i & m_table->sizeMask];
            ASSERT(entryIndex != emptyEntryIndex);
            if (rep == m_table->entries()[entryIndex - 1].key)
                break;
            if (k == 0)
                k = 1 | doubleHash(rep->computedHash());
            i += k;
        }
        ASSERT(entryIndex == c + 1);
    }

    ASSERT(nonEmptyEntryCount == m_table->keyCount);
}

#endif // DO_PROPERTYMAP_CONSTENCY_CHECK

} // namespace JSC
