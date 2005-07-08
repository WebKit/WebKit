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
#include <utility>

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
#define checkTableConsistency() ((void)0)
#define checkTableConsistencyExceptSize() ((void)0)
#endif

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
class HashTable;

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
class HashTableIterator {
 private:
    typedef HashTable<Key, Value, ExtractKey, HashFunctions, Traits> HashTableType;
    typedef HashTableIterator<Key, Value, ExtractKey, HashFunctions, Traits> iterator;
    typedef Value ValueType;
    typedef ValueType& ReferenceType;
    typedef ValueType *PointerType;

    friend class HashTable<Key, Value, ExtractKey, HashFunctions, Traits>;
    
    HashTableIterator(PointerType position, PointerType endPosition) 
        : m_position(position), m_endPosition(endPosition) 
    { 
        skipEmptyBuckets();
    }
 public:
    HashTableIterator() {}

    // default copy, assignment and destructor are ok
    
    ReferenceType operator*() const { return *m_position; }
    PointerType operator->() const { return &(operator*()); }
    
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
        return m_position != other.m_position; 
    }

private:
    void skipEmptyBuckets() 
    {
        while (m_position != m_endPosition && (HashTableType::isEmptyOrDeletedBucket(*m_position))) {
            ++m_position;
        }
    }

    PointerType m_position;
    PointerType m_endPosition;
};

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
class HashTableConstIterator {
 private:
    typedef HashTable<Key, Value, ExtractKey, HashFunctions, Traits> HashTableType;
    typedef HashTableIterator<Key, Value, ExtractKey, HashFunctions, Traits> iterator;
    typedef HashTableConstIterator<Key, Value, ExtractKey, HashFunctions, Traits> const_iterator;
    typedef Value ValueType;
    typedef const ValueType& ReferenceType;
    typedef const ValueType *PointerType;

    friend class HashTable<Key, Value, ExtractKey, HashFunctions, Traits>;
    
    HashTableConstIterator(PointerType position, PointerType endPosition) 
        : m_position(position), m_endPosition(endPosition) 
    { 
        skipEmptyBuckets();
    }
 public:
    HashTableConstIterator() {}
    HashTableConstIterator(const iterator &other)
        : m_position(other.m_position), m_endPosition(other.m_endPosition) { }

    // default copy, assignment and destructor are ok
    
    ReferenceType operator*() const { return *m_position; }
    PointerType operator->() const { return &(operator*()); }
    
    void skipEmptyBuckets() 
    {
        while (m_position != m_endPosition && (HashTableType::isEmptyOrDeletedBucket(*m_position))) {
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
    bool operator==(const const_iterator& other) const
    { 
        return m_position == other.m_position; 
    }
    
    bool operator!=(const const_iterator& other) const 
    { 
        return m_position != other.m_position; 
    }

private:
    PointerType m_position;
    PointerType m_endPosition;
};

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
class HashTable {
 public:
    typedef HashTableIterator<Key, Value, ExtractKey, HashFunctions, Traits> iterator;
    typedef HashTableConstIterator<Key, Value, ExtractKey, HashFunctions, Traits> const_iterator;
    typedef Key KeyType;
    typedef Value ValueType;

    HashTable() : m_table(0), m_tableSize(0), m_tableSizeMask(0), m_keyCount(0), m_deletedCount(0) {}
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

    std::pair<iterator, bool> insert(const ValueType& value) { return insert<KeyType, ValueType, hash, equal, identityConvert>(extractKey(value), value); }

    // a special version of insert() that finds the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion if the object is already
    // in the table
    template<typename T, typename Extra, unsigned HashT(const T&), bool EqualT(const Key&, const T&), ValueType ConvertT(const T&, const Extra &, unsigned)> 
    std::pair<iterator, bool> insert(const T& key, const Extra& extra);

    iterator find(const KeyType& key);
    const_iterator find(const KeyType& key) const;
    bool contains(const KeyType& key) const;

    void remove(const KeyType& key);
    void remove(iterator it);
    void clear();

    static bool isEmptyBucket(const ValueType& value) { return extractKey(value) == extractKey(Traits::emptyValue()); }
    static bool isDeletedBucket(const ValueType& value) { return extractKey(value) == extractKey(Traits::deletedValue()); }
    static bool isEmptyOrDeletedBucket(const ValueType& value) { return isEmptyBucket(value) || isDeletedBucket(value); }

private:
    static unsigned hash(const KeyType& key) { return HashFunctions::hash(key); }
    static bool equal(const KeyType& a, const KeyType& b) { return HashFunctions::equal(a, b); }                    
    // FIXME: assumes key == value; special lookup needs adjusting
    static ValueType identityConvert(const KeyType& key, const ValueType& value, unsigned) { return value; }
    static KeyType extractKey(const ValueType& value) { return ExtractKey(value); }

    static ValueType *allocateTable(int size);

    typedef std::pair<ValueType *, bool> LookupType;
    typedef std::pair<LookupType, unsigned> FullLookupType;

    LookupType lookup(const KeyType& key) { return lookup<KeyType, hash, equal>(key).first; }
    template<typename T, unsigned HashT(const T&), bool EqualT(const Key&, const T&)>
    FullLookupType lookup(const T&);

    void remove(ValueType *);

    bool shouldExpand() const { return (m_keyCount + m_deletedCount) * m_maxLoad >= m_tableSize; }
    bool mustRehashInPlace() const { return m_keyCount * m_minLoad < m_tableSize * 2; }
    bool shouldShrink() const { return m_keyCount * m_minLoad < m_tableSize && m_tableSize > m_minTableSize; }
    void expand();
    void shrink() { rehash(m_tableSize / 2); }

    void rehash(int newTableSize);
    void reinsert(const ValueType&);

    static void clearBucket(ValueType& key) { key = Traits::emptyValue();}
    static void deleteBucket(ValueType& key) { key = Traits::deletedValue();}

    FullLookupType makeLookupResult(ValueType *position, bool found, unsigned hash)
    {
        return FullLookupType(LookupType(position, found), hash);
    }

    iterator makeIterator(ValueType *pos) { return iterator(pos, m_table + m_tableSize); }
    const_iterator makeConstIterator(ValueType *pos) const { return const_iterator(pos, m_table + m_tableSize); }

#if CHECK_HASHTABLE_CONSISTENCY
    void checkTableConsistency() const;
    void checkTableConsistencyExceptSize() const;
#endif

    static const int m_minTableSize = 64;
    static const int m_maxLoad = 2;
    static const int m_minLoad = 6;

    ValueType *m_table;
    int m_tableSize;
    int m_tableSizeMask;
    int m_keyCount;
    int m_deletedCount;
};

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
template<typename T, unsigned HashT(const T&), bool EqualT(const Key&, const T&)>
inline typename HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::FullLookupType HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::lookup(const T& key)
{
    if (!m_table)
        return makeLookupResult(0, false, 0);
    
    unsigned h = HashT(key);
    int i = h & m_tableSizeMask;
    int k = 0;

#if DUMP_HASHTABLE_STATS
    ++HashTableStats::numAccesses;
    int probeCount = 0;
#endif
    
    ValueType *entry;
    ValueType *deletedEntry = 0;
    while (!isEmptyBucket(*(entry = m_table + i))) {
        if (isDeletedBucket(*entry))
            deletedEntry = entry;
        else if (EqualT(extractKey(*entry), key))
            return makeLookupResult(entry, true, h);
#if DUMP_HASHTABLE_STATS
        HashTableStats::recordCollisionAtCount(probeCount);
        ++probeCount;
#endif
        if (k == 0)
            k = 1 | (h % m_tableSizeMask);
        i = (i + k) & m_tableSizeMask;
    }

    return makeLookupResult(deletedEntry ? deletedEntry : entry, false, h);
}


template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
template<typename T, typename Extra, unsigned HashT(const T&), bool EqualT(const Key&, const T&), Value ConvertT(const T&, const Extra &, unsigned)> 
inline std::pair<typename HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::iterator, bool> HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::insert(const T& key, const Extra &extra)
{
    if (!m_table)
        expand();

    checkTableConsistency();

    FullLookupType lookupResult = lookup<T, HashT, EqualT>(key);

    ValueType *entry = lookupResult.first.first;
    bool found = lookupResult.first.second;
    unsigned h = lookupResult.second;

    if (found) {
        return std::make_pair(makeIterator(entry), false);
    }

    if (isDeletedBucket(*entry))
        --m_deletedCount;

    *entry = ConvertT(key, extra, h);
    ++m_keyCount;
    
    if (shouldExpand()) {
        KeyType enteredKey = extractKey(*entry);
        expand();
        return std::make_pair(find(enteredKey), true);
    }

    checkTableConsistency();
    
    return std::make_pair(makeIterator(entry), true);
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
inline void HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::reinsert(const ValueType& entry)
{
    assert(m_table);
    assert(!lookup(extractKey(entry)).second);
    assert(!isDeletedBucket(*(lookup(extractKey(entry)).first)));
#if DUMP_HASHTABLE_STATS
    ++HashTableStats::numReinserts;
#endif

    *(lookup(extractKey(entry)).first) = entry;
}
 
template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
inline typename HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::iterator HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::find(const Key& key)
{
    LookupType result = lookup(key);
    if (!result.second)
        return end();
    return makeIterator(result.first);
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
inline typename HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::const_iterator HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::find(const Key& key) const
{
    LookupType result = const_cast<HashTable *>(this)->lookup(key);
    if (!result.second)
        return end();
    return makeConstIterator(result.first);
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
inline bool HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::contains(const KeyType& key) const 
{
    return const_cast<HashTable *>(this)->lookup(key).second;
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
inline void HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::remove(ValueType *pos)
{
    checkTableConsistency();

#if DUMP_HASHTABLE_STATS
    ++HashTableStats::numRemoves;
#endif
    
    deleteBucket(*pos);
    ++m_deletedCount;
    --m_keyCount;

    if (shouldShrink())
        shrink();

    checkTableConsistency();
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
inline void HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::remove(const KeyType& key)
{ 
    remove(lookup(key).first); 
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
inline void HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::remove(iterator it)
{ 
    remove(it.m_position); 
}


template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
inline Value *HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::allocateTable(int size)
{
    // would use a template member function with explicit specializations here, but
    // gcc doesn't appear to support that
    if (Traits::emptyValueIsZero)
        return reinterpret_cast<ValueType *>(main_thread_calloc(size, sizeof(ValueType))); 
    else {
        ValueType *result = reinterpret_cast<ValueType *>(main_thread_malloc(size * sizeof(ValueType))); 
        for (int i = 0; i < size; i++) {
            clearBucket(result[i]);
        }
        return result;
    }
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
inline void HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::expand()
{ 
    int newSize;
    if (m_tableSize == 0)
        newSize = m_minTableSize;
    else if (mustRehashInPlace())
        newSize = m_tableSize;
    else
        newSize = m_tableSize * 2;

    rehash(newSize); 
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
void HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::rehash(int newTableSize)
{
    checkTableConsistencyExceptSize();

    int oldTableSize = m_tableSize;
    ValueType *oldTable = m_table;

#if DUMP_HASHTABLE_STATS
    if (oldTableSize != 0)
        ++HashTableStats::numRehashes;
#endif

    m_tableSize = newTableSize;
    m_tableSizeMask = newTableSize - 1;
    m_table = allocateTable(newTableSize);
    
    for (int i = 0; i != oldTableSize; ++i) {
        ValueType entry = oldTable[i];
        if (!isEmptyOrDeletedBucket(entry))
            reinsert(entry);
    }

    m_deletedCount = 0;
    
    main_thread_free(oldTable);

    checkTableConsistency();
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
inline void HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::clear()
{
    main_thread_free(m_table);
    m_table = 0;
    m_tableSize = 0;
    m_tableSizeMask = 0;
    m_keyCount = 0;
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::HashTable(const HashTable& other)
    : m_table(0)
    , m_tableSize(other.m_tableSize)
    , m_tableSizeMask(other.m_tableSizeMask)
    , m_keyCount(other.m_keyCount)
    , m_deletedCount(other.m_deletedCount)
{
    if (m_tableSize != 0) {
        m_table = main_thread_malloc(m_tableSize);
        memcpy(other.m_table, m_table, m_tableSize * sizeof(ValueType));
    }
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
void HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::swap(const HashTable& other)
{
    ValueType *tmp_table = m_table;
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

    int tmp_deletedCount = m_deletedCount;
    m_deletedCount = other.m_deletedCount;
    other.m_deletedCount = tmp_deletedCount;
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
HashTable<Key, Value, ExtractKey, HashFunctions, Traits>& HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::operator=(const HashTable& other)
{
    HashTable tmp(other);
    swap(tmp);
    return *this;
}

#if CHECK_HASHTABLE_CONSISTENCY

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
void HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::checkTableConsistency() const
{
    checkTableConsistencyExceptSize();
    assert(!shouldExpand());
    assert(!shouldShrink());
}

template<typename Key, typename Value, Key ExtractKey(const Value &), typename HashFunctions, typename Traits>
void HashTable<Key, Value, ExtractKey, HashFunctions, Traits>::checkTableConsistencyExceptSize() const
{
    if (!m_table)
        return;

    int count = 0;
    int deletedCount = 0;
    for (int j = 0; j < m_tableSize; ++j) {
        ValueType *entry = m_table + j;
        if (isEmptyBucket(*entry))
            continue;

        if (isDeletedBucket(*entry)) {
            ++deletedCount;
            continue;
        }
        
        const_iterator it = find(extractKey(*entry));
        assert(entry == it.m_position);
        ++count;
    }

    assert(count == m_keyCount);
    assert(deletedCount == m_deletedCount);
    assert(m_tableSize >= m_minTableSize);
    assert(m_tableSizeMask);
    assert(m_tableSize == m_tableSizeMask + 1);
}

#endif // CHECK_HASHTABLE_CONSISTENCY

} // namespace khtml

#endif // HASHTABLE_H
