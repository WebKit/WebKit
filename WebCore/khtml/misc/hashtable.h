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
#define CHECK_HASHTABLE_CONSISTENCY 0

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

#if !CHECK_HASHTABLE_CONSISTENCY
#define checkConsistency() ((void)0)
#define checkConsistencyExceptSize() ((void)0)
#endif

// FIXME: this template actually depends on Key being a pointer type in two ways:
// it compares to 0 to check for empty slots, and assigns to 0 when deleting.

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
class HashTable;

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
class HashTableIterator {
 private:
    typedef HashTable<Key, Hash, Equal> HashTableType;
    typedef HashTableType *HashTablePointerType;
    typedef HashTableIterator<Key, Hash, Equal> iterator;
    typedef Key KeyType;
    typedef KeyType& ReferenceType;
    typedef KeyType *PointerType;

    friend class HashTable<Key, Hash, Equal>;
    
    HashTableIterator(HashTablePointerType table, PointerType position, PointerType endPosition) 
        : m_table(table), m_position(position), m_endPosition(endPosition) 
    { 
        skipEmptyBuckets();
    }
 public:
    HashTableIterator() {}

    // default copy, assignment and destructor are ok
    
    ReferenceType operator*() const { return *m_position; }
    PointerType operator->() const { return &(operator*()); }
    
    void skipEmptyBuckets() 
    {
        while (m_position != m_endPosition && (m_table->isEmptyBucket(*m_position))) {
            ++m_position;
        }
    }
    
    iterator& operator++()
    {
        ++m_position;
        skipEmptyBuckets();
        return *this;
    }
    
    // postfix ++ intentionally omitted
    
    // Comparison.
    bool operator==(const iterator& other) const
    { 
        return m_position == other.m_position; 
    }
    
    bool operator!=(const iterator& other) const 
    { 
        return m_position != other.m_posisiton; 
    }

private:
    HashTablePointerType m_table;
    PointerType m_position;
    PointerType m_endPosition;
};

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
class HashTableConstIterator {
 private:
    typedef HashTable<Key, Hash, Equal> HashTableType;
    typedef const HashTableType *HashTablePointerType;
    typedef HashTableIterator<Key, Hash, Equal> iterator;
    typedef HashTableConstIterator<Key, Hash, Equal> const_iterator;
    typedef Key KeyType;
    typedef const KeyType& ReferenceType;
    typedef const KeyType *PointerType;

    friend class HashTable<Key, Hash, Equal>;
    
    HashTableConstIterator(HashTablePointerType table, PointerType position, PointerType endPosition) 
        : m_table(table), m_position(position), m_endPosition(endPosition) 
    { 
        skipEmptyBuckets();
    }
 public:
    HashTableConstIterator() {}
    HashTableConstIterator(const iterator &other)
        : m_table(other.ht), m_position(other.m_position), m_endPosition(other.m_endPosition) { }

    // default copy, assignment and destructor are ok
    
    ReferenceType operator*() const { return *m_position; }
    PointerType operator->() const { return &(operator*()); }
    
    void skipEmptyBuckets() 
    {
        while (m_position != m_endPosition && (m_table->isEmptyBucket(*m_position))) {
            ++m_position;
        }
    }
    
    iterator& operator++()
    {
        ++m_position;
        skipEmptyBuckets();
        return *this;
    }
    
    // postfix ++ intentionally omitted
    
    // Comparison.
    bool operator==(const iterator& other) const
    { 
        return m_position == other.m_position; 
    }
    
    bool operator!=(const iterator& other) const 
    { 
        return m_position != other.m_posisiton; 
    }

private:
    HashTablePointerType m_table;
    PointerType m_position;
    PointerType m_endPosition;
};

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
class HashTable {
 public:
    typedef HashTableIterator<Key, Hash, Equal> iterator;
    typedef HashTableConstIterator<Key, Hash, Equal> const_iterator;
    typedef Key KeyType;
    typedef const Key& ReferenceType;
    typedef const Key *PointerType;

    HashTable() : m_table(0), m_tableSize(0), m_tableSizeMask(0), m_keyCount(0) {}
    ~HashTable() { main_thread_free(m_table); }

    HashTable(const HashTable& other);
    void swap(const HashTable& other);
    HashTable& operator=(const HashTable& other);

    iterator begin() { return makeIterator(m_table); }
    iterator end() { return makeIterator(m_table + m_tableSize); }
    const_iterator begin() const { return makeConstIterator(m_table); }
    const_iterator end() const { return makeConstIterator(m_table + m_tableSize); }

    int size() const { return m_keyCount; }
    int capacity() const { return m_tableSize; }

    iterator insert(const KeyType& key) { return insert<KeyType, hash, equal, identityConvert>(key); }

    // a special version of insert() that finds the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion if the object is already
    // in the table
    template<typename T, unsigned HashT(const T&), bool EqualT(const Key&, const T&), KeyType ConvertT(const T&, unsigned)> iterator insert(const T& key);

    iterator find(const KeyType& key);

    const_iterator find(const KeyType& key) const
    {
        return makeConstIterator(const_cast<HashTable *>(this)->find(key).m_position);
    }

    bool contains(const KeyType& key) const 
    {
        return find(key) != end();
    }

    void remove(const KeyType& key) 
    {
        remove(find(key));
    }
    void remove(iterator it);
    void clear();

    bool isEmptyBucket(const KeyType& key) const { return key == 0; }

private:
    static unsigned hash(const KeyType& key) { return Hash(key); }
    static bool equal(const KeyType& a, const KeyType& b) { return Equal(a, b); }                    
    static KeyType identityConvert(const KeyType& key, unsigned) { return key; }

    bool shouldExpand() const { return m_keyCount * m_maxLoad >= m_tableSize; }
    void expand() { rehash(m_tableSize == 0 ? m_minTableSize : m_tableSize * 2); }
    bool shouldShrink() const { return m_keyCount * m_minLoad < m_tableSize && m_tableSize > m_minTableSize; }
    void shrink() { rehash(m_tableSize / 2); }
    void rehash(int newTableSize);
    void reinsert(const KeyType& key);

    void clearBucket(KeyType& key) { key = 0;}

    iterator makeIterator(KeyType *pos) { return iterator(this, pos, m_table + m_tableSize); }
    const_iterator makeConstIterator(KeyType *pos) const { return const_iterator(this, pos, m_table + m_tableSize); }

#if CHECK_HASHTABLE_CONSISTENCY
    void checkConsistency() const;
    void checkConsistencyExceptSize() const;
#endif

    static const int m_minTableSize = 64;
    static const int m_maxLoad = 2;
    static const int m_minLoad = 6;

    KeyType *m_table;
    int m_tableSize;
    int m_tableSizeMask;
    int m_keyCount;
};

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
template<typename T, unsigned HashT(const T&), bool EqualT(const Key&, const T&), Key ConvertT(const T&, unsigned)> 
typename HashTable<Key, Hash, Equal>::iterator HashTable<Key, Hash, Equal>::insert(const T& key)
{
    if (!m_table)
        expand();

    checkConsistency();
    
    unsigned h = HashT(key);
    int i = h & m_tableSizeMask;
#if DUMP_HASHTABLE_STATS
    ++HashTableStats::numAccesses;
    int collisionCount = 0;
#endif
    
    KeyType *entry;
    while (!isEmptyBucket(*(entry = m_table + i))) {
        if (EqualT(*entry, key)) {
            return makeIterator(entry);
        }
#if DUMP_HASHTABLE_STATS
        ++collisionCount;
        HashTableStats::recordCollisionAtCount(collisionCount);
#endif
        i = (i + 1) & m_tableSizeMask;
    }
    
    *entry = ConvertT(key, h);
    ++m_keyCount;
    
    if (shouldExpand()) {
        KeyType enteredKey = *entry;
        expand();
        return find(enteredKey);
    }

    checkConsistency();
    
    return makeIterator(entry);
}

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
void HashTable<Key, Hash, Equal>::reinsert(const KeyType& key)
{
    if (!m_table)
        expand();
    
    unsigned h = hash(key);
    int i = h & m_tableSizeMask;
#if DUMP_HASHTABLE_STATS
    ++HashTableStats::numAccesses;
    int collisionCount = 0;
#endif
    
    KeyType *entry;
    while (!isEmptyBucket(*(entry = m_table + i))) {
#if DUMP_HASHTABLE_STATS
        ++collisionCount;
        HashTableStats::recordCollisionAtCount(collisionCount);
#endif
        i = (i + 1) & m_tableSizeMask;
    }
    
    *entry = key;
}
 
template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
typename HashTable<Key, Hash, Equal>::iterator HashTable<Key, Hash, Equal>::find(const Key& key)
{
    if (!m_table)
        return end();
    
    unsigned h = hash(key);
    int i = h & m_tableSizeMask;
#if DUMP_HASHTABLE_STATS
    ++HashTableStats::numAccesses;
    int collisionCount = 0;
#endif
    
    KeyType *entry;
    while (!isEmptyBucket(*(entry = m_table + i))) {
        if (equal(*entry, key))
            return makeIterator(entry);
#if DUMP_HASHTABLE_STATS
        ++collisionCount;
        HashTableStats::recordCollisionAtCount(collisionCount);
#endif
        i = (i + 1) & m_tableSizeMask;
    }

    return end();
}

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
void HashTable<Key, Hash, Equal>::remove(iterator it)
{
    checkConsistency();

    if (it == end())
        return;

#if DUMP_HASHTABLE_STATS
    ++HashTableStats::numRemoves;
#endif
    
    *it = 0;
    --m_keyCount;
    
    if (shouldShrink()) {
        shrink();
        return;
    }

    int i = it.operator->() - m_table;
    
    // Reinsert all the items to the right in the same cluster.
    while (1) {
        i = (i + 1) & m_tableSizeMask;
        KeyType entry = m_table[i];
        if (isEmptyBucket(entry))
            break;
#if DUMP_HASHTABLE_STATS
        ++HashTableStats::numReinserts;
#endif
        clearBucket(m_table[i]);
        reinsert(entry);
    }

    checkConsistency();
}

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
void HashTable<Key, Hash, Equal>::rehash(int newTableSize)
{
    checkConsistencyExceptSize();

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
        KeyType key = oldTable[i];
        if (!isEmptyBucket(key))
            reinsert(key);
    }
    
    main_thread_free(oldTable);

    checkConsistency();
}

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
void HashTable<Key, Hash, Equal>::clear()
{
    main_thread_free(m_table);
    m_table = 0;
    m_tableSize = 0;
    m_tableSizeMask = 0;
    m_keyCount = 0;
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

#if CHECK_HASHTABLE_CONSISTENCY

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
void HashTable<Key, Hash, Equal>::checkConsistency() const
{
    checkConsistencyExceptSize();
    assert(!shouldExpand());
    assert(!shouldShrink());
}

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
void HashTable<Key, Hash, Equal>::checkConsistencyExceptSize() const
{
    if (!m_table)
        return;

    int count = 0;
    for (int j = 0; j < m_tableSize; ++j) {
        KeyType *entry = m_table + j;
        if (isEmptyBucket(*entry))
            continue;
        
        const_iterator it = find(*entry);
        assert(entry == it.m_position);
        ++count;
    }

    assert(count == m_keyCount);
    assert(m_tableSize >= m_minTableSize);
    assert(m_tableSizeMask);
    assert(m_tableSize == m_tableSizeMask + 1);
}

#endif // CHECK_HASHTABLE_CONSISTENCY

} // namespace khtml

#endif // HASHTABLE_H
