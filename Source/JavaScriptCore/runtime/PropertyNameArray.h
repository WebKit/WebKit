/*
 *  Copyright (C) 2006, 2008, 2012 Apple Inc. All rights reserved.
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

class JSPropertyNameEnumerator;
class Structure;
class StructureChain;

class RefCountedIdentifierSet : public RefCounted<RefCountedIdentifierSet> {
public:
    typedef HashSet<StringImpl*, PtrHash<StringImpl*>> Set;

    bool contains(StringImpl* impl) const { return m_set.contains(impl); }
    size_t size() const  { return m_set.size(); }
    Set::AddResult add(StringImpl* impl) { return m_set.add(impl); }

private:
    Set m_set;
};

// FIXME: Rename to PropertyNameArray.
class PropertyNameArrayData : public RefCounted<PropertyNameArrayData> {
public:
    typedef Vector<Identifier, 20> PropertyNameVector;

    static PassRefPtr<PropertyNameArrayData> create() { return adoptRef(new PropertyNameArrayData); }

    PropertyNameVector& propertyNameVector() { return m_propertyNameVector; }

private:
    PropertyNameArrayData()
    {
    }

    PropertyNameVector m_propertyNameVector;
};

// FIXME: Rename to PropertyNameArrayBuilder.
class PropertyNameArray {
public:
    PropertyNameArray(VM* vm)
        : m_data(PropertyNameArrayData::create())
        , m_set(adoptRef(new RefCountedIdentifierSet))
        , m_vm(vm)
        , m_numCacheableSlots(0)
        , m_baseObject(0)
        , m_previouslyEnumeratedLength(0)
    {
    }

    PropertyNameArray(ExecState* exec)
        : m_data(PropertyNameArrayData::create())
        , m_set(adoptRef(new RefCountedIdentifierSet))
        , m_vm(&exec->vm())
        , m_numCacheableSlots(0)
        , m_baseObject(0)
        , m_previouslyEnumeratedLength(0)
    {
    }

    VM* vm() { return m_vm; }

    void add(uint32_t index)
    {
        if (index < m_previouslyEnumeratedLength)
            return;
        add(Identifier::from(m_vm, index));
    }

    void add(const Identifier& identifier) { add(identifier.impl()); }
    JS_EXPORT_PRIVATE void add(StringImpl*);
    void addKnownUnique(StringImpl* identifier)
    {
        m_set->add(identifier);
        m_data->propertyNameVector().append(Identifier(m_vm, identifier));
    }

    Identifier& operator[](unsigned i) { return m_data->propertyNameVector()[i]; }
    const Identifier& operator[](unsigned i) const { return m_data->propertyNameVector()[i]; }

    void setData(PassRefPtr<PropertyNameArrayData> data) { m_data = data; }
    PropertyNameArrayData* data() { return m_data.get(); }
    PassRefPtr<PropertyNameArrayData> releaseData() { return m_data.release(); }

    RefCountedIdentifierSet* identifierSet() const { return m_set.get(); }

    // FIXME: Remove these functions.
    bool canAddKnownUniqueForStructure() const { return !m_set->size() && (!m_alternateSet || !m_alternateSet->size()); }
    typedef PropertyNameArrayData::PropertyNameVector::const_iterator const_iterator;
    size_t size() const { return m_data->propertyNameVector().size(); }
    const_iterator begin() const { return m_data->propertyNameVector().begin(); }
    const_iterator end() const { return m_data->propertyNameVector().end(); }

    size_t numCacheableSlots() const { return m_numCacheableSlots; }
    void setNumCacheableSlotsForObject(JSObject* object, size_t numCacheableSlots)
    {
        if (object != m_baseObject)
            return;
        m_numCacheableSlots = numCacheableSlots;
    }
    void setBaseObject(JSObject* object)
    {
        if (m_baseObject)
            return;
        m_baseObject = object;
    }

    void setPreviouslyEnumeratedLength(uint32_t length) { m_previouslyEnumeratedLength = length; }
    void setPreviouslyEnumeratedProperties(const JSPropertyNameEnumerator*);

private:
    RefPtr<PropertyNameArrayData> m_data;
    RefPtr<RefCountedIdentifierSet> m_set;
    RefPtr<RefCountedIdentifierSet> m_alternateSet;
    VM* m_vm;
    size_t m_numCacheableSlots;
    JSObject* m_baseObject;
    uint32_t m_previouslyEnumeratedLength;
};

} // namespace JSC

#endif // PropertyNameArray_h
