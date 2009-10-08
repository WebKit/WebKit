/*
 *  Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef PropertyNameArray_h
#define PropertyNameArray_h

#include "CallFrame.h"
#include "Identifier.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace JSC {
    
    class Structure;
    class StructureChain;

    class PropertyNameArrayData : public RefCounted<PropertyNameArrayData> {
    public:
        typedef Vector<Identifier, 20> PropertyNameVector;
        typedef PropertyNameVector::const_iterator const_iterator;

        static PassRefPtr<PropertyNameArrayData> create() { return adoptRef(new PropertyNameArrayData); }

        const_iterator begin() const { return m_propertyNameVector.begin(); }
        const_iterator end() const { return m_propertyNameVector.end(); }

        PropertyNameVector& propertyNameVector() { return m_propertyNameVector; }

        void setCachedStructure(Structure* structure) { m_cachedStructure = structure; }
        Structure* cachedStructure() const { return m_cachedStructure; }

        void setCachedPrototypeChain(NonNullPassRefPtr<StructureChain> cachedPrototypeChain) { m_cachedPrototypeChain = cachedPrototypeChain; }
        StructureChain* cachedPrototypeChain() { return m_cachedPrototypeChain.get(); }

    private:
        PropertyNameArrayData()
            : m_cachedStructure(0)
        {
        }

        PropertyNameVector m_propertyNameVector;
        Structure* m_cachedStructure;
        RefPtr<StructureChain> m_cachedPrototypeChain;
    };

    class PropertyNameArray {
    public:
        typedef PropertyNameArrayData::const_iterator const_iterator;

        PropertyNameArray(JSGlobalData* globalData)
            : m_data(PropertyNameArrayData::create())
            , m_globalData(globalData)
            , m_shouldCache(true)
        {
        }

        PropertyNameArray(ExecState* exec)
            : m_data(PropertyNameArrayData::create())
            , m_globalData(&exec->globalData())
            , m_shouldCache(true)
        {
        }

        JSGlobalData* globalData() { return m_globalData; }

        void add(const Identifier& identifier) { add(identifier.ustring().rep()); }
        void add(UString::Rep*);
        void addKnownUnique(UString::Rep* identifier) { m_data->propertyNameVector().append(Identifier(m_globalData, identifier)); }

        size_t size() const { return m_data->propertyNameVector().size(); }

        Identifier& operator[](unsigned i) { return m_data->propertyNameVector()[i]; }
        const Identifier& operator[](unsigned i) const { return m_data->propertyNameVector()[i]; }

        const_iterator begin() const { return m_data->begin(); }
        const_iterator end() const { return m_data->end(); }

        void setData(PassRefPtr<PropertyNameArrayData> data) { m_data = data; }
        PropertyNameArrayData* data() { return m_data.get(); }

        PassRefPtr<PropertyNameArrayData> releaseData() { return m_data.release(); }

        void setShouldCache(bool shouldCache) { m_shouldCache = shouldCache; }
        bool shouldCache() const { return m_shouldCache; }

    private:
        typedef HashSet<UString::Rep*, PtrHash<UString::Rep*> > IdentifierSet;

        RefPtr<PropertyNameArrayData> m_data;
        IdentifierSet m_set;
        JSGlobalData* m_globalData;
        bool m_shouldCache;
    };

} // namespace JSC

#endif // PropertyNameArray_h
