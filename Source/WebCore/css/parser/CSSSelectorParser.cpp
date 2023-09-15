// Copyright 2014 The Chromium Authors. All rights reserved.
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

#include "CSSParserSelector.h"
#include "CSSSelector.h"
#include "CommonAtomStrings.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "SelectorPseudoTypeMap.h"
#include <memory>
#include <wtf/OptionSet.h>
#include <wtf/SetForScope.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

static AtomString serializeANPlusB(const std::pair<int, int>&);
static bool consumeANPlusB(CSSParserTokenRange&, std::pair<int, int>&);

CSSSelectorParserContext::CSSSelectorParserContext(const CSSParserContext& context)
    : mode(context.mode)
    , cssNestingEnabled(context.cssNestingEnabled)
    , focusVisibleEnabled(context.focusVisibleEnabled)
    , hasPseudoClassEnabled(context.hasPseudoClassEnabled)
{
}

CSSSelectorParserContext::CSSSelectorParserContext(const Document& document)
    : mode(document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode)
    , cssNestingEnabled(document.settings().cssNestingEnabled())
    , focusVisibleEnabled(document.settings().focusVisibleEnabled())
    , hasPseudoClassEnabled(document.settings().hasPseudoClassEnabled())
{
}

void add(Hasher& hasher, const CSSSelectorParserContext& context)
{
    add(hasher,
        context.mode,
        context.cssNestingEnabled,
        context.focusVisibleEnabled,
        context.hasPseudoClassEnabled
    );
}

std::optional<CSSSelectorList> parseCSSSelector(CSSParserTokenRange range, const CSSSelectorParserContext& context, StyleSheetContents* styleSheet, CSSParserEnum::IsNestedContext isNestedContext)
{
    CSSSelectorParser parser(context, styleSheet, isNestedContext);
    range.consumeWhitespace();
    auto consume = [&] {
        if (isNestedContext == CSSParserEnum::IsNestedContext::Yes)
            return parser.consumeNestedSelectorList(range);

        return parser.consumeComplexSelectorList(range);
    };
    CSSSelectorList result = consume();
    if (result.isEmpty() || !range.atEnd())
        return { };
    return result;
}

CSSSelectorParser::CSSSelectorParser(const CSSSelectorParserContext& context, StyleSheetContents* styleSheet, CSSParserEnum::IsNestedContext isNestedContext)
    : m_context(context)
    , m_styleSheet(styleSheet)
    , m_isNestedContext(isNestedContext)
{
}

template <typename ConsumeSelector>
CSSSelectorList CSSSelectorParser::consumeSelectorList(CSSParserTokenRange& range, ConsumeSelector&& consumeSelector)
{
    Vector<std::unique_ptr<CSSParserSelector>> selectorList;
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

    return CSSSelectorList { WTFMove(selectorList) };
}

CSSSelectorList CSSSelectorParser::consumeComplexSelectorList(CSSParserTokenRange& range)
{
    return consumeSelectorList(range, [&] (CSSParserTokenRange& range) {
        return consumeComplexSelector(range);
    });
}

CSSSelectorList CSSSelectorParser::consumeRelativeSelectorList(CSSParserTokenRange& range)
{
    return consumeSelectorList(range, [&](CSSParserTokenRange& range) {
        return consumeRelativeScopeSelector(range);
    });
}

CSSSelectorList CSSSelectorParser::consumeNestedSelectorList(CSSParserTokenRange& range)
{
    return consumeSelectorList(range, [&] (CSSParserTokenRange& range) {
        return consumeNestedComplexSelector(range);
    });
}

template<typename ConsumeSelector>
CSSSelectorList CSSSelectorParser::consumeForgivingSelectorList(CSSParserTokenRange& range, ConsumeSelector&& consumeSelector)
{
    if (m_failedParsing)
        return { };

    Vector<std::unique_ptr<CSSParserSelector>> selectorList;

    auto consumeForgiving = [&] {
        auto initialRange = range;
        auto unknownSelector = [&] {
            auto unknownSelector = makeUnique<CSSParserSelector>();
            auto unknownRange = initialRange.makeSubRange(initialRange.begin(), range.begin());
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

    return CSSSelectorList { WTFMove(selectorList) };
}

CSSSelectorList CSSSelectorParser::consumeForgivingComplexSelectorList(CSSParserTokenRange& range)
{
    return consumeForgivingSelectorList(range, [&](CSSParserTokenRange& range) {
        return consumeComplexSelector(range);
    });
}

bool CSSSelectorParser::supportsComplexSelector(CSSParserTokenRange range, const CSSSelectorParserContext& context, CSSParserEnum::IsNestedContext isNestedContext)
{
    range.consumeWhitespace();
    CSSSelectorParser parser(context, nullptr, isNestedContext);

    // @supports requires that all arguments parse.
    parser.m_disableForgivingParsing = true;

    std::unique_ptr<CSSParserSelector> parserSelector;
    if (isNestedContext == CSSParserEnum::IsNestedContext::Yes)
        parserSelector = parser.consumeNestedComplexSelector(range);
    else
        parserSelector = parser.consumeComplexSelector(range);

    if (parser.m_failedParsing || !range.atEnd() || !parserSelector)
        return false;

    auto complexSelector = parserSelector->releaseSelector();
    ASSERT(complexSelector);

    return !containsUnknownWebKitPseudoElements(*complexSelector);
}

CSSSelectorList CSSSelectorParser::consumeCompoundSelectorList(CSSParserTokenRange& range)
{
    Vector<std::unique_ptr<CSSParserSelector>> selectorList;
    auto selector = consumeCompoundSelector(range);
    range.consumeWhitespace();
    if (!selector)
        return CSSSelectorList();
    selectorList.append(WTFMove(selector));
    while (!range.atEnd() && range.peek().type() == CommaToken) {
        range.consumeIncludingWhitespace();
        selector = consumeCompoundSelector(range);
        range.consumeWhitespace();
        if (!selector)
            return CSSSelectorList();
        selectorList.append(WTFMove(selector));
    }
    return CSSSelectorList { WTFMove(selectorList) };
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

enum class CompoundSelectorFlag {
    HasPseudoElementForRightmostCompound = 1 << 0,
};

static OptionSet<CompoundSelectorFlag> extractCompoundFlags(const CSSParserSelector& simpleSelector, CSSParserMode parserMode)
{
    if (simpleSelector.match() != CSSSelector::Match::PseudoElement)
        return { };
    
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=161747
    // The UASheetMode check is a work-around to allow this selector in mediaControls(New).css:
    // input[type="range" i]::-webkit-media-slider-container > div {
    if (parserMode == UASheetMode && simpleSelector.pseudoElementType() == CSSSelector::PseudoElementWebKitCustom)
        return { };

    return CompoundSelectorFlag::HasPseudoElementForRightmostCompound;
}

static bool isDescendantCombinator(CSSSelector::RelationType relation)
{
    return relation == CSSSelector::RelationType::DescendantSpace;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeNestedComplexSelector(CSSParserTokenRange& range)
{
    auto appendNestingSelector = [] (auto& parserSelector) {
        // https://drafts.csswg.org/css-nesting/#cssom
        // Relative selector should be absolutized (only when not "nest-containing" for the descendant one),
        // with the implied nesting selector inserted.

        auto lastSelector = parserSelector->leftmostSimpleSelector()->selector();
        ASSERT(lastSelector);

        // Relation is Descendant by default.
        auto relation = lastSelector->relation();
        if (relation == CSSSelector::RelationType::Subselector)
            relation = CSSSelector::RelationType::DescendantSpace;

        auto nestingSelector = makeUnique<CSSParserSelector>();
        nestingSelector->setMatch(CSSSelector::Match::NestingParent);
        // We add the implicit parent selector at the beginning of the selector.
        parserSelector->appendTagHistory(relation, WTFMove(nestingSelector));
    };

    auto selector = consumeComplexSelector(range);
    if (selector) {
        if (selector->hasExplicitNestingParent())
            return selector;

        appendNestingSelector(selector);
        return selector;
    }

    selector = consumeRelativeNestedSelector(range);
    if (selector) {
        appendNestingSelector(selector);
        return selector;
    }

    return nullptr;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeComplexSelector(CSSParserTokenRange& range)
{
    auto selector = consumeCompoundSelector(range);
    if (!selector)
        return nullptr;

    OptionSet<CompoundSelectorFlag> previousCompoundFlags;

    for (CSSParserSelector* simple = selector.get(); simple && !previousCompoundFlags; simple = simple->tagHistory())
        previousCompoundFlags = extractCompoundFlags(*simple, m_context.mode);

    while (true) {
        auto combinator = consumeCombinator(range);
        if (combinator == CSSSelector::RelationType::Subselector)
            break;

        auto nextSelector = consumeCompoundSelector(range);
        if (!nextSelector)
            return isDescendantCombinator(combinator) ? WTFMove(selector) : nullptr;
        if (previousCompoundFlags.contains(CompoundSelectorFlag::HasPseudoElementForRightmostCompound))
            return nullptr;
        CSSParserSelector* end = nextSelector.get();
        auto compoundFlags = extractCompoundFlags(*end, m_context.mode);
        while (end->tagHistory()) {
            end = end->tagHistory();
            compoundFlags.add(extractCompoundFlags(*end, m_context.mode));
        }
        end->setRelation(combinator);
        previousCompoundFlags = compoundFlags;
        end->setTagHistory(WTFMove(selector));

        selector = WTFMove(nextSelector);
    }

    return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeRelativeScopeSelector(CSSParserTokenRange& range)
{
    auto scopeCombinator = consumeCombinator(range);

    if (scopeCombinator == CSSSelector::RelationType::Subselector)
        scopeCombinator = CSSSelector::RelationType::DescendantSpace;

    auto selector = consumeComplexSelector(range);
    if (!selector)
        return nullptr;

    auto* end = selector.get();
    while (end->tagHistory())
        end = end->tagHistory();

    auto scopeSelector = makeUnique<CSSParserSelector>();
    scopeSelector->setMatch(CSSSelector::Match::PseudoClass);
    scopeSelector->setPseudoClassType(CSSSelector::PseudoClassType::HasScope);

    end->setRelation(scopeCombinator);
    end->setTagHistory(WTFMove(scopeSelector));

    return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeRelativeNestedSelector(CSSParserTokenRange& range)
{
    auto scopeCombinator = consumeCombinator(range);

    // Nesting should only work with ~ > + combinators in this function. 
    // The descendant combinator is handled in another code path.
    if (scopeCombinator != CSSSelector::RelationType::DirectAdjacent && scopeCombinator != CSSSelector::RelationType::IndirectAdjacent && scopeCombinator != CSSSelector::RelationType::Child)
        return nullptr;
    
    auto selector = consumeComplexSelector(range);
    if (!selector)
        return nullptr;

    auto last = selector->leftmostSimpleSelector();
    last->setRelation(scopeCombinator);

    return selector;
}

static bool isScrollbarPseudoClass(CSSSelector::PseudoClassType pseudo)
{
    switch (pseudo) {
    case CSSSelector::PseudoClassType::Enabled:
    case CSSSelector::PseudoClassType::Disabled:
    case CSSSelector::PseudoClassType::Hover:
    case CSSSelector::PseudoClassType::Active:
    case CSSSelector::PseudoClassType::Horizontal:
    case CSSSelector::PseudoClassType::Vertical:
    case CSSSelector::PseudoClassType::Decrement:
    case CSSSelector::PseudoClassType::Increment:
    case CSSSelector::PseudoClassType::Start:
    case CSSSelector::PseudoClassType::End:
    case CSSSelector::PseudoClassType::DoubleButton:
    case CSSSelector::PseudoClassType::SingleButton:
    case CSSSelector::PseudoClassType::NoButton:
    case CSSSelector::PseudoClassType::CornerPresent:
    case CSSSelector::PseudoClassType::WindowInactive:
        return true;
    default:
        return false;
    }
}

static bool isUserActionPseudoClass(CSSSelector::PseudoClassType pseudo)
{
    switch (pseudo) {
    case CSSSelector::PseudoClassType::Hover:
    case CSSSelector::PseudoClassType::Focus:
    case CSSSelector::PseudoClassType::Active:
    case CSSSelector::PseudoClassType::FocusVisible:
    case CSSSelector::PseudoClassType::FocusWithin:
        return true;
    default:
        return false;
    }
}

static bool isPseudoClassValidAfterPseudoElement(CSSSelector::PseudoClassType pseudoClass, CSSSelector::PseudoElementType compoundPseudoElement)
{
    // Validity of these is determined by their content.
    if (isLogicalCombinationPseudoClass(pseudoClass))
        return true;

    switch (compoundPseudoElement) {
    case CSSSelector::PseudoElementPart:
        return !isTreeStructuralPseudoClass(pseudoClass);
    case CSSSelector::PseudoElementSlotted:
        return false;
    case CSSSelector::PseudoElementResizer:
    case CSSSelector::PseudoElementScrollbar:
    case CSSSelector::PseudoElementScrollbarCorner:
    case CSSSelector::PseudoElementScrollbarButton:
    case CSSSelector::PseudoElementScrollbarThumb:
    case CSSSelector::PseudoElementScrollbarTrack:
    case CSSSelector::PseudoElementScrollbarTrackPiece:
        return isScrollbarPseudoClass(pseudoClass);
    case CSSSelector::PseudoElementSelection:
        return pseudoClass == CSSSelector::PseudoClassType::WindowInactive;
    case CSSSelector::PseudoElementWebKitCustom:
    case CSSSelector::PseudoElementWebKitCustomLegacyPrefixed:
        return isUserActionPseudoClass(pseudoClass);
    default:
        return false;
    }
}

static bool isTreeAbidingPseudoElement(CSSSelector::PseudoElementType pseudoElementType)
{
    switch (pseudoElementType) {
    // FIXME: This list should also include ::placeholder and ::file-selector-button
    case CSSSelector::PseudoElementBefore:
    case CSSSelector::PseudoElementAfter:
    case CSSSelector::PseudoElementMarker:
        return true;
    default:
        return false;
    }
}

static bool isSimpleSelectorValidAfterPseudoElement(const CSSParserSelector& simpleSelector, CSSSelector::PseudoElementType compoundPseudoElement)
{
    ASSERT(compoundPseudoElement != CSSSelector::PseudoElementUnknown);
    
    if (compoundPseudoElement == CSSSelector::PseudoElementPart) {
        if (simpleSelector.match() == CSSSelector::Match::PseudoElement && simpleSelector.pseudoElementType() != CSSSelector::PseudoElementPart)
            return true;
    }
    if (compoundPseudoElement == CSSSelector::PseudoElementSlotted) {
        if (simpleSelector.match() == CSSSelector::Match::PseudoElement && isTreeAbidingPseudoElement(simpleSelector.pseudoElementType()))
            return true;
    }
    if (simpleSelector.match() != CSSSelector::Match::PseudoClass)
        return false;

    return isPseudoClassValidAfterPseudoElement(simpleSelector.pseudoClassType(), compoundPseudoElement);
}

static bool atEndIgnoringWhitespace(CSSParserTokenRange range)
{
    range.consumeWhitespace();
    return range.atEnd();
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeCompoundSelector(CSSParserTokenRange& range)
{
    ASSERT(!m_precedingPseudoElement || m_disallowPseudoElements);

    std::unique_ptr<CSSParserSelector> compoundSelector;

    AtomString namespacePrefix;
    AtomString elementName;
    const bool hasName = consumeName(range, elementName, namespacePrefix);
    if (!hasName) {
        compoundSelector = consumeSimpleSelector(range);
        if (!compoundSelector)
            return nullptr;
        if (compoundSelector->match() == CSSSelector::Match::PseudoElement)
            m_precedingPseudoElement = compoundSelector->pseudoElementType();
    }

    while (auto simpleSelector = consumeSimpleSelector(range)) {
        if (simpleSelector->match() == CSSSelector::Match::PseudoElement)
            m_precedingPseudoElement = simpleSelector->pseudoElementType();

        if (compoundSelector)
            compoundSelector->appendTagHistory(CSSSelector::RelationType::Subselector, WTFMove(simpleSelector));
        else
            compoundSelector = WTFMove(simpleSelector);
    }

    if (!m_disallowPseudoElements)
        m_precedingPseudoElement = { };

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
        
        return makeUnique<CSSParserSelector>(QualifiedName(namespacePrefix, elementName, namespaceURI));
    }
    prependTypeSelectorIfNeeded(namespacePrefix, elementName, *compoundSelector);
    return splitCompoundAtImplicitShadowCrossingCombinator(WTFMove(compoundSelector), m_context);
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeSimpleSelector(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    std::unique_ptr<CSSParserSelector> selector;
    if (token.type() == HashToken)
        selector = consumeId(range);
    else if (token.type() == DelimiterToken && token.delimiter() == '.')
        selector = consumeClass(range);
    else if (token.type() == DelimiterToken && token.delimiter() == '&' && m_context.cssNestingEnabled)
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
        if (m_context.mode != UASheetMode && !isSimpleSelectorValidAfterPseudoElement(*selector, *m_precedingPseudoElement))
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

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeId(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == HashToken);
    if (range.peek().getHashTokenType() != HashTokenId)
        return nullptr;

    auto selector = makeUnique<CSSParserSelector>();
    selector->setMatch(CSSSelector::Match::Id);
    
    // FIXME-NEWPARSER: Avoid having to do this, but the old parser does and we need
    // to be compatible for now.
    CSSParserToken token = range.consume();
    selector->setValue(token.value().toAtomString(), m_context.mode == HTMLQuirksMode);
    return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeClass(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == DelimiterToken);
    ASSERT(range.peek().delimiter() == '.');
    range.consume();
    if (range.peek().type() != IdentToken)
        return nullptr;

    auto selector = makeUnique<CSSParserSelector>();
    selector->setMatch(CSSSelector::Match::Class);
    
    // FIXME-NEWPARSER: Avoid having to do this, but the old parser does and we need
    // to be compatible for now.
    CSSParserToken token = range.consume();
    selector->setValue(token.value().toAtomString(), m_context.mode == HTMLQuirksMode);

    return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeNesting(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == DelimiterToken);
    ASSERT(range.peek().delimiter() == '&');
    range.consume();

    auto selector = makeUnique<CSSParserSelector>();
    selector->setMatch(CSSSelector::Match::NestingParent);
    
    return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeAttribute(CSSParserTokenRange& range)
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

    auto selector = makeUnique<CSSParserSelector>();

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

static bool isOnlyPseudoClassFunction(CSSSelector::PseudoClassType pseudoClassType)
{
    switch (pseudoClassType) {
    case CSSSelector::PseudoClassType::Not:
    case CSSSelector::PseudoClassType::Is:
    case CSSSelector::PseudoClassType::Matches:
    case CSSSelector::PseudoClassType::Where:
    case CSSSelector::PseudoClassType::Has:
    case CSSSelector::PseudoClassType::NthChild:
    case CSSSelector::PseudoClassType::NthLastChild:
    case CSSSelector::PseudoClassType::NthOfType:
    case CSSSelector::PseudoClassType::NthLastOfType:
    case CSSSelector::PseudoClassType::Lang:
    case CSSSelector::PseudoClassType::Any:
    case CSSSelector::PseudoClassType::Dir:
        return true;
    default:
        break;
    }
    return false;
}
    
static bool isOnlyPseudoElementFunction(CSSSelector::PseudoElementType pseudoElementType)
{
    // Note that we omit cue since it can be either an ident or a function.
    switch (pseudoElementType) {
    case CSSSelector::PseudoElementPart:
    case CSSSelector::PseudoElementSlotted:
        return true;
    default:
        break;
    }
    return false;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumePseudo(CSSParserTokenRange& range)
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

    std::unique_ptr<CSSParserSelector> selector;
    
    if (colons == 1) {
        selector = CSSParserSelector::parsePseudoClassSelector(token.value());
        if (!selector)
            return nullptr;
        if (selector->match() == CSSSelector::Match::PseudoClass) {
            if (!m_context.focusVisibleEnabled && selector->pseudoClassType() == CSSSelector::PseudoClassType::FocusVisible)
                return nullptr;
            if (!m_context.hasPseudoClassEnabled && selector->pseudoClassType() == CSSSelector::PseudoClassType::Has)
                return nullptr;
            if (m_context.mode != UASheetMode && selector->pseudoClassType() == CSSSelector::PseudoClassType::HtmlDocument)
                return nullptr;
#if ENABLE(ATTACHMENT_ELEMENT)
            if (!DeprecatedGlobalSettings::attachmentElementEnabled() && selector->pseudoClassType() == CSSSelector::PseudoClassType::HasAttachment)
                return nullptr;
#endif
        }
    } else {
        selector = CSSParserSelector::parsePseudoElementSelector(token.value());
#if ENABLE(VIDEO)
        // Treat the ident version of cue as PseudoElementWebkitCustom.
        if (token.type() == IdentToken && selector && selector->match() == CSSSelector::Match::PseudoElement && selector->pseudoElementType() == CSSSelector::PseudoElementCue)
            selector->setPseudoElementType(CSSSelector::PseudoElementWebKitCustom);
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
        if ((selector->match() == CSSSelector::Match::PseudoElement && (selector->pseudoElementType() == CSSSelector::PseudoElementUnknown || isOnlyPseudoElementFunction(selector->pseudoElementType())))
            || (selector->match() == CSSSelector::Match::PseudoClass && (selector->pseudoClassType() == CSSSelector::PseudoClassType::Unknown || isOnlyPseudoClassFunction(selector->pseudoClassType()))))
            return nullptr;
        return selector;
    }

    CSSParserTokenRange block = range.consumeBlock();
    block.consumeWhitespace();
    if (token.type() != FunctionToken)
        return nullptr;

    if (selector->match() == CSSSelector::Match::PseudoClass) {
        switch (selector->pseudoClassType()) {
        case CSSSelector::PseudoClassType::Not: {
            SetForScope resistDefaultNamespace(m_resistDefaultNamespace, true);
            auto selectorList = makeUnique<CSSSelectorList>();
            *selectorList = consumeComplexSelectorList(block);
            if (!selectorList->first() || !block.atEnd())
                return nullptr;
            selector->setSelectorList(WTFMove(selectorList));
            return selector;
        }
        case CSSSelector::PseudoClassType::NthChild:
        case CSSSelector::PseudoClassType::NthLastChild:
        case CSSSelector::PseudoClassType::NthOfType:
        case CSSSelector::PseudoClassType::NthLastOfType: {
            std::pair<int, int> ab;
            if (!consumeANPlusB(block, ab))
                return nullptr;
            block.consumeWhitespace();
            // FIXME: We should be able to do this lazily. See: https://bugs.webkit.org/show_bug.cgi?id=217149
            selector->setArgument(serializeANPlusB(ab));
            if (!block.atEnd()) {
                auto type = selector->pseudoClassType();
                if (type == CSSSelector::PseudoClassType::NthOfType || type == CSSSelector::PseudoClassType::NthLastOfType)
                    return nullptr;
                if (block.peek().type() != IdentToken)
                    return nullptr;
                const CSSParserToken& ident = block.consume();
                if (!equalLettersIgnoringASCIICase(ident.value(), "of"_s))
                    return nullptr;
                block.consumeWhitespace();
                auto selectorList = makeUnique<CSSSelectorList>();
                *selectorList = consumeComplexSelectorList(block);
                if (selectorList->isEmpty() || !block.atEnd())
                    return nullptr;
                selector->setSelectorList(WTFMove(selectorList));
            }
            selector->setNth(ab.first, ab.second);
            return selector;
        }
        case CSSSelector::PseudoClassType::Lang: {
            auto list = consumeLangArgumentList(block);
            if (list.isEmpty() || !block.atEnd())
                return nullptr;
            selector->setArgumentList(WTFMove(list));
            return selector;
        }
        case CSSSelector::PseudoClassType::Is:
        case CSSSelector::PseudoClassType::Where:
        case CSSSelector::PseudoClassType::Matches:
        case CSSSelector::PseudoClassType::Any: {
            SetForScope resistDefaultNamespace(m_resistDefaultNamespace, true);
            auto selectorList = makeUnique<CSSSelectorList>();
            *selectorList = consumeForgivingComplexSelectorList(block);
            if (!block.atEnd())
                return nullptr;
            selector->setSelectorList(WTFMove(selectorList));
            return selector;
        }
        case CSSSelector::PseudoClassType::Host: {
            auto innerSelector = consumeCompoundSelector(block);
            block.consumeWhitespace();
            if (!innerSelector || !block.atEnd())
                return nullptr;
            selector->adoptSelectorVector(Vector<std::unique_ptr<CSSParserSelector>>::from(WTFMove(innerSelector)));
            return selector;
        }
        case CSSSelector::PseudoClassType::Has: {
            if (m_disallowHasPseudoClass)
                return nullptr;
            SetForScope resistDefaultNamespace(m_resistDefaultNamespace, true);
            SetForScope disallowNestedHas(m_disallowHasPseudoClass, true);
            auto selectorList = makeUnique<CSSSelectorList>();
            *selectorList = consumeRelativeSelectorList(block);
            if (selectorList->isEmpty() || !block.atEnd())
                return nullptr;
            selector->setSelectorList(WTFMove(selectorList));
            return selector;
        }
        case CSSSelector::PseudoClassType::Dir: {
            const CSSParserToken& ident = block.consumeIncludingWhitespace();
            if (ident.type() != IdentToken || !block.atEnd())
                return nullptr;
            selector->setArgument(ident.value().toAtomString());
            return selector;
        }
        default:
            break;
        }
    }
    
    if (selector->match() == CSSSelector::Match::PseudoElement) {
        switch (selector->pseudoElementType()) {
#if ENABLE(VIDEO)
        case CSSSelector::PseudoElementCue: {
            auto selectorList = makeUnique<CSSSelectorList>();
            *selectorList = consumeCompoundSelectorList(block);
            if (selectorList->isEmpty() || !block.atEnd())
                return nullptr;
            selector->setSelectorList(WTFMove(selectorList));
            return selector;
        }
#endif
        case CSSSelector::PseudoElementHighlight: {
            auto& ident = block.consumeIncludingWhitespace();
            if (ident.type() != IdentToken || !block.atEnd())
                return nullptr;
            selector->setArgumentList({ { ident.value().toAtomString() } });
            return selector;
        }
        case CSSSelector::PseudoElementPart: {
            Vector<PossiblyQuotedIdentifier> argumentList;
            do {
                auto& ident = block.consumeIncludingWhitespace();
                if (ident.type() != IdentToken)
                    return nullptr;
                argumentList.append({ ident.value().toAtomString() });
            } while (!block.atEnd());
            selector->setArgumentList(FixedVector<PossiblyQuotedIdentifier> { WTFMove(argumentList) });
            return selector;
        }
        case CSSSelector::PseudoElementSlotted: {
            auto innerSelector = consumeCompoundSelector(block);
            block.consumeWhitespace();
            if (!innerSelector || !block.atEnd())
                return nullptr;
            selector->adoptSelectorVector(Vector<std::unique_ptr<CSSParserSelector>>::from(WTFMove(innerSelector)));
            return selector;
        }
        default:
            break;
        }
    }

    return nullptr;
}

CSSSelector::RelationType CSSSelectorParser::consumeCombinator(CSSParserTokenRange& range)
{
    auto fallbackResult = CSSSelector::RelationType::Subselector;
    while (range.peek().type() == WhitespaceToken) {
        range.consume();
        fallbackResult = CSSSelector::RelationType::DescendantSpace;
    }

    if (range.peek().type() != DelimiterToken)
        return fallbackResult;

    UChar delimiter = range.peek().delimiter();

    if (delimiter == '+' || delimiter == '~' || delimiter == '>') {
        range.consumeIncludingWhitespace();
        if (delimiter == '+')
            return CSSSelector::RelationType::DirectAdjacent;
        if (delimiter == '~')
            return CSSSelector::RelationType::IndirectAdjacent;
        return CSSSelector::RelationType::Child;
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
        FALLTHROUGH;
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

void CSSSelectorParser::prependTypeSelectorIfNeeded(const AtomString& namespacePrefix, const AtomString& elementName, CSSParserSelector& compoundSelector)
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
    bool explicitForHost = compoundSelector.isHostPseudoSelector() && !elementName.isNull();
    if (tag != anyQName() || explicitForHost || isShadowDOM)
        compoundSelector.prependTagSelector(tag, determinedPrefix == nullAtom() && determinedElementName == starAtom() && !explicitForHost);
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::splitCompoundAtImplicitShadowCrossingCombinator(std::unique_ptr<CSSParserSelector> compoundSelector, const CSSSelectorParserContext& context)
{
    // The tagHistory is a linked list that stores combinator separated compound selectors
    // from right-to-left. Yet, within a single compound selector, stores the simple selectors
    // from left-to-right.
    //
    // ".a.b > div#id" is stored in a tagHistory as [div, #id, .a, .b], each element in the
    // list stored with an associated relation (combinator or Subselector).
    //
    // ::cue, ::shadow, and custom pseudo elements have an implicit ShadowPseudo combinator
    // to their left, which really makes for a new compound selector, yet it's consumed by
    // the selector parser as a single compound selector.
    //
    // Example: input#x::-webkit-inner-spin-button -> [ ::-webkit-inner-spin-button, input, #x ]
    //
    CSSParserSelector* splitAfter = compoundSelector.get();
    while (splitAfter->tagHistory() && !splitAfter->tagHistory()->needsImplicitShadowCombinatorForMatching())
        splitAfter = splitAfter->tagHistory();

    if (!splitAfter || !splitAfter->tagHistory())
        return compoundSelector;

    // ::part() combines with other pseudo elements.
    bool isPart = splitAfter->tagHistory()->match() == CSSSelector::Match::PseudoElement && splitAfter->tagHistory()->pseudoElementType() == CSSSelector::PseudoElementPart;

    // ::slotted() combines with other pseudo elements.
    bool isSlotted = splitAfter->tagHistory()->match() == CSSSelector::Match::PseudoElement && splitAfter->tagHistory()->pseudoElementType() == CSSSelector::PseudoElementSlotted;

    std::unique_ptr<CSSParserSelector> secondCompound;
    if (context.mode == UASheetMode || isPart) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=161747
        // We have to recur, since we have rules in media controls like video::a::b. This should not be allowed, and
        // we should remove this recursion once those rules are gone.
        secondCompound = splitCompoundAtImplicitShadowCrossingCombinator(splitAfter->releaseTagHistory(), context);
    } else
        secondCompound = splitAfter->releaseTagHistory();

    auto relation = [&] {
        if (isSlotted)
            return CSSSelector::RelationType::ShadowSlotted;
        if (isPart)
            return CSSSelector::RelationType::ShadowPartDescendant;
        return CSSSelector::RelationType::ShadowDescendant;
    }();
    secondCompound->appendTagHistory(relation, WTFMove(compoundSelector));
    return secondCompound;
}

bool CSSSelectorParser::containsUnknownWebKitPseudoElements(const CSSSelector& complexSelector)
{
    for (auto current = &complexSelector; current; current = current->tagHistory()) {
        if (current->match() == CSSSelector::Match::PseudoElement) {
            if (current->pseudoElementType() != CSSSelector::PseudoElementWebKitCustom)
                continue;

            if (parsePseudoElementString(StringView(current->value())) == CSSSelector::PseudoElementUnknown)
                return true;
        }
    }

    return false;
}

CSSSelectorList CSSSelectorParser::resolveNestingParent(const CSSSelectorList& nestedSelectorList, const CSSSelectorList* parentResolvedSelectorList)
{
    Vector<std::unique_ptr<CSSParserSelector>> result;
    CSSSelectorList copiedSelectorList { nestedSelectorList };
    auto selector = copiedSelectorList.first();
    while (selector) {
        if (parentResolvedSelectorList) {
            // FIXME: We should build a new CSSParserSelector from this selector and resolve it
            const_cast<CSSSelector*>(selector)->resolveNestingParentSelectors(*parentResolvedSelectorList);
        } else {
            // It's top-level, the nesting parent selector should be replace by :scope
            const_cast<CSSSelector*>(selector)->replaceNestingParentByPseudoClassScope();
        }
        auto parserSelector = makeUnique<CSSParserSelector>(*selector);
        result.append(WTFMove(parserSelector));
        selector = copiedSelectorList.next(selector);
    }

    auto final = CSSSelectorList { WTFMove(result) };
    return final;
}

} // namespace WebCore
