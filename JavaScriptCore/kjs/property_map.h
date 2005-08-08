// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef _KJS_PROPERTY_MAP_H_
#define _KJS_PROPERTY_MAP_H_

#include "identifier.h"

namespace KJS {

    class ObjectImp;
    class ReferenceList;
    class ValueImp;
    
    class SavedProperty;
    
    struct PropertyMapHashTable;
    
/**
* Saved Properties
*/
    class SavedProperties {
    friend class PropertyMap;
    public:
        SavedProperties();
        ~SavedProperties();
        
    private:
        int _count;
        SavedProperty *_properties;
        
        SavedProperties(const SavedProperties&);
        SavedProperties& operator=(const SavedProperties&);
    };
    
/**
* A hashtable entry for the @ref PropertyMap.
*/
    struct PropertyMapHashTableEntry
    {
        PropertyMapHashTableEntry() : key(0) { }
        UString::Rep *key;
        ValueImp *value;
        int attributes;
        int index;
    };
/**
* Javascript Property Map.
*/

    class PropertyMap {
    public:
        PropertyMap();
        ~PropertyMap();

        void clear();
        
        void put(const Identifier &name, ValueImp *value, int attributes);
        void remove(const Identifier &name);
        ValueImp *get(const Identifier &name) const;
        ValueImp *get(const Identifier &name, int &attributes) const;
        ValueImp **getLocation(const Identifier &name);

        void mark() const;
        void addEnumerablesToReferenceList(ReferenceList &, ObjectImp *) const;
	void addSparseArrayPropertiesToReferenceList(ReferenceList &, ObjectImp *) const;

        void save(SavedProperties &) const;
        void restore(const SavedProperties &p);

    private:
        static bool keysMatch(const UString::Rep *, const UString::Rep *);
        void expand();
        void rehash();
        void rehash(int newTableSize);
        
        void insert(UString::Rep *, ValueImp *value, int attributes, int index);
        
        void checkConsistency();
        
        typedef PropertyMapHashTableEntry Entry;
        typedef PropertyMapHashTable Table;

        Table *_table;
        
        Entry _singleEntry;
    };

} // namespace

#endif // _KJS_PROPERTY_MAP_H_
