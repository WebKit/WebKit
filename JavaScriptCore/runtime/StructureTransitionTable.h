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

#ifndef StructureTransitionTable_h
#define StructureTransitionTable_h

#include "UString.h"
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/PtrAndFlags.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

namespace JSC {

    class Structure;

    struct StructureTransitionTableHash {
        typedef std::pair<RefPtr<UString::Rep>, unsigned> Key;
        static unsigned hash(const Key& p)
        {
            return p.first->computedHash();
        }

        static bool equal(const Key& a, const Key& b)
        {
            return a == b;
        }

        static const bool safeToCompareToEmptyOrDeleted = true;
    };

    struct StructureTransitionTableHashTraits {
        typedef WTF::HashTraits<RefPtr<UString::Rep> > FirstTraits;
        typedef WTF::GenericHashTraits<unsigned> SecondTraits;
        typedef std::pair<FirstTraits::TraitType, SecondTraits::TraitType > TraitType;

        static const bool emptyValueIsZero = FirstTraits::emptyValueIsZero && SecondTraits::emptyValueIsZero;
        static TraitType emptyValue() { return std::make_pair(FirstTraits::emptyValue(), SecondTraits::emptyValue()); }

        static const bool needsDestruction = FirstTraits::needsDestruction || SecondTraits::needsDestruction;

        static void constructDeletedValue(TraitType& slot) { FirstTraits::constructDeletedValue(slot.first); }
        static bool isDeletedValue(const TraitType& value) { return FirstTraits::isDeletedValue(value.first); }
    };

    class StructureTransitionTable {
        typedef std::pair<Structure*, Structure*> Transition;
        struct TransitionTable : public HashMap<StructureTransitionTableHash::Key, Transition, StructureTransitionTableHash, StructureTransitionTableHashTraits> {
            typedef HashMap<unsigned, Structure*> AnonymousSlotMap;

            void addSlotTransition(unsigned count, Structure* structure)
            {
                ASSERT(!getSlotTransition(count));
                if (!m_anonymousSlotTable)
                    m_anonymousSlotTable.set(new AnonymousSlotMap);
                m_anonymousSlotTable->add(count, structure);
            }

            void removeSlotTransition(unsigned count)
            {
                ASSERT(getSlotTransition(count));
                m_anonymousSlotTable->remove(count);
            }

            Structure* getSlotTransition(unsigned count)
            {
                if (!m_anonymousSlotTable)
                    return 0;

                AnonymousSlotMap::iterator find = m_anonymousSlotTable->find(count);
                if (find == m_anonymousSlotTable->end())
                    return 0;
                return find->second;
            }
        private:
            OwnPtr<AnonymousSlotMap> m_anonymousSlotTable;
        };
    public:
        StructureTransitionTable() {
            m_transitions.m_singleTransition.set(0);
            m_transitions.m_singleTransition.setFlag(usingSingleSlot);
        }

        ~StructureTransitionTable() {
            if (!usingSingleTransitionSlot())
                delete table();
        }

        // The contains and get methods accept imprecise matches, so if an unspecialised transition exists
        // for the given key they will consider that transition to be a match.  If a specialised transition
        // exists and it matches the provided specificValue, get will return the specific transition.
        inline bool contains(const StructureTransitionTableHash::Key&, JSCell* specificValue);
        inline Structure* get(const StructureTransitionTableHash::Key&, JSCell* specificValue) const;
        inline bool hasTransition(const StructureTransitionTableHash::Key& key) const;
        void remove(const StructureTransitionTableHash::Key& key, JSCell* specificValue)
        {
            if (usingSingleTransitionSlot()) {
                ASSERT(contains(key, specificValue));
                setSingleTransition(0);
                return;
            }
            TransitionTable::iterator find = table()->find(key);
            if (!specificValue)
                find->second.first = 0;
            else
                find->second.second = 0;
            if (!find->second.first && !find->second.second)
                table()->remove(find);
        }
        void add(const StructureTransitionTableHash::Key& key, Structure* structure, JSCell* specificValue)
        {
            if (usingSingleTransitionSlot()) {
                if (!singleTransition()) {
                    setSingleTransition(structure);
                    return;
                }
                reifySingleTransition();
            }
            if (!specificValue) {
                TransitionTable::iterator find = table()->find(key);
                if (find == table()->end())
                    table()->add(key, Transition(structure, 0));
                else
                    find->second.first = structure;
            } else {
                // If we're adding a transition to a specific value, then there cannot be
                // an existing transition
                ASSERT(!table()->contains(key));
                table()->add(key, Transition(0, structure));
            }
        }

        Structure* getAnonymousSlotTransition(unsigned count)
        {
            if (usingSingleTransitionSlot())
                return 0;
            return table()->getSlotTransition(count);
        }

        void addAnonymousSlotTransition(unsigned count, Structure* structure)
        {
            if (usingSingleTransitionSlot())
                reifySingleTransition();
            ASSERT(!table()->getSlotTransition(count));
            table()->addSlotTransition(count, structure);
        }
        
        void removeAnonymousSlotTransition(unsigned count)
        {
            ASSERT(!usingSingleTransitionSlot());
            table()->removeSlotTransition(count);
        }
    private:
        TransitionTable* table() const { ASSERT(!usingSingleTransitionSlot()); return m_transitions.m_table; }
        Structure* singleTransition() const {
            ASSERT(usingSingleTransitionSlot());
            return m_transitions.m_singleTransition.get();
        }
        bool usingSingleTransitionSlot() const { return m_transitions.m_singleTransition.isFlagSet(usingSingleSlot); }
        void setSingleTransition(Structure* structure)
        { 
            ASSERT(usingSingleTransitionSlot());
            m_transitions.m_singleTransition.set(structure);
        }

        void setTransitionTable(TransitionTable* table)
        {
            ASSERT(usingSingleTransitionSlot());
#ifndef NDEBUG
            setSingleTransition(0);
#endif
            m_transitions.m_table = table;
            // This implicitly clears the flag that indicates we're using a single transition
            ASSERT(!usingSingleTransitionSlot());
        }
        inline void reifySingleTransition();

        enum UsingSingleSlot {
            usingSingleSlot
        };
        // Last bit indicates whether we are using the single transition optimisation
        union {
            TransitionTable* m_table;
            PtrAndFlagsBase<Structure, UsingSingleSlot> m_singleTransition;
        } m_transitions;
    };

} // namespace JSC

#endif // StructureTransitionTable_h
