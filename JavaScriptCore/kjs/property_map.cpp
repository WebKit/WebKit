// -*- c-basic-offset: 2 -*-
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

// At the time I added this switch, the optimization still gave a 1.5% performance boost so I couldn't remove it.
#define USE_SINGLE_ENTRY 1

namespace KJS {

class SavedProperty {
public:
    Identifier key;
    Value value;
};

// Algorithm concepts from Algorithms in C++, Sedgewick.

PropertyMap::PropertyMap() : _tableSize(0), _table(0), _keyCount(0)
{
}

PropertyMap::~PropertyMap()
{
#if USE_SINGLE_ENTRY
    UString::Rep *key = _singleEntry.key;
    if (key)
        key->deref();
#endif
    for (int i = 0; i < _tableSize; i++) {
        UString::Rep *key = _table[i].key;
        if (key)
            key->deref();
    }
    free(_table);
}

void PropertyMap::clear()
{
#if USE_SINGLE_ENTRY
    UString::Rep *key = _singleEntry.key;
    if (key) {
        key->deref();
        _singleEntry.key = 0;
    }
#endif
    for (int i = 0; i < _tableSize; i++) {
        UString::Rep *key = _table[i].key;
        if (key) {
            key->deref();
            _table[i].key = 0;
        }
    }
    _keyCount = 0;
}

inline int PropertyMap::hash(const UString::Rep *s) const
{
    return s->hash() & _tableSizeMask;
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
    while (UString::Rep *key = _table[i].key) {
        if (rep == key) {
            attributes = _table[i].attributes;
            return _table[i].value;
        }
        i = (i + 1) & _tableSizeMask;
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
    while (UString::Rep *key = _table[i].key) {
        if (rep == key)
            return _table[i].value;
        i = (i + 1) & _tableSizeMask;
    }
    return 0;
}

void PropertyMap::put(const Identifier &name, ValueImp *value, int attributes)
{
    UString::Rep *rep = name._ustring.rep;
    
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
            _keyCount = 1;
            return;
        }
    }
#endif

    if (_keyCount * 2 >= _tableSize)
        expand();
    
    int i = hash(rep);
    while (UString::Rep *key = _table[i].key) {
        if (rep == key) {
            // Put a new value in an existing hash table entry.
            _table[i].value = value;
            // Attributes are intentionally not updated.
            return;
        }
        i = (i + 1) & _tableSizeMask;
    }
    
    // Create a new hash table entry.
    rep->ref();
    _table[i].key = rep;
    _table[i].value = value;
    _table[i].attributes = attributes;
    ++_keyCount;
}

inline void PropertyMap::insert(UString::Rep *key, ValueImp *value, int attributes)
{
    int i = hash(key);
    while (_table[i].key)
        i = (i + 1) & _tableSizeMask;
    
    _table[i].key = key;
    _table[i].value = value;
    _table[i].attributes = attributes;
}

void PropertyMap::expand()
{
    int oldTableSize = _tableSize;
    Entry *oldTable = _table;

    _tableSize = oldTableSize ? oldTableSize * 2 : 16;
    _tableSizeMask = _tableSize - 1;
    _table = (Entry *)calloc(_tableSize, sizeof(Entry));

#if USE_SINGLE_ENTRY
    UString::Rep *key = _singleEntry.key;
    if (key) {
        insert(key, _singleEntry.value, _singleEntry.attributes);
        _singleEntry.key = 0;
    }
#endif
    
    for (int i = 0; i != oldTableSize; ++i) {
        UString::Rep *key = oldTable[i].key;
        if (key)
            insert(key, oldTable[i].value, oldTable[i].attributes);
    }

    free(oldTable);
}

void PropertyMap::remove(const Identifier &name)
{
    UString::Rep *rep = name._ustring.rep;

    UString::Rep *key;

    if (!_table) {
#if USE_SINGLE_ENTRY
        key = _singleEntry.key;
        if (rep == key) {
            key->deref();
            _singleEntry.key = 0;
            _keyCount = 0;
        }
#endif
        return;
    }

    // Find the thing to remove.
    int i = hash(rep);
    while ((key = _table[i].key)) {
        if (rep == key)
            break;
        i = (i + 1) & _tableSizeMask;
    }
    if (!key)
        return;
    
    // Remove the one key.
    key->deref();
    _table[i].key = 0;
    --_keyCount;
    
    // Reinsert all the items to the right in the same cluster.
    while (1) {
        i = (i + 1) & _tableSizeMask;
        key = _table[i].key;
        if (!key)
            break;
        _table[i].key = 0;
        insert(key, _table[i].value, _table[i].attributes);
    }
}

void PropertyMap::mark() const
{
#if USE_SINGLE_ENTRY
    if (_singleEntry.key) {
        ValueImp *v = _singleEntry.value;
        if (!v->marked())
            v->mark();
    }
#endif
    for (int i = 0; i != _tableSize; ++i) {
        if (_table[i].key) {
            ValueImp *v = _table[i].value;
            if (!v->marked())
                v->mark();
        }
    }
}

void PropertyMap::addEnumerablesToReferenceList(ReferenceList &list, const Object &base) const
{
#if USE_SINGLE_ENTRY
    UString::Rep *key = _singleEntry.key;
    if (key && !(_singleEntry.attributes & DontEnum))
        list.append(Reference(base, Identifier(key)));
#endif
    for (int i = 0; i != _tableSize; ++i) {
        UString::Rep *key = _table[i].key;
        if (key && !(_table[i].attributes & DontEnum))
            list.append(Reference(base, Identifier(key)));
    }
}

void PropertyMap::save(SavedProperties &p) const
{
    int count = 0;

#if USE_SINGLE_ENTRY
    if (_singleEntry.key)
        ++count;
#endif
    for (int i = 0; i != _tableSize; ++i)
        if (_table[i].key && _table[i].attributes == 0)
            ++count;

    delete [] p._properties;
    if (count == 0) {
        p._properties = 0;
        return;
    }
    p._properties = new SavedProperty [count];
    
    SavedProperty *prop = p._properties;
    
#if USE_SINGLE_ENTRY
    if (_singleEntry.key) {
        prop->key = Identifier(_singleEntry.key);
        prop->value = Value(_singleEntry.value);
        ++prop;
    }
#endif
    for (int i = 0; i != _tableSize; ++i)
        if (_table[i].key && _table[i].attributes == 0) {
            prop->key = Identifier(_table[i].key);
            prop->value = Value(_table[i].value);
        }
}

void PropertyMap::restore(const SavedProperties &p)
{
    for (int i = 0; i != p._count; ++i)
        put(p._properties[i].key, p._properties[i].value.imp(), 0);
}

} // namespace KJS
