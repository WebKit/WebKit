/*
 *  Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "JSExportMacros.h"
#include "PropertyOffset.h"
#include "Structure.h"
#include "WriteBarrier.h"
#include <wtf/HashTable.h>
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomStringImpl.h>


#define DUMP_PROPERTYMAP_STATS 0
#define DUMP_PROPERTYMAP_COLLISIONS 0

#define PROPERTY_MAP_DELETED_ENTRY_KEY ((UniquedStringImpl*)1)

namespace JSC {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PropertyTable);

#if DUMP_PROPERTYMAP_STATS

struct PropertyMapHashTableStats {
    std::atomic<unsigned> numFinds;
    std::atomic<unsigned> numCollisions;
    std::atomic<unsigned> numLookups;
    std::atomic<unsigned> numLookupProbing;
    std::atomic<unsigned> numAdds;
    std::atomic<unsigned> numRemoves;
    std::atomic<unsigned> numRehashes;
    std::atomic<unsigned> numReinserts;
};

JS_EXPORT_PRIVATE extern PropertyMapHashTableStats* propertyMapHashTableStats;

#endif

inline bool isPowerOf2(unsigned v)
{
    return hasOneBitSet(v);
}

inline unsigned nextPowerOf2(unsigned v)
{
    // Taken from http://www.cs.utk.edu/~vose/c-stuff/bithacks.html
    // Devised by Sean Anderson, Sepember 14, 2001

    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

class PropertyTable final : public JSCell {

    // This is the implementation for 'iterator' and 'const_iterator',
    // used for iterating over the table in insertion order.
    template<typename T>
    class ordered_iterator {
    public:
        ordered_iterator<T>& operator++()
        {
            m_valuePtr = skipDeletedEntries(m_valuePtr + 1, m_endValuePtr);
            return *this;
        }

        bool operator==(const ordered_iterator<T>& other)
        {
            return m_valuePtr == other.m_valuePtr;
        }

        bool operator!=(const ordered_iterator<T>& other)
        {
            return m_valuePtr != other.m_valuePtr;
        }

        T& operator*()
        {
            return *m_valuePtr;
        }

        T* operator->()
        {
            return m_valuePtr;
        }

        ordered_iterator(T* valuePtr, T* endValuePtr)
            : m_valuePtr(valuePtr)
            , m_endValuePtr(endValuePtr)
        {
        }

    private:
        T* m_valuePtr;
        T* m_endValuePtr;
    };

public:
    typedef JSCell Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.propertyTableSpace;
    }

    static constexpr bool needsDestruction = true;
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
    }

    typedef UniquedStringImpl* KeyType;
    typedef PropertyMapEntry ValueType;

    // The in order iterator provides overloaded * and -> to access the Value at the current position.
    typedef ordered_iterator<ValueType> iterator;
    typedef ordered_iterator<const ValueType> const_iterator;

    // The find_iterator is a pair of a pointer to a Value* an the entry in the index.
    // If 'find' does not find an entry then iter.first will be 0, and iter.second will
    // give the point in m_index where an entry should be inserted.
    typedef std::pair<ValueType*, unsigned> find_iterator;

    // Constructor is passed an initial capacity, a PropertyTable to copy, or both.
    static PropertyTable* create(VM&, unsigned initialCapacity);
    static PropertyTable* clone(VM&, const PropertyTable&);
    static PropertyTable* clone(VM&, unsigned initialCapacity, const PropertyTable&);
    ~PropertyTable();

    // Ordered iteration methods.
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    // Find a value in the table.
    find_iterator find(const KeyType&);
    ValueType* get(const KeyType&);
    // Add a value to the table
    std::pair<find_iterator, bool> WARN_UNUSED_RETURN add(VM&, const ValueType& entry);
    // Remove a value from the table.
    void remove(VM&, const find_iterator& iter);
    void remove(VM&, const KeyType& key);

    // Returns the number of values in the hashtable.
    unsigned size() const;

    // Checks if there are any values in the hashtable.
    bool isEmpty() const;

    // Number of slots in the property storage array in use, included deletedOffsets.
    unsigned propertyStorageSize() const;

    // Used to maintain a list of unused entries in the property storage.
    void clearDeletedOffsets();
    bool hasDeletedOffset();
    PropertyOffset getDeletedOffset();
    void addDeletedOffset(PropertyOffset);
    
    PropertyOffset nextOffset(PropertyOffset inlineCapacity);

    // Copy this PropertyTable, ensuring the copy has at least the capacity provided.
    PropertyTable* copy(VM&, unsigned newCapacity);

#ifndef NDEBUG
    size_t sizeInMemory();
    void checkConsistency();
#endif
    
    static ptrdiff_t offsetOfIndexSize() { return OBJECT_OFFSETOF(PropertyTable, m_indexSize); }
    static ptrdiff_t offsetOfIndexMask() { return OBJECT_OFFSETOF(PropertyTable, m_indexMask); }
    static ptrdiff_t offsetOfIndex() { return OBJECT_OFFSETOF(PropertyTable, m_index); }

    static constexpr unsigned EmptyEntryIndex = 0;

private:
    PropertyTable(VM&, unsigned initialCapacity);
    PropertyTable(VM&, const PropertyTable&);
    PropertyTable(VM&, unsigned initialCapacity, const PropertyTable&);

    PropertyTable(const PropertyTable&);

    void finishCreation(VM&);

    // Used to insert a value known not to be in the table, and where we know capacity to be available.
    void reinsert(const ValueType& entry);

    // Rehash the table.  Used to grow, or to recover deleted slots.
    void rehash(VM&, unsigned newCapacity);

    // The capacity of the table of values is half of the size of the index.
    unsigned tableCapacity() const;

    // We keep an extra deleted slot after the array to make iteration work,
    // and to use for deleted values. Index values into the array are 1-based,
    // so this is tableCapacity() + 1.
    // For example, if m_tableSize is 16, then tableCapacity() is 8 - but the
    // values array is actually 9 long (the 9th used for the deleted value/
    // iteration guard).  The 8 valid entries are numbered 1..8, so the
    // deleted index is 9 (0 being reserved for empty).
    unsigned deletedEntryIndex() const;

    // Used in iterator creation/progression.
    template<typename T>
    static T* skipDeletedEntries(T* valuePtr, T* endValuePtr);

    // The table of values lies after the hash index.
    ValueType* table();
    const ValueType* table() const;

    ValueType* tableEnd() { return table() + usedCount(); }
    const ValueType* tableEnd() const { return table() + usedCount(); }

    // total number of  used entries in the values array - by either valid entries, or deleted ones.
    unsigned usedCount() const;

    // The size in bytes of data needed for by the table.
    size_t dataSize();

    // Calculates the appropriate table size (rounds up to a power of two).
    static unsigned sizeForCapacity(unsigned capacity);

    // Check if capacity is available.
    bool canInsert();

    unsigned m_indexSize;
    unsigned m_indexMask;
    unsigned* m_index;
    unsigned m_keyCount;
    unsigned m_deletedCount;
    std::unique_ptr<Vector<PropertyOffset>> m_deletedOffsets;

    static constexpr unsigned MinimumTableSize = 16;
};

inline PropertyTable::iterator PropertyTable::begin()
{
    auto* tableEnd = this->tableEnd();
    return iterator(skipDeletedEntries(table(), tableEnd), tableEnd);
}

inline PropertyTable::iterator PropertyTable::end()
{
    auto* tableEnd = this->tableEnd();
    return iterator(tableEnd, tableEnd);
}

inline PropertyTable::const_iterator PropertyTable::begin() const
{
    auto* tableEnd = this->tableEnd();
    return const_iterator(skipDeletedEntries(table(), tableEnd), tableEnd);
}

inline PropertyTable::const_iterator PropertyTable::end() const
{
    auto* tableEnd = this->tableEnd();
    return const_iterator(tableEnd, tableEnd);
}

inline PropertyTable::find_iterator PropertyTable::find(const KeyType& key)
{
    ASSERT(key);
    ASSERT(key->isAtom() || key->isSymbol());
    unsigned hash = IdentifierRepHash::hash(key);

#if DUMP_PROPERTYMAP_STATS
    ++propertyMapHashTableStats->numFinds;
#endif

    while (true) {
        unsigned entryIndex = m_index[hash & m_indexMask];
        if (entryIndex == EmptyEntryIndex)
            return std::make_pair((ValueType*)nullptr, hash & m_indexMask);
        if (key == table()[entryIndex - 1].key)
            return std::make_pair(&table()[entryIndex - 1], hash & m_indexMask);

#if DUMP_PROPERTYMAP_STATS
        ++propertyMapHashTableStats->numCollisions;
#endif

#if DUMP_PROPERTYMAP_COLLISIONS
        dataLog("PropertyTable collision for ", key, " (", hash, ")\n");
        dataLog("Collided with ", table()[entryIndex - 1].key, "(", IdentifierRepHash::hash(table()[entryIndex - 1].key), ")\n");
#endif

        hash++;
    }
}

inline PropertyTable::ValueType* PropertyTable::get(const KeyType& key)
{
    ASSERT(key);
    ASSERT(key->isAtom() || key->isSymbol());
    ASSERT(key != PROPERTY_MAP_DELETED_ENTRY_KEY);

    if (!m_keyCount)
        return nullptr;

    unsigned hash = IdentifierRepHash::hash(key);

#if DUMP_PROPERTYMAP_STATS
    ++propertyMapHashTableStats->numLookups;
#endif

    while (true) {
        unsigned entryIndex = m_index[hash & m_indexMask];
        if (entryIndex == EmptyEntryIndex)
            return nullptr;
        if (key == table()[entryIndex - 1].key) {
            ASSERT(!m_deletedOffsets || !m_deletedOffsets->contains(table()[entryIndex - 1].offset));
            return &table()[entryIndex - 1];
        }

#if DUMP_PROPERTYMAP_STATS
        ++propertyMapHashTableStats->numLookupProbing;
#endif

        hash++;
    }
}

inline std::pair<PropertyTable::find_iterator, bool> WARN_UNUSED_RETURN PropertyTable::add(VM& vm, const ValueType& entry)
{
    ASSERT(!m_deletedOffsets || !m_deletedOffsets->contains(entry.offset));

    // Look for a value with a matching key already in the array.
    find_iterator iter = find(entry.key);
    if (iter.first)
        return std::make_pair(iter, false);

#if DUMP_PROPERTYMAP_STATS
    ++propertyMapHashTableStats->numAdds;
#endif

    // Ref the key
    entry.key->ref();

    // ensure capacity is available.
    if (!canInsert()) {
        rehash(vm, m_keyCount + 1);
        iter = find(entry.key);
        ASSERT(!iter.first);
    }

    // Allocate a slot in the hashtable, and set the index to reference this.
    unsigned entryIndex = usedCount() + 1;
    m_index[iter.second] = entryIndex;
    iter.first = &table()[entryIndex - 1];
    *iter.first = entry;

    ++m_keyCount;
    
    return std::make_pair(iter, true);
}

inline void PropertyTable::remove(VM& vm, const find_iterator& iter)
{
    // Removing a key that doesn't exist does nothing!
    if (!iter.first)
        return;

#if DUMP_PROPERTYMAP_STATS
    ++propertyMapHashTableStats->numRemoves;
#endif

    // Replace this one element with the deleted sentinel. Also clear out
    // the entry so we can iterate all the entries as needed.
    m_index[iter.second] = deletedEntryIndex();
    iter.first->key->deref();
    iter.first->key = PROPERTY_MAP_DELETED_ENTRY_KEY;

    ASSERT(m_keyCount >= 1);
    --m_keyCount;
    ++m_deletedCount;

    if (m_deletedCount * 4 >= m_indexSize)
        rehash(vm, m_keyCount);
}

inline void PropertyTable::remove(VM& vm, const KeyType& key)
{
    remove(vm, find(key));
}

// returns the number of values in the hashtable.
inline unsigned PropertyTable::size() const
{
    return m_keyCount;
}

inline bool PropertyTable::isEmpty() const
{
    return !m_keyCount;
}

inline unsigned PropertyTable::propertyStorageSize() const
{
    return size() + (m_deletedOffsets ? m_deletedOffsets->size() : 0);
}

inline void PropertyTable::clearDeletedOffsets()
{
    m_deletedOffsets = nullptr;
}

inline bool PropertyTable::hasDeletedOffset()
{
    return m_deletedOffsets && !m_deletedOffsets->isEmpty();
}

inline PropertyOffset PropertyTable::getDeletedOffset()
{
    PropertyOffset offset = m_deletedOffsets->last();
    m_deletedOffsets->removeLast();
    return offset;
}

inline void PropertyTable::addDeletedOffset(PropertyOffset offset)
{
    if (!m_deletedOffsets)
        m_deletedOffsets = makeUnique<Vector<PropertyOffset>>();
    ASSERT(!m_deletedOffsets->contains(offset));
    m_deletedOffsets->append(offset);
}

inline PropertyOffset PropertyTable::nextOffset(PropertyOffset inlineCapacity)
{
    if (hasDeletedOffset())
        return getDeletedOffset();

    return offsetForPropertyNumber(size(), inlineCapacity);
}

inline PropertyTable* PropertyTable::copy(VM& vm, unsigned newCapacity)
{
    ASSERT(newCapacity >= m_keyCount);

    // Fast case; if the new table will be the same m_indexSize as this one, we can memcpy it,
    // save rehashing all keys.
    if (sizeForCapacity(newCapacity) == m_indexSize)
        return PropertyTable::clone(vm, *this);
    return PropertyTable::clone(vm, newCapacity, *this);
}

#ifndef NDEBUG
inline size_t PropertyTable::sizeInMemory()
{
    size_t result = sizeof(PropertyTable) + dataSize();
    if (m_deletedOffsets)
        result += (m_deletedOffsets->capacity() * sizeof(PropertyOffset));
    return result;
}
#endif

inline void PropertyTable::reinsert(const ValueType& entry)
{
#if DUMP_PROPERTYMAP_STATS
    ++propertyMapHashTableStats->numReinserts;
#endif

    // Used to insert a value known not to be in the table, and where
    // we know capacity to be available.
    ASSERT(canInsert());
    find_iterator iter = find(entry.key);
    ASSERT(!iter.first);

    unsigned entryIndex = usedCount() + 1;
    m_index[iter.second] = entryIndex;
    table()[entryIndex - 1] = entry;

    ++m_keyCount;
}

inline void PropertyTable::rehash(VM& vm, unsigned newCapacity)
{
#if DUMP_PROPERTYMAP_STATS
    ++propertyMapHashTableStats->numRehashes;
#endif

    size_t oldDataSize = dataSize();
    unsigned* oldEntryIndices = m_index;
    iterator iter = this->begin();
    iterator end = this->end();

    m_indexSize = sizeForCapacity(newCapacity);
    m_indexMask = m_indexSize - 1;
    m_keyCount = 0;
    m_deletedCount = 0;

    m_index = static_cast<unsigned*>(PropertyTableMalloc::zeroedMalloc(dataSize()));

    for (; iter != end; ++iter) {
        ASSERT(canInsert());
        reinsert(*iter);
    }

    PropertyTableMalloc::free(oldEntryIndices);

    if (oldDataSize < dataSize())
        vm.heap.reportExtraMemoryAllocated(dataSize() - oldDataSize);
}

inline unsigned PropertyTable::tableCapacity() const { return m_indexSize >> 1; }

inline unsigned PropertyTable::deletedEntryIndex() const { return tableCapacity() + 1; }

template<typename T>
inline T* PropertyTable::skipDeletedEntries(T* valuePtr, T* endValuePtr)
{
    while (valuePtr < endValuePtr && valuePtr->key == PROPERTY_MAP_DELETED_ENTRY_KEY)
        ++valuePtr;
    return valuePtr;
}

inline PropertyTable::ValueType* PropertyTable::table()
{
    // The table of values lies after the hash index.
    return reinterpret_cast<ValueType*>(m_index + m_indexSize);
}

inline const PropertyTable::ValueType* PropertyTable::table() const
{
    // The table of values lies after the hash index.
    return reinterpret_cast<const ValueType*>(m_index + m_indexSize);
}

inline unsigned PropertyTable::usedCount() const
{
    // Total number of  used entries in the values array - by either valid entries, or deleted ones.
    return m_keyCount + m_deletedCount;
}

inline size_t PropertyTable::dataSize()
{
    // The size in bytes of data needed for by the table.
    // Ensure that this function can be called concurrently.
    unsigned indexSize = m_indexSize;
    return indexSize * sizeof(unsigned) + ((indexSize >> 1) + 1) * sizeof(ValueType);
}

inline unsigned PropertyTable::sizeForCapacity(unsigned capacity)
{
    if (capacity < MinimumTableSize / 2)
        return MinimumTableSize;
    return nextPowerOf2(capacity + 1) * 2;
}

inline bool PropertyTable::canInsert()
{
    return usedCount() < tableCapacity();
}

} // namespace JSC
