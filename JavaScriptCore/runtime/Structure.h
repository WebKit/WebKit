/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef Structure_h
#define Structure_h

#include "Identifier.h"
#include "JSType.h"
#include "JSValue.h"
#include "PropertyMapHashTable.h"
#include "StructureChain.h"
#include "StructureTransitionTable.h"
#include "JSTypeInfo.h"
#include "UString.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#ifndef NDEBUG
#define DUMP_PROPERTYMAP_STATS 0
#else
#define DUMP_PROPERTYMAP_STATS 0
#endif

namespace JSC {

    class MarkStack;
    class PropertyNameArray;
    class PropertyNameArrayData;

    class Structure : public RefCounted<Structure> {
    public:
        friend class JIT;
        friend class StructureTransitionTable;
        static PassRefPtr<Structure> create(JSValue prototype, const TypeInfo& typeInfo)
        {
            return adoptRef(new Structure(prototype, typeInfo));
        }

        static void startIgnoringLeaks();
        static void stopIgnoringLeaks();

        static void dumpStatistics();

        static PassRefPtr<Structure> addPropertyTransition(Structure*, const Identifier& propertyName, unsigned attributes, JSCell* specificValue, size_t& offset);
        static PassRefPtr<Structure> addPropertyTransitionToExistingStructure(Structure*, const Identifier& propertyName, unsigned attributes, JSCell* specificValue, size_t& offset);
        static PassRefPtr<Structure> removePropertyTransition(Structure*, const Identifier& propertyName, size_t& offset);
        static PassRefPtr<Structure> changePrototypeTransition(Structure*, JSValue prototype);
        static PassRefPtr<Structure> despecifyFunctionTransition(Structure*, const Identifier&);
        static PassRefPtr<Structure> addAnonymousSlotsTransition(Structure*, unsigned count);
        static PassRefPtr<Structure> getterSetterTransition(Structure*);
        static PassRefPtr<Structure> toDictionaryTransition(Structure*);
        static PassRefPtr<Structure> fromDictionaryTransition(Structure*);

        ~Structure();

        void markAggregate(MarkStack&);

        // These should be used with caution.  
        size_t addPropertyWithoutTransition(const Identifier& propertyName, unsigned attributes, JSCell* specificValue);
        size_t removePropertyWithoutTransition(const Identifier& propertyName);
        void setPrototypeWithoutTransition(JSValue prototype) { m_prototype = prototype; }

        bool isDictionary() const { return m_isDictionary; }

        const TypeInfo& typeInfo() const { return m_typeInfo; }

        JSValue storedPrototype() const { return m_prototype; }
        JSValue prototypeForLookup(ExecState*) const;
        StructureChain* prototypeChain(ExecState*) const;

        Structure* previousID() const { return m_previous.get(); }

        void growPropertyStorageCapacity();
        size_t propertyStorageCapacity() const { return m_propertyStorageCapacity; }
        size_t propertyStorageSize() const { return m_propertyTable ? m_propertyTable->keyCount + m_propertyTable->anonymousSlotCount + (m_propertyTable->deletedOffsets ? m_propertyTable->deletedOffsets->size() : 0) : m_offset + 1; }
        bool isUsingInlineStorage() const;

        size_t get(const Identifier& propertyName);
        size_t get(const UString::Rep* rep, unsigned& attributes, JSCell*& specificValue);
        size_t get(const Identifier& propertyName, unsigned& attributes, JSCell*& specificValue)
        {
            ASSERT(!propertyName.isNull());
            return get(propertyName.ustring().rep(), attributes, specificValue);
        }
        bool transitionedFor(const JSCell* specificValue)
        {
            return m_specificValueInPrevious == specificValue;
        }
        bool hasTransition(UString::Rep*, unsigned attributes);
        bool hasTransition(const Identifier& propertyName, unsigned attributes)
        {
            return hasTransition(propertyName._ustring.rep(), attributes);
        }

        void getEnumerablePropertyNames(ExecState*, PropertyNameArray&, JSObject*);
        void getOwnEnumerablePropertyNames(ExecState*, PropertyNameArray&, JSObject*);

        bool hasGetterSetterProperties() const { return m_hasGetterSetterProperties; }
        void setHasGetterSetterProperties(bool hasGetterSetterProperties) { m_hasGetterSetterProperties = hasGetterSetterProperties; }

        bool isEmpty() const { return m_propertyTable ? !m_propertyTable->keyCount : m_offset == noOffset; }

        JSCell* specificValue() { return m_specificValueInPrevious; }
        void despecifyDictionaryFunction(const Identifier& propertyName);

    private:
        Structure(JSValue prototype, const TypeInfo&);

        size_t put(const Identifier& propertyName, unsigned attributes, JSCell* specificValue);
        size_t remove(const Identifier& propertyName);
        void addAnonymousSlots(unsigned slotCount);
        void getEnumerableNamesFromPropertyTable(PropertyNameArray&);
        void getEnumerableNamesFromClassInfoTable(ExecState*, const ClassInfo*, PropertyNameArray&);

        void expandPropertyMapHashTable();
        void rehashPropertyMapHashTable();
        void rehashPropertyMapHashTable(unsigned newTableSize);
        void createPropertyMapHashTable();
        void createPropertyMapHashTable(unsigned newTableSize);
        void insertIntoPropertyMapHashTable(const PropertyMapEntry&);
        void checkConsistency();

        bool despecifyFunction(const Identifier&);

        PropertyMapHashTable* copyPropertyTable();
        void materializePropertyMap();
        void materializePropertyMapIfNecessary()
        {
            if (m_propertyTable || !m_previous)             
                return;
            materializePropertyMap();
        }

        void clearEnumerationCache();

        signed char transitionCount() const
        {
            // Since the number of transitions is always the same as m_offset, we keep the size of Structure down by not storing both.
            return m_offset == noOffset ? 0 : m_offset + 1;
        }
        
        bool isValid(ExecState*, StructureChain* cachedPrototypeChain) const;

        static const unsigned emptyEntryIndex = 0;
    
        static const signed char s_maxTransitionLength = 64;

        static const signed char noOffset = -1;

        TypeInfo m_typeInfo;

        JSValue m_prototype;
        mutable RefPtr<StructureChain> m_cachedPrototypeChain;

        RefPtr<Structure> m_previous;
        RefPtr<UString::Rep> m_nameInPrevious;
        JSCell* m_specificValueInPrevious;

        StructureTransitionTable table;

        RefPtr<PropertyNameArrayData> m_cachedPropertyNameArrayData;

        PropertyMapHashTable* m_propertyTable;

        size_t m_propertyStorageCapacity;
        signed char m_offset;

        bool m_isDictionary : 1;
        bool m_isPinnedPropertyTable : 1;
        bool m_hasGetterSetterProperties : 1;
        unsigned m_attributesInPrevious : 7;
        unsigned m_anonymousSlotsInPrevious : 6;
    };

    inline size_t Structure::get(const Identifier& propertyName)
    {
        ASSERT(!propertyName.isNull());

        materializePropertyMapIfNecessary();
        if (!m_propertyTable)
            return WTF::notFound;

        UString::Rep* rep = propertyName._ustring.rep();

        unsigned i = rep->computedHash();

#if DUMP_PROPERTYMAP_STATS
        ++numProbes;
#endif

        unsigned entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return WTF::notFound;

        if (rep == m_propertyTable->entries()[entryIndex - 1].key)
            return m_propertyTable->entries()[entryIndex - 1].offset;

#if DUMP_PROPERTYMAP_STATS
        ++numCollisions;
#endif

        unsigned k = 1 | WTF::doubleHash(rep->computedHash());

        while (1) {
            i += k;

#if DUMP_PROPERTYMAP_STATS
            ++numRehashes;
#endif

            entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
            if (entryIndex == emptyEntryIndex)
                return WTF::notFound;

            if (rep == m_propertyTable->entries()[entryIndex - 1].key)
                return m_propertyTable->entries()[entryIndex - 1].offset;
        }
    }
    
    bool StructureTransitionTable::contains(const StructureTransitionTableHash::Key& key, JSCell* specificValue)
    {
        if (usingSingleTransitionSlot()) {
            Structure* existingTransition = singleTransition();
            return existingTransition && existingTransition->m_nameInPrevious.get() == key.first
                   && existingTransition->m_attributesInPrevious == key.second
                   && (existingTransition->m_specificValueInPrevious == specificValue || existingTransition->m_specificValueInPrevious == 0);
        }
        TransitionTable::iterator find = table()->find(key);
        if (find == table()->end())
            return false;

        return find->second.first || find->second.second->transitionedFor(specificValue);
    }

    Structure* StructureTransitionTable::get(const StructureTransitionTableHash::Key& key, JSCell* specificValue) const
    {
        if (usingSingleTransitionSlot()) {
            Structure* existingTransition = singleTransition();
            if (existingTransition && existingTransition->m_nameInPrevious.get() == key.first
                && existingTransition->m_attributesInPrevious == key.second
                && (existingTransition->m_specificValueInPrevious == specificValue || existingTransition->m_specificValueInPrevious == 0))
                return existingTransition;
            return 0;
        }

        Transition transition = table()->get(key);
        if (transition.second && transition.second->transitionedFor(specificValue))
            return transition.second;
        return transition.first;
    }

    bool StructureTransitionTable::hasTransition(const StructureTransitionTableHash::Key& key) const
    {
        if (usingSingleTransitionSlot()) {
            Structure* transition = singleTransition();
            return transition && transition->m_nameInPrevious == key.first
            && transition->m_attributesInPrevious == key.second;
        }
        return table()->contains(key);
    }
    
    void StructureTransitionTable::reifySingleTransition()
    {
        ASSERT(usingSingleTransitionSlot());
        Structure* existingTransition = singleTransition();
        TransitionTable* transitionTable = new TransitionTable;
        setTransitionTable(transitionTable);
        if (existingTransition)
            add(make_pair(existingTransition->m_nameInPrevious.get(), existingTransition->m_attributesInPrevious), existingTransition, existingTransition->m_specificValueInPrevious);
    }
} // namespace JSC

#endif // Structure_h
