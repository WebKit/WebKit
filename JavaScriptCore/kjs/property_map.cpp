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

namespace KJS {

// Algorithm concepts from Algorithms in C++, Sedgewick.

PropertyMap::PropertyMap() : _tableSize(0), _table(0), _keyCount(0)
{
}

PropertyMap::~PropertyMap()
{
    //printf("key count is %d\n", _keyCount);
    UString::Rep *key = _singleEntry.key;
    if (key)
        key->deref();
    for (int i = 0; i < _tableSize; i++) {
        key = _table[i].key;
        if (key)
            key->deref();
    }
    free(_table);
}

void PropertyMap::clear()
{
    UString::Rep *key = _singleEntry.key;
    if (key) {
        key->deref();
        _singleEntry.key = 0;
    }
    for (int i = 0; i < _tableSize; i++) {
        UString::Rep *key = _table[i].key;
        if (key) {
            key->deref();
            _table[i].key = 0;
        }
    }
    _keyCount = 0;
}

int PropertyMap::hash(const UString::Rep *s) const
{
    int h = 0;
    int length = s->len;
    int prefixLength = length < 8 ? length : 8;
    for (int i = 0; i < prefixLength; i++)
        h = (127 * h + s->dat[i].unicode()) & _tableSizeHashMask;
    int suffixPosition = length < 16 ? 8 : length - 8;
    for (int i = suffixPosition; i < length; i++)
        h = (127 * h + s->dat[i].unicode()) & _tableSizeHashMask;
    return h;
}

bool PropertyMap::keysMatch(const UString::Rep *a, const UString::Rep *b)
{
    if (a == b)
        return true;
    
    int len = a->len;
    if (len != b->len)
        return false;
    
    for (int i = 0; i != len; ++i)
        if (a->dat[i].unicode() != b->dat[i].unicode())
            return false;
    
    return true;
}

ValueImp *PropertyMap::get(const Identifier &name, int &attributes) const
{
    if (!_table) {
        UString::Rep *key = _singleEntry.key;
        if (key && keysMatch(name.rep, key)) {
            attributes = _singleEntry.attributes;
            return _singleEntry.value;
        }
        return 0;
    }
    
    int i = hash(name.rep);
    while (UString::Rep *key = _table[i].key) {
        if (keysMatch(name.rep, key)) {
            attributes = _table[i].attributes;
            return _table[i].value;
        }
        i = (i + 1) & _tableSizeHashMask;
    }
    return 0;
}

ValueImp *PropertyMap::get(const Identifier &name) const
{
    if (!_table) {
        UString::Rep *key = _singleEntry.key;
        if (key && keysMatch(name.rep, key))
            return _singleEntry.value;
        return 0;
    }
    
    int i = hash(name.rep);
    while (UString::Rep *key = _table[i].key) {
        if (keysMatch(name.rep, key))
            return _table[i].value;
        i = (i + 1) & _tableSizeHashMask;
    }
    return 0;
}

void PropertyMap::put(const Identifier &name, ValueImp *value, int attributes)
{
    if (!_table) {
        UString::Rep *key = _singleEntry.key;
        if (key) {
            if (keysMatch(name.rep, key)) {
            	_singleEntry.value = value;
                return;
            }
        } else {
            name.rep->ref();
            _singleEntry.key = name.rep;
            _singleEntry.value = value;
            _singleEntry.attributes = attributes;
            _keyCount = 1;
            return;
        }
    }

    if (_keyCount * 2 >= _tableSize)
        expand();
    
    int i = hash(name.rep);
    while (UString::Rep *key = _table[i].key) {
        if (keysMatch(name.rep, key)) {
            // Put a new value in an existing hash table entry.
            _table[i].value = value;
            // Attributes are intentionally not updated.
            return;
        }
        i = (i + 1) & _tableSizeHashMask;
    }
    
    // Create a new hash table entry.
    name.rep->ref();
    _table[i].key = name.rep;
    _table[i].value = value;
    _table[i].attributes = attributes;
    ++_keyCount;
}

inline void PropertyMap::insert(UString::Rep *key, ValueImp *value, int attributes)
{
    int i = hash(key);
    while (_table[i].key)
        i = (i + 1) & _tableSizeHashMask;
    
    _table[i].key = key;
    _table[i].value = value;
    _table[i].attributes = attributes;
}

void PropertyMap::expand()
{
    int oldTableSize = _tableSize;
    Entry *oldTable = _table;

    _tableSize = oldTableSize ? oldTableSize * 2 : 16;
    _tableSizeHashMask = _tableSize - 1;
    _table = (Entry *)calloc(_tableSize, sizeof(Entry));

    UString::Rep *key = _singleEntry.key;
    if (key) {
        insert(key, _singleEntry.value, _singleEntry.attributes);
        _singleEntry.key = 0;
    }
    
    for (int i = 0; i != oldTableSize; ++i) {
        key = oldTable[i].key;
        if (key)
            insert(key, oldTable[i].value, oldTable[i].attributes);
    }

    free(oldTable);
}

void PropertyMap::remove(const Identifier &name)
{
    UString::Rep *key;

    if (!_table) {
        key = _singleEntry.key;
        if (key && keysMatch(name.rep, key)) {
            key->deref();
            _singleEntry.key = 0;
            _keyCount = 0;
        }
        return;
    }

    // Find the thing to remove.
    int i = hash(name.rep);
    while ((key = _table[i].key)) {
        if (keysMatch(name.rep, key))
            break;
        i = (i + 1) & _tableSizeHashMask;
    }
    if (!key)
        return;
    
    // Remove the one key.
    key->deref();
    _table[i].key = 0;
    --_keyCount;
    
    // Reinsert all the items to the right in the same cluster.
    while (1) {
        i = (i + 1) & _tableSizeHashMask;
        key = _table[i].key;
        if (!key)
            break;
        _table[i].key = 0;
        insert(key, _table[i].value, _table[i].attributes);
    }
}

void PropertyMap::mark() const
{
    if (_singleEntry.key) {
        ValueImp *v = _singleEntry.value;
        if (!v->marked())
            v->mark();
    }
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
    UString::Rep *key = _singleEntry.key;
    if (key && !(_singleEntry.attributes & DontEnum))
        list.append(Reference(base, Identifier(key)));
    for (int i = 0; i != _tableSize; ++i) {
        UString::Rep *key = _table[i].key;
        if (key && !(_table[i].attributes & DontEnum))
            list.append(Reference(base, Identifier(key)));
    }
}

} // namespace KJS
