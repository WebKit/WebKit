/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#include "property_map.h"

#include "object.h"
#include "reference_list.h"

#define DEBUG_PROPERTIES 0
#define DO_CONSISTENCY_CHECK 0
#define DUMP_STATISTICS 0
#define USE_SINGLE_ENTRY 1

// At the time I added USE_SINGLE_ENTRY, the optimization still gave a 1.5% performance boost so I couldn't remove it.

#if !DO_CONSISTENCY_CHECK
#define checkConsistency() ((void)0)
#endif

namespace KJS {

#if DUMP_STATISTICS

static int numProbes;
static int numCollisions;

struct PropertyMapStatisticsExitLogger { ~PropertyMapStatisticsExitLogger(); };

static PropertyMapStatisticsExitLogger logger;

PropertyMapStatisticsExitLogger::~PropertyMapStatisticsExitLogger()
{
    printf("\nKJS::PropertyMap statistics\n\n");
    printf("%d probes\n", numProbes);
    printf("%d collisions (%.1f%%)\n", numCollisions, 100.0 * numCollisions / numProbes);
}

#endif

struct PropertyMapHashTable
{
    int sizeMask;
    int size;
    int keyCount;
    PropertyMapHashTableEntry entries[1];
};

class SavedProperty {
public:
    Identifier key;
    Value value;
    int attributes;
};

SavedProperties::SavedProperties() : _count(0), _properties(0) { }

SavedProperties::~SavedProperties()
{
    delete [] _properties;
}

// Algorithm concepts from Algorithms in C++, Sedgewick.

PropertyMap::PropertyMap() : _table(0)
{
}

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
    
    for (int i = 0; i < _table->size; i++) {
        UString::Rep *key = _table->entries[i].key;
        if (key)
	  key->deref();
    }
    free(_table);
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

    for (int i = 0; i < _table->size; i++) {
        UString::Rep *key = _table->entries[i].key;
        if (key) {
            key->deref();
            _table->entries[i].key = 0;
        }
    }
    _table->keyCount = 0;
}

inline int PropertyMap::hash(const UString::Rep *s) const
{
    return s->hash() & _table->sizeMask;
}

ValueImp *PropertyMap::get(const Identifier &name, int &attributes) const
{
    UString::Rep *rep = name._ustring.rep;
    
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
    
    int i = hash(rep);
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table->entries[i].key && _table->entries[i].key != rep;
#endif
    while (UString::Rep *key = _table->entries[i].key) {
        if (rep == key) {
            attributes = _table->entries[i].attributes;
            return _table->entries[i].value;
        }
        i = (i + 1) & _table->sizeMask;
    }
    return 0;
}

ValueImp *PropertyMap::get(const Identifier &name) const
{
    UString::Rep *rep = name._ustring.rep;

    if (!_table) {
#if USE_SINGLE_ENTRY
        UString::Rep *key = _singleEntry.key;
        if (rep == key)
            return _singleEntry.value;
#endif
        return 0;
    }
    
    int i = hash(rep);
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table->entries[i].key && _table->entries[i].key != rep;
#endif
    while (UString::Rep *key = _table->entries[i].key) {
        if (rep == key)
            return _table->entries[i].value;
        i = (i + 1) & _table->sizeMask;
    }
    return 0;
}

#if DEBUG_PROPERTIES
static void printAttributes(int attributes)
{
    if (attributes == 0)
        printf ("None ");
    if (attributes & ReadOnly)
        printf ("ReadOnly ");
    if (attributes & DontEnum)
        printf ("DontEnum ");
    if (attributes & DontDelete)
        printf ("DontDelete ");
    if (attributes & Internal)
        printf ("Internal ");
    if (attributes & Function)
        printf ("Function ");
}
#endif

void PropertyMap::put(const Identifier &name, ValueImp *value, int attributes)
{
    checkConsistency();

    UString::Rep *rep = name._ustring.rep;
    
#if DEBUG_PROPERTIES
    printf ("adding property %s, attributes = 0x%08x (", name.ascii(), attributes);
    printAttributes(attributes);
    printf (")\n");
#endif
    
#if USE_SINGLE_ENTRY
    if (!_table) {
        UString::Rep *key = _singleEntry.key;
        if (key) {
            if (rep == key) {
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
    
    int i = hash(rep);
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table->entries[i].key && _table->entries[i].key != rep;
#endif
    while (UString::Rep *key = _table->entries[i].key) {
        if (rep == key) {
            // Put a new value in an existing hash table entry.
            _table->entries[i].value = value;
            // Attributes are intentionally not updated.
            return;
        }
        i = (i + 1) & _table->sizeMask;
    }
    
    // Create a new hash table entry.
    rep->ref();
    _table->entries[i].key = rep;
    _table->entries[i].value = value;
    _table->entries[i].attributes = attributes;
    ++_table->keyCount;

    checkConsistency();
}

inline void PropertyMap::insert(UString::Rep *key, ValueImp *value, int attributes)
{
    assert(_table);

    int i = hash(key);
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table->entries[i].key && _table->entries[i].key != key;
#endif
    while (_table->entries[i].key)
        i = (i + 1) & _table->sizeMask;
    
    _table->entries[i].key = key;
    _table->entries[i].value = value;
    _table->entries[i].attributes = attributes;
}

void PropertyMap::expand()
{
    checkConsistency();
    
    Table *oldTable = _table;
    int oldTableSize = oldTable ? oldTable->size : 0;

    int newTableSize = oldTableSize ? oldTableSize * 2 : 16;
    _table = (Table *)calloc(1, sizeof(Table) + (newTableSize - 1) * sizeof(Entry) );
    _table->size = newTableSize;
    _table->sizeMask = newTableSize - 1;

#if USE_SINGLE_ENTRY
    UString::Rep *key = _singleEntry.key;
    if (key) {
        insert(key, _singleEntry.value, _singleEntry.attributes);
        _singleEntry.key = 0;
    }
#endif
    
    for (int i = 0; i != oldTableSize; ++i) {
        UString::Rep *key = oldTable->entries[i].key;
        if (key)
            insert(key, oldTable->entries[i].value, oldTable->entries[i].attributes);
    }

    free(oldTable);

    checkConsistency();
}

void PropertyMap::remove(const Identifier &name)
{
    checkConsistency();

    UString::Rep *rep = name._ustring.rep;

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
    int i = hash(rep);
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table->entries[i].key && _table->entries[i].key != rep;
#endif
    while ((key = _table->entries[i].key)) {
        if (rep == key)
            break;
        i = (i + 1) & _table->sizeMask;
    }
    if (!key)
        return;
    
    // Remove the one key.
    key->deref();
    _table->entries[i].key = 0;
    assert(_table->keyCount >= 1);
    --_table->keyCount;
    
    // Reinsert all the items to the right in the same cluster.
    while (1) {
        i = (i + 1) & _table->sizeMask;
        key = _table->entries[i].key;
        if (!key)
            break;
        _table->entries[i].key = 0;
        insert(key, _table->entries[i].value, _table->entries[i].attributes);
    }

    checkConsistency();
}

void PropertyMap::mark() const
{
    if (!_table) {
#if USE_SINGLE_ENTRY
        if (_singleEntry.key) {
            ValueImp *v = _singleEntry.value;
            if (!v->marked())
                v->mark();
        }
#endif
        return;
    }

    for (int i = 0; i != _table->size; ++i) {
        if (_table->entries[i].key) {
            ValueImp *v = _table->entries[i].value;
            if (!v->marked())
                v->mark();
        }
    }
}

void PropertyMap::addEnumerablesToReferenceList(ReferenceList &list, const Object &base) const
{
    if (!_table) {
#if USE_SINGLE_ENTRY
        UString::Rep *key = _singleEntry.key;
        if (key && !(_singleEntry.attributes & DontEnum))
            list.append(Reference(base, Identifier(key)));
#endif
        return;
    }

    for (int i = 0; i != _table->size; ++i) {
        UString::Rep *key = _table->entries[i].key;
        if (key && !(_table->entries[i].attributes & DontEnum))
            list.append(Reference(base, Identifier(key)));
    }
}

void PropertyMap::addSparseArrayPropertiesToReferenceList(ReferenceList &list, const Object &base) const
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

    for (int i = 0; i != _table->size; ++i) {
        UString::Rep *key = _table->entries[i].key;
        if (key) {
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
        if (_singleEntry.key && !(_singleEntry.attributes & (ReadOnly | DontEnum | Function)))
            ++count;
#endif
    } else {
        for (int i = 0; i != _table->size; ++i)
            if (_table->entries[i].key && !(_table->entries[i].attributes & (ReadOnly | DontEnum | Function)))
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
        if (_singleEntry.key && !(_singleEntry.attributes & (ReadOnly | DontEnum | Function))) {
            prop->key = Identifier(_singleEntry.key);
            prop->value = Value(_singleEntry.value);
            prop->attributes = _singleEntry.attributes;
            ++prop;
        }
#endif
    } else {
        for (int i = 0; i != _table->size; ++i) {
            if (_table->entries[i].key && !(_table->entries[i].attributes & (ReadOnly | DontEnum | Function))) {
                prop->key = Identifier(_table->entries[i].key);
                prop->value = Value(_table->entries[i].value);
                prop->attributes = _table->entries[i].attributes;
                ++prop;
            }
        }
    }
}

void PropertyMap::restore(const SavedProperties &p)
{
    for (int i = 0; i != p._count; ++i)
        put(p._properties[i].key, p._properties[i].value.imp(), p._properties[i].attributes);
}

#if DO_CONSISTENCY_CHECK

void PropertyMap::checkConsistency()
{
    if (!_table)
        return;

    int count = 0;
    for (int j = 0; j != _table->size; ++j) {
        UString::Rep *rep = _table->entries[j].key;
        if (!rep)
            continue;
        int i = hash(rep);
        while (UString::Rep *key = _table->entries[i].key) {
            if (rep == key)
                break;
            i = (i + 1) & _tableSizeMask;
        }
        assert(i == j);
        count++;
    }
    assert(count == _table->keyCount);
    assert(_table->size >= 16);
    assert(_table->sizeMask);
    assert(_table->size == _table->sizeMask + 1);
}

#endif // DO_CONSISTENCY_CHECK

} // namespace KJS
