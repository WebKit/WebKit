// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "CSSParserImpl.h"

#include "CSSAtRuleID.h"
#include "CSSCounterStyleRule.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFontFamily.h"
#include "CSSFontPaletteValuesOverrideColorsValue.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSParserObserver.h"
#include "CSSParserObserverWrapper.h"
#include "CSSParserSelector.h"
#include "CSSPropertyParser.h"
#include "CSSSelectorParser.h"
#include "CSSStyleSheet.h"
#include "CSSSupportsParser.h"
#include "CSSTokenizer.h"
#include "CSSVariableParser.h"
#include "ContainerQueryParser.h"
#include "Document.h"
#include "Element.h"
#include "FontPaletteValues.h"
#include "LegacyMediaQueryParser.h"
#include "MediaList.h"
#include "MediaQueryParserContext.h"
#include "StyleProperties.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"

#include <bitset>
#include <memory>

namespace WebCore {

CSSParserImpl::CSSParserImpl(const CSSParserContext& context, StyleSheetContents* styleSheet)
    : m_context(context)
    , m_styleSheet(styleSheet)
{
    
}

CSSParserImpl::CSSParserImpl(const CSSParserContext& context, const String& string, StyleSheetContents* styleSheet, CSSParserObserverWrapper* wrapper)
    : m_context(context)
    , m_styleSheet(styleSheet)
    , m_tokenizer(wrapper ? CSSTokenizer::tryCreate(string, *wrapper) : CSSTokenizer::tryCreate(string))
    , m_observerWrapper(wrapper)
{
}

CSSParser::ParseResult CSSParserImpl::parseValue(MutableStyleProperties* declaration, CSSPropertyID propertyID, const String& string, bool important, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    auto ruleType = context.enclosingRuleType.value_or(StyleRuleType::Style);
    parser.consumeDeclarationValue(parser.tokenizer()->tokenRange(), propertyID, important, ruleType);
    if (parser.m_parsedProperties.isEmpty())
        return CSSParser::ParseResult::Error;
    return declaration->addParsedProperties(parser.m_parsedProperties) ? CSSParser::ParseResult::Changed : CSSParser::ParseResult::Unchanged;
}

CSSParser::ParseResult CSSParserImpl::parseCustomPropertyValue(MutableStyleProperties* declaration, const AtomString& propertyName, const String& string, bool important, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    parser.consumeCustomPropertyValue(parser.tokenizer()->tokenRange(), propertyName, important);
    if (parser.m_parsedProperties.isEmpty())
        return CSSParser::ParseResult::Error;
    return declaration->addParsedProperties(parser.m_parsedProperties) ? CSSParser::ParseResult::Changed : CSSParser::ParseResult::Unchanged;
}

static inline void filterProperties(bool important, const ParsedPropertyVector& input, ParsedPropertyVector& output, size_t& unusedEntries, std::bitset<numCSSProperties>& seenProperties, HashSet<AtomString>& seenCustomProperties)
{
    // Add properties in reverse order so that highest priority definitions are reached first. Duplicate definitions can then be ignored when found.
    for (size_t i = input.size(); i--; ) {
        const CSSProperty& property = input[i];
        if (property.isImportant() != important)
            continue;
        const unsigned propertyIDIndex = property.id() - firstCSSProperty;
        
        if (property.id() == CSSPropertyCustom) {
            auto& name = downcast<CSSCustomPropertyValue>(*property.value()).name();
            if (!seenCustomProperties.add(name).isNewEntry)
                continue;
            output[--unusedEntries] = property;
            continue;
        }

        auto seenPropertyBit = seenProperties[propertyIDIndex];
        if (seenPropertyBit)
            continue;
        seenPropertyBit = true;

        output[--unusedEntries] = property;
    }
}

static Ref<ImmutableStyleProperties> createStyleProperties(ParsedPropertyVector& parsedProperties, CSSParserMode mode)
{
    std::bitset<numCSSProperties> seenProperties;
    size_t unusedEntries = parsedProperties.size();
    ParsedPropertyVector results(unusedEntries);
    HashSet<AtomString> seenCustomProperties;

    filterProperties(true, parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
    filterProperties(false, parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);

    Ref<ImmutableStyleProperties> result = ImmutableStyleProperties::create(results.data() + unusedEntries, results.size() - unusedEntries, mode);
    parsedProperties.clear();
    return result;
}

Ref<ImmutableStyleProperties> CSSParserImpl::parseInlineStyleDeclaration(const String& string, const Element* element)
{
    CSSParserContext context(element->document());
    context.mode = strictToCSSParserMode(element->isHTMLElement() && !element->document().inQuirksMode());

    CSSParserImpl parser(context, string);
    parser.consumeDeclarationList(parser.tokenizer()->tokenRange(), StyleRuleType::Style);
    return createStyleProperties(parser.m_parsedProperties, context.mode);
}

bool CSSParserImpl::parseDeclarationList(MutableStyleProperties* declaration, const String& string, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    auto ruleType = context.enclosingRuleType.value_or(StyleRuleType::Style);
    parser.consumeDeclarationList(parser.tokenizer()->tokenRange(), ruleType);
    if (parser.m_parsedProperties.isEmpty())
        return false;

    std::bitset<numCSSProperties> seenProperties;
    size_t unusedEntries = parser.m_parsedProperties.size();
    ParsedPropertyVector results(unusedEntries);
    HashSet<AtomString> seenCustomProperties;
    filterProperties(true, parser.m_parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
    filterProperties(false, parser.m_parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
    if (unusedEntries)
        results.remove(0, unusedEntries);
    return declaration->addParsedProperties(results);
}

RefPtr<StyleRuleBase> CSSParserImpl::parseRule(const String& string, const CSSParserContext& context, StyleSheetContents* styleSheet, AllowedRulesType allowedRules)
{
    CSSParserImpl parser(context, string, styleSheet);
    CSSParserTokenRange range = parser.tokenizer()->tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr; // Parse error, empty rule
    RefPtr<StyleRuleBase> rule;
    if (range.peek().type() == AtKeywordToken)
        rule = parser.consumeAtRule(range, allowedRules);
    else
        rule = parser.consumeQualifiedRule(range, allowedRules);
    if (!rule)
        return nullptr; // Parse error, failed to consume rule
    range.consumeWhitespace();
    if (!rule || !range.atEnd())
        return nullptr; // Parse error, trailing garbage
    return rule;
}

void CSSParserImpl::parseStyleSheet(const String& string, const CSSParserContext& context, StyleSheetContents& styleSheet)
{
    CSSParserImpl parser(context, string, &styleSheet, nullptr);
    bool firstRuleValid = parser.consumeRuleList(parser.tokenizer()->tokenRange(), TopLevelRuleList, [&](RefPtr<StyleRuleBase> rule) {
        if (rule->isCharsetRule())
            return;
        if (context.shouldIgnoreImportRules && rule->isImportRule())
            return;
        styleSheet.parserAppendRule(rule.releaseNonNull());
    });
    styleSheet.setHasSyntacticallyValidCSSHeader(firstRuleValid);
    styleSheet.shrinkToFit();
}

CSSSelectorList CSSParserImpl::parsePageSelector(CSSParserTokenRange range, StyleSheetContents* styleSheet)
{
    // We only support a small subset of the css-page spec.
    range.consumeWhitespace();
    AtomString typeSelector;
    if (range.peek().type() == IdentToken)
        typeSelector = range.consume().value().toAtomString();

    StringView pseudo;
    if (range.peek().type() == ColonToken) {
        range.consume();
        if (range.peek().type() != IdentToken)
            return CSSSelectorList();
        pseudo = range.consume().value();
    }

    range.consumeWhitespace();
    if (!range.atEnd())
        return CSSSelectorList(); // Parse error; extra tokens in @page selector

    std::unique_ptr<CSSParserSelector> selector;
    if (!typeSelector.isNull() && pseudo.isNull())
        selector = makeUnique<CSSParserSelector>(QualifiedName(nullAtom(), typeSelector, styleSheet->defaultNamespace()));
    else {
        selector = makeUnique<CSSParserSelector>();
        if (!pseudo.isNull()) {
            selector = std::unique_ptr<CSSParserSelector>(CSSParserSelector::parsePagePseudoSelector(pseudo));
            if (!selector || selector->match() != CSSSelector::PagePseudoClass)
                return CSSSelectorList();
        }
        if (!typeSelector.isNull())
            selector->prependTagSelector(QualifiedName(nullAtom(), typeSelector, styleSheet->defaultNamespace()));
    }

    selector->setForPage();
    return CSSSelectorList { Vector<std::unique_ptr<CSSParserSelector>>::from(WTFMove(selector)) };
}

Vector<double> CSSParserImpl::parseKeyframeKeyList(const String& keyList)
{
    return consumeKeyframeKeyList(CSSTokenizer(keyList).tokenRange());
}

bool CSSParserImpl::supportsDeclaration(CSSParserTokenRange& range)
{
    ASSERT(m_parsedProperties.isEmpty());
    consumeDeclaration(range, StyleRuleType::Style);
    bool result = !m_parsedProperties.isEmpty();
    m_parsedProperties.clear();
    return result;
}

void CSSParserImpl::parseDeclarationListForInspector(const String& declaration, const CSSParserContext& context, CSSParserObserver& observer)
{
    CSSParserObserverWrapper wrapper(observer);
    CSSParserImpl parser(context, declaration, nullptr, &wrapper);
    observer.startRuleHeader(StyleRuleType::Style, 0);
    observer.endRuleHeader(1);
    parser.consumeDeclarationList(parser.tokenizer()->tokenRange(), StyleRuleType::Style);
}

void CSSParserImpl::parseStyleSheetForInspector(const String& string, const CSSParserContext& context, StyleSheetContents* styleSheet, CSSParserObserver& observer)
{
    CSSParserObserverWrapper wrapper(observer);
    CSSParserImpl parser(context, string, styleSheet, &wrapper);
    bool firstRuleValid = parser.consumeRuleList(parser.tokenizer()->tokenRange(), TopLevelRuleList, [&styleSheet](RefPtr<StyleRuleBase> rule) {
        if (rule->isCharsetRule())
            return;
        styleSheet->parserAppendRule(rule.releaseNonNull());
    });
    styleSheet->setHasSyntacticallyValidCSSHeader(firstRuleValid);
}

static CSSParserImpl::AllowedRulesType computeNewAllowedRules(CSSParserImpl::AllowedRulesType allowedRules, StyleRuleBase* rule)
{
    if (!rule || allowedRules == CSSParserImpl::KeyframeRules || allowedRules == CSSParserImpl::CounterStyleRules || allowedRules == CSSParserImpl::NoRules)
        return allowedRules;
    
    ASSERT(allowedRules <= CSSParserImpl::RegularRules);
    if (rule->isCharsetRule())
        return CSSParserImpl::AllowLayerStatementRules;
    if (allowedRules <= CSSParserImpl::AllowLayerStatementRules && rule->isLayerRule() && downcast<StyleRuleLayer>(*rule).isStatement())
        return CSSParserImpl::AllowLayerStatementRules;
    if (rule->isImportRule())
        return CSSParserImpl::AllowImportRules;
    if (rule->isNamespaceRule())
        return CSSParserImpl::AllowNamespaceRules;
    return CSSParserImpl::RegularRules;
}

template<typename T>
bool CSSParserImpl::consumeRuleList(CSSParserTokenRange range, RuleListType ruleListType, const T callback)
{
    AllowedRulesType allowedRules = RegularRules;
    switch (ruleListType) {
    case TopLevelRuleList:
        allowedRules = AllowCharsetRules;
        break;
    case RegularRuleList:
        allowedRules = RegularRules;
        break;
    case KeyframesRuleList:
        allowedRules = KeyframeRules;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    bool seenRule = false;
    bool firstRuleValid = false;
    while (!range.atEnd()) {
        RefPtr<StyleRuleBase> rule;
        switch (range.peek().type()) {
        case WhitespaceToken:
            range.consumeWhitespace();
            continue;
        case AtKeywordToken:
            rule = consumeAtRule(range, allowedRules);
            break;
        case CDOToken:
        case CDCToken:
            if (ruleListType == TopLevelRuleList) {
                range.consume();
                continue;
            }
            FALLTHROUGH;
        default:
            rule = consumeQualifiedRule(range, allowedRules);
            break;
        }
        if (!seenRule) {
            seenRule = true;
            firstRuleValid = rule;
        }
        if (rule) {
            allowedRules = computeNewAllowedRules(allowedRules, rule.get());
            callback(rule);
        }
    }

    return firstRuleValid;
}

RefPtr<StyleRuleBase> CSSParserImpl::consumeAtRule(CSSParserTokenRange& range, AllowedRulesType allowedRules)
{
    ASSERT(range.peek().type() == AtKeywordToken);
    const StringView name = range.consumeIncludingWhitespace().value();
    const CSSParserToken* preludeStart = &range.peek();
    while (!range.atEnd() && range.peek().type() != LeftBraceToken && range.peek().type() != SemicolonToken)
        range.consumeComponentValue();

    CSSParserTokenRange prelude = range.makeSubRange(preludeStart, &range.peek());
    CSSAtRuleID id = cssAtRuleID(name);
    
    if (range.atEnd() || range.peek().type() == SemicolonToken) {
        range.consume();
        if (allowedRules == AllowCharsetRules && id == CSSAtRuleCharset)
            return consumeCharsetRule(prelude);
        if (allowedRules <= AllowImportRules && id == CSSAtRuleImport)
            return consumeImportRule(prelude);
        if (allowedRules <= AllowNamespaceRules && id == CSSAtRuleNamespace)
            return consumeNamespaceRule(prelude);
        if (allowedRules <= RegularRules && id == CSSAtRuleLayer)
            return consumeLayerRule(prelude, { });
        return nullptr; // Parse error, unrecognised at-rule without block
    }

    CSSParserTokenRange block = range.consumeBlock();
    if (allowedRules == KeyframeRules)
        return nullptr; // Parse error, no at-rules supported inside @keyframes
    if (allowedRules == NoRules)
        return nullptr;

    ASSERT(allowedRules <= RegularRules);

    switch (id) {
    case CSSAtRuleMedia:
        return consumeMediaRule(prelude, block);
    case CSSAtRuleSupports:
        return consumeSupportsRule(prelude, block);
    case CSSAtRuleFontFace:
        return consumeFontFaceRule(prelude, block);
    case CSSAtRuleFontPaletteValues:
        return consumeFontPaletteValuesRule(prelude, block);
    case CSSAtRuleWebkitKeyframes:
    case CSSAtRuleKeyframes:
        return consumeKeyframesRule(prelude, block);
    case CSSAtRulePage:
        return consumePageRule(prelude, block);
    case CSSAtRuleCounterStyle:
        return consumeCounterStyleRule(prelude, block);
    case CSSAtRuleLayer:
        return consumeLayerRule(prelude, block);
    case CSSAtRuleContainer:
        return consumeContainerRule(prelude, block);
    default:
        return nullptr; // Parse error, unrecognised at-rule with block
    }
}

RefPtr<StyleRuleBase> CSSParserImpl::consumeQualifiedRule(CSSParserTokenRange& range, AllowedRulesType allowedRules)
{
    const CSSParserToken* preludeStart = &range.peek();
    while (!range.atEnd() && range.peek().type() != LeftBraceToken)
        range.consumeComponentValue();

    if (range.atEnd())
        return nullptr; // Parse error, EOF instead of qualified rule block

    CSSParserTokenRange prelude = range.makeSubRange(preludeStart, &range.peek());
    CSSParserTokenRange block = range.consumeBlockCheckingForEditability(m_styleSheet.get());

    if (allowedRules <= RegularRules)
        return consumeStyleRule(prelude, block);
    if (allowedRules == KeyframeRules)
        return consumeKeyframeStyleRule(prelude, block);

    ASSERT_NOT_REACHED();
    return nullptr;
}

// This may still consume tokens if it fails
static AtomString consumeStringOrURI(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();

    if (token.type() == StringToken || token.type() == UrlToken)
        return range.consumeIncludingWhitespace().value().toAtomString();

    if (token.type() != FunctionToken || !equalLettersIgnoringASCIICase(token.value(), "url"_s))
        return AtomString();

    CSSParserTokenRange contents = range.consumeBlock();
    const CSSParserToken& uri = contents.consumeIncludingWhitespace();
    if (uri.type() == BadStringToken || !contents.atEnd())
        return AtomString();
    return uri.value().toAtomString();
}

RefPtr<StyleRuleCharset> CSSParserImpl::consumeCharsetRule(CSSParserTokenRange prelude)
{
    const CSSParserToken& string = prelude.consumeIncludingWhitespace();
    if (string.type() != StringToken || !prelude.atEnd())
        return nullptr; // Parse error, expected a single string
    return StyleRuleCharset::create();
}

enum class AllowAnonymous { Yes, No };
static std::optional<CascadeLayerName> consumeCascadeLayerName(CSSParserTokenRange& range, AllowAnonymous allowAnonymous)
{
    CascadeLayerName name;
    if (range.atEnd()) {
        if (allowAnonymous == AllowAnonymous::Yes)
            return name;
        return { };
    }

    while (true) {
        auto nameToken = range.consume();
        if (nameToken.type() != IdentToken)
            return { };

        name.append(nameToken.value().toAtomString());

        if (range.peek().type() != DelimiterToken || range.peek().delimiter() != '.')
            break;
        range.consume();
    }

    range.consumeWhitespace();
    return name;
}

RefPtr<StyleRuleImport> CSSParserImpl::consumeImportRule(CSSParserTokenRange prelude)
{
    AtomString uri(consumeStringOrURI(prelude));
    if (uri.isNull())
        return nullptr; // Parse error, expected string or URI

    if (m_observerWrapper) {
        unsigned endOffset = m_observerWrapper->endOffset(prelude);
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::Import, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(endOffset);
        m_observerWrapper->observer().startRuleBody(endOffset);
        m_observerWrapper->observer().endRuleBody(endOffset);
    }

    prelude.consumeWhitespace();

    auto consumeCascadeLayer = [&]() -> std::optional<CascadeLayerName> {
        if (!m_context.cascadeLayersEnabled)
            return { };

        auto& token = prelude.peek();
        if (token.type() == FunctionToken && equalLettersIgnoringASCIICase(token.value(), "layer"_s)) {
            auto savedPreludeForFailure = prelude;
            auto contents = CSSPropertyParserHelpers::consumeFunction(prelude);
            auto layerName = consumeCascadeLayerName(contents, AllowAnonymous::No);
            if (!layerName || !contents.atEnd()) {
                prelude = savedPreludeForFailure;
                return { };
            }
            return layerName;
        }
        if (token.type() == IdentToken && equalLettersIgnoringASCIICase(token.value(), "layer"_s)) {
            prelude.consumeIncludingWhitespace();
            return CascadeLayerName { };
        }
        return { };
    };

    auto cascadeLayerName = consumeCascadeLayer();
    auto mediaQuerySet = LegacyMediaQueryParser::parseMediaQuerySet(prelude, MediaQueryParserContext(m_context));
    
    return StyleRuleImport::create(uri, mediaQuerySet.releaseNonNull(), WTFMove(cascadeLayerName));
}

RefPtr<StyleRuleNamespace> CSSParserImpl::consumeNamespaceRule(CSSParserTokenRange prelude)
{
    AtomString namespacePrefix;
    if (prelude.peek().type() == IdentToken)
        namespacePrefix = prelude.consumeIncludingWhitespace().value().toAtomString();

    AtomString uri(consumeStringOrURI(prelude));
    if (uri.isNull() || !prelude.atEnd())
        return nullptr; // Parse error, expected string or URI

    return StyleRuleNamespace::create(namespacePrefix, uri);
}

RefPtr<StyleRuleMedia> CSSParserImpl::consumeMediaRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    Vector<RefPtr<StyleRuleBase>> rules;

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::Media, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(block));
    }

    consumeRuleList(block, RegularRuleList, [&rules](RefPtr<StyleRuleBase> rule) {
        rules.append(rule);
    });
    rules.shrinkToFit();

    if (m_observerWrapper)
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(block));

    return StyleRuleMedia::create(LegacyMediaQueryParser::parseMediaQuerySet(prelude, { m_context }).releaseNonNull(), WTFMove(rules));
}

RefPtr<StyleRuleSupports> CSSParserImpl::consumeSupportsRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    auto supported = CSSSupportsParser::supportsCondition(prelude, *this, CSSSupportsParser::ForAtRule);
    if (supported == CSSSupportsParser::Invalid)
        return nullptr; // Parse error, invalid @supports condition

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::Supports, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(block));
    }

    Vector<RefPtr<StyleRuleBase>> rules;
    consumeRuleList(block, RegularRuleList, [&rules](RefPtr<StyleRuleBase> rule) {
        rules.append(rule);
    });
    rules.shrinkToFit();

    if (m_observerWrapper)
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(block));

    return StyleRuleSupports::create(prelude.serialize().stripWhiteSpace(), supported, WTFMove(rules));
}

RefPtr<StyleRuleFontFace> CSSParserImpl::consumeFontFaceRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (!prelude.atEnd())
        return nullptr; // Parse error; @font-face prelude should be empty

    if (m_observerWrapper) {
        unsigned endOffset = m_observerWrapper->endOffset(prelude);
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::FontFace, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(endOffset);
        m_observerWrapper->observer().startRuleBody(endOffset);
        m_observerWrapper->observer().endRuleBody(endOffset);
    }

    consumeDeclarationList(block, StyleRuleType::FontFace);
    return StyleRuleFontFace::create(createStyleProperties(m_parsedProperties, m_context.mode));
}

RefPtr<StyleRuleFontPaletteValues> CSSParserImpl::consumeFontPaletteValuesRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    auto name = CSSPropertyParserHelpers::consumeDashedIdent(prelude);
    if (!name || !prelude.atEnd())
        return nullptr; // Parse error; expected custom ident in @font-palette-values header

    if (m_observerWrapper) {
        unsigned endOffset = m_observerWrapper->endOffset(prelude);
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::FontPaletteValues, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(endOffset);
        m_observerWrapper->observer().startRuleBody(endOffset);
        m_observerWrapper->observer().endRuleBody(endOffset);
    }

    consumeDeclarationList(block, StyleRuleType::FontPaletteValues);
    auto properties = createStyleProperties(m_parsedProperties, m_context.mode);

    AtomString fontFamily;
    if (auto fontFamilyValue = properties->getPropertyCSSValue(CSSPropertyFontFamily))
        fontFamily = AtomString { downcast<CSSPrimitiveValue>(*fontFamilyValue).fontFamily().familyName };

    std::optional<FontPaletteIndex> basePalette;
    if (auto basePaletteValue = properties->getPropertyCSSValue(CSSPropertyBasePalette)) {
        const auto& primitiveValue = downcast<CSSPrimitiveValue>(*basePaletteValue);
        if (primitiveValue.isInteger())
            basePalette = FontPaletteIndex(primitiveValue.value<unsigned>());
        else if (primitiveValue.valueID() == CSSValueLight)
            basePalette = FontPaletteIndex(FontPaletteIndex::Type::Light);
        else if (primitiveValue.valueID() == CSSValueDark)
            basePalette = FontPaletteIndex(FontPaletteIndex::Type::Dark);
    }

    Vector<FontPaletteValues::OverriddenColor> overrideColors;
    if (auto overrideColorsValue = properties->getPropertyCSSValue(CSSPropertyOverrideColors)) {
        const auto& list = downcast<CSSValueList>(*overrideColorsValue);
        for (const auto& item : list) {
            const auto& pair = downcast<CSSFontPaletteValuesOverrideColorsValue>(item.get());
            if (!pair.key().isInteger())
                continue;
            unsigned key = pair.key().value<unsigned>();
            Color color = pair.color().isRGBColor() ? pair.color().color() : StyleColor::colorFromKeyword(pair.color().valueID(), { });
            overrideColors.append(std::make_pair(key, color));
        }
    }

    return StyleRuleFontPaletteValues::create(AtomString { name->stringValue() }, AtomString { fontFamily }, WTFMove(basePalette), WTFMove(overrideColors));
}

RefPtr<StyleRuleKeyframes> CSSParserImpl::consumeKeyframesRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    CSSParserTokenRange rangeCopy = prelude; // For inspector callbacks
    const CSSParserToken& nameToken = prelude.consumeIncludingWhitespace();
    if (!prelude.atEnd())
        return nullptr; // Parse error; expected single non-whitespace token in @keyframes header

    AtomString name;
    if (nameToken.type() == IdentToken) {
        name = nameToken.value().toAtomString();
    } else if (nameToken.type() == StringToken)
        name = nameToken.value().toAtomString();
    else
        return nullptr; // Parse error; expected ident token in @keyframes header

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::Keyframes, m_observerWrapper->startOffset(rangeCopy));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(block));
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(block));
    }

    RefPtr<StyleRuleKeyframes> keyframeRule = StyleRuleKeyframes::create(name);
    consumeRuleList(block, KeyframesRuleList, [keyframeRule](const RefPtr<StyleRuleBase>& keyframe) {
        keyframeRule->parserAppendKeyframe(downcast<const StyleRuleKeyframe>(keyframe.get()));
    });

    // FIXME-NEWPARSER: Find out why this is done. Behavior difference when prefixed?
    // keyframeRule->setVendorPrefixed(webkitPrefixed);
    
    keyframeRule->shrinkToFit();
    return keyframeRule;
}

RefPtr<StyleRulePage> CSSParserImpl::consumePageRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    CSSSelectorList selectorList = parsePageSelector(prelude, m_styleSheet.get());
    if (selectorList.isEmpty())
        return nullptr; // Parse error, invalid @page selector

    if (m_observerWrapper) {
        unsigned endOffset = m_observerWrapper->endOffset(prelude);
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::Page, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(endOffset);
    }

    consumeDeclarationList(block, StyleRuleType::Style);
    
    return StyleRulePage::create(createStyleProperties(m_parsedProperties, m_context.mode), WTFMove(selectorList));
}

RefPtr<StyleRuleCounterStyle> CSSParserImpl::consumeCounterStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (!m_context.propertySettings.cssCounterStyleAtRulesEnabled)
        return nullptr;

    auto rangeCopy = prelude; // For inspector callbacks
    auto name = CSSPropertyParserHelpers::consumeCounterStyleNameInPrelude(rangeCopy);
    if (name.isNull())
        return nullptr;

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::CounterStyle, m_observerWrapper->startOffset(rangeCopy));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(block));
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(block));
    }

    consumeDeclarationList(block, StyleRuleType::CounterStyle);
    return StyleRuleCounterStyle::create(name, createStyleProperties(m_parsedProperties, m_context.mode));
}

RefPtr<StyleRuleLayer> CSSParserImpl::consumeLayerRule(CSSParserTokenRange prelude, std::optional<CSSParserTokenRange> block)
{
    if (!m_context.cascadeLayersEnabled)
        return nullptr;

    auto preludeCopy = prelude;

    if (!block) {
        // List syntax.
        Vector<CascadeLayerName> nameList;
        while (true) {
            auto name = consumeCascadeLayerName(prelude, AllowAnonymous::No);
            if (!name)
                return nullptr;
            nameList.append(*name);

            if (prelude.atEnd())
                break;

            auto commaToken = prelude.consumeIncludingWhitespace();
            if (commaToken.type() != CommaToken)
                return { };
        }

        if (m_observerWrapper) {
            unsigned endOffset = m_observerWrapper->endOffset(preludeCopy);
            m_observerWrapper->observer().startRuleHeader(StyleRuleType::LayerStatement, m_observerWrapper->startOffset(preludeCopy));
            m_observerWrapper->observer().endRuleHeader(endOffset);
            m_observerWrapper->observer().startRuleBody(endOffset);
            m_observerWrapper->observer().endRuleBody(endOffset);
        }

        return StyleRuleLayer::createStatement(WTFMove(nameList));
    }

    auto name = consumeCascadeLayerName(prelude, AllowAnonymous::Yes);
    if (!name)
        return nullptr;

    // No comma separated list when using the block syntax.
    if (!prelude.atEnd())
        return nullptr;

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::LayerBlock, m_observerWrapper->startOffset(preludeCopy));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(preludeCopy));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(*block));
    }

    Vector<RefPtr<StyleRuleBase>> rules;
    consumeRuleList(*block, RegularRuleList, [&](RefPtr<StyleRuleBase> rule) {
        rules.append(rule);
    });
    rules.shrinkToFit();

    if (m_observerWrapper)
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(*block));

    return StyleRuleLayer::createBlock(WTFMove(*name), WTFMove(rules));
}

RefPtr<StyleRuleContainer> CSSParserImpl::consumeContainerRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (!m_context.propertySettings.cssContainerQueriesEnabled)
        return nullptr;

    if (prelude.atEnd())
        return nullptr;

    auto query = ContainerQueryParser::consumeContainerQuery(prelude, m_context);
    if (!query)
        return nullptr;

    prelude.consumeWhitespace();
    if (!prelude.atEnd())
        return nullptr;

    Vector<RefPtr<StyleRuleBase>> rules;

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::Container, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(block));
    }

    consumeRuleList(block, RegularRuleList, [&rules](RefPtr<StyleRuleBase> rule) {
        rules.append(rule);
    });
    rules.shrinkToFit();

    if (m_observerWrapper)
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(block));

    return StyleRuleContainer::create(WTFMove(*query), WTFMove(rules));
}
    
RefPtr<StyleRuleKeyframe> CSSParserImpl::consumeKeyframeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    auto keyList = consumeKeyframeKeyList(prelude);
    if (keyList.isEmpty())
        return nullptr;

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::Keyframe, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
    }

    consumeDeclarationList(block, StyleRuleType::Keyframe);
    return StyleRuleKeyframe::create(WTFMove(keyList), createStyleProperties(m_parsedProperties, m_context.mode));
}

static void observeSelectors(CSSParserObserverWrapper& wrapper, CSSParserTokenRange selectors)
{
    // This is easier than hooking into the CSSSelectorParser
    selectors.consumeWhitespace();
    CSSParserTokenRange originalRange = selectors;
    wrapper.observer().startRuleHeader(StyleRuleType::Style, wrapper.startOffset(originalRange));

    while (!selectors.atEnd()) {
        const CSSParserToken* selectorStart = &selectors.peek();
        while (!selectors.atEnd() && selectors.peek().type() != CommaToken)
            selectors.consumeComponentValue();
        CSSParserTokenRange selector = selectors.makeSubRange(selectorStart, &selectors.peek());
        selectors.consumeIncludingWhitespace();

        wrapper.observer().observeSelector(wrapper.startOffset(selector), wrapper.endOffset(selector));
    }

    wrapper.observer().endRuleHeader(wrapper.endOffset(originalRange));
}

RefPtr<StyleRule> CSSParserImpl::consumeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    auto selectorList = parseCSSSelector(prelude, m_context, m_styleSheet.get());
    if (!selectorList)
        return nullptr; // Parse error, invalid selector list

    if (m_observerWrapper)
        observeSelectors(*m_observerWrapper, prelude);
    
    consumeDeclarationList(block, StyleRuleType::Style);
    return StyleRule::create(createStyleProperties(m_parsedProperties, m_context.mode), m_context.hasDocumentSecurityOrigin, WTFMove(*selectorList));
}

void CSSParserImpl::consumeDeclarationList(CSSParserTokenRange range, StyleRuleType ruleType)
{
    ASSERT(m_parsedProperties.isEmpty());

    bool useObserver = m_observerWrapper && (ruleType == StyleRuleType::Style || ruleType == StyleRuleType::Keyframe || ruleType == StyleRuleType::CounterStyle);
    if (useObserver) {
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(range));
        m_observerWrapper->skipCommentsBefore(range, true);
    }

    while (!range.atEnd()) {
        switch (range.peek().type()) {
        case WhitespaceToken:
        case SemicolonToken:
            range.consume();
            break;
        case IdentToken: {
            const CSSParserToken* declarationStart = &range.peek();

            if (useObserver)
                m_observerWrapper->yieldCommentsBefore(range);

            while (!range.atEnd() && range.peek().type() != SemicolonToken)
                range.consumeComponentValue();

            consumeDeclaration(range.makeSubRange(declarationStart, &range.peek()), ruleType);

            if (useObserver)
                m_observerWrapper->skipCommentsBefore(range, false);
            break;
        }
        case AtKeywordToken: {
            RefPtr<StyleRuleBase> rule = consumeAtRule(range, NoRules);
            ASSERT_UNUSED(rule, !rule);
            break;
        }
        default: // Parse error, unexpected token in declaration list
            while (!range.atEnd() && range.peek().type() != SemicolonToken)
                range.consumeComponentValue();
            break;
        }
    }

    // Yield remaining comments
    if (useObserver) {
        m_observerWrapper->yieldCommentsBefore(range);
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(range));
    }
}

static void removeTrailingWhitespace(const CSSParserTokenRange& range, const CSSParserToken*& position)
{
    while (position != range.begin() && position[-1].type() == WhitespaceToken)
        --position;
}

// https://drafts.csswg.org/css-syntax/#consume-declaration
void CSSParserImpl::consumeDeclaration(CSSParserTokenRange range, StyleRuleType ruleType)
{
    CSSParserTokenRange rangeCopy = range; // For inspector callbacks

    ASSERT(range.peek().type() == IdentToken);
    auto& token = range.consumeIncludingWhitespace();
    auto propertyID = token.parseAsCSSPropertyID();
    if (range.consume().type() != ColonToken)
        return; // Parse error
    range.consumeWhitespace();

    auto declarationValueEnd = range.end();
    bool important = false;
    if (!range.atEnd()) {
        auto end = range.end();
        removeTrailingWhitespace(range, end);
        declarationValueEnd = end;
        if (end[-1].type() == IdentToken && equalLettersIgnoringASCIICase(end[-1].value(), "important"_s)) {
            --end;
            removeTrailingWhitespace(range, end);
            if (end[-1].type() == DelimiterToken && end[-1].delimiter() == '!') {
                important = true;
                --end;
                removeTrailingWhitespace(range, end);
                declarationValueEnd = end;
            }
        }
    }

    size_t propertiesCount = m_parsedProperties.size();

    if (!isCSSPropertyExposed(propertyID, &m_context.propertySettings))
        propertyID = CSSPropertyInvalid;

    if (propertyID == CSSPropertyInvalid && CSSVariableParser::isValidVariableName(token)) {
        AtomString variableName = token.value().toAtomString();
        consumeCustomPropertyValue(range.makeSubRange(&range.peek(), declarationValueEnd), variableName, important);
    }

    if (important && (ruleType == StyleRuleType::FontFace || ruleType == StyleRuleType::Keyframe || ruleType == StyleRuleType::CounterStyle || ruleType == StyleRuleType::FontPaletteValues))
        return;

    if (propertyID != CSSPropertyInvalid)
        consumeDeclarationValue(range.makeSubRange(&range.peek(), declarationValueEnd), propertyID, important, ruleType);

    if (m_observerWrapper && (ruleType == StyleRuleType::Style || ruleType == StyleRuleType::Keyframe)) {
        m_observerWrapper->observer().observeProperty(
            m_observerWrapper->startOffset(rangeCopy), m_observerWrapper->endOffset(rangeCopy),
            important, m_parsedProperties.size() != propertiesCount);
    }
}

void CSSParserImpl::consumeCustomPropertyValue(CSSParserTokenRange range, const AtomString& variableName, bool important)
{
    if (range.atEnd())
        m_parsedProperties.append(CSSProperty(CSSPropertyCustom, CSSCustomPropertyValue::createEmpty(variableName), important));
    else if (auto value = CSSVariableParser::parseDeclarationValue(variableName, range, m_context))
        m_parsedProperties.append(CSSProperty(CSSPropertyCustom, WTFMove(value), important));
}

void CSSParserImpl::consumeDeclarationValue(CSSParserTokenRange range, CSSPropertyID propertyID, bool important, StyleRuleType ruleType)
{
    CSSPropertyParser::parseValue(propertyID, important, range, m_context, m_parsedProperties, ruleType);
}

Vector<double> CSSParserImpl::consumeKeyframeKeyList(CSSParserTokenRange range)
{
    Vector<double> result;
    while (true) {
        range.consumeWhitespace();
        const CSSParserToken& token = range.consumeIncludingWhitespace();
        if (token.type() == PercentageToken && token.numericValue() >= 0 && token.numericValue() <= 100)
            result.append(token.numericValue() / 100);
        else if (token.type() == IdentToken && equalLettersIgnoringASCIICase(token.value(), "from"_s))
            result.append(0);
        else if (token.type() == IdentToken && equalLettersIgnoringASCIICase(token.value(), "to"_s))
            result.append(1);
        else
            return { }; // Parser error, invalid value in keyframe selector

        if (range.atEnd()) {
            result.shrinkToFit();
            return result;
        }

        if (range.consume().type() != CommaToken)
            return { }; // Parser error
    }
}

} // namespace WebCore
