/*
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef PropertyMap_h
#define PropertyMap_h

#include "PropertySlot.h"
#include "identifier.h"
#include <wtf/OwnArrayPtr.h>
#include <wtf/NotFound.h>

#ifndef NDEBUG
#define DUMP_PROPERTYMAP_STATS 0
#else
#define DUMP_PROPERTYMAP_STATS 0
#endif

namespace JSC {

    class JSObject;
    class PropertyNameArray;

    typedef JSValue** PropertyStorage;

    struct PropertyMapEntry {
        UString::Rep* key;
        unsigned offset;
        unsigned attributes;
        unsigned index;

        PropertyMapEntry(UString::Rep* k, int a)
            : key(k)
            , offset(0)
            , attributes(a)
            , index(0)
        {
        }
    };

    // lastIndexUsed is an ever-increasing index used to identify the order items
    // were inserted into the property map. It's required that getEnumerablePropertyNames
    // return the properties in the order they were added for compatibility with other
    // browsers' JavaScript implementations.
    struct PropertyMapHashTable {
        unsigned sizeMask;
        unsigned size;
        unsigned keyCount;
        unsigned deletedSentinelCount;
        unsigned lastIndexUsed;
        unsigned entryIndices[1];

        PropertyMapEntry* entries()
        {
            // The entries vector comes after the indices vector.
            // The 0th item in the entries vector is not really used; it has to
            // have a 0 in its key to allow the hash table lookup to handle deleted
            // sentinels without any special-case code, but the other fields are unused.
            return reinterpret_cast<PropertyMapEntry*>(&entryIndices[size]);
        }

        static size_t allocationSize(unsigned size)
        {
            // We never let a hash table get more than half full,
            // So the number of indices we need is the size of the hash table.
            // But the number of entries is half that (plus one for the deleted sentinel).
            return sizeof(PropertyMapHashTable)
                + (size - 1) * sizeof(unsigned)
                + (1 + size / 2) * sizeof(PropertyMapEntry);
        }
    };

    class PropertyMap {
    public:
        PropertyMap();
        ~PropertyMap();

        PropertyMap& operator=(const PropertyMap&);

        size_t get(const Identifier& propertyName) const;
        size_t get(const Identifier& propertyName, unsigned& attributes) const;
        size_t put(const Identifier& propertyName, unsigned attributes);
        size_t remove(const Identifier& propertyName);

        void getEnumerablePropertyNames(PropertyNameArray&) const;

        bool isEmpty() const { return !m_table; }
        unsigned storageSize() const { return m_table ? m_table->keyCount + m_deletedOffsets.size() : 0; }

        size_t propertyMapSize() const
        {
            return sizeof(PropertyMap) + (m_table ? PropertyMapHashTable::allocationSize(m_table->size) : 0);
        }

        static const unsigned emptyEntryIndex = 0;

    private:
        typedef PropertyMapEntry Entry;
        typedef PropertyMapHashTable Table;

        void expand();
        void rehash();
        void rehash(unsigned newTableSize);
        void createTable();

        void insert(const Entry&);

        void checkConsistency();

        Table* m_table;
        Vector<unsigned> m_deletedOffsets;
    };

    inline PropertyMap::PropertyMap() 
        : m_table(0)
    {
    }

    inline size_t PropertyMap::get(const Identifier& propertyName) const
    {
        ASSERT(!propertyName.isNull());

        if (!m_table)
            return WTF::notFound;

        UString::Rep* rep = propertyName._ustring.rep();

        unsigned i = rep->computedHash();

#if DUMP_PROPERTYMAP_STATS
        ++numProbes;
#endif

        unsigned entryIndex = m_table->entryIndices[i & m_table->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return WTF::notFound;

        if (rep == m_table->entries()[entryIndex - 1].key)
            return m_table->entries()[entryIndex - 1].offset;

#if DUMP_PROPERTYMAP_STATS
        ++numCollisions;
#endif

        unsigned k = 1 | WTF::doubleHash(rep->computedHash());

        while (1) {
            i += k;

#if DUMP_PROPERTYMAP_STATS
            ++numRehashes;
#endif

            entryIndex = m_table->entryIndices[i & m_table->sizeMask];
            if (entryIndex == emptyEntryIndex)
                return WTF::notFound;

            if (rep == m_table->entries()[entryIndex - 1].key)
                return m_table->entries()[entryIndex - 1].offset;
        }
    }

} // namespace JSC

#endif // PropertyMap_h
