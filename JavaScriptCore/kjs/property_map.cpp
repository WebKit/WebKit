/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "property_map.h"

#include <kxmlcore/FastMalloc.h>
#include "object.h"
#include "protect.h"
#include "reference_list.h"

#include <algorithm>

using std::max;

#define DEBUG_PROPERTIES 0
#define DO_CONSISTENCY_CHECK 0
#define DUMP_STATISTICS 0
#define USE_SINGLE_ENTRY 1

// At the time I added USE_SINGLE_ENTRY, the optimization still gave a 1.5%
// performance boost to the iBench JavaScript benchmark so I didn't remove it.

// FIXME: _singleEntry.index is unused.

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
// were inserted into the property map. It's vital that addEnumerablesToReferenceList
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

SavedProperties::SavedProperties() : _count(0), _properties(0) { }

SavedProperties::~SavedProperties()
{
    delete [] _properties;
}

// Algorithm concepts from Algorithms in C++, Sedgewick.

PropertyMap::~PropertyMap()
{
    if (!_table) {
#if USE_SINGLE_ENTRY
        UString::Rep *key = _singleEntry.key;
        if (key)
            key->deref();
#endif
        return;
    }
    
    int minimumKeysToProcess = _table->keyCount + _table->sentinelCount;
    Entry *entries = _table->entries;
    for (int i = 0; i < minimumKeysToProcess; i++) {
        UString::Rep *key = entries[i].key;
        if (key)
            key->deref();
        else
            ++minimumKeysToProcess;
    }
    fastFree(_table);
}

void PropertyMap::clear()
{
    if (!_table) {
#if USE_SINGLE_ENTRY
        UString::Rep *key = _singleEntry.key;
        if (key) {
            key->deref();
            _singleEntry.key = 0;
        }
#endif
        return;
    }

    int size = _table->size;
    Entry *entries = _table->entries;
    for (int i = 0; i < size; i++) {
        UString::Rep *key = entries[i].key;
        if (key) {
            key->deref();
            entries[i].key = 0;
            entries[i].value = 0;
        }
    }
    _table->keyCount = 0;
    _table->sentinelCount = 0;
}

JSValue *PropertyMap::get(const Identifier &name, int &attributes) const
{
    assert(!name.isNull());
    
    UString::Rep *rep = name._ustring.rep();
    
    if (!_table) {
#if USE_SINGLE_ENTRY
        UString::Rep *key = _singleEntry.key;
        if (rep == key) {
            attributes = _singleEntry.attributes;
            return _singleEntry.value;
        }
#endif
        return 0;
    }
    
    unsigned h = rep->hash();
    int sizeMask = _table->sizeMask;
    Entry *entries = _table->entries;
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

    if (!_table) {
#if USE_SINGLE_ENTRY
        UString::Rep *key = _singleEntry.key;
        if (rep == key)
            return _singleEntry.value;
#endif
        return 0;
    }
    
    unsigned h = rep->hash();
    int sizeMask = _table->sizeMask;
    Entry *entries = _table->entries;
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

    if (!_table) {
#if USE_SINGLE_ENTRY
        UString::Rep *key = _singleEntry.key;
        if (rep == key)
            return &_singleEntry.value;
#endif
        return 0;
    }
    
    unsigned h = rep->hash();
    int sizeMask = _table->sizeMask;
    Entry *entries = _table->entries;
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
    if (!_table) {
        UString::Rep *key = _singleEntry.key;
        if (key) {
            if (rep == key && !(roCheck && (_singleEntry.attributes & ReadOnly))) {
                _singleEntry.value = value;
                return;
            }
        } else {
            rep->ref();
            _singleEntry.key = rep;
            _singleEntry.value = value;
            _singleEntry.attributes = attributes;
            checkConsistency();
            return;
        }
    }
#endif

    if (!_table || _table->keyCount * 2 >= _table->size)
        expand();
    
    unsigned h = rep->hash();
    int sizeMask = _table->sizeMask;
    Entry *entries = _table->entries;
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
            if (roCheck && (_table->entries[i].attributes & ReadOnly)) 
                return;
            // Put a new value in an existing hash table entry.
            entries[i].value = value;
            // Attributes are intentionally not updated.
            return;
        }
        // If we find the deleted-element sentinel, remember it for use later.
        if (key == &UString::Rep::null && !foundDeletedElement) {
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
        entries[i].key->deref();
        --_table->sentinelCount;
    }

    // Create a new hash table entry.
    rep->ref();
    entries[i].key = rep;
    entries[i].value = value;
    entries[i].attributes = attributes;
    entries[i].index = ++_table->lastIndexUsed;
    ++_table->keyCount;

    checkConsistency();
}

void PropertyMap::insert(UString::Rep *key, JSValue *value, int attributes, int index)
{
    assert(_table);

    unsigned h = key->hash();
    int sizeMask = _table->sizeMask;
    Entry *entries = _table->entries;
    int i = h & sizeMask;
    int k = 0;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += entries[i].key && entries[i].key != key;
#endif
    while (entries[i].key) {
        assert(entries[i].key != &UString::Rep::null);
        if (k == 0)
            k = 1 | (h % sizeMask);
        i = (i + k) & sizeMask;
#if DUMP_STATISTICS
        ++numRehashes;
#endif
    }
    
    entries[i].key = key;
    entries[i].value = value;
    entries[i].attributes = attributes;
    entries[i].index = index;
}

void PropertyMap::expand()
{
    Table *oldTable = _table;
    int oldTableSize = oldTable ? oldTable->size : 0;    
    rehash(oldTableSize ? oldTableSize * 2 : 16);
}

void PropertyMap::rehash()
{
    assert(_table);
    assert(_table->size);
    rehash(_table->size);
}

void PropertyMap::rehash(int newTableSize)
{
    checkConsistency();
    
    Table *oldTable = _table;
    int oldTableSize = oldTable ? oldTable->size : 0;
    int oldTableKeyCount = oldTable ? oldTable->keyCount : 0;
    
    _table = (Table *)fastCalloc(1, sizeof(Table) + (newTableSize - 1) * sizeof(Entry) );
    _table->size = newTableSize;
    _table->sizeMask = newTableSize - 1;
    _table->keyCount = oldTableKeyCount;

#if USE_SINGLE_ENTRY
    UString::Rep *key = _singleEntry.key;
    if (key) {
        insert(key, _singleEntry.value, _singleEntry.attributes, 0);
        _singleEntry.key = 0;
        // update the count, because single entries don't count towards
        // the table key count
        ++_table->keyCount;
        assert(_table->keyCount == 1);
    }
#endif
    
    int lastIndexUsed = 0;
    for (int i = 0; i != oldTableSize; ++i) {
        Entry &entry = oldTable->entries[i];
        UString::Rep *key = entry.key;
        if (key) {
            // Don't copy deleted-element sentinels.
            if (key == &UString::Rep::null)
                key->deref();
            else {
                int index = entry.index;
                lastIndexUsed = max(index, lastIndexUsed);
                insert(key, entry.value, entry.attributes, index);
            }
        }
    }
    _table->lastIndexUsed = lastIndexUsed;

    fastFree(oldTable);

    checkConsistency();
}

void PropertyMap::remove(const Identifier &name)
{
    assert(!name.isNull());
    
    checkConsistency();

    UString::Rep *rep = name._ustring.rep();

    UString::Rep *key;

    if (!_table) {
#if USE_SINGLE_ENTRY
        key = _singleEntry.key;
        if (rep == key) {
            key->deref();
            _singleEntry.key = 0;
            checkConsistency();
        }
#endif
        return;
    }

    // Find the thing to remove.
    unsigned h = rep->hash();
    int sizeMask = _table->sizeMask;
    Entry *entries = _table->entries;
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
    
    // Replace this one element with the deleted sentinel,
    // &UString::Rep::null; also set value to 0 and attributes to DontEnum
    // to help callers that iterate all keys not have to check for the sentinel.
    key->deref();
    key = &UString::Rep::null;
    key->ref();
    entries[i].key = key;
    entries[i].value = 0;
    entries[i].attributes = DontEnum;
    assert(_table->keyCount >= 1);
    --_table->keyCount;
    ++_table->sentinelCount;
    
    if (_table->sentinelCount * 4 >= _table->size)
        rehash();

    checkConsistency();
}

void PropertyMap::mark() const
{
    if (!_table) {
#if USE_SINGLE_ENTRY
        if (_singleEntry.key) {
            JSValue *v = _singleEntry.value;
            if (!v->marked())
                v->mark();
        }
#endif
        return;
    }

    int minimumKeysToProcess = _table->keyCount;
    Entry *entries = _table->entries;
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
    if (!_table) {
#if USE_SINGLE_ENTRY
        return _singleEntry.attributes & GetterSetter;
#endif
        return false;
    }

    for (int i = 0; i != _table->size; ++i) {
        if (_table->entries[i].attributes & GetterSetter)
            return true;
    }
    
    return false;
}

void PropertyMap::addEnumerablesToReferenceList(ReferenceList &list, JSObject *base) const
{
    if (!_table) {
#if USE_SINGLE_ENTRY
        UString::Rep *key = _singleEntry.key;
        if (key && !(_singleEntry.attributes & DontEnum))
            list.append(Reference(base, Identifier(key)));
#endif
        return;
    }

    // Allocate a buffer to use to sort the keys.
    Entry *fixedSizeBuffer[smallMapThreshold];
    Entry **sortedEnumerables;
    if (_table->keyCount <= smallMapThreshold)
        sortedEnumerables = fixedSizeBuffer;
    else
        sortedEnumerables = new Entry *[_table->keyCount];

    // Get pointers to the enumerable entries in the buffer.
    Entry **p = sortedEnumerables;
    int size = _table->size;
    Entry *entries = _table->entries;
    for (int i = 0; i != size; ++i) {
        Entry *e = &entries[i];
        if (e->key && !(e->attributes & DontEnum))
            *p++ = e;
    }

    // Sort the entries by index.
    qsort(sortedEnumerables, p - sortedEnumerables, sizeof(sortedEnumerables[0]), comparePropertyMapEntryIndices);

    // Put the keys of the sorted entries into the reference list.
    Entry **q = sortedEnumerables;
    while (q != p)
        list.append(Reference(base, Identifier((*q++)->key)));

    // Deallocate the buffer.
    if (sortedEnumerables != fixedSizeBuffer)
        delete [] sortedEnumerables;
}

void PropertyMap::addSparseArrayPropertiesToReferenceList(ReferenceList &list, JSObject *base) const
{
    if (!_table) {
#if USE_SINGLE_ENTRY
        UString::Rep *key = _singleEntry.key;
        if (key) {
            UString k(key);
            bool fitsInUInt32;
            k.toUInt32(&fitsInUInt32);
            if (fitsInUInt32)
                list.append(Reference(base, Identifier(key)));
        }
#endif
        return;
    }

    int size = _table->size;
    Entry *entries = _table->entries;
    for (int i = 0; i != size; ++i) {
        UString::Rep *key = entries[i].key;
        if (key && key != &UString::Rep::null) {
            UString k(key);
            bool fitsInUInt32;
            k.toUInt32(&fitsInUInt32);
            if (fitsInUInt32)
                list.append(Reference(base, Identifier(key)));
        }
    }
}

void PropertyMap::save(SavedProperties &p) const
{
    int count = 0;

    if (!_table) {
#if USE_SINGLE_ENTRY
        if (_singleEntry.key && !(_singleEntry.attributes & (ReadOnly | Function)))
            ++count;
#endif
    } else {
        int size = _table->size;
        Entry *entries = _table->entries;
        for (int i = 0; i != size; ++i)
            if (entries[i].key && !(entries[i].attributes & (ReadOnly | Function)))
                ++count;
    }

    delete [] p._properties;

    p._count = count;

    if (count == 0) {
        p._properties = 0;
        return;
    }
    
    p._properties = new SavedProperty [count];
    
    SavedProperty *prop = p._properties;
    
    if (!_table) {
#if USE_SINGLE_ENTRY
        if (_singleEntry.key && !(_singleEntry.attributes & (ReadOnly | Function))) {
            prop->key = Identifier(_singleEntry.key);
            prop->value = _singleEntry.value;
            prop->attributes = _singleEntry.attributes;
            ++prop;
        }
#endif
    } else {
        // Save in the right order so we don't lose the order.
        // Another possibility would be to save the indices.

        // Allocate a buffer to use to sort the keys.
        Entry *fixedSizeBuffer[smallMapThreshold];
        Entry **sortedEntries;
        if (count <= smallMapThreshold)
            sortedEntries = fixedSizeBuffer;
        else
            sortedEntries = new Entry *[count];

        // Get pointers to the entries in the buffer.
        Entry **p = sortedEntries;
        int size = _table->size;
        Entry *entries = _table->entries;
        for (int i = 0; i != size; ++i) {
            Entry *e = &entries[i];
            if (e->key && !(e->attributes & (ReadOnly | Function)))
                *p++ = e;
        }
        assert(p - sortedEntries == count);

        // Sort the entries by index.
        qsort(sortedEntries, p - sortedEntries, sizeof(sortedEntries[0]), comparePropertyMapEntryIndices);

        // Put the sorted entries into the saved properties list.
        Entry **q = sortedEntries;
        while (q != p) {
            Entry *e = *q++;
            prop->key = Identifier(e->key);
            prop->value = e->value;
            prop->attributes = e->attributes;
            ++prop;
        }

        // Deallocate the buffer.
        if (sortedEntries != fixedSizeBuffer)
            delete [] sortedEntries;
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
    if (!_table)
        return;

    int count = 0;
    int sentinelCount = 0;
    for (int j = 0; j != _table->size; ++j) {
        UString::Rep *rep = _table->entries[j].key;
        if (!rep)
            continue;
        if (rep == &UString::Rep::null) {
            ++sentinelCount;
            continue;
        }
        unsigned h = rep->hash();
        int i = h & _table->sizeMask;
        int k = 0;
        while (UString::Rep *key = _table->entries[i].key) {
            if (rep == key)
                break;
            if (k == 0)
                k = 1 | (h % _table->sizeMask);
            i = (i + k) & _table->sizeMask;
        }
        assert(i == j);
        ++count;
    }
    assert(count == _table->keyCount);
    assert(sentinelCount == _table->sentinelCount);
    assert(_table->size >= 16);
    assert(_table->sizeMask);
    assert(_table->size == _table->sizeMask + 1);
}

#endif // DO_CONSISTENCY_CHECK

} // namespace KJS
