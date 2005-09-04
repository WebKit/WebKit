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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

// For JavaScriptCore we need to avoid having static constructors.
// Our strategy is to declare the global objects with a different type (initialized to 0)
// and then use placement new to initialize the global objects later. This is not completely
// portable, and it would be good to figure out a 100% clean way that still avoids code that
// runs at init time.

#if !WIN32 // Visual C++ can't handle placement new, it seems.
#define AVOID_STATIC_CONSTRUCTORS 1
#endif

#if AVOID_STATIC_CONSTRUCTORS
#define KJS_IDENTIFIER_HIDE_GLOBALS 1
#endif

#include "identifier.h"

#include "fast_malloc.h"
#include <string.h> // for strlen

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

const int _minTableSize = 64;

UString::Rep **Identifier::_table;
int Identifier::_tableSize;
int Identifier::_tableSizeMask;
int Identifier::_keyCount;

bool Identifier::equal(UString::Rep *r, const char *s)
{
    int length = r->len;
    const UChar *d = r->data();
    for (int i = 0; i != length; ++i)
        if (d[i].uc != (unsigned char)s[i])
            return false;
    return s[length] == 0;
}

bool Identifier::equal(UString::Rep *r, const UChar *s, int length)
{
    if (r->len != length)
        return false;
    const UChar *d = r->data();
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
    const UChar *d = r->data();
    const UChar *s = b->data();
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
    
    UChar *d = static_cast<UChar *>(kjs_fast_malloc(sizeof(UChar) * length));
    for (int j = 0; j != length; j++)
        d[j] = c[j];
    
    UString::Rep *r = UString::Rep::create(d, length);
    r->isIdentifier = 1;
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
    
    UChar *d = static_cast<UChar *>(kjs_fast_malloc(sizeof(UChar) * length));
    for (int j = 0; j != length; j++)
        d[j] = s[j];
    
    UString::Rep *r = UString::Rep::create(d, length);
    r->isIdentifier = 1;
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
    if (r->isIdentifier)
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
    
    r->isIdentifier = 1;
    
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
    
    if (_keyCount * 6 < _tableSize && _tableSize > _minTableSize) {
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

// Global constants for property name strings.

#if !AVOID_STATIC_CONSTRUCTORS
    // Define an Identifier in the normal way.
    #define DEFINE_GLOBAL(name, string) extern const Identifier name(string);
#else
    // Define an Identifier-sized array of pointers to avoid static initialization.
    // Use an array of pointers instead of an array of char in case there is some alignment issue.
    #define DEFINE_GLOBAL(name, string) \
        void * name[(sizeof(Identifier) + sizeof(void *) - 1) / sizeof(void *)];
#endif

const char * const nullCString = 0;

DEFINE_GLOBAL(nullIdentifier, nullCString)
DEFINE_GLOBAL(specialPrototypePropertyName, "__proto__")

#define DEFINE_PROPERTY_NAME_GLOBAL(name) DEFINE_GLOBAL(name ## PropertyName, #name)
KJS_IDENTIFIER_EACH_PROPERTY_NAME_GLOBAL(DEFINE_PROPERTY_NAME_GLOBAL)

void Identifier::init()
{
#if AVOID_STATIC_CONSTRUCTORS
    static bool initialized;
    if (!initialized) {
        // Use placement new to initialize the globals.

        new (&nullIdentifier) Identifier(nullCString);
        new (&specialPrototypePropertyName) Identifier("__proto__");

        #define PLACEMENT_NEW_PROPERTY_NAME_GLOBAL(name) new(&name ## PropertyName) Identifier(#name);
        KJS_IDENTIFIER_EACH_PROPERTY_NAME_GLOBAL(PLACEMENT_NEW_PROPERTY_NAME_GLOBAL)

        initialized = true;
    }
#endif
}

} // namespace KJS
