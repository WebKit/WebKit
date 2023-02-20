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
#include <wtf/CompactPointerTuple.h>
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
        CSSSelector();
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

        void visitAllSimpleSelectors(auto& apply) const;

        bool hasExplicitNestingParent() const;
        void resolveNestingParentSelectors(const CSSSelectorList& parent);
        void replaceNestingParentByNotAll();

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
            PseudoClassOpen,
            PseudoClassClosed,
            PseudoClassUserInvalid,
            PseudoClassUserValid,
            PseudoClassNestingParent,
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
        const CSSSelector* tagHistory() const { return hasFlag(Flag::IsLastInTagHistory) ? nullptr : this + 1; }
        const CSSSelector* firstInCompound() const;

        QualifiedName tagQName() const;
        const AtomString& tagLowercaseLocalName() const;

        AtomString value() const;
        AtomStringImpl* valueImpl() const;
        AtomString serializingValue() const;
        const QualifiedName& attribute() const;
        const AtomString& attributeCanonicalLocalName() const;
        const AtomString& argument() const { return hasFlag(Flag::HasRareData) ? rareDataPointer()->argument : nullAtom(); }
        bool attributeValueMatchingIsCaseInsensitive() const { return hasFlag(Flag::CaseInsensitiveAttributeValueMatching); }
        const FixedVector<PossiblyQuotedIdentifier>* argumentList() const { return hasFlag(Flag::HasRareData) ? &rareDataPointer()->argumentList : nullptr; }
        const CSSSelectorList* selectorList() const { return hasFlag(Flag::HasRareData) ? rareDataPointer()->selectorList.get() : nullptr; }
        CSSSelectorList* selectorList() { return hasFlag(Flag::HasRareData) ? rareDataPointer()->selectorList.get() : nullptr; }

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

        RelationType relation() const;
        void setRelation(RelationType);

        Match match() const;
        void setMatch(Match);

        bool isLastInSelectorList() const { return hasFlag(Flag::IsLastInSelectorList); }
        bool isFirstInTagHistory() const { return hasFlag(Flag::IsFirstInTagHistory); }
        bool isLastInTagHistory() const { return hasFlag(Flag::IsLastInTagHistory); }
        bool isForPage() const { return hasFlag(Flag::IsForPage); }

        void setIsLastInSelectorList(bool value = true) { setFlag(Flag::IsLastInSelectorList, value); }
        void setIsFirstInTagHistory(bool value = true) { setFlag(Flag::IsFirstInTagHistory, value); }
        void setIsLastInTagHistory(bool value = true) { setFlag(Flag::IsLastInTagHistory, value); }
        void setIsForPage(bool value = true) { setFlag(Flag::IsForPage, value); }

    private:
        bool hasRareData() const { return hasFlag(Flag::HasRareData); }
        bool hasNameWithCase() const { return hasFlag(Flag::HasNameWithCase); }
        bool tagIsForNamespaceRule() const { return hasFlag(Flag::TagIsForNamespaceRule); }
        bool caseInsensitiveAttributeValueMatching() const { return hasFlag(Flag::CaseInsensitiveAttributeValueMatching); }

        void setHasRareData(bool value = true) { setFlag(Flag::HasRareData, value); }
        void setHasNameWithCase(bool value = true) { setFlag(Flag::HasNameWithCase, value); }
        void setTagIsForNamespaceRule(bool value = true) { setFlag(Flag::TagIsForNamespaceRule, value); }
        void setCaseInsensitiveAttributeValueMatching(bool value = true) { setFlag(Flag::CaseInsensitiveAttributeValueMatching, value); }

        static constexpr uint16_t s_relationMask = 0x0F; // enum RelationType.
        static constexpr uint16_t s_matchMask = 0xF0; // enum Match.
        static constexpr unsigned s_relationShift = 0;
        static constexpr unsigned s_matchShift = 4;

        enum class Flag : uint16_t {
            IsLastInSelectorList = 1 << 8,
            IsFirstInTagHistory = 1 << 9,
            IsLastInTagHistory = 1 << 10,
            HasRareData = 1 << 11,
            HasNameWithCase = 1 << 12,
            IsForPage = 1 << 13,
            TagIsForNamespaceRule = 1 << 14,
            CaseInsensitiveAttributeValueMatching = 1 << 15,
        };

        bool hasFlag(Flag) const;
        void setFlag(Flag, bool value = true);

        struct RareData : public RefCounted<RareData> {
            WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSSelectorRareData);
            static Ref<RareData> create(AtomString);
            WEBCORE_EXPORT ~RareData();

            bool matchNth(int count);

            unsigned pseudoType : 8 { 0 }; // PseudoType.

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

            Ref<RareData> deepCopy() const;

        private:
            RareData(AtomString&& value);
            RareData(const RareData& other);
        };

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

        AtomStringImpl* valuePointer() const { return static_cast<AtomStringImpl*>(m_dataWithFlags.pointer()); }
        QualifiedName::QualifiedNameImpl* tagQNamePointer() const { return static_cast<QualifiedName::QualifiedNameImpl*>(m_dataWithFlags.pointer()); }
        RareData* rareDataPointer() const { return static_cast<RareData*>(m_dataWithFlags.pointer()); }
        NameWithCase* nameWithCasePointer() const { return static_cast<NameWithCase*>(m_dataWithFlags.pointer()); }

        void setValuePointer(AtomStringImpl* pointer) { m_dataWithFlags.setPointer(pointer); }
        void setTagQNamePointer(QualifiedName::QualifiedNameImpl* pointer) { m_dataWithFlags.setPointer(pointer); }
        void setRareDataPointer(RareData* pointer) { m_dataWithFlags.setPointer(pointer); }
        void setNameWithCasePointer(NameWithCase* pointer) { m_dataWithFlags.setPointer(pointer); }

        void createRareData();

        unsigned simpleSelectorSpecificityForPage() const;
        CSSSelector* tagHistory() { return isLastInTagHistory() ? nullptr : this + 1; }

        unsigned pseudoType() const { return hasRareData() ? rareDataPointer()->pseudoType : 0; }

        CSSSelector& operator=(const CSSSelector&) = delete;
        CSSSelector(CSSSelector&&) = delete;

        CompactPointerTuple<void*, uint16_t> m_dataWithFlags; // The uint16_t stores the RelationType, Match, and Flag values.

#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
        unsigned m_destructorHasBeenCalled : 1 { false };
#endif
    };

inline bool operator==(const AtomString& a, const PossiblyQuotedIdentifier& b) { return a == b.identifier; }
inline bool operator==(const PossiblyQuotedIdentifier& a, const AtomString& b) { return a.identifier == b; }
inline bool operator!=(const AtomString& a, const PossiblyQuotedIdentifier& b) { return a != b.identifier; }
inline bool operator!=(const PossiblyQuotedIdentifier& a, const AtomString& b) { return a.identifier != b; }

inline const QualifiedName& CSSSelector::attribute() const
{
    ASSERT(isAttributeSelector());
    ASSERT(hasRareData());
    return rareDataPointer()->attribute;
}

inline const AtomString& CSSSelector::attributeCanonicalLocalName() const
{
    ASSERT(isAttributeSelector());
    ASSERT(hasRareData());
    return rareDataPointer()->attributeCanonicalLocalName;
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
    if (!hasRareData() && matchingValue != value)
        createRareData();
    
    // Need to do ref counting manually for the union.
    if (!hasRareData()) {
        if (valuePointer())
            valuePointer()->deref();
        setValuePointer(value.impl());
        valuePointer()->ref();
        return;
    }

    rareDataPointer()->matchingValue = WTFMove(matchingValue);
    rareDataPointer()->serializingValue = value;
}

inline CSSSelector::~CSSSelector()
{
    ASSERT_WITH_SECURITY_IMPLICATION(!m_destructorHasBeenCalled);
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    m_destructorHasBeenCalled = true;
#endif
    if (hasRareData()) {
        rareDataPointer()->deref();
        setRareDataPointer(nullptr);
        setHasRareData(false);
    } else if (hasNameWithCase()) {
        nameWithCasePointer()->deref();
        setNameWithCasePointer(nullptr);
        setHasNameWithCase(false);
    } else if (match() == Tag) {
        tagQNamePointer()->deref();
        setTagQNamePointer(nullptr);
        setMatch(Unknown);
    } else if (valuePointer()) {
        valuePointer()->deref();
        setValuePointer(nullptr);
    }
}

inline QualifiedName CSSSelector::tagQName() const
{
    ASSERT(match() == Tag);
    if (hasNameWithCase())
        return { *nameWithCasePointer()->originalName.impl() };
    return { *tagQNamePointer() };
}

inline const AtomString& CSSSelector::tagLowercaseLocalName() const
{
    if (hasNameWithCase())
        return nameWithCasePointer()->lowercaseLocalName;
    return tagQNamePointer()->m_localName;
}

inline AtomStringImpl* CSSSelector::valueImpl() const
{
    ASSERT(match() != Tag);
    if (hasRareData())
        return rareDataPointer()->matchingValue.impl();
    return valuePointer();
}

inline AtomString CSSSelector::value() const
{
    return { valueImpl() };
}

inline AtomString CSSSelector::serializingValue() const
{
    ASSERT(match() != Tag);
    if (hasRareData())
        return { rareDataPointer()->serializingValue.impl() };
    return { valuePointer() };
}

inline auto CSSSelector::pseudoClassType() const -> PseudoClassType
{
    ASSERT(match() == PseudoClass);
    return static_cast<PseudoClassType>(pseudoType());
}

inline void CSSSelector::setPseudoClassType(PseudoClassType pseudoType)
{
    createRareData();
    rareDataPointer()->pseudoType = pseudoType;
    ASSERT(rareDataPointer()->pseudoType == pseudoType);
}

inline auto CSSSelector::pseudoElementType() const -> PseudoElementType
{
    ASSERT(match() == PseudoElement);
    return static_cast<PseudoElementType>(pseudoType());
}

inline void CSSSelector::setPseudoElementType(PseudoElementType pseudoElementType)
{
    createRareData();
    rareDataPointer()->pseudoType = pseudoElementType;
    ASSERT(rareDataPointer()->pseudoType == pseudoElementType);
}

inline auto CSSSelector::pagePseudoClassType() const -> PagePseudoClassType
{
    ASSERT(match() == PagePseudoClass);
    return static_cast<PagePseudoClassType>(pseudoType());
}

inline void CSSSelector::setPagePseudoType(PagePseudoClassType pagePseudoType)
{
    createRareData();
    rareDataPointer()->pseudoType = pagePseudoType;
    ASSERT(rareDataPointer()->pseudoType == pagePseudoType);
}

inline auto CSSSelector::relation() const -> RelationType
{
    return static_cast<RelationType>((m_dataWithFlags.type() & s_relationMask) >> s_relationShift);
}

inline void CSSSelector::setRelation(RelationType relation)
{
    m_dataWithFlags.setType((m_dataWithFlags.type() & ~s_relationMask) | (static_cast<uint16_t>(relation) << s_relationShift));
}

inline auto CSSSelector::match() const -> Match
{
    return static_cast<Match>((m_dataWithFlags.type() & s_matchMask) >> s_matchShift);
}

inline void CSSSelector::setMatch(Match match)
{
    m_dataWithFlags.setType((m_dataWithFlags.type() & ~s_matchMask) | (static_cast<uint16_t>(match) << s_matchShift));
}

inline bool CSSSelector::hasFlag(Flag flag) const
{
    return m_dataWithFlags.type() & static_cast<uint16_t>(flag);
}

inline void CSSSelector::setFlag(Flag flag, bool value)
{
    if (value)
        m_dataWithFlags.setType(m_dataWithFlags.type() | static_cast<uint16_t>(flag));
    else
        m_dataWithFlags.setType(m_dataWithFlags.type() & ~static_cast<uint16_t>(flag));
}

} // namespace WebCore
