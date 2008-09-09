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

namespace JSC {

    class JSObject;
    class JSValue;
    class PropertyNameArray;

    typedef OwnArrayPtr<JSValue*> PropertyStorage;

    struct PropertyMapEntry {
        UString::Rep* key;
        unsigned attributes;
        unsigned index;

        PropertyMapEntry(UString::Rep* k, int a)
            : key(k)
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
        friend class CTI;
    public:
        PropertyMap();
        ~PropertyMap();

        PropertyMap& operator=(const PropertyMap&);

        bool isEmpty() { return !m_table; }

        void put(const Identifier& propertyName, JSValue*, unsigned attributes, bool checkReadOnly, JSObject* slotBase, PutPropertySlot&, PropertyStorage&);
        void remove(const Identifier& propertyName, PropertyStorage&);
        JSValue* get(const Identifier& propertyName, const PropertyStorage&) const;
        JSValue* get(const Identifier& propertyName, unsigned& attributes, const PropertyStorage&) const;
        JSValue** getLocation(const Identifier& propertyName, const PropertyStorage&);
        JSValue** getLocation(const Identifier& propertyName, bool& isWriteable, const PropertyStorage&);

        size_t getOffset(const Identifier& propertyName);
        size_t getOffset(const Identifier& propertyName, bool& isWriteable);

        void getEnumerablePropertyNames(PropertyNameArray&) const;

        bool hasGetterSetterProperties() const { return m_getterSetterFlag; }
        void setHasGetterSetterProperties(bool f) { m_getterSetterFlag = f; }

        unsigned size() const { return m_table ? m_table->size : 0; }
        unsigned makingCount() const { return m_table ? m_table->keyCount + m_table->deletedSentinelCount : 0; }

        void resizePropertyStorage(PropertyStorage&, unsigned oldSize);

    private:
        typedef PropertyMapEntry Entry;
        typedef PropertyMapHashTable Table;

        static bool keysMatch(const UString::Rep*, const UString::Rep*);
        void expand(PropertyStorage&);
        void rehash(PropertyStorage&);
        void rehash(unsigned newTableSize, PropertyStorage&);
        void createTable(PropertyStorage&);

        void insert(const Entry&, JSValue*, PropertyStorage&);

        void checkConsistency(PropertyStorage&);

        Table* m_table;
        bool m_getterSetterFlag : 1;
    };

    inline PropertyMap::PropertyMap() 
        : m_table(0)
        , m_getterSetterFlag(false)
    {
    }

} // namespace JSC

#endif // PropertyMap_h
