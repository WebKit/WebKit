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
#include <wtf/NotFound.h>

namespace JSC {

    class JSObject;
    class JSValue;
    class PropertyNameArray;
    struct PropertyMapEntry;
    struct PropertyMapHashTable;

    struct PropertyMapEntry {
        UString::Rep* key;
        JSValue* value;
        unsigned attributes;
        unsigned index;

        PropertyMapEntry(UString::Rep* k, JSValue* v, int a)
            : key(k)
            , value(v)
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

    class PropertyMap : Noncopyable {
        friend class CTI;

    public:
        PropertyMap();
        ~PropertyMap();

        bool isEmpty() { return !m_usingTable & !m_singleEntryKey; }

        void put(const Identifier& propertyName, JSValue*, unsigned attributes, bool checkReadOnly, JSObject* slotBase, PutPropertySlot&);
        void remove(const Identifier& propertyName);
        JSValue* get(const Identifier& propertyName) const;
        JSValue* get(const Identifier& propertyName, unsigned& attributes) const;
        JSValue** getLocation(const Identifier& propertyName);
        JSValue** getLocation(const Identifier& propertyName, bool& isWriteable);
        
        JSValue* getOffset(size_t offset)
        {
            ASSERT(m_usingTable);
            return reinterpret_cast<JSValue**>(m_u.table->entryIndices)[offset];
        }
        void putOffset(size_t offset, JSValue* v)
        {
            ASSERT(m_usingTable);
            reinterpret_cast<JSValue**>(m_u.table->entryIndices)[offset] = v;
        }

        size_t offsetForLocation(JSValue** location) { return m_usingTable ? offsetForTableLocation(location) : WTF::notFound; }

        void mark() const;
        void getEnumerablePropertyNames(PropertyNameArray&) const;

        bool hasGetterSetterProperties() const { return m_getterSetterFlag; }
        void setHasGetterSetterProperties(bool f) { m_getterSetterFlag = f; }

    private:
        typedef PropertyMapEntry Entry;
        typedef PropertyMapHashTable Table;

        static bool keysMatch(const UString::Rep*, const UString::Rep*);
        void expand();
        void rehash();
        void rehash(unsigned newTableSize);
        void createTable();
        
        void insert(const Entry&);
        
        size_t offsetForTableLocation(JSValue** location)
        {
            ASSERT(m_usingTable);
            return location - reinterpret_cast<JSValue**>(m_u.table->entryIndices);
        }

        void checkConsistency();
        
        UString::Rep* m_singleEntryKey;
        union {
            JSValue* singleEntryValue;
            Table* table;
        } m_u;

        short m_singleEntryAttributes;
        bool m_getterSetterFlag : 1;
        bool m_usingTable : 1;
    };

    inline PropertyMap::PropertyMap() 
        : m_singleEntryKey(0)
        , m_getterSetterFlag(false)
        , m_usingTable(false)

    {
    }

} // namespace JSC

#endif // PropertyMap_h
