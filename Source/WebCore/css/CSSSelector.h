/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010, 2013, 2014 Apple Inc. All rights reserved.
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

#pragma once

#include "QualifiedName.h"
#include "RenderStyleConstants.h"
#include <wtf/Variant.h>

namespace WebCore {
    class CSSSelectorList;

    enum class SelectorSpecificityIncrement {
        ClassA = 0x10000,
        ClassB = 0x100,
        ClassC = 1
    };

    // this class represents a selector for a StyleRule
    class CSSSelector {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        CSSSelector();
        CSSSelector(const CSSSelector&);
        explicit CSSSelector(const QualifiedName&, bool tagIsForNamespaceRule = false);

        ~CSSSelector();

        /**
         * Re-create selector text from selector's data
         */
        String selectorText(const String& = emptyString()) const;

        // checks if the 2 selectors (including sub selectors) agree.
        bool operator==(const CSSSelector&) const;

        static const unsigned maxValueMask = 0xffffff;
        static const unsigned idMask = 0xff0000;
        static const unsigned classMask = 0xff00;
        static const unsigned elementMask = 0xff;

        unsigned staticSpecificity(bool& ok) const;
        unsigned specificityForPage() const;
        unsigned simpleSelectorSpecificity() const;
        static unsigned addSpecificities(unsigned, unsigned);

        /* how the attribute value has to match.... Default is Exact */
        enum Match {
            Unknown = 0,
            Tag,
            Id,
            Class,
            Exact,
            Set,
            List,
            Hyphen,
            PseudoClass,
            PseudoElement,
            Contain, // css3: E[foo*="bar"]
            Begin, // css3: E[foo^="bar"]
            End, // css3: E[foo$="bar"]
            PagePseudoClass
        };

        enum RelationType {
            Subselector,
            DescendantSpace,
            Child,
            DirectAdjacent,
            IndirectAdjacent,
            ShadowDescendant
        };

        enum PseudoClassType {
            PseudoClassUnknown = 0,
            PseudoClassEmpty,
            PseudoClassFirstChild,
            PseudoClassFirstOfType,
            PseudoClassLastChild,
            PseudoClassLastOfType,
            PseudoClassOnlyChild,
            PseudoClassOnlyOfType,
            PseudoClassNthChild,
            PseudoClassNthOfType,
            PseudoClassNthLastChild,
            PseudoClassNthLastOfType,
            PseudoClassLink,
            PseudoClassVisited,
            PseudoClassAny,
            PseudoClassAnyLink,
            PseudoClassAnyLinkDeprecated,
            PseudoClassAutofill,
            PseudoClassAutofillStrongPassword,
            PseudoClassHover,
            PseudoClassDrag,
            PseudoClassFocus,
            PseudoClassFocusWithin,
            PseudoClassActive,
            PseudoClassChecked,
            PseudoClassEnabled,
            PseudoClassFullPageMedia,
            PseudoClassDefault,
            PseudoClassDisabled,
            PseudoClassMatches,
            PseudoClassOptional,
            PseudoClassPlaceholderShown,
            PseudoClassRequired,
            PseudoClassReadOnly,
            PseudoClassReadWrite,
            PseudoClassValid,
            PseudoClassInvalid,
            PseudoClassIndeterminate,
            PseudoClassTarget,
            PseudoClassLang,
            PseudoClassNot,
            PseudoClassRoot,
            PseudoClassScope,
            PseudoClassWindowInactive,
            PseudoClassCornerPresent,
            PseudoClassDecrement,
            PseudoClassIncrement,
            PseudoClassHorizontal,
            PseudoClassVertical,
            PseudoClassStart,
            PseudoClassEnd,
            PseudoClassDoubleButton,
            PseudoClassSingleButton,
            PseudoClassNoButton,
#if ENABLE(FULLSCREEN_API)
            PseudoClassFullScreen,
            PseudoClassFullScreenDocument,
            PseudoClassFullScreenAncestor,
            PseudoClassAnimatingFullScreenTransition,
            PseudoClassFullScreenControlsHidden,
#endif
            PseudoClassInRange,
            PseudoClassOutOfRange,
#if ENABLE(VIDEO_TRACK)
            PseudoClassFuture,
            PseudoClassPast,
#endif
#if ENABLE(CSS_SELECTORS_LEVEL4)
            PseudoClassDir,
            PseudoClassRole,
#endif
            PseudoClassHost,
            PseudoClassDefined,
        };

        enum PseudoElementType {
            PseudoElementUnknown = 0,
            PseudoElementAfter,
            PseudoElementBefore,
#if ENABLE(VIDEO_TRACK)
            PseudoElementCue,
#endif
            PseudoElementFirstLetter,
            PseudoElementFirstLine,
            PseudoElementMarker,
            PseudoElementResizer,
            PseudoElementScrollbar,
            PseudoElementScrollbarButton,
            PseudoElementScrollbarCorner,
            PseudoElementScrollbarThumb,
            PseudoElementScrollbarTrack,
            PseudoElementScrollbarTrackPiece,
            PseudoElementSelection,
            PseudoElementSlotted,
            PseudoElementWebKitCustom,

            // WebKitCustom that appeared in an old prefixed form
            // and need special handling.
            PseudoElementWebKitCustomLegacyPrefixed,
        };

        enum PagePseudoClassType {
            PagePseudoClassFirst = 1,
            PagePseudoClassLeft,
            PagePseudoClassRight,
        };

        enum MarginBoxType {
            TopLeftCornerMarginBox,
            TopLeftMarginBox,
            TopCenterMarginBox,
            TopRightMarginBox,
            TopRightCornerMarginBox,
            BottomLeftCornerMarginBox,
            BottomLeftMarginBox,
            BottomCenterMarginBox,
            BottomRightMarginBox,
            BottomRightCornerMarginBox,
            LeftTopMarginBox,
            LeftMiddleMarginBox,
            LeftBottomMarginBox,
            RightTopMarginBox,
            RightMiddleMarginBox,
            RightBottomMarginBox,
        };

        enum AttributeMatchType {
            CaseSensitive,
            CaseInsensitive,
        };

        static PseudoElementType parsePseudoElementType(const String&);
        static PseudoId pseudoId(PseudoElementType);

        // Selectors are kept in an array by CSSSelectorList. The next component of the selector is
        // the next item in the array.
        const CSSSelector* tagHistory() const { return m_isLastInTagHistory ? 0 : const_cast<CSSSelector*>(this + 1); }

        const QualifiedName& tagQName() const;
        const AtomicString& tagLowercaseLocalName() const;

        const AtomicString& value() const;
        const AtomicString& serializingValue() const;
        const QualifiedName& attribute() const;
        const AtomicString& attributeCanonicalLocalName() const;
        const AtomicString& argument() const { return WTF::holds_alternative<RefPtr<RareData>>(m_data) ? WTF::get<RefPtr<RareData>>(m_data)->m_argument : nullAtom(); }
        bool attributeValueMatchingIsCaseInsensitive() const;
        const Vector<AtomicString>* langArgumentList() const { return WTF::holds_alternative<RefPtr<RareData>>(m_data) ? WTF::get<RefPtr<RareData>>(m_data)->m_langArgumentList.get() : nullptr; }
        const CSSSelectorList* selectorList() const { return WTF::holds_alternative<RefPtr<RareData>>(m_data) ? WTF::get<RefPtr<RareData>>(m_data)->m_selectorList.get() : nullptr; }

        void setValue(const AtomicString&, bool matchLowerCase = false);
        
        void setAttribute(const QualifiedName&, bool convertToLowercase, AttributeMatchType);
        void setNth(int a, int b);
        void setArgument(const AtomicString&);
        void setLangArgumentList(std::unique_ptr<Vector<AtomicString>>);
        void setSelectorList(std::unique_ptr<CSSSelectorList>);

        bool matchNth(int count) const;
        int nthA() const;
        int nthB() const;

        bool hasDescendantRelation() const { return relation() == DescendantSpace; }

        bool hasDescendantOrChildRelation() const { return relation() == Child || hasDescendantRelation(); }

        PseudoClassType pseudoClassType() const
        {
            ASSERT(match() == PseudoClass);
            return static_cast<PseudoClassType>(m_pseudoType);
        }
        void setPseudoClassType(PseudoClassType pseudoType)
        {
            m_pseudoType = pseudoType;
            ASSERT(m_pseudoType == pseudoType);
        }

        PseudoElementType pseudoElementType() const
        {
            ASSERT(match() == PseudoElement);
            return static_cast<PseudoElementType>(m_pseudoType);
        }
        void setPseudoElementType(PseudoElementType pseudoElementType)
        {
            m_pseudoType = pseudoElementType;
            ASSERT(m_pseudoType == pseudoElementType);
        }

        PagePseudoClassType pagePseudoClassType() const
        {
            ASSERT(match() == PagePseudoClass);
            return static_cast<PagePseudoClassType>(m_pseudoType);
        }
        void setPagePseudoType(PagePseudoClassType pagePseudoType)
        {
            m_pseudoType = pagePseudoType;
            ASSERT(m_pseudoType == pagePseudoType);
        }

        bool matchesPseudoElement() const;
        bool isUnknownPseudoElement() const;
        bool isCustomPseudoElement() const;
        bool isWebKitCustomPseudoElement() const;
        bool isSiblingSelector() const;
        bool isAttributeSelector() const;

        RelationType relation() const { return static_cast<RelationType>(m_relation); }
        void setRelation(RelationType relation)
        {
            m_relation = relation;
            ASSERT(m_relation == relation);
        }

        Match match() const { return static_cast<Match>(m_match); }
        void setMatch(Match match)
        {
            m_match = match;
            ASSERT(m_match == match);
        }

        bool isLastInSelectorList() const { return m_isLastInSelectorList; }
        void setLastInSelectorList() { m_isLastInSelectorList = true; }
        bool isLastInTagHistory() const { return m_isLastInTagHistory; }
        void setNotLastInTagHistory() { m_isLastInTagHistory = false; }

#if !ASSERT_DISABLED
        bool isForPage() const { return m_isForPage; }
        void setForPage() { m_isForPage = true; }
#endif

    private:
        struct RareData;
        struct NameWithCase;
        Variant<AtomicString, QualifiedName, RefPtr<RareData>, RefPtr<NameWithCase>> m_data;

        unsigned char m_relation               : 3; // enum RelationType.
        mutable unsigned char m_match          : 4; // enum Match.
        unsigned char m_isLastInSelectorList   : 1;
        unsigned char m_isLastInTagHistory     : 1;
        mutable unsigned char m_pseudoType     : 7; // enum PseudoClassType.
        unsigned char m_tagIsForNamespaceRule  : 1;
#if !ASSERT_DISABLED
        unsigned char m_isForPage              : 1;
#endif
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
        unsigned char m_destructorHasBeenCalled : 1;
#endif

        unsigned simpleSelectorSpecificityForPage() const;

        // Hide.
        CSSSelector& operator=(const CSSSelector&);

        struct RareData : public RefCounted<RareData> {
            RareData(AtomicString&& value);
            ~RareData();

            bool matchNth(int count);

            // For quirks mode, class and id are case-insensitive. In the case where uppercase
            // letters are used in quirks mode, |m_matchingValue| holds the lowercase class/id
            // and |m_serializingValue| holds the original string.
            AtomicString m_matchingValue;
            AtomicString m_serializingValue;
            
            int m_a { 0 }; // Used for :nth-*
            int m_b { 0 }; // Used for :nth-*
            QualifiedName m_attribute { anyQName() }; // used for attribute selector
            AtomicString m_attributeCanonicalLocalName;
            AtomicString m_argument { nullAtom() }; // Used for :contains and :nth-*
            std::unique_ptr<Vector<AtomicString>> m_langArgumentList; // Used for :lang arguments.
            std::unique_ptr<CSSSelectorList> m_selectorList; // Used for :matches() and :not().
            bool m_caseInsensitiveAttributeValueMatching { false };
        };
        void createRareData();

        struct NameWithCase : public RefCounted<NameWithCase> {
            NameWithCase(const QualifiedName& originalName, const AtomicString& lowercaseName)
                : m_originalName(originalName)
                , m_lowercaseLocalName(lowercaseName)
            {
                ASSERT(originalName.localName() != lowercaseName);
            }

            const QualifiedName m_originalName;
            const AtomicString m_lowercaseLocalName;
        };
    };

inline const QualifiedName& CSSSelector::attribute() const
{
    ASSERT(isAttributeSelector());
    ASSERT(WTF::holds_alternative<RefPtr<RareData>>(m_data));
    return WTF::get<RefPtr<RareData>>(m_data)->m_attribute;
}

inline const AtomicString& CSSSelector::attributeCanonicalLocalName() const
{
    ASSERT(isAttributeSelector());
    ASSERT(WTF::holds_alternative<RefPtr<RareData>>(m_data));
    return WTF::get<RefPtr<RareData>>(m_data)->m_attributeCanonicalLocalName;
}

inline bool CSSSelector::matchesPseudoElement() const
{
    return match() == PseudoElement;
}

inline bool CSSSelector::isUnknownPseudoElement() const
{
    return match() == PseudoElement && pseudoElementType() == PseudoElementUnknown;
}

inline bool CSSSelector::isCustomPseudoElement() const
{
    return match() == PseudoElement
        && (pseudoElementType() == PseudoElementWebKitCustom
            || pseudoElementType() == PseudoElementWebKitCustomLegacyPrefixed);
}

inline bool CSSSelector::isWebKitCustomPseudoElement() const
{
    return pseudoElementType() == PseudoElementWebKitCustom || pseudoElementType() == PseudoElementWebKitCustomLegacyPrefixed;
}

static inline bool pseudoClassIsRelativeToSiblings(CSSSelector::PseudoClassType type)
{
    return type == CSSSelector::PseudoClassEmpty
        || type == CSSSelector::PseudoClassFirstChild
        || type == CSSSelector::PseudoClassFirstOfType
        || type == CSSSelector::PseudoClassLastChild
        || type == CSSSelector::PseudoClassLastOfType
        || type == CSSSelector::PseudoClassOnlyChild
        || type == CSSSelector::PseudoClassOnlyOfType
        || type == CSSSelector::PseudoClassNthChild
        || type == CSSSelector::PseudoClassNthOfType
        || type == CSSSelector::PseudoClassNthLastChild
        || type == CSSSelector::PseudoClassNthLastOfType;
}

inline bool CSSSelector::isSiblingSelector() const
{
    return relation() == DirectAdjacent
        || relation() == IndirectAdjacent
        || (match() == CSSSelector::PseudoClass && pseudoClassIsRelativeToSiblings(pseudoClassType()));
}

inline bool CSSSelector::isAttributeSelector() const
{
    return match() == CSSSelector::Exact
        || match() ==  CSSSelector::Set
        || match() == CSSSelector::List
        || match() == CSSSelector::Hyphen
        || match() == CSSSelector::Contain
        || match() == CSSSelector::Begin
        || match() == CSSSelector::End;
}

inline void CSSSelector::setValue(const AtomicString& value, bool matchLowerCase)
{
    ASSERT(match() != Tag);
    AtomicString matchingValue = matchLowerCase ? value.convertToASCIILowercase() : value;
    if (!WTF::holds_alternative<RefPtr<RareData>>(m_data) && matchingValue != value)
        createRareData();
    
    if (!WTF::holds_alternative<RefPtr<RareData>>(m_data)) {
        m_data = value;
        return;
    }

    WTF::get<RefPtr<RareData>>(m_data)->m_matchingValue = WTFMove(matchingValue);
    WTF::get<RefPtr<RareData>>(m_data)->m_serializingValue = value;
}

inline CSSSelector::CSSSelector()
    : m_relation(DescendantSpace)
    , m_match(Unknown)
    , m_isLastInSelectorList(false)
    , m_isLastInTagHistory(true)
    , m_pseudoType(0)
    , m_tagIsForNamespaceRule(false)
#if !ASSERT_DISABLED
    , m_isForPage(false)
#endif
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    , m_destructorHasBeenCalled(false)
#endif
{
}

inline CSSSelector::CSSSelector(const CSSSelector& o)
    : m_data(o.m_data)
    , m_relation(o.m_relation)
    , m_match(o.m_match)
    , m_isLastInSelectorList(o.m_isLastInSelectorList)
    , m_isLastInTagHistory(o.m_isLastInTagHistory)
    , m_pseudoType(o.m_pseudoType)
    , m_tagIsForNamespaceRule(o.m_tagIsForNamespaceRule)
#if !ASSERT_DISABLED
    , m_isForPage(o.m_isForPage)
#endif
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    , m_destructorHasBeenCalled(false)
#endif
{
}

inline CSSSelector::~CSSSelector()
{
    ASSERT_WITH_SECURITY_IMPLICATION(!m_destructorHasBeenCalled);
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    m_destructorHasBeenCalled = true;
#endif
}

inline const QualifiedName& CSSSelector::tagQName() const
{
    ASSERT(match() == Tag);
    if (WTF::holds_alternative<RefPtr<NameWithCase>>(m_data))
        return WTF::get<RefPtr<NameWithCase>>(m_data)->m_originalName;
    return WTF::get<QualifiedName>(m_data);
}

inline const AtomicString& CSSSelector::tagLowercaseLocalName() const
{
    if (WTF::holds_alternative<RefPtr<NameWithCase>>(m_data))
        return WTF::get<RefPtr<NameWithCase>>(m_data)->m_lowercaseLocalName;
    return WTF::get<QualifiedName>(m_data).localName();
}

inline const AtomicString& CSSSelector::value() const
{
    ASSERT(match() != Tag);
    if (WTF::holds_alternative<RefPtr<RareData>>(m_data))
        return WTF::get<RefPtr<RareData>>(m_data)->m_matchingValue;

    return WTF::get<AtomicString>(m_data);
}

inline const AtomicString& CSSSelector::serializingValue() const
{
    ASSERT(match() != Tag);
    if (WTF::holds_alternative<RefPtr<RareData>>(m_data))
        return WTF::get<RefPtr<RareData>>(m_data)->m_serializingValue;
    
    return WTF::get<AtomicString>(m_data);
}
    
inline bool CSSSelector::attributeValueMatchingIsCaseInsensitive() const
{
    return WTF::holds_alternative<RefPtr<RareData>>(m_data) && WTF::get<RefPtr<RareData>>(m_data)->m_caseInsensitiveAttributeValueMatching;
}

} // namespace WebCore
