/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "CSSSelector.h"

#include "CSSMarkup.h"
#include "CSSParserSelector.h"
#include "CSSSelectorList.h"
#include "CSSSelectorParserContext.h"
#include "CommonAtomStrings.h"
#include "HTMLNames.h"
#include "SelectorPseudoTypeMap.h"
#include <memory>
#include <queue>
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

using namespace HTMLNames;

struct SameSizeAsCSSSelector {
    unsigned flags;
    void* unionPointer;
};

static_assert(CSSSelector::Relation::Subselector == static_cast<CSSSelector::Relation>(0u), "Subselector must be 0 for consumeCombinator.");
static_assert(sizeof(CSSSelector) == sizeof(SameSizeAsCSSSelector), "CSSSelector should remain small.");

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSSelectorRareData);

CSSSelector::CSSSelector(const QualifiedName& tagQName, bool tagIsForNamespaceRule)
    : m_relation(enumToUnderlyingType(Relation::DescendantSpace))
    , m_match(enumToUnderlyingType(Match::Tag))
    , m_tagIsForNamespaceRule(tagIsForNamespaceRule)
{
    m_data.tagQName = tagQName.impl();
    m_data.tagQName->ref();
}

void CSSSelector::createRareData()
{
    ASSERT(match() != Match::Tag);
    if (m_hasRareData)
        return;
    // Move the value to the rare data stucture.
    m_data.rareData = &RareData::create(adoptRef(m_data.value)).leakRef();
    m_hasRareData = true;
}

struct SelectorSpecificity {
    unsigned specificity { 0 };

    SelectorSpecificity() = default;
    SelectorSpecificity(unsigned);
    SelectorSpecificity(SelectorSpecificityIncrement);
    SelectorSpecificity& operator+=(SelectorSpecificity);

    std::array<uint8_t, 3> specificityTuple() const
    {
        uint8_t a = specificity >> 16;
        uint8_t b = specificity >> 8;
        uint8_t c = specificity;
        return { a, b, c };
    }

    String debugDescription() const
    {
        StringBuilder builder;
        auto tuple = specificityTuple();
        builder.append('{', tuple[0], ' ', tuple[1], ' ', tuple[2], '}');
        return builder.toString();
    }
};

UNUSED_FUNCTION static TextStream& operator<<(TextStream& ts, const SelectorSpecificity& selectorSpecificity)
{
    ts << selectorSpecificity.debugDescription();
    return ts;
}

SelectorSpecificity::SelectorSpecificity(unsigned specificity)
    : specificity(specificity)
{
}

SelectorSpecificity::SelectorSpecificity(SelectorSpecificityIncrement specificity)
    : specificity(enumToUnderlyingType(specificity))
{
}

SelectorSpecificity& SelectorSpecificity::operator+=(SelectorSpecificity other)
{
    auto addSaturating = [&](unsigned mask) {
        unsigned otherValue = (other.specificity & mask);
        if (((specificity & mask) + otherValue) & ~mask)
            specificity |= mask;
        else
            specificity += otherValue;
    };
    addSaturating(0xFF0000);
    addSaturating(0xFF00);
    addSaturating(0xFF);
    return *this;
}

static SelectorSpecificity operator+(SelectorSpecificity a, SelectorSpecificity b)
{
    return a += b;
}

static SelectorSpecificity simpleSelectorSpecificity(const CSSSelector&);

static SelectorSpecificity selectorSpecificity(const CSSSelector& firstSimpleSelector)
{
    SelectorSpecificity total;
    for (const auto* selector = &firstSimpleSelector; selector; selector = selector->tagHistory())
        total += simpleSelectorSpecificity(*selector);
    return total;
}

static SelectorSpecificity maxSpecificity(const CSSSelectorList* selectorList)
{
    unsigned max = 0;
    if (selectorList) {
        for (const auto* selector = selectorList->first(); selector; selector = CSSSelectorList::next(selector))
            max = std::max(max, selectorSpecificity(*selector).specificity);
    }
    return max;
}

SelectorSpecificity simpleSelectorSpecificity(const CSSSelector& simpleSelector)
{
    ASSERT_WITH_MESSAGE(!simpleSelector.isForPage(), "At the time of this writing, page selectors are not treated as real selectors that are matched. The value computed here only account for real selectors.");

    if (UNLIKELY(simpleSelector.isImplicit()))
        return 0;

    switch (simpleSelector.match()) {
    case CSSSelector::Match::Id:
        return SelectorSpecificityIncrement::ClassA;
    case CSSSelector::Match::NestingParent:
    case CSSSelector::Match::PagePseudoClass:
        break;
    case CSSSelector::Match::PseudoClass:
        switch (simpleSelector.pseudoClass()) {
        case CSSSelector::PseudoClass::Is:
        case CSSSelector::PseudoClass::Matches:
        case CSSSelector::PseudoClass::Not:
        case CSSSelector::PseudoClass::Has:
            return maxSpecificity(simpleSelector.selectorList());
        case CSSSelector::PseudoClass::Where:
            return 0;
        case CSSSelector::PseudoClass::NthChild:
        case CSSSelector::PseudoClass::NthLastChild:
        case CSSSelector::PseudoClass::Host:
            return SelectorSpecificityIncrement::ClassB + maxSpecificity(simpleSelector.selectorList());
        case CSSSelector::PseudoClass::HasScope:
            return 0;
        default:
            return SelectorSpecificityIncrement::ClassB;
        }
    case CSSSelector::Match::Exact:
    case CSSSelector::Match::Class:
    case CSSSelector::Match::Set:
    case CSSSelector::Match::List:
    case CSSSelector::Match::Hyphen:
    case CSSSelector::Match::Contain:
    case CSSSelector::Match::Begin:
    case CSSSelector::Match::End:
        return SelectorSpecificityIncrement::ClassB;
    case CSSSelector::Match::Tag:
        if (simpleSelector.tagQName().localName() == starAtom())
            return 0;
        return SelectorSpecificityIncrement::ClassC;
    case CSSSelector::Match::PseudoElement:
        // Slotted only competes with other slotted selectors for specificity,
        // so whether we add the ClassC specificity shouldn't be observable.
        if (simpleSelector.pseudoElement() == CSSSelector::PseudoElement::Slotted)
            return maxSpecificity(simpleSelector.selectorList());
        return SelectorSpecificityIncrement::ClassC;
    case CSSSelector::Match::Unknown:
    case CSSSelector::Match::ForgivingUnknown:
    case CSSSelector::Match::ForgivingUnknownNestContaining:
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned CSSSelector::computeSpecificity() const
{
    return selectorSpecificity(*this).specificity;
}

std::array<uint8_t, 3> CSSSelector::computeSpecificityTuple() const
{
    return selectorSpecificity(*this).specificityTuple();
}

unsigned CSSSelector::specificityForPage() const
{
    ASSERT(isForPage());

    // See http://dev.w3.org/csswg/css3-page/#cascading-and-page-context
    unsigned s = 0;

    for (const CSSSelector* component = this; component; component = component->tagHistory()) {
        switch (component->match()) {
        case Match::Tag:
            s += tagQName().localName() == starAtom() ? 0 : 4;
            break;
        case Match::PagePseudoClass:
            switch (component->pagePseudoClass()) {
            case PagePseudoClass::First:
                s += 2;
                break;
            case PagePseudoClass::Left:
            case PagePseudoClass::Right:
                s += 1;
                break;
            }
            break;
        default:
            break;
        }
    }
    return s;
}

PseudoId CSSSelector::pseudoId(PseudoElement type)
{
    switch (type) {
    case PseudoElement::FirstLine:
        return PseudoId::FirstLine;
    case PseudoElement::FirstLetter:
        return PseudoId::FirstLetter;
    case PseudoElement::GrammarError:
        return PseudoId::GrammarError;
    case PseudoElement::SpellingError:
        return PseudoId::SpellingError;
    case PseudoElement::Selection:
        return PseudoId::Selection;
    case PseudoElement::Highlight:
        return PseudoId::Highlight;
    case PseudoElement::Marker:
        return PseudoId::Marker;
    case PseudoElement::Backdrop:
        return PseudoId::Backdrop;
    case PseudoElement::Before:
        return PseudoId::Before;
    case PseudoElement::After:
        return PseudoId::After;
    case PseudoElement::Scrollbar:
        return PseudoId::Scrollbar;
    case PseudoElement::ScrollbarButton:
        return PseudoId::ScrollbarButton;
    case PseudoElement::ScrollbarCorner:
        return PseudoId::ScrollbarCorner;
    case PseudoElement::ScrollbarThumb:
        return PseudoId::ScrollbarThumb;
    case PseudoElement::ScrollbarTrack:
        return PseudoId::ScrollbarTrack;
    case PseudoElement::ScrollbarTrackPiece:
        return PseudoId::ScrollbarTrackPiece;
    case PseudoElement::Resizer:
        return PseudoId::Resizer;
    case PseudoElement::ViewTransition:
        return PseudoId::ViewTransition;
    case PseudoElement::ViewTransitionGroup:
        return PseudoId::ViewTransitionGroup;
    case PseudoElement::ViewTransitionImagePair:
        return PseudoId::ViewTransitionImagePair;
    case PseudoElement::ViewTransitionOld:
        return PseudoId::ViewTransitionOld;
    case PseudoElement::ViewTransitionNew:
        return PseudoId::ViewTransitionNew;
#if ENABLE(VIDEO)
    case PseudoElement::Cue:
#endif
    case PseudoElement::Slotted:
    case PseudoElement::Part:
    case PseudoElement::WebKitCustom:
    case PseudoElement::WebKitCustomLegacyPrefixed:
        return PseudoId::None;
    }

    ASSERT_NOT_REACHED();
    return PseudoId::None;
}

std::optional<CSSSelector::PseudoElement> CSSSelector::parsePseudoElement(StringView name, const CSSSelectorParserContext& context)
{
    if (name.isNull())
        return std::nullopt;

    auto type = parsePseudoElementString(name);
    if (!type) {
        // FIXME: Investigate removing -apple- as it's non-standard.
        if (name.startsWithIgnoringASCIICase("-webkit-"_s) || name.startsWithIgnoringASCIICase("-apple-"_s))
            return PseudoElement::WebKitCustom;
        return type;
    }

    switch (*type) {
    case PseudoElement::WebKitCustom:
        if (!context.thumbAndTrackPseudoElementsEnabled && (equalLettersIgnoringASCIICase(name, "thumb"_s) || equalLettersIgnoringASCIICase(name, "track"_s)))
            return std::nullopt;
        break;
    case PseudoElement::Highlight:
        if (!context.highlightAPIEnabled)
            return std::nullopt;
        break;
    case PseudoElement::GrammarError:
    case PseudoElement::SpellingError:
        if (!context.grammarAndSpellingPseudoElementsEnabled)
            return std::nullopt;
        break;
    case PseudoElement::ViewTransition:
    case PseudoElement::ViewTransitionGroup:
    case PseudoElement::ViewTransitionImagePair:
    case PseudoElement::ViewTransitionOld:
    case PseudoElement::ViewTransitionNew:
        if (!context.viewTransitionsEnabled)
            return std::nullopt;
        break;
    default:
        break;
    }
    return type;
}

const CSSSelector* CSSSelector::firstInCompound() const
{
    auto* selector = this;
    while (!selector->isFirstInTagHistory()) {
        auto* previousSelector = selector - 1;
        if (previousSelector->relation() != Relation::Subselector)
            break;
        selector = previousSelector;
    }
    return selector;
}

static void appendPseudoClassFunctionTail(StringBuilder& builder, const CSSSelector* selector)
{
    switch (selector->pseudoClass()) {
    case CSSSelector::PseudoClass::Dir:
    case CSSSelector::PseudoClass::Lang:
    case CSSSelector::PseudoClass::NthChild:
    case CSSSelector::PseudoClass::NthLastChild:
    case CSSSelector::PseudoClass::NthOfType:
    case CSSSelector::PseudoClass::NthLastOfType:
        builder.append(selector->argument(), ')');
        break;
    default:
        break;
    }

}

static void appendLangArgumentList(StringBuilder& builder, const FixedVector<PossiblyQuotedIdentifier>& list)
{
    for (unsigned i = 0, size = list.size(); i < size; ++i) {
        if (!list[i].wasQuoted)
            serializeIdentifier(list[i].identifier, builder);
        else
            serializeString(list[i].identifier, builder);
        if (i != size - 1)
            builder.append(", ");
    }
}

// http://dev.w3.org/csswg/css-syntax/#serializing-anb
static void outputNthChildAnPlusB(const CSSSelector& selector, StringBuilder& builder)
{
    auto outputFirstTerm = [&builder] (int a) {
        switch (a) {
        case 1:
            break;
        case -1:
            builder.append('-');
            break;
        default:
            builder.append(a);
        }
    };

    if (selector.argument() == nullAtom())
        return;

    int a = selector.nthA();
    int b = selector.nthB();
    if (a == 0 && b == 0)
        builder.append('0');
    else if (a == 0)
        builder.append(b);
    else if (b == 0) {
        outputFirstTerm(a);
        builder.append('n');
    } else if (b < 0) {
        outputFirstTerm(a);
        builder.append('n', b);
    } else {
        outputFirstTerm(a);
        builder.append("n+", b);
    }
}

String CSSSelector::selectorText(StringView separator, StringView rightSide) const
{
    StringBuilder builder;

    auto serializeIdentifierOrStar = [&] (const AtomString& identifier) {
        if (identifier == starAtom())
            builder.append('*');
        else
            serializeIdentifier(identifier, builder);
    };

    if (match() == Match::Tag && !m_tagIsForNamespaceRule) {
        if (auto& prefix = tagQName().prefix(); !prefix.isNull()) {
            serializeIdentifierOrStar(prefix);
            builder.append('|');
        }
        serializeIdentifierOrStar(tagQName().localName());
    }

    const CSSSelector* cs = this;
    while (true) {
        if (cs->isImplicit()) {
            // Remove the space before the implicit selector.
            separator = separator.substring(1);
            break;
        }
        if (cs->match() == Match::Id) {
            builder.append('#');
            serializeIdentifier(cs->serializingValue(), builder);
        } else if (cs->match() == Match::NestingParent) {
            builder.append('&');
        } else if (cs->match() == Match::Class) {
            builder.append('.');
            serializeIdentifier(cs->serializingValue(), builder);
        } else if (cs->match() == Match::ForgivingUnknown || cs->match() == Match::ForgivingUnknownNestContaining) {
            builder.append(cs->value());
        } else if (cs->match() == Match::PseudoClass) {
            switch (cs->pseudoClass()) {
#if ENABLE(FULLSCREEN_API)
            case CSSSelector::PseudoClass::AnimatingFullScreenTransition:
                builder.append(":-webkit-animating-full-screen-transition");
                break;
#endif
            case CSSSelector::PseudoClass::Any: {
                builder.append(":-webkit-any(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoClass::AnyLink:
                builder.append(":any-link");
                break;
            case CSSSelector::PseudoClass::AnyLinkDeprecated:
                builder.append(":-webkit-any-link");
                break;
            case CSSSelector::PseudoClass::Autofill:
                builder.append(":autofill");
                break;
            case CSSSelector::PseudoClass::AutofillAndObscured:
                builder.append(":-webkit-autofill-and-obscured");
                break;
            case CSSSelector::PseudoClass::AutofillStrongPassword:
                builder.append(":-webkit-autofill-strong-password");
                break;
            case CSSSelector::PseudoClass::AutofillStrongPasswordViewable:
                builder.append(":-webkit-autofill-strong-password-viewable");
                break;
            case CSSSelector::PseudoClass::Drag:
                builder.append(":-webkit-drag");
                break;
            case CSSSelector::PseudoClass::FullPageMedia:
                builder.append(":-webkit-full-page-media");
                break;
#if ENABLE(FULLSCREEN_API)
            case CSSSelector::PseudoClass::Fullscreen:
                builder.append(":fullscreen");
                break;
            case CSSSelector::PseudoClass::WebkitFullScreen:
                builder.append(":-webkit-full-screen");
                break;
            case CSSSelector::PseudoClass::FullScreenAncestor:
                builder.append(":-webkit-full-screen-ancestor");
                break;
            case CSSSelector::PseudoClass::FullScreenDocument:
                builder.append(":-webkit-full-screen-document");
                break;
            case CSSSelector::PseudoClass::FullScreenControlsHidden:
                builder.append(":-webkit-full-screen-controls-hidden");
                break;
#endif
#if ENABLE(PICTURE_IN_PICTURE_API)
            case CSSSelector::PseudoClass::PictureInPicture:
                builder.append(":picture-in-picture");
                break;
#endif
            case CSSSelector::PseudoClass::Active:
                builder.append(":active");
                break;
            case CSSSelector::PseudoClass::Checked:
                builder.append(":checked");
                break;
            case CSSSelector::PseudoClass::CornerPresent:
                builder.append(":corner-present");
                break;
            case CSSSelector::PseudoClass::Decrement:
                builder.append(":decrement");
                break;
            case CSSSelector::PseudoClass::Default:
                builder.append(":default");
                break;
            case CSSSelector::PseudoClass::Dir:
                builder.append(":dir(");
                appendPseudoClassFunctionTail(builder, cs);
                break;
            case CSSSelector::PseudoClass::Disabled:
                builder.append(":disabled");
                break;
            case CSSSelector::PseudoClass::DoubleButton:
                builder.append(":double-button");
                break;
            case CSSSelector::PseudoClass::Empty:
                builder.append(":empty");
                break;
            case CSSSelector::PseudoClass::Enabled:
                builder.append(":enabled");
                break;
            case CSSSelector::PseudoClass::End:
                builder.append(":end");
                break;
            case CSSSelector::PseudoClass::FirstChild:
                builder.append(":first-child");
                break;
            case CSSSelector::PseudoClass::FirstOfType:
                builder.append(":first-of-type");
                break;
            case CSSSelector::PseudoClass::Focus:
                builder.append(":focus");
                break;
            case CSSSelector::PseudoClass::FocusVisible:
                builder.append(":focus-visible");
                break;
            case CSSSelector::PseudoClass::FocusWithin:
                builder.append(":focus-within");
                break;
#if ENABLE(VIDEO)
            case CSSSelector::PseudoClass::Future:
                builder.append(":future");
                break;
            case CSSSelector::PseudoClass::Playing:
                builder.append(":playing");
                break;
            case CSSSelector::PseudoClass::Paused:
                builder.append(":paused");
                break;
            case CSSSelector::PseudoClass::Seeking:
                builder.append(":seeking");
                break;
            case CSSSelector::PseudoClass::Buffering:
                builder.append(":buffering");
                break;
            case CSSSelector::PseudoClass::Stalled:
                builder.append(":stalled");
                break;
            case CSSSelector::PseudoClass::Muted:
                builder.append(":muted");
                break;
            case CSSSelector::PseudoClass::VolumeLocked:
                builder.append(":volume-locked");
                break;
#endif
            case CSSSelector::PseudoClass::Has:
                builder.append(":has(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
#if ENABLE(ATTACHMENT_ELEMENT)
            case CSSSelector::PseudoClass::HasAttachment:
                builder.append(":has-attachment");
                break;
#endif
            case CSSSelector::PseudoClass::Horizontal:
                builder.append(":horizontal");
                break;
            case CSSSelector::PseudoClass::Hover:
                builder.append(":hover");
                break;
            case CSSSelector::PseudoClass::InRange:
                builder.append(":in-range");
                break;
            case CSSSelector::PseudoClass::Increment:
                builder.append(":increment");
                break;
            case CSSSelector::PseudoClass::Indeterminate:
                builder.append(":indeterminate");
                break;
            case CSSSelector::PseudoClass::Invalid:
                builder.append(":invalid");
                break;
            case CSSSelector::PseudoClass::HtmlDocument:
                builder.append(":-internal-html-document");
                break;
            case CSSSelector::PseudoClass::Lang:
                builder.append(":lang(");
                ASSERT_WITH_MESSAGE(cs->argumentList() && !cs->argumentList()->isEmpty(), "An empty :lang() is invalid and should never be generated by the parser.");
                appendLangArgumentList(builder, *cs->argumentList());
                builder.append(')');
                break;
            case CSSSelector::PseudoClass::LastChild:
                builder.append(":last-child");
                break;
            case CSSSelector::PseudoClass::LastOfType:
                builder.append(":last-of-type");
                break;
            case CSSSelector::PseudoClass::Link:
                builder.append(":link");
                break;
            case CSSSelector::PseudoClass::Modal:
                builder.append(":modal");
                break;
            case CSSSelector::PseudoClass::NoButton:
                builder.append(":no-button");
                break;
            case CSSSelector::PseudoClass::Not:
                builder.append(":not(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            case CSSSelector::PseudoClass::NthChild:
                builder.append(":nth-child(");
                outputNthChildAnPlusB(*cs, builder);
                if (const CSSSelectorList* selectorList = cs->selectorList()) {
                    builder.append(" of ");
                    selectorList->buildSelectorsText(builder);
                }
                builder.append(')');
                break;
            case CSSSelector::PseudoClass::NthLastChild:
                builder.append(":nth-last-child(");
                outputNthChildAnPlusB(*cs, builder);
                if (const CSSSelectorList* selectorList = cs->selectorList()) {
                    builder.append(" of ");
                    selectorList->buildSelectorsText(builder);
                }
                builder.append(')');
                break;
            case CSSSelector::PseudoClass::NthLastOfType:
                builder.append(":nth-last-of-type(");
                appendPseudoClassFunctionTail(builder, cs);
                break;
            case CSSSelector::PseudoClass::NthOfType:
                builder.append(":nth-of-type(");
                appendPseudoClassFunctionTail(builder, cs);
                break;
            case CSSSelector::PseudoClass::OnlyChild:
                builder.append(":only-child");
                break;
            case CSSSelector::PseudoClass::OnlyOfType:
                builder.append(":only-of-type");
                break;
            case CSSSelector::PseudoClass::PopoverOpen:
                builder.append(":popover-open");
                break;
            case CSSSelector::PseudoClass::Optional:
                builder.append(":optional");
                break;
            case CSSSelector::PseudoClass::Is: {
                builder.append(":is(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoClass::Matches: {
                builder.append(":matches(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoClass::Where: {
                builder.append(":where(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoClass::PlaceholderShown:
                builder.append(":placeholder-shown");
                break;
            case CSSSelector::PseudoClass::OutOfRange:
                builder.append(":out-of-range");
                break;
#if ENABLE(VIDEO)
            case CSSSelector::PseudoClass::Past:
                builder.append(":past");
                break;
#endif
            case CSSSelector::PseudoClass::ReadOnly:
                builder.append(":read-only");
                break;
            case CSSSelector::PseudoClass::ReadWrite:
                builder.append(":read-write");
                break;
            case CSSSelector::PseudoClass::Required:
                builder.append(":required");
                break;
            case CSSSelector::PseudoClass::Root:
                builder.append(":root");
                break;
            case CSSSelector::PseudoClass::Scope:
                builder.append(":scope");
                break;
            case CSSSelector::PseudoClass::State:
                builder.append(":state(");
                serializeIdentifier(cs->argument(), builder);
                builder.append(')');
                break;
            case CSSSelector::PseudoClass::HasScope:
                // Remove the space from the start to generate a relative selector string like in ":has(> foo)".
                return makeString(separator.substring(1), rightSide);
            case CSSSelector::PseudoClass::SingleButton:
                builder.append(":single-button");
                break;
            case CSSSelector::PseudoClass::Start:
                builder.append(":start");
                break;
            case CSSSelector::PseudoClass::Target:
                builder.append(":target");
                break;
            case CSSSelector::PseudoClass::UserInvalid:
                builder.append(":user-invalid");
                break;
            case CSSSelector::PseudoClass::UserValid:
                builder.append(":user-valid");
                break;
            case CSSSelector::PseudoClass::Valid:
                builder.append(":valid");
                break;
            case CSSSelector::PseudoClass::Vertical:
                builder.append(":vertical");
                break;
            case CSSSelector::PseudoClass::Visited:
                builder.append(":visited");
                break;
            case CSSSelector::PseudoClass::WindowInactive:
                builder.append(":window-inactive");
                break;
            case CSSSelector::PseudoClass::Host:
                builder.append(":host");
                if (auto* selectorList = cs->selectorList()) {
                    builder.append('(');
                    selectorList->buildSelectorsText(builder);
                    builder.append(')');
                }
                break;
            case CSSSelector::PseudoClass::Defined:
                builder.append(":defined");
                break;
            }
        } else if (cs->match() == Match::PseudoElement) {
            switch (cs->pseudoElement()) {
            case CSSSelector::PseudoElement::Slotted:
                builder.append("::slotted("_s);
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            case CSSSelector::PseudoElement::Highlight:
            case CSSSelector::PseudoElement::ViewTransitionGroup:
            case CSSSelector::PseudoElement::ViewTransitionImagePair:
            case CSSSelector::PseudoElement::ViewTransitionOld:
            case CSSSelector::PseudoElement::ViewTransitionNew: {
                builder.append("::"_s, cs->serializingValue(), '(');
                serializeIdentifierOrStar(cs->argumentList()->first().identifier);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoElement::Part: {
                builder.append("::part("_s);
                bool isFirst = true;
                for (auto& partName : *cs->argumentList()) {
                    if (!isFirst)
                        builder.append(' ');
                    isFirst = false;
                    serializeIdentifier(partName.identifier, builder);
                }
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoElement::WebKitCustomLegacyPrefixed:
                if (cs->value() == "placeholder"_s)
                    builder.append("::-webkit-input-placeholder"_s);
                if (cs->value() == "file-selector-button"_s)
                    builder.append("::-webkit-file-upload-button"_s);
                break;
#if ENABLE(VIDEO)
            case CSSSelector::PseudoElement::Cue: {
                if (auto* selectorList = cs->selectorList()) {
                    builder.append("::cue("_s);
                    selectorList->buildSelectorsText(builder);
                    builder.append(')');
                } else
                    builder.append("::cue"_s);
                break;
            }
#endif
            default:
                builder.append("::");
                serializeIdentifier(cs->serializingValue(), builder);
            }
        } else if (cs->isAttributeSelector()) {
            builder.append('[');
            if (auto& prefix = cs->attribute().prefix(); !prefix.isEmpty()) {
                serializeIdentifierOrStar(prefix);
                builder.append('|');
            }
            serializeIdentifierOrStar(cs->attribute().localName());
            switch (cs->match()) {
            case Match::Exact:
                builder.append('=');
                break;
            case Match::Set:
                // set has no operator or value, just the attrName
                builder.append(']');
                break;
            case Match::List:
                builder.append("~=");
                break;
            case Match::Hyphen:
                builder.append("|=");
                break;
            case Match::Begin:
                builder.append("^=");
                break;
            case Match::End:
                builder.append("$=");
                break;
            case Match::Contain:
                builder.append("*=");
                break;
            default:
                break;
            }
            if (cs->match() != Match::Set) {
                serializeString(cs->serializingValue(), builder);
                if (cs->attributeValueMatchingIsCaseInsensitive())
                    builder.append(" i]");
                else
                    builder.append(']');
            }
        } else if (cs->match() == Match::PagePseudoClass) {
            switch (cs->pagePseudoClass()) {
            case PagePseudoClass::First:
                builder.append(":first");
                break;
            case PagePseudoClass::Left:
                builder.append(":left");
                break;
            case PagePseudoClass::Right:
                builder.append(":right");
                break;
            }
        }

        if (cs->relation() != Relation::Subselector || !cs->tagHistory())
            break;
        cs = cs->tagHistory();
    }

    builder.append(separator, rightSide);

    auto separatorTextForNestingRelative = [&] () -> String {
        switch (cs->relation()) {
        case CSSSelector::Relation::Child:
            return "> "_s;
        case CSSSelector::Relation::DirectAdjacent:
            return "+ "_s;
        case CSSSelector::Relation::IndirectAdjacent:
            return "~ "_s;
        default:
            return { };
        }
    };

    if (auto* previousSelector = cs->tagHistory()) {
        ASCIILiteral separator = ""_s;
        switch (cs->relation()) {
        case CSSSelector::Relation::DescendantSpace:
            separator = " "_s;
            break;
        case CSSSelector::Relation::Child:
            separator = " > "_s;
            break;
        case CSSSelector::Relation::DirectAdjacent:
            separator = " + "_s;
            break;
        case CSSSelector::Relation::IndirectAdjacent:
            separator = " ~ "_s;
            break;
        case CSSSelector::Relation::Subselector:
            ASSERT_NOT_REACHED();
            break;
        case CSSSelector::Relation::ShadowDescendant:
        case CSSSelector::Relation::ShadowPartDescendant:
        case CSSSelector::Relation::ShadowSlotted:
            break;
        }
        return previousSelector->selectorText(separator, builder);
    } else if (auto separatorText = separatorTextForNestingRelative(); !separatorText.isNull()) {
        // We have a separator but no tag history which can happen with implicit relative nesting selector
        return separatorText + builder.toString();
    }

    return builder.toString();
}

void CSSSelector::setAttribute(const QualifiedName& value, AttributeMatchType matchType)
{
    createRareData();
    m_data.rareData->attribute = value;
    m_caseInsensitiveAttributeValueMatching = matchType == CaseInsensitive;
}
    
void CSSSelector::setArgument(const AtomString& value)
{
    createRareData();
    m_data.rareData->argument = value;
}

void CSSSelector::setArgumentList(FixedVector<PossiblyQuotedIdentifier> argumentList)
{
    createRareData();
    m_data.rareData->argumentList = WTFMove(argumentList);
}

void CSSSelector::setSelectorList(std::unique_ptr<CSSSelectorList> selectorList)
{
    createRareData();
    m_data.rareData->selectorList = WTFMove(selectorList);
}

void CSSSelector::setNth(int a, int b)
{
    createRareData();
    m_data.rareData->a = a;
    m_data.rareData->b = b;
}

bool CSSSelector::matchNth(int count) const
{
    ASSERT(m_hasRareData);
    return m_data.rareData->matchNth(count);
}

int CSSSelector::nthA() const
{
    ASSERT(m_hasRareData);
    return m_data.rareData->a;
}

int CSSSelector::nthB() const
{
    ASSERT(m_hasRareData);
    return m_data.rareData->b;
}

CSSSelector::RareData::RareData(AtomString&& value)
    : matchingValue(value)
    , serializingValue(WTFMove(value))
    , attribute(anyQName())
{
}

CSSSelector::RareData::RareData(const RareData& other)
    : matchingValue(other.matchingValue)
    , serializingValue(other.serializingValue)
    , a(other.a)
    , b(other.b)
    , attribute(other.attribute)
    , argument(other.argument)
    , argumentList(other.argumentList)
{
    if (other.selectorList)
        this->selectorList = makeUnique<CSSSelectorList>(*other.selectorList);
};

auto CSSSelector::RareData::deepCopy() const -> Ref<RareData> 
{
    return adoptRef(*new RareData (*this));
}

CSSSelector::RareData::~RareData() = default;

auto CSSSelector::RareData::create(AtomString value) -> Ref<RareData>
{
    return adoptRef(*new RareData(WTFMove(value)));
}

bool CSSSelector::RareData::matchNth(int count)
{
    if (a > 0)
        return count >= b && !((count - b) % a);
    if (a < 0)
        return count <= b && !((b - count) % -a);
    return count == b;
}

CSSSelector::CSSSelector(const CSSSelector& other)
    : m_relation(other.m_relation)
    , m_match(other.m_match)
    , m_pseudoType(other.m_pseudoType)
    , m_isLastInSelectorList(other.m_isLastInSelectorList)
    , m_isFirstInTagHistory(other.m_isFirstInTagHistory)
    , m_isLastInTagHistory(other.m_isLastInTagHistory)
    , m_hasRareData(other.m_hasRareData)
    , m_isForPage(other.m_isForPage)
    , m_tagIsForNamespaceRule(other.m_tagIsForNamespaceRule)
    , m_caseInsensitiveAttributeValueMatching(other.m_caseInsensitiveAttributeValueMatching)
    , m_isImplicit(other.m_isImplicit)
{
    // Manually ref count the m_data union because they are stored as raw ptr, not as Ref.
    if (other.m_hasRareData)
        m_data.rareData = &other.m_data.rareData->deepCopy().leakRef();
    else if (other.match() == Match::Tag) {
        m_data.tagQName = other.m_data.tagQName;
        m_data.tagQName->ref();
    } else if (other.m_data.value) {
        m_data.value = other.m_data.value;
        m_data.value->ref();
    }
}

bool CSSSelector::visitAllSimpleSelectors(auto& apply) const
{
    std::queue<const CSSSelector*> worklist;
    worklist.push(this);
    while (!worklist.empty()) {
        auto current = worklist.front();
        worklist.pop();

        // Effective C++ advices for this cast to deal with generic const/non-const member function.
        if (apply(*const_cast<CSSSelector*>(current)))
            return true;

        // Visit the selector list member (if any) recursively (such as: :has(<list>), :is(<list>),...)
        if (auto selectorList = current->selectorList()) {
            auto next = selectorList->first();
            while (next) {
                worklist.push(next);
                next = CSSSelectorList::next(next);
            }
        }

        // Visit the next simple selector
        if (auto next = current->tagHistory())
            worklist.push(next);
    }
    return false;
}

void CSSSelector::resolveNestingParentSelectors(const CSSSelectorList& parent)
{
    auto replaceParentSelector = [&parent] (CSSSelector& selector) {
        if (selector.match() == CSSSelector::Match::NestingParent) {
            // FIXME: Optimize cases where we can include the parent selector directly instead of wrapping it in a ":is" pseudo class.
            selector.setMatch(Match::PseudoClass);
            selector.setPseudoClass(PseudoClass::Is);
            selector.setSelectorList(makeUnique<CSSSelectorList>(parent));
        }
        return false;
    };

    visitAllSimpleSelectors(replaceParentSelector);
}

void CSSSelector::replaceNestingParentByPseudoClassScope()
{
    auto replaceParentSelector = [] (CSSSelector& selector) {
        if (selector.match() == CSSSelector::Match::NestingParent) {
            // Replace by :scope
            selector.setMatch(Match::PseudoClass);
            selector.setPseudoClass(PseudoClass::Scope);
        }
        return false;
    };

    visitAllSimpleSelectors(replaceParentSelector); 
}

bool CSSSelector::hasExplicitNestingParent() const
{
    auto checkForExplicitParent = [] (const CSSSelector& selector) {
        if (selector.match() == Match::NestingParent)
            return true;

        if (selector.match() == Match::ForgivingUnknownNestContaining)
            return true;

        return false;
    };

    return visitAllSimpleSelectors(checkForExplicitParent);
}

bool CSSSelector::hasExplicitPseudoClassScope() const
{
    auto check = [] (const CSSSelector& selector) {
        if (selector.match() == Match::PseudoClass && selector.pseudoClass() == PseudoClass::Scope)
            return true;

        return false;
    };

    return visitAllSimpleSelectors(check);
}

} // namespace WebCore
