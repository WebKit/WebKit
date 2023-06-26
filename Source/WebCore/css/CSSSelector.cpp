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
#include "CommonAtomStrings.h"
#include "DeprecatedGlobalSettings.h"
#include "HTMLNames.h"
#include "SelectorPseudoTypeMap.h"
#include <memory>
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

static_assert(CSSSelector::RelationType::Subselector == static_cast<CSSSelector::RelationType>(0u), "Subselector must be 0 for consumeCombinator.");
static_assert(sizeof(CSSSelector) == sizeof(SameSizeAsCSSSelector), "CSSSelector should remain small.");

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSSelectorRareData);

CSSSelector::CSSSelector(const QualifiedName& tagQName, bool tagIsForNamespaceRule)
    : m_relation(static_cast<unsigned>(RelationType::DescendantSpace))
    , m_match(static_cast<unsigned>(Match::Tag))
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
};

SelectorSpecificity::SelectorSpecificity(unsigned specificity)
    : specificity(specificity)
{
}

SelectorSpecificity::SelectorSpecificity(SelectorSpecificityIncrement specificity)
    : specificity(static_cast<unsigned>(specificity))
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
    for (auto* selector = &firstSimpleSelector; selector; selector = selector->tagHistory())
        total += simpleSelectorSpecificity(*selector);
    return total;
}

static SelectorSpecificity maxSpecificity(const CSSSelectorList* selectorList)
{
    unsigned max = 0;
    if (selectorList) {
        for (auto* selector = selectorList->first(); selector; selector = CSSSelectorList::next(selector))
            max = std::max(max, selectorSpecificity(*selector).specificity);
    }
    return max;
}

SelectorSpecificity simpleSelectorSpecificity(const CSSSelector& simpleSelector)
{
    ASSERT_WITH_MESSAGE(!simpleSelector.isForPage(), "At the time of this writing, page selectors are not treated as real selectors that are matched. The value computed here only account for real selectors.");

    switch (simpleSelector.match()) {
    case CSSSelector::Match::Id:
        return SelectorSpecificityIncrement::ClassA;
    case CSSSelector::Match::NestingParent:
    case CSSSelector::Match::PagePseudoClass:
        break;
    case CSSSelector::Match::PseudoClass:
        switch (simpleSelector.pseudoClassType()) {
        case CSSSelector::PseudoClassIs:
        case CSSSelector::PseudoClassMatches:
        case CSSSelector::PseudoClassNot:
        case CSSSelector::PseudoClassHas:
            return maxSpecificity(simpleSelector.selectorList());
        case CSSSelector::PseudoClassWhere:
            return 0;
        case CSSSelector::PseudoClassNthChild:
        case CSSSelector::PseudoClassNthLastChild:
        case CSSSelector::PseudoClassHost:
            return SelectorSpecificityIncrement::ClassB + maxSpecificity(simpleSelector.selectorList());
        case CSSSelector::PseudoClassRelativeScope:
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
        if (simpleSelector.pseudoElementType() == CSSSelector::PseudoElementSlotted)
            return maxSpecificity(simpleSelector.selectorList());
        return SelectorSpecificityIncrement::ClassC;
    case CSSSelector::Match::Unknown:
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
    auto integer = computeSpecificity();
    uint8_t a = integer >> 16;
    uint8_t b = integer >> 8;
    uint8_t c = integer;
    return { a, b, c };
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
            switch (component->pagePseudoClassType()) {
            case PagePseudoClassFirst:
                s += 2;
                break;
            case PagePseudoClassLeft:
            case PagePseudoClassRight:
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

PseudoId CSSSelector::pseudoId(PseudoElementType type)
{
    switch (type) {
    case PseudoElementFirstLine:
        return PseudoId::FirstLine;
    case PseudoElementFirstLetter:
        return PseudoId::FirstLetter;
    case PseudoElementSelection:
        return PseudoId::Selection;
    case PseudoElementHighlight:
        return PseudoId::Highlight;
    case PseudoElementMarker:
        return PseudoId::Marker;
    case PseudoElementBackdrop:
        return PseudoId::Backdrop;
    case PseudoElementBefore:
        return PseudoId::Before;
    case PseudoElementAfter:
        return PseudoId::After;
    case PseudoElementScrollbar:
        return PseudoId::Scrollbar;
    case PseudoElementScrollbarButton:
        return PseudoId::ScrollbarButton;
    case PseudoElementScrollbarCorner:
        return PseudoId::ScrollbarCorner;
    case PseudoElementScrollbarThumb:
        return PseudoId::ScrollbarThumb;
    case PseudoElementScrollbarTrack:
        return PseudoId::ScrollbarTrack;
    case PseudoElementScrollbarTrackPiece:
        return PseudoId::ScrollbarTrackPiece;
    case PseudoElementResizer:
        return PseudoId::Resizer;
#if ENABLE(VIDEO)
    case PseudoElementCue:
#endif
    case PseudoElementSlotted:
    case PseudoElementPart:
    case PseudoElementUnknown:
    case PseudoElementWebKitCustom:
    case PseudoElementWebKitCustomLegacyPrefixed:
        return PseudoId::None;
    }

    ASSERT_NOT_REACHED();
    return PseudoId::None;
}

CSSSelector::PseudoElementType CSSSelector::parsePseudoElementType(StringView name)
{
    if (name.isNull())
        return PseudoElementUnknown;

    auto type = parsePseudoElementString(name);
    if (type == PseudoElementUnknown) {
        if (name.startsWith("-webkit-"_s) || name.startsWith("-apple-"_s))
            type = PseudoElementWebKitCustom;
    }

    if (type == PseudoElementHighlight && !DeprecatedGlobalSettings::highlightAPIEnabled())
        return PseudoElementUnknown;

    return type;
}

const CSSSelector* CSSSelector::firstInCompound() const
{
    auto* selector = this;
    while (!selector->isFirstInTagHistory()) {
        auto* previousSelector = selector - 1;
        if (previousSelector->relation() != RelationType::Subselector)
            break;
        selector = previousSelector;
    }
    return selector;
}

static void appendPseudoClassFunctionTail(StringBuilder& builder, const CSSSelector* selector)
{
    switch (selector->pseudoClassType()) {
    case CSSSelector::PseudoClassDir:
    case CSSSelector::PseudoClassLang:
    case CSSSelector::PseudoClassNthChild:
    case CSSSelector::PseudoClassNthLastChild:
    case CSSSelector::PseudoClassNthOfType:
    case CSSSelector::PseudoClassNthLastOfType:
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
        if (cs->match() == Match::Id) {
            builder.append('#');
            serializeIdentifier(cs->serializingValue(), builder);
        } else if (cs->match() == CSSSelector::Match::NestingParent) {
            builder.append('&');
        } else if (cs->match() == CSSSelector::Match::Class) {
            builder.append('.');
            serializeIdentifier(cs->serializingValue(), builder);
        } else if (cs->match() == Match::PseudoClass) {
            switch (cs->pseudoClassType()) {
#if ENABLE(FULLSCREEN_API)
            case CSSSelector::PseudoClassAnimatingFullScreenTransition:
                builder.append(":-webkit-animating-full-screen-transition");
                break;
#endif
            case CSSSelector::PseudoClassAny: {
                builder.append(":-webkit-any(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoClassAnyLink:
                builder.append(":any-link");
                break;
            case CSSSelector::PseudoClassAnyLinkDeprecated:
                builder.append(":-webkit-any-link");
                break;
            case CSSSelector::PseudoClassAutofill:
                builder.append(":autofill");
                break;
            case CSSSelector::PseudoClassAutofillAndObscured:
                builder.append(":-webkit-autofill-and-obscured");
                break;
            case CSSSelector::PseudoClassAutofillStrongPassword:
                builder.append(":-webkit-autofill-strong-password");
                break;
            case CSSSelector::PseudoClassAutofillStrongPasswordViewable:
                builder.append(":-webkit-autofill-strong-password-viewable");
                break;
            case CSSSelector::PseudoClassDrag:
                builder.append(":-webkit-drag");
                break;
            case CSSSelector::PseudoClassFullPageMedia:
                builder.append(":-webkit-full-page-media");
                break;
#if ENABLE(FULLSCREEN_API)
            case CSSSelector::PseudoClassFullscreen:
                builder.append(":fullscreen");
                break;
            case CSSSelector::PseudoClassWebkitFullScreen:
                builder.append(":-webkit-full-screen");
                break;
            case CSSSelector::PseudoClassFullScreenAncestor:
                builder.append(":-webkit-full-screen-ancestor");
                break;
            case CSSSelector::PseudoClassFullScreenDocument:
                builder.append(":-webkit-full-screen-document");
                break;
            case CSSSelector::PseudoClassFullScreenControlsHidden:
                builder.append(":-webkit-full-screen-controls-hidden");
                break;
#endif
#if ENABLE(PICTURE_IN_PICTURE_API)
            case CSSSelector::PseudoClassPictureInPicture:
                builder.append(":picture-in-picture");
                break;
#endif
            case CSSSelector::PseudoClassActive:
                builder.append(":active");
                break;
            case CSSSelector::PseudoClassChecked:
                builder.append(":checked");
                break;
            case CSSSelector::PseudoClassCornerPresent:
                builder.append(":corner-present");
                break;
            case CSSSelector::PseudoClassDecrement:
                builder.append(":decrement");
                break;
            case CSSSelector::PseudoClassDefault:
                builder.append(":default");
                break;
            case CSSSelector::PseudoClassDir:
                builder.append(":dir(");
                appendPseudoClassFunctionTail(builder, cs);
                break;
            case CSSSelector::PseudoClassDisabled:
                builder.append(":disabled");
                break;
            case CSSSelector::PseudoClassDoubleButton:
                builder.append(":double-button");
                break;
            case CSSSelector::PseudoClassEmpty:
                builder.append(":empty");
                break;
            case CSSSelector::PseudoClassEnabled:
                builder.append(":enabled");
                break;
            case CSSSelector::PseudoClassEnd:
                builder.append(":end");
                break;
            case CSSSelector::PseudoClassFirstChild:
                builder.append(":first-child");
                break;
            case CSSSelector::PseudoClassFirstOfType:
                builder.append(":first-of-type");
                break;
            case CSSSelector::PseudoClassFocus:
                builder.append(":focus");
                break;
            case CSSSelector::PseudoClassFocusVisible:
                builder.append(":focus-visible");
                break;
            case CSSSelector::PseudoClassFocusWithin:
                builder.append(":focus-within");
                break;
#if ENABLE(VIDEO)
            case CSSSelector::PseudoClassFuture:
                builder.append(":future");
                break;
            case CSSSelector::PseudoClassPlaying:
                builder.append(":playing");
                break;
            case CSSSelector::PseudoClassPaused:
                builder.append(":paused");
                break;
            case CSSSelector::PseudoClassSeeking:
                builder.append(":seeking");
                break;
            case CSSSelector::PseudoClassBuffering:
                builder.append(":buffering");
                break;
            case CSSSelector::PseudoClassStalled:
                builder.append(":stalled");
                break;
            case CSSSelector::PseudoClassMuted:
                builder.append(":muted");
                break;
            case CSSSelector::PseudoClassVolumeLocked:
                builder.append(":volume-locked");
                break;
#endif
            case CSSSelector::PseudoClassHas:
                builder.append(":has(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
#if ENABLE(ATTACHMENT_ELEMENT)
            case CSSSelector::PseudoClassHasAttachment:
                builder.append(":has-attachment");
                break;
#endif
            case CSSSelector::PseudoClassHorizontal:
                builder.append(":horizontal");
                break;
            case CSSSelector::PseudoClassHover:
                builder.append(":hover");
                break;
            case CSSSelector::PseudoClassInRange:
                builder.append(":in-range");
                break;
            case CSSSelector::PseudoClassIncrement:
                builder.append(":increment");
                break;
            case CSSSelector::PseudoClassIndeterminate:
                builder.append(":indeterminate");
                break;
            case CSSSelector::PseudoClassInvalid:
                builder.append(":invalid");
                break;
            case CSSSelector::PseudoClassHtmlDocument:
                builder.append(":-internal-html-document");
                break;
            case CSSSelector::PseudoClassLang:
                builder.append(":lang(");
                ASSERT_WITH_MESSAGE(cs->argumentList() && !cs->argumentList()->isEmpty(), "An empty :lang() is invalid and should never be generated by the parser.");
                appendLangArgumentList(builder, *cs->argumentList());
                builder.append(')');
                break;
            case CSSSelector::PseudoClassLastChild:
                builder.append(":last-child");
                break;
            case CSSSelector::PseudoClassLastOfType:
                builder.append(":last-of-type");
                break;
            case CSSSelector::PseudoClassLink:
                builder.append(":link");
                break;
            case CSSSelector::PseudoClassModal:
                builder.append(":modal");
                break;
            case CSSSelector::PseudoClassNoButton:
                builder.append(":no-button");
                break;
            case CSSSelector::PseudoClassNot:
                builder.append(":not(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            case CSSSelector::PseudoClassNthChild:
                builder.append(":nth-child(");
                outputNthChildAnPlusB(*cs, builder);
                if (const CSSSelectorList* selectorList = cs->selectorList()) {
                    builder.append(" of ");
                    selectorList->buildSelectorsText(builder);
                }
                builder.append(')');
                break;
            case CSSSelector::PseudoClassNthLastChild:
                builder.append(":nth-last-child(");
                outputNthChildAnPlusB(*cs, builder);
                if (const CSSSelectorList* selectorList = cs->selectorList()) {
                    builder.append(" of ");
                    selectorList->buildSelectorsText(builder);
                }
                builder.append(')');
                break;
            case CSSSelector::PseudoClassNthLastOfType:
                builder.append(":nth-last-of-type(");
                appendPseudoClassFunctionTail(builder, cs);
                break;
            case CSSSelector::PseudoClassNthOfType:
                builder.append(":nth-of-type(");
                appendPseudoClassFunctionTail(builder, cs);
                break;
            case CSSSelector::PseudoClassOnlyChild:
                builder.append(":only-child");
                break;
            case CSSSelector::PseudoClassOnlyOfType:
                builder.append(":only-of-type");
                break;
            case CSSSelector::PseudoClassPopoverOpen:
                builder.append(":popover-open");
                break;
            case CSSSelector::PseudoClassOptional:
                builder.append(":optional");
                break;
            case CSSSelector::PseudoClassIs: {
                builder.append(":is(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoClassMatches: {
                builder.append(":matches(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoClassWhere: {
                builder.append(":where(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoClassPlaceholderShown:
                builder.append(":placeholder-shown");
                break;
            case CSSSelector::PseudoClassOutOfRange:
                builder.append(":out-of-range");
                break;
#if ENABLE(VIDEO)
            case CSSSelector::PseudoClassPast:
                builder.append(":past");
                break;
#endif
            case CSSSelector::PseudoClassReadOnly:
                builder.append(":read-only");
                break;
            case CSSSelector::PseudoClassReadWrite:
                builder.append(":read-write");
                break;
            case CSSSelector::PseudoClassRequired:
                builder.append(":required");
                break;
            case CSSSelector::PseudoClassRoot:
                builder.append(":root");
                break;
            case CSSSelector::PseudoClassScope:
                builder.append(":scope");
                break;
            case CSSSelector::PseudoClassRelativeScope:
                // Remove the space from the start to generate a relative selector string like in ":has(> foo)".
                return makeString(separator.substring(1), rightSide);
            case CSSSelector::PseudoClassSingleButton:
                builder.append(":single-button");
                break;
            case CSSSelector::PseudoClassStart:
                builder.append(":start");
                break;
            case CSSSelector::PseudoClassTarget:
                builder.append(":target");
                break;
            case CSSSelector::PseudoClassUserInvalid:
                builder.append(":user-invalid");
                break;
            case CSSSelector::PseudoClassUserValid:
                builder.append(":user-valid");
                break;
            case CSSSelector::PseudoClassValid:
                builder.append(":valid");
                break;
            case CSSSelector::PseudoClassVertical:
                builder.append(":vertical");
                break;
            case CSSSelector::PseudoClassVisited:
                builder.append(":visited");
                break;
            case CSSSelector::PseudoClassWindowInactive:
                builder.append(":window-inactive");
                break;
            case CSSSelector::PseudoClassHost:
                builder.append(":host");
                if (auto* selectorList = cs->selectorList()) {
                    builder.append('(');
                    selectorList->buildSelectorsText(builder);
                    builder.append(')');
                }
                break;
            case CSSSelector::PseudoClassDefined:
                builder.append(":defined");
                break;
            case CSSSelector::PseudoClassUnknown:
                ASSERT_NOT_REACHED();
            }
        } else if (cs->match() == Match::PseudoElement) {
            switch (cs->pseudoElementType()) {
            case CSSSelector::PseudoElementSlotted:
                builder.append("::slotted(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            case CSSSelector::PseudoElementPart: {
                builder.append("::part(");
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
            case CSSSelector::PseudoElementWebKitCustomLegacyPrefixed:
                if (cs->value() == "placeholder"_s)
                    builder.append("::-webkit-input-placeholder"_s);
                if (cs->value() == "file-selector-button"_s)
                    builder.append("::-webkit-file-upload-button"_s);
                break;
#if ENABLE(VIDEO)
            case CSSSelector::PseudoElementCue: {
                if (auto* selectorList = cs->selectorList()) {
                    builder.append("::cue(");
                    selectorList->buildSelectorsText(builder);
                    builder.append(')');
                } else
                    builder.append("::cue");
                break;
            }
#endif
            default:
                builder.append("::", cs->serializingValue());
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
            switch (cs->pagePseudoClassType()) {
            case PagePseudoClassFirst:
                builder.append(":first");
                break;
            case PagePseudoClassLeft:
                builder.append(":left");
                break;
            case PagePseudoClassRight:
                builder.append(":right");
                break;
            }
        }

        if (cs->relation() != RelationType::Subselector || !cs->tagHistory())
            break;
        cs = cs->tagHistory();
    }

    builder.append(separator, rightSide);

    auto separatorTextForNestingRelative = [&] () -> String {
        switch (cs->relation()) {
        case CSSSelector::RelationType::Child:
            return "> "_s;
        case CSSSelector::RelationType::DirectAdjacent:
            return "+ "_s;
        case CSSSelector::RelationType::IndirectAdjacent:
            return "~ "_s;
        default:
            return { };
        }
    };

    if (auto* previousSelector = cs->tagHistory()) {
        ASCIILiteral separator = ""_s;
        switch (cs->relation()) {
        case CSSSelector::RelationType::DescendantSpace:
            separator = " "_s;
            break;
        case CSSSelector::RelationType::Child:
            separator = " > "_s;
            break;
        case CSSSelector::RelationType::DirectAdjacent:
            separator = " + "_s;
            break;
        case CSSSelector::RelationType::IndirectAdjacent:
            separator = " ~ "_s;
            break;
        case CSSSelector::RelationType::Subselector:
            ASSERT_NOT_REACHED();
            break;
        case CSSSelector::RelationType::ShadowDescendant:
        case CSSSelector::RelationType::ShadowPartDescendant:
        case CSSSelector::RelationType::ShadowSlotted:
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

void CSSSelector::visitAllSimpleSelectors(auto& apply) const
{
    // Effective C++ advices for this cast to deal with generic const/non-const member function.
    apply(*const_cast<CSSSelector*>(this));

    // Visit the selector list member (if any) recursively (such as: :has(<list>), :is(<list>),...)
    if (auto selectorList = this->selectorList()) {
        auto next = selectorList->first();
        while (next) {
            next->visitAllSimpleSelectors(apply);
            next = CSSSelectorList::next(next);
        }
    }

    // Visit the next simple selector recursively
    if (auto next = tagHistory())
        next->visitAllSimpleSelectors(apply); 
}

void CSSSelector::resolveNestingParentSelectors(const CSSSelectorList& parent)
{
    auto replaceParentSelector = [&parent] (CSSSelector& selector) {
        if (selector.match() == CSSSelector::Match::NestingParent) {
            // FIXME: Optimize cases where we can include the parent selector directly instead of wrapping it in a ":is" pseudo class.
            selector.setMatch(Match::PseudoClass);
            selector.setPseudoClassType(PseudoClassType::PseudoClassIs);
            selector.setSelectorList(makeUnique<CSSSelectorList>(parent));
        }
    };

    visitAllSimpleSelectors(replaceParentSelector);
}

void CSSSelector::replaceNestingParentByPseudoClassScope()
{
    auto replaceParentSelector = [] (CSSSelector& selector) {
        if (selector.match() == CSSSelector::Match::NestingParent) {
            // Replace by :scope
            selector.setMatch(Match::PseudoClass);
            selector.setPseudoClassType(PseudoClassType::PseudoClassScope);
        }
    };

    visitAllSimpleSelectors(replaceParentSelector); 
}

bool CSSSelector::hasExplicitNestingParent() const
{
    bool result = false;

    auto checkForExplicitParent = [&result] (const CSSSelector& selector) {
        if (selector.match() == CSSSelector::Match::NestingParent)
            result = true;
    };

    visitAllSimpleSelectors(checkForExplicitParent);

    return result;
}

} // namespace WebCore
