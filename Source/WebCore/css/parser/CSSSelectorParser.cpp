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

#include "CommonAtomStrings.h"
#include "DeprecatedGlobalSettings.h"
#include <memory>
#include <wtf/OptionSet.h>
#include <wtf/SetForScope.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

static AtomString serializeANPlusB(const std::pair<int, int>&);
static bool consumeANPlusB(CSSParserTokenRange&, std::pair<int, int>&);

std::optional<CSSSelectorList> parseCSSSelector(CSSParserTokenRange range, const CSSParserContext& context, StyleSheetContents* styleSheet)
{
    CSSSelectorParser parser(context, styleSheet);
    range.consumeWhitespace();
    CSSSelectorList result = parser.consumeComplexSelectorList(range);
    if (result.isEmpty() || !range.atEnd())
        return { };
    return result;
}

CSSSelectorParser::CSSSelectorParser(const CSSParserContext& context, StyleSheetContents* styleSheet)
    : m_context(context)
    , m_styleSheet(styleSheet)
{
}

CSSSelectorList CSSSelectorParser::consumeComplexSelectorList(CSSParserTokenRange& range)
{
    Vector<std::unique_ptr<CSSParserSelector>> selectorList;
    auto selector = consumeComplexSelector(range);
    if (!selector)
        return { };

    selectorList.append(WTFMove(selector));
    while (!range.atEnd() && range.peek().type() == CommaToken) {
        range.consumeIncludingWhitespace();
        selector = consumeComplexSelector(range);
        if (!selector)
            return { };
        selectorList.append(WTFMove(selector));
    }

    if (m_failedParsing)
        return { };

    return CSSSelectorList { WTFMove(selectorList) };
}

template<typename ConsumeSelector>
CSSSelectorList CSSSelectorParser::consumeForgivingSelectorList(CSSParserTokenRange& range, ConsumeSelector&& consumeSelector)
{
    if (m_failedParsing)
        return { };

    Vector<std::unique_ptr<CSSParserSelector>> selectorList;

    auto consumeForgiving = [&] {
        auto selector = consumeSelector(range);

        if (m_failedParsing && !m_disableForgivingParsing) {
            selector = { };
            m_failedParsing = false;
        }
        if (!range.atEnd() && range.peek().type() != CommaToken) {
            while (!range.atEnd() && range.peek().type() != CommaToken)
                range.consume();
            return;
        }
        if (selector)
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

CSSSelectorList CSSSelectorParser::consumeForgivingRelativeSelectorList(CSSParserTokenRange& range)
{
    return consumeForgivingSelectorList(range, [&](CSSParserTokenRange& range) {
        return consumeRelativeSelector(range);
    });
}

bool CSSSelectorParser::supportsComplexSelector(CSSParserTokenRange range, const CSSParserContext& context)
{
    range.consumeWhitespace();
    CSSSelectorParser parser(context, nullptr);

    // @supports requires that all arguments parse.
    parser.m_disableForgivingParsing = true;

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=215635
    // Unknown css selector combinator is not addressed correctly in |CSSSelectorParser::consumeComplexSelector|.
    auto parserSelector = parser.consumeComplexSelector(range);
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

static PossiblyQuotedIdentifier consumePossiblyQuotedIdentifer(CSSParserTokenRange& range)
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
    auto item = consumePossiblyQuotedIdentifer(range);
    if (item.isNull())
        return { };
    list.append(WTFMove(item));
    while (!range.atEnd() && range.peek().type() == CommaToken) {
        range.consumeIncludingWhitespace();
        item = consumePossiblyQuotedIdentifer(range);
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
    if (simpleSelector.match() != CSSSelector::PseudoElement)
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
    return relation == CSSSelector::DescendantSpace;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeComplexSelector(CSSParserTokenRange& range)
{
    auto selector = consumeCompoundSelector(range);
    if (!selector)
        return nullptr;

    OptionSet<CompoundSelectorFlag> previousCompoundFlags;

    for (CSSParserSelector* simple = selector.get(); simple && !previousCompoundFlags; simple = simple->tagHistory())
        previousCompoundFlags = extractCompoundFlags(*simple, m_context.mode);

    while (auto combinator = consumeCombinator(range)) {
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

std::unique_ptr<CSSParserSelector> CSSSelectorParser::consumeRelativeSelector(CSSParserTokenRange& range)
{
    auto scopeCombinator = consumeCombinator(range);

    if (scopeCombinator == CSSSelector::Subselector)
        scopeCombinator = CSSSelector::DescendantSpace;

    auto selector = consumeComplexSelector(range);
    if (!selector)
        return nullptr;

    auto hasScopePseudoClass = [](auto& selector) {
        return selector.match() == CSSSelector::PseudoClass && selector.pseudoClassType() == CSSSelector::PseudoClassScope;
    };

    bool hasExplicitScope = hasScopePseudoClass(*selector);

    auto* end = selector.get();
    while (end->tagHistory()) {
        end = end->tagHistory();
        if (hasScopePseudoClass(*end))
            hasExplicitScope = true;
    }

    // If the selector doesn't have an explicit :scope on the left, add an implicit one.
    if (!hasExplicitScope || scopeCombinator != CSSSelector::DescendantSpace) {
        auto scopeSelector = makeUnique<CSSParserSelector>();
        scopeSelector->setMatch(CSSSelector::PseudoClass);
        scopeSelector->setPseudoClassType(CSSSelector::PseudoClassRelativeScope);

        end->setRelation(scopeCombinator);
        end->setTagHistory(WTFMove(scopeSelector));
    }

    return selector;
}

static bool isScrollbarPseudoClass(CSSSelector::PseudoClassType pseudo)
{
    switch (pseudo) {
    case CSSSelector::PseudoClassEnabled:
    case CSSSelector::PseudoClassDisabled:
    case CSSSelector::PseudoClassHover:
    case CSSSelector::PseudoClassActive:
    case CSSSelector::PseudoClassHorizontal:
    case CSSSelector::PseudoClassVertical:
    case CSSSelector::PseudoClassDecrement:
    case CSSSelector::PseudoClassIncrement:
    case CSSSelector::PseudoClassStart:
    case CSSSelector::PseudoClassEnd:
    case CSSSelector::PseudoClassDoubleButton:
    case CSSSelector::PseudoClassSingleButton:
    case CSSSelector::PseudoClassNoButton:
    case CSSSelector::PseudoClassCornerPresent:
    case CSSSelector::PseudoClassWindowInactive:
        return true;
    default:
        return false;
    }
}

static bool isUserActionPseudoClass(CSSSelector::PseudoClassType pseudo)
{
    switch (pseudo) {
    case CSSSelector::PseudoClassHover:
    case CSSSelector::PseudoClassFocus:
    case CSSSelector::PseudoClassActive:
    case CSSSelector::PseudoClassFocusVisible:
    case CSSSelector::PseudoClassFocusWithin:
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
        return pseudoClass == CSSSelector::PseudoClassWindowInactive;
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
        if (simpleSelector.match() == CSSSelector::PseudoElement && simpleSelector.pseudoElementType() != CSSSelector::PseudoElementPart)
            return true;
    }
    if (compoundPseudoElement == CSSSelector::PseudoElementSlotted) {
        if (simpleSelector.match() == CSSSelector::PseudoElement && isTreeAbidingPseudoElement(simpleSelector.pseudoElementType()))
            return true;
    }
    if (simpleSelector.match() != CSSSelector::PseudoClass)
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
        if (compoundSelector->match() == CSSSelector::PseudoElement)
            m_precedingPseudoElement = compoundSelector->pseudoElementType();
    }

    while (auto simpleSelector = consumeSimpleSelector(range)) {
        if (simpleSelector->match() == CSSSelector::PseudoElement)
            m_precedingPseudoElement = simpleSelector->pseudoElementType();

        if (compoundSelector)
            compoundSelector->appendTagHistory(CSSSelector::Subselector, WTFMove(simpleSelector));
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
    range.consume();

    namespacePrefix = name;
    const CSSParserToken& nameToken = range.consume();
    if (nameToken.type() == IdentToken) {
        name = nameToken.value().toAtomString();
    } else if (nameToken.type() == DelimiterToken && nameToken.delimiter() == '*')
        name = starAtom();
    else {
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
    selector->setMatch(CSSSelector::Id);
    
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
    selector->setMatch(CSSSelector::Class);
    
    // FIXME-NEWPARSER: Avoid having to do this, but the old parser does and we need
    // to be compatible for now.
    CSSParserToken token = range.consume();
    selector->setValue(token.value().toAtomString(), m_context.mode == HTMLQuirksMode);

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
        selector->setAttribute(qualifiedName, m_context.isHTMLDocument, CSSSelector::CaseSensitive);
        selector->setMatch(CSSSelector::Set);
        return selector;
    }

    selector->setMatch(consumeAttributeMatch(block));

    const CSSParserToken& attributeValue = block.consumeIncludingWhitespace();
    if (attributeValue.type() != IdentToken && attributeValue.type() != StringToken)
        return nullptr;
    selector->setValue(attributeValue.value().toAtomString());
    
    selector->setAttribute(qualifiedName, m_context.isHTMLDocument, consumeAttributeFlags(block));

    if (!block.atEnd())
        return nullptr;
    return selector;
}

static bool isOnlyPseudoClassFunction(CSSSelector::PseudoClassType pseudoClassType)
{
    switch (pseudoClassType) {
    case CSSSelector::PseudoClassNot:
    case CSSSelector::PseudoClassIs:
    case CSSSelector::PseudoClassMatches:
    case CSSSelector::PseudoClassWhere:
    case CSSSelector::PseudoClassHas:
    case CSSSelector::PseudoClassNthChild:
    case CSSSelector::PseudoClassNthLastChild:
    case CSSSelector::PseudoClassNthOfType:
    case CSSSelector::PseudoClassNthLastOfType:
    case CSSSelector::PseudoClassLang:
    case CSSSelector::PseudoClassAny:
    case CSSSelector::PseudoClassDir:
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
        if (selector->match() == CSSSelector::PseudoClass) {
            if (!m_context.focusVisibleEnabled && selector->pseudoClassType() == CSSSelector::PseudoClassFocusVisible)
                return nullptr;
            if (!m_context.hasPseudoClassEnabled && selector->pseudoClassType() == CSSSelector::PseudoClassHas)
                return nullptr;
#if ENABLE(ATTACHMENT_ELEMENT)
            if (!DeprecatedGlobalSettings::attachmentElementEnabled() && selector->pseudoClassType() == CSSSelector::PseudoClassHasAttachment)
                return nullptr;
#endif
        }
    } else {
        selector = CSSParserSelector::parsePseudoElementSelector(token.value());
#if ENABLE(VIDEO)
        // Treat the ident version of cue as PseudoElementWebkitCustom.
        if (token.type() == IdentToken && selector && selector->match() == CSSSelector::PseudoElement && selector->pseudoElementType() == CSSSelector::PseudoElementCue)
            selector->setPseudoElementType(CSSSelector::PseudoElementWebKitCustom);
#endif
    }

    if (!selector)
        return nullptr;

    // Pseudo-elements are not allowed inside pseudo-classes or pseudo-elements.
    if (selector->match() == CSSSelector::PseudoElement && m_disallowPseudoElements)
        return nullptr;
    SetForScope disallowPseudoElementsScope(m_disallowPseudoElements, true);

    if (token.type() == IdentToken) {
        range.consume();
        if ((selector->match() == CSSSelector::PseudoElement && (selector->pseudoElementType() == CSSSelector::PseudoElementUnknown || isOnlyPseudoElementFunction(selector->pseudoElementType())))
            || (selector->match() == CSSSelector::PseudoClass && (selector->pseudoClassType() == CSSSelector::PseudoClassUnknown || isOnlyPseudoClassFunction(selector->pseudoClassType()))))
            return nullptr;
        return selector;
    }

    CSSParserTokenRange block = range.consumeBlock();
    block.consumeWhitespace();
    if (token.type() != FunctionToken)
        return nullptr;

    if (selector->match() == CSSSelector::PseudoClass) {
        switch (selector->pseudoClassType()) {
        case CSSSelector::PseudoClassNot: {
            SetForScope resistDefaultNamespace(m_resistDefaultNamespace, true);
            auto selectorList = makeUnique<CSSSelectorList>();
            *selectorList = consumeComplexSelectorList(block);
            if (!selectorList->first() || !block.atEnd())
                return nullptr;
            selector->setSelectorList(WTFMove(selectorList));
            return selector;
        }
        case CSSSelector::PseudoClassNthChild:
        case CSSSelector::PseudoClassNthLastChild:
        case CSSSelector::PseudoClassNthOfType:
        case CSSSelector::PseudoClassNthLastOfType: {
            std::pair<int, int> ab;
            if (!consumeANPlusB(block, ab))
                return nullptr;
            block.consumeWhitespace();
            // FIXME: We should be able to do this lazily. See: https://bugs.webkit.org/show_bug.cgi?id=217149
            selector->setArgument(serializeANPlusB(ab));
            if (!block.atEnd()) {
                if (block.peek().type() != IdentToken)
                    return nullptr;
                const CSSParserToken& ident = block.consume();
                if (!equalLettersIgnoringASCIICase(ident.value(), "of"_s))
                    return nullptr;
                if (block.peek().type() != WhitespaceToken)
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
        case CSSSelector::PseudoClassLang: {
            auto list = consumeLangArgumentList(block);
            if (list.isEmpty() || !block.atEnd())
                return nullptr;
            selector->setArgumentList(WTFMove(list));
            return selector;
        }
        case CSSSelector::PseudoClassIs:
        case CSSSelector::PseudoClassWhere:
        case CSSSelector::PseudoClassMatches:
        case CSSSelector::PseudoClassAny: {
            SetForScope resistDefaultNamespace(m_resistDefaultNamespace, true);
            auto selectorList = makeUnique<CSSSelectorList>();
            *selectorList = consumeForgivingComplexSelectorList(block);
            if (!block.atEnd())
                return nullptr;
            selector->setSelectorList(WTFMove(selectorList));
            return selector;
        }
        case CSSSelector::PseudoClassHost: {
            auto innerSelector = consumeCompoundSelector(block);
            block.consumeWhitespace();
            if (!innerSelector || !block.atEnd())
                return nullptr;
            selector->adoptSelectorVector(Vector<std::unique_ptr<CSSParserSelector>>::from(WTFMove(innerSelector)));
            return selector;
        }
        case CSSSelector::PseudoClassHas: {
            if (m_disallowHasPseudoClass)
                return nullptr;
            SetForScope resistDefaultNamespace(m_resistDefaultNamespace, true);
            SetForScope disallowNestedHas(m_disallowHasPseudoClass, true);
            auto selectorList = makeUnique<CSSSelectorList>();
            *selectorList = consumeForgivingRelativeSelectorList(block);
            if (selectorList->isEmpty() || !block.atEnd())
                return nullptr;
            selector->setSelectorList(WTFMove(selectorList));
            return selector;
        }
        case CSSSelector::PseudoClassDir: {
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
    
    if (selector->match() == CSSSelector::PseudoElement) {
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
    auto fallbackResult = CSSSelector::Subselector;
    while (range.peek().type() == WhitespaceToken) {
        range.consume();
        fallbackResult = CSSSelector::DescendantSpace;
    }

    if (range.peek().type() != DelimiterToken)
        return fallbackResult;

    UChar delimiter = range.peek().delimiter();

    if (delimiter == '+' || delimiter == '~' || delimiter == '>') {
        range.consumeIncludingWhitespace();
        if (delimiter == '+')
            return CSSSelector::DirectAdjacent;
        if (delimiter == '~')
            return CSSSelector::IndirectAdjacent;
        return CSSSelector::Child;
    }

    return fallbackResult;
}

CSSSelector::Match CSSSelectorParser::consumeAttributeMatch(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.consumeIncludingWhitespace();
    switch (token.type()) {
    case IncludeMatchToken:
        return CSSSelector::List;
    case DashMatchToken:
        return CSSSelector::Hyphen;
    case PrefixMatchToken:
        return CSSSelector::Begin;
    case SuffixMatchToken:
        return CSSSelector::End;
    case SubstringMatchToken:
        return CSSSelector::Contain;
    case DelimiterToken:
        if (token.delimiter() == '=')
            return CSSSelector::Exact;
        FALLTHROUGH;
    default:
        m_failedParsing = true;
        return CSSSelector::Exact;
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

std::unique_ptr<CSSParserSelector> CSSSelectorParser::splitCompoundAtImplicitShadowCrossingCombinator(std::unique_ptr<CSSParserSelector> compoundSelector, const CSSParserContext& context)
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
    bool isPart = splitAfter->tagHistory()->match() == CSSSelector::PseudoElement && splitAfter->tagHistory()->pseudoElementType() == CSSSelector::PseudoElementPart;

    // ::slotted() combines with other pseudo elements.
    bool isSlotted = splitAfter->tagHistory()->match() == CSSSelector::PseudoElement && splitAfter->tagHistory()->pseudoElementType() == CSSSelector::PseudoElementSlotted;

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
            return CSSSelector::ShadowSlotted;
        if (isPart)
            return CSSSelector::ShadowPartDescendant;
        return CSSSelector::ShadowDescendant;
    }();
    secondCompound->appendTagHistory(relation, WTFMove(compoundSelector));
    return secondCompound;
}

bool CSSSelectorParser::containsUnknownWebKitPseudoElements(const CSSSelector& complexSelector)
{
    for (auto current = &complexSelector; current; current = current->tagHistory()) {
        if (current->match() == CSSSelector::PseudoElement && current->pseudoElementType() == CSSSelector::PseudoElementWebKitCustom)
            return true;
    }

    return false;
}

} // namespace WebCore
