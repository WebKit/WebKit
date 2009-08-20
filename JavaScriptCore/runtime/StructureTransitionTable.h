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
        typedef HashMap<StructureTransitionTableHash::Key, Transition, StructureTransitionTableHash, StructureTransitionTableHashTraits> TransitionTable;
    public:
        inline bool contains(const StructureTransitionTableHash::Key& key, JSCell* specificValue);
        inline Structure* get(const StructureTransitionTableHash::Key& key, JSCell* specificValue) const;
        bool hasTransition(const StructureTransitionTableHash::Key& key)
        {
            return m_table.contains(key);
        }
        void remove(const StructureTransitionTableHash::Key& key, JSCell* specificValue)
        {
            TransitionTable::iterator find = m_table.find(key);
            if (!specificValue)
                find->second.first = 0;
            else
                find->second.second = 0;
            if (!find->second.first && !find->second.second)
                m_table.remove(find);
        }
        void add(const StructureTransitionTableHash::Key& key, Structure* structure, JSCell* specificValue)
        {
            if (!specificValue) {
                TransitionTable::iterator find = m_table.find(key);
                if (find == m_table.end())
                    m_table.add(key, Transition(structure, 0));
                else
                    find->second.first = structure;
            } else {
                // If we're adding a transition to a specific value, then there cannot be
                // an existing transition
                ASSERT(!m_table.contains(key));
                m_table.add(key, Transition(0, structure));
            }
                

        }
    private:
        TransitionTable m_table;
    };

} // namespace JSC

#endif // StructureTransitionTable_h
