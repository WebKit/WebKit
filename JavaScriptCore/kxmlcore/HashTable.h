// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * This file is part of the KDE libraries
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef KXMLCORE_HASH_TABLE_H
#define KXMLCORE_HASH_TABLE_H

#include "FastMalloc.h"
#include "HashTraits.h"
#include <assert.h>
#include <algorithm>

namespace KXMLCore {

#define DUMP_HASHTABLE_STATS 0
#define CHECK_HASHTABLE_CONSISTENCY 0

#ifdef NDEBUG
#define CHECK_HASHTABLE_ITERATORS 0
#else
#define CHECK_HASHTABLE_ITERATORS 1
#endif

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

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTable;
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTableIterator;
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTableConstIterator;

#if CHECK_HASHTABLE_ITERATORS
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void addIterator(const HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>*,
        HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>*);

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void removeIterator(HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>*);
#else
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    inline void addIterator(const HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>*,
        HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>*) { }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    inline void removeIterator(HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>*) { }
#endif

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTableConstIterator {
    private:
        typedef HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> HashTableType;
        typedef HashTableIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> iterator;
        typedef HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> const_iterator;
        typedef Value ValueType;
        typedef const ValueType& ReferenceType;
        typedef const ValueType* PointerType;

        friend class HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;
        friend class HashTableIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;

        void skipEmptyBuckets()
        {
            while (m_position != m_endPosition && HashTableType::isEmptyOrDeletedBucket(*m_position))
                ++m_position;
        }

        HashTableConstIterator(const HashTableType* table, PointerType position, PointerType endPosition)
            : m_position(position), m_endPosition(endPosition)
        {
            addIterator(table, this);
            skipEmptyBuckets();
        }

    public:
        HashTableConstIterator()
        {
            addIterator(0, this);
        }

        // default copy, assignment and destructor are OK if CHECK_HASHTABLE_ITERATORS is 0

#if CHECK_HASHTABLE_ITERATORS
        ~HashTableConstIterator()
        {
            removeIterator(this);
        }

        HashTableConstIterator(const const_iterator& other)
            : m_position(other.m_position), m_endPosition(other.m_endPosition)
        {
            addIterator(other.m_table, this);
        }

        const_iterator& operator=(const const_iterator& other)
        {
            m_position = other.m_position;
            m_endPosition = other.m_endPosition;

            removeIterator(this);
            addIterator(other.m_table, this);

            return *this;
        }
#endif

        PointerType get() const
        {
            checkValidity();
            return m_position;
        }
        ReferenceType operator*() const { return *get(); }
        PointerType operator->() const { return get(); }

        const_iterator& operator++()
        {
            checkValidity();
            assert(m_position != m_endPosition);
            ++m_position;
            skipEmptyBuckets();
            return *this;
        }

        // postfix ++ intentionally omitted

        // Comparison.
        bool operator==(const const_iterator& other) const
        {
            checkValidity(other);
            return m_position == other.m_position;
        }
        bool operator!=(const const_iterator& other) const
        {
            checkValidity(other);
            return m_position != other.m_position;
        }

    private:
        void checkValidity() const
        {
#if CHECK_HASHTABLE_ITERATORS
            assert(m_table);
#endif
        }


#if CHECK_HASHTABLE_ITERATORS
        void checkValidity(const const_iterator& other) const
        {
            assert(m_table);
            assert(other.m_table);
            assert(m_table == other.m_table);
        }
#else
        void checkValidity(const const_iterator&) const { }
#endif

        PointerType m_position;
        PointerType m_endPosition;

#if CHECK_HASHTABLE_ITERATORS
    public:
        mutable const HashTableType* m_table;
        mutable const_iterator* m_next;
        mutable const_iterator* m_previous;
#endif
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTableIterator {
    private:
        typedef HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> HashTableType;
        typedef HashTableIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> iterator;
        typedef HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> const_iterator;
        typedef Value ValueType;
        typedef ValueType& ReferenceType;
        typedef ValueType* PointerType;

        friend class HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;

        HashTableIterator(HashTableType* table, PointerType pos, PointerType end) : m_iterator(table, pos, end) { }

    public:
        HashTableIterator() { }

        // default copy, assignment and destructor are OK

        PointerType get() const { return const_cast<PointerType>(m_iterator.get()); }
        ReferenceType operator*() const { return *get(); }
        PointerType operator->() const { return get(); }

        iterator& operator++() { ++m_iterator; return *this; }

        // postfix ++ intentionally omitted

        // Comparison.
        bool operator==(const iterator& other) const { return m_iterator == other.m_iterator; }
        bool operator!=(const iterator& other) const { return m_iterator != other.m_iterator; }

        operator const_iterator() const { return m_iterator; }

    private:
        const_iterator m_iterator;
    };

    using std::swap;

#if !COMPILER(MSVC)
    // Visual C++ has a swap for pairs defined.

    // swap pairs by component, in case of pair members that specialize swap
    template<typename T, typename U> inline void swap(pair<T, U>& a, pair<T, U>& b)
    {
        swap(a.first, b.first);
        swap(a.second, b.second);
    }
#endif

    template<typename T, bool useSwap> struct Mover;
    template<typename T> struct Mover<T, true> { static void move(T& from, T& to) { swap(from, to); } };
    template<typename T> struct Mover<T, false> { static void move(T& from, T& to) { to = from; } };

    template<typename Key, typename Value, typename HashFunctions> class IdentityHashTranslator {
    public:
        static unsigned hash(const Key& key) { return HashFunctions::hash(key); }
        static bool equal(const Key& a, const Key& b) { return HashFunctions::equal(a, b); }
        static void translate(Value& location, const Key&, const Value& value, unsigned) { location = value; }
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTable {
    public:
        typedef HashTableIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> iterator;
        typedef HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> const_iterator;
        typedef Traits ValueTraits;
        typedef Key KeyType;
        typedef Value ValueType;
        typedef IdentityHashTranslator<Key, Value, HashFunctions> IdentityTranslatorType;

        HashTable();
        ~HashTable() { invalidateIterators(); deallocateTable(m_table, m_tableSize); }

        HashTable(const HashTable&);
        void swap(HashTable&);
        HashTable& operator=(const HashTable&);

        iterator begin() { return makeIterator(m_table); }
        iterator end() { return makeIterator(m_table + m_tableSize); }
        const_iterator begin() const { return makeConstIterator(m_table); }
        const_iterator end() const { return makeConstIterator(m_table + m_tableSize); }

        int size() const { return m_keyCount; }
        int capacity() const { return m_tableSize; }
        bool isEmpty() const { return !m_keyCount; }

        pair<iterator, bool> add(const ValueType& value) { return add<KeyType, ValueType, IdentityTranslatorType>(Extractor::extract(value), value); }

        // A special version of add() that finds the object by hashing and comparing
        // with some other type, to avoid the cost of type conversion if the object is already
        // in the table.
        template<typename T, typename Extra, typename HashTranslator> pair<iterator, bool> add(const T& key, const Extra&);

        iterator find(const KeyType&);
        const_iterator find(const KeyType&) const;
        bool contains(const KeyType&) const;

        void remove(const KeyType&);
        void remove(iterator);
        void clear();

        static bool isEmptyBucket(const ValueType& value) { return Extractor::extract(value) == KeyTraits::emptyValue(); }
        static bool isDeletedBucket(const ValueType& value) { return Extractor::extract(value) == KeyTraits::deletedValue(); }
        static bool isEmptyOrDeletedBucket(const ValueType& value) { return isEmptyBucket(value) || isDeletedBucket(value); }

    private:
        static ValueType* allocateTable(int size);
        static void deallocateTable(ValueType* table, int size);

        typedef pair<ValueType*, bool> LookupType;
        typedef pair<LookupType, unsigned> FullLookupType;

        LookupType lookup(const Key& key) { return lookup<Key, IdentityTranslatorType>(key).first; }
        template<typename T, typename HashTranslator> FullLookupType lookup(const T&);

        void remove(ValueType*);

        bool shouldExpand() const { return (m_keyCount + m_deletedCount) * m_maxLoad >= m_tableSize; }
        bool mustRehashInPlace() const { return m_keyCount * m_minLoad < m_tableSize * 2; }
        bool shouldShrink() const { return m_keyCount * m_minLoad < m_tableSize && m_tableSize > m_minTableSize; }
        void expand();
        void shrink() { rehash(m_tableSize / 2); }

        void rehash(int newTableSize);
        void reinsert(ValueType&);

        static void initializeBucket(ValueType& bucket) { new (&bucket) ValueType(Traits::emptyValue()); }
        static void deleteBucket(ValueType& bucket) { assignDeleted<ValueType, Traits>(bucket); }

        FullLookupType makeLookupResult(ValueType* position, bool found, unsigned hash)
            { return FullLookupType(LookupType(position, found), hash); }

        iterator makeIterator(ValueType* pos) { return iterator(this, pos, m_table + m_tableSize); }
        const_iterator makeConstIterator(ValueType* pos) const { return const_iterator(this, pos, m_table + m_tableSize); }

#if CHECK_HASHTABLE_CONSISTENCY
        void checkTableConsistency() const;
        void checkTableConsistencyExceptSize() const;
#else
        static void checkTableConsistency() { }
        static void checkTableConsistencyExceptSize() { }
#endif

#if CHECK_HASHTABLE_ITERATORS
        void invalidateIterators();
#else
        static void invalidateIterators() { }
#endif

        static const int m_minTableSize = 64;
        static const int m_maxLoad = 2;
        static const int m_minLoad = 6;

        ValueType* m_table;
        int m_tableSize;
        int m_tableSizeMask;
        int m_keyCount;
        int m_deletedCount;

#if CHECK_HASHTABLE_ITERATORS
    public:
        mutable const_iterator* m_iterators;
#endif
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    inline HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::HashTable()
        : m_table(0)
        , m_tableSize(0)
        , m_tableSizeMask(0)
        , m_keyCount(0)
        , m_deletedCount(0)
#if CHECK_HASHTABLE_ITERATORS
        , m_iterators(0)
#endif
    {
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    template<typename T, typename HashTranslator>
    inline typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::FullLookupType HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::lookup(const T& key)
    {
        assert(m_table);

        unsigned h = HashTranslator::hash(key);
        int sizeMask = m_tableSizeMask;
        int i = h & sizeMask;
        int k = 0;

#if DUMP_HASHTABLE_STATS
        ++HashTableStats::numAccesses;
        int probeCount = 0;
#endif

        ValueType *table = m_table;
        ValueType *entry;
        ValueType *deletedEntry = 0;
        while (!isEmptyBucket(*(entry = table + i))) {
            if (isDeletedBucket(*entry))
                deletedEntry = entry;
            else if (HashTranslator::equal(Extractor::extract(*entry), key))
                return makeLookupResult(entry, true, h);
#if DUMP_HASHTABLE_STATS
            ++probeCount;
            HashTableStats::recordCollisionAtCount(probeCount);
#endif
            if (k == 0)
                k = 1 | (h % sizeMask);
            i = (i + k) & sizeMask;
        }

        return makeLookupResult(deletedEntry ? deletedEntry : entry, false, h);
    }


    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    template<typename T, typename Extra, typename HashTranslator>
    inline pair<typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::iterator, bool> HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::add(const T& key, const Extra &extra)
    {
        invalidateIterators();

        if (!m_table)
            expand();

        checkTableConsistency();

        FullLookupType lookupResult = lookup<T, HashTranslator>(key);

        ValueType *entry = lookupResult.first.first;
        bool found = lookupResult.first.second;
        unsigned h = lookupResult.second;

        if (found)
            return std::make_pair(makeIterator(entry), false);

        if (isDeletedBucket(*entry))
            --m_deletedCount;

        HashTranslator::translate(*entry, key, extra, h);
        ++m_keyCount;

        if (shouldExpand()) {
            // FIXME: this makes an extra copy on expand. Probably not that bad since
            // expand is rare, but would be better to have a version of expand that can
            // follow a pivot entry and return the new position
            KeyType enteredKey = Extractor::extract(*entry);
            expand();
            return std::make_pair(find(enteredKey), true);
        }

        checkTableConsistency();

        return std::make_pair(makeIterator(entry), true);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::reinsert(ValueType& entry)
    {
        assert(m_table);
        assert(!lookup(Extractor::extract(entry)).second);
        assert(!isDeletedBucket(*(lookup(Extractor::extract(entry)).first)));
#if DUMP_HASHTABLE_STATS
        ++HashTableStats::numReinserts;
#endif

        Mover<ValueType, Traits::needsDestruction>::move(entry, *(lookup(Extractor::extract(entry)).first));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::iterator HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::find(const Key& key)
    {
        if (!m_table)
            return end();

        LookupType result = lookup(key);
        if (!result.second)
            return end();
        return makeIterator(result.first);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::const_iterator HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::find(const Key& key) const
    {
        if (!m_table)
            return end();

        LookupType result = const_cast<HashTable *>(this)->lookup(key);
        if (!result.second)
            return end();
        return makeConstIterator(result.first);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    bool HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::contains(const KeyType& key) const
    {
        if (!m_table)
            return false;

        return const_cast<HashTable *>(this)->lookup(key).second;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::remove(ValueType* pos)
    {
        invalidateIterators();
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

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::remove(iterator it)
    {
        if (it == end())
            return;

        remove(const_cast<ValueType*>(it.m_iterator.m_position));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::remove(const KeyType& key)
    {
        remove(find(key));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    Value *HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::allocateTable(int size)
    {
        // would use a template member function with explicit specializations here, but
        // gcc doesn't appear to support that
        if (Traits::emptyValueIsZero)
            return static_cast<ValueType *>(fastCalloc(size, sizeof(ValueType)));
        ValueType* result = static_cast<ValueType*>(fastMalloc(size * sizeof(ValueType)));
        for (int i = 0; i < size; i++)
            initializeBucket(result[i]);
        return result;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::deallocateTable(ValueType *table, int size)
    {
        if (Traits::needsDestruction)
            for (int i = 0; i < size; ++i)
                table[i].~ValueType();
        fastFree(table);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::expand()
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

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::rehash(int newTableSize)
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

        for (int i = 0; i != oldTableSize; ++i)
            if (!isEmptyOrDeletedBucket(oldTable[i]))
                reinsert(oldTable[i]);

        m_deletedCount = 0;

        deallocateTable(oldTable, oldTableSize);

        checkTableConsistency();
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::clear()
    {
        invalidateIterators();
        deallocateTable(m_table, m_tableSize);
        m_table = 0;
        m_tableSize = 0;
        m_tableSizeMask = 0;
        m_keyCount = 0;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::HashTable(const HashTable& other)
        : m_table(0)
        , m_tableSize(0)
        , m_tableSizeMask(0)
        , m_keyCount(0)
        , m_deletedCount(0)
#if CHECK_HASHTABLE_ITERATORS
        , m_iterators(0)
#endif
    {
        // Copy the hash table the dumb way, by adding each element to the new table.
        // It might be more efficient to copy the table slots, but it's not clear that efficiency is needed.
        const_iterator end = other.end();
        for (const_iterator it = other.begin(); it != end; ++it)
            add(*it);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::swap(HashTable& other)
    {
        invalidateIterators();
        other.invalidateIterators();

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

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>& HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::operator=(const HashTable& other)
    {
        HashTable tmp(other);
        swap(tmp);
        return *this;
    }

#if CHECK_HASHTABLE_CONSISTENCY

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::checkTableConsistency() const
    {
        checkTableConsistencyExceptSize();
        assert(!shouldExpand());
        assert(!shouldShrink());
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::checkTableConsistencyExceptSize() const
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

            const_iterator it = find(Extractor::extract(*entry));
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

#if CHECK_HASHTABLE_ITERATORS

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>::invalidateIterators()
    {
        const_iterator* next;
        for (const_iterator* p = m_iterators; p; p = next) {
            next = p->m_next;
            p->m_table = 0;
            p->m_next = 0;
            p->m_previous = 0;
        }
        m_iterators = 0;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void addIterator(const HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>* table,
        HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>* it)
    {
        it->m_table = table;
        it->m_previous = 0;

        // Insert iterator at head of doubly-linked list of iterators.
        if (!table) {
            it->m_next = 0;
        } else {
            assert(table->m_iterators != it);
            it->m_next = table->m_iterators;
            table->m_iterators = it;
            if (it->m_next) {
                assert(!it->m_next->m_previous);
                it->m_next->m_previous = it;
            }
        }
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    void removeIterator(HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits>* it)
    {
        typedef HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> HashTableType;
        typedef HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits> const_iterator;

        // Delete iterator from doubly-linked list of iterators.
        if (!it->m_table) {
            assert(!it->m_next);
            assert(!it->m_previous);
        } else {
            if (it->m_next) {
                assert(it->m_next->m_previous == it);
                it->m_next->m_previous = it->m_previous;
            }
            if (it->m_previous) {
                assert(it->m_table->m_iterators != it);
                assert(it->m_previous->m_next == it);
                it->m_previous->m_next = it->m_next;
            } else {
                assert(it->m_table->m_iterators == it);
                it->m_table->m_iterators = it->m_next;
            }
        }

        it->m_table = 0;
        it->m_next = 0;
        it->m_previous = 0;
    }

#endif // CHECK_HASHTABLE_ITERATORS

    // iterator adapters

    template<typename HashTableType, typename ValueType> struct HashTableConstIteratorAdapter {
        HashTableConstIteratorAdapter(const typename HashTableType::const_iterator& impl) : m_impl(impl) {}

        const ValueType* get() const { return (const ValueType*)m_impl.get(); }
        const ValueType& operator*() const { return *get(); }
        const ValueType* operator->() const { return get(); }

        HashTableConstIteratorAdapter& operator++() { ++m_impl; return *this; }
        // postfix ++ intentionally omitted

        typename HashTableType::const_iterator m_impl;
    };

    template<typename HashTableType, typename ValueType> struct HashTableIteratorAdapter {
        HashTableIteratorAdapter(const typename HashTableType::iterator& impl) : m_impl(impl) {}

        ValueType* get() const { return (ValueType*)m_impl.get(); }
        ValueType& operator*() const { return *get(); }
        ValueType* operator->() const { return get(); }

        HashTableIteratorAdapter& operator++() { ++m_impl; return *this; }
        // postfix ++ intentionally omitted

        operator HashTableConstIteratorAdapter<HashTableType, ValueType>() {
            typename HashTableType::const_iterator i = m_impl;
            return i;
        }

        typename HashTableType::iterator m_impl;
    };

    template<typename T, typename U>
    inline bool operator==(const HashTableConstIteratorAdapter<T, U>& a, const HashTableConstIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator!=(const HashTableConstIteratorAdapter<T, U>& a, const HashTableConstIteratorAdapter<T, U>& b)
    {
        return a.m_impl != b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator==(const HashTableIteratorAdapter<T, U>& a, const HashTableIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator!=(const HashTableIteratorAdapter<T, U>& a, const HashTableIteratorAdapter<T, U>& b)
    {
        return a.m_impl != b.m_impl;
    }

    // reference count manager
    
    template<typename ValueTraits, typename ValueStorageTraits> struct NeedsRef {
        static const bool value = ValueTraits::needsRef && !ValueStorageTraits::needsRef;
    };
    template<typename FirstTraits, typename SecondTraits, typename ValueStorageTraits>
    struct NeedsRef<PairBaseHashTraits<FirstTraits, SecondTraits>, ValueStorageTraits> {
        typedef typename ValueStorageTraits::FirstTraits FirstStorageTraits;
        typedef typename ValueStorageTraits::SecondTraits SecondStorageTraits;
        static const bool firstNeedsRef = NeedsRef<FirstTraits, FirstStorageTraits>::value;
        static const bool secondNeedsRef = NeedsRef<SecondTraits, SecondStorageTraits>::value;
        static const bool value = firstNeedsRef || secondNeedsRef;
    };

    template<bool needsRef, typename ValueTraits> struct RefCounterBase;

    template<typename ValueTraits>
    struct RefCounterBase<false, ValueTraits> {
        typedef typename ValueTraits::TraitType ValueType;
        static void ref(const ValueType&) { }
        static void deref(const ValueType&) { }
    };

    template<typename ValueTraits>
    struct RefCounterBase<true, ValueTraits> {
        typedef typename ValueTraits::TraitType ValueType;
        static void ref(const ValueType& v) { ValueTraits::ref(*(const ValueType*)&v); }
        static void deref(const ValueType& v) { ValueTraits::deref(*(const ValueType*)&v); }
    };

    template<typename ValueTraits, typename ValueStorageTraits> struct RefCounter {
        typedef typename ValueTraits::TraitType ValueType;
        typedef typename ValueStorageTraits::TraitType ValueStorageType;
        static const bool needsRef = NeedsRef<ValueTraits, ValueStorageTraits>::value;
        typedef RefCounterBase<needsRef, ValueTraits> Base;
        static void ref(const ValueStorageType& v) { Base::ref(*(const ValueType*)&v); }
        static void deref(const ValueStorageType& v) { Base::deref(*(const ValueType*)&v); }
    };

    template<typename FirstTraits, typename SecondTraits, typename ValueStorageTraits>
    struct RefCounter<PairBaseHashTraits<FirstTraits, SecondTraits>, ValueStorageTraits> {
        typedef typename FirstTraits::TraitType FirstType;
        typedef typename SecondTraits::TraitType SecondType;
        typedef typename ValueStorageTraits::FirstTraits FirstStorageTraits;
        typedef typename ValueStorageTraits::SecondTraits SecondStorageTraits;
        typedef typename ValueStorageTraits::TraitType ValueStorageType;
        static const bool firstNeedsRef = NeedsRef<FirstTraits, FirstStorageTraits>::value;
        static const bool secondNeedsRef = NeedsRef<SecondTraits, SecondStorageTraits>::value;
        typedef RefCounterBase<firstNeedsRef, FirstTraits> FirstBase;
        typedef RefCounterBase<secondNeedsRef, SecondTraits> SecondBase;
        static void ref(const ValueStorageType& v) {
            FirstBase::ref(*(const FirstType*)&v.first);
            SecondBase::ref(*(const SecondType*)&v.second);
        }
        static void deref(const ValueStorageType& v) {
            FirstBase::deref(*(const FirstType*)&v.first);
            SecondBase::deref(*(const SecondType*)&v.second);
        }
    };

    template<bool needsRef, typename HashTableType, typename ValueTraits> struct HashTableRefCounterBase;

    template<typename HashTableType, typename ValueTraits>
    struct HashTableRefCounterBase<false, HashTableType, ValueTraits>
    {
        static void refAll(HashTableType&) { }
        static void derefAll(HashTableType&) { }
    };

    template<typename HashTableType, typename ValueTraits>
    struct HashTableRefCounterBase<true, HashTableType, ValueTraits>
    {
        typedef typename HashTableType::iterator iterator;
        typedef RefCounter<ValueTraits, typename HashTableType::ValueTraits> ValueRefCounter;
        static void refAll(HashTableType&);
        static void derefAll(HashTableType&);
    };

    template<typename HashTableType, typename ValueTraits>
    void HashTableRefCounterBase<true, HashTableType, ValueTraits>::refAll(HashTableType& table)
    {
        iterator end = table.end();
        for (iterator it = table.begin(); it != end; ++it)
            ValueRefCounter::ref(*it);
    }

    template<typename HashTableType, typename ValueTraits>
    void HashTableRefCounterBase<true, HashTableType, ValueTraits>::derefAll(HashTableType& table)
    {
        iterator end = table.end();
        for (iterator it = table.begin(); it != end; ++it)
            ValueRefCounter::deref(*it);
    }

    template<typename HashTableType, typename ValueTraits> struct HashTableRefCounter {
        static const bool needsRef = NeedsRef<ValueTraits, typename HashTableType::ValueTraits>::value;
        typedef HashTableRefCounterBase<needsRef, HashTableType, ValueTraits> Base;
        static void refAll(HashTableType& table) { Base::refAll(table); }
        static void derefAll(HashTableType& table) { Base::derefAll(table); }
    };

} // namespace KXMLCore

#endif // KXMLCORE_HASH_TABLE_H
