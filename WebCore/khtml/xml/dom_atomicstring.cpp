/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2004 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

// For KHTML we need to avoid having static constructors.
// Our strategy is to declare the global objects with a different type (initialized to 0)
// and then use placement new to initialize the global objects later. This is not completely
// portable, and it would be good to figure out a 100% clean way that still avoids code that
// runs at init time.

#if APPLE_CHANGES
#define AVOID_STATIC_CONSTRUCTORS 1
#endif

#if AVOID_STATIC_CONSTRUCTORS
#define KHTML_ATOMICSTRING_HIDE_GLOBALS 1
#endif

#include "dom_atomicstring.h"
#include "xml/dom_stringimpl.h"

namespace DOM {
   
#define DUMP_STATISTICS 0

#if DUMP_STATISTICS
    
static int numAccesses;
static int numCollisions;
static int collisionGraph[4096];
static int maxCollisions;
static int numRehashes;
static int numRemoves;
static int numReinserts;

struct AtomicStringStatisticsExitLogger { ~AtomicStringStatisticsExitLogger(); };

static AtomicStringStatisticsExitLogger logger;

AtomicStringStatisticsExitLogger::~AtomicStringStatisticsExitLogger()
{
    printf("\nkhtml::HashTable statistics\n\n");
    printf("%d accesses\n", numAccesses);
    printf("%d total collisions, average %.2f probes per access\n", numCollisions, 1.0 * (numAccesses + numCollisions) / numAccesses);
    printf("longest collision chain: %d\n", maxCollisions);
    for (int i = 1; i <= maxCollisions; i++) {
        printf("  %d lookups with exactly %d collisions (%.2f%% , %.2f%% with this many or more)\n", collisionGraph[i], i, 100.0 * (collisionGraph[i] - collisionGraph[i+1]) / numAccesses, 100.0 * collisionGraph[i] / numAccesses);
    }
    printf("%d rehashes\n", numRehashes);
    printf("%d removes, %d reinserts\n", numRemoves, numReinserts);
}

static void recordCollisionAtCount(int count)
{
    if (count > maxCollisions)
        maxCollisions = count;
    numCollisions++;
    collisionGraph[count]++;
}
#endif

const int _minTableSize = 64;

DOMStringImpl **AtomicString::_table;
int AtomicString::_tableSize;
int AtomicString::_tableSizeMask;
int AtomicString::_keyCount;

bool AtomicString::equal(DOMStringImpl *r, const char *s)
{
    if (!r && !s) return true;
    if (!r || !s) return false;

    int length = r->l;
    const QChar *d = r->s;
    for (int i = 0; i != length; ++i)
        if (d[i] != s[i])
            return false;
    return s[length] == 0;
}

bool AtomicString::equal(const AtomicString &a, const char *b)
{ 
    return equal(a.m_string.implementation(), b); 
}

bool AtomicString::equal(DOMStringImpl *r, const QChar *s, uint length)
{
    if (!r && !s) return true;
    if (!r || !s) return false;
    
    if (r->l != length)
        return false;
    const QChar *d = r->s;
    for (uint i = 0; i != length; ++i)
        if (d[i] != s[i])
            return false;
    return true;
}

bool AtomicString::equal(DOMStringImpl *r, DOMStringImpl *b)
{
    if (r == b) return true;
    if (!r || !b) return false;
    uint length = r->l;
    if (length != b->l)
        return false;
    const QChar *d = r->s;
    const QChar *s = b->s;
    for (uint i = 0; i != length; ++i)
        if (d[i] != s[i])
            return false;
    return true;
}

DOMStringImpl *AtomicString::add(const char *c)
{
    if (!c)
        return 0;
    int length = strlen(c);
    if (length == 0)
        return DOMStringImpl::empty();
    
    if (!_table)
        expand();
    
    unsigned hash = DOMStringImpl::computeHash(c);
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numAccesses;
    int collisionCount = 0;
#endif
    while (DOMStringImpl *key = _table[i]) {
        if (equal(key, c))
            return key;
#if DUMP_STATISTICS
        ++collisionCount;
        recordCollisionAtCount(collisionCount);
#endif
        i = (i + 1) & _tableSizeMask;
    }
    
    DOMStringImpl *r = new DOMStringImpl(c, length);
    r->_hash = hash;
    r->_inTable = true;
    
    _table[i] = r;
    ++_keyCount;
    
    if (_keyCount * 2 >= _tableSize)
        expand();
    
    return r;
}

DOMStringImpl *AtomicString::add(const QChar *s, int length)
{
    if (!s)
        return 0;

    if (length == 0)
        return DOMStringImpl::empty();
    
    if (!_table)
        expand();
    
    unsigned hash = DOMStringImpl::computeHash(s, length);
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numAccesses;
    int collisionCount = 0;
#endif
    while (DOMStringImpl *key = _table[i]) {
        if (equal(key, s, length))
            return key;
#if DUMP_STATISTICS
        ++collisionCount;
        recordCollisionAtCount(collisionCount);
#endif
        i = (i + 1) & _tableSizeMask;
    }
    
    DOMStringImpl *r = new DOMStringImpl(s, length);
    r->_hash = hash;
    r->_inTable = true;
    
    _table[i] = r;
    ++_keyCount;
    
    if (_keyCount * 2 >= _tableSize)
        expand();
    
    return r;
}

DOMStringImpl *AtomicString::add(DOMStringImpl *r)
{
    if (!r || r->_inTable)
        return r;

    if (r->l == 0)
        return DOMStringImpl::empty();
    
    if (!_table)
        expand();
    
    unsigned hash = r->hash();
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numAccesses;
    int collisionCount = 0;
#endif
    while (DOMStringImpl *key = _table[i]) {
        if (equal(key, r)) {
            return key;
        }
#if DUMP_STATISTICS
        ++collisionCount;
        recordCollisionAtCount(collisionCount);
#endif
        i = (i + 1) & _tableSizeMask;
    }

    r->_inTable = true;
    _table[i] = r;
    ++_keyCount;
    
    if (_keyCount * 2 >= _tableSize)
        expand();
    
    return r;
}

inline void AtomicString::insert(DOMStringImpl *key)
{
    unsigned hash = key->hash();
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numAccesses;
    int collisionCount = 0;
#endif
    while (_table[i] && !(key == _table[i])) {
#if DUMP_STATISTICS
        ++collisionCount;
        recordCollisionAtCount(collisionCount);
#endif
        i = (i + 1) & _tableSizeMask;
    }
    _table[i] = key;
}

void AtomicString::remove(DOMStringImpl *r)
{
    unsigned hash = r->_hash;
    
    DOMStringImpl *key;
    
    int i = hash & _tableSizeMask;
#if DUMP_STATISTICS
    ++numRemoves;
    ++numAccesses;
    int collisionCount = 0;
#endif
    while ((key = _table[i])) {
        if (key == r)
            break;
#if DUMP_STATISTICS
        ++collisionCount;
        recordCollisionAtCount(collisionCount);
#endif
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
#if DUMP_STATISTICS
        ++numReinserts;
#endif
        _table[i] = 0;
        insert(key);
    }
}

void AtomicString::expand()
{
    rehash(_tableSize == 0 ? _minTableSize : _tableSize * 2);
}

void AtomicString::shrink()
{
    rehash(_tableSize / 2);
}

void AtomicString::rehash(int newTableSize)
{
    int oldTableSize = _tableSize;
    DOMStringImpl **oldTable = _table;
    
#if DUMP_STATISTICS
    if (oldTableSize != 0)
        ++numRehashes;
#endif

    _tableSize = newTableSize;
    _tableSizeMask = newTableSize - 1;
    _table = (DOMStringImpl **)calloc(newTableSize, sizeof(DOMStringImpl *));
    
    for (int i = 0; i != oldTableSize; ++i)
        if (DOMStringImpl *key = oldTable[i])
            insert(key);
    
    free(oldTable);
}

// Global constants for property name strings.

#if !AVOID_STATIC_CONSTRUCTORS
    // Define an AtomicString in the normal way.
    #define DEFINE_GLOBAL(name, string) extern const AtomicString name ## Atom(string);

    extern const AtomicString nullAtom;
    extern const AtomicString emptyAtom;
#else
    // Define an AtomicString-sized array of pointers to avoid static initialization.
    // Use an array of pointers instead of an array of char in case there is some alignment issue.
    #define DEFINE_GLOBAL(name, string) \
        void * name ## Atom[(sizeof(AtomicString) + sizeof(void *) - 1) / sizeof(void *)];

    DEFINE_GLOBAL(null, ignored)
    DEFINE_GLOBAL(empty, ignored)
#endif

#define CALL_DEFINE_GLOBAL(name) DEFINE_GLOBAL(name, #name)
KHTML_ATOMICSTRING_EACH_GLOBAL(CALL_DEFINE_GLOBAL)

void AtomicString::init()
{
#if AVOID_STATIC_CONSTRUCTORS
    static bool initialized;
    if (!initialized) {
        // Use placement new to initialize the globals.
        new (&nullAtom) AtomicString;
        new (&emptyAtom) AtomicString("");
        
        #define PLACEMENT_NEW_GLOBAL(name, string) new (&name ## PropertyName) AtomicString(string);
        #define CALL_PLACEMENT_NEW_GLOBAL(name) PLACEMENT_NEW_GLOBAL(name, #name)
        KHTML_ATOMICSTRING_EACH_GLOBAL(CALL_PLACEMENT_NEW_GLOBAL)
        initialized = true;
    }
#endif
}

bool operator==(const AtomicString &a, const DOMString &b)
{
    return a.domString() == b;    
}
   
bool equalsIgnoreCase(const AtomicString &as, const DOMString &bs)
{
    // returns true when equal, false otherwise (ignoring case)
    return !strcasecmp(as.domString(), bs);
}

bool equalsIgnoreCase(const AtomicString &as, const AtomicString &bs)
{
    // returns true when equal, false otherwise (ignoring case)
    if (as == bs)
        return true;
    return !strcasecmp(as.domString(), bs.domString());
}

}
