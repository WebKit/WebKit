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

#ifndef CSSSelector_h
#define CSSSelector_h

#include "QualifiedName.h"
#include "RenderStyleConstants.h"
#include <wtf/Noncopyable.h>

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
        String selectorText(const String& = "") const;

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

        enum Relation {
            Descendant = 0,
            Child,
            DirectAdjacent,
            IndirectAdjacent,
            SubSelector,
            ShadowDescendant,
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
            PseudoClassHover,
            PseudoClassDrag,
            PseudoClassFocus,
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
#if ENABLE(SHADOW_DOM)
            PseudoClassHost,
#endif
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
            PseudoElementResizer,
            PseudoElementScrollbar,
            PseudoElementScrollbarButton,
            PseudoElementScrollbarCorner,
            PseudoElementScrollbarThumb,
            PseudoElementScrollbarTrack,
            PseudoElementScrollbarTrackPiece,
            PseudoElementSelection,
#if ENABLE(SHADOW_DOM)
            PseudoElementSlotted,
#endif
            PseudoElementUserAgentCustom,
            PseudoElementWebKitCustom,
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

        static PseudoElementType parsePseudoElementType(const String&);
        static PseudoId pseudoId(PseudoElementType);

        // Selectors are kept in an array by CSSSelectorList. The next component of the selector is
        // the next item in the array.
        const CSSSelector* tagHistory() const { return m_isLastInTagHistory ? 0 : const_cast<CSSSelector*>(this + 1); }

        const QualifiedName& tagQName() const;
        const AtomicString& tagLowercaseLocalName() const;

        const AtomicString& value() const;
        const QualifiedName& attribute() const;
        const AtomicString& attributeCanonicalLocalName() const;
        const AtomicString& argument() const { return m_hasRareData ? m_data.m_rareData->m_argument : nullAtom; }
        bool attributeValueMatchingIsCaseInsensitive() const;
        const Vector<AtomicString>* langArgumentList() const { return m_hasRareData ? m_data.m_rareData->m_langArgumentList.get() : nullptr; }
        const CSSSelectorList* selectorList() const { return m_hasRareData ? m_data.m_rareData->m_selectorList.get() : nullptr; }

        void setValue(const AtomicString&);
        void setAttribute(const QualifiedName&, bool isCaseInsensitive);
        void setArgument(const AtomicString&);
        void setAttributeValueMatchingIsCaseInsensitive(bool);
        void setLangArgumentList(std::unique_ptr<Vector<AtomicString>>);
        void setSelectorList(std::unique_ptr<CSSSelectorList>);

        bool parseNth() const;
        bool matchNth(int count) const;
        int nthA() const;
        int nthB() const;

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
        bool isSiblingSelector() const;
        bool isAttributeSelector() const;

        Relation relation() const { return static_cast<Relation>(m_relation); }
        void setRelation(Relation relation)
        {
            m_relation = relation;
            ASSERT(m_relation == relation);
        }

#if ENABLE(CSS_SELECTORS_LEVEL4)
        void setDescendantUseDoubleChildSyntax()
        {
            ASSERT(relation() == Descendant);
            m_descendantDoubleChildSyntax = true;
        }
#endif

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

        bool isForPage() const { return m_isForPage; }
        void setForPage() { m_isForPage = true; }

    private:
        unsigned m_relation              : 3; // enum Relation.
        mutable unsigned m_match         : 4; // enum Match.
        mutable unsigned m_pseudoType    : 8; // PseudoType.
        mutable unsigned m_parsedNth     : 1; // Used for :nth-*.
        unsigned m_isLastInSelectorList  : 1;
        unsigned m_isLastInTagHistory    : 1;
        unsigned m_hasRareData           : 1;
        unsigned m_hasNameWithCase       : 1;
        unsigned m_isForPage             : 1;
        unsigned m_tagIsForNamespaceRule : 1;
#if ENABLE(CSS_SELECTORS_LEVEL4)
        unsigned m_descendantDoubleChildSyntax : 1;
#endif
        unsigned m_caseInsensitiveAttributeValueMatching : 1;
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
        unsigned m_destructorHasBeenCalled : 1;
#endif

        unsigned simpleSelectorSpecificityForPage() const;

        // Hide.
        CSSSelector& operator=(const CSSSelector&);

        struct RareData : public RefCounted<RareData> {
            static Ref<RareData> create(PassRefPtr<AtomicStringImpl> value) { return adoptRef(*new RareData(value)); }
            ~RareData();

            bool parseNth();
            bool matchNth(int count);

            AtomicStringImpl* m_value; // Plain pointer to keep things uniform with the union.
            int m_a; // Used for :nth-*
            int m_b; // Used for :nth-*
            QualifiedName m_attribute; // used for attribute selector
            AtomicString m_attributeCanonicalLocalName;
            AtomicString m_argument; // Used for :contains and :nth-*
            std::unique_ptr<Vector<AtomicString>> m_langArgumentList; // Used for :lang arguments.
            std::unique_ptr<CSSSelectorList> m_selectorList; // Used for :matches() and :not().
        
        private:
            RareData(PassRefPtr<AtomicStringImpl> value);
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

        union DataUnion {
            DataUnion() : m_value(0) { }
            AtomicStringImpl* m_value;
            QualifiedName::QualifiedNameImpl* m_tagQName;
            RareData* m_rareData;
            NameWithCase* m_nameWithCase;
        } m_data;
    };

inline const QualifiedName& CSSSelector::attribute() const
{
    ASSERT(isAttributeSelector());
    ASSERT(m_hasRareData);
    return m_data.m_rareData->m_attribute;
}

inline const AtomicString& CSSSelector::attributeCanonicalLocalName() const
{
    ASSERT(isAttributeSelector());
    ASSERT(m_hasRareData);
    return m_data.m_rareData->m_attributeCanonicalLocalName;
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
    return match() == PseudoElement && (pseudoElementType() == PseudoElementUserAgentCustom || pseudoElementType() == PseudoElementWebKitCustom);
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

inline void CSSSelector::setValue(const AtomicString& value)
{
    ASSERT(match() != Tag);
    // Need to do ref counting manually for the union.
    if (m_hasRareData) {
        if (m_data.m_rareData->m_value)
            m_data.m_rareData->m_value->deref();
        m_data.m_rareData->m_value = value.impl();
        m_data.m_rareData->m_value->ref();
        return;
    }
    if (m_data.m_value)
        m_data.m_value->deref();
    m_data.m_value = value.impl();
    m_data.m_value->ref();
}

inline CSSSelector::CSSSelector()
    : m_relation(Descendant)
    , m_match(Unknown)
    , m_pseudoType(0)
    , m_parsedNth(false)
    , m_isLastInSelectorList(false)
    , m_isLastInTagHistory(true)
    , m_hasRareData(false)
    , m_hasNameWithCase(false)
    , m_isForPage(false)
    , m_tagIsForNamespaceRule(false)
#if ENABLE(CSS_SELECTORS_LEVEL4)
    , m_descendantDoubleChildSyntax(false)
#endif
    , m_caseInsensitiveAttributeValueMatching(false)
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    , m_destructorHasBeenCalled(false)
#endif
{
}

inline CSSSelector::CSSSelector(const CSSSelector& o)
    : m_relation(o.m_relation)
    , m_match(o.m_match)
    , m_pseudoType(o.m_pseudoType)
    , m_parsedNth(o.m_parsedNth)
    , m_isLastInSelectorList(o.m_isLastInSelectorList)
    , m_isLastInTagHistory(o.m_isLastInTagHistory)
    , m_hasRareData(o.m_hasRareData)
    , m_hasNameWithCase(o.m_hasNameWithCase)
    , m_isForPage(o.m_isForPage)
    , m_tagIsForNamespaceRule(o.m_tagIsForNamespaceRule)
#if ENABLE(CSS_SELECTORS_LEVEL4)
    , m_descendantDoubleChildSyntax(o.m_descendantDoubleChildSyntax)
#endif
    , m_caseInsensitiveAttributeValueMatching(o.m_caseInsensitiveAttributeValueMatching)
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    , m_destructorHasBeenCalled(false)
#endif
{
    if (o.m_hasRareData) {
        m_data.m_rareData = o.m_data.m_rareData;
        m_data.m_rareData->ref();
    } else if (o.m_hasNameWithCase) {
        m_data.m_nameWithCase = o.m_data.m_nameWithCase;
        m_data.m_nameWithCase->ref();
    } if (o.match() == Tag) {
        m_data.m_tagQName = o.m_data.m_tagQName;
        m_data.m_tagQName->ref();
    } else if (o.m_data.m_value) {
        m_data.m_value = o.m_data.m_value;
        m_data.m_value->ref();
    }
}

inline CSSSelector::~CSSSelector()
{
    ASSERT_WITH_SECURITY_IMPLICATION(!m_destructorHasBeenCalled);
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    m_destructorHasBeenCalled = true;
#endif
    if (m_hasRareData) {
        m_data.m_rareData->deref();
        m_data.m_rareData = nullptr;
        m_hasRareData = false;
    } else if (m_hasNameWithCase) {
        m_data.m_nameWithCase->deref();
        m_data.m_nameWithCase = nullptr;
        m_hasNameWithCase = false;
    } else if (match() == Tag) {
        m_data.m_tagQName->deref();
        m_data.m_tagQName = nullptr;
        m_match = Unknown;
    } else if (m_data.m_value) {
        m_data.m_value->deref();
        m_data.m_value = nullptr;
    }
}

inline const QualifiedName& CSSSelector::tagQName() const
{
    ASSERT(match() == Tag);
    if (m_hasNameWithCase)
        return m_data.m_nameWithCase->m_originalName;
    return *reinterpret_cast<const QualifiedName*>(&m_data.m_tagQName);
}

inline const AtomicString& CSSSelector::tagLowercaseLocalName() const
{
    if (m_hasNameWithCase)
        return m_data.m_nameWithCase->m_lowercaseLocalName;
    return m_data.m_tagQName->m_localName;
}

inline const AtomicString& CSSSelector::value() const
{
    ASSERT(match() != Tag);
    // AtomicString is really just an AtomicStringImpl* so the cast below is safe.
    // FIXME: Perhaps call sites could be changed to accept AtomicStringImpl?
    return *reinterpret_cast<const AtomicString*>(m_hasRareData ? &m_data.m_rareData->m_value : &m_data.m_value);
}

inline void CSSSelector::setAttributeValueMatchingIsCaseInsensitive(bool isCaseInsensitive)
{
    ASSERT(isAttributeSelector() && match() != CSSSelector::Set);
    m_caseInsensitiveAttributeValueMatching = isCaseInsensitive;
}

inline bool CSSSelector::attributeValueMatchingIsCaseInsensitive() const
{
    return m_caseInsensitiveAttributeValueMatching;
}

} // namespace WebCore

#endif // CSSSelector_h
