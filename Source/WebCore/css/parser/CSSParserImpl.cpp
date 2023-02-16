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
#include "CSSFontFeatureValuesRule.h"
#include "CSSFontPaletteValuesOverrideColorsValue.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSParserIdioms.h"
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
#include "MediaList.h"
#include "MediaQueryParser.h"
#include "MediaQueryParserContext.h"
#include "StylePropertiesInlines.h"
#include "StyleRule.h"
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
    if (parser.topContext().m_parsedProperties.isEmpty())
        return CSSParser::ParseResult::Error;
    return declaration->addParsedProperties(parser.topContext().m_parsedProperties) ? CSSParser::ParseResult::Changed : CSSParser::ParseResult::Unchanged;
}

CSSParser::ParseResult CSSParserImpl::parseCustomPropertyValue(MutableStyleProperties* declaration, const AtomString& propertyName, const String& string, bool important, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    parser.consumeCustomPropertyValue(parser.tokenizer()->tokenRange(), propertyName, important);
    if (parser.topContext().m_parsedProperties.isEmpty())
        return CSSParser::ParseResult::Error;
    return declaration->addParsedProperties(parser.topContext().m_parsedProperties) ? CSSParser::ParseResult::Changed : CSSParser::ParseResult::Unchanged;
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
    return createStyleProperties(parser.topContext().m_parsedProperties, context.mode);
}

bool CSSParserImpl::parseDeclarationList(MutableStyleProperties* declaration, const String& string, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    auto ruleType = context.enclosingRuleType.value_or(StyleRuleType::Style);
    parser.consumeDeclarationList(parser.tokenizer()->tokenRange(), ruleType);
    if (parser.topContext().m_parsedProperties.isEmpty())
        return false;

    std::bitset<numCSSProperties> seenProperties;
    size_t unusedEntries = parser.topContext().m_parsedProperties.size();
    ParsedPropertyVector results(unusedEntries);
    HashSet<AtomString> seenCustomProperties;
    filterProperties(true, parser.topContext().m_parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
    filterProperties(false, parser.topContext().m_parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
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
    ASSERT(topContext().m_parsedProperties.isEmpty());
    consumeDeclaration(range, StyleRuleType::Style);
    bool result = !topContext().m_parsedProperties.isEmpty();
    topContext().m_parsedProperties.clear();
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
    if (!rule || allowedRules == CSSParserImpl::FontFeatureValuesRules || allowedRules == CSSParserImpl::KeyframeRules || allowedRules == CSSParserImpl::CounterStyleRules || allowedRules == CSSParserImpl::NoRules)
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
    case FontFeatureValuesRuleList:
        allowedRules = FontFeatureValuesRules;
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

    switch (id) {
    case CSSAtRuleMedia:
        return consumeMediaRule(prelude, block);
    case CSSAtRuleSupports:
        return consumeSupportsRule(prelude, block);
    case CSSAtRuleFontFace:
        return consumeFontFaceRule(prelude, block);
    case CSSAtRuleFontFeatureValues:
        return consumeFontFeatureValuesRule(prelude, block);
    case CSSAtRuleStyleset:
    case CSSAtRuleStylistic:
    case CSSAtRuleCharacterVariant:
    case CSSAtRuleSwash:
    case CSSAtRuleOrnaments:
    case CSSAtRuleAnnotation:
        return allowedRules == FontFeatureValuesRules ? consumeFontFeatureValuesRuleBlock(id, prelude, block) : nullptr;
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
    case CSSAtRuleProperty:
        return consumePropertyRule(prelude, block);
    default:
        return nullptr; // Parse error, unrecognised at-rule with block
    }
}

// https://drafts.csswg.org/css-syntax/#consume-a-qualified-rule
RefPtr<StyleRuleBase> CSSParserImpl::consumeQualifiedRule(CSSParserTokenRange& range, AllowedRulesType allowedRules)
{
    auto isNestedStyleRule = [&]() {
        return isNestedContext() && allowedRules <= RegularRules;
    };

    const CSSParserToken* preludeStart = &range.peek();

    // Parsing a selector (aka a component value) should stop at the first semicolon (and goes to error recovery)
    // instead of consuming the whole list of declaration (in nested context).
    // At top level (aka non nested context), it's the normal rule list error recovery and we don't need this.
    while (!range.atEnd() && range.peek().type() != LeftBraceToken && (!isNestedStyleRule() || range.peek().type() != SemicolonToken))
        range.consumeComponentValue();

    if (range.atEnd())
        return nullptr; // Parse error, EOF instead of qualified rule block

    // See comment above
    if (isNestedStyleRule() && range.peek().type() == SemicolonToken) {
        range.consume();
        return nullptr;
    }

    CSSParserTokenRange prelude = range.makeSubRange(preludeStart, &range.peek());
    CSSParserTokenRange block = range.consumeBlockCheckingForEditability(m_styleSheet.get());

    if (allowedRules <= RegularRules)
        return consumeStyleRule(prelude, block);
    
    if (allowedRules == KeyframeRules)
        return consumeKeyframeStyleRule(prelude, block);

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
    auto mediaQueries = MQ::MediaQueryParser::parse(prelude, MediaQueryParserContext(m_context));
    
    return StyleRuleImport::create(uri, WTFMove(mediaQueries), WTFMove(cascadeLayerName));
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

void CSSParserImpl::runInNewNestingContext(auto&& run)
{
    m_nestingContextStack.append(NestingContext { });
    run();
    m_nestingContextStack.removeLast();
}

RefPtr<StyleRuleBase> CSSParserImpl::createNestingParentRule()
{
    CSSSelector nestingParentSelector;
    nestingParentSelector.setMatch(CSSSelector::Match::PseudoClass);
    nestingParentSelector.setPseudoClassType(CSSSelector::PseudoClassType::PseudoClassNestingParent);
    auto parserSelector = makeUnique<CSSParserSelector>(nestingParentSelector);
    Vector<std::unique_ptr<CSSParserSelector>> selectorList;
    selectorList.append(WTFMove(parserSelector));
    auto properties = createStyleProperties(topContext().m_parsedProperties, m_context.mode);
    return StyleRuleWithNesting::create(WTFMove(properties), m_context.hasDocumentSecurityOrigin, CSSSelectorList { WTFMove(selectorList) }, { });
}

Vector<RefPtr<StyleRuleBase>> CSSParserImpl::consumeRegularRuleList(CSSParserTokenRange block)
{
    Vector<RefPtr<StyleRuleBase>> rules;
    if (isNestedContext()) {
        runInNewNestingContext([&]() {
            consumeStyleBlock(block, StyleRuleType::Style);
            if (!topContext().m_parsedProperties.isEmpty()) {
                // This at-rule contains orphan declarations, we attach them to an implicit parent nesting rule
                rules.append(createNestingParentRule());
            }
            for (auto& rule : topContext().m_parsedRules)
                rules.append(rule.ptr());
        });

    } else {
        consumeRuleList(block, RegularRuleList, [&rules](RefPtr<StyleRuleBase> rule) {
            rules.append(rule);
        });
    }
    rules.shrinkToFit();
    return rules;
}

RefPtr<StyleRuleMedia> CSSParserImpl::consumeMediaRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::Media, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(block));
    }

    auto rules = consumeRegularRuleList(block);

    if (m_observerWrapper)
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(block));

    return StyleRuleMedia::create(MQ::MediaQueryParser::parse(prelude, { m_context }), WTFMove(rules));
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

    auto rules = consumeRegularRuleList(block);

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

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::FontFace);
    return StyleRuleFontFace::create(createStyleProperties(declarations, m_context.mode));
}

// The associated number represents the maximum number of allowed values for this font-feature-values type.
// No value means unlimited (for styleset).
static std::pair<FontFeatureValuesType, std::optional<unsigned>> fontFeatureValuesTypeMappings(CSSAtRuleID id)
{
    switch (id) {
    case CSSAtRuleStyleset: return { FontFeatureValuesType::Styleset, { } }; 
    case CSSAtRuleStylistic: return { FontFeatureValuesType::Stylistic, 1 };
    case CSSAtRuleCharacterVariant: return { FontFeatureValuesType::CharacterVariant, 2 };
    case CSSAtRuleSwash: return { FontFeatureValuesType::Swash, 1 };
    case CSSAtRuleOrnaments: return { FontFeatureValuesType::Ornaments, 1 };
    case CSSAtRuleAnnotation: return { FontFeatureValuesType::Annotation, 1 };
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

RefPtr<StyleRuleFontFeatureValuesBlock> CSSParserImpl::consumeFontFeatureValuesRuleBlock(CSSAtRuleID id, CSSParserTokenRange prelude, CSSParserTokenRange range)
{
    // <feature-value-block> = <font-feature-value-type> { <declaration-list> }
    // <font-feature-value-type> = @stylistic | @historical-forms | @styleset | @character-variant | @swash | @ornaments | @annotation

    // Prelude should be empty.
    if (!prelude.atEnd())
        return { };
    
    // Block should be present.
    if (range.atEnd())
        return { };
    
    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::FontFeatureValuesBlock, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(range));
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(range));
    }

    auto [type, maxValues] = fontFeatureValuesTypeMappings(id);
    
    auto consumeTag = [](CSSParserTokenRange range, std::optional<unsigned> maxValues) -> std::optional<FontFeatureValuesTag> {
        if (range.peek().type() != IdentToken)
            return { };
        auto name = range.consumeIncludingWhitespace().value();
        if (range.consume().type() != ColonToken)
            return { };
        range.consumeWhitespace();

        Vector<unsigned> values;
        while (!range.atEnd()) {
            auto value = CSSPropertyParserHelpers::consumeNonNegativeInteger(range);
            if (!value)
                return { };
            ASSERT(value->isInteger());
            auto tagInteger = value->intValue();
            ASSERT(tagInteger >= 0);
            values.append(std::make_unsigned_t<int>(tagInteger));
            if (maxValues && values.size() > *maxValues)
                return { };
        }
        if (values.isEmpty())
            return { };
        
        return { FontFeatureValuesTag { name.toString(), values } };
    };

    Vector<FontFeatureValuesTag> tags;
    while (!range.atEnd()) {
        switch (range.peek().type()) {
        case WhitespaceToken:
        case SemicolonToken:
            range.consume();
            break;
        case IdentToken: {
            const CSSParserToken* declarationStart = &range.peek();

            while (!range.atEnd() && range.peek().type() != SemicolonToken)
                range.consumeComponentValue();

            if (auto tag = consumeTag(range.makeSubRange(declarationStart, &range.peek()), maxValues))
                tags.append(*tag);

            break;
        }
        default: // Parse error, unexpected token in declaration list
            while (!range.atEnd() && range.peek().type() != SemicolonToken)
                range.consumeComponentValue();
            break;
        }
    }
    return StyleRuleFontFeatureValuesBlock::create(type, tags);
}
RefPtr<StyleRuleFontFeatureValues> CSSParserImpl::consumeFontFeatureValuesRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    // @font-feature-values <family-name># { <declaration-list> }

    auto originalPrelude = prelude;
    auto fontFamilies = CSSPropertyParserHelpers::consumeFamilyNameList(prelude);
    if (fontFamilies.isEmpty() || !prelude.atEnd())
        return nullptr;
    
    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::FontFeatureValues, m_observerWrapper->startOffset(originalPrelude));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(block));
    }

    Vector<RefPtr<StyleRuleBase>> rules;
    consumeRuleList(block, FontFeatureValuesRuleList, [&rules](RefPtr<StyleRuleBase> rule) {
        rules.append(rule);
    });
    rules.shrinkToFit();
    
    if (m_observerWrapper)
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(block));

    // Convert block rules to value (remove duplicate...etc)
    auto fontFeatureValues = FontFeatureValues::create();

    for (auto block : rules) {
        if (!block)
            continue;
        if (!block->isFontFeatureValuesBlockRule())
            continue;
        const auto& fontFeatureValuesBlockRule = downcast<StyleRuleFontFeatureValuesBlock>(*block);
        fontFeatureValues->updateOrInsertForType(fontFeatureValuesBlockRule.fontFeatureValuesType(), fontFeatureValuesBlockRule.tags());
    }

    return StyleRuleFontFeatureValues::create(fontFamilies, WTFMove(fontFeatureValues));
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

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::FontPaletteValues);
    auto properties = createStyleProperties(declarations, m_context.mode);

    AtomString fontFamily { properties->getPropertyValue(CSSPropertyFontFamily) };

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
            Color color = pair.color().isColor() ? pair.color().color() : StyleColor::colorFromKeyword(pair.color().valueID(), { });
            overrideColors.append(std::make_pair(key, color));
        }
    }

    return StyleRuleFontPaletteValues::create(AtomString { name->stringValue() }, fontFamily, WTFMove(basePalette), WTFMove(overrideColors));
}

RefPtr<StyleRuleKeyframes> CSSParserImpl::consumeKeyframesRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    CSSParserTokenRange rangeCopy = prelude; // For inspector callbacks
    const CSSParserToken& nameToken = prelude.consumeIncludingWhitespace();
    if (!prelude.atEnd())
        return nullptr; // Parse error; expected single non-whitespace token in @keyframes header

    if (nameToken.type() == IdentToken) {
        // According to the CSS Values specification, identifier-based keyframe names
        // are not allowed to be CSS wide keywords or "default". And CSS Animations 
        // additionally excludes the "none" keyword.
        if (!isValidCustomIdentifier(nameToken.id()) || nameToken.id() == CSSValueNone)
            return nullptr;
    } else if (nameToken.type() != StringToken)
        return nullptr; // Parse error; expected ident token or string in @keyframes header

    auto name = nameToken.value().toAtomString();

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

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::Style);
    
    return StyleRulePage::create(createStyleProperties(declarations, m_context.mode), WTFMove(selectorList));
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

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::CounterStyle);
    return StyleRuleCounterStyle::create(name, createStyleProperties(declarations, m_context.mode));
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

    auto rules = consumeRegularRuleList(*block);

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

    auto originalPreludeRange = prelude;

    auto query = ContainerQueryParser::consumeContainerQuery(prelude, m_context);
    if (!query)
        return nullptr;

    prelude.consumeWhitespace();
    if (!prelude.atEnd())
        return nullptr;

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRuleType::Container, m_observerWrapper->startOffset(originalPreludeRange));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(originalPreludeRange));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(block));
    }

    auto rules = consumeRegularRuleList(block);

    if (m_observerWrapper)
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(block));

    return StyleRuleContainer::create(WTFMove(*query), WTFMove(rules));
}

RefPtr<StyleRuleProperty> CSSParserImpl::consumePropertyRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (!m_context.propertySettings.cssCustomPropertiesAndValuesEnabled)
        return nullptr;

    auto nameToken = prelude.consumeIncludingWhitespace();
    if (nameToken.type() != IdentToken || !prelude.atEnd())
        return nullptr;

    auto name = nameToken.value().toAtomString();
    if (!isCustomPropertyName(name))
        return nullptr;

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::Property);

    auto propertyDescriptor = StyleRuleProperty::Descriptor { name };

    for (auto& property : declarations) {
        switch (property.id()) {
        case CSSPropertySyntax:
            propertyDescriptor.syntax = downcast<CSSPrimitiveValue>(*property.value()).stringValue();
            continue;
        case CSSPropertyInherits:
            propertyDescriptor.inherits = downcast<CSSPrimitiveValue>(*property.value()).valueID() == CSSValueTrue;
            break;
        case CSSPropertyInitialValue:
            propertyDescriptor.initialValue = downcast<CSSCustomPropertyValue>(*property.value()).asVariableData();
            break;
        default:
            break;
        };
    };

    return StyleRuleProperty::create(WTFMove(propertyDescriptor));
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

    auto declarations = consumeDeclarationListInNewNestingContext(block, StyleRuleType::Keyframe);

    return StyleRuleKeyframe::create(WTFMove(keyList), createStyleProperties(declarations, m_context.mode));
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

RefPtr<StyleRuleBase> CSSParserImpl::consumeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    auto selectorList = parseCSSSelector(prelude, m_context, m_styleSheet.get(), isNestedContext() ? CSSSelectorParser::IsNestedContext::Yes : CSSSelectorParser::IsNestedContext::No);
    if (!selectorList)
        return nullptr; // Parse error, invalid selector list

    if (m_observerWrapper)
        observeSelectors(*m_observerWrapper, prelude);
    
    RefPtr<StyleRuleBase> styleRule;

    runInNewNestingContext([&]() {
        m_styleRuleNestingDepth += 1;
        consumeStyleBlock(block, StyleRuleType::Style);  
        m_styleRuleNestingDepth -= 1;

        auto nestedRules = WTFMove(topContext().m_parsedRules);
        auto properties = createStyleProperties(topContext().m_parsedProperties, m_context.mode);

        // We save memory by creating a simple StyleRule instead of a heavier StyleWithNestingSupportRule when we don't need the CSS Nesting features.
        if (nestedRules.isEmpty() && !selectorList->hasExplicitNestingParent() && !isNestedContext())
            styleRule = StyleRule::create(WTFMove(properties), m_context.hasDocumentSecurityOrigin, WTFMove(*selectorList));
        else
            styleRule = StyleRuleWithNesting::create(WTFMove(properties), m_context.hasDocumentSecurityOrigin, WTFMove(*selectorList), WTFMove(nestedRules));
    });
    
    return styleRule;
}

void CSSParserImpl::consumeDeclarationListOrStyleBlockHelper(CSSParserTokenRange range, StyleRuleType ruleType, OnlyDeclarations onlyDeclarations)
{
    auto nestedRulesAllowed = [&]() {
        return m_styleRuleNestingDepth && context().cssNestingEnabled && onlyDeclarations == OnlyDeclarations::No;        
    };
    
    ASSERT(topContext().m_parsedProperties.isEmpty());
    ASSERT(topContext().m_parsedRules.isEmpty());

    bool useObserver = m_observerWrapper && (ruleType == StyleRuleType::Style || ruleType == StyleRuleType::Keyframe || ruleType == StyleRuleType::CounterStyle);
    if (useObserver) {
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(range));
        m_observerWrapper->skipCommentsBefore(range, true);
    }

    auto consumeErrorUntilSemicolon = [&] () {
        while (!range.atEnd() && range.peek().type() != SemicolonToken)
            range.consumeComponentValue();
    };

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

            consumeErrorUntilSemicolon();

            consumeDeclaration(range.makeSubRange(declarationStart, &range.peek()), ruleType);

            if (useObserver)
                m_observerWrapper->skipCommentsBefore(range, false);
            break;
        }
        case AtKeywordToken: {
            if (nestedRulesAllowed()) {
                auto rule = consumeAtRule(range, RegularRules);
                if (!rule)
                    break;
                if (!rule->isGroupRule())
                    break;
                topContext().m_parsedRules.append(*rule);
            } else {
                auto rule = consumeAtRule(range, NoRules);
                ASSERT_UNUSED(rule, !rule);    
            }
            break;
        }
        default: 
            if (nestedRulesAllowed()) {
                auto rule = consumeQualifiedRule(range, AllowedRulesType::RegularRules);
                if (!rule)
                    break;
                if (!rule->isStyleRule())
                    break;
                topContext().m_parsedRules.append(*rule);
                break;
            }
        FALLTHROUGH;
        case FunctionToken:
            // Parse error, unexpected token in declaration list
            consumeErrorUntilSemicolon();
            break;
        }
    }

    // Yield remaining comments
    if (useObserver) {
        m_observerWrapper->yieldCommentsBefore(range);
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(range));
    }
}

ParsedPropertyVector CSSParserImpl::consumeDeclarationListInNewNestingContext(CSSParserTokenRange range, StyleRuleType ruleType)
{
    ParsedPropertyVector result;
    runInNewNestingContext([&]() {
        consumeDeclarationList(range, ruleType); 
        result = WTFMove(topContext().m_parsedProperties);
    });
    return result;
}

void CSSParserImpl::consumeDeclarationList(CSSParserTokenRange range, StyleRuleType ruleType)
{
    consumeDeclarationListOrStyleBlockHelper(range, ruleType, OnlyDeclarations::Yes);
}

void CSSParserImpl::consumeStyleBlock(CSSParserTokenRange range, StyleRuleType ruleType)
{
    consumeDeclarationListOrStyleBlockHelper(range, ruleType, OnlyDeclarations::No);
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

    size_t propertiesCount = topContext().m_parsedProperties.size();

    if (!isExposed(propertyID, &m_context.propertySettings))
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
            important, topContext().m_parsedProperties.size() != propertiesCount);
    }
}

void CSSParserImpl::consumeCustomPropertyValue(CSSParserTokenRange range, const AtomString& variableName, bool important)
{
    if (range.atEnd())
        topContext().m_parsedProperties.append(CSSProperty(CSSPropertyCustom, CSSCustomPropertyValue::createEmpty(variableName), important));
    else if (auto value = CSSVariableParser::parseDeclarationValue(variableName, range, m_context))
        topContext().m_parsedProperties.append(CSSProperty(CSSPropertyCustom, WTFMove(value), important));
}

void CSSParserImpl::consumeDeclarationValue(CSSParserTokenRange range, CSSPropertyID propertyID, bool important, StyleRuleType ruleType)
{
    CSSPropertyParser::parseValue(propertyID, important, range, m_context, topContext().m_parsedProperties, ruleType);
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
