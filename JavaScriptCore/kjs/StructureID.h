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
#include "PropertyMap.h"
#include "ustring.h"
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace JSC {

    class JSValue;
    class StructureIDChain;

    struct TransitionTableHash {
        typedef std::pair<RefPtr<UString::Rep>, unsigned> TransitionTableKey;
        static unsigned hash(const TransitionTableKey& p)
        {
            return p.first->computedHash();
        }

        static bool equal(const TransitionTableKey& a, const TransitionTableKey& b)
        {
            return a == b;
        }

        static const bool safeToCompareToEmptyOrDeleted = true;
    };

    struct TransitionTableHashTraits {
        typedef WTF::HashTraits<RefPtr<UString::Rep> > FirstTraits;
        typedef WTF::GenericHashTraits<unsigned> SecondTraits;
        typedef std::pair<FirstTraits::TraitType, SecondTraits::TraitType> TraitType;

        static const bool emptyValueIsZero = FirstTraits::emptyValueIsZero && SecondTraits::emptyValueIsZero;
        static TraitType emptyValue() { return std::make_pair(FirstTraits::emptyValue(), SecondTraits::emptyValue()); }

        static const bool needsDestruction = FirstTraits::needsDestruction || SecondTraits::needsDestruction;

        static void constructDeletedValue(TraitType& slot) { FirstTraits::constructDeletedValue(slot.first); }
        static bool isDeletedValue(const TraitType& value) { return FirstTraits::isDeletedValue(value.first); }
    };

    class StructureID : public RefCounted<StructureID> {
    public:
        static PassRefPtr<StructureID> create(JSValue* prototype, JSType type = ObjectType)
        {
            return adoptRef(new StructureID(prototype, type));
        }

        static PassRefPtr<StructureID> changePrototypeTransition(StructureID*, JSValue* prototype);
        static PassRefPtr<StructureID> addPropertyTransition(StructureID*, const Identifier& propertyName, JSValue*, unsigned attributes, JSObject* slotBase, PutPropertySlot&, PropertyStorage&);
        static PassRefPtr<StructureID> getterSetterTransition(StructureID*);
        static PassRefPtr<StructureID> toDictionaryTransition(StructureID*);
        static PassRefPtr<StructureID> fromDictionaryTransition(StructureID*);

        ~StructureID();

        void mark()
        {
            if (!m_prototype->marked())
                m_prototype->mark();
        }

        bool isDictionary() const { return m_isDictionary; }

        JSValue* storedPrototype() const { return m_prototype; }
        JSValue* prototypeForLookup(ExecState*); 
        
        void setCachedPrototypeChain(PassRefPtr<StructureIDChain> cachedPrototypeChain) { m_cachedPrototypeChain = cachedPrototypeChain; }
        StructureIDChain* cachedPrototypeChain() const { return m_cachedPrototypeChain.get(); }

        const PropertyMap& propertyMap() const { return m_propertyMap; }
        PropertyMap& propertyMap() { return m_propertyMap; }

    private:
        typedef std::pair<RefPtr<UString::Rep>, unsigned> TransitionTableKey;
        typedef HashMap<TransitionTableKey, StructureID*, TransitionTableHash, TransitionTableHashTraits> TransitionTable;

        StructureID(JSValue* prototype, JSType);
        
        static const size_t s_maxTransitionLength = 64;

        bool m_isDictionary;
        JSType m_type;

        JSValue* m_prototype;
        RefPtr<StructureIDChain> m_cachedPrototypeChain;

        RefPtr<StructureID> m_previous;
        UString::Rep* m_nameInPrevious;
        unsigned m_attributesInPrevious;

        size_t m_transitionCount;
        TransitionTable m_transitionTable;

        PropertyMap m_propertyMap;
    };

    class StructureIDChain : public RefCounted<StructureIDChain> {
    public:
        static PassRefPtr<StructureIDChain> create(StructureID* structureID) { return adoptRef(new StructureIDChain(structureID)); }

        RefPtr<StructureID>* head() { return m_vector.get(); }

    private:
        StructureIDChain(StructureID* structureID);

        OwnArrayPtr<RefPtr<StructureID> > m_vector;
    };

} // namespace JSC

#endif // StructureID_h
