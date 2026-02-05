/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002-2026 Apple Inc. All rights reserved.
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
#include "CSSSelectorInlines.h"
#include "CSSSelectorList.h"
#include "CommonAtomStrings.h"
#include "HTMLNames.h"
#include "MutableCSSSelector.h"
#include "SelectorPseudoTypeMap.h"
#include <memory>
#include <queue>
#include <wtf/Assertions.h>
#include <wtf/Hasher.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CSSSelector);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CSSSelector::RareData);

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

enum class IgnorePseudoElement : bool { No, Yes };

static SelectorSpecificity simpleSelectorSpecificity(const CSSSelector&, IgnorePseudoElement = IgnorePseudoElement::No);

static SelectorSpecificity selectorSpecificity(const CSSSelector& firstSimpleSelector, IgnorePseudoElement ignorePseudoElement = IgnorePseudoElement::No)
{
    SelectorSpecificity total;
    for (const auto* selector = &firstSimpleSelector; selector; selector = selector->precedingInComplexSelector())
        total += simpleSelectorSpecificity(*selector, ignorePseudoElement);
    return total;
}

static SelectorSpecificity maxSpecificity(const CSSSelectorList* selectorList, IgnorePseudoElement ignorePseudoElement = IgnorePseudoElement::No)
{
    unsigned max = 0;
    if (selectorList) {
        for (const auto& selector : *selectorList)
            max = std::max(max, selectorSpecificity(selector, ignorePseudoElement).specificity);
    }
    return max;
}

SelectorSpecificity simpleSelectorSpecificity(const CSSSelector& simpleSelector, IgnorePseudoElement ignorePseudoElement)
{
    ASSERT_WITH_MESSAGE(!simpleSelector.isForPage(), "At the time of this writing, page selectors are not treated as real selectors that are matched. The value computed here only account for real selectors.");

    if (simpleSelector.isImplicit()) [[unlikely]]
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
            return maxSpecificity(simpleSelector.selectorList(), IgnorePseudoElement::Yes);
        case CSSSelector::PseudoClass::Not:
        case CSSSelector::PseudoClass::Has:
            return maxSpecificity(simpleSelector.selectorList());
        case CSSSelector::PseudoClass::Where:
            return 0;
        case CSSSelector::PseudoClass::NthChild:
        case CSSSelector::PseudoClass::NthLastChild:
        case CSSSelector::PseudoClass::Host:
            return SelectorSpecificityIncrement::ClassB + maxSpecificity(simpleSelector.selectorList());
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
        if (ignorePseudoElement == IgnorePseudoElement::Yes)
            return 0;
        switch (simpleSelector.pseudoElement()) {
        // Slotted only competes with other slotted selectors for specificity,
        // so whether we add the ClassC specificity shouldn't be observable.
        case CSSSelector::PseudoElement::Slotted:
            return maxSpecificity(simpleSelector.selectorList());
        case CSSSelector::PseudoElement::ViewTransitionGroup:
        case CSSSelector::PseudoElement::ViewTransitionImagePair:
        case CSSSelector::PseudoElement::ViewTransitionNew:
        case CSSSelector::PseudoElement::ViewTransitionOld:
            ASSERT(simpleSelector.stringList() && simpleSelector.stringList()->size());
            // Standalone universal selector gets 0 specificity.
            if (simpleSelector.stringList()->first() == starAtom() && simpleSelector.stringList()->size() == 1)
                return 0;
            break;
        default:
            break;
        }
        return SelectorSpecificityIncrement::ClassC;
    case CSSSelector::Match::HasScope:
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

    for (const CSSSelector* component = this; component; component = component->precedingInComplexSelector()) {
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

std::optional<PseudoElementType> CSSSelector::stylePseudoElementTypeFor(PseudoElement type)
{
    switch (type) {
    case PseudoElement::FirstLine:
        return PseudoElementType::FirstLine;
    case PseudoElement::FirstLetter:
        return PseudoElementType::FirstLetter;
    case PseudoElement::GrammarError:
        return PseudoElementType::GrammarError;
    case PseudoElement::SpellingError:
        return PseudoElementType::SpellingError;
    case PseudoElement::Selection:
        return PseudoElementType::Selection;
    case PseudoElement::TargetText:
        return PseudoElementType::TargetText;
    case PseudoElement::Highlight:
        return PseudoElementType::Highlight;
    case PseudoElement::Marker:
        return PseudoElementType::Marker;
    case PseudoElement::Backdrop:
        return PseudoElementType::Backdrop;
    case PseudoElement::Before:
        return PseudoElementType::Before;
    case PseudoElement::After:
        return PseudoElementType::After;
    case PseudoElement::Checkmark:
        return PseudoElementType::Checkmark;
    case PseudoElement::WebKitScrollbar:
        return PseudoElementType::WebKitScrollbar;
    case PseudoElement::WebKitScrollbarButton:
        return PseudoElementType::WebKitScrollbarButton;
    case PseudoElement::WebKitScrollbarCorner:
        return PseudoElementType::WebKitScrollbarCorner;
    case PseudoElement::WebKitScrollbarThumb:
        return PseudoElementType::WebKitScrollbarThumb;
    case PseudoElement::WebKitScrollbarTrack:
        return PseudoElementType::WebKitScrollbarTrack;
    case PseudoElement::WebKitScrollbarTrackPiece:
        return PseudoElementType::WebKitScrollbarTrackPiece;
    case PseudoElement::WebKitResizer:
        return PseudoElementType::WebKitResizer;
    case PseudoElement::ViewTransition:
        return PseudoElementType::ViewTransition;
    case PseudoElement::ViewTransitionGroup:
        return PseudoElementType::ViewTransitionGroup;
    case PseudoElement::ViewTransitionImagePair:
        return PseudoElementType::ViewTransitionImagePair;
    case PseudoElement::ViewTransitionOld:
        return PseudoElementType::ViewTransitionOld;
    case PseudoElement::ViewTransitionNew:
        return PseudoElementType::ViewTransitionNew;
    case PseudoElement::InternalWritingSuggestions:
        return PseudoElementType::InternalWritingSuggestions;
#if ENABLE(VIDEO)
    case PseudoElement::Cue:
#endif
    case PseudoElement::Slotted:
    case PseudoElement::Part:
    case PseudoElement::Picker:
    case PseudoElement::UserAgentPart:
    case PseudoElement::UserAgentPartLegacyAlias:
    case PseudoElement::WebKitUnknown:
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

std::optional<CSSSelector::PseudoElement> CSSSelector::parsePseudoElementName(StringView name, const CSSSelectorParserContext& context)
{
    if (name.isEmpty())
        return std::nullopt;

    auto type = findPseudoElementName(name);
    if (!type) {
        ASSERT_WITH_MESSAGE(!isUASheetBehavior(context.mode), "Unknown pseudo-element %s in user-agent stylesheet", name.toString().utf8().data());
        if (name.startsWithIgnoringASCIICase("-webkit-"_s))
            return PseudoElement::WebKitUnknown;
        return type;
    }

    if (!CSSSelector::isPseudoElementEnabled(*type, name, context))
        return std::nullopt;

    return *type;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
const CSSSelector* CSSSelector::firstInCompound() const
{
    auto* selector = this;
    while (!selector->isFirstInComplexSelector()) {
        if (selector->relation() != Relation::Subselector)
            break;
        ++selector;
    }
    return selector;
}

const CSSSelector* CSSSelector::lastInCompound() const
{
    auto* selector = this;
    while (!selector->isLastInComplexSelector()) {
        auto* next = selector - 1;
        if (next->relation() != Relation::Subselector)
            break;
        selector = next;
    }
    return selector;
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

const CSSSelector* CSSSelector::precedingInCompound() const
{
    if (relation() != Relation::Subselector)
        return nullptr;
    return precedingInComplexSelector();
}

static void appendPseudoClassFunctionTail(StringBuilder& builder, const CSSSelector* selector)
{
    switch (selector->pseudoClass()) {
    case CSSSelector::PseudoClass::Dir:
    case CSSSelector::PseudoClass::NthOfType:
    case CSSSelector::PseudoClass::NthLastOfType:
        builder.append(selector->argument(), ')');
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

}

static void appendPossiblyQuotedIdentifier(StringBuilder& builder, const PossiblyQuotedIdentifier& identifier)
{
    if (!identifier.wasQuoted)
        serializeIdentifier(identifier.identifier, builder);
    else
        serializeString(identifier.identifier, builder);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, PossiblyQuotedIdentifier identifier)
{
    StringBuilder builder;
    appendPossiblyQuotedIdentifier(builder, identifier);
    return ts << builder.toString();
}

static void appendCommaSeparatedPossiblyQuotedIdentifierList(StringBuilder& builder, const FixedVector<PossiblyQuotedIdentifier>& list)
{
    builder.append(interleave(list, appendPossiblyQuotedIdentifier, ", "_s));
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
        builder.append("n+"_s, b);
    }
}

String CSSSelector::selectorText(StringView separator, StringView rightSide) const
{
    auto serializeIdentifierOrStar = [](const AtomString& identifier, StringBuilder& builder) {
        if (identifier == starAtom())
            builder.append('*');
        else
            serializeIdentifier(identifier, builder);
    };

    StringBuilder builder;

    if (match() == Match::Tag && !m_tagIsForNamespaceRule) {
        if (auto& prefix = tagQName().prefix(); !prefix.isNull()) {
            serializeIdentifierOrStar(prefix, builder);
            builder.append('|');
        }
        serializeIdentifierOrStar(tagQName().localName(), builder);
    }

    const auto* selector = this;
    while (true) {
        if (selector->isImplicit()) {
            // Remove the space before the implicit selector.
            separator = separator.substring(1);
            break;
        }
        if (selector->match() == Match::Id) {
            builder.append('#');
            serializeIdentifier(selector->serializingValue(), builder);
        } else if (selector->match() == Match::NestingParent) {
            builder.append('&');
        } else if (selector->match() == Match::Class) {
            builder.append('.');
            serializeIdentifier(selector->serializingValue(), builder);
        } else if (selector->match() == Match::ForgivingUnknown || selector->match() == Match::ForgivingUnknownNestContaining) {
            builder.append(selector->value());
        } else if (selector->match() == Match::HasScope) {
            // Remove the space from the start to generate a relative selector string like in ":has(> foo)".
            return makeString(separator.substring(1), rightSide);
        } else if (selector->match() == Match::PseudoClass) {
            builder.append(selectorTextForPseudoClass(selector->pseudoClass()));

            // Handle serialization of functional variants.
            switch (selector->pseudoClass()) {
            case PseudoClass::Host:
                if (auto* selectorList = selector->selectorList()) {
                    builder.append('(');
                    selectorList->buildSelectorsText(builder);
                    builder.append(')');
                }
                break;
            case PseudoClass::Lang:
                builder.append('(');
                ASSERT_WITH_MESSAGE(selector->langList() && !selector->langList()->isEmpty(), "An empty :lang() is invalid and should never be generated by the parser.");
                appendCommaSeparatedPossiblyQuotedIdentifierList(builder, *selector->langList());
                builder.append(')');
                break;
            case PseudoClass::NthChild:
            case PseudoClass::NthLastChild:
                builder.append('(');
                outputNthChildAnPlusB(*selector, builder);
                if (auto* selectorList = selector->selectorList()) {
                    builder.append(" of "_s);
                    selectorList->buildSelectorsText(builder);
                }
                builder.append(')');
                break;
            case PseudoClass::Dir:
            case PseudoClass::NthOfType:
            case PseudoClass::NthLastOfType:
                builder.append('(');
                appendPseudoClassFunctionTail(builder, selector);
                break;
            case PseudoClass::Has:
            case PseudoClass::Is:
            case PseudoClass::Not:
            case PseudoClass::Where:
            case PseudoClass::WebKitAny: {
                builder.append('(');
                selector->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case PseudoClass::State:
                builder.append('(');
                serializeIdentifier(selector->argument(), builder);
                builder.append(')');
                break;
            case PseudoClass::Heading:
                if (auto* integerList = selector->integerList(); integerList && !integerList->isEmpty()) {
                    builder.append("("_s,
                        interleave(*integerList, [&](auto& builder, auto number) {
                            builder.append(number);
                        }, ", "_s),
                    ')');
                }
                break;
            case PseudoClass::ActiveViewTransitionType:
                ASSERT_WITH_MESSAGE(selector->stringList() && !selector->stringList()->isEmpty(), "An empty :active-view-transition-type() is invalid and should never be generated by the parser.");

                builder.append("("_s,
                    interleave(*selector->stringList(), [&](auto& builder, auto& partName) {
                        serializeIdentifier(partName, builder);
                    }, ", "_s),
                ')');
                break;
            default:
                ASSERT(!pseudoClassMayHaveArgument(selector->pseudoClass()), "Missing serialization for pseudo-class argument");
                break;
            }
        } else if (selector->match() == Match::PseudoElement) {
            switch (selector->pseudoElement()) {
            case PseudoElement::Slotted:
                builder.append("::slotted("_s);
                selector->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            case PseudoElement::Highlight:
            case PseudoElement::ViewTransitionGroup:
            case PseudoElement::ViewTransitionImagePair:
            case PseudoElement::ViewTransitionOld:
            case PseudoElement::ViewTransitionNew:
                // Name or universal selector always comes first, followed by classes.
                ASSERT(selector->stringList() && !selector->stringList()->isEmpty());

                builder.append("::"_s, selector->serializingValue(), '(',
                    interleave(*selector->stringList(), [&](auto& builder, auto& nameOrClass) {
                        serializeIdentifierOrStar(nameOrClass, builder);
                    }, '.'),
                ')');
                break;
            case PseudoElement::Part:
                ASSERT(selector->stringList() && !selector->stringList()->isEmpty());

                builder.append("::part("_s,
                    interleave(*selector->stringList(), [&](auto& builder, auto& partName) {
                        serializeIdentifier(partName, builder);
                    }, ' '),
                ')');
                break;
            case PseudoElement::Picker:
                ASSERT(selector->stringList() && !selector->stringList()->isEmpty());
                builder.append("::picker("_s, selector->stringList()->at(0), ')');
                break;
#if ENABLE(VIDEO)
            case PseudoElement::Cue: {
                builder.append("::cue"_s);
                if (auto* selectorList = selector->selectorList()) {
                    builder.append('(');
                    selectorList->buildSelectorsText(builder);
                    builder.append(')');
                }
                break;
            }
#endif
            default:
                ASSERT(!pseudoElementMayHaveArgument(selector->pseudoElement()), "Missing serialization for pseudo-element argument");
                builder.append("::"_s);
                serializeIdentifier(selector->serializingValue(), builder);
                break;
            }
        } else if (selector->isAttributeSelector()) {
            builder.append('[');
            if (auto& prefix = selector->attribute().prefix(); !prefix.isEmpty()) {
                serializeIdentifierOrStar(prefix, builder);
                builder.append('|');
            }
            serializeIdentifierOrStar(selector->attribute().localName(), builder);
            switch (selector->match()) {
            case Match::Exact:
                builder.append('=');
                break;
            case Match::Set:
                // set has no operator or value, just the attrName
                builder.append(']');
                break;
            case Match::List:
                builder.append("~="_s);
                break;
            case Match::Hyphen:
                builder.append("|="_s);
                break;
            case Match::Begin:
                builder.append("^="_s);
                break;
            case Match::End:
                builder.append("$="_s);
                break;
            case Match::Contain:
                builder.append("*="_s);
                break;
            default:
                break;
            }
            if (selector->match() != Match::Set) {
                serializeString(selector->serializingValue(), builder);
                if (selector->attributeValueMatchingIsCaseInsensitive())
                    builder.append(" i]"_s);
                else
                    builder.append(']');
            }
        } else if (selector->match() == Match::PagePseudoClass) {
            switch (selector->pagePseudoClass()) {
            case PagePseudoClass::First:
                builder.append(":first"_s);
                break;
            case PagePseudoClass::Left:
                builder.append(":left"_s);
                break;
            case PagePseudoClass::Right:
                builder.append(":right"_s);
                break;
            }
        }

        if (selector->relation() != Relation::Subselector || !selector->precedingInComplexSelector())
            break;
        selector = selector->precedingInComplexSelector();
    }

    builder.append(separator, rightSide);

    auto separatorTextForNestingRelative = [&] () -> String {
        switch (selector->relation()) {
        case Relation::Child:
            return "> "_s;
        case Relation::DirectAdjacent:
            return "+ "_s;
        case Relation::IndirectAdjacent:
            return "~ "_s;
        default:
            return { };
        }
    };

    if (auto* previousSelector = selector->precedingInComplexSelector()) {
        ASCIILiteral separator = ""_s;
        switch (selector->relation()) {
        case Relation::DescendantSpace:
            separator = " "_s;
            break;
        case Relation::Child:
            separator = " > "_s;
            break;
        case Relation::DirectAdjacent:
            separator = " + "_s;
            break;
        case Relation::IndirectAdjacent:
            separator = " ~ "_s;
            break;
        case Relation::Subselector:
            ASSERT_NOT_REACHED();
            break;
        case Relation::ShadowDescendant:
        case Relation::ShadowPartDescendant:
        case Relation::ShadowSlotted:
            break;
        }
        return previousSelector->selectorText(separator, builder);
    } else if (auto separatorText = separatorTextForNestingRelative(); !separatorText.isNull()) {
        // We have a separator but no preceding selector which can happen with implicit relative nesting selector
        return makeString(separatorText, builder.toString());
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

void CSSSelector::setIntegerList(FixedVector<int> integerList)
{
    createRareData();
    m_data.rareData->argumentList = WTF::move(integerList);
}

void CSSSelector::setStringList(FixedVector<AtomString> stringList)
{
    createRareData();
    m_data.rareData->argumentList = WTF::move(stringList);
}

void CSSSelector::setLangList(FixedVector<PossiblyQuotedIdentifier> langList)
{
    createRareData();
    m_data.rareData->argumentList = WTF::move(langList);
}

void CSSSelector::setSelectorList(std::unique_ptr<CSSSelectorList> selectorList)
{
    createRareData();
    m_data.rareData->selectorList = WTF::move(selectorList);
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
    , serializingValue(WTF::move(value))
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
    return adoptRef(*new RareData(WTF::move(value)));
}

bool CSSSelector::RareData::matchNth(int count)
{
    if (a > 0)
        return count >= b && !((count - b) % a);
    if (a < 0)
        return count <= b && !((b - count) % -a);
    return count == b;
}

bool CSSSelector::RareData::equals(const RareData& other) const
{
    if (selectorList || other.selectorList) {
        if (!selectorList || !other.selectorList || *selectorList != *other.selectorList)
            return false;
    }
    return matchingValue == other.matchingValue
        && serializingValue == other.serializingValue
        && a == other.a
        && b == other.b
        && attribute == other.attribute
        && argument == other.argument
        && argumentList == other.argumentList;
}

CSSSelector::CSSSelector(const CSSSelector& other)
    : m_relation(other.m_relation)
    , m_match(other.m_match)
    , m_pseudoType(other.m_pseudoType)
    , m_isFirstInComplexSelector(other.m_isFirstInComplexSelector)
    , m_isLastInComplexSelector(other.m_isLastInComplexSelector)
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

CSSSelector::CSSSelector(const CSSSelector& other, MutableSelectorCopyTag)
    : CSSSelector(other)
{
    // Restore the selector list bits to the initial state when copying to a MutableCSSSelector.
    m_isFirstInComplexSelector = true;
    m_isLastInComplexSelector = true;
}

bool CSSSelector::visitSimpleSelectors(VisitFunctor&& functor, VisitFunctionalPseudoClasses visitFunctionalPseudoClasses, VisitOnlySubject visitOnlySubject) const
{
    std::queue<const CSSSelector*> worklist;
    worklist.push(this);
    while (!worklist.empty()) {
        auto current = worklist.front();
        worklist.pop();

        // Effective C++ advices for this cast to deal with generic const/non-const member function.
        if (functor(*const_cast<CSSSelector*>(current)))
            return true;

        // Visit the selector list member (if any) recursively (such as: :has(<list>), :is(<list>),...)
        if (visitFunctionalPseudoClasses == VisitFunctionalPseudoClasses::Yes) {
            if (auto selectorList = current->selectorList()) {
                for (auto& selector : *selectorList)
                    worklist.push(&selector);
            }
        }

        // Visit the next simple selector
        if (auto next = current->precedingInComplexSelector()) {
            // We stop visiting at the end of the compound selector (= when relation is anything else than subselector) if we are in subject only mode.
            if (current->relation() != Relation::Subselector || visitOnlySubject != VisitOnlySubject::Yes)
                worklist.push(next);
        }
    }
    return false;
}

bool CSSSelector::hasExplicitNestingParent() const
{
    return visitSimpleSelectors([](const CSSSelector& selector) {
        if (selector.match() == Match::NestingParent)
            return true;

        if (selector.match() == Match::ForgivingUnknownNestContaining)
            return true;

        return false;
    } , VisitFunctionalPseudoClasses::Yes);
}

bool CSSSelector::hasExplicitPseudoClassScope() const
{
    return visitSimpleSelectors([] (const CSSSelector& selector) {
        if (selector.isScopePseudoClass())
            return true;
        return false;
    }, VisitFunctionalPseudoClasses::Yes);
}

bool CSSSelector::isHostPseudoClass() const
{
    return match() == Match::PseudoClass && pseudoClass() == PseudoClass::Host;
}

bool CSSSelector::isScopePseudoClass() const
{
    return match() == Match::PseudoClass && pseudoClass() == PseudoClass::Scope;
}

bool CSSSelector::hasScope() const
{
    return visitSimpleSelectors([] (auto& selector) {
        if (selector.isScopePseudoClass())
            return true;
        return false;
    });
}

bool complexSelectorCanMatchPseudoElement(const CSSSelector& complexSelector)
{
    const CSSSelector* selector = &complexSelector;
    do {
        if (selector->matchesPseudoElement())
            return true;

        // FIXME: This is probably unneeded as functional pseudo-classes can't contain valid pseudo elements.
        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            for (auto& subSelector : *selectorList) {
                if (complexSelectorCanMatchPseudoElement(subSelector))
                    return true;
            }
        }

        selector = selector->precedingInComplexSelector();
    } while (selector);
    return false;
}

bool complexSelectorMatchesElementBackedPseudoElement(const CSSSelector& complexSelector)
{
    auto isElementBacked = [](CSSSelector::PseudoElement pseudoElement) {
        switch (pseudoElement) {
        case CSSSelector::PseudoElement::Slotted:
        case CSSSelector::PseudoElement::Part:
        case CSSSelector::PseudoElement::Picker:
        case CSSSelector::PseudoElement::UserAgentPart:
        case CSSSelector::PseudoElement::UserAgentPartLegacyAlias:
            return true;
        default:
            return false;
        }
    };

    auto result = false;
    for (auto* simpleSelector = &complexSelector; simpleSelector; simpleSelector = simpleSelector->precedingInCompound()) {
        if (simpleSelector->matchesPseudoElement()) {
            if (!isElementBacked(simpleSelector->pseudoElement()))
                return false;
            result = true;
        }
    }
    return result;
}

bool CSSSelector::simpleSelectorEqual(const CSSSelector& other) const
{
    auto valuesEqual = [&] {
        if (m_hasRareData)
            return m_data.rareData->equals(*other.m_data.rareData);
        if (match() == Match::Tag)
            return *m_data.tagQName == *other.m_data.tagQName;
        return m_data.value == other.m_data.value;
    };

    // Relation and selector list bits are ignored.
    return m_match == other.m_match
        && m_pseudoType == other.m_pseudoType
        && m_hasRareData == other.m_hasRareData
        && m_tagIsForNamespaceRule == other.m_tagIsForNamespaceRule
        && m_caseInsensitiveAttributeValueMatching == other.m_caseInsensitiveAttributeValueMatching
        && m_isImplicit == other.m_isImplicit
        && valuesEqual();
}

bool isElementBackedPseudoElement(CSSSelector::PseudoElement pseudoElement)
{
    switch (pseudoElement) {
    case CSSSelector::PseudoElement::Part:
    case CSSSelector::PseudoElement::Picker:
    case CSSSelector::PseudoElement::Slotted:
    case CSSSelector::PseudoElement::UserAgentPart:
    case CSSSelector::PseudoElement::UserAgentPartLegacyAlias:
#if ENABLE(VIDEO)
    case CSSSelector::PseudoElement::Cue:
#endif
        return true;
    default:
        return false;
    }
}

static bool shouldSkipForEqualMode(const CSSSelector& simpleSelector, ComplexSelectorsEqualMode mode)
{
    if (mode == ComplexSelectorsEqualMode::IgnoreNonElementBackedPseudoElements)
        return simpleSelector.matchesPseudoElement() && !isElementBackedPseudoElement(simpleSelector.pseudoElement());
    return false;
};

bool complexSelectorsEqual(const CSSSelector& complexA, const CSSSelector& complexB, ComplexSelectorsEqualMode mode)
{
    auto aRelation = CSSSelector::Relation::Subselector;
    auto bRelation = CSSSelector::Relation::Subselector;

    for (auto a = &complexA, b = &complexB; a || b; a = a->precedingInComplexSelector(), b = b->precedingInComplexSelector()) {
        if (a && shouldSkipForEqualMode(*a, mode)) {
            aRelation = a->relation();
            a = a->precedingInComplexSelector();
        }
        if (b && shouldSkipForEqualMode(*b, mode)) {
            bRelation = b->relation();
            b = b->precedingInComplexSelector();
        }
        if (!a || !b)
            return a == b;
        if (aRelation != bRelation)
            return false;
        if (!a->simpleSelectorEqual(*b))
            return false;
        aRelation = a->relation();
        bRelation = b->relation();
    }
    return true;
}

static void addSimpleSelector(Hasher& hasher, const CSSSelector& simpleSelector)
{
    // This hash does try to include every possible thing in a selector.
    add(hasher, simpleSelector.match());

    switch (simpleSelector.match()) {
    case CSSSelector::Match::Tag:
        add(hasher, simpleSelector.tagQName());
        break;
    case CSSSelector::Match::PseudoClass:
        add(hasher, simpleSelector.pseudoClass());
        break;
    case CSSSelector::Match::PseudoElement:
        add(hasher, simpleSelector.pseudoElement());
        break;
    case CSSSelector::Match::Exact:
    case CSSSelector::Match::Set:
    case CSSSelector::Match::List:
    case CSSSelector::Match::Hyphen:
    case CSSSelector::Match::Begin:
    case CSSSelector::Match::End:
    case CSSSelector::Match::Contain:
        add(hasher, simpleSelector.attribute());
        add(hasher, simpleSelector.value());
        break;
    default:
        add(hasher, simpleSelector.value());
        break;
    }
    if (simpleSelector.selectorList())
        add(hasher, *simpleSelector.selectorList());
}

void addComplexSelector(Hasher& hasher, const CSSSelector& complexSelector, ComplexSelectorsEqualMode mode)
{
    auto relationToRight = CSSSelector::Relation::Subselector;
    for (auto simpleSelector = &complexSelector; simpleSelector; simpleSelector = simpleSelector->precedingInComplexSelector()) {
        if (shouldSkipForEqualMode(*simpleSelector, mode)) {
            relationToRight = simpleSelector->relation();
            continue;
        }
        add(hasher, relationToRight);
        addSimpleSelector(hasher, *simpleSelector);
        relationToRight = simpleSelector->relation();
    }
}

} // namespace WebCore
