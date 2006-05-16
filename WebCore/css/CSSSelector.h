/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef CSSSelector_H
#define CSSSelector_H

#include "QualifiedName.h"

namespace WebCore {

// this class represents a selector for a StyleRule
    class CSSSelector
    {
    public:
        CSSSelector()
            : tagHistory(0)
            , simpleSelector(0)
            , nextSelector(0)
            , attr(anyQName())
            , tag(anyQName())
            , m_relation(Descendant)
            , match(None)
            , pseudoId(0)
            , _pseudoType(PseudoNotParsed)
        {}
        
        CSSSelector(const QualifiedName& qName)
            : tagHistory(0)
            , simpleSelector(0)
            , nextSelector(0)
            , attr(anyQName())
            , tag(qName)
            , m_relation(Descendant)
            , match(None)
            , pseudoId(0)
            , _pseudoType(PseudoNotParsed)
        {}

        ~CSSSelector() {
            delete tagHistory;
            delete simpleSelector;
            delete nextSelector;
        }

        void append(CSSSelector* n) {
            CSSSelector* end = this;
            while (end->nextSelector)
                end = end->nextSelector;
            end->nextSelector = n;
        }
        CSSSelector* next() { return nextSelector; }

        /**
         * Print debug output for this selector
         */
        void print();

        /**
         * Re-create selector text from selector's data
         */
        String selectorText() const;

        // checks if the 2 selectors (including sub selectors) agree.
        bool operator ==(const CSSSelector&);

        // tag == -1 means apply to all elements (Selector = *)

        unsigned specificity();

        /* how the attribute value has to match.... Default is Exact */
        enum Match {
            None = 0,
            Id,
            Class,
            Exact,
            Set,
            List,
            Hyphen,
            PseudoClass,
            PseudoElement,
            Contain,   // css3: E[foo*="bar"]
            Begin,     // css3: E[foo^="bar"]
            End        // css3: E[foo$="bar"]
        };

        enum Relation {
            Descendant = 0,
            Child,
            DirectAdjacent,
            IndirectAdjacent,
            SubSelector
        };

        enum PseudoType {
            PseudoNotParsed = 0,
            PseudoOther,
            PseudoEmpty,
            PseudoFirstChild,
            PseudoFirstOfType,
            PseudoLastChild,
            PseudoLastOfType,
            PseudoOnlyChild,
            PseudoOnlyOfType,
            PseudoFirstLine,
            PseudoFirstLetter,
            PseudoLink,
            PseudoVisited,
            PseudoAnyLink,
            PseudoAutofill,
            PseudoHover,
            PseudoDrag,
            PseudoFocus,
            PseudoActive,
            PseudoChecked,
            PseudoEnabled,
            PseudoDisabled,
            PseudoIndeterminate,
            PseudoTarget,
            PseudoBefore,
            PseudoAfter,
            PseudoLang,
            PseudoNot,
            PseudoRoot,
            PseudoSelection
        };

        PseudoType pseudoType() const
        {
            if (_pseudoType == PseudoNotParsed)
                extractPseudoType();
            return static_cast<PseudoType>(_pseudoType);
        }

        bool hasTag() const { return tag != anyQName(); }
        bool hasAttribute() const { return attr != anyQName(); }

        mutable AtomicString value;
        CSSSelector* tagHistory;
        CSSSelector* simpleSelector; // Used for :not.
        CSSSelector* nextSelector; // used for ,-chained selectors
        
        QualifiedName attr;
        QualifiedName tag;
        
        Relation relation() const { return static_cast<Relation>(m_relation); }

        unsigned m_relation          : 3; // enum Relation
        mutable unsigned match       : 4; // enum Match
        unsigned pseudoId            : 3;
        mutable unsigned _pseudoType : 5; // PseudoType

    private:
        void extractPseudoType() const;
    };
}

#endif
