/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef StructureID_h
#define StructureID_h

#include "JSType.h"
#include "JSValue.h"
#include "PropertyMapHashTable.h"
#include "StructureIDChain.h"
#include "StructureIDTransitionTable.h"
#include "TypeInfo.h"
#include "identifier.h"
#include "UString.h"
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#ifndef NDEBUG
#define DUMP_PROPERTYMAP_STATS 0
#else
#define DUMP_PROPERTYMAP_STATS 0
#endif

namespace JSC {

    class PropertyNameArray;
    class PropertyNameArrayData;

    class StructureID : public RefCounted<StructureID> {
    public:
        friend class CTI;
        static PassRefPtr<StructureID> create(JSValue* prototype, const TypeInfo& typeInfo)
        {
            return adoptRef(new StructureID(prototype, typeInfo));
        }

        static void startIgnoringLeaks();
        static void stopIgnoringLeaks();

        static void dumpStatistics();

        static PassRefPtr<StructureID> changePrototypeTransition(StructureID*, JSValue* prototype);
        static PassRefPtr<StructureID> addPropertyTransition(StructureID*, const Identifier& propertyName, unsigned attributes, size_t& offset);
        static PassRefPtr<StructureID> removePropertyTransition(StructureID*, const Identifier& propertyName, size_t& offset);
        static PassRefPtr<StructureID> getterSetterTransition(StructureID*);
        static PassRefPtr<StructureID> toDictionaryTransition(StructureID*);
        static PassRefPtr<StructureID> fromDictionaryTransition(StructureID*);

        ~StructureID();

        void mark()
        {
            if (!m_prototype->marked())
                m_prototype->mark();
        }

        // These should be used with caution.  
        size_t addPropertyWithoutTransition(const Identifier& propertyName, unsigned attributes);
        size_t removePropertyWithoutTransition(const Identifier& propertyName);
        void setPrototypeWithoutTransition(JSValue* prototype) { m_prototype = prototype; }

        bool isDictionary() const { return m_isDictionary; }

        const TypeInfo& typeInfo() const { return m_typeInfo; }

        // For use when first creating a new structure.
        TypeInfo& mutableTypeInfo() { return m_typeInfo; }

        JSValue* storedPrototype() const { return m_prototype; }
        JSValue* prototypeForLookup(ExecState*); 

        StructureID* previousID() const { return m_previous.get(); }

        StructureIDChain* createCachedPrototypeChain();
        void setCachedPrototypeChain(PassRefPtr<StructureIDChain> cachedPrototypeChain) { m_cachedPrototypeChain = cachedPrototypeChain; }
        StructureIDChain* cachedPrototypeChain() const { return m_cachedPrototypeChain.get(); }

        void setCachedTransistionOffset(size_t offset) { m_cachedTransistionOffset = offset; }
        size_t cachedTransistionOffset() const { return m_cachedTransistionOffset; }

        void growPropertyStorageCapacity();
        size_t propertyStorageCapacity() const { return m_propertyStorageCapacity; }
        size_t propertyStorageSize() const { return m_propertyTable ? m_propertyTable->keyCount + m_deletedOffsets.size() : 0; }

        size_t get(const Identifier& propertyName) const;
        size_t get(const Identifier& propertyName, unsigned& attributes) const;
        void getEnumerablePropertyNames(ExecState*, PropertyNameArray&, JSObject*);

        bool hasGetterSetterProperties() const { return m_hasGetterSetterProperties; }
        void setHasGetterSetterProperties(bool hasGetterSetterProperties) { m_hasGetterSetterProperties = hasGetterSetterProperties; }

        bool isEmpty() const { return !m_propertyTable; }

    private:
        StructureID(JSValue* prototype, const TypeInfo&);

        size_t put(const Identifier& propertyName, unsigned attributes);
        size_t remove(const Identifier& propertyName);
        void getEnumerablePropertyNamesInternal(PropertyNameArray&) const;

        void expandPropertyMapHashTable();
        void rehashPropertyMapHashTable();
        void rehashPropertyMapHashTable(unsigned newTableSize);
        void createPropertyMapHashTable();
        void insertIntoPropertyMapHashTable(const PropertyMapEntry&);
        void checkConsistency();

        PropertyMapHashTable* copyPropertyTable();

        void clearEnumerationCache();

        static const unsigned emptyEntryIndex = 0;
    
        static const size_t s_maxTransitionLength = 64;

        TypeInfo m_typeInfo;

        JSValue* m_prototype;
        RefPtr<StructureIDChain> m_cachedPrototypeChain;

        RefPtr<StructureID> m_previous;
        UString::Rep* m_nameInPrevious;

        size_t m_transitionCount;
        union {
            StructureID* singleTransition;
            StructureIDTransitionTable* table;
        } m_transitions;

        RefPtr<PropertyNameArrayData> m_cachedPropertyNameArrayData;

        PropertyMapHashTable* m_propertyTable;
        Vector<unsigned> m_deletedOffsets;

        size_t m_propertyStorageCapacity;

        size_t m_cachedTransistionOffset;

        bool m_isDictionary : 1;
        bool m_hasGetterSetterProperties : 1;
        bool m_usingSingleTransitionSlot : 1;
        unsigned m_attributesInPrevious : 5;
    };

    inline size_t StructureID::get(const Identifier& propertyName) const
    {
        ASSERT(!propertyName.isNull());

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

} // namespace JSC

#endif // StructureID_h
