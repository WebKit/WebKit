/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Apple Computer, Inc
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

#include "identifier.h"

#define DUMP_STATISTICS 0

namespace KJS {

#if DUMP_STATISTICS

static int numProbes;
static int numCollisions;

struct IdentifierStatisticsExitLogger { ~IdentifierStatisticsExitLogger(); };

static IdentifierStatisticsExitLogger logger;

IdentifierStatisticsExitLogger::~IdentifierStatisticsExitLogger()
{
    printf("\nKJS::Identifier statistics\n\n");
    printf("%d probes\n", numProbes);
    printf("%d collisions (%.1f%%)\n", numCollisions, 100.0 * numCollisions / numProbes);
}

#endif

Identifier Identifier::null;

extern const Identifier argumentsPropertyName("arguments");
extern const Identifier calleePropertyName("callee");
extern const Identifier constructorPropertyName("constructor");
extern const Identifier lengthPropertyName("length");
extern const Identifier messagePropertyName("message");
extern const Identifier namePropertyName("name");
extern const Identifier prototypePropertyName("prototype");
extern const Identifier specialPrototypePropertyName("__proto__");
extern const Identifier toLocaleStringPropertyName("toLocaleString");
extern const Identifier toStringPropertyName("toString");
extern const Identifier valueOfPropertyName("valueOf");

const int _minTableSize = 64;

UString::Rep **Identifier::_table;
int Identifier::_tableSize;
int Identifier::_tableSizeMask;
int Identifier::_keyCount;

bool Identifier::equal(UString::Rep *r, const char *s)
{
    int length = r->len;
    const UChar *d = r->dat;
    for (int i = 0; i != length; ++i)
        if (d[i].uc != (unsigned char)s[i])
            return false;
    return s[length] == 0;
}

bool Identifier::equal(UString::Rep *r, const UChar *s, int length)
{
    if (r->len != length)
        return false;
    const UChar *d = r->dat;
    for (int i = 0; i != length; ++i)
        if (d[i].uc != s[i].uc)
            return false;
    return true;
}

bool Identifier::equal(UString::Rep *r, UString::Rep *b)
{
    int length = r->len;
    if (length != b->len)
        return false;
    const UChar *d = r->dat;
    const UChar *s = b->dat;
    for (int i = 0; i != length; ++i)
        if (d[i].uc != s[i].uc)
            return false;
    return true;
}

UString::Rep *Identifier::add(const char *c)
{
    if (!c)
        return &UString::Rep::null;
    int length = strlen(c);
    if (length == 0)
        return &UString::Rep::empty;
    
    if (!_table)
        expand();
    
    unsigned hash = UString::Rep::computeHash(c);
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i] && !equal(_table[i], c);
#endif
    while (UString::Rep *key = _table[i]) {
        if (equal(key, c))
            return key;
        i = (i + 1) & _tableSizeMask;
    }
    
    UChar *d = new UChar[length];
    for (int j = 0; j != length; j++)
        d[j] = c[j];
    
    UString::Rep *r = new UString::Rep;
    r->dat = d;
    r->len = length;
    r->capacity = UString::Rep::capacityForIdentifier;
    r->rc = 0;
    r->_hash = hash;
    
    _table[i] = r;
    ++_keyCount;
    
    if (_keyCount * 2 >= _tableSize)
        expand();
    
    return r;
}

UString::Rep *Identifier::add(const UChar *s, int length)
{
    if (length == 0)
        return &UString::Rep::empty;
    
    if (!_table)
        expand();
    
    unsigned hash = UString::Rep::computeHash(s, length);
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i] && !equal(_table[i], s, length);
#endif
    while (UString::Rep *key = _table[i]) {
        if (equal(key, s, length))
            return key;
        i = (i + 1) & _tableSizeMask;
    }
    
    UChar *d = new UChar[length];
    for (int j = 0; j != length; j++)
        d[j] = s[j];
    
    UString::Rep *r = new UString::Rep;
    r->dat = d;
    r->len = length;
    r->capacity = UString::Rep::capacityForIdentifier;
    r->rc = 0;
    r->_hash = hash;
    
    _table[i] = r;
    ++_keyCount;
    
    if (_keyCount * 2 >= _tableSize)
        expand();
    
    return r;
}

UString::Rep *Identifier::add(UString::Rep *r)
{
    if (r->capacity == UString::Rep::capacityForIdentifier)
        return r;
    if (r->len == 0)
        return &UString::Rep::empty;
    
    if (!_table)
        expand();
    
    unsigned hash = r->hash();
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i] && !equal(_table[i], r);
#endif
    while (UString::Rep *key = _table[i]) {
        if (equal(key, r))
            return key;
        i = (i + 1) & _tableSizeMask;
    }
    
    r->capacity = UString::Rep::capacityForIdentifier;
    
    _table[i] = r;
    ++_keyCount;
    
    if (_keyCount * 2 >= _tableSize)
        expand();
    
    return r;
}

inline void Identifier::insert(UString::Rep *key)
{
    unsigned hash = key->hash();
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i] != 0;
#endif
    while (_table[i])
        i = (i + 1) & _tableSizeMask;
    
    _table[i] = key;
}

void Identifier::remove(UString::Rep *r)
{
    unsigned hash = r->hash();
    
    UString::Rep *key;
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numProbes;
    numCollisions += _table[i] && equal(_table[i], r);
#endif
    while ((key = _table[i])) {
        if (equal(key, r))
            break;
        i = (i + 1) & _tableSizeMask;
    }
    if (!key)
        return;
    
    _table[i] = 0;
    --_keyCount;
    
    if (_keyCount * 3 < _tableSize && _tableSize > _minTableSize) {
        shrink();
        return;
    }
    
    // Reinsert all the items to the right in the same cluster.
    while (1) {
        i = (i + 1) & _tableSizeMask;
        key = _table[i];
        if (!key)
            break;
        _table[i] = 0;
        insert(key);
    }
}

void Identifier::expand()
{
    rehash(_tableSize == 0 ? _minTableSize : _tableSize * 2);
}

void Identifier::shrink()
{
    rehash(_tableSize / 2);
}

void Identifier::rehash(int newTableSize)
{
    int oldTableSize = _tableSize;
    UString::Rep **oldTable = _table;

    _tableSize = newTableSize;
    _tableSizeMask = newTableSize - 1;
    _table = (UString::Rep **)calloc(newTableSize, sizeof(UString::Rep *));

    for (int i = 0; i != oldTableSize; ++i)
        if (UString::Rep *key = oldTable[i])
            insert(key);

    free(oldTable);
}

} // namespace KJS
