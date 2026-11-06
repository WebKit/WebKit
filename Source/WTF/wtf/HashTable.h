/*
 * Copyright (C) 2005-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Levin <levin@chromium.org>
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

#pragma once

#include <atomic>
#include <bit>
#include <iterator>
#include <memory>
#include <mutex>
#include <numeric>
#include <string.h>
#include <type_traits>
#include <utility>
#include <wtf/AlignedStorage.h>
#include <wtf/Assertions.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/DebugHeap.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashTraits.h>
#include <wtf/Lock.h>
#include <wtf/MathExtras.h>
#include <wtf/Packed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ValueCheck.h>
#include <wtf/WeakRandomNumber.h>

// Configuration of WTF::HashTable.
//  - 75% load factor for small tables.
//  - 50% load factor for large tables.
//  - One status byte per slot: EMPTY, DELETED, or full with 7 bits of the key's hash.
//  - Always use power-of-two hashtable size, which is also important to make triangular probing work.

#define DUMP_HASHTABLE_STATS 0
#define DUMP_HASHTABLE_STATS_PER_TABLE 0

#if DUMP_HASHTABLE_STATS_PER_TABLE
#include <wtf/DataLog.h>
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(HashTable);

struct StatusByte {
    static constexpr uint8_t empty = 0x00;
    static constexpr uint8_t deleted = 0x01;
    static constexpr uint8_t isFullBit = 0x80;

    static constexpr StatusByte forHash(unsigned hash, unsigned hashShift) { return { static_cast<uint8_t>((hash >> hashShift) | isFullBit) }; }

    constexpr bool isEmpty() const { return value == empty; }
    constexpr bool isDeleted() const { return value == deleted; }
    constexpr bool isFull() const { return value & isFullBit; }
    friend constexpr bool operator==(const StatusByte&, const StatusByte&) = default;

    uint8_t value;
};
static_assert(sizeof(StatusByte) == 1 && std::is_trivial_v<StatusByte>);

// 8 status bytes, with efficient filtering for full bytes.
class StatusWord {
public:
    StatusWord() = default;
    NODELETE SUPPRESS_NODELETE explicit StatusWord(const StatusByte* statusBytes)
    {
        memcpy(&m_bits, statusBytes, sizeof(m_bits)); // NOLINT
        m_bits &= isFullMask;
    }

    explicit operator bool() const { return m_bits; }
    unsigned firstFull() const { return std::countr_zero(m_bits) / 8; }
    void removeFirstFull() { m_bits &= m_bits - 1; }
    void removeBefore(unsigned slot) { m_bits &= ~uint64_t(0) << (8 * slot); }

private:
    static constexpr uint64_t isFullMask = StatusByte::isFullBit * 0x0101010101010101ULL;

    uint64_t m_bits { 0 };
};
static_assert(sizeof(StatusWord) == sizeof(uint64_t));

// Table sizes are powers of two and at least wordSize, so the status bytes are a whole number of words.
class StatusBytes {
public:
    static constexpr unsigned wordSize = sizeof(StatusWord);

    StatusBytes() = default;
    StatusBytes(StatusByte* bytes, unsigned size)
        : m_bytes(bytes)
        , m_size(size)
    {
        ASSERT(!(size % wordSize));
    }

    unsigned size() const { return m_size; }
    StatusByte* data() const { return m_bytes; }
    StatusByte& operator[](size_t index) const { return m_bytes[index]; }

    void clear() { memset(m_bytes, StatusByte::empty, m_size); } // NOLINT

    template<typename T, typename Functor>
    ALWAYS_INLINE void forEachFullSlot(T* slots, const Functor& functor) const
    {
        const StatusByte* bytes = m_bytes;
        for (T* word = slots; word != slots + m_size; word += wordSize, bytes += wordSize) {
            for (StatusWord statusWord(bytes); statusWord; statusWord.removeFirstFull()) {
                unsigned slot = statusWord.firstFull();
                functor(word[slot]);
            }
        }
    }

private:
    StatusByte* m_bytes;
    unsigned m_size;
};
static_assert(std::is_trivial_v<StatusBytes>);

// Enables internal WTF consistency checks that are invoked automatically. Non-WTF callers can call checkTableConsistency() even if internal checks are disabled.
#define CHECK_HASHTABLE_CONSISTENCY 0

#ifdef NDEBUG
#define CHECK_HASHTABLE_ITERATORS 0
#define CHECK_HASHTABLE_USE_AFTER_DESTRUCTION 0
#else
#define CHECK_HASHTABLE_ITERATORS 1
#define CHECK_HASHTABLE_USE_AFTER_DESTRUCTION 1
#endif

#if DUMP_HASHTABLE_STATS

    struct HashTableStats {
        // The following variables are all atomically incremented when modified.
        WTF_EXPORT_PRIVATE static std::atomic<unsigned> numAccesses;
        WTF_EXPORT_PRIVATE static std::atomic<unsigned> numRehashes;
        WTF_EXPORT_PRIVATE static std::atomic<unsigned> numRemoves;
        WTF_EXPORT_PRIVATE static std::atomic<unsigned> numReinserts;

        // The following variables are only modified in the recordCollisionAtCount method within a mutex.
        WTF_EXPORT_PRIVATE static unsigned maxCollisions;
        WTF_EXPORT_PRIVATE static unsigned numCollisions;
        WTF_EXPORT_PRIVATE static unsigned collisionGraph[4096];

        WTF_EXPORT_PRIVATE static void recordCollisionAtCount(unsigned count);
        WTF_EXPORT_PRIVATE static void dumpStats();
    };

#endif

    template<typename HashTable, typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTableIterator;

    template<typename HashTableType, typename IteratorType>
    void addIterator(const HashTableType*, IteratorType*);

    template<typename IteratorType>
    void removeIterator(IteratorType*);

    template<typename HashTableType>
    void invalidateIterators(const HashTableType*);

#if !CHECK_HASHTABLE_ITERATORS

    template<typename HashTableType, typename IteratorType>
    inline void addIterator(const HashTableType*, IteratorType*) { }

    template<typename IteratorType>
    inline void removeIterator(IteratorType*) { }

    template<typename HashTableType>
    void invalidateIterators(const HashTableType*) { }

#endif

    typedef enum { HashItemKnownGood } HashItemKnownGoodTag;

    template<typename HashTable, typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTableConstIterator {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(HashTableConstIterator);
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Value;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

    private:
        using HashTableType = HashTable;
        typedef HashTableIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits> iterator;
        typedef HashTableConstIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits> const_iterator;
        typedef Value ValueType;
        typedef const ValueType& ReferenceType;
        typedef const ValueType* PointerType;

        friend HashTableType;
        friend iterator;

        static constexpr unsigned wordSize = StatusBytes::wordSize;

        void scanWords()
        {
            while (m_position != m_endPosition) {
                m_statusWord = StatusWord(m_statusBytes);
                if (m_statusWord) {
                    m_position += m_statusWord.firstFull();
                    return;
                }
                m_position += wordSize;
                m_statusBytes += wordSize;
            }
        }

        void advanceFrom(unsigned slot)
        {
            if (m_statusWord) {
                m_position += m_statusWord.firstFull() - slot;
                return;
            }
            m_position += wordSize - slot;
            m_statusBytes += wordSize;
            scanWords();
        }

        void advance()
        {
            if (!m_statusWord)
                enterWord();
            unsigned slot = m_statusWord.firstFull();
            m_statusWord.removeFirstFull();
            advanceFrom(slot);
        }

        void skipWeakNullBuckets()
        {
            while (m_position != m_endPosition && HashTableType::isWeakNullBucket(*m_position))
                advance();
        }

        unsigned enterWord()
        {
            unsigned offset = static_cast<unsigned>(m_statusBytes - reinterpret_cast<const StatusByte*>(m_endPosition)) % wordSize;
            m_statusBytes -= offset;
            m_statusWord = StatusWord(m_statusBytes);
            m_statusWord.removeBefore(offset);
            return offset;
        }

        HashTableConstIterator(const HashTableType* table, PointerType position, PointerType endPosition, const StatusByte* statusByte)
            : m_position(position), m_endPosition(endPosition), m_statusBytes(statusByte)
        {
            addIterator(table, this);
            if (m_position == m_endPosition)
                return;
            advanceFrom(enterWord());
            skipWeakNullBuckets();
        }

        HashTableConstIterator(const HashTableType* table, PointerType position, PointerType endPosition, const StatusByte* statusByte, HashItemKnownGoodTag)
            : m_position(position), m_endPosition(endPosition), m_statusBytes(statusByte)
        {
            addIterator(table, this);
        }

    public:
        HashTableConstIterator()
        {
            addIterator(static_cast<const HashTableType*>(0), this);
        }

        // default copy, assignment and destructor are OK if CHECK_HASHTABLE_ITERATORS is 0

#if CHECK_HASHTABLE_ITERATORS
        ~HashTableConstIterator()
        {
            removeIterator(this);
        }

        HashTableConstIterator(const const_iterator& other)
            : m_position(other.m_position), m_endPosition(other.m_endPosition), m_statusBytes(other.m_statusBytes), m_statusWord(other.m_statusWord)
        {
            addIterator(other.m_table, this);
        }

        const_iterator& operator=(const const_iterator& other)
        {
            m_position = other.m_position;
            m_endPosition = other.m_endPosition;
            m_statusBytes = other.m_statusBytes;
            m_statusWord = other.m_statusWord;

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
            ASSERT(m_position != m_endPosition);
            advance();
            skipWeakNullBuckets();
            return *this;
        }

        // postfix ++ intentionally omitted

        // Comparison.
        bool operator==(const const_iterator& other) const
        {
            checkValidity(other);
            return m_position == other.m_position;
        }
        bool operator==(const iterator& other) const
        {
            return *this == static_cast<const_iterator>(other);
        }

    private:
        void checkValidity() const
        {
#if CHECK_HASHTABLE_ITERATORS
            ASSERT(m_table);
#endif
        }


#if CHECK_HASHTABLE_ITERATORS
        void checkValidity(const const_iterator& other) const
        {
            ASSERT(m_table);
            ASSERT_UNUSED(other, other.m_table);
            ASSERT(m_table == other.m_table);
        }
#else
        void checkValidity(const const_iterator&) const { }
#endif

        PointerType m_position { nullptr };
        PointerType m_endPosition { nullptr };
        const StatusByte* m_statusBytes { nullptr };
        StatusWord m_statusWord;

#if CHECK_HASHTABLE_ITERATORS
    public:
        // Any modifications of the m_next or m_previous of an iterator that is in a linked list of a HashTable::m_iterator,
        // should be guarded with m_table->m_mutex.
        mutable const HashTableType* m_table;
        mutable const_iterator* m_next;
        mutable const_iterator* m_previous;
#endif
    };

    template<typename HashTable, typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
    class HashTableIterator {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(HashTableIterator);
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Value;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

    private:
        using HashTableType = HashTable;
        typedef HashTableIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits> iterator;
        typedef HashTableConstIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits> const_iterator;
        typedef Value ValueType;
        typedef ValueType& ReferenceType;
        typedef ValueType* PointerType;

        friend HashTableType;

        HashTableIterator(HashTableType* table, PointerType pos, PointerType end, const StatusByte* statusByte)
            : m_iterator(table, pos, end, statusByte)
        {
        }

        HashTableIterator(HashTableType* table, PointerType pos, PointerType end, const StatusByte* statusByte, HashItemKnownGoodTag tag)
            : m_iterator(table, pos, end, statusByte, tag)
        {
        }

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
        bool operator==(const const_iterator& other) const { return m_iterator == other; }

        operator const_iterator() const { return m_iterator; }

    private:
        const_iterator m_iterator;
    };

    template<typename ValueTraits, typename HashFunctions>
    class IdentityHashTranslator {
    public:
        static unsigned hash(const auto& key) { return HashFunctions::hash(key); }
        static bool equal(const auto& a, const auto& b) { return HashFunctions::equal(a, b); }
        static void translate(typename ValueTraits::TraitType* location, const auto&, NOESCAPE const Invocable<typename ValueTraits::TraitType()> auto& functor)
        {
            std::construct_at(location, functor());
        }
    };

    template<typename IteratorType> struct HashTableAddResult {
        HashTableAddResult() : isNewEntry(false) { }
        HashTableAddResult(IteratorType iter, bool isNewEntry) : iterator(iter), isNewEntry(isNewEntry) { }
        IteratorType iterator;
        bool isNewEntry;

        explicit operator bool() const { return isNewEntry; }
    };

    // HashTableCapacityForSize computes the upper power of two capacity to hold the size parameter.
    // This is done at compile time to initialize the HashTraits.
    template<unsigned size>
    struct HashTableCapacityForSize {
        // Load-factor for small table is 75%.
        static constexpr unsigned smallMaxLoadNumerator = 3;
        static constexpr unsigned smallMaxLoadDenominator = 4;
        // Load-factor for large table is 50%.
        static constexpr unsigned largeMaxLoadNumerator = 1;
        static constexpr unsigned largeMaxLoadDenominator = 2;
        static constexpr unsigned maxSmallTableCapacity = 1024;
        static constexpr unsigned minLoad = 6;

        static constexpr bool shouldExpand(uint64_t keyAndDeleteCount, uint64_t tableSize)
        {
            if (tableSize <= maxSmallTableCapacity)
                return keyAndDeleteCount * smallMaxLoadDenominator >= tableSize * smallMaxLoadNumerator;
            return keyAndDeleteCount * largeMaxLoadDenominator >= tableSize * largeMaxLoadNumerator;
        }

        static constexpr unsigned capacityForSize(uint32_t sizeArg)
        {
            if (!sizeArg)
                return 0;
            constexpr unsigned maxCapacity = 1U << 31;
            UNUSED_PARAM(maxCapacity);
            ASSERT_UNDER_CONSTEXPR_CONTEXT(sizeArg <= maxCapacity);
            uint32_t capacity = roundUpToPowerOfTwo(sizeArg);
            ASSERT_UNDER_CONSTEXPR_CONTEXT(capacity <= maxCapacity);
            if (shouldExpand(sizeArg, capacity)) {
                ASSERT_UNDER_CONSTEXPR_CONTEXT((static_cast<uint64_t>(capacity) * 2) <= maxCapacity);
                return capacity * 2;
            }
            return capacity;
        }

        static constexpr unsigned value = capacityForSize(size);
        static_assert(size > 0, "HashTableNonZeroMinimumCapacity");
        static_assert(!static_cast<unsigned>(value >> 31), "HashTableNoCapacityOverflow");
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    class HashTable {
    public:
        using HashTableType = HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>;
        typedef HashTableIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits> iterator;
        typedef HashTableConstIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits> const_iterator;
        typedef Traits ValueTraits;
        typedef Key KeyType;
        typedef Value ValueType;
        using TakeType = typename ValueTraits::TakeType;
        typedef IdentityHashTranslator<ValueTraits, HashFunctions> IdentityTranslatorType;
        typedef HashTableAddResult<iterator> AddResult;

        using HashTableSizePolicy = HashTableCapacityForSize<1>;

#if DUMP_HASHTABLE_STATS_PER_TABLE
        struct Stats {
            WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(Stats);

            Stats()
                : numAccesses(0)
                , numRehashes(0)
                , numRemoves(0)
                , numReinserts(0)
                , maxCollisions(0)
                , numCollisions(0)
                , collisionGraph()
            {
            }

            unsigned numAccesses;
            unsigned numRehashes;
            unsigned numRemoves;
            unsigned numReinserts;

            unsigned maxCollisions;
            unsigned numCollisions;
            unsigned collisionGraph[4096];

            void recordCollisionAtCount(unsigned count)
            {
                if (count > maxCollisions)
                    maxCollisions = count;
                numCollisions++;
                collisionGraph[count]++;
            }

            void dumpStats()
            {
                dataLogF("\nWTF::HashTable::Stats dump\n\n");
                dataLogF("%d accesses\n", numAccesses);
                dataLogF("%d total collisions, average %.2f probes per access\n", numCollisions, 1.0 * (numAccesses + numCollisions) / numAccesses);
                dataLogF("longest collision chain: %d\n", maxCollisions);
                for (unsigned i = 1; i <= maxCollisions; i++) {
                    dataLogF("  %d lookups with exactly %d collisions (%.2f%% , %.2f%% with this many or more)\n", collisionGraph[i], i, 100.0 * (collisionGraph[i] - collisionGraph[i+1]) / numAccesses, 100.0 * collisionGraph[i] / numAccesses);
                }
                dataLogF("%d rehashes\n", numRehashes);
                dataLogF("%d reinserts\n", numReinserts);
            }
        };
#endif

        HashTable();
        ~HashTable() 
        {
            invalidateIterators(this); 
            if (auto* table = std::exchange(m_table, nullptr))
                deallocateTable(table);
#if CHECK_HASHTABLE_USE_AFTER_DESTRUCTION
            m_table = (ValueType*)(uintptr_t)0xbbadbeef;
#endif
        }

        HashTable(const HashTable&);
        void swap(HashTable&);
        HashTable& operator=(const HashTable&);

        HashTable(HashTable&&);
        HashTable& operator=(HashTable&&);

        // When the hash table is empty, just return the same iterator for end as for begin.
        // This is more efficient because we don't have to skip all the empty and deleted
        // buckets, and iterating an empty table is a common case that's worth optimizing.
        iterator begin() LIFETIME_BOUND { return isEmpty() ? end() : makeIterator(m_table); }
        iterator end() LIFETIME_BOUND { return makeKnownGoodIterator(m_table + tableSize()); }
        const_iterator begin() const LIFETIME_BOUND { return isEmpty() ? end() : makeConstIterator(m_table); }
        const_iterator end() const LIFETIME_BOUND { return makeKnownGoodConstIterator(m_table + tableSize()); }

        iterator random() LIFETIME_BOUND
        {
            if (isEmpty())
                return end();

            StatusBytes statusBytes = this->statusBytes();
            while (true) {
                unsigned index = weakRandomNumber<uint32_t>() & (tableSize() - 1);
                if (statusBytes[index].isFull())
                    return makeKnownGoodIterator(m_table + index);
            };
        }

        const_iterator random() const LIFETIME_BOUND { return static_cast<const_iterator>(const_cast<HashTable*>(this)->random()); }

        unsigned size() const { return keyCount(); }
        unsigned capacity() const { return tableSize(); }
        size_t byteSize() const { return m_table ? allocationSize(tableSize()) : 0; }
        bool isEmpty() const { return !keyCount(); }

        void reserveInitialCapacity(unsigned keyCount)
        {
            ASSERT(!m_table);
            ASSERT(!tableSize());

            unsigned newTableSize = computeBestTableSize(keyCount);

            m_table = allocateTable(newTableSize);
            setTableSize(newTableSize);
            setHashShift(hashShiftForSize(newTableSize));
            setDeletedCount(0);
            setKeyCount(0);
        }

        template<ShouldValidateKey shouldValidateKey = ShouldValidateKey::Yes> AddResult add(const ValueType& value) LIFETIME_BOUND { return add<IdentityTranslatorType, shouldValidateKey>(Extractor::extract(value), [&]() ALWAYS_INLINE_LAMBDA { return value; }); }
        template<ShouldValidateKey shouldValidateKey = ShouldValidateKey::Yes> AddResult add(ValueType&& value) LIFETIME_BOUND { return add<IdentityTranslatorType, shouldValidateKey>(Extractor::extract(value), [&]() ALWAYS_INLINE_LAMBDA { return WTF::move(value); }); }

        // A special version of add() that finds the object by hashing and comparing
        // with some other type, to avoid the cost of type conversion if the object is already
        // in the table.
        template<typename HashTranslator, ShouldValidateKey> AddResult add(auto&& key, NOESCAPE const std::invocable<> auto& functor) LIFETIME_BOUND;
        template<typename HashTranslator, ShouldValidateKey> AddResult addPassingHashCode(auto&& key, NOESCAPE const std::invocable<> auto& functor) LIFETIME_BOUND;

        template<ShouldValidateKey shouldValidateKey = ShouldValidateKey::Yes> iterator find(const KeyType& key) LIFETIME_BOUND { return find<IdentityTranslatorType, shouldValidateKey>(key); }
        template<ShouldValidateKey shouldValidateKey = ShouldValidateKey::Yes> const_iterator find(const KeyType& key) const LIFETIME_BOUND { return find<IdentityTranslatorType, shouldValidateKey>(key); }
        template<ShouldValidateKey shouldValidateKey = ShouldValidateKey::Yes> bool contains(const KeyType& key) const { return contains<IdentityTranslatorType, shouldValidateKey>(key); }

        template<typename HashTranslator, ShouldValidateKey, typename T> iterator find(const T&) LIFETIME_BOUND;
        template<typename HashTranslator, ShouldValidateKey, typename T> const_iterator find(const T&) const LIFETIME_BOUND;
        template<typename HashTranslator, ShouldValidateKey, typename T> bool contains(const T&) const;

        template<ShouldValidateKey> void remove(const KeyType&);
        void remove(iterator);
        void removeWithoutEntryConsistencyCheck(iterator);
        void removeWithoutEntryConsistencyCheck(const_iterator);
        // FIXME: This feels like it should be Invocable<bool(const ValueType&)> but that breaks many HashMap users.
        bool removeIf(NOESCAPE const Invocable<bool(ValueType&)> auto&);
        void clear();

        // Useful when the key type is WeakPtr
        size_t computeSize() const requires (KeyTraits::hasIsWeakNullValueFunction);
        bool isEmptyIgnoringNullReferences() const requires (KeyTraits::hasIsWeakNullValueFunction);
        void removeWeakNullEntries() const requires (KeyTraits::hasIsWeakNullValueFunction);

        template<size_t inlineCapacity>
        Vector<TakeType, inlineCapacity> takeIf(NOESCAPE const Invocable<bool(const ValueType&)> auto&);

        static bool isWeakNullBucket(const ValueType& value) { return isHashTraitsWeakNullValue<KeyTraits>(Extractor::extract(value)); }

        template<ShouldValidateKey shouldValidateKey = ShouldValidateKey::Yes> ValueType* lookup(const Key& key) { return lookup<IdentityTranslatorType, shouldValidateKey>(key); }
        template<typename HashTranslator, ShouldValidateKey = ShouldValidateKey::Yes, typename T> ValueType* lookup(const T&);
        template<typename HashTranslator, ShouldValidateKey, typename T> ValueType* inlineLookup(const T&);

        ALWAYS_INLINE bool isNullStorage() const { return !m_table; }

#if ASSERT_ENABLED
        void checkTableConsistency() const;
#else
        static void checkTableConsistency() { }
#endif
#if CHECK_HASHTABLE_CONSISTENCY
        void internalCheckTableConsistency() const { checkTableConsistency(); }
        void internalCheckTableConsistencyExceptSize() const { checkTableConsistencyExceptSize(); }
#else
        static void internalCheckTableConsistencyExceptSize() { }
        static void internalCheckTableConsistency() { }
#endif

    private:
        static_assert(hasOneBitSet(KeyTraits::minimumTableSize) && KeyTraits::minimumTableSize >= StatusBytes::wordSize);

        static ValueType* allocateTable(unsigned size);
        static void deallocateTable(ValueType* table);

        struct WriteLocation {
            ValueType* table;
            StatusBytes statusBytes;
            unsigned index;
            unsigned hash;
            StatusByte hashByte;
            bool found;
            bool isDeleted;

            ValueType* entry() const { return table + index; }
        };

        struct ReinsertLocation {
            ValueType* entry;
            StatusByte* statusByte;
            StatusByte hashByte;
        };
        struct ReinsertTable {
            ValueType* table;
            StatusBytes statusBytes;
            unsigned hashShift;
            unsigned sizeMask;
        };
        static ReinsertTable makeReinsertTable(ValueType* table, unsigned tableSize)
        {
            unsigned hashShift = hashShiftForSize(tableSize);
            return { table, statusBytesForTable(table, tableSize), hashShift, tableSize - 1 };
        }
        ReinsertLocation lookupForReinsert(const ReinsertTable& target, const Key& key) { return lookupForReinsert<IdentityTranslatorType>(target, key); }
        template<typename HashTranslator, typename T> ReinsertLocation lookupForReinsert(const ReinsertTable&, const T&);
        template<typename HashTranslator, typename T> WriteLocation fullLookupForWriting(const T&);

        void writeStatusByte(const WriteLocation&);
        static void writeStatusByte(const ReinsertLocation&);

        void removeAndInvalidateWithoutEntryConsistencyCheck(ValueType*);
        void removeAndInvalidate(ValueType*);
        void remove(ValueType*);
        void removeWithoutShrinking(ValueType*);

        void deleteWeakNullEntries();

        static constexpr unsigned computeBestTableSize(unsigned keyCount);
        bool shouldExpand() const { return HashTableSizePolicy::shouldExpand(keyCount() + deletedCount(), tableSize()); }
        bool mustRehashInPlace() const { return keyCount() * minLoad < tableSize() * 2; }
        // Prevent deleted slots from creating long probes.
        bool shouldRehashForDeletedSlots() const { return deletedCount() * 4 > tableSize(); }
        static bool shouldShrink(unsigned keyCount, unsigned tableSize) { return keyCount * minLoad < tableSize && tableSize > KeyTraits::minimumTableSize; }
        bool shouldShrink() const { return shouldShrink(keyCount(), tableSize()); }
        ValueType* expand(ValueType* entry = nullptr);
        void shrink() { rehash(tableSize() / 2, nullptr); }
        void shrinkToBestSize();

        ValueType* rehash(Checked<unsigned> newTableSize, ValueType* entry);
        ValueType* reinsert(const ReinsertTable&, ValueType&&);

        const StatusByte* statusByteFor(const ValueType* pos) const { return m_table ? statusBytes().data() + (pos - m_table) : nullptr; }

        iterator makeIterator(ValueType* pos) { return iterator(this, pos, m_table + tableSize(), statusByteFor(pos)); }
        const_iterator makeConstIterator(ValueType* pos) const { return const_iterator(this, pos, m_table + tableSize(), statusByteFor(pos)); }
        iterator makeKnownGoodIterator(ValueType* pos) { return iterator(this, pos, m_table + tableSize(), statusByteFor(pos), HashItemKnownGood); }
        iterator makeKnownGoodIterator(const WriteLocation& location) { return iterator(this, location.entry(), location.table + location.statusBytes.size(), &location.statusBytes[location.index], HashItemKnownGood); }
        const_iterator makeKnownGoodConstIterator(ValueType* pos) const { return const_iterator(this, pos, m_table + tableSize(), statusByteFor(pos), HashItemKnownGood); }

#if ASSERT_ENABLED
        void checkTableConsistencyExceptSize() const;
#else
        static void checkTableConsistencyExceptSize() { }
#endif

        // Load-factor for small table is 75%.
        static constexpr unsigned smallMaxLoadNumerator = HashTableSizePolicy::smallMaxLoadNumerator;
        static constexpr unsigned smallMaxLoadDenominator = HashTableSizePolicy::smallMaxLoadDenominator;
        // Load-factor for large table is 50%.
        static constexpr unsigned largeMaxLoadNumerator = HashTableSizePolicy::largeMaxLoadNumerator;
        static constexpr unsigned largeMaxLoadDenominator = HashTableSizePolicy::largeMaxLoadDenominator;
        static constexpr unsigned maxSmallTableCapacity = HashTableSizePolicy::maxSmallTableCapacity;
        static constexpr unsigned minLoad = HashTableSizePolicy::minLoad;

        static constexpr int tableSizeOffset = -1;
        static constexpr int hashShiftOffset = -2;
        static constexpr int keyCountOffset = -3;
        static constexpr int deletedCountOffset = -4;
        static constexpr unsigned metadataSize = std::max(4 * sizeof(unsigned), alignof(ValueType));

        unsigned tableSize() const { return m_table ? reinterpret_cast_ptr<unsigned*>(m_table)[tableSizeOffset] : 0; }
        void setTableSize(unsigned size) const { ASSERT(m_table); reinterpret_cast_ptr<unsigned*>(m_table)[tableSizeOffset] = size; }
        void setHashShift(unsigned shift) { ASSERT(m_table); reinterpret_cast_ptr<unsigned*>(m_table)[hashShiftOffset] = shift; }
        unsigned keyCount() const { return m_table ? reinterpret_cast_ptr<unsigned*>(m_table)[keyCountOffset] : 0; }
        void setKeyCount(unsigned count) const { ASSERT(m_table); reinterpret_cast_ptr<unsigned*>(m_table)[keyCountOffset] = count; }
        unsigned deletedCount() const { ASSERT(m_table); return reinterpret_cast_ptr<unsigned*>(m_table)[deletedCountOffset]; }
        void setDeletedCount(unsigned count) const { ASSERT(m_table); reinterpret_cast_ptr<unsigned*>(m_table)[deletedCountOffset] = count; }

        // An allocation is metadata, then tableSize slots, then tableSize status bytes; m_table points at the first slot.
        static size_t allocationSize(unsigned tableSize) { return tableSize + metadataSize + static_cast<size_t>(tableSize) * sizeof(ValueType); }
        static StatusBytes statusBytesForTable(ValueType* table, unsigned tableSize) { return { reinterpret_cast<StatusByte*>(table + tableSize), tableSize }; }
        static void* mallocPointerForTable(ValueType* table) { return reinterpret_cast<uint8_t*>(table) - metadataSize; }
        static unsigned tableSizeForTable(const ValueType* table) { return reinterpret_cast_ptr<const unsigned*>(table)[tableSizeOffset]; }
        static unsigned hashShiftForTable(const ValueType* table) { return reinterpret_cast_ptr<const unsigned*>(table)[hashShiftOffset]; }
        static unsigned& keyCountForTable(ValueType* table) { return reinterpret_cast_ptr<unsigned*>(table)[keyCountOffset]; }
        static constexpr unsigned hashShiftForSize(unsigned tableSize) { return std::countr_zero(tableSize); }
        StatusBytes statusBytes() const { ASSERT(m_table); return statusBytesForTable(m_table, tableSize()); }

        ValueType* m_table { nullptr };

#if CHECK_HASHTABLE_ITERATORS
    public:
        // All access to m_iterators should be guarded with m_mutex.
        mutable const_iterator* m_iterators;
        // Use std::unique_ptr so HashTable can still be memmove'd or memcpy'ed.
        mutable std::unique_ptr<Lock> m_mutex;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
    public:
        mutable std::unique_ptr<Stats> m_stats;
#endif
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    inline HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::HashTable()
        : m_table(nullptr)
#if CHECK_HASHTABLE_ITERATORS
        , m_iterators(0)
        , m_mutex(makeUnique<Lock>())
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
        , m_stats(makeUnique<Stats>())
#endif
    {
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
    inline auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::lookup(const T& key) -> ValueType*
    {
        return inlineLookup<HashTranslator, shouldValidateKey>(key);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
    ALWAYS_INLINE auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::inlineLookup(const T& key) -> ValueType*
    {
        static_assert(sizeof(Value) <= 150, "Your HashTable types are too big to efficiently move when rehashing.  Consider using UniqueRef instead");

        ValueType* table = m_table;
        if (!table)
            return nullptr;

        unsigned tableSize = tableSizeForTable(table);
        unsigned h = HashTranslator::hash(key);
        StatusByte hashByte = StatusByte::forHash(h, hashShiftForTable(table));
        StatusBytes statusBytes = statusBytesForTable(table, tableSize);
        unsigned probeCount = 0;

#if DUMP_HASHTABLE_STATS
        ++HashTableStats::numAccesses;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numAccesses;
#endif

        unsigned sizeMask = tableSize - 1;
        unsigned index = h & sizeMask;
        StatusByte statusByte = statusBytes[index];
        if (statusByte.isEmpty())
            return nullptr;
        if (statusByte == hashByte && HashTranslator::equal(Extractor::extract(table[index]), key))
            return table + index;
        while (true) {
            ++probeCount;

#if DUMP_HASHTABLE_STATS
            HashTableStats::recordCollisionAtCount(probeCount);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
            m_stats->recordCollisionAtCount(probeCount);
#endif

            index = (index + probeCount) & sizeMask;
            statusByte = statusBytes[index];
            if (statusByte == hashByte) {
                if (HashTranslator::equal(Extractor::extract(table[index]), key))
                    return table + index;
            } else if (statusByte.isEmpty())
                return nullptr;
        }
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    template<typename HashTranslator, typename T>
    inline auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::lookupForReinsert(const ReinsertTable& target, const T& key) -> ReinsertLocation
    {
        ASSERT(target.table);

        unsigned h = HashTranslator::hash(key);
        StatusByte hashByte = StatusByte::forHash(h, target.hashShift);
        unsigned probeCount = 0;

#if DUMP_HASHTABLE_STATS
        ++HashTableStats::numAccesses;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numAccesses;
#endif

        for (unsigned index = h & target.sizeMask; ; index = (index + probeCount) & target.sizeMask) {
            if (target.statusBytes[index].isEmpty()) [[likely]]
                return { target.table + index, &target.statusBytes[index], hashByte };

            ++probeCount;

#if DUMP_HASHTABLE_STATS
            HashTableStats::recordCollisionAtCount(probeCount);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
            m_stats->recordCollisionAtCount(probeCount);
#endif
        }
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    template<typename HashTranslator, typename T>
    ALWAYS_INLINE auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::fullLookupForWriting(const T& key) -> WriteLocation
    {
        ASSERT(m_table);

        ValueType* table = m_table;
        unsigned tableSize = tableSizeForTable(table);
        unsigned h = HashTranslator::hash(key);
        StatusByte hashByte = StatusByte::forHash(h, hashShiftForTable(table));
        StatusBytes statusBytes = statusBytesForTable(table, tableSize);
        unsigned probeCount = 0;

#if DUMP_HASHTABLE_STATS
        ++HashTableStats::numAccesses;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numAccesses;
#endif

        unsigned sizeMask = tableSize - 1;
        unsigned index = h & sizeMask;
        StatusByte statusByte = statusBytes[index];
        if (statusByte == hashByte && HashTranslator::equal(Extractor::extract(table[index]), key))
            return { table, statusBytes, index, h, hashByte, true, false };
        if (statusByte.isEmpty())
            return { table, statusBytes, index, h, hashByte, false, false };
        constexpr unsigned noDeletedIndex = std::numeric_limits<unsigned>::max();
        unsigned deletedIndex = noDeletedIndex;
        while (true) {
            deletedIndex = statusByte.isDeleted() ? index : deletedIndex;
            ++probeCount;

#if DUMP_HASHTABLE_STATS
            HashTableStats::recordCollisionAtCount(probeCount);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
            m_stats->recordCollisionAtCount(probeCount);
#endif

            index = (index + probeCount) & sizeMask;
            statusByte = statusBytes[index];
            if (statusByte == hashByte) {
                if (HashTranslator::equal(Extractor::extract(table[index]), key))
                    return { table, statusBytes, index, h, hashByte, true, false };
            } else if (statusByte.isEmpty()) [[unlikely]] {
                bool isDeleted = deletedIndex != noDeletedIndex;
                return { table, statusBytes, isDeleted ? deletedIndex : index, h, hashByte, false, isDeleted };
            }
        }
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    ALWAYS_INLINE void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::writeStatusByte(const ReinsertLocation& location)
    {
        *location.statusByte = location.hashByte;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    ALWAYS_INLINE void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::writeStatusByte(const WriteLocation& location)
    {
        if (location.isDeleted)
            setDeletedCount(deletedCount() - 1);
        location.statusBytes[location.index] = location.hashByte;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
    ALWAYS_INLINE auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::add(T&& key, NOESCAPE const std::invocable<> auto& functor) LIFETIME_BOUND -> AddResult
    {
        invalidateIterators(this);

        if (!m_table) [[unlikely]]
            expand();

        internalCheckTableConsistency();

        auto location = fullLookupForWriting<HashTranslator>(key);
        if (location.found)
            return AddResult(makeKnownGoodIterator(location), false);

        ValueType* entry = location.entry();
        // Passing a prvalue keeps a translator that takes auto& from binding to entry.
        HashTranslator::translate(static_cast<ValueType*>(entry), std::forward<T>(key), functor);
        writeStatusByte(location);
        setKeyCount(keyCountForTable(m_table) + 1);

        if (HashTableSizePolicy::shouldExpand(keyCountForTable(m_table) + deletedCount(), tableSizeForTable(m_table))) {
            entry = expand(entry);
            internalCheckTableConsistency();
            return AddResult(makeKnownGoodIterator(entry), true);
        }

        internalCheckTableConsistency();

        return AddResult(makeKnownGoodIterator(location), true);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
    inline auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::addPassingHashCode(T&& key, NOESCAPE const std::invocable<> auto& functor) LIFETIME_BOUND -> AddResult
    {
        invalidateIterators(this);

        if (!m_table) [[unlikely]]
            expand();

        internalCheckTableConsistency();

        auto location = fullLookupForWriting<HashTranslator>(key);
        if (location.found)
            return AddResult(makeKnownGoodIterator(location), false);

        ValueType* entry = location.entry();
        HashTranslator::translate(static_cast<ValueType*>(entry), std::forward<T>(key), functor, location.hash);
        writeStatusByte(location);
        setKeyCount(keyCountForTable(m_table) + 1);

        if (HashTableSizePolicy::shouldExpand(keyCountForTable(m_table) + deletedCount(), tableSizeForTable(m_table))) {
            entry = expand(entry);
            internalCheckTableConsistency();
            return AddResult(makeKnownGoodIterator(entry), true);
        }

        internalCheckTableConsistency();

        return AddResult(makeKnownGoodIterator(location), true);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    inline auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::reinsert(const ReinsertTable& target, ValueType&& entry) -> ValueType*
    {
#if DUMP_HASHTABLE_STATS
        ++HashTableStats::numReinserts;
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numReinserts;
#endif

        auto location = lookupForReinsert(target, Extractor::extract(entry));
        std::construct_at(location.entry, WTF::move(entry));
        writeStatusByte(location);
        return location.entry;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    template <typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
    auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::find(const T& key) LIFETIME_BOUND -> iterator
    {
        if (!m_table)
            return end();

        ValueType* entry = lookup<HashTranslator, shouldValidateKey>(key);
        if (!entry)
            return end();

        return makeKnownGoodIterator(entry);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    template <typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
    auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::find(const T& key) const LIFETIME_BOUND -> const_iterator
    {
        if (!m_table)
            return end();

        ValueType* entry = const_cast<HashTable*>(this)->lookup<HashTranslator, shouldValidateKey>(key);
        if (!entry)
            return end();

        return makeKnownGoodConstIterator(entry);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    template <typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
    bool HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::contains(const T& key) const
    {
        if (!m_table)
            return false;

        return const_cast<HashTable*>(this)->lookup<HashTranslator, shouldValidateKey>(key);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeAndInvalidateWithoutEntryConsistencyCheck(ValueType* pos)
    {
        invalidateIterators(this);
        remove(pos);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeAndInvalidate(ValueType* pos)
    {
        invalidateIterators(this);
        internalCheckTableConsistency();
        remove(pos);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeWithoutShrinking(ValueType* pos)
    {
        ValueType* table = m_table;
        unsigned index = pos - table;
        StatusBytes statusBytes = statusBytesForTable(table, tableSizeForTable(table));
        ASSERT(statusBytes[index].isFull());

        keyCountForTable(table)--;
        statusBytes[index] = StatusByte { StatusByte::deleted };
        setDeletedCount(deletedCount() + 1);
        std::destroy_at(pos);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::remove(ValueType* pos)
    {
#if DUMP_HASHTABLE_STATS
        ++HashTableStats::numRemoves;
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numRemoves;
#endif

        removeWithoutShrinking(pos);

        if (shouldShrink(keyCountForTable(m_table), tableSizeForTable(m_table)))
            shrink();
        else if (shouldRehashForDeletedSlots())
            rehash(tableSize(), nullptr);

        internalCheckTableConsistency();
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::remove(iterator it)
    {
        if (it == end())
            return;

        removeAndInvalidate(const_cast<ValueType*>(it.get()));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeWithoutEntryConsistencyCheck(iterator it)
    {
        if (it == end())
            return;

        removeAndInvalidateWithoutEntryConsistencyCheck(const_cast<ValueType*>(it.get()));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeWithoutEntryConsistencyCheck(const_iterator it)
    {
        if (it == end())
            return;

        removeAndInvalidateWithoutEntryConsistencyCheck(const_cast<ValueType*>(it.get()));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    template<ShouldValidateKey shouldValidateKey>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::remove(const KeyType& key)
    {
        remove(find<shouldValidateKey>(key));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    inline size_t HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::computeSize() const requires (KeyTraits::hasIsWeakNullValueFunction)
    {
        removeWeakNullEntries();
        return size();
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    inline bool HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::isEmptyIgnoringNullReferences() const requires (KeyTraits::hasIsWeakNullValueFunction)
    {
        // FIXME: We should probably return isEmpty() || !computeSize() here to avoid pathological weak null iteration in an empty table.
        return isEmpty() || begin() == end(); // Iterators skip weak null
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeWeakNullEntries() const requires (KeyTraits::hasIsWeakNullValueFunction)
    {
        const_cast<HashTable&>(*this).removeIf([](ValueType&) {
            return false;
        });
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::deleteWeakNullEntries()
    {
        if (!m_table)
            return;

        StatusBytes statusBytes = this->statusBytes();
        for (unsigned i = tableSize(); i--;) {
            if (statusBytes[i].isFull() && isWeakNullBucket(m_table[i]))
                removeWithoutShrinking(m_table + i);
        }

        internalCheckTableConsistencyExceptSize();
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    inline bool HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeIf(NOESCAPE const Invocable<bool(ValueType&)> auto& functor)
    {
        if (!m_table)
            return false;

        unsigned removedBucketCount = 0;
        ValueType* table = m_table;
        StatusBytes statusBytes = this->statusBytes();

        for (unsigned i = tableSize(); i--;) {
            if (!statusBytes[i].isFull())
                continue;

            ValueType& bucket = table[i];
            if (!isWeakNullBucket(bucket)) {
                if (!functor(bucket))
                    continue;
            }

            removeWithoutShrinking(&bucket);
            ++removedBucketCount;
        }

        if (shouldShrink())
            shrinkToBestSize();
        else if (shouldRehashForDeletedSlots())
            rehash(tableSize(), nullptr);

        internalCheckTableConsistency();
        return removedBucketCount;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    template<size_t inlineCapacity>
    inline auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::takeIf(NOESCAPE const Invocable<bool(const ValueType&)> auto& functor) -> Vector<TakeType, inlineCapacity>
    {
        Vector<TakeType, inlineCapacity> result;

        removeIf([&] (ValueType& value) {
            if (!functor(value))
                return false;

            result.append(ValueTraits::take(WTF::move(value)));
            return true;
        });

        return result;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::allocateTable(unsigned size) -> ValueType*
    {
        static_assert(!(metadataSize % alignof(ValueType)));

        ValueType* table = reinterpret_cast_ptr<ValueType*>(static_cast<uint8_t*>(Malloc::malloc(allocationSize(size))) + metadataSize);
        statusBytesForTable(table, size).clear();
        return table;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::deallocateTable(ValueType* table)
    {
        if constexpr (!std::is_trivially_destructible_v<ValueType>) {
            unsigned size = tableSizeForTable(table);
            statusBytesForTable(table, size).forEachFullSlot(table, [](ValueType& value) ALWAYS_INLINE_LAMBDA {
                std::destroy_at(std::addressof(value));
            });
        }
        Malloc::free(mallocPointerForTable(table));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::expand(ValueType* entry) -> ValueType*
    {
        if constexpr (KeyTraits::hasIsWeakNullValueFunction)
            deleteWeakNullEntries();

        Checked<unsigned> newSize;
        Checked<unsigned> oldSize = tableSize();
        if (!oldSize)
            newSize = KeyTraits::minimumTableSize;
        else if (mustRehashInPlace() || shouldRehashForDeletedSlots())
            newSize = oldSize;
        else
            newSize = oldSize * 2;

        ALWAYS_INLINE_CALLEE return rehash(newSize, entry);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    constexpr unsigned HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::computeBestTableSize(unsigned keyCount)
    {
        Checked<unsigned> bestTableSize = roundUpToPowerOfTwo(keyCount);
        constexpr double minLoadRatio = 1.0 / minLoad;

        if (HashTableSizePolicy::shouldExpand(keyCount, bestTableSize))
            bestTableSize *= 2;

        auto aboveThresholdForEagerExpansion = [](double loadFactor, unsigned keyCount, unsigned tableSize)
        {
            // Here is the rationale behind this calculation, using 3/4 load-factor.
            // With maxLoad at 3/4 and minLoad at 1/6, our average load is 11/24.
            // If we are getting half-way between 11/24 and 3/4, we double the size
            // to avoid being too close to loadMax and bring the ratio close to 11/24. This
            // give us a load in the bounds [9/24, 15/24).
            double maxLoadRatio = loadFactor;
            double averageLoadRatio = std::midpoint(minLoadRatio, maxLoadRatio);
            double halfWayBetweenAverageAndMaxLoadRatio = std::midpoint(averageLoadRatio, maxLoadRatio);
            return keyCount >= tableSize * halfWayBetweenAverageAndMaxLoadRatio;
        };

        if (bestTableSize <= maxSmallTableCapacity) {
            constexpr double smallLoadFactor = static_cast<double>(smallMaxLoadNumerator) / smallMaxLoadDenominator;
            if (aboveThresholdForEagerExpansion(smallLoadFactor, keyCount, bestTableSize))
                bestTableSize *= 2;
        } else {
            constexpr double largeLoadFactor = static_cast<double>(largeMaxLoadNumerator) / largeMaxLoadDenominator;
            if (aboveThresholdForEagerExpansion(largeLoadFactor, keyCount, bestTableSize))
                bestTableSize *= 2;
        }
        return std::max(bestTableSize.value(), KeyTraits::minimumTableSize);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::shrinkToBestSize()
    {
        rehash(computeBestTableSize(keyCount()), nullptr);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::rehash(Checked<unsigned> newTableSize, ValueType* entry) -> ValueType*
    {
        internalCheckTableConsistencyExceptSize();

        ValueType* oldTable = m_table;
        StatusBytes oldStatusBytes = oldTable ? statusBytes() : StatusBytes { nullptr, 0 };

#if DUMP_HASHTABLE_STATS
        if (oldTable)
            ++HashTableStats::numRehashes;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        if (oldTable)
            ++m_stats->numRehashes;
#endif

        unsigned oldKeyCount = keyCount();
        ValueType* table = allocateTable(newTableSize);
        m_table = table;
        setTableSize(newTableSize);
        setHashShift(hashShiftForSize(newTableSize));
        setDeletedCount(0);
        setKeyCount(oldKeyCount);

        auto target = makeReinsertTable(table, newTableSize);
        Value* newEntry = nullptr;
        oldStatusBytes.forEachFullSlot(oldTable, [&](ValueType& oldEntry) ALWAYS_INLINE_LAMBDA {
            if (isWeakNullBucket(oldEntry)) {
                ASSERT(std::addressof(oldEntry) != entry);
                std::destroy_at(std::addressof(oldEntry));
                setKeyCount(keyCount() - 1);
                return;
            }

            Value* reinsertedEntry = reinsert(target, WTF::move(oldEntry));
            std::destroy_at(std::addressof(oldEntry));
            if (std::addressof(oldEntry) == entry) {
                ASSERT(!newEntry);
                newEntry = reinsertedEntry;
            }
        });

        if (oldTable)
            Malloc::free(mallocPointerForTable(oldTable));

        internalCheckTableConsistency();
        return newEntry;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::clear()
    {
        invalidateIterators(this);
        if (!m_table)
            return;

        deallocateTable(std::exchange(m_table, nullptr));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::HashTable(const HashTable& other)
        : m_table(nullptr)
#if CHECK_HASHTABLE_ITERATORS
        , m_iterators(nullptr)
        , m_mutex(makeUnique<Lock>())
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
        , m_stats(makeUnique<Stats>(*other.m_stats))
#endif
    {
        unsigned otherKeyCount = other.size();
        if (!otherKeyCount)
            return;

        unsigned bestTableSize = computeBestTableSize(otherKeyCount);
        ValueType* table = allocateTable(bestTableSize);
        m_table = table;
        setTableSize(bestTableSize);
        setHashShift(hashShiftForSize(bestTableSize));
        setDeletedCount(0);

        auto target = makeReinsertTable(table, bestTableSize);
        // Iterating other skips weak null entries, so other.size() can exceed what we copy.
        unsigned copiedCount = 0;
        for (const auto& otherValue : other) {
            auto location = lookupForReinsert(target, Extractor::extract(otherValue));
            std::construct_at(location.entry, otherValue);
            writeStatusByte(location);
            ++copiedCount;
        }
        setKeyCount(copiedCount);

        internalCheckTableConsistency();
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::swap(HashTable& other)
    {
        invalidateIterators(this);
        invalidateIterators(&other);

        std::swap(m_table, other.m_table);

#if DUMP_HASHTABLE_STATS_PER_TABLE
        m_stats.swap(other.m_stats);
#endif
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::operator=(const HashTable& other) -> HashTable&
    {
        if (&other == this)
            return *this;

        HashTable tmp(other);
        swap(tmp);
        return *this;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    inline HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::HashTable(HashTable&& other)
#if CHECK_HASHTABLE_ITERATORS
        : m_iterators(nullptr)
        , m_mutex(makeUnique<Lock>())
#endif
    {
        invalidateIterators(&other);

        m_table = std::exchange(other.m_table, nullptr);

#if DUMP_HASHTABLE_STATS_PER_TABLE
        m_stats = WTF::move(other.m_stats);
        other.m_stats = nullptr;
#endif
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    inline auto HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::operator=(HashTable&& other) -> HashTable&
    {
        HashTable temp = WTF::move(other);
        swap(temp);
        return *this;
    }

#if ASSERT_ENABLED

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::checkTableConsistency() const
    {
        checkTableConsistencyExceptSize();
        ASSERT(!m_table || !shouldExpand());
        ASSERT(!shouldShrink());
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::checkTableConsistencyExceptSize() const
    {
        if (!m_table)
            return;

        unsigned count = 0;
        unsigned deletedCount = 0;
        unsigned tableSize = this->tableSize();
        StatusBytes statusBytes = this->statusBytes();

        for (unsigned j = 0; j < tableSize; ++j) {
            if (statusBytes[j].isEmpty())
                continue;

            if (statusBytes[j].isDeleted()) {
                ++deletedCount;
                continue;
            }

            ++count;
            ValueType* entry = m_table + j;
            if (isWeakNullBucket(*entry))
                continue;

            auto& key = Extractor::extract(*entry);
            ASSERT(statusBytes[j] == StatusByte::forHash(IdentityTranslatorType::hash(key), hashShiftForSize(tableSize)));
            const_iterator it = find<ShouldValidateKey::No>(key);
            ASSERT(entry == it.get());

            ValueCheck<Key>::checkConsistency(key);
        }

        ASSERT(count == keyCount());
        ASSERT(deletedCount == this->deletedCount());
        ASSERT(tableSize >= KeyTraits::minimumTableSize);
        ASSERT(hashShiftForTable(m_table) == hashShiftForSize(tableSize));
    }

#endif // ASSERT_ENABLED

#if CHECK_HASHTABLE_ITERATORS

    template<typename HashTableType>
    void invalidateIterators(const HashTableType* table)
    {
        Locker locker { *table->m_mutex };
        typename HashTableType::const_iterator* next;
        for (auto* p = table->m_iterators; p; p = next) {
            next = p->m_next;
            p->m_table = nullptr;
            p->m_next = nullptr;
            p->m_previous = nullptr;
        }
        table->m_iterators = nullptr;
    }

    template<typename HashTableType, typename IteratorType>
    void addIterator(const HashTableType* table, IteratorType* it)
    {
        it->m_table = table;
        it->m_previous = nullptr;

        // Insert iterator at head of doubly-linked list of iterators.
        if (!table) {
            it->m_next = nullptr;
        } else {
            Locker locker { *table->m_mutex };
            ASSERT(table->m_iterators != it);
            it->m_next = table->m_iterators;
            table->m_iterators = it;
            if (it->m_next) {
                ASSERT(!it->m_next->m_previous);
                it->m_next->m_previous = it;
            }
        }
    }

    template<typename IteratorType>
    void removeIterator(IteratorType* it)
    {
        // Delete iterator from doubly-linked list of iterators.
        if (!it->m_table) {
            ASSERT(!it->m_next);
            ASSERT(!it->m_previous);
        } else {
            Locker locker { *it->m_table->m_mutex };
            if (it->m_next) {
                ASSERT(it->m_next->m_previous == it);
                it->m_next->m_previous = it->m_previous;
            }
            if (it->m_previous) {
                ASSERT(it->m_table->m_iterators != it);
                ASSERT(it->m_previous->m_next == it);
                it->m_previous->m_next = it->m_next;
            } else {
                ASSERT(it->m_table->m_iterators == it);
                it->m_table->m_iterators = it->m_next;
            }
        }

        it->m_table = nullptr;
        it->m_next = nullptr;
        it->m_previous = nullptr;
    }

#endif // CHECK_HASHTABLE_ITERATORS

    struct HashTableTraits {
        template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
        using TableType = HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>;
    };

    // iterator adapters

    template<typename HashTableType, typename ValueType> struct HashTableConstIteratorAdapter {
        using iterator_category = std::forward_iterator_tag;
        using value_type = ValueType;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        HashTableConstIteratorAdapter() = default;
        HashTableConstIteratorAdapter(const typename HashTableType::const_iterator& impl) : m_impl(impl) {}

        const ValueType* get() const { return (const ValueType*)m_impl.get(); }
        const ValueType& operator*() const { return *get(); }
        const ValueType* operator->() const { return get(); }

        HashTableConstIteratorAdapter& operator++() { ++m_impl; return *this; }
        HashTableConstIteratorAdapter& operator++(int) { auto result = *this; ++m_impl; return result; }

        typename HashTableType::const_iterator m_impl;
    };

    template<typename HashTableType, typename ValueType> struct HashTableIteratorAdapter {
        using iterator_category = std::forward_iterator_tag;
        using value_type = ValueType;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        HashTableIteratorAdapter() = default;
        HashTableIteratorAdapter(const typename HashTableType::iterator& impl) : m_impl(impl) {}

        ValueType* get() const { return (ValueType*)m_impl.get(); }
        ValueType& operator*() const { return *get(); }
        ValueType* operator->() const { return get(); }

        HashTableIteratorAdapter& operator++() { ++m_impl; return *this; }
        HashTableIteratorAdapter& operator++(int) { auto result = *this; ++m_impl; return result; }

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
    inline bool operator==(const HashTableIteratorAdapter<T, U>& a, const HashTableIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator==(const HashTableConstIteratorAdapter<T, U>& a, const HashTableIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator==(const HashTableIteratorAdapter<T, U>& a, const HashTableConstIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#include <wtf/HashIterators.h>
