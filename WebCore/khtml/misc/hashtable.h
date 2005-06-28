/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "main_thread_malloc.h"

namespace khtml {

#define DUMP_HASHTABLE_STATS 0

#if DUMP_HASHTABLE_STATS
    
struct HashTableStats {
    ~HashTableStats(); 
    static int numAccesses;
    static int numCollisions;
    static int collisionGraph[4096];
    static int maxCollisions;
    static int numRehashes;
    static int numRemoves;
    static int numReinserts;
    static void recordCollisionAtCount(int count);
};

#endif

// FIXME: this template actually depends on Key being a pointer type in two ways:
// it compares to 0 to check for empty slots, and assigns to 0 when deleting.

// FIXME: add Iterator type, along with begin() and end() methods, change insert and
// find to return Iterator

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
class HashTable {
 public:
    typedef Key KeyType;

    HashTable() : m_table(0), m_tableSize(0), m_tableSizeMask(0), m_keyCount(0) {}
    ~HashTable() { main_thread_free(m_table); }

    HashTable(const HashTable& other);
    void swap(const HashTable& other);
    HashTable& operator=(const HashTable& other);

    int size() const { return m_keyCount; }

    KeyType insert(const KeyType& key);

    // a special version of insert() that finds the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion if the object is already
    // in the table
    template<typename T, unsigned HashT(const T&), bool EqualT(const KeyType&, const T&), KeyType ConvertT(const T&, unsigned)> KeyType insert(const T& key);

#if 0
    Iterator find(const KeyType& key)
    {
        if (!m_table)
            return 0;
        
        unsigned h = hash(key);
        int i = h & m_tableSizeMask;
        
        KeyType *item;
        while (*(item = m_table +i)) {
            if (equal(*item, key))
                return item;
            i = (i + 1) & m_tableSizeMask;
        }
        
        return 0;
    }
#endif

    void remove(const KeyType& key);

 private:
    unsigned hash(const KeyType& key) const { return Hash(key); }
    bool equal(const KeyType& a, const KeyType& b) const { return Equal(a, b); }                    
    void expand() { rehash(m_tableSize == 0 ? m_minTableSize : m_tableSize * 2); }

    void shrink() { rehash(m_tableSize / 2); }

    void rehash(int newTableSize);

    void reinsert(const KeyType& key);

    bool isEmptyBucket(const KeyType& key) { return key == 0; }
    void clearBucket(KeyType& key) { key = 0;}

    static const int m_minTableSize = 64;

    KeyType *m_table;
    int m_tableSize;
    int m_tableSizeMask;
    int m_keyCount;
};

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
Key HashTable<Key, Hash, Equal>::insert(const Key& key)
{
    if (!m_table)
        expand();
    
    unsigned h = hash(key);
    int i = h & m_tableSizeMask;
#if DUMP_HASHTABLE_STATS
    ++HashTableStats::numAccesses;
    int collisionCount = 0;
#endif
    
    KeyType entry;
    while (!isEmptyBucket(entry = m_table[i])) {
        if (equal(entry, key))
            return entry;
#if DUMP_HASHTABLE_STATS
        ++collisionCount;
        HashTableStats::recordCollisionAtCount(collisionCount);
#endif
        i = (i + 1) & m_tableSizeMask;
    }
    
    m_table[i] = key;
    ++m_keyCount;
    
    if (m_keyCount * 2 >= m_tableSize)
        expand();
    
    return key;
}
 
template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
template<typename T, unsigned HashT(const T&), bool EqualT(const Key&, const T&), Key ConvertT(const T&, unsigned)> 
Key HashTable<Key, Hash, Equal>::insert(const T& key)
{
    if (!m_table)
        expand();
    
    unsigned h = HashT(key);
    int i = h & m_tableSizeMask;
#if DUMP_HASHTABLE_STATS
    ++HashTableStats::numAccesses;
    int collisionCount = 0;
#endif
    
    KeyType entry;
    while (!isEmptyBucket(entry = m_table[i])) {
        if (EqualT(entry, key))
            return entry;
#if DUMP_HASHTABLE_STATS
        ++collisionCount;
        HashTableStats::recordCollisionAtCount(collisionCount);
#endif
        i = (i + 1) & m_tableSizeMask;
    }
    
    KeyType convertedKey = ConvertT(key, h);
    m_table[i] = convertedKey;
    ++m_keyCount;
    
    if (m_keyCount * 2 >= m_tableSize)
        expand();
    
    return convertedKey;
}

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
void HashTable<Key, Hash, Equal>::reinsert(const Key& key)
{
    if (!m_table)
        expand();
    
    unsigned h = hash(key);
    int i = h & m_tableSizeMask;
#if DUMP_HASHTABLE_STATS
    ++HashTableStats::numAccesses;
    int collisionCount = 0;
#endif
    
    KeyType entry;
    while (!isEmptyBucket(entry = m_table[i])) {
#if DUMP_HASHTABLE_STATS
        ++collisionCount;
        HashTableStats::recordCollisionAtCount(collisionCount);
#endif
        i = (i + 1) & m_tableSizeMask;
    }
    
    m_table[i] = key;
}
 
template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
void HashTable<Key, Hash, Equal>::remove(const Key& key)
{
    unsigned h = hash(key);
    int i = h & m_tableSizeMask;
#if DUMP_HASHTABLE_STATS
    ++HashTableStats::numAccesses;
    ++HashTableStats::numRemoves;
    int collisionCount = 0;
#endif
    
    KeyType entry;
    while (!isEmptyBucket(entry = m_table[i])) {
        if (equal(entry, key))
            break;
#if DUMP_HASHTABLE_STATS
        ++collisionCount;
        HashTableStats::recordCollisionAtCount(collisionCount);
#endif
        i = (i + 1) & m_tableSizeMask;
    }
    if (!key)
        return;
    
    m_table[i] = 0;
    --m_keyCount;
    
    if (m_keyCount * 6 < m_tableSize && m_tableSize > m_minTableSize) {
        shrink();
        return;
    }
    
    // Reinsert all the items to the right in the same cluster.
    while (1) {
        i = (i + 1) & m_tableSizeMask;
        entry = m_table[i];
        if (isEmptyBucket(entry))
            break;
#if DUMP_HASHTABLE_STATS
        ++HashTableStats::numReinserts;
#endif
        clearBucket(m_table[i]);
        reinsert(entry);
    }
}

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
void HashTable<Key, Hash, Equal>::rehash(int newTableSize)
{
    int oldTableSize = m_tableSize;
    KeyType *oldTable = m_table;

#if DUMP_HASHTABLE_STATS
    if (oldTableSize != 0)
        ++HashTableStats::numRehashes;
#endif

    m_tableSize = newTableSize;
    m_tableSizeMask = newTableSize - 1;
    m_table = (KeyType *)main_thread_calloc(newTableSize, sizeof(KeyType));
    
    for (int i = 0; i != oldTableSize; ++i) {
        KeyType key;
        if (!isEmptyBucket(key = oldTable[i]))
            reinsert(key);
    }
    
    main_thread_free(oldTable);
}

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
HashTable<Key, Hash, Equal>::HashTable(const HashTable& other)
    : m_table(0)
    , m_tableSize(other.m_tableSize)
    , m_tableSizeMask(other.m_tableSizeMask)
    , m_keyCount(other.m_keyCount)
{
    if (m_tableSize != 0) {
        m_table = main_thread_malloc(m_tableSize);
        memcpy(other.m_table, m_table, m_tableSize * sizeof(KeyType));
    }
}

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
void HashTable<Key, Hash, Equal>::swap(const HashTable& other)
{
    KeyType *tmp_table = m_table;
    m_table = other.m_table;
    other.m_table = tmp_table;

    int tmp_tableSize = m_tableSize;
    m_tableSize = other.m_tableSize;
    other.m_tableSize = tmp_tableSize;

    int tmp_tableSizeMask = m_tableSizeMask;
    m_tableSizeMask = other.m_tableSizeMask;
    other.m_tableSizeMask = tmp_tableSizeMask;

    int tmp_keyCount = m_keyCount;
    m_keyCount = other.m_keyCount;
    other.m_keyCount = tmp_keyCount;
}

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
HashTable<Key, Hash, Equal>& HashTable<Key, Hash, Equal>::operator=(const HashTable& other)
{
    HashTable tmp(other);
    swap(tmp);
    return *this;
}

} // namespace khtml

#endif // HASHTABLE_H
