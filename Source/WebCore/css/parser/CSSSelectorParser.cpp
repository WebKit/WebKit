// Copyright 2014-2017 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2020 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CSSSelectorParser.h"

#include "CSSParserEnum.h"
#include "CSSParserIdioms.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSSelectorInlines.h"
#include "CSSTokenizer.h"
#include "CommonAtomStrings.h"
#include "Document.h"
#include "MutableCSSSelector.h"
#include "PseudoElementIdentifier.h"
#include "SelectorPseudoTypeMap.h"
#include "UserAgentParts.h"
#include <memory>
#include <wtf/OptionSet.h>
#include <wtf/SetForScope.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

static AtomString serializeANPlusB(const std::pair<int, int>&);
static bool consumeANPlusB(CSSParserTokenRange&, std::pair<int, int>&);

static void appendImplicitSelectorPseudoClassScopeIfNeeded(MutableCSSSelector& selector)
{
    if ((!selector.hasExplicitNestingParent() && !selector.hasExplicitPseudoClassScope()) || selector.startsWithExplicitCombinator()) {
        auto scopeSelector = makeUnique<MutableCSSSelector>();
        scopeSelector->setMatch(CSSSelector::Match::PseudoClass);
        scopeSelector->setPseudoClass(CSSSelector::PseudoClass::Scope);
        scopeSelector->setImplicit();
        selector.prependInComplexSelectorAsRelative(WTFMove(scopeSelector));
    }
}

static void appendImplicitSelectorNestingParentIfNeeded(MutableCSSSelector& selector)
{
    if (!selector.hasExplicitNestingParent() || selector.startsWithExplicitCombinator()) {
        auto nestingParentSelector = makeUnique<MutableCSSSelector>();
        nestingParentSelector->setMatch(CSSSelector::Match::NestingParent);
        // https://drafts.csswg.org/css-nesting/#nesting
        // Spec: nested rules with relative selectors include the specificity of their implied nesting selector.
        selector.prependInComplexSelectorAsRelative(WTFMove(nestingParentSelector));
    }
}

static void appendImplicitSelectorIfNeeded(MutableCSSSelector& selector, CSSParserEnum::NestedContextType last)
{
    if (last == CSSParserEnum::NestedContextType::Style) {
        // For a rule inside a style rule, we had the implicit & if it's not there already or if it starts with a combinator > ~ +
        appendImplicitSelectorNestingParentIfNeeded(selector);
    } else if (last == CSSParserEnum::NestedContextType::Scope) {
        // For a rule inside a scope rule, we had the implicit ":scope" if there is no explicit & or :scope already
        appendImplicitSelectorPseudoClassScopeIfNeeded(selector);
    }
}

MutableCSSSelectorList parseMutableCSSSelectorList(CSSParserTokenRange& range, const CSSSelectorParserContext& context, StyleSheetContents* styleSheet, CSSParserEnum::NestedContext nestedContext, CSSParserEnum::IsForgiving isForgiving, CSSSelectorParser::DisallowPseudoElement disallowPseudoElement)
{
    CSSSelectorParser parser(context, styleSheet, nestedContext, disallowPseudoElement);
    range.consumeWhitespace();
    auto consume = [&] {
        if (nestedContext && isForgiving == CSSParserEnum::IsForgiving::No)
            return parser.consumeNestedSelectorList(range);
        if (nestedContext && isForgiving == CSSParserEnum::IsForgiving::Yes)
            return parser.consumeNestedComplexForgivingSelectorList(range);
        if (isForgiving == CSSParserEnum::IsForgiving::Yes)
            return parser.consumeComplexForgivingSelectorList(range);
        return parser.consumeComplexSelectorList(range);
    };
    auto result = consume();
    if (result.isEmpty() || !range.atEnd())
        return { };

    // In nested context, add the implicit :scope or &
    if (nestedContext) {
        for (auto& selector : result)
            appendImplicitSelectorIfNeeded(*selector, *nestedContext);
    }

    return result;
}

std::optional<CSSSelectorList> parseCSSSelectorList(CSSParserTokenRange range, const CSSSelectorParserContext& context, StyleSheetContents* styleSheet, CSSParserEnum::NestedContext nestedContext)
{
    auto result = parseMutableCSSSelectorList(range, context, styleSheet, nestedContext, CSSParserEnum::IsForgiving::No, CSSSelectorParser::DisallowPseudoElement::No);

    if (result.isEmpty() || !range.atEnd())
        return { };

    return CSSSelectorList { WTFMove(result) };
}

CSSSelectorParser::CSSSelectorParser(const CSSSelectorParserContext& context, StyleSheetContents* styleSheet, CSSParserEnum::NestedContext nestedContext, DisallowPseudoElement disallowPseudoElement)
    : m_context(context)
    , m_styleSheet(styleSheet)
    , m_nestedContext(nestedContext)
    , m_disallowPseudoElements(disallowPseudoElement == DisallowPseudoElement::Yes)
{
}

template <typename ConsumeSelector>
MutableCSSSelectorList CSSSelectorParser::consumeSelectorList(CSSParserTokenRange& range, ConsumeSelector&& consumeSelector)
{
    MutableCSSSelectorList selectorList;
    auto selector = consumeSelector(range);
    if (!selector)
        return { };

    selectorList.append(WTFMove(selector));
    while (!range.atEnd() && range.peek().type() == CommaToken) {
        range.consumeIncludingWhitespace();
        selector = consumeSelector(range);
        if (!selector)
            return { };
        selectorList.append(WTFMove(selector));
    }

    if (m_failedParsing)
        return { };

    return selectorList;
}

MutableCSSSelectorList CSSSelectorParser::consumeComplexSelectorList(CSSParserTokenRange& range)
{
    return consumeSelectorList(range, [&] (CSSParserTokenRange& range) {
        return consumeComplexSelector(range);
    });
}

MutableCSSSelectorList CSSSelectorParser::consumeRelativeSelectorList(CSSParserTokenRange& range)
{
    return consumeSelectorList(range, [&](CSSParserTokenRange& range) {
        return consumeRelativeScopeSelector(range);
    });
}

MutableCSSSelectorList CSSSelectorParser::consumeNestedSelectorList(CSSParserTokenRange& range)
{
    return consumeSelectorList(range, [&] (CSSParserTokenRange& range) {
        return consumeNestedComplexSelector(range);
    });
}

template<typename ConsumeSelector>
MutableCSSSelectorList CSSSelectorParser::consumeForgivingSelectorList(CSSParserTokenRange& range, ConsumeSelector&& consumeSelector)
{
    if (m_failedParsing)
        return { };

    MutableCSSSelectorList selectorList;

    auto consumeForgiving = [&] {
        auto initialRange = range;
        auto unknownSelector = [&] {
            auto unknownSelector = makeUnique<MutableCSSSelector>();
            auto unknownRange = initialRange.rangeUntil(range);
            unknownSelector->setMatch(CSSSelector::Match::ForgivingUnknown);
            // We store the complete range content for serialization.
            unknownSelector->setValue(AtomString { unknownRange.serialize() });
            // If the range contains a nesting selector, we mark this unknown selector as "nest containing" (it will be used during rule set building)
            for (auto& token : unknownRange) {
                if (token.type() == DelimiterToken && token.delimiter() == '&') {
                    unknownSelector->setMatch(CSSSelector::Match::ForgivingUnknownNestContaining);
                    break;
                }
            }
            return unknownSelector;
        };

        auto selector = consumeSelector(range);

        if (m_failedParsing && !m_disableForgivingParsing) {
            selector = { };
            m_failedParsing = false;
        }

        // Range is not over and next token is not a comma (means there is more to this selector) so this selector is unknown.
        // Consume until next comma and add the full range as an unknown selector to the selector list.
        if ((!range.atEnd() && range.peek().type() != CommaToken) || !selector) {
            while (!range.atEnd() && range.peek().type() != CommaToken)
                range.consume();
            if (!m_disableForgivingParsing)
                selectorList.append(unknownSelector());
            return;
        }

        selectorList.append(WTFMove(selector));
    };

    consumeForgiving();

    while (!range.atEnd() && range.peek().type() == CommaToken) {
        range.consumeIncludingWhitespace();
        consumeForgiving();
    }

    if (selectorList.isEmpty()) {
        if (m_disableForgivingParsing)
            m_failedParsing = true;
        return { };
    }

    return selectorList;
}

MutableCSSSelectorList CSSSelectorParser::consumeComplexForgivingSelectorList(CSSParserTokenRange& range)
{
    return consumeForgivingSelectorList(range, [&](CSSParserTokenRange& range) {
        return consumeComplexSelector(range);
    });
}

MutableCSSSelectorList CSSSelectorParser::consumeNestedComplexForgivingSelectorList(CSSParserTokenRange& range)
{
    return consumeForgivingSelectorList(range, [&] (CSSParserTokenRange& range) {
        return consumeNestedComplexSelector(range);
    });
}

std::optional<CSSSelectorList> CSSSelectorParser::parseSelectorList(const String& string, const CSSParserContext& context, StyleSheetContents* styleSheet, CSSParserEnum::NestedContext nestedContext)
{
    return parseCSSSelectorList(CSSTokenizer(string).tokenRange(), context, styleSheet, nestedContext);
}

bool CSSSelectorParser::supportsComplexSelector(CSSParserTokenRange range, const CSSSelectorParserContext& context)
{
    range.consumeWhitespace();
    CSSSelectorParser parser(context, nullptr);

    // @supports requires that all arguments parse.
    parser.m_disableForgivingParsing = true;

    auto mutableSelector = parser.consumeComplexSelector(range);

    if (parser.m_failedParsing || !range.atEnd() || !mutableSelector)
        return false;

    auto complexSelector = mutableSelector->releaseSelector();
    ASSERT(complexSelector);

    return !containsUnknownWebKitPseudoElements(*complexSelector);
}

MutableCSSSelectorList CSSSelectorParser::consumeCompoundSelectorList(CSSParserTokenRange& range)
{
    MutableCSSSelectorList selectorList;
    auto selector = consumeCompoundSelector(range);
    range.consumeWhitespace();
    if (!selector)
        return { };
    selectorList.append(WTFMove(selector));
    while (!range.atEnd() && range.peek().type() == CommaToken) {
        range.consumeIncludingWhitespace();
        selector = consumeCompoundSelector(range);
        range.consumeWhitespace();
        if (!selector)
            return { };
        selectorList.append(WTFMove(selector));
    }
    return selectorList;
}

static PossiblyQuotedIdentifier consumePossiblyQuotedIdentifier(CSSParserTokenRange& range)
{
    auto& token = range.consumeIncludingWhitespace();
    if (token.type() != IdentToken && token.type() != StringToken)
        return { };
    auto string = token.value();
    if (string.startsWith("--"_s))
        return { };
    return { string.toAtomString(), token.type() == StringToken };
}

static FixedVector<PossiblyQuotedIdentifier> consumeLangArgumentList(CSSParserTokenRange& range)
{
    Vector<PossiblyQuotedIdentifier> list;
    auto item = consumePossiblyQuotedIdentifier(range);
    if (item.isNull())
        return { };
    list.append(WTFMove(item));
    while (!range.atEnd() && range.peek().type() == CommaToken) {
        range.consumeIncludingWhitespace();
        item = consumePossiblyQuotedIdentifier(range);
        if (item.isNull())
            return { };
        list.append(WTFMove(item));
    }
    return FixedVector<PossiblyQuotedIdentifier> { WTFMove(list) };
}

static std::optional<FixedVector<AtomString>> consumeCommaSeparatedCustomIdentList(CSSParserTokenRange& range)
{
    Vector<AtomString> customIdents { };

    do {
        auto ident = CSSPropertyParserHelpers::consumeCustomIdentRaw(range);
        if (ident.isEmpty())
            return std::nullopt;

        customIdents.append(WTFMove(ident));
    } while (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range));

    if (!range.atEnd())
        return std::nullopt;

    // The parsing code guarantees there has to be at least one custom ident.
    ASSERT(!customIdents.isEmpty());

    return FixedVector<AtomString> { WTFMove(customIdents) };
}

enum class CompoundSelectorFlag {
    HasPseudoElementForRightmostCompound = 1 << 0,
};

static OptionSet<CompoundSelectorFlag> extractCompoundFlags(const MutableCSSSelector& simpleSelector, CSSParserMode parserMode)
{
    if (simpleSelector.match() != CSSSelector::Match::PseudoElement)
        return { };
    
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=161747
    // The UASheetMode check is a work-around to allow this selector in mediaControls(New).css:
    // input[type="range" i]::-webkit-media-slider-container > div {
    if (isUASheetBehavior(parserMode) && simpleSelector.pseudoElement() == CSSSelector::PseudoElement::UserAgentPart)
        return { };

    return CompoundSelectorFlag::HasPseudoElementForRightmostCompound;
}

static bool isDescendantCombinator(CSSSelector::Relation relation)
{
    return relation == CSSSelector::Relation::DescendantSpace;
}

std::unique_ptr<MutableCSSSelector> CSSSelectorParser::consumeNestedComplexSelector(CSSParserTokenRange& range)
{
    auto selector = consumeComplexSelector(range);
    if (selector)
        return selector;

    selector = consumeRelativeNestedSelector(range);
    if (selector)
        return selector;

    return nullptr;
}

std::unique_ptr<MutableCSSSelector> CSSSelectorParser::consumeComplexSelector(CSSParserTokenRange& range)
{
    auto selector = consumeCompoundSelector(range);
    if (!selector)
        return nullptr;

    OptionSet<CompoundSelectorFlag> previousCompoundFlags;

    for (auto* simple = selector.get(); simple && !previousCompoundFlags; simple = simple->precedingInComplexSelector())
        previousCompoundFlags = extractCompoundFlags(*simple, m_context.mode);

    while (true) {
        auto combinator = consumeCombinator(range);
        if (combinator == CSSSelector::Relation::Subselector)
            break;

        auto nextSelector = consumeCompoundSelector(range);
        if (!nextSelector)
            return isDescendantCombinator(combinator) ? WTFMove(selector) : nullptr;
        if (previousCompoundFlags.contains(CompoundSelectorFlag::HasPseudoElementForRightmostCompound))
            return nullptr;
        auto* end = nextSelector.get();
        auto compoundFlags = extractCompoundFlags(*end, m_context.mode);
        while (end->precedingInComplexSelector()) {
            end = end->precedingInComplexSelector();
            compoundFlags.add(extractCompoundFlags(*end, m_context.mode));
        }
        end->setRelation(combinator);
        previousCompoundFlags = compoundFlags;
        end->setPrecedingInComplexSelector(WTFMove(selector));

        selector = WTFMove(nextSelector);
    }

    return selector;
}

std::unique_ptr<MutableCSSSelector> CSSSelectorParser::consumeRelativeScopeSelector(CSSParserTokenRange& range)
{
    auto scopeCombinator = consumeCombinator(range);

    if (scopeCombinator == CSSSelector::Relation::Subselector)
        scopeCombinator = CSSSelector::Relation::DescendantSpace;

    auto selector = consumeComplexSelector(range);
    if (!selector)
        return nullptr;

    auto* end = selector.get();
    while (end->precedingInComplexSelector())
        end = end->precedingInComplexSelector();

    auto scopeSelector = makeUnique<MutableCSSSelector>();
    scopeSelector->setMatch(CSSSelector::Match::HasScope);

    end->setRelation(scopeCombinator);
    end->setPrecedingInComplexSelector(WTFMove(scopeSelector));

    return selector;
}

std::unique_ptr<MutableCSSSelector> CSSSelectorParser::consumeRelativeNestedSelector(CSSParserTokenRange& range)
{
    auto scopeCombinator = consumeCombinator(range);

    // Nesting should only work with ~ > + combinators in this function. 
    // The descendant combinator is handled in another code path.
    if (scopeCombinator != CSSSelector::Relation::DirectAdjacent && scopeCombinator != CSSSelector::Relation::IndirectAdjacent && scopeCombinator != CSSSelector::Relation::Child)
        return nullptr;
    
    auto selector = consumeComplexSelector(range);
    if (!selector)
        return nullptr;

    auto last = selector->leftmostSimpleSelector();
    last->setRelation(scopeCombinator);

    return selector;
}

static bool isScrollbarPseudoClass(CSSSelector::PseudoClass pseudo)
{
    switch (pseudo) {
    case CSSSelector::PseudoClass::Enabled:
    case CSSSelector::PseudoClass::Disabled:
    case CSSSelector::PseudoClass::Hover:
    case CSSSelector::PseudoClass::Active:
    case CSSSelector::PseudoClass::Horizontal:
    case CSSSelector::PseudoClass::Vertical:
    case CSSSelector::PseudoClass::Decrement:
    case CSSSelector::PseudoClass::Increment:
    case CSSSelector::PseudoClass::Start:
    case CSSSelector::PseudoClass::End:
    case CSSSelector::PseudoClass::DoubleButton:
    case CSSSelector::PseudoClass::SingleButton:
    case CSSSelector::PseudoClass::NoButton:
    case CSSSelector::PseudoClass::CornerPresent:
    case CSSSelector::PseudoClass::WindowInactive:
        return true;
    default:
        return false;
    }
}

static bool isUserActionPseudoClass(CSSSelector::PseudoClass pseudo)
{
    switch (pseudo) {
    case CSSSelector::PseudoClass::Hover:
    case CSSSelector::PseudoClass::Focus:
    case CSSSelector::PseudoClass::Active:
    case CSSSelector::PseudoClass::FocusVisible:
    case CSSSelector::PseudoClass::FocusWithin:
        return true;
    default:
        return false;
    }
}

static bool isPseudoClassValidAfterPseudoElement(CSSSelector::PseudoClass pseudoClass, CSSSelector::PseudoElement compoundPseudoElement)
{
    // FIXME: https://drafts.csswg.org/selectors-4/#pseudo-element-states states all pseudo-elements
    // can be followed by isUserActionPseudoClass().
    // Validity of these is determined by their content.
    if (isLogicalCombinationPseudoClass(pseudoClass))
        return true;

    switch (compoundPseudoElement) {
    case CSSSelector::PseudoElement::Part:
        return !isTreeStructuralPseudoClass(pseudoClass);
    case CSSSelector::PseudoElement::Slotted:
        return false;
    case CSSSelector::PseudoElement::WebKitResizer:
    case CSSSelector::PseudoElement::WebKitScrollbar:
    case CSSSelector::PseudoElement::WebKitScrollbarCorner:
    case CSSSelector::PseudoElement::WebKitScrollbarButton:
    case CSSSelector::PseudoElement::WebKitScrollbarThumb:
    case CSSSelector::PseudoElement::WebKitScrollbarTrack:
    case CSSSelector::PseudoElement::WebKitScrollbarTrackPiece:
        return isScrollbarPseudoClass(pseudoClass);
    case CSSSelector::PseudoElement::Selection:
        return pseudoClass == CSSSelector::PseudoClass::WindowInactive;
    case CSSSelector::PseudoElement::ViewTransitionGroup:
    case CSSSelector::PseudoElement::ViewTransitionImagePair:
    case CSSSelector::PseudoElement::ViewTransitionNew:
    case CSSSelector::PseudoElement::ViewTransitionOld:
        return pseudoClass == CSSSelector::PseudoClass::OnlyChild;
    case CSSSelector::PseudoElement::UserAgentPart:
    case CSSSelector::PseudoElement::UserAgentPartLegacyAlias:
    case CSSSelector::PseudoElement::WebKitUnknown:
        return isUserActionPseudoClass(pseudoClass);
    default:
        return false;
    }
}

static bool isTreeAbidingPseudoElement(CSSSelector::PseudoElement pseudoElement)
{
    switch (pseudoElement) {
    // FIXME: This list should also include ::placeholder and ::file-selector-button
    case CSSSelector::PseudoElement::Before:
    case CSSSelector::PseudoElement::After:
    case CSSSelector::PseudoElement::Marker:
    case CSSSelector::PseudoElement::Backdrop:
        return true;
    default:
        return false;
    }
}

static bool isSimpleSelectorValidAfterPseudoElement(const MutableCSSSelector& simpleSelector, const MutableCSSSelector& compoundPseudoElement)
{
    if (compoundPseudoElement.pseudoElement() == CSSSelector::PseudoElement::UserAgentPart && compoundPseudoElement.value() == UserAgentParts::detailsContent()) {
        if (simpleSelector.match() == CSSSelector::Match::PseudoElement)
            return true;
    }
    if (compoundPseudoElement.pseudoElement() == CSSSelector::PseudoElement::Part) {
        if (simpleSelector.match() == CSSSelector::Match::PseudoElement && simpleSelector.pseudoElement() != CSSSelector::PseudoElement::Part)
            return true;
    }
    if (compoundPseudoElement.pseudoElement() == CSSSelector::PseudoElement::Slotted) {
        if (simpleSelector.match() == CSSSelector::Match::PseudoElement && isTreeAbidingPseudoElement(simpleSelector.pseudoElement()))
            return true;
    }
    if (simpleSelector.match() != CSSSelector::Match::PseudoClass)
        return false;

    return isPseudoClassValidAfterPseudoElement(simpleSelector.pseudoClass(), compoundPseudoElement.pseudoElement());
}

static bool atEndIgnoringWhitespace(CSSParserTokenRange range)
{
    range.consumeWhitespace();
    return range.atEnd();
}

std::unique_ptr<MutableCSSSelector> CSSSelectorParser::consumeCompoundSelector(CSSParserTokenRange& range)
{
    ASSERT(!m_precedingPseudoElement || m_disallowPseudoElements);

    std::unique_ptr<MutableCSSSelector> compoundSelector;

    AtomString namespacePrefix;
    AtomString elementName;
    const bool hasName = consumeName(range, elementName, namespacePrefix);
    if (!hasName) {
        compoundSelector = consumeSimpleSelector(range);
        if (!compoundSelector)
            return nullptr;
        if (compoundSelector->match() == CSSSelector::Match::PseudoElement)
            m_precedingPseudoElement = compoundSelector.get();
    }

    while (auto simpleSelector = consumeSimpleSelector(range)) {
        if (simpleSelector->match() == CSSSelector::Match::PseudoElement)
            m_precedingPseudoElement = simpleSelector.get();

        if (compoundSelector)
            compoundSelector->prependInComplexSelector(CSSSelector::Relation::Subselector, WTFMove(simpleSelector));
        else
            compoundSelector = WTFMove(simpleSelector);
    }

    if (!m_disallowPseudoElements)
        m_precedingPseudoElement = nullptr;

    // While inside a nested selector like :is(), the default namespace shall be ignored when [1]:
    // * The compound selector represents the subject [2], and
    // * The compound selector does not contain a type/universal selector.
    //
    // [1] https://drafts.csswg.org/selectors/#matches
    // [2] https://drafts.csswg.org/selectors/#selector-subject
    SetForScope ignoreDefaultNamespace(m_ignoreDefaultNamespace, m_resistDefaultNamespace && !hasName && atEndIgnoringWhitespace(range));
    if (!compoundSelector) {
        AtomString namespaceURI = determineNamespace(namespacePrefix);
        if (namespaceURI.isNull()) {
            m_failedParsing = true;
            return nullptr;
        }
        if (namespaceURI == defaultNamespace())
            namespacePrefix = nullAtom();
        
        return makeUnique<MutableCSSSelector>(QualifiedName(namespacePrefix, elementName, namespaceURI));
    }
    prependTypeSelectorIfNeeded(namespacePrefix, elementName, *compoundSelector);
    return splitCompoundAtImplicitShadowCrossingCombinator(WTFMove(compoundSelector), m_context);
}

std::unique_ptr<MutableCSSSelector> CSSSelectorParser::consumeSimpleSelector(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    std::unique_ptr<MutableCSSSelector> selector;
    if (token.type() == HashToken)
        selector = consumeId(range);
    else if (token.type() == DelimiterToken && token.delimiter() == '.')
        selector = consumeClass(range);
    else if (token.type() == DelimiterToken && token.delimiter() == '&')
        selector = consumeNesting(range);
    else if (token.type() == LeftBracketToken)
        selector = consumeAttribute(range);
    else if (token.type() == ColonToken)
        selector = consumePseudo(range);
    else
        return nullptr;

    if (!selector) {
        m_failedParsing = true;
        return nullptr;
    }

    if (m_precedingPseudoElement) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=161747
        // The UASheetMode check is a work-around to allow this selector in mediaControls(New).css:
        // video::-webkit-media-text-track-region-container.scrolling
        if (!isUASheetBehavior(m_context.mode) && !isSimpleSelectorValidAfterPseudoElement(*selector, *m_precedingPseudoElement))
            m_failedParsing = true;
    }

    return selector;
}

bool CSSSelectorParser::consumeName(CSSParserTokenRange& range, AtomString& name, AtomString& namespacePrefix)
{
    name = nullAtom();
    namespacePrefix = nullAtom();

    const CSSParserToken& firstToken = range.peek();
    if (firstToken.type() == IdentToken) {
        name = firstToken.value().toAtomString();
        range.consume();
    } else if (firstToken.type() == DelimiterToken && firstToken.delimiter() == '*') {
        name = starAtom();
        range.consume();
    } else if (firstToken.type() == DelimiterToken && firstToken.delimiter() == '|') {
        // This is an empty namespace, which'll get assigned this value below
        name = emptyAtom();
    } else
        return false;

    if (range.peek().type() != DelimiterToken || range.peek().delimiter() != '|')
        return true;

    namespacePrefix = name;
    if (range.peek(1).type() == IdentToken) {
        range.consume();
        name = range.consume().value().toAtomString();
    } else if (range.peek(1).type() == DelimiterToken && range.peek(1).delimiter() == '*') {
        range.consume();
        range.consume();
        name = starAtom();
    } else {
        name = nullAtom();
        namespacePrefix = nullAtom();
        return false;
    }

    return true;
}

std::unique_ptr<MutableCSSSelector> CSSSelectorParser::consumeId(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == HashToken);
    if (range.peek().getHashTokenType() != HashTokenId)
        return nullptr;

    auto selector = makeUnique<MutableCSSSelector>();
    selector->setMatch(CSSSelector::Match::Id);

    auto token = range.consume();
    selector->setValue(token.value().toAtomString(), m_context.mode == HTMLQuirksMode);
    return selector;
}

std::unique_ptr<MutableCSSSelector> CSSSelectorParser::consumeClass(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == DelimiterToken);
    ASSERT(range.peek().delimiter() == '.');
    range.consume();
    if (range.peek().type() != IdentToken)
        return nullptr;

    auto selector = makeUnique<MutableCSSSelector>();
    selector->setMatch(CSSSelector::Match::Class);

    auto token = range.consume();
    selector->setValue(token.value().toAtomString(), m_context.mode == HTMLQuirksMode);

    return selector;
}

std::unique_ptr<MutableCSSSelector> CSSSelectorParser::consumeNesting(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == DelimiterToken);
    ASSERT(range.peek().delimiter() == '&');
    range.consume();

    auto selector = makeUnique<MutableCSSSelector>();
    selector->setMatch(CSSSelector::Match::NestingParent);
    
    return selector;
}

std::unique_ptr<MutableCSSSelector> CSSSelectorParser::consumeAttribute(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == LeftBracketToken);
    CSSParserTokenRange block = range.consumeBlock();
    block.consumeWhitespace();

    AtomString namespacePrefix;
    AtomString attributeName;
    if (!consumeName(block, attributeName, namespacePrefix))
        return nullptr;
    block.consumeWhitespace();

    AtomString namespaceURI = determineNamespace(namespacePrefix);
    if (namespaceURI.isNull())
        return nullptr;

    QualifiedName qualifiedName = namespacePrefix.isNull()
        ? QualifiedName(nullAtom(), attributeName, nullAtom())
        : QualifiedName(namespacePrefix, attributeName, namespaceURI);

    auto selector = makeUnique<MutableCSSSelector>();

    if (block.atEnd()) {
        selector->setAttribute(qualifiedName, CSSSelector::CaseSensitive);
        selector->setMatch(CSSSelector::Match::Set);
        return selector;
    }

    selector->setMatch(consumeAttributeMatch(block));

    const CSSParserToken& attributeValue = block.consumeIncludingWhitespace();
    if (attributeValue.type() != IdentToken && attributeValue.type() != StringToken)
        return nullptr;
    selector->setValue(attributeValue.value().toAtomString());

    selector->setAttribute(qualifiedName, consumeAttributeFlags(block));

    if (!block.atEnd())
        return nullptr;
    return selector;
}

std::unique_ptr<MutableCSSSelector> CSSSelectorParser::consumePseudo(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == ColonToken);
    range.consume();

    int colons = 1;
    if (range.peek().type() == ColonToken) {
        range.consume();
        colons++;
    }

    const CSSParserToken& token = range.peek();
    if (token.type() != IdentToken && token.type() != FunctionToken)
        return nullptr;

    std::unique_ptr<MutableCSSSelector> selector;

    if (colons == 1)
        selector = MutableCSSSelector::parsePseudoClassSelector(token.value(), m_context);
    else {
        selector = MutableCSSSelector::parsePseudoElementSelector(token.value(), m_context);
#if ENABLE(VIDEO)
        if (m_context.webkitMediaTextTrackDisplayQuirkEnabled
            && token.type() == IdentToken
            && selector
            && selector->match() == CSSSelector::Match::PseudoElement
            && selector->pseudoElement() == CSSSelector::PseudoElement::UserAgentPart
            && selector->value() == UserAgentParts::webkitMediaTextTrackDisplay()) {
            // This quirk will convert a `::-webkit-media-text-track-display` selector
            // into a `::-webkit-media-text-track-container` selector, so
            // that websites which were previously using that selector to move cues
            // with transform:translateY() rules can continue to do so, without hitting
            // the PropertyAllowList::Cue restriction.
            selector->setValue(UserAgentParts::webkitMediaTextTrackContainer());
        }

        // Treat the ident version of cue as PseudoElement::UserAgentPart.
        if (token.type() == IdentToken && selector && selector->match() == CSSSelector::Match::PseudoElement && selector->pseudoElement() == CSSSelector::PseudoElement::Cue)
            selector->setPseudoElement(CSSSelector::PseudoElement::UserAgentPart);
#endif
    }

    if (!selector)
        return nullptr;

    // Pseudo-elements are not allowed inside pseudo-classes or pseudo-elements.
    if (selector->match() == CSSSelector::Match::PseudoElement && m_disallowPseudoElements)
        return nullptr;
    SetForScope disallowPseudoElementsScope(m_disallowPseudoElements, true);

    if (token.type() == IdentToken) {
        range.consume();
        if ((selector->match() == CSSSelector::Match::PseudoElement && CSSSelector::pseudoElementRequiresArgument(selector->pseudoElement()))
            || (selector->match() == CSSSelector::Match::PseudoClass && CSSSelector::pseudoClassRequiresArgument(selector->pseudoClass())))
            return nullptr;
        return selector;
    }

    ASSERT(token.type() == FunctionToken);
    CSSParserTokenRange block = range.consumeBlock();
    block.consumeWhitespace();

    if (selector->match() == CSSSelector::Match::PseudoClass) {
        switch (selector->pseudoClass()) {
        case CSSSelector::PseudoClass::Not: {
            SetForScope resistDefaultNamespace(m_resistDefaultNamespace, true);
            auto selectorList = consumeComplexSelectorList(block);
            if (selectorList.size() < 1 || !block.atEnd())
                return nullptr;
            selector->setSelectorList(makeUnique<CSSSelectorList>(WTFMove(selectorList)));
            return selector;
        }
        case CSSSelector::PseudoClass::NthChild:
        case CSSSelector::PseudoClass::NthLastChild:
        case CSSSelector::PseudoClass::NthOfType:
        case CSSSelector::PseudoClass::NthLastOfType: {
            std::pair<int, int> ab;
            if (!consumeANPlusB(block, ab))
                return nullptr;
            block.consumeWhitespace();
            // FIXME: We should be able to do this lazily. See: https://bugs.webkit.org/show_bug.cgi?id=217149
            selector->setArgument(serializeANPlusB(ab));
            if (!block.atEnd()) {
                auto type = selector->pseudoClass();
                if (type == CSSSelector::PseudoClass::NthOfType || type == CSSSelector::PseudoClass::NthLastOfType)
                    return nullptr;
                if (block.peek().type() != IdentToken)
                    return nullptr;
                const CSSParserToken& ident = block.consume();
                if (!equalLettersIgnoringASCIICase(ident.value(), "of"_s))
                    return nullptr;
                block.consumeWhitespace();
                auto selectorList = consumeComplexSelectorList(block);
                if (selectorList.isEmpty() || !block.atEnd())
                    return nullptr;
                selector->setSelectorList(makeUnique<CSSSelectorList>(WTFMove(selectorList)));
            }
            selector->setNth(ab.first, ab.second);
            return selector;
        }
        case CSSSelector::PseudoClass::Lang: {
            auto list = consumeLangArgumentList(block);
            if (list.isEmpty() || !block.atEnd())
                return nullptr;
            selector->setLangList(WTFMove(list));
            return selector;
        }
        case CSSSelector::PseudoClass::Is:
        case CSSSelector::PseudoClass::Where:
        case CSSSelector::PseudoClass::WebKitAny: {
            SetForScope resistDefaultNamespace(m_resistDefaultNamespace, true);
            auto selectorList = makeUnique<CSSSelectorList>();
            auto consumedBlock = consumeComplexForgivingSelectorList(block);
            if (!block.atEnd())
                return nullptr;
            if (consumedBlock.isEmpty())
                *selectorList = CSSSelectorList { };
            else
                *selectorList = CSSSelectorList { WTFMove(consumedBlock) };
            selector->setSelectorList(WTFMove(selectorList));
            return selector;
        }
        case CSSSelector::PseudoClass::Host: {
            auto innerSelector = consumeCompoundSelector(block);
            block.consumeWhitespace();
            if (!innerSelector || !block.atEnd())
                return nullptr;
            selector->adoptSelectorVector(MutableCSSSelectorList::from(WTFMove(innerSelector)));
            return selector;
        }
        case CSSSelector::PseudoClass::Has: {
            if (m_disallowHasPseudoClass)
                return nullptr;
            SetForScope resistDefaultNamespace(m_resistDefaultNamespace, true);
            SetForScope disallowNestedHas(m_disallowHasPseudoClass, true);
            auto selectorList = consumeRelativeSelectorList(block);
            if (selectorList.isEmpty() || !block.atEnd())
                return nullptr;
            selector->setSelectorList(makeUnique<CSSSelectorList>(WTFMove(selectorList)));
            return selector;
        }
        case CSSSelector::PseudoClass::Dir: {
            const CSSParserToken& ident = block.consumeIncludingWhitespace();
            if (ident.type() != IdentToken || !block.atEnd())
                return nullptr;
            selector->setArgument(ident.value().toAtomString());
            return selector;
        }
        case CSSSelector::PseudoClass::State: {
            const CSSParserToken& ident = block.consumeIncludingWhitespace();
            if (ident.type() != IdentToken || !block.atEnd())
                return nullptr;
            selector->setArgument(ident.value().toAtomString());
            return selector;
        }
        case CSSSelector::PseudoClass::ActiveViewTransitionType: {
            auto typeList = consumeCommaSeparatedCustomIdentList(block);
            if (!typeList)
                return nullptr;
            selector->setArgumentList(WTFMove(*typeList));
            return selector;
        }
        default:
            break;
        }
    }
    
    if (selector->match() == CSSSelector::Match::PseudoElement) {
        switch (selector->pseudoElement()) {
#if ENABLE(VIDEO)
        case CSSSelector::PseudoElement::Cue: {
            auto selectorList = consumeCompoundSelectorList(block);
            if (selectorList.isEmpty() || !block.atEnd())
                return nullptr;
            selector->setSelectorList(makeUnique<CSSSelectorList>(WTFMove(selectorList)));
            return selector;
        }
#endif
        case CSSSelector::PseudoElement::Highlight: {
            auto& ident = block.consumeIncludingWhitespace();
            if (ident.type() != IdentToken || !block.atEnd())
                return nullptr;
            selector->setArgumentList({ { ident.value().toAtomString() } });
            return selector;
        }

        case CSSSelector::PseudoElement::ViewTransitionGroup:
        case CSSSelector::PseudoElement::ViewTransitionImagePair:
        case CSSSelector::PseudoElement::ViewTransitionOld:
        case CSSSelector::PseudoElement::ViewTransitionNew: {
            Vector<AtomString> nameAndClasses;

            // Check for implicit universal selector.
            if (block.peek().type() == DelimiterToken && block.peek().delimiter() == '.')
                nameAndClasses.append(starAtom());

            // Parse name or explicit universal selector.
            if (nameAndClasses.isEmpty()) {
                const auto& ident = block.consume();
                if (ident.type() == IdentToken && isValidCustomIdentifier(ident.id()))
                    nameAndClasses.append(ident.value().toAtomString());
                else if (ident.type() == DelimiterToken && ident.delimiter() == '*')
                    nameAndClasses.append(starAtom());
                else
                    return nullptr;
            }

            // Parse classes.
            while (!block.atEnd() && !CSSTokenizer::isWhitespace(block.peek().type())) {
                if (block.peek().type() != DelimiterToken || block.consume().delimiter() != '.')
                    return nullptr;

                if (block.peek().type() != IdentToken)
                    return nullptr;
                nameAndClasses.append({ block.consume().value().toAtomString() });
            }

            block.consumeWhitespace();

            if (!block.atEnd())
                return nullptr;

            selector->setArgumentList(FixedVector<AtomString> { WTFMove(nameAndClasses) });
            return selector;
        }

        case CSSSelector::PseudoElement::Part: {
            Vector<AtomString> argumentList;
            do {
                auto& ident = block.consumeIncludingWhitespace();
                if (ident.type() != IdentToken)
                    return nullptr;
                argumentList.append(ident.value().toAtomString());
            } while (!block.atEnd());
            selector->setArgumentList(FixedVector<AtomString> { WTFMove(argumentList) });
            return selector;
        }
        case CSSSelector::PseudoElement::Slotted: {
            auto innerSelector = consumeCompoundSelector(block);
            block.consumeWhitespace();
            if (!innerSelector || !block.atEnd())
                return nullptr;
            selector->adoptSelectorVector(MutableCSSSelectorList::from(WTFMove(innerSelector)));
            return selector;
        }
        default:
            break;
        }
    }

    return nullptr;
}

CSSSelector::Relation CSSSelectorParser::consumeCombinator(CSSParserTokenRange& range)
{
    auto fallbackResult = CSSSelector::Relation::Subselector;
    while (CSSTokenizer::isWhitespace(range.peek().type())) {
        range.consume();
        fallbackResult = CSSSelector::Relation::DescendantSpace;
    }

    if (range.peek().type() != DelimiterToken)
        return fallbackResult;

    char16_t delimiter = range.peek().delimiter();

    if (delimiter == '+' || delimiter == '~' || delimiter == '>') {
        range.consumeIncludingWhitespace();
        if (delimiter == '+')
            return CSSSelector::Relation::DirectAdjacent;
        if (delimiter == '~')
            return CSSSelector::Relation::IndirectAdjacent;
        return CSSSelector::Relation::Child;
    }

    return fallbackResult;
}

CSSSelector::Match CSSSelectorParser::consumeAttributeMatch(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.consumeIncludingWhitespace();
    switch (token.type()) {
    case IncludeMatchToken:
        return CSSSelector::Match::List;
    case DashMatchToken:
        return CSSSelector::Match::Hyphen;
    case PrefixMatchToken:
        return CSSSelector::Match::Begin;
    case SuffixMatchToken:
        return CSSSelector::Match::End;
    case SubstringMatchToken:
        return CSSSelector::Match::Contain;
    case DelimiterToken:
        if (token.delimiter() == '=')
            return CSSSelector::Match::Exact;
        [[fallthrough]];
    default:
        m_failedParsing = true;
        return CSSSelector::Match::Exact;
    }
}

CSSSelector::AttributeMatchType CSSSelectorParser::consumeAttributeFlags(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return CSSSelector::CaseSensitive;
    const CSSParserToken& flag = range.consumeIncludingWhitespace();
    if (equalLettersIgnoringASCIICase(flag.value(), "i"_s))
        return CSSSelector::CaseInsensitive;
    m_failedParsing = true;
    return CSSSelector::CaseSensitive;
}

// <an+b> token sequences have special serialization rules: https://www.w3.org/TR/css-syntax-3/#serializing-anb
static AtomString serializeANPlusB(const std::pair<int, int>& ab)
{
    if (!ab.first)
        return AtomString::number(ab.second);

    StringBuilder builder;

    if (ab.first == -1)
        builder.append('-');
    else if (ab.first != 1)
        builder.append(ab.first);
    builder.append('n');

    if (ab.second) {
        if (ab.second > 0)
            builder.append('+');
        builder.append(ab.second);
    }

    return builder.toAtomString();
}

static bool consumeANPlusB(CSSParserTokenRange& range, std::pair<int, int>& result)
{
    const CSSParserToken& token = range.consume();
    if (token.type() == NumberToken && token.numericValueType() == IntegerValueType) {
        result = std::make_pair(0, static_cast<int>(token.numericValue()));
        return true;
    }
    if (token.type() == IdentToken) {
        if (equalLettersIgnoringASCIICase(token.value(), "odd"_s)) {
            result = std::make_pair(2, 1);
            return true;
        }
        if (equalLettersIgnoringASCIICase(token.value(), "even"_s)) {
            result = std::make_pair(2, 0);
            return true;
        }
    }

    // The 'n' will end up as part of an ident or dimension. For a valid <an+b>,
    // this will store a string of the form 'n', 'n-', or 'n-123'.
    StringView nString;

    if (token.type() == DelimiterToken && token.delimiter() == '+' && range.peek().type() == IdentToken) {
        result.first = 1;
        nString = range.consume().value();
    } else if (token.type() == DimensionToken && token.numericValueType() == IntegerValueType) {
        result.first = token.numericValue();
        nString = token.unitString();
    } else if (token.type() == IdentToken) {
        if (token.value()[0] == '-') {
            result.first = -1;
            nString = token.value().substring(1);
        } else {
            result.first = 1;
            nString = token.value();
        }
    }

    range.consumeWhitespace();

    if (nString.isEmpty() || !isASCIIAlphaCaselessEqual(nString[0], 'n'))
        return false;
    if (nString.length() > 1 && nString[1] != '-')
        return false;

    if (nString.length() > 2) {
        auto parsedNumber = parseInteger<int>(nString.substring(1));
        result.second = parsedNumber.value_or(0);
        return parsedNumber.has_value();
    }

    NumericSign sign = nString.length() == 1 ? NoSign : MinusSign;
    if (sign == NoSign && range.peek().type() == DelimiterToken) {
        char delimiterSign = range.consumeIncludingWhitespace().delimiter();
        if (delimiterSign == '+')
            sign = PlusSign;
        else if (delimiterSign == '-')
            sign = MinusSign;
        else
            return false;
    }

    if (sign == NoSign && range.peek().type() != NumberToken) {
        result.second = 0;
        return true;
    }

    const CSSParserToken& b = range.consume();
    if (b.type() != NumberToken || b.numericValueType() != IntegerValueType)
        return false;
    if ((b.numericSign() == NoSign) == (sign == NoSign))
        return false;
    result.second = b.numericValue();
    if (sign == MinusSign)
        result.second = -result.second;
    return true;
}

const AtomString& CSSSelectorParser::defaultNamespace() const
{
    if (!m_styleSheet || m_ignoreDefaultNamespace)
        return starAtom();
    return m_styleSheet->defaultNamespace();
}

const AtomString& CSSSelectorParser::determineNamespace(const AtomString& prefix)
{
    if (prefix.isNull())
        return defaultNamespace();
    if (prefix.isEmpty())
        return emptyAtom(); // No namespace. If an element/attribute has a namespace, we won't match it.
    if (prefix == starAtom())
        return starAtom(); // We'll match any namespace.
    if (!m_styleSheet)
        return nullAtom(); // Cannot resolve prefix to namespace without a stylesheet, syntax error.
    return m_styleSheet->namespaceURIFromPrefix(prefix);
}

void CSSSelectorParser::prependTypeSelectorIfNeeded(const AtomString& namespacePrefix, const AtomString& elementName, MutableCSSSelector& compoundSelector)
{
    bool isShadowDOM = compoundSelector.needsImplicitShadowCombinatorForMatching();
    
    if (elementName.isNull() && defaultNamespace() == starAtom() && !isShadowDOM)
        return;

    AtomString determinedElementName = elementName.isNull() ? starAtom() : elementName;
    AtomString namespaceURI = determineNamespace(namespacePrefix);
    if (namespaceURI.isNull()) {
        m_failedParsing = true;
        return;
    }
    AtomString determinedPrefix = namespacePrefix;
    if (namespaceURI == defaultNamespace())
        determinedPrefix = nullAtom();
    QualifiedName tag(determinedPrefix, determinedElementName, namespaceURI);

    // *:host never matches, so we can't discard the *,
    // otherwise we can't tell the difference between *:host and just :host.
    //
    // Also, selectors where we use a ShadowPseudo combinator between the
    // element and the pseudo element for matching (custom pseudo elements,
    // ::cue), we need a universal selector to set the combinator
    // (relation) on in the cases where there are no simple selectors preceding
    // the pseudo element.
    bool isHostPseudo = compoundSelector.isHostPseudoClass();
    if (isHostPseudo && elementName.isNull() && namespacePrefix.isNull())
        return;
    if (tag != anyQName() || isHostPseudo || isShadowDOM)
        compoundSelector.appendTagInComplexSelector(tag, determinedPrefix == nullAtom() && determinedElementName == starAtom() && !isHostPseudo);
}

std::unique_ptr<MutableCSSSelector> CSSSelectorParser::splitCompoundAtImplicitShadowCrossingCombinator(std::unique_ptr<MutableCSSSelector> compoundSelector, const CSSSelectorParserContext& context)
{
    // Complex selectors are represented as a linked list that stores combinator separated compound selectors
    // from right-to-left. Yet, within a single compound selector, stores the simple selectors
    // from left-to-right.
    //
    // ".a.b > div#id" is stored in a complex selector as [div, #id, .a, .b], each element in the
    // list stored with an associated relation (combinator or Subselector).
    //
    // ::cue, ::shadow, and custom pseudo elements have an implicit ShadowPseudo combinator
    // to their left, which really makes for a new compound selector, yet it's consumed by
    // the selector parser as a single compound selector.
    //
    // Example: input#x::-webkit-inner-spin-button -> [ ::-webkit-inner-spin-button, input, #x ]
    //
    auto* splitAfter = compoundSelector.get();
    while (splitAfter->precedingInComplexSelector() && !splitAfter->precedingInComplexSelector()->needsImplicitShadowCombinatorForMatching())
        splitAfter = splitAfter->precedingInComplexSelector();

    if (!splitAfter || !splitAfter->precedingInComplexSelector())
        return compoundSelector;

    // ::part() combines with other pseudo elements.
    bool isPart = splitAfter->precedingInComplexSelector()->match() == CSSSelector::Match::PseudoElement && splitAfter->precedingInComplexSelector()->pseudoElement() == CSSSelector::PseudoElement::Part;

    // ::slotted() combines with other pseudo elements.
    bool isSlotted = splitAfter->precedingInComplexSelector()->match() == CSSSelector::Match::PseudoElement && splitAfter->precedingInComplexSelector()->pseudoElement() == CSSSelector::PseudoElement::Slotted;

    std::unique_ptr<MutableCSSSelector> secondCompound;
    if (isUASheetBehavior(context.mode) || isPart) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=161747
        // We have to recur, since we have rules in media controls like video::a::b. This should not be allowed, and
        // we should remove this recursion once those rules are gone.
        secondCompound = splitCompoundAtImplicitShadowCrossingCombinator(splitAfter->releaseFromComplexSelector(), context);
    } else
        secondCompound = splitAfter->releaseFromComplexSelector();

    auto relation = [&] {
        if (isSlotted)
            return CSSSelector::Relation::ShadowSlotted;
        if (isPart)
            return CSSSelector::Relation::ShadowPartDescendant;
        return CSSSelector::Relation::ShadowDescendant;
    }();
    secondCompound->prependInComplexSelector(relation, WTFMove(compoundSelector));
    return secondCompound;
}

bool CSSSelectorParser::containsUnknownWebKitPseudoElements(const CSSSelector& complexSelector)
{
    for (auto current = &complexSelector; current; current = current->precedingInComplexSelector()) {
        if (current->match() == CSSSelector::Match::PseudoElement && current->pseudoElement() == CSSSelector::PseudoElement::WebKitUnknown)
            return true;
    }
    return false;
}

CSSSelectorList CSSSelectorParser::resolveNestingParent(const CSSSelectorList& nestedSelectorList, const CSSSelectorList* parentResolvedSelectorList, bool parentRuleIsScope)
{
    MutableCSSSelectorList result;

    auto canInline = [](const CSSSelector& nestingSelector, const CSSSelectorList& list) {
        auto hasTagInCompound = [](const CSSSelector& simpleSelector) {
            // A compound is organized so that any tag selector is always last.
            return simpleSelector.lastInCompound()->match() == CSSSelector::Match::Tag;
        };

        if (list.listSize() != 1) {
            // .foo, .bar { & .baz {...} } -> :is(.foo, .bar) .baz {...}
            return false;
        }
        if (complexSelectorCanMatchPseudoElement(*list.first())) {
            // .foo::before { & {...} } -> :is(.foo::before) {...} (which matches nothing)
            return false;
        }
        if (hasTagInCompound(*list.first()) && hasTagInCompound(nestingSelector)) {
            // foo { bar& {...} } -> bar:is(foo) {...}
            return false;
        }
        if (!nestingSelector.precedingInComplexSelector()) {
            // .foo .bar { & .baz {...} } -> .foo .bar .baz {...}
            return true;
        }
        bool hasSingleCompound = !list.first()->firstInCompound()->precedingInComplexSelector();
        if (hasSingleCompound) {
            // .foo.bar { .baz & {...} } -> .baz .foo.bar {...}
            return true;
        }
        // .foo .bar { .baz & {...} } -> .baz :is(.foo .bar) {...}
        return false;
    };

    auto resolveNestingSelector = [&](const CSSSelector& nestingSelector) {
        ASSERT(nestingSelector.match() == CSSSelector::Match::NestingParent);

        if (parentResolvedSelectorList && !parentRuleIsScope) {
            if (canInline(nestingSelector, *parentResolvedSelectorList)) {
                // :is() not needed.
                return makeUnique<MutableCSSSelector>(*parentResolvedSelectorList->first());
            }
            // General case where we wrap with :is().
            auto isSelector = makeUnique<MutableCSSSelector>();
            isSelector->setMatch(CSSSelector::Match::PseudoClass);
            isSelector->setPseudoClass(CSSSelector::PseudoClass::Is);
            isSelector->setSelectorList(makeUnique<CSSSelectorList>(*parentResolvedSelectorList));
            return isSelector;
        }

        // Top-level nesting parent selector acts like :scope with zero specificity thanks to :where
        // https://github.com/w3c/csswg-drafts/issues/10196#issuecomment-2161119978
        // Replace by :where(:scope)
        auto scopeSelector = makeUnique<MutableCSSSelector>();
        scopeSelector->setMatch(CSSSelector::Match::PseudoClass);
        scopeSelector->setPseudoClass(CSSSelector::PseudoClass::Scope);
        auto scopeSelectorList = MutableCSSSelectorList::from(WTFMove(scopeSelector));

        auto whereSelector = makeUnique<MutableCSSSelector>();
        whereSelector->setMatch(CSSSelector::Match::PseudoClass);
        whereSelector->setPseudoClass(CSSSelector::PseudoClass::Where);
        whereSelector->setSelectorList(makeUnique<CSSSelectorList>(WTFMove(scopeSelectorList)));
        return whereSelector;
    };

    auto resolveSimpleSelector = [&](const CSSSelector& simpleSelector) {
        if (simpleSelector.match() == CSSSelector::Match::NestingParent)
            return resolveNestingSelector(simpleSelector);

        auto resolvedSelector = makeUnique<MutableCSSSelector>(simpleSelector, MutableCSSSelector::SimpleSelector);

        if (auto* subselectorList = simpleSelector.selectorList(); subselectorList && subselectorList->hasExplicitNestingParent()) {
            // Resolve nested selector lists like :has(&).
            auto resolvedSubselectorList = resolveNestingParent(*subselectorList, parentResolvedSelectorList, parentRuleIsScope);
            resolvedSelector->setSelectorList(makeUnique<CSSSelectorList>(WTFMove(resolvedSubselectorList)));
        }
        return resolvedSelector;
    };

    for (auto& complexSelector : nestedSelectorList) {
        MutableCSSSelector* leftmost = nullptr;
        for (const auto* simpleSelector = &complexSelector; simpleSelector; simpleSelector = simpleSelector->precedingInComplexSelector()) {
            auto resolvedSimpleSelector = resolveSimpleSelector(*simpleSelector);

            if (!leftmost) {
                result.append(WTFMove(resolvedSimpleSelector));
                leftmost = result.last().get();
            } else
                leftmost->setPrecedingInComplexSelector(WTFMove(resolvedSimpleSelector));

            // A nesting selector may resolve to multiple selectors. Find the leftmost one to continue.
            leftmost = leftmost->leftmostSimpleSelector();
            leftmost->setRelation(simpleSelector->relation());
        }
    }

    return CSSSelectorList { WTFMove(result) };
}

static std::optional<Style::PseudoElementIdentifier> pseudoElementIdentifierFor(CSSSelectorPseudoElement selectorPseudoElement)
{
    auto type = CSSSelector::stylePseudoElementTypeFor(selectorPseudoElement);
    if (!type)
        return { };
    return Style::PseudoElementIdentifier { *type };
}

// FIXME: It's probably worth investigating if more logic can be shared with
// CSSSelectorParser::consumePseudo(), though note that the requirements are subtly different.
std::pair<bool, std::optional<Style::PseudoElementIdentifier>> CSSSelectorParser::parsePseudoElement(const String& input, const CSSSelectorParserContext& context)
{
    auto tokenizer = CSSTokenizer { input };
    auto range = tokenizer.tokenRange();
    auto token = range.consume();
    if (token.type() != ColonToken)
        return { };
    token = range.consume();
    if (token.type() == IdentToken) {
        if (!range.atEnd())
            return { };
        auto pseudoClassOrElement = findPseudoClassAndCompatibilityElementName(token.value());
        if (!pseudoClassOrElement.compatibilityPseudoElement)
            return { };
        ASSERT(CSSSelector::isPseudoElementEnabled(*pseudoClassOrElement.compatibilityPseudoElement, token.value(), context));
        return { true, pseudoElementIdentifierFor(*pseudoClassOrElement.compatibilityPseudoElement) };
    }
    if (token.type() != ColonToken)
        return { };
    token = range.peek();
    if (token.type() != IdentToken && token.type() != FunctionToken)
        return { };
    auto pseudoElement = CSSSelector::parsePseudoElementName(token.value(), context);
    if (!pseudoElement)
        return { };
    if (token.type() == IdentToken) {
        range.consume();
        if (!range.atEnd() || CSSSelector::pseudoElementRequiresArgument(*pseudoElement))
            return { };
        return { true, pseudoElementIdentifierFor(*pseudoElement) };
    }
    ASSERT(token.type() == FunctionToken);
    auto block = range.consumeBlock();
    if (!range.atEnd())
        return { };
    block.consumeWhitespace();
    switch (*pseudoElement) {
    case CSSSelector::PseudoElement::Highlight: {
        auto& ident = block.consumeIncludingWhitespace();
        if (ident.type() != IdentToken || !block.atEnd())
            return { };
        return { true, Style::PseudoElementIdentifier { PseudoElementType::Highlight, ident.value().toAtomString() } };
    }
    case CSSSelector::PseudoElement::ViewTransitionGroup:
    case CSSSelector::PseudoElement::ViewTransitionImagePair:
    case CSSSelector::PseudoElement::ViewTransitionOld:
    case CSSSelector::PseudoElement::ViewTransitionNew: {
        auto& ident = block.consumeIncludingWhitespace();
        if (ident.type() != IdentToken || !isValidCustomIdentifier(ident.id()) || !block.atEnd())
            return { };
        return { true, Style::PseudoElementIdentifier { *CSSSelector::stylePseudoElementTypeFor(*pseudoElement), ident.value().toAtomString() } };
    }
    default:
        return { };
    }
}

} // namespace WebCore
