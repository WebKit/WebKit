/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CSSSelector_h
#define CSSSelector_h

#include "QualifiedName.h"

namespace WebCore {

    // this class represents a selector for a StyleRule
    class CSSSelector {
    public:
        CSSSelector()
            : m_tagHistory(0)
            , m_simpleSelector(0)
            , m_nextSelector(0)
            , m_argument(nullAtom)
            , m_attr(anyQName())
            , m_tag(anyQName())
            , m_relation(Descendant)
            , m_match(None)
            , m_pseudoType(PseudoNotParsed)
        {
        }

        CSSSelector(const QualifiedName& qName)
            : m_tagHistory(0)
            , m_simpleSelector(0)
            , m_nextSelector(0)
            , m_argument(nullAtom)
            , m_attr(anyQName())
            , m_tag(qName)
            , m_relation(Descendant)
            , m_match(None)
            , m_pseudoType(PseudoNotParsed)
        {
        }

        ~CSSSelector()
        {
            delete m_tagHistory;
            delete m_simpleSelector;
            delete m_nextSelector;
        }

        void append(CSSSelector* n)
        {
            CSSSelector* end = this;
            while (end->m_nextSelector)
                end = end->m_nextSelector;
            end->m_nextSelector = n;
        }

        CSSSelector* next() { return m_nextSelector; }

        /**
         * Print debug output for this selector
         */
        void print();

        /**
         * Re-create selector text from selector's data
         */
        String selectorText() const;

        // checks if the 2 selectors (including sub selectors) agree.
        bool operator==(const CSSSelector&);

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
            PseudoUnknown,
            PseudoEmpty,
            PseudoFirstChild,
            PseudoFirstOfType,
            PseudoLastChild,
            PseudoLastOfType,
            PseudoOnlyChild,
            PseudoOnlyOfType,
            PseudoFirstLine,
            PseudoFirstLetter,
            PseudoNthChild,
            PseudoNthOfType,
            PseudoNthLastChild,
            PseudoNthLastOfType,
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
            PseudoSelection,
            PseudoFileUploadButton,
            PseudoSliderThumb,
            PseudoSearchCancelButton,
            PseudoSearchDecoration,
            PseudoSearchResultsDecoration,
            PseudoSearchResultsButton,
            PseudoMediaControlsPanel,
            PseudoMediaControlsMuteButton,
            PseudoMediaControlsPlayButton,
            PseudoMediaControlsTimeDisplay,
            PseudoMediaControlsTimeline,
            PseudoMediaControlsSeekBackButton,
            PseudoMediaControlsSeekForwardButton,
            PseudoMediaControlsFullscreenButton
        };

        PseudoType pseudoType() const
        {
            if (m_pseudoType == PseudoNotParsed)
                extractPseudoType();
            return static_cast<PseudoType>(m_pseudoType);
        }

        bool hasTag() const { return m_tag != anyQName(); }
        bool hasAttribute() const { return m_attr != anyQName(); }

        Relation relation() const { return static_cast<Relation>(m_relation); }

        mutable AtomicString m_value;
        CSSSelector* m_tagHistory;
        CSSSelector* m_simpleSelector; // Used for :not.
        CSSSelector* m_nextSelector; // used for ,-chained selectors
        AtomicString m_argument; // Used for :contains, :lang and :nth-*

        QualifiedName m_attr;
        QualifiedName m_tag;

        unsigned m_relation           : 3; // enum Relation
        mutable unsigned m_match      : 4; // enum Match
        mutable unsigned m_pseudoType : 8; // PseudoType

    private:
        void extractPseudoType() const;
    };

} // namespace WebCore

#endif // CSSSelector_h
