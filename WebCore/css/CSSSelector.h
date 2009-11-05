/*
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
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

    // this class represents a selector for a StyleRule
    class CSSSelector : public Noncopyable {
    public:
        CSSSelector()
            : m_tag(anyQName())
            , m_relation(Descendant)
            , m_match(None)
            , m_pseudoType(PseudoNotParsed)
            , m_parsedNth(false)
            , m_isLastInSelectorList(false)
            , m_hasRareData(false)
        {
        }

        CSSSelector(const QualifiedName& qName)
            : m_tag(qName)
            , m_relation(Descendant)
            , m_match(None)
            , m_pseudoType(PseudoNotParsed)
            , m_parsedNth(false)
            , m_isLastInSelectorList(false)
            , m_hasRareData(false)
        {
        }

        ~CSSSelector()
        {
            if (m_hasRareData)
                delete m_data.m_rareData;
            else
                delete m_data.m_tagHistory;
        }

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
            PseudoFullPageMedia,
            PseudoDefault,
            PseudoDisabled,
            PseudoInputPlaceholder,
            PseudoOptional,
            PseudoRequired,
            PseudoReadOnly,
            PseudoReadWrite,
            PseudoValid,
            PseudoInvalid,
            PseudoIndeterminate,
            PseudoTarget,
            PseudoBefore,
            PseudoAfter,
            PseudoLang,
            PseudoNot,
            PseudoResizer,
            PseudoRoot,
            PseudoScrollbar,
            PseudoScrollbarBack,
            PseudoScrollbarButton,
            PseudoScrollbarCorner,
            PseudoScrollbarForward,
            PseudoScrollbarThumb,
            PseudoScrollbarTrack,
            PseudoScrollbarTrackPiece,
            PseudoWindowInactive,
            PseudoCornerPresent,
            PseudoDecrement,
            PseudoIncrement,
            PseudoHorizontal,
            PseudoVertical,
            PseudoStart,
            PseudoEnd,
            PseudoDoubleButton,
            PseudoSingleButton,
            PseudoNoButton,
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
            PseudoMediaControlsTimelineContainer,
            PseudoMediaControlsVolumeSliderContainer,
            PseudoMediaControlsCurrentTimeDisplay,
            PseudoMediaControlsTimeRemainingDisplay,
            PseudoMediaControlsTimeline,
            PseudoMediaControlsVolumeSlider,
            PseudoMediaControlsSeekBackButton,
            PseudoMediaControlsSeekForwardButton,
            PseudoMediaControlsRewindButton,
            PseudoMediaControlsReturnToRealtimeButton,
            PseudoMediaControlsStatusDisplay,
            PseudoMediaControlsFullscreenButton,
            PseudoInputListButton
        };

        PseudoType pseudoType() const
        {
            if (m_pseudoType == PseudoNotParsed)
                extractPseudoType();
            return static_cast<PseudoType>(m_pseudoType);
        }
        
        CSSSelector* tagHistory() const { return m_hasRareData ? m_data.m_rareData->m_tagHistory.get() : m_data.m_tagHistory; }
        void setTagHistory(CSSSelector* tagHistory);

        bool hasTag() const { return m_tag != anyQName(); }
        bool hasAttribute() const { return m_match == Id || m_match == Class || (m_hasRareData && m_data.m_rareData->m_attribute != anyQName()); }
        
        const QualifiedName& attribute() const;
        const AtomicString& argument() const { return m_hasRareData ? m_data.m_rareData->m_argument : nullAtom; }
        CSSSelector* simpleSelector() const { return m_hasRareData ? m_data.m_rareData->m_simpleSelector.get() : 0; }
        
        void setAttribute(const QualifiedName& value);
        void setArgument(const AtomicString& value);
        void setSimpleSelector(CSSSelector* value);
        
        bool parseNth();
        bool matchNth(int count);

        Relation relation() const { return static_cast<Relation>(m_relation); }

        bool isLastInSelectorList() const { return m_isLastInSelectorList; }
        void setLastInSelectorList() { m_isLastInSelectorList = true; }

        mutable AtomicString m_value;
        QualifiedName m_tag;

        unsigned m_relation           : 3; // enum Relation
        mutable unsigned m_match      : 4; // enum Match
        mutable unsigned m_pseudoType : 8; // PseudoType
        
    private:
        bool m_parsedNth              : 1; // Used for :nth-* 
        bool m_isLastInSelectorList   : 1;
        bool m_hasRareData            : 1;

        void extractPseudoType() const;

        struct RareData : Noncopyable {
            RareData(CSSSelector* tagHistory)
                : m_tagHistory(tagHistory)
                , m_simpleSelector(0)
                , m_attribute(anyQName())
                , m_argument(nullAtom)
                , m_a(0)
                , m_b(0)
            {
            }

            bool parseNth();
            bool matchNth(int count);

            OwnPtr<CSSSelector> m_tagHistory;
            OwnPtr<CSSSelector> m_simpleSelector; // Used for :not.
            QualifiedName m_attribute; // used for attribute selector
            AtomicString m_argument; // Used for :contains, :lang and :nth-*
            int m_a; // Used for :nth-*
            int m_b; // Used for :nth-*
        };

        void createRareData()
        {
            if (m_hasRareData) 
                return;
            m_data.m_rareData = new RareData(m_data.m_tagHistory); 
            m_hasRareData = true;
        }
        
        union DataUnion {
            DataUnion() : m_tagHistory(0) { }
            CSSSelector* m_tagHistory;
            RareData* m_rareData;
        } m_data;
    };

} // namespace WebCore

#endif // CSSSelector_h
