// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005 Apple Computer, Inc
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

#ifndef KJS_IDENTIFIER_SEQUENCED_SET_H
#define KJS_IDENTIFIER_SEQUENCED_SET_H

#include "identifier.h"

#include <kxmlcore/HashSet.h>

namespace KJS {

    class IdentifierSequencedSet;

    class IdentifierSequencedSetIterator {
    private:
        friend class IdentifierSequencedSet;
        IdentifierSequencedSetIterator(const Identifier *position) : m_position(position) {}

    public:
        typedef IdentifierSequencedSetIterator iterator;

        IdentifierSequencedSetIterator() {}
        
        // default copy, assignment and destructor are ok
        
        const Identifier& operator*() const 
        { 
            return *m_position; 
        }

        const Identifier *operator->() const 
        { 
            return &(operator*()); 
        }
        
        iterator& operator++() 
        { 
            ++m_position; 
            return *this;
        }
        
        // postfix ++ intentionally omitted
        
        // Comparison.
        bool operator==(const iterator& other) const
        { 
            return m_position == other.m_position; 
        }
        
        bool operator!=(const iterator& other) const 
        { 
            return m_position != other.m_position; 
        }
        
    private:
        const Identifier *m_position;
    };

    class IdentifierSequencedSet {
    public:
        typedef IdentifierSequencedSetIterator iterator;

        IdentifierSequencedSet();
        ~IdentifierSequencedSet();

        void insert(const Identifier&);
        iterator begin() const { return iterator(m_vector); }
        iterator end() const { return iterator(m_vector + m_vectorLength); }
        int size() const { return m_vectorLength; }

    private:
        void deallocateVector();

        typedef HashSet<UString::Impl *, PointerHash<UString::Impl *> > IdentifierSet;
        IdentifierSet m_set;

        Identifier *m_vector;
        int m_vectorLength;
        int m_vectorCapacity;
    };


} // namespace KJS


#endif // KJS_IDENTIFIER_SEQUENCED_SET_H
