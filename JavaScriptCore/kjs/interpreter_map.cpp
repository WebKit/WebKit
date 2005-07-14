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

#include "interpreter_map.h"
#include "pointer_hash.h"

namespace KJS {

const int _minTableSize = 64;

InterpreterMap::KeyValue *InterpreterMap::_table;
int InterpreterMap::_tableSize;
int InterpreterMap::_tableSizeMask;
int InterpreterMap::_keyCount;


InterpreterImp * InterpreterMap::getInterpreterForGlobalObject(ObjectImp *global)
{
    if (!_table)
        expand();
    
    unsigned hash = computeHash(global);
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i].key && _table[i].key != global;
#endif
    while (ObjectImp *key = _table[i].key) {
        if (key == global) {
	    return _table[i].value;
	}
        i = (i + 1) & _tableSizeMask;
    }
    
    return 0;
}


void InterpreterMap::setInterpreterForGlobalObject(InterpreterImp *interpreter, ObjectImp *global)
{
    if (!_table)
        expand();
    
    unsigned hash = computeHash(global);
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i].key && _table[i].key != global;
#endif
    while (ObjectImp *key = _table[i].key) {
        if (key == global) {
	    _table[i].value = interpreter;
	    return;
	}
        i = (i + 1) & _tableSizeMask;
    }
    
    _table[i].key = global;
    _table[i].value = interpreter;
    ++_keyCount;
    
    if (_keyCount * 2 >= _tableSize)
        expand();
}

inline void InterpreterMap::insert(InterpreterImp *interpreter, ObjectImp *global)
{
    unsigned hash = computeHash(global);
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i] != 0;
#endif
    while (_table[i].key)
        i = (i + 1) & _tableSizeMask;
    
    _table[i].key = global;
    _table[i].value = interpreter;
}

void InterpreterMap::removeInterpreterForGlobalObject(ObjectImp *global)
{
    unsigned hash = computeHash(global);
    
    ObjectImp *key;
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i].key && _table[i].key == global;
#endif
    while ((key = _table[i].key)) {
        if (key == global)
            break;
        i = (i + 1) & _tableSizeMask;
    }
    if (!key)
        return;
    
    _table[i].key = 0;
    _table[i].value = 0;
    --_keyCount;
    
    if (_keyCount * 6 < _tableSize && _tableSize > _minTableSize) {
        shrink();
        return;
    }
    
    // Reinsert all the items to the right in the same cluster.
    while (1) {
        i = (i + 1) & _tableSizeMask;
        key = _table[i].key;
	InterpreterImp *value = _table[i].value;
        if (!key)
            break;
        _table[i].key = 0;
        _table[i].value = 0;
        insert(value,key);
    }
}

void InterpreterMap::expand()
{
    rehash(_tableSize == 0 ? _minTableSize : _tableSize * 2);
}

void InterpreterMap::shrink()
{
    rehash(_tableSize / 2);
}

void InterpreterMap::rehash(int newTableSize)
{
    int oldTableSize = _tableSize;
    KeyValue *oldTable = _table;

    _tableSize = newTableSize;
    _tableSizeMask = newTableSize - 1;
    _table = (KeyValue *)calloc(newTableSize, sizeof(KeyValue));

    for (int i = 0; i != oldTableSize; ++i)
        if (oldTable[i].key)
            insert(oldTable[i].value, oldTable[i].key);

    free(oldTable);
}

unsigned InterpreterMap::computeHash(ObjectImp *pointer)
{
    return pointerHash(pointer);
}


} // namespace
