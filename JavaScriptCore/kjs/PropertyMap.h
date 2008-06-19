// -*- mode: c++; c-basic-offset: 4 -*-
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

#include "identifier.h"
#include "protect.h"
#include <wtf/OwnArrayPtr.h>

namespace KJS {

    class JSObject;
    class JSValue;
    class PropertyNameArray;
    
    struct PropertyMapEntry;
    struct PropertyMapHashTable;

    class PropertyMap : Noncopyable {
    public:
        PropertyMap();
        ~PropertyMap();
        
        void clear();
        
        void put(const Identifier&, JSValue*, unsigned attributes, bool checkReadOnly = false);
        void remove(const Identifier&);
        JSValue* get(const Identifier&) const;
        JSValue* get(const Identifier&, unsigned& attributes) const;
        JSValue** getLocation(const Identifier& name);
        JSValue** getLocation(const Identifier& name, bool& isWriteable);

        void mark() const;
        void getEnumerablePropertyNames(PropertyNameArray&) const;

        bool hasGetterSetterProperties() const { return m_getterSetterFlag; }
        void setHasGetterSetterProperties(bool f) { m_getterSetterFlag = f; }

        bool containsGettersOrSetters() const;

    private:
        typedef PropertyMapEntry Entry;
        typedef PropertyMapHashTable Table;

        static bool keysMatch(const UString::Rep*, const UString::Rep*);
        void expand();
        void rehash();
        void rehash(unsigned newTableSize);
        void createTable();
        
        void insert(const Entry&);
        
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
} // namespace

#endif // _KJS_PROPERTY_MAP_H_
