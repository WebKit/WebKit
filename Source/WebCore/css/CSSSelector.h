/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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
#include <wtf/FixedVector.h>

namespace WebCore {

class CSSSelectorList;

struct PossiblyQuotedIdentifier {
    AtomString identifier;
    bool wasQuoted { false };

    bool isNull() const { return identifier.isNull(); }
};

    enum class SelectorSpecificityIncrement {
        ClassA = 0x10000,
        ClassB = 0x100,
        ClassC = 1
    };

    // Selector for a StyleRule.
    DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSSelectorRareData);
    class CSSSelector {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        CSSSelector() = default;
        CSSSelector(const CSSSelector&);
        explicit CSSSelector(const QualifiedName&, bool tagIsForNamespaceRule = false);

        ~CSSSelector();

        // Re-create selector text from selector's data.
        String selectorText(StringView separator = { }, StringView rightSide = { }) const;

        // Check if the 2 selectors (including sub selectors) agree.
        bool operator==(const CSSSelector&) const;

        unsigned computeSpecificity() const;
        std::array<uint8_t, 3> computeSpecificityTuple() const;
        unsigned specificityForPage() const;

        // How the attribute value has to match. Default is Exact.
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
            ShadowDescendant,
            ShadowPartDescendant,
            ShadowSlotted
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
            PseudoClassAutofillAndObscured,
            PseudoClassAutofillStrongPassword,
            PseudoClassAutofillStrongPasswordViewable,
            PseudoClassHover,
            PseudoClassDrag,
            PseudoClassFocus,
            PseudoClassFocusVisible,
            PseudoClassFocusWithin,
            PseudoClassActive,
            PseudoClassChecked,
            PseudoClassEnabled,
            PseudoClassFullPageMedia,
            PseudoClassDefault,
            PseudoClassDisabled,
            PseudoClassIs,
            PseudoClassMatches, // obsolete synonym for PseudoClassIs
            PseudoClassWhere,
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
            PseudoClassRelativeScope, // Like :scope but for internal use with relative selectors like :has(> foo).
            PseudoClassWindowInactive,
            PseudoClassCornerPresent,
            PseudoClassDecrement,
            PseudoClassIncrement,
            PseudoClassHas,
            PseudoClassHorizontal,
            PseudoClassVertical,
            PseudoClassStart,
            PseudoClassEnd,
            PseudoClassDoubleButton,
            PseudoClassSingleButton,
            PseudoClassNoButton,
#if ENABLE(FULLSCREEN_API)
            PseudoClassFullscreen,
            PseudoClassWebkitFullScreen,
            PseudoClassFullScreenDocument,
            PseudoClassFullScreenAncestor,
            PseudoClassAnimatingFullScreenTransition,
            PseudoClassFullScreenControlsHidden,
#endif
#if ENABLE(PICTURE_IN_PICTURE_API)
            PseudoClassPictureInPicture,
#endif
            PseudoClassInRange,
            PseudoClassOutOfRange,
#if ENABLE(VIDEO)
            PseudoClassFuture,
            PseudoClassPast,
            PseudoClassPlaying,
            PseudoClassPaused,
            PseudoClassSeeking,
            PseudoClassBuffering,
            PseudoClassStalled,
            PseudoClassMuted,
            PseudoClassVolumeLocked,
#endif
            PseudoClassDir,
            PseudoClassHost,
            PseudoClassDefined,
#if ENABLE(ATTACHMENT_ELEMENT)
            PseudoClassHasAttachment,
#endif
            PseudoClassModal,
            PseudoClassUserInvalid,
            PseudoClassUserValid,
        };

        enum PseudoElementType {
            PseudoElementUnknown = 0,
            PseudoElementAfter,
            PseudoElementBackdrop,
            PseudoElementBefore,
#if ENABLE(VIDEO)
            PseudoElementCue,
#endif
            PseudoElementFirstLetter,
            PseudoElementFirstLine,
            PseudoElementHighlight,
            PseudoElementMarker,
            PseudoElementPart,
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

        static PseudoElementType parsePseudoElementType(StringView);
        static PseudoId pseudoId(PseudoElementType);

        // Selectors are kept in an array by CSSSelectorList.
        // The next component of the selector is the next item in the array.
        const CSSSelector* tagHistory() const { return m_isLastInTagHistory ? nullptr : this + 1; }
        const CSSSelector* firstInCompound() const;

        const QualifiedName& tagQName() const;
        const AtomString& tagLowercaseLocalName() const;

        const AtomString& value() const;
        const AtomString& serializingValue() const;
        const QualifiedName& attribute() const;
        const AtomString& attributeCanonicalLocalName() const;
        const AtomString& argument() const { return m_hasRareData ? m_data.rareData->argument : nullAtom(); }
        bool attributeValueMatchingIsCaseInsensitive() const;
        const FixedVector<PossiblyQuotedIdentifier>* argumentList() const { return m_hasRareData ? &m_data.rareData->argumentList : nullptr; }
        const CSSSelectorList* selectorList() const { return m_hasRareData ? m_data.rareData->selectorList.get() : nullptr; }

        void setValue(const AtomString&, bool matchLowerCase = false);

        enum AttributeMatchType { CaseSensitive, CaseInsensitive };
        void setAttribute(const QualifiedName&, bool convertToLowercase, AttributeMatchType);
        void setNth(int a, int b);
        void setArgument(const AtomString&);
        void setArgumentList(FixedVector<PossiblyQuotedIdentifier>);
        void setSelectorList(std::unique_ptr<CSSSelectorList>);

        bool matchNth(int count) const;
        int nthA() const;
        int nthB() const;

        bool hasDescendantRelation() const { return relation() == DescendantSpace; }

        bool hasDescendantOrChildRelation() const { return relation() == Child || hasDescendantRelation(); }

        PseudoClassType pseudoClassType() const;
        void setPseudoClassType(PseudoClassType);

        PseudoElementType pseudoElementType() const;
        void setPseudoElementType(PseudoElementType);

        PagePseudoClassType pagePseudoClassType() const;
        void setPagePseudoType(PagePseudoClassType);

        bool matchesPseudoElement() const;
        bool isUnknownPseudoElement() const;
        bool isCustomPseudoElement() const;
        bool isWebKitCustomPseudoElement() const;
        bool isSiblingSelector() const;
        bool isAttributeSelector() const;

        RelationType relation() const { return static_cast<RelationType>(m_relation); }
        void setRelation(RelationType);

        Match match() const { return static_cast<Match>(m_match); }
        void setMatch(Match);

        bool isLastInSelectorList() const { return m_isLastInSelectorList; }
        void setLastInSelectorList() { m_isLastInSelectorList = true; }
        bool isFirstInTagHistory() const { return m_isFirstInTagHistory; }
        bool isLastInTagHistory() const { return m_isLastInTagHistory; }
        void setNotFirstInTagHistory() { m_isFirstInTagHistory = false; }
        void setNotLastInTagHistory() { m_isLastInTagHistory = false; }

        bool isForPage() const { return m_isForPage; }
        void setForPage() { m_isForPage = true; }

    private:
        unsigned m_relation : 4 { DescendantSpace }; // enum RelationType.
        mutable unsigned m_match : 4 { Unknown }; // enum Match.
        mutable unsigned m_pseudoType : 8 { 0 }; // PseudoType.
        unsigned m_isLastInSelectorList : 1 { false };
        unsigned m_isFirstInTagHistory : 1 { true };
        unsigned m_isLastInTagHistory : 1 { true };
        unsigned m_hasRareData : 1 { false };
        unsigned m_hasNameWithCase : 1 { false };
        unsigned m_isForPage : 1 { false };
        unsigned m_tagIsForNamespaceRule : 1 { false };
        unsigned m_caseInsensitiveAttributeValueMatching : 1 { false };
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
        unsigned m_destructorHasBeenCalled : 1 { false };
#endif

        unsigned simpleSelectorSpecificityForPage() const;

        CSSSelector& operator=(const CSSSelector&) = delete;

        struct RareData : public RefCounted<RareData> {
            WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSSelectorRareData);
            static Ref<RareData> create(AtomString);
            ~RareData();

            bool matchNth(int count);

            // For quirks mode, class and id are case-insensitive. In the case where uppercase
            // letters are used in quirks mode, |m_matchingValue| holds the lowercase class/id
            // and |m_serializingValue| holds the original string.
            AtomString matchingValue;
            AtomString serializingValue;

            int a { 0 }; // Used for :nth-*
            int b { 0 }; // Used for :nth-*
            QualifiedName attribute; // used for attribute selector
            AtomString attributeCanonicalLocalName;
            AtomString argument; // Used for :contains and :nth-*
            FixedVector<PossiblyQuotedIdentifier> argumentList; // Used for :lang and ::part arguments.
            std::unique_ptr<CSSSelectorList> selectorList; // Used for :is(), :matches(), and :not().

        private:
            RareData(AtomString&& value);
        };
        void createRareData();

        struct NameWithCase : public RefCounted<NameWithCase> {
            NameWithCase(const QualifiedName& originalName, const AtomString& lowercaseName)
                : originalName(originalName)
                , lowercaseLocalName(lowercaseName)
            {
                ASSERT(originalName.localName() != lowercaseName);
            }

            const QualifiedName originalName;
            const AtomString lowercaseLocalName;
        };

        union DataUnion {
            AtomStringImpl* value { nullptr };
            QualifiedName::QualifiedNameImpl* tagQName;
            RareData* rareData;
            NameWithCase* nameWithCase;
        } m_data;
    };

inline bool operator==(const AtomString& a, const PossiblyQuotedIdentifier& b) { return a == b.identifier; }
inline bool operator==(const PossiblyQuotedIdentifier& a, const AtomString& b) { return a.identifier == b; }
inline bool operator!=(const AtomString& a, const PossiblyQuotedIdentifier& b) { return a != b.identifier; }
inline bool operator!=(const PossiblyQuotedIdentifier& a, const AtomString& b) { return a.identifier != b; }

inline const QualifiedName& CSSSelector::attribute() const
{
    ASSERT(isAttributeSelector());
    ASSERT(m_hasRareData);
    return m_data.rareData->attribute;
}

inline const AtomString& CSSSelector::attributeCanonicalLocalName() const
{
    ASSERT(isAttributeSelector());
    ASSERT(m_hasRareData);
    return m_data.rareData->attributeCanonicalLocalName;
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

static inline bool isTreeStructuralPseudoClass(CSSSelector::PseudoClassType type)
{
    return pseudoClassIsRelativeToSiblings(type) || type == CSSSelector::PseudoClassRoot;
}

inline bool isLogicalCombinationPseudoClass(CSSSelector::PseudoClassType pseudoClassType)
{
    switch (pseudoClassType) {
    case CSSSelector::PseudoClassIs:
    case CSSSelector::PseudoClassWhere:
    case CSSSelector::PseudoClassNot:
    case CSSSelector::PseudoClassAny:
    case CSSSelector::PseudoClassMatches:
    case CSSSelector::PseudoClassHas:
        return true;
    default:
        return false;
    }
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
        || match() == CSSSelector::Set
        || match() == CSSSelector::List
        || match() == CSSSelector::Hyphen
        || match() == CSSSelector::Contain
        || match() == CSSSelector::Begin
        || match() == CSSSelector::End;
}

inline void CSSSelector::setValue(const AtomString& value, bool matchLowerCase)
{
    ASSERT(match() != Tag);
    AtomString matchingValue = matchLowerCase ? value.convertToASCIILowercase() : value;
    if (!m_hasRareData && matchingValue != value)
        createRareData();
    
    // Need to do ref counting manually for the union.
    if (!m_hasRareData) {
        if (m_data.value)
            m_data.value->deref();
        m_data.value = value.impl();
        m_data.value->ref();
        return;
    }

    m_data.rareData->matchingValue = WTFMove(matchingValue);
    m_data.rareData->serializingValue = value;
}

inline CSSSelector::CSSSelector(const CSSSelector& o)
    : m_relation(o.m_relation)
    , m_match(o.m_match)
    , m_pseudoType(o.m_pseudoType)
    , m_isLastInSelectorList(o.m_isLastInSelectorList)
    , m_isFirstInTagHistory(o.m_isFirstInTagHistory)
    , m_isLastInTagHistory(o.m_isLastInTagHistory)
    , m_hasRareData(o.m_hasRareData)
    , m_hasNameWithCase(o.m_hasNameWithCase)
    , m_isForPage(o.m_isForPage)
    , m_tagIsForNamespaceRule(o.m_tagIsForNamespaceRule)
    , m_caseInsensitiveAttributeValueMatching(o.m_caseInsensitiveAttributeValueMatching)
{
    if (o.m_hasRareData) {
        m_data.rareData = o.m_data.rareData;
        m_data.rareData->ref();
    } else if (o.m_hasNameWithCase) {
        m_data.nameWithCase = o.m_data.nameWithCase;
        m_data.nameWithCase->ref();
    } if (o.match() == Tag) {
        m_data.tagQName = o.m_data.tagQName;
        m_data.tagQName->ref();
    } else if (o.m_data.value) {
        m_data.value = o.m_data.value;
        m_data.value->ref();
    }
}

inline CSSSelector::~CSSSelector()
{
    ASSERT_WITH_SECURITY_IMPLICATION(!m_destructorHasBeenCalled);
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    m_destructorHasBeenCalled = true;
#endif
    if (m_hasRareData) {
        m_data.rareData->deref();
        m_data.rareData = nullptr;
        m_hasRareData = false;
    } else if (m_hasNameWithCase) {
        m_data.nameWithCase->deref();
        m_data.nameWithCase = nullptr;
        m_hasNameWithCase = false;
    } else if (match() == Tag) {
        m_data.tagQName->deref();
        m_data.tagQName = nullptr;
        m_match = Unknown;
    } else if (m_data.value) {
        m_data.value->deref();
        m_data.value = nullptr;
    }
}

inline const QualifiedName& CSSSelector::tagQName() const
{
    ASSERT(match() == Tag);
    if (m_hasNameWithCase)
        return m_data.nameWithCase->originalName;
    return *reinterpret_cast<const QualifiedName*>(&m_data.tagQName);
}

inline const AtomString& CSSSelector::tagLowercaseLocalName() const
{
    if (m_hasNameWithCase)
        return m_data.nameWithCase->lowercaseLocalName;
    return m_data.tagQName->m_localName;
}

inline const AtomString& CSSSelector::value() const
{
    ASSERT(match() != Tag);
    if (m_hasRareData)
        return m_data.rareData->matchingValue;

    // AtomString is really just an AtomStringImpl* so the cast below is safe.
    return *reinterpret_cast<const AtomString*>(&m_data.value);
}

inline const AtomString& CSSSelector::serializingValue() const
{
    ASSERT(match() != Tag);
    if (m_hasRareData)
        return m_data.rareData->serializingValue;
    
    // AtomString is really just an AtomStringImpl* so the cast below is safe.
    return *reinterpret_cast<const AtomString*>(&m_data.value);
}

inline bool CSSSelector::attributeValueMatchingIsCaseInsensitive() const
{
    return m_caseInsensitiveAttributeValueMatching;
}

inline auto CSSSelector::pseudoClassType() const -> PseudoClassType
{
    ASSERT(match() == PseudoClass);
    return static_cast<PseudoClassType>(m_pseudoType);
}

inline void CSSSelector::setPseudoClassType(PseudoClassType pseudoType)
{
    m_pseudoType = pseudoType;
    ASSERT(m_pseudoType == pseudoType);
}

inline auto CSSSelector::pseudoElementType() const -> PseudoElementType
{
    ASSERT(match() == PseudoElement);
    return static_cast<PseudoElementType>(m_pseudoType);
}

inline void CSSSelector::setPseudoElementType(PseudoElementType pseudoElementType)
{
    m_pseudoType = pseudoElementType;
    ASSERT(m_pseudoType == pseudoElementType);
}

inline auto CSSSelector::pagePseudoClassType() const -> PagePseudoClassType
{
    ASSERT(match() == PagePseudoClass);
    return static_cast<PagePseudoClassType>(m_pseudoType);
}

inline void CSSSelector::setPagePseudoType(PagePseudoClassType pagePseudoType)
{
    m_pseudoType = pagePseudoType;
    ASSERT(m_pseudoType == pagePseudoType);
}

inline void CSSSelector::setRelation(RelationType relation)
{
    m_relation = relation;
    ASSERT(m_relation == relation);
}

inline void CSSSelector::setMatch(Match match)
{
    m_match = match;
    ASSERT(m_match == match);
}

} // namespace WebCore
