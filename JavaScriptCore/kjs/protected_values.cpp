// -*- c-basic-offset: 2 -*-
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#include "protected_values.h"
#include "simple_number.h"

namespace KJS {

const int _minTableSize = 64;

ProtectedValues::KeyValue *ProtectedValues::_table;
int ProtectedValues::_tableSize;
int ProtectedValues::_tableSizeMask;
int ProtectedValues::_keyCount;

int ProtectedValues::getProtectCount(ValueImp *k)
{
    if (!_table)
	return 0;

    if (SimpleNumber::is(k))
      return 0;

    unsigned hash = computeHash(k);
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i].key && _table[i].key != k;
#endif
    while (ValueImp *key = _table[i].key) {
        if (key == k) {
	    return _table[i].value;
	}
        i = (i + 1) & _tableSizeMask;
    }

    return 0;
}


void ProtectedValues::increaseProtectCount(ValueImp *k)
{
    assert(k);

    if (SimpleNumber::is(k))
      return;

    if (!_table)
        expand();
    
    unsigned hash = computeHash(k);
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i].key && _table[i].key != k;
#endif
    while (ValueImp *key = _table[i].key) {
        if (key == k) {
	    _table[i].value++;
	    return;
	}
        i = (i + 1) & _tableSizeMask;
    }
    
    _table[i].key = k;
    _table[i].value = 1;
    ++_keyCount;
    
    if (_keyCount * 2 >= _tableSize)
        expand();
}

inline void ProtectedValues::insert(ValueImp *k, int v)
{
    unsigned hash = computeHash(k);
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i] != 0;
#endif
    while (_table[i].key)
        i = (i + 1) & _tableSizeMask;
    
    _table[i].key = k;
    _table[i].value = v;
}

void ProtectedValues::decreaseProtectCount(ValueImp *k)
{
    assert(k);

    if (SimpleNumber::is(k))
      return;

    unsigned hash = computeHash(k);
    
    ValueImp *key;
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i].key && _table[i].key == k;
#endif
    while ((key = _table[i].key)) {
        if (key == k)
            break;
        i = (i + 1) & _tableSizeMask;
    }
    if (!key)
        return;
    
    _table[i].value--;

    if (_table[i].value != 0)
	return;

    _table[i].key = 0;
    --_keyCount;
    
    if (_keyCount * 6 < _tableSize && _tableSize > _minTableSize) {
        shrink();
        return;
    }
    
    // Reinsert all the items to the right in the same cluster.
    while (1) {
        i = (i + 1) & _tableSizeMask;
        key = _table[i].key;
	int value = _table[i].value;
        if (!key)
            break;
        _table[i].key = 0;
        _table[i].value = 0;
        insert(key, value);
    }
}

void ProtectedValues::expand()
{
    rehash(_tableSize == 0 ? _minTableSize : _tableSize * 2);
}

void ProtectedValues::shrink()
{
    rehash(_tableSize / 2);
}

void ProtectedValues::rehash(int newTableSize)
{
    int oldTableSize = _tableSize;
    KeyValue *oldTable = _table;

    _tableSize = newTableSize;
    _tableSizeMask = newTableSize - 1;
    _table = (KeyValue *)calloc(newTableSize, sizeof(KeyValue));

    for (int i = 0; i != oldTableSize; ++i)
        if (oldTable[i].key)
            insert(oldTable[i].key, oldTable[i].value);

    free(oldTable);
}

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
// or anything like that.
const unsigned PHI = 0x9e3779b9U;

template <int size> static unsigned hash(ValueImp *pointer);

template <> static inline unsigned hash<4>(ValueImp *pointer) 
{
  int a = (int)PHI;
  int b = (int)pointer;
  int c = 0;

  a -= b; a -= c; a ^= (c>>13);
  b -= c; b -= a; b ^= (a<<8); 
  c -= a; c -= b; c ^= (b>>13);
  a -= b; a -= c; a ^= (c>>12);
  b -= c; b -= a; b ^= (a<<16);
  c -= a; c -= b; c ^= (b>>5);
  a -= b; a -= c; a ^= (c>>3);
  b -= c; b -= a; b ^= (a<<10);
  c -= a; c -= b; c ^= (b>>15);
  
  return (unsigned)c;
}

template <> static inline unsigned hash<8>(ValueImp *pointer)
{
  int a = (int)PHI;
  int b = (int)(long)pointer;
  int c = (int)(((long)pointer >> 16) >> 16);

  a -= b; a -= c; a ^= (c>>13);
  b -= c; b -= a; b ^= (a<<8); 
  c -= a; c -= b; c ^= (b>>13);
  a -= b; a -= c; a ^= (c>>12);
  b -= c; b -= a; b ^= (a<<16);
  c -= a; c -= b; c ^= (b>>5);
  a -= b; a -= c; a ^= (c>>3);
  b -= c; b -= a; b ^= (a<<10);
  c -= a; c -= b; c ^= (b>>15);
  
  return (unsigned)c;
}


// This hash algorithm comes from:
// http://burtleburtle.net/bob/hash/hashfaq.html
// http://burtleburtle.net/bob/hash/doobs.html
unsigned ProtectedValues::computeHash(ValueImp *pointer)
{
  return hash<sizeof(ValueImp *)>(pointer);
}

} // namespace
