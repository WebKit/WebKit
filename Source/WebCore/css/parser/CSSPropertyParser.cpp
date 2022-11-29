// Copyright 2015 The Chromium Authors. All rights reserved.
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
#include "CSSPropertyParser.h"

#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFontPaletteValuesOverrideColorsValue.h"
#include "CSSFontVariantLigaturesParser.h"
#include "CSSFontVariantNumericParser.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSParserFastPaths.h"
#include "CSSParserIdioms.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParsing.h"
#include "CSSVariableParser.h"
#include "CSSVariableReferenceValue.h"
#include "Counter.h"
#include "FontFace.h"
#include "Pair.h"
#include "Rect.h"
#include "StyleBuilder.h"
#include "StyleBuilderConverter.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"
#include "TimingFunction.h"
#include <memory>
#include <wtf/text/StringBuilder.h>

// FIXME-NEWPARSER: CSSPrimitiveValue is a large class that holds many unrelated objects,
// switching behavior on the type of the object it is holding.
// Since CSSValue is already a class hierarchy, this adds an unnecessary second level to the hierarchy that complicates code.
// So we need to remove the various behaviors from CSSPrimitiveValue and split them into separate subclasses of CSSValue.
// FIXME-NEWPARSER: Replace Pair and Rect with actual CSSValue subclasses (CSSValuePair and CSSQuadValue).

namespace WebCore {

bool isCustomPropertyName(const String& propertyName)
{
    return propertyName.length() > 2 && propertyName.characterAt(0) == '-' && propertyName.characterAt(1) == '-';
}

static bool hasPrefix(const char* string, unsigned length, const char* prefix)
{
    for (unsigned i = 0; i < length; ++i) {
        if (!prefix[i])
            return true;
        if (string[i] != prefix[i])
            return false;
    }
    return false;
}

template<typename CharacterType> static CSSPropertyID cssPropertyID(const CharacterType* characters, unsigned length)
{
    char buffer[maxCSSPropertyNameLength];
    for (unsigned i = 0; i != length; ++i) {
        auto character = characters[i];
        if (!character || !isASCII(character))
            return CSSPropertyInvalid;
        buffer[i] = toASCIILower(character);
    }
    return findCSSProperty(buffer, length);
}

// FIXME: Remove this mechanism entirely once we can do it without breaking the web.
static bool isAppleLegacyCSSValueKeyword(const char* characters, unsigned length)
{
    return hasPrefix(characters + 1, length - 1, "apple-")
        && !hasPrefix(characters + 7, length - 7, "system")
        && !hasPrefix(characters + 7, length - 7, "pay")
        && !hasPrefix(characters + 7, length - 7, "wireless");
}

template<typename CharacterType> static CSSValueID cssValueKeywordID(const CharacterType* characters, unsigned length)
{
    char buffer[maxCSSValueKeywordLength + 1]; // 1 to turn "apple" into "webkit"
    
    for (unsigned i = 0; i != length; ++i) {
        auto character = characters[i];
        if (!character || !isASCII(character))
            return CSSValueInvalid;
        buffer[i] = toASCIILower(character);
    }

    // In most cases, if the prefix is -apple-, change it to -webkit-. This makes the string one character longer.
    if (buffer[0] == '-' && isAppleLegacyCSSValueKeyword(buffer, length)) {
        memmove(buffer + 7, buffer + 6, length - 6);
        memcpy(buffer + 1, "webkit", 6);
        ++length;
    }

    return findCSSValueKeyword(buffer, length);
}

CSSValueID cssValueKeywordID(StringView string)
{
    unsigned length = string.length();
    if (!length)
        return CSSValueInvalid;
    if (length > maxCSSValueKeywordLength)
        return CSSValueInvalid;
    
    return string.is8Bit() ? cssValueKeywordID(string.characters8(), length) : cssValueKeywordID(string.characters16(), length);
}

CSSPropertyID cssPropertyID(StringView string)
{
    unsigned length = string.length();
    
    if (!length)
        return CSSPropertyInvalid;
    if (length > maxCSSPropertyNameLength)
        return CSSPropertyInvalid;
    
    return string.is8Bit() ? cssPropertyID(string.characters8(), length) : cssPropertyID(string.characters16(), length);
}
    
using namespace CSSPropertyParserHelpers;
using namespace CSSPropertyParserHelpersWorkerSafe;

CSSPropertyParser::CSSPropertyParser(const CSSParserTokenRange& range, const CSSParserContext& context, Vector<CSSProperty, 256>* parsedProperties, bool consumeWhitespace)
    : m_range(range)
    , m_context(context)
    , m_parsedProperties(parsedProperties)
{
    if (consumeWhitespace)
        m_range.consumeWhitespace();
}

void CSSPropertyParser::addProperty(CSSPropertyID property, CSSPropertyID currentShorthand, Ref<CSSValue>&& value, bool important, bool implicit)
{
    int shorthandIndex = 0;
    bool setFromShorthand = false;

    if (currentShorthand) {
        auto shorthands = matchingShorthandsForLonghand(property);
        setFromShorthand = true;
        if (shorthands.size() > 1)
            shorthandIndex = indexOfShorthandForLonghand(currentShorthand, shorthands);
    }

    // Allow anything to be set from a shorthand (e.g. the CSS all property always sets everything,
    // regardless of whether the longhands are enabled), and allow internal properties as we use
    // them to handle certain DOM-exposed values (e.g. -webkit-font-size-delta from
    // execCommand('FontSizeDelta')).
    ASSERT(isExposed(property, &m_context.propertySettings) || setFromShorthand || isInternal(property));

    m_parsedProperties->append(CSSProperty(property, WTFMove(value), important, setFromShorthand, shorthandIndex, implicit));
}

void CSSPropertyParser::addPropertyWithImplicitDefault(CSSPropertyID property, CSSPropertyID currentShorthand, RefPtr<CSSValue>&& value, Ref<CSSValue>&& implicitDefault, bool important)
{
    if (value)
        addProperty(property, currentShorthand, value.releaseNonNull(), important, false);
    else
        addProperty(property, currentShorthand, WTFMove(implicitDefault), important, true);
}

void CSSPropertyParser::addExpandedPropertyForValue(CSSPropertyID property, Ref<CSSValue>&& value, bool important)
{
    for (auto longhand : shorthandForProperty(property))
        addProperty(longhand, property, value.copyRef(), important);
}

bool CSSPropertyParser::parseValue(CSSPropertyID propertyID, bool important, const CSSParserTokenRange& range, const CSSParserContext& context, ParsedPropertyVector& parsedProperties, StyleRuleType ruleType)
{
    int parsedPropertiesSize = parsedProperties.size();

    CSSPropertyParser parser(range, context, &parsedProperties);
    bool parseSuccess;

    if (ruleType == StyleRuleType::FontFace)
        parseSuccess = parser.parseFontFaceDescriptor(propertyID);
    else if (ruleType == StyleRuleType::FontPaletteValues)
        parseSuccess = parser.parseFontPaletteValuesDescriptor(propertyID);
    else if (ruleType == StyleRuleType::CounterStyle)
        parseSuccess = parser.parseCounterStyleDescriptor(propertyID, context);
    else if (ruleType == StyleRuleType::Keyframe)
        parseSuccess = parser.parseKeyframeDescriptor(propertyID, important);
    else
        parseSuccess = parser.parseValueStart(propertyID, important);

    if (!parseSuccess)
        parsedProperties.shrink(parsedPropertiesSize);

    return parseSuccess;
}

static RefPtr<CSSValue> maybeConsumeCSSWideKeyword(CSSParserTokenRange& range)
{
    CSSParserTokenRange rangeCopy = range;
    CSSValueID valueID = rangeCopy.consumeIncludingWhitespace().id();
    if (!rangeCopy.atEnd())
        return nullptr;

    if (!isCSSWideKeyword(valueID))
        return nullptr;

    range = rangeCopy;
    return CSSValuePool::singleton().createIdentifierValue(valueID);
}

RefPtr<CSSValue> CSSPropertyParser::parseSingleValue(CSSPropertyID property, const CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSPropertyParser parser(range, context, nullptr);
    if (auto value = maybeConsumeCSSWideKeyword(parser.m_range))
        return value;
    
    RefPtr<CSSValue> value = parser.parseSingleValue(property);
    if (!value || !parser.m_range.atEnd())
        return nullptr;
    return value;
}

bool CSSPropertyParser::canParseTypedCustomPropertyValue(const String& syntax, const CSSParserTokenRange& tokens, const CSSParserContext& context)
{
    CSSPropertyParser parser(tokens, context, nullptr);
    return parser.canParseTypedCustomPropertyValue(syntax);
}

RefPtr<CSSCustomPropertyValue> CSSPropertyParser::parseTypedCustomPropertyValue(const AtomString& name, const String& syntax, const CSSParserTokenRange& tokens, const Style::BuilderState& builderState, const CSSParserContext& context)
{
    CSSPropertyParser parser(tokens, context, nullptr, false);
    RefPtr<CSSCustomPropertyValue> value = parser.parseTypedCustomPropertyValue(name, syntax, builderState);
    if (!value || !parser.m_range.atEnd())
        return nullptr;
    return value;
}

void CSSPropertyParser::collectParsedCustomPropertyValueDependencies(const String& syntax, bool isRoot, HashSet<CSSPropertyID>& dependencies, const CSSParserTokenRange& tokens, const CSSParserContext& context)
{
    CSSPropertyParser parser(tokens, context, nullptr);
    parser.collectParsedCustomPropertyValueDependencies(syntax, isRoot, dependencies);
}

bool CSSPropertyParser::parseValueStart(CSSPropertyID propertyID, bool important)
{
    if (consumeCSSWideKeyword(propertyID, important))
        return true;

    CSSParserTokenRange originalRange = m_range;
    bool isShorthand = isShorthandCSSProperty(propertyID);

    if (isShorthand) {
        // Variable references will fail to parse here and will fall out to the variable ref parser below.
        if (parseShorthand(propertyID, important))
            return true;
    } else {
        RefPtr<CSSValue> parsedValue = parseSingleValue(propertyID);
        if (parsedValue && m_range.atEnd()) {
            addProperty(propertyID, CSSPropertyInvalid, *parsedValue, important);
            return true;
        }
    }

    if (CSSVariableParser::containsValidVariableReferences(originalRange, m_context)) {
        auto variable = CSSVariableReferenceValue::create(originalRange, m_context);
        if (isShorthand)
            addExpandedPropertyForValue(propertyID, CSSPendingSubstitutionValue::create(propertyID, WTFMove(variable)), important);
        else
            addProperty(propertyID, CSSPropertyInvalid, WTFMove(variable), important);
        return true;
    }

    return false;
}

bool CSSPropertyParser::consumeCSSWideKeyword(CSSPropertyID propertyID, bool important)
{
    CSSParserTokenRange rangeCopy = m_range;
    auto value = maybeConsumeCSSWideKeyword(rangeCopy);
    if (!value)
        return false;
    
    const StylePropertyShorthand& shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length()) {
        if (CSSProperty::isDescriptorOnly(propertyID))
            return false;
        addProperty(propertyID, CSSPropertyInvalid, value.releaseNonNull(), important);
    } else
        addExpandedPropertyForValue(propertyID, value.releaseNonNull(), important);
    m_range = rangeCopy;
    return true;
}

RefPtr<CSSValue> CSSPropertyParser::parseSingleValue(CSSPropertyID property, CSSPropertyID currentShorthand)
{
    return CSSPropertyParsing::parse(m_range, property, currentShorthand, m_context);
}

bool CSSPropertyParser::canParseTypedCustomPropertyValue(const String& syntax)
{
    if (syntax != "*"_s) {
        m_range.consumeWhitespace();

        // First check for keywords
        if (isCSSWideKeyword(m_range.peek().id()))
            return true;

        auto localRange = m_range;
        while (!localRange.atEnd()) {
            auto id = localRange.consume().functionId();
            if (id == CSSValueVar || id == CSSValueEnv)
                return true; // For variables, we just permit everything
        }

        auto primitiveVal = CSSPropertyParsing::consumeWidthOrHeight(m_range, m_context);
        if (primitiveVal && primitiveVal->isPrimitiveValue() && m_range.atEnd())
            return true;
        return false;
    }

    return true;
}

void CSSPropertyParser::collectParsedCustomPropertyValueDependencies(const String& syntax, bool isRoot, HashSet<CSSPropertyID>& dependencies)
{
    if (syntax != "*"_s) {
        m_range.consumeWhitespace();
        auto primitiveVal = CSSPropertyParsing::consumeWidthOrHeight(m_range, m_context);
        if (!m_range.atEnd())
            return;
        if (primitiveVal && primitiveVal->isPrimitiveValue()) {
            primitiveVal->collectDirectComputationalDependencies(dependencies);
            if (isRoot)
                primitiveVal->collectDirectRootComputationalDependencies(dependencies);
        }
    }
}

RefPtr<CSSCustomPropertyValue> CSSPropertyParser::parseTypedCustomPropertyValue(const AtomString& name, const String& syntax, const Style::BuilderState& builderState)
{
    if (syntax != "*"_s) {
        m_range.consumeWhitespace();
        auto primitiveVal = CSSPropertyParsing::consumeWidthOrHeight(m_range, m_context);
        if (primitiveVal && primitiveVal->isPrimitiveValue() && downcast<CSSPrimitiveValue>(*primitiveVal).isLength()) {
            auto length = Style::BuilderConverter::convertLength(builderState, *primitiveVal);
            if (!length.isCalculated() && !length.isUndefined())
                return CSSCustomPropertyValue::createSyntaxLength(name, WTFMove(length));
        }
    } else {
        auto propertyValue = CSSCustomPropertyValue::createSyntaxAll(name, CSSVariableData::create(m_range));
        while (!m_range.atEnd())
            m_range.consume();
        return { WTFMove(propertyValue) };
    }

    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-system
static RefPtr<CSSPrimitiveValue> consumeCounterStyleSystem(CSSParserTokenRange& range)
{
    if (auto ident = consumeIdent<CSSValueCyclic, CSSValueNumeric, CSSValueAlphabetic, CSSValueSymbolic, CSSValueAdditive>(range))
        return ident;

    if (auto ident = consumeIdent<CSSValueFixed>(range)) {
        if (range.atEnd())
            return ident;
        // If we have the `fixed` keyword but the range is not at the end, the next token must be a integer.
        // If it's not, this value is invalid.
        auto firstSymbolValue = consumeInteger(range);
        if (!firstSymbolValue)
            return nullptr;
        return createPrimitiveValuePair(ident.releaseNonNull(), firstSymbolValue.releaseNonNull());
    }

    if (auto ident = consumeIdent<CSSValueExtends>(range)) {
        // There must be a `<counter-style-name>` following the `extends` keyword. If there isn't, this value is invalid.
        auto parsedCounterStyleName = consumeCounterStyleName(range);
        if (!parsedCounterStyleName)
            return nullptr;
        return createPrimitiveValuePair(ident.releaseNonNull(), parsedCounterStyleName.releaseNonNull());
    }
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#typedef-symbol
static RefPtr<CSSValue> consumeCounterStyleSymbol(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto string = consumeString(range))
        return string;
    if (auto customIdent = consumeCustomIdent(range))
        return customIdent;
    // There are inherent difficulties in supporting <image> symbols in @counter-styles, so gate them behind a
    // flag for now. https://bugs.webkit.org/show_bug.cgi?id=167645
    if (context.counterStyleAtRuleImageSymbolsEnabled) {
        if (auto image = consumeImage(range, context, { AllowedImageType::URLFunction, AllowedImageType::GeneratedImage }))
            return image;
    }
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-negative
static RefPtr<CSSValue> consumeCounterStyleNegative(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto prependValue = consumeCounterStyleSymbol(range, context);
    if (!prependValue)
        return nullptr;
    if (range.atEnd())
        return prependValue;

    auto appendValue = consumeCounterStyleSymbol(range, context);
    if (!appendValue || !range.atEnd())
        return nullptr;

    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    values->append(prependValue.releaseNonNull());
    values->append(appendValue.releaseNonNull());
    return values;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-range
static RefPtr<CSSPrimitiveValue> consumeCounterStyleRangeBound(CSSParserTokenRange& range)
{
    if (auto infinite = consumeIdent<CSSValueInfinite>(range))
        return infinite;
    if (auto integer = consumeInteger(range))
        return integer;
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-range
static RefPtr<CSSValue> consumeCounterStyleRange(CSSParserTokenRange& range)
{
    if (auto autoValue = consumeIdent<CSSValueAuto>(range))
        return autoValue;

    auto rangeList = consumeCommaSeparatedListWithoutSingleValueOptimization(range, [](auto& range) -> RefPtr<CSSValue> {
        auto lowerBound = consumeCounterStyleRangeBound(range);
        if (!lowerBound)
            return nullptr;
        auto upperBound = consumeCounterStyleRangeBound(range);
        if (!upperBound)
            return nullptr;

        // If the lower bound of any range is higher than the upper bound, the entire descriptor is invalid and must be
        // ignored.
        if (lowerBound->isInteger() && upperBound->isInteger() && lowerBound->intValue() > upperBound->intValue())
            return nullptr;
        
        return createPrimitiveValuePair(lowerBound.releaseNonNull(), upperBound.releaseNonNull(), Pair::IdenticalValueEncoding::DoNotCoalesce);
    });

    if (!range.atEnd() || !rangeList || !rangeList->length())
        return nullptr;
    return rangeList;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-pad
static RefPtr<CSSValue> consumeCounterStylePad(CSSParserTokenRange& range, const CSSParserContext& context)
{
    RefPtr<CSSValue> integer;
    RefPtr<CSSValue> symbol;
    while (!integer || !symbol) {
        if (!integer) {
            integer = consumeNonNegativeInteger(range);
            if (integer)
                continue;
        }
        if (!symbol) {
            symbol = consumeCounterStyleSymbol(range, context);
            if (symbol)
                continue;
        }
        return nullptr;
    }
    if (!range.atEnd())
        return nullptr;
    auto values = CSSValueList::createSpaceSeparated();
    values->append(integer.releaseNonNull());
    values->append(symbol.releaseNonNull());
    return values;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-symbols
static RefPtr<CSSValue> consumeCounterStyleSymbols(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto symbols = CSSValueList::createSpaceSeparated();
    while (!range.atEnd()) {
        auto symbol = consumeCounterStyleSymbol(range, context);
        if (!symbol)
            return nullptr;
        symbols->append(symbol.releaseNonNull());
    }
    if (!symbols->length())
        return nullptr;
    return symbols;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-symbols
static RefPtr<CSSValue> consumeCounterStyleAdditiveSymbols(CSSParserTokenRange& range, const CSSParserContext& context)
{
    std::optional<int> lastWeight;
    auto values = consumeCommaSeparatedListWithoutSingleValueOptimization(range, [&lastWeight](auto& range, auto& context) -> RefPtr<CSSValue> {
        auto integer = consumeNonNegativeInteger(range);
        auto symbol = consumeCounterStyleSymbol(range, context);
        if (!integer) {
            if (!symbol)
                return nullptr;
            integer = consumeNonNegativeInteger(range);
            if (!integer)
                return nullptr;
        }

        // Additive tuples must be specified in order of strictly descending weight.
        auto weight = integer->intValue();
        if (lastWeight && !(weight < lastWeight))
            return nullptr;
        lastWeight = weight;

        auto pair = CSSValueList::createSpaceSeparated();
        pair->append(integer.releaseNonNull());
        pair->append(symbol.releaseNonNull());
        return pair;
    }, context);

    if (!range.atEnd() || !values || !values->length())
        return nullptr;
    return values;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-speak-as
static RefPtr<CSSValue> consumeCounterStyleSpeakAs(CSSParserTokenRange& range)
{
    if (auto speakAsIdent = consumeIdent<CSSValueAuto, CSSValueBullets, CSSValueNumbers, CSSValueWords, CSSValueSpellOut>(range))
        return speakAsIdent;
    return consumeCounterStyleName(range);
}

RefPtr<CSSValue> CSSPropertyParser::parseCounterStyleDescriptor(CSSPropertyID propId, CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(context.propertySettings.cssCounterStyleAtRulesEnabled);
    ASSERT(isExposed(propId, &context.propertySettings));

    switch (propId) {
    case CSSPropertySystem:
        return consumeCounterStyleSystem(range);
    case CSSPropertyNegative:
        return consumeCounterStyleNegative(range, context);
    case CSSPropertyPrefix:
    case CSSPropertySuffix:
        return consumeCounterStyleSymbol(range, context);
    case CSSPropertyRange:
        return consumeCounterStyleRange(range);
    case CSSPropertyPad:
        return consumeCounterStylePad(range, context);
    case CSSPropertyFallback:
        return consumeCounterStyleName(range);
    case CSSPropertySymbols:
        return consumeCounterStyleSymbols(range, context);
    case CSSPropertyAdditiveSymbols:
        return consumeCounterStyleAdditiveSymbols(range, context);
    case CSSPropertySpeakAs:
        return consumeCounterStyleSpeakAs(range);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

bool CSSPropertyParser::parseCounterStyleDescriptor(CSSPropertyID propId, const CSSParserContext& context)
{
    auto parsedValue = parseCounterStyleDescriptor(propId, m_range, context);
    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(propId, CSSPropertyInvalid, *parsedValue, false);
    return true;
}

bool CSSPropertyParser::parseFontFaceDescriptor(CSSPropertyID propId)
{
    ASSERT(isExposed(propId, &m_context.propertySettings));

    RefPtr<CSSValue> parsedValue;
    switch (propId) {
    case CSSPropertyFontFamily:
        parsedValue = consumeFontFamilyDescriptor(m_range);
        break;
    case CSSPropertySrc: // This is a list of urls or local references.
        parsedValue = consumeFontFaceSrc(m_range, m_context);
        break;
    case CSSPropertyUnicodeRange:
        parsedValue = consumeFontFaceUnicodeRange(m_range);
        break;
    case CSSPropertyFontDisplay:
        parsedValue = consumeFontFaceFontDisplay(m_range, CSSValuePool::singleton());
        break;
    case CSSPropertyFontWeight:
#if ENABLE(VARIATION_FONTS)
        parsedValue = consumeFontWeightAbsoluteRange(m_range, CSSValuePool::singleton());
#else
        parsedValue = consumeFontWeightAbsolute(m_range, CSSValuePool::singleton());
#endif
        break;
    case CSSPropertyFontStretch:
#if ENABLE(VARIATION_FONTS)
        parsedValue = consumeFontStretchRange(m_range, CSSValuePool::singleton());
#else
        parsedValue = consumeFontStretch(m_range, CSSValuePool::singleton());
#endif
        break;
    case CSSPropertyFontStyle:
#if ENABLE(VARIATION_FONTS)
        parsedValue = consumeFontStyleRange(m_range, m_context.mode, CSSValuePool::singleton());
#else
        parsedValue = consumeFontStyle(m_range, m_context.mode, CSSValuePool::singleton());
#endif
        break;
    case CSSPropertyFontVariantCaps:
        parsedValue = CSSPropertyParsing::consumeFontVariantCaps(m_range);
        break;
    case CSSPropertyFontVariantLigatures:
        parsedValue = consumeFontVariantLigatures(m_range);
        break;
    case CSSPropertyFontVariantNumeric:
        parsedValue = consumeFontVariantNumeric(m_range);
        break;
    case CSSPropertyFontVariantEastAsian:
        parsedValue = consumeFontVariantEastAsian(m_range);
        break;
    case CSSPropertyFontVariantAlternates:
        parsedValue = consumeFontVariantAlternates(m_range);
        break;
    case CSSPropertyFontVariantPosition:
        parsedValue = CSSPropertyParsing::consumeFontVariantPosition(m_range);
        break;
    case CSSPropertyFontVariant:
        return consumeFontVariantShorthand(false);
    case CSSPropertyFontFeatureSettings:
        parsedValue = consumeFontFeatureSettings(m_range, CSSValuePool::singleton());
        break;
    default:
        break;
    }

    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(propId, CSSPropertyInvalid, *parsedValue, false);
    return true;
}

bool CSSPropertyParser::parseKeyframeDescriptor(CSSPropertyID propertyID, bool important)
{
    // https://www.w3.org/TR/css-animations-1/#keyframes
    // The <declaration-list> inside of <keyframe-block> accepts any CSS property except those
    // defined in this specification, but does accept the animation-timing-function property and
    // interprets it specially.
    switch (propertyID) {
    case CSSPropertyAnimation:
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
        return false;
    default:
        return parseValueStart(propertyID, important);
    }
}

static RefPtr<CSSPrimitiveValue> consumeBasePaletteDescriptor(CSSParserTokenRange& range)
{
    if (auto result = consumeIdent<CSSValueLight, CSSValueDark>(range))
        return result;
    return consumeNonNegativeInteger(range);
}

static RefPtr<CSSValueList> consumeOverrideColorsDescriptor(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto list = consumeCommaSeparatedListWithoutSingleValueOptimization(range, [](auto& range, auto& context) -> RefPtr<CSSValue> {
        auto key = consumeNonNegativeInteger(range);
        if (!key)
            return nullptr;

        auto color = consumeColor(range, context, false, { StyleColor::CSSColorType::Absolute });
        if (!color)
            return nullptr;

        return CSSFontPaletteValuesOverrideColorsValue::create(key.releaseNonNull(), color.releaseNonNull());
    }, context);

    if (!range.atEnd() || !list || !list->length())
        return nullptr;

    return list;
}

bool CSSPropertyParser::parseFontPaletteValuesDescriptor(CSSPropertyID propId)
{
    ASSERT(isExposed(propId, &m_context.propertySettings));

    RefPtr<CSSValue> parsedValue;
    switch (propId) {
    case CSSPropertyFontFamily:
        parsedValue = consumeFamilyName(m_range);
        break;
    case CSSPropertyBasePalette:
        parsedValue = consumeBasePaletteDescriptor(m_range);
        break;
    case CSSPropertyOverrideColors:
        parsedValue = consumeOverrideColorsDescriptor(m_range, m_context);
        break;
    default:
        break;
    }

    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(propId, CSSPropertyInvalid, *parsedValue, false);
    return true;
}

bool CSSPropertyParser::consumeFont(bool important)
{
    if (CSSPropertyParserHelpers::isSystemFontShorthand(m_range.peek().id())) {
        auto systemFont = m_range.consumeIncludingWhitespace().id();
        if (!m_range.atEnd())
            return false;

        // We can't store properties (weight, size, etc.) of the system font here,
        // since those values can change (e.g. accessibility font sizes, or accessibility bold).
        // Parsing (correctly) doesn't re-run in response to updateStyleAfterChangeInEnvironment().
        // Instead, we store sentinel values, later replaced by environment-sensitive values
        // inside Style::BuilderCustom and Style::BuilderConverter.
        for (auto property : fontShorthand())
            addProperty(property, CSSPropertyFont, CSSValuePool::singleton().createIdentifierValue(systemFont), important, true);

        return true;
    }

    auto range = m_range;

    RefPtr<CSSValue> fontStyle;
    RefPtr<CSSValue> fontVariantCaps;
    RefPtr<CSSValue> fontWeight;
    RefPtr<CSSValue> fontStretch;
    RefPtr<CSSValue> fontSize;
    RefPtr<CSSValue> lineHeight;
    RefPtr<CSSValue> fontFamily;

    // Optional font-style, font-variant, font-stretch and font-weight, in any order.
    for (unsigned i = 0; i < 4 && !range.atEnd(); ++i) {
        if (consumeIdent<CSSValueNormal>(range))
            continue;
        if (!fontStyle && (fontStyle = consumeFontStyle(range, m_context.mode, CSSValuePool::singleton())))
            continue;
        if (!fontVariantCaps && (fontVariantCaps = consumeIdent<CSSValueSmallCaps>(range)))
            continue;
        if (!fontWeight && (fontWeight = consumeFontWeight(range)))
            continue;
        if (!fontStretch && (fontStretch = consumeFontStretchKeywordValue(range, CSSValuePool::singleton())))
            continue;
        break;
    }

    if (range.atEnd())
        return false;

    fontSize = CSSPropertyParsing::consumeFontSize(range, m_context);
    if (!fontSize || range.atEnd())
        return false;

    if (consumeSlashIncludingWhitespace(range)) {
        if (!consumeIdent<CSSValueNormal>(range)) {
            lineHeight = CSSPropertyParsing::consumeLineHeight(range, m_context);
            if (!lineHeight)
                return false;
        }
        if (range.atEnd())
            return false;
    }

    fontFamily = consumeFontFamily(range);
    if (!fontFamily || !range.atEnd())
        return false;

    auto reset = [&] (CSSPropertyID property, CSSValueID initialValue) {
        ASSERT(initialValue != CSSValueInvalid);
        addProperty(property, CSSPropertyFont, CSSValuePool::singleton().createIdentifierValue(initialValue), important, true);
    };
    auto add = [&] (CSSPropertyID property, RefPtr<CSSValue>& value) {
        if (value)
            addProperty(property, CSSPropertyFont, value.releaseNonNull(), important);
        else
            reset(property, CSSValueNormal);
    };

    // This must be in the same order as the list of shorthands in CSSProperties.json.
    add(CSSPropertyFontStyle, fontStyle);
    add(CSSPropertyFontVariantCaps, fontVariantCaps);
    add(CSSPropertyFontWeight, fontWeight);
    add(CSSPropertyFontStretch, fontStretch);
    add(CSSPropertyFontSize, fontSize);
    add(CSSPropertyLineHeight, lineHeight);
    add(CSSPropertyFontFamily, fontFamily);
    for (auto [property, initialValue] : fontShorthandSubpropertiesResetToInitialValues)
        reset(property, initialValue);

    m_range = range;
    return true;
}

bool CSSPropertyParser::consumeTextDecorationSkip(bool important)
{
    if (RefPtr<CSSPrimitiveValue> skip = consumeIdent<CSSValueNone, CSSValueAuto, CSSValueInk>(m_range)) {
        switch (skip->valueID()) {
        case CSSValueNone:
            addProperty(CSSPropertyTextDecorationSkipInk, CSSPropertyTextDecorationSkip, skip.releaseNonNull(), important);
            return m_range.atEnd();
        case CSSValueAuto:
            addProperty(CSSPropertyTextDecorationSkipInk, CSSPropertyTextDecorationSkip, skip.releaseNonNull(), important);
            return m_range.atEnd();
        case CSSValueInk:
            addProperty(CSSPropertyTextDecorationSkipInk, CSSPropertyTextDecorationSkip, CSSValuePool::singleton().createIdentifierValue(CSSValueAuto), important);
            return m_range.atEnd();
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }
    return false;
}

bool CSSPropertyParser::consumeFontVariantShorthand(bool important)
{
    if (identMatches<CSSValueNormal, CSSValueNone>(m_range.peek().id())) {
        addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFontVariant, consumeIdent(m_range).releaseNonNull(), important);
        addProperty(CSSPropertyFontVariantCaps, CSSPropertyFontVariant, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
        addProperty(CSSPropertyFontVariantAlternates, CSSPropertyFontVariant, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
        addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFontVariant, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
        addProperty(CSSPropertyFontVariantEastAsian, CSSPropertyFontVariant, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
        addProperty(CSSPropertyFontVariantPosition, CSSPropertyFontVariant, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
        return m_range.atEnd();
    }

    RefPtr<CSSValue> capsValue;
    RefPtr<CSSValue> alternatesValue;
    RefPtr<CSSValue> positionValue;

    RefPtr<CSSValue> eastAsianValue;
    CSSFontVariantLigaturesParser ligaturesParser;
    CSSFontVariantNumericParser numericParser;
    bool implicitLigatures = true;
    bool implicitNumeric = true;
    do {
        if (!capsValue) {
            capsValue = CSSPropertyParsing::consumeFontVariantCaps(m_range);
            if (capsValue)
                continue;
        }
        
        if (!positionValue) {
            positionValue = CSSPropertyParsing::consumeFontVariantPosition(m_range);
            if (positionValue)
                continue;
        }

        if (!alternatesValue) {
            alternatesValue = consumeFontVariantAlternates(m_range);
            if (alternatesValue)
                continue;
        }

        CSSFontVariantLigaturesParser::ParseResult ligaturesParseResult = ligaturesParser.consumeLigature(m_range);
        CSSFontVariantNumericParser::ParseResult numericParseResult = numericParser.consumeNumeric(m_range);
        if (ligaturesParseResult == CSSFontVariantLigaturesParser::ParseResult::ConsumedValue) {
            implicitLigatures = false;
            continue;
        }
        if (numericParseResult == CSSFontVariantNumericParser::ParseResult::ConsumedValue) {
            implicitNumeric = false;
            continue;
        }

        if (ligaturesParseResult == CSSFontVariantLigaturesParser::ParseResult::DisallowedValue
            || numericParseResult == CSSFontVariantNumericParser::ParseResult::DisallowedValue)
            return false;

        if (!eastAsianValue) {
            eastAsianValue = consumeFontVariantEastAsian(m_range);
            if (eastAsianValue)
                continue;
        }

        // Saw some value that didn't match anything else.
        return false;

    } while (!m_range.atEnd());

    auto& valuePool = CSSValuePool::singleton();
    addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFontVariant, ligaturesParser.finalizeValue().releaseNonNull(), important, implicitLigatures);
    addPropertyWithImplicitDefault(CSSPropertyFontVariantCaps, CSSPropertyFontVariant, WTFMove(capsValue), valuePool.createIdentifierValue(CSSValueNormal), important);
    addPropertyWithImplicitDefault(CSSPropertyFontVariantAlternates, CSSPropertyFontVariant, WTFMove(alternatesValue), valuePool.createIdentifierValue(CSSValueNormal), important);
    addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFontVariant, numericParser.finalizeValue().releaseNonNull(), important, implicitNumeric);
    addPropertyWithImplicitDefault(CSSPropertyFontVariantEastAsian, CSSPropertyFontVariant, WTFMove(eastAsianValue), valuePool.createIdentifierValue(CSSValueNormal), important);
    addPropertyWithImplicitDefault(CSSPropertyFontVariantPosition, CSSPropertyFontVariant, WTFMove(positionValue), valuePool.createIdentifierValue(CSSValueNormal), important);

    return true;
}

bool CSSPropertyParser::consumeFontSynthesis(bool important)
{
    // none | [ weight || style || small-caps ]
    if (m_range.peek().id() == CSSValueNone) {
        addProperty(CSSPropertyFontSynthesisSmallCaps, CSSPropertyFontSynthesis, consumeIdent(m_range).releaseNonNull(), important);
        addProperty(CSSPropertyFontSynthesisStyle, CSSPropertyFontSynthesis, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        addProperty(CSSPropertyFontSynthesisWeight, CSSPropertyFontSynthesis, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        return m_range.atEnd();
    }

    bool foundWeight = false;
    bool foundStyle = false;
    bool foundSmallCaps = false;

    auto checkAndMarkExistence = [](bool* found) {
        if (*found)
            return false;
        return *found = true;
    };

    while (!m_range.atEnd()) {
        auto ident = consumeIdent<CSSValueWeight, CSSValueStyle, CSSValueSmallCaps>(m_range);
        if (!ident)
            return false;
        switch (ident->valueID()) {
        case CSSValueWeight:
            if (!checkAndMarkExistence(&foundWeight))
                return false;
            break;
        case CSSValueStyle:
            if (!checkAndMarkExistence(&foundStyle))
                return false;
            break;
        case CSSValueSmallCaps:
            if (!checkAndMarkExistence(&foundSmallCaps))
                return false;
            break;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    addProperty(CSSPropertyFontSynthesisWeight, CSSPropertyFontSynthesis, CSSValuePool::singleton().createIdentifierValue(foundWeight ? CSSValueAuto : CSSValueNone), important);
    addProperty(CSSPropertyFontSynthesisStyle, CSSPropertyFontSynthesis, CSSValuePool::singleton().createIdentifierValue(foundStyle ? CSSValueAuto : CSSValueNone), important);
    addProperty(CSSPropertyFontSynthesisSmallCaps, CSSPropertyFontSynthesis, CSSValuePool::singleton().createIdentifierValue(foundSmallCaps ? CSSValueAuto : CSSValueNone), important);
    
    return true;
}

bool CSSPropertyParser::consumeBorderSpacing(bool important)
{
    RefPtr<CSSValue> horizontalSpacing = consumeLength(m_range, m_context.mode, ValueRange::NonNegative, UnitlessQuirk::Allow);
    if (!horizontalSpacing)
        return false;
    RefPtr<CSSValue> verticalSpacing = horizontalSpacing;
    if (!m_range.atEnd())
        verticalSpacing = consumeLength(m_range, m_context.mode, ValueRange::NonNegative, UnitlessQuirk::Allow);
    if (!verticalSpacing || !m_range.atEnd())
        return false;
    addProperty(CSSPropertyWebkitBorderHorizontalSpacing, CSSPropertyBorderSpacing, horizontalSpacing.releaseNonNull(), important);
    addProperty(CSSPropertyWebkitBorderVerticalSpacing, CSSPropertyBorderSpacing, verticalSpacing.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumeColumns(bool important)
{
    RefPtr<CSSValue> columnWidth;
    RefPtr<CSSValue> columnCount;

    for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !m_range.atEnd(); ++propertiesParsed) {
        if (m_range.peek().id() == CSSValueAuto) {
            // 'auto' is a valid value for any of the two longhands, and at this point
            // we don't know which one(s) it is meant for. We need to see if there are other values first.
            consumeIdent(m_range);
        } else {
            if (!columnWidth && (columnWidth = CSSPropertyParsing::consumeColumnWidth(m_range)))
                continue;
            if (!columnCount && (columnCount = CSSPropertyParsing::consumeColumnCount(m_range)))
                continue;
            // If we didn't find at least one match, this is an invalid shorthand and we have to ignore it.
            return false;
        }
    }

    if (!m_range.atEnd())
        return false;

    // If both values are auto, set column-width explicitly to auto so the shorthand serializes correctly.
    if (!columnWidth && !columnCount)
        columnWidth = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);

    addPropertyWithImplicitDefault(CSSPropertyColumnWidth, CSSPropertyColumns, WTFMove(columnWidth), CSSValuePool::singleton().createIdentifierValue(CSSValueAuto), important);
    addPropertyWithImplicitDefault(CSSPropertyColumnCount, CSSPropertyColumns, WTFMove(columnCount), CSSValuePool::singleton().createIdentifierValue(CSSValueAuto), important);

    return true;
}

bool CSSPropertyParser::consumeShorthandGreedily(const StylePropertyShorthand& shorthand, bool important)
{
    ASSERT(shorthand.length() <= 6); // Existing shorthands have at most 6 longhands.
    RefPtr<CSSValue> longhands[6];
    const CSSPropertyID* shorthandProperties = shorthand.properties();
    do {
        bool foundLonghand = false;
        for (size_t i = 0; !foundLonghand && i < shorthand.length(); ++i) {
            if (longhands[i])
                continue;
            longhands[i] = parseSingleValue(shorthandProperties[i], shorthand.id());
            if (longhands[i])
                foundLonghand = true;
        }
        if (!foundLonghand)
            return false;
    } while (!m_range.atEnd());

    for (size_t i = 0; i < shorthand.length(); ++i) {
        addPropertyWithImplicitDefault(shorthandProperties[i], shorthand.id(), WTFMove(longhands[i]), CSSValuePool::singleton().createImplicitInitialValue(), important);
    }
    return true;
}

bool CSSPropertyParser::consumeFlex(bool important)
{
    static const double unsetValue = -1;
    double flexGrow = unsetValue;
    double flexShrink = unsetValue;
    RefPtr<CSSPrimitiveValue> flexBasis;

    if (m_range.peek().id() == CSSValueNone) {
        flexGrow = 0;
        flexShrink = 0;
        flexBasis = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
        m_range.consumeIncludingWhitespace();
    } else {
        unsigned index = 0;
        while (!m_range.atEnd() && index++ < 3) {
            if (auto number = consumeNumberRaw(m_range)) {
                if (number->value < 0)
                    return false;
                if (flexGrow == unsetValue)
                    flexGrow = number->value;
                else if (flexShrink == unsetValue)
                    flexShrink = number->value;
                else if (!number->value) // flex only allows a basis of 0 (sans units) if flex-grow and flex-shrink values have already been set.
                    flexBasis = CSSPrimitiveValue::create(0, CSSUnitType::CSS_PX);
                else
                    return false;
            } else if (!flexBasis) {
                if (isFlexBasisIdent(m_range.peek().id()))
                    flexBasis = consumeIdent(m_range);
                if (!flexBasis)
                    flexBasis = consumeLengthOrPercent(m_range, m_context.mode, ValueRange::NonNegative);
                if (index == 2 && !m_range.atEnd())
                    return false;
            }
        }
        if (index == 0)
            return false;
        if (flexGrow == unsetValue)
            flexGrow = 1;
        if (flexShrink == unsetValue)
            flexShrink = 1;
        
        // FIXME: Using % here is a hack to work around intrinsic sizing implementation being
        // a mess (e.g., turned off for nested column flexboxes, failing to relayout properly even
        // if turned back on for nested columns, etc.). We have layout test coverage of both
        // scenarios.
        if (!flexBasis)
            flexBasis = CSSPrimitiveValue::create(0, CSSUnitType::CSS_PERCENTAGE);
    }

    if (!m_range.atEnd())
        return false;
    addProperty(CSSPropertyFlexGrow, CSSPropertyFlex, CSSPrimitiveValue::create(clampTo<float>(flexGrow), CSSUnitType::CSS_NUMBER), important);
    addProperty(CSSPropertyFlexShrink, CSSPropertyFlex, CSSPrimitiveValue::create(clampTo<float>(flexShrink), CSSUnitType::CSS_NUMBER), important);
    addProperty(CSSPropertyFlexBasis, CSSPropertyFlex, flexBasis.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumeBorder(RefPtr<CSSValue>& width, RefPtr<CSSValue>& style, RefPtr<CSSValue>& color)
{
    while (!width || !style || !color) {
        if (!width) {
            width = CSSPropertyParsing::consumeLineWidth(m_range, m_context);
            if (width)
                continue;
        }
        if (!style) {
            style = parseSingleValue(CSSPropertyBorderLeftStyle, CSSPropertyBorder);
            if (style)
                continue;
        }
        if (!color) {
            color = consumeColor(m_range, m_context);
            if (color)
                continue;
        }
        break;
    }

    if (!width && !style && !color)
        return false;

    if (!width)
        width = CSSValuePool::singleton().createImplicitInitialValue();
    if (!style)
        style = CSSValuePool::singleton().createImplicitInitialValue();
    if (!color)
        color = CSSValuePool::singleton().createImplicitInitialValue();

    return m_range.atEnd();
}

bool CSSPropertyParser::consume2ValueShorthand(const StylePropertyShorthand& shorthand, bool important)
{
    ASSERT(shorthand.length() == 2);
    const CSSPropertyID* longhands = shorthand.properties();
    RefPtr<CSSValue> start = parseSingleValue(longhands[0], shorthand.id());
    if (!start)
        return false;

    RefPtr<CSSValue> end = parseSingleValue(longhands[1], shorthand.id());
    bool endImplicit = !end;
    if (endImplicit)
        end = start;
    addProperty(longhands[0], shorthand.id(), start.releaseNonNull(), important);
    addProperty(longhands[1], shorthand.id(), end.releaseNonNull(), important, endImplicit);

    return m_range.atEnd();
}

bool CSSPropertyParser::consume4ValueShorthand(const StylePropertyShorthand& shorthand, bool important)
{
    ASSERT(shorthand.length() == 4);
    const CSSPropertyID* longhands = shorthand.properties();
    RefPtr<CSSValue> top = parseSingleValue(longhands[0], shorthand.id());
    if (!top)
        return false;

    RefPtr<CSSValue> right = parseSingleValue(longhands[1], shorthand.id());
    RefPtr<CSSValue> bottom;
    RefPtr<CSSValue> left;
    if (right) {
        bottom = parseSingleValue(longhands[2], shorthand.id());
        if (bottom)
            left = parseSingleValue(longhands[3], shorthand.id());
    }

    bool rightImplicit = !right;
    bool bottomImplicit = !bottom;
    bool leftImplicit = !left;

    if (!right)
        right = top;
    if (!bottom)
        bottom = top;
    if (!left)
        left = right;

    addProperty(longhands[0], shorthand.id(), top.releaseNonNull(), important);
    addProperty(longhands[1], shorthand.id(), right.releaseNonNull(), important, rightImplicit);
    addProperty(longhands[2], shorthand.id(), bottom.releaseNonNull(), important, bottomImplicit);
    addProperty(longhands[3], shorthand.id(), left.releaseNonNull(), important, leftImplicit);

    return m_range.atEnd();
}

bool CSSPropertyParser::consumeBorderImage(CSSPropertyID property, bool important)
{
    RefPtr<CSSValue> source;
    RefPtr<CSSValue> slice;
    RefPtr<CSSValue> width;
    RefPtr<CSSValue> outset;
    RefPtr<CSSValue> repeat;

    if (consumeBorderImageComponents(property, m_range, m_context, source, slice, width, outset, repeat)) {
        auto& valuePool = CSSValuePool::singleton();
        auto createQuad = [&](double value, CSSUnitType type) {
            auto quad = Quad::create();
            quad->setTop(valuePool.createValue(value, type));
            quad->setRight(valuePool.createValue(value, type));
            quad->setBottom(valuePool.createValue(value, type));
            quad->setLeft(valuePool.createValue(value, type));
            return quad;
        };
        switch (property) {
        case CSSPropertyWebkitMaskBoxImage:
            addPropertyWithImplicitDefault(CSSPropertyWebkitMaskBoxImageSource, property, WTFMove(source), valuePool.createImplicitInitialValue(), important);
            addPropertyWithImplicitDefault(CSSPropertyWebkitMaskBoxImageSlice, property, WTFMove(slice), valuePool.createImplicitInitialValue(), important);
            addPropertyWithImplicitDefault(CSSPropertyWebkitMaskBoxImageWidth, property, WTFMove(width), valuePool.createImplicitInitialValue(), important);
            addPropertyWithImplicitDefault(CSSPropertyWebkitMaskBoxImageOutset, property, WTFMove(outset), valuePool.createImplicitInitialValue(), important);
            addPropertyWithImplicitDefault(CSSPropertyWebkitMaskBoxImageRepeat, property, WTFMove(repeat), valuePool.createImplicitInitialValue(), important);
            return true;
        case CSSPropertyBorderImage:
        case CSSPropertyWebkitBorderImage:
            addPropertyWithImplicitDefault(CSSPropertyBorderImageSource, property, WTFMove(source), valuePool.createIdentifierValue(CSSValueNone), important);
            addPropertyWithImplicitDefault(CSSPropertyBorderImageSlice, property, WTFMove(slice), CSSBorderImageSliceValue::create(createQuad(100, CSSUnitType::CSS_PERCENTAGE), false), important);
            addPropertyWithImplicitDefault(CSSPropertyBorderImageWidth, property, WTFMove(width), CSSBorderImageWidthValue::create(createQuad(1, CSSUnitType::CSS_NUMBER), false), important);
            addPropertyWithImplicitDefault(CSSPropertyBorderImageOutset, property, WTFMove(outset), valuePool.singleton().createValue(createQuad(0, CSSUnitType::CSS_NUMBER)), important);
            addPropertyWithImplicitDefault(CSSPropertyBorderImageRepeat, property, WTFMove(repeat), valuePool.createIdentifierValue(CSSValueStretch), important);
            return true;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }
    return false;
}

static inline CSSValueID mapFromPageBreakBetween(CSSValueID value)
{
    if (value == CSSValueAlways)
        return CSSValuePage;
    if (value == CSSValueAuto || value == CSSValueAvoid || value == CSSValueLeft || value == CSSValueRight)
        return value;
    return CSSValueInvalid;
}

static inline CSSValueID mapFromColumnBreakBetween(CSSValueID value)
{
    if (value == CSSValueAlways)
        return CSSValueColumn;
    if (value == CSSValueAuto)
        return value;
    if (value == CSSValueAvoid)
        return CSSValueAvoidColumn;
    return CSSValueInvalid;
}

static inline CSSValueID mapFromColumnRegionOrPageBreakInside(CSSValueID value)
{
    if (value == CSSValueAuto || value == CSSValueAvoid)
        return value;
    return CSSValueInvalid;
}

static inline CSSPropertyID mapFromLegacyBreakProperty(CSSPropertyID property)
{
    if (property == CSSPropertyPageBreakAfter || property == CSSPropertyWebkitColumnBreakAfter)
        return CSSPropertyBreakAfter;
    if (property == CSSPropertyPageBreakBefore || property == CSSPropertyWebkitColumnBreakBefore)
        return CSSPropertyBreakBefore;
    ASSERT(property == CSSPropertyPageBreakInside || property == CSSPropertyWebkitColumnBreakInside);
    return CSSPropertyBreakInside;
}

bool CSSPropertyParser::consumeLegacyBreakProperty(CSSPropertyID property, bool important)
{
    // The fragmentation spec says that page-break-(after|before|inside) are to be treated as
    // shorthands for their break-(after|before|inside) counterparts. We'll do the same for the
    // non-standard properties -webkit-column-break-(after|before|inside).
    RefPtr<CSSPrimitiveValue> keyword = consumeIdent(m_range);
    if (!keyword)
        return false;
    if (!m_range.atEnd())
        return false;
    CSSValueID value = keyword->valueID();
    switch (property) {
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
        value = mapFromPageBreakBetween(value);
        break;
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
        value = mapFromColumnBreakBetween(value);
        break;
    case CSSPropertyPageBreakInside:
    case CSSPropertyWebkitColumnBreakInside:
        value = mapFromColumnRegionOrPageBreakInside(value);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    if (value == CSSValueInvalid)
        return false;

    CSSPropertyID genericBreakProperty = mapFromLegacyBreakProperty(property);
    addProperty(genericBreakProperty, property, CSSValuePool::singleton().createIdentifierValue(value), important);
    return true;
}

bool CSSPropertyParser::consumeLegacyTextOrientation(bool important)
{
    // -webkit-text-orientation is a legacy shorthand for text-orientation.
    // The only difference is that it accepts 'sideways-right', which is mapped into 'sideways'.
    RefPtr<CSSPrimitiveValue> keyword;
    auto valueID = m_range.peek().id();
    if (valueID == CSSValueSidewaysRight) {
        keyword = CSSValuePool::singleton().createIdentifierValue(CSSValueSideways);
        consumeIdentRaw(m_range);
    } else if (CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyTextOrientation, valueID, m_context))
        keyword = consumeIdent(m_range);
    if (!keyword || !m_range.atEnd())
        return false;
    addProperty(CSSPropertyTextOrientation, CSSPropertyWebkitTextOrientation, keyword.releaseNonNull(), important);
    return true;
}

static bool isValidAnimationPropertyList(CSSPropertyID property, const CSSValueList& valueList)
{
    // If there is more than one <single-transition> in the shorthand, and any of the transitions
    // has none as the <single-transition-property>, then the declaration is invalid.

    if (property != CSSPropertyTransitionProperty || valueList.length() < 2)
        return true;

    for (auto& value : valueList) {
        if (isValueID(value, CSSValueNone))
            return false;
    }
    return true;
}

static RefPtr<CSSValue> consumeAnimationValueForShorthand(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context)
{
    switch (property) {
    case CSSPropertyAnimationDelay:
    case CSSPropertyTransitionDelay:
        return consumeTime(range, context.mode, ValueRange::All, UnitlessQuirk::Forbid);
    case CSSPropertyAnimationDirection:
        return CSSPropertyParsing::consumeSingleAnimationDirection(range);
    case CSSPropertyAnimationDuration:
    case CSSPropertyTransitionDuration:
        return consumeTime(range, context.mode, ValueRange::NonNegative, UnitlessQuirk::Forbid);
    case CSSPropertyAnimationFillMode:
        return CSSPropertyParsing::consumeSingleAnimationFillMode(range);
    case CSSPropertyAnimationIterationCount:
        return CSSPropertyParsing::consumeSingleAnimationIterationCount(range);
    case CSSPropertyAnimationName:
        return CSSPropertyParsing::consumeSingleAnimationName(range, context);
    case CSSPropertyAnimationPlayState:
        return CSSPropertyParsing::consumeSingleAnimationPlayState(range);
    case CSSPropertyAnimationComposition:
        return CSSPropertyParsing::consumeSingleAnimationComposition(range);
    case CSSPropertyTransitionProperty:
        return consumeSingleTransitionPropertyOrNone(range);
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
        return consumeTimingFunction(range, context);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

bool CSSPropertyParser::consumeAnimationShorthand(const StylePropertyShorthand& shorthand, bool important)
{
    const unsigned longhandCount = shorthand.length();
    RefPtr<CSSValueList> longhands[8];
    ASSERT(longhandCount <= 8);
    for (size_t i = 0; i < longhandCount; ++i)
        longhands[i] = CSSValueList::createCommaSeparated();

    do {
        bool parsedLonghand[8] = { false };
        do {
            bool foundProperty = false;
            for (size_t i = 0; i < longhandCount; ++i) {
                if (parsedLonghand[i])
                    continue;

                if (auto value = consumeAnimationValueForShorthand(shorthand.properties()[i], m_range, m_context)) {
                    parsedLonghand[i] = true;
                    foundProperty = true;
                    longhands[i]->append(*value);
                    break;
                }
            }
            if (!foundProperty)
                return false;
        } while (!m_range.atEnd() && m_range.peek().type() != CommaToken);

        // FIXME: This will make invalid longhands, see crbug.com/386459
        for (size_t i = 0; i < longhandCount; ++i) {
            if (!parsedLonghand[i])
                ComputedStyleExtractor::addValueForAnimationPropertyToList(*longhands[i], shorthand.properties()[i], nullptr);
            parsedLonghand[i] = false;
        }
    } while (consumeCommaIncludingWhitespace(m_range));

    for (size_t i = 0; i < longhandCount; ++i) {
        if (!isValidAnimationPropertyList(shorthand.properties()[i], *longhands[i]))
            return false;
    }

    for (size_t i = 0; i < longhandCount; ++i)
        addProperty(shorthand.properties()[i], shorthand.id(), *longhands[i], important);

    return m_range.atEnd();
}

static void addBackgroundValue(RefPtr<CSSValue>& list, Ref<CSSValue>&& value)
{
    assignOrDowngradeToListAndAppend(list, WTFMove(value));
}

static bool consumeBackgroundPosition(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, RefPtr<CSSValue>& resultX, RefPtr<CSSValue>& resultY)
{
    do {
        auto position = consumePositionCoordinates(range, context.mode, UnitlessQuirk::Allow, property == CSSPropertyMaskPosition ? PositionSyntax::Position : PositionSyntax::BackgroundPosition, NegativePercentagePolicy::Allow);
        if (!position)
            return false;
        addBackgroundValue(resultX, WTFMove(position->x));
        addBackgroundValue(resultY, WTFMove(position->y));
    } while (consumeCommaIncludingWhitespace(range));
    return true;
}

// Note: consumeBackgroundShorthand assumes y properties (for example background-position-y) follow
// the x properties in the shorthand array.
bool CSSPropertyParser::consumeBackgroundShorthand(const StylePropertyShorthand& shorthand, bool important)
{
    const unsigned longhandCount = shorthand.length();
    RefPtr<CSSValue> longhands[10];
    ASSERT(longhandCount <= 10);

    bool implicit = false;
    do {
        bool parsedLonghand[10] = { false };
        RefPtr<CSSValue> originValue;
        do {
            bool foundProperty = false;
            for (size_t i = 0; i < longhandCount; ++i) {
                if (parsedLonghand[i])
                    continue;

                RefPtr<CSSValue> value;
                RefPtr<CSSValue> valueY;
                CSSPropertyID property = shorthand.properties()[i];
                if (property == CSSPropertyBackgroundPositionX || property == CSSPropertyWebkitMaskPositionX) {
                    CSSParserTokenRange rangeCopy = m_range;
                    auto position = consumePositionCoordinates(rangeCopy, m_context.mode, UnitlessQuirk::Forbid, PositionSyntax::BackgroundPosition);
                    if (!position)
                        continue;
                    value = WTFMove(position->x);
                    valueY = WTFMove(position->y);
                    m_range = rangeCopy;
                } else if (property == CSSPropertyBackgroundSize) {
                    if (!consumeSlashIncludingWhitespace(m_range))
                        continue;
                    value = consumeSingleBackgroundSize(m_range, m_context);
                    if (!value || !parsedLonghand[i - 1]) // Position must have been parsed in the current layer.
                        return false;
                } else if (property == CSSPropertyMaskSize) {
                    if (!consumeSlashIncludingWhitespace(m_range))
                        continue;
                    value = consumeSingleMaskSize(m_range, m_context);
                    if (!value || !parsedLonghand[i - 1]) // Position must have been parsed in the current layer.
                        return false;
                } else if (property == CSSPropertyBackgroundPositionY || property == CSSPropertyWebkitMaskPositionY) {
                    continue;
                } else {
                    value = consumeBackgroundComponent(property, m_range, m_context);
                }
                if (value) {
                    if (property == CSSPropertyBackgroundOrigin || property == CSSPropertyMaskOrigin)
                        originValue = value;
                    parsedLonghand[i] = true;
                    foundProperty = true;
                    addBackgroundValue(longhands[i], value.releaseNonNull());
                    if (valueY) {
                        parsedLonghand[i + 1] = true;
                        addBackgroundValue(longhands[i + 1], valueY.releaseNonNull());
                    }
                }
            }
            if (!foundProperty)
                return false;
        } while (!m_range.atEnd() && m_range.peek().type() != CommaToken);

        // FIXME: This will make invalid longhands, see crbug.com/386459
        for (size_t i = 0; i < longhandCount; ++i) {
            CSSPropertyID property = shorthand.properties()[i];
            if (property == CSSPropertyBackgroundColor && !m_range.atEnd()) {
                if (parsedLonghand[i])
                    return false; // Colors are only allowed in the last layer.
                continue;
            }
            if ((property == CSSPropertyBackgroundClip || property == CSSPropertyMaskClip || property == CSSPropertyWebkitMaskClip) && !parsedLonghand[i] && originValue) {
                addBackgroundValue(longhands[i], originValue.releaseNonNull());
                continue;
            }
            if (!parsedLonghand[i])
                addBackgroundValue(longhands[i], CSSValuePool::singleton().createImplicitInitialValue());
        }
    } while (consumeCommaIncludingWhitespace(m_range));
    if (!m_range.atEnd())
        return false;

    for (size_t i = 0; i < longhandCount; ++i) {
        CSSPropertyID property = shorthand.properties()[i];
        if (property == CSSPropertyBackgroundSize && longhands[i] && m_context.useLegacyBackgroundSizeShorthandBehavior)
            continue;
        addProperty(property, shorthand.id(), *longhands[i], important, implicit);
    }
    return true;
}

bool CSSPropertyParser::consumeOverflowShorthand(bool important)
{
    CSSValueID xValueID = m_range.consumeIncludingWhitespace().id();
    if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyOverflowY, xValueID, m_context))
        return false;

    CSSValueID yValueID;
    if (m_range.atEnd()) {
        yValueID = xValueID;

        // FIXME: -webkit-paged-x or -webkit-paged-y only apply to overflow-y. If this value has been
        // set using the shorthand, then for now overflow-x will default to auto, but once we implement
        // pagination controls, it should default to hidden. If the overflow-y value is anything but
        // paged-x or paged-y, then overflow-x and overflow-y should have the same value.
        if (xValueID == CSSValueWebkitPagedX || xValueID == CSSValueWebkitPagedY)
            xValueID = CSSValueAuto;
    } else 
        yValueID = m_range.consumeIncludingWhitespace().id();

    if (!CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyOverflowY, yValueID, m_context))
        return false;
    if (!m_range.atEnd())
        return false;

    addProperty(CSSPropertyOverflowX, CSSPropertyOverflow, CSSValuePool::singleton().createIdentifierValue(xValueID), important);
    addProperty(CSSPropertyOverflowY, CSSPropertyOverflow, CSSValuePool::singleton().createIdentifierValue(yValueID), important);
    return true;
}

static bool isCustomIdentValue(const CSSValue& value)
{
    return is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).isCustomIdent();
}

bool CSSPropertyParser::consumeGridItemPositionShorthand(CSSPropertyID shorthandId, bool important)
{
    const StylePropertyShorthand& shorthand = shorthandForProperty(shorthandId);
    ASSERT(shorthand.length() == 2);
    RefPtr<CSSValue> startValue = consumeGridLine(m_range);
    if (!startValue)
        return false;

    RefPtr<CSSValue> endValue;
    if (consumeSlashIncludingWhitespace(m_range)) {
        endValue = consumeGridLine(m_range);
        if (!endValue)
            return false;
    } else {
        endValue = isCustomIdentValue(*startValue) ? startValue : CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    }
    if (!m_range.atEnd())
        return false;
    addProperty(shorthand.properties()[0], shorthandId, startValue.releaseNonNull(), important);
    addProperty(shorthand.properties()[1], shorthandId, endValue.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumeGridAreaShorthand(bool important)
{
    RefPtr<CSSValue> rowStartValue = consumeGridLine(m_range);
    if (!rowStartValue)
        return false;
    RefPtr<CSSValue> columnStartValue;
    RefPtr<CSSValue> rowEndValue;
    RefPtr<CSSValue> columnEndValue;
    if (consumeSlashIncludingWhitespace(m_range)) {
        columnStartValue = consumeGridLine(m_range);
        if (!columnStartValue)
            return false;
        if (consumeSlashIncludingWhitespace(m_range)) {
            rowEndValue = consumeGridLine(m_range);
            if (!rowEndValue)
                return false;
            if (consumeSlashIncludingWhitespace(m_range)) {
                columnEndValue = consumeGridLine(m_range);
                if (!columnEndValue)
                    return false;
            }
        }
    }
    if (!m_range.atEnd())
        return false;
    if (!columnStartValue)
        columnStartValue = isCustomIdentValue(*rowStartValue) ? rowStartValue : CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    if (!rowEndValue)
        rowEndValue = isCustomIdentValue(*rowStartValue) ? rowStartValue : CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    if (!columnEndValue)
        columnEndValue = isCustomIdentValue(*columnStartValue) ? columnStartValue : CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);

    addProperty(CSSPropertyGridRowStart, CSSPropertyGridArea, rowStartValue.releaseNonNull(), important);
    addProperty(CSSPropertyGridColumnStart, CSSPropertyGridArea, columnStartValue.releaseNonNull(), important);
    addProperty(CSSPropertyGridRowEnd, CSSPropertyGridArea, rowEndValue.releaseNonNull(), important);
    addProperty(CSSPropertyGridColumnEnd, CSSPropertyGridArea, columnEndValue.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumeGridTemplateRowsAndAreasAndColumns(CSSPropertyID shorthandId, bool important)
{
    NamedGridAreaMap gridAreaMap;
    size_t rowCount = 0;
    size_t columnCount = 0;
    RefPtr<CSSValueList> templateRows = CSSValueList::createSpaceSeparated();

    // Persists between loop iterations so we can use the same value for
    // consecutive <line-names> values
    RefPtr<CSSGridLineNamesValue> lineNames;

    do {
        // Handle leading <custom-ident>*.
        bool hasPreviousLineNames = lineNames;
        lineNames = consumeGridLineNames(m_range, lineNames.get());
        if (lineNames && !hasPreviousLineNames)
            templateRows->append(lineNames.releaseNonNull());

        // Handle a template-area's row.
        if (m_range.peek().type() != StringToken || !parseGridTemplateAreasRow(m_range.consumeIncludingWhitespace().value(), gridAreaMap, rowCount, columnCount))
            return false;
        ++rowCount;

        // Handle template-rows's track-size.
        RefPtr<CSSValue> value = consumeGridTrackSize(m_range, m_context.mode);
        if (!value)
            value = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
        templateRows->append(*value);

        // This will handle the trailing/leading <custom-ident>* in the grammar.
        lineNames = consumeGridLineNames(m_range);
        if (lineNames)
            templateRows->append(*lineNames);
    } while (!m_range.atEnd() && !(m_range.peek().type() == DelimiterToken && m_range.peek().delimiter() == '/'));

    RefPtr<CSSValue> columnsValue;
    if (!m_range.atEnd()) {
        if (!consumeSlashIncludingWhitespace(m_range))
            return false;
        columnsValue = consumeGridTrackList(m_range, m_context, GridTemplateNoRepeat);
        if (!columnsValue || !m_range.atEnd())
            return false;
    } else {
        columnsValue = CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    }
    addProperty(CSSPropertyGridTemplateRows, shorthandId, templateRows.releaseNonNull(), important);
    addProperty(CSSPropertyGridTemplateColumns, shorthandId, columnsValue.releaseNonNull(), important);
    addProperty(CSSPropertyGridTemplateAreas, shorthandId, CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount), important);
    return true;
}

bool CSSPropertyParser::consumeGridTemplateShorthand(CSSPropertyID shorthandId, bool important)
{
    CSSParserTokenRange rangeCopy = m_range;
    RefPtr<CSSValue> rowsValue = consumeIdent<CSSValueNone>(m_range);

    // 1- 'none' case.
    if (rowsValue && m_range.atEnd()) {
        addProperty(CSSPropertyGridTemplateRows, shorthandId, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        addProperty(CSSPropertyGridTemplateColumns, shorthandId, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        addProperty(CSSPropertyGridTemplateAreas, shorthandId, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        return true;
    }

    // 2- <grid-template-rows> / <grid-template-columns>
    if (!rowsValue)
        rowsValue = consumeGridTrackList(m_range, m_context, GridTemplate);

    if (rowsValue) {
        if (!consumeSlashIncludingWhitespace(m_range))
            return false;
        RefPtr<CSSValue> columnsValue = consumeGridTemplatesRowsOrColumns(m_range, m_context);
        if (!columnsValue || !m_range.atEnd())
            return false;

        addProperty(CSSPropertyGridTemplateRows, shorthandId, rowsValue.releaseNonNull(), important);
        addProperty(CSSPropertyGridTemplateColumns, shorthandId, columnsValue.releaseNonNull(), important);
        addProperty(CSSPropertyGridTemplateAreas, shorthandId, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        return true;
    }

    // 3- [ <line-names>? <string> <track-size>? <line-names>? ]+ [ / <track-list> ]?
    m_range = rangeCopy;
    return consumeGridTemplateRowsAndAreasAndColumns(shorthandId, important);
}

static RefPtr<CSSValue> consumeImplicitGridAutoFlow(CSSParserTokenRange& range, Ref<CSSPrimitiveValue>&& flowDirection)
{
    // [ auto-flow && dense? ]
    if (range.atEnd())
        return nullptr;
    RefPtr<CSSValue> denseIdent;
    if (range.peek().id() == CSSValueAutoFlow) {
        range.consumeIncludingWhitespace();
        denseIdent = consumeIdent<CSSValueDense>(range);
    } else {
        // Dense case
        if (range.peek().id() != CSSValueDense)
            return nullptr;
        denseIdent = consumeIdent<CSSValueDense>(range);
        if (!denseIdent || !consumeIdent<CSSValueAutoFlow>(range))
            return nullptr;
    }
    auto list = CSSValueList::createSpaceSeparated();
    if (flowDirection->valueID() == CSSValueColumn || !denseIdent)
        list->append(WTFMove(flowDirection));
    if (denseIdent)
        list->append(denseIdent.releaseNonNull());
    
    return list;
}

bool CSSPropertyParser::consumeGridShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyGrid).length() == 6);

    CSSParserTokenRange rangeCopy = m_range;

    // 1- <grid-template>
    if (consumeGridTemplateShorthand(CSSPropertyGrid, important)) {
        // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
        // The sub-properties not specified are set to their initial value, as normal for shorthands.
        addProperty(CSSPropertyGridAutoFlow, CSSPropertyGrid, CSSValuePool::singleton().createIdentifierValue(CSSValueRow), important);
        addProperty(CSSPropertyGridAutoColumns, CSSPropertyGrid, CSSValuePool::singleton().createIdentifierValue(CSSValueAuto), important);
        addProperty(CSSPropertyGridAutoRows, CSSPropertyGrid, CSSValuePool::singleton().createIdentifierValue(CSSValueAuto), important);

        return true;
    }

    m_range = rangeCopy;

    RefPtr<CSSValue> autoColumnsValue;
    RefPtr<CSSValue> autoRowsValue;
    RefPtr<CSSValue> templateRows;
    RefPtr<CSSValue> templateColumns;
    RefPtr<CSSValue> gridAutoFlow;
    
    if (m_range.peek().id() == CSSValueAutoFlow || m_range.peek().id() == CSSValueDense) {
        // 2- [ auto-flow && dense? ] <grid-auto-rows>? / <grid-template-columns>
        gridAutoFlow = consumeImplicitGridAutoFlow(m_range, CSSValuePool::singleton().createIdentifierValue(CSSValueRow));
        if (!gridAutoFlow || m_range.atEnd())
            return false;
        if (consumeSlashIncludingWhitespace(m_range))
            autoRowsValue = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
        else {
            autoRowsValue = consumeGridTrackList(m_range, m_context, GridAuto);
            if (!autoRowsValue)
                return false;
            if (!consumeSlashIncludingWhitespace(m_range))
                return false;
        }
        if (m_range.atEnd())
            return false;
        templateColumns = consumeGridTemplatesRowsOrColumns(m_range, m_context);
        if (!templateColumns)
            return false;
        templateRows = CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
        autoColumnsValue = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    } else {
        // 3- <grid-template-rows> / [ auto-flow && dense? ] <grid-auto-columns>?
        templateRows = consumeGridTemplatesRowsOrColumns(m_range, m_context.mode);
        if (!templateRows)
            return false;
        if (!consumeSlashIncludingWhitespace(m_range) || m_range.atEnd())
            return false;
        gridAutoFlow = consumeImplicitGridAutoFlow(m_range, CSSValuePool::singleton().createIdentifierValue(CSSValueColumn));
        if (!gridAutoFlow)
            return false;
        if (m_range.atEnd())
            autoColumnsValue = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
        else {
            autoColumnsValue = consumeGridTrackList(m_range, m_context, GridAuto);
            if (!autoColumnsValue)
                return false;
        }
        templateColumns = CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
        autoRowsValue = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    }
    
    if (!m_range.atEnd())
        return false;
    
    // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
    // The sub-properties not specified are set to their initial value, as normal for shorthands.
    addProperty(CSSPropertyGridTemplateColumns, CSSPropertyGrid, templateColumns.releaseNonNull(), important);
    addProperty(CSSPropertyGridTemplateRows, CSSPropertyGrid, templateRows.releaseNonNull(), important);
    addProperty(CSSPropertyGridTemplateAreas, CSSPropertyGrid, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
    addProperty(CSSPropertyGridAutoFlow, CSSPropertyGrid, gridAutoFlow.releaseNonNull(), important);
    addProperty(CSSPropertyGridAutoColumns, CSSPropertyGrid, autoColumnsValue.releaseNonNull(), important);
    addProperty(CSSPropertyGridAutoRows, CSSPropertyGrid, autoRowsValue.releaseNonNull(), important);
    
    return true;
}

bool CSSPropertyParser::consumePlaceContentShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyPlaceContent).length() == 2);

    if (m_range.atEnd())
        return false;

    CSSParserTokenRange rangeCopy = m_range;
    bool isBaseline = isBaselineKeyword(m_range.peek().id());
    RefPtr<CSSValue> alignContentValue = consumeContentDistributionOverflowPosition(m_range, isContentPositionKeyword);
    if (!alignContentValue)
        return false;

    // justify-content property does not allow the <baseline-position> values.
    if (m_range.atEnd() && isBaseline)
        return false;
    if (isBaselineKeyword(m_range.peek().id()))
        return false;

    if (m_range.atEnd())
        m_range = rangeCopy;
    RefPtr<CSSValue> justifyContentValue = consumeContentDistributionOverflowPosition(m_range, isContentPositionOrLeftOrRightKeyword);
    if (!justifyContentValue)
        return false;
    if (!m_range.atEnd())
        return false;

    addProperty(CSSPropertyAlignContent, CSSPropertyPlaceContent, alignContentValue.releaseNonNull(), important);
    addProperty(CSSPropertyJustifyContent, CSSPropertyPlaceContent, justifyContentValue.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumePlaceItemsShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyPlaceItems).length() == 2);

    CSSParserTokenRange rangeCopy = m_range;
    RefPtr<CSSValue> alignItemsValue = consumeAlignItems(m_range);
    if (!alignItemsValue)
        return false;

    if (m_range.atEnd())
        m_range = rangeCopy;
    RefPtr<CSSValue> justifyItemsValue = consumeJustifyItems(m_range);
    if (!justifyItemsValue)
        return false;

    if (!m_range.atEnd())
        return false;

    addProperty(CSSPropertyAlignItems, CSSPropertyPlaceItems, alignItemsValue.releaseNonNull(), important);
    addProperty(CSSPropertyJustifyItems, CSSPropertyPlaceItems, justifyItemsValue.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumePlaceSelfShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyPlaceSelf).length() == 2);

    CSSParserTokenRange rangeCopy = m_range;
    RefPtr<CSSValue> alignSelfValue = consumeSelfPositionOverflowPosition(m_range, isSelfPositionKeyword);
    if (!alignSelfValue)
        return false;

    if (m_range.atEnd())
        m_range = rangeCopy;
    RefPtr<CSSValue> justifySelfValue = consumeSelfPositionOverflowPosition(m_range, isSelfPositionOrLeftOrRightKeyword);
    if (!justifySelfValue)
        return false;

    if (!m_range.atEnd())
        return false;

    addProperty(CSSPropertyAlignSelf, CSSPropertyPlaceSelf, alignSelfValue.releaseNonNull(), important);
    addProperty(CSSPropertyJustifySelf, CSSPropertyPlaceSelf, justifySelfValue.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumeOverscrollBehaviorShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyOverscrollBehavior).length() == 2);

    if (m_range.atEnd())
        return false;

    RefPtr<CSSValue> overscrollBehaviorX = CSSPropertyParsing::consumeOverscrollBehaviorX(m_range);
    if (!overscrollBehaviorX)
        return false;

    RefPtr<CSSValue> overscrollBehaviorY;
    m_range.consumeWhitespace();
    if (m_range.atEnd())
        overscrollBehaviorY = overscrollBehaviorX;
    else {
        overscrollBehaviorY = CSSPropertyParsing::consumeOverscrollBehaviorY(m_range);
        m_range.consumeWhitespace();
        if (!m_range.atEnd())
            return false;
    }

    addProperty(CSSPropertyOverscrollBehaviorX, CSSPropertyOverscrollBehavior, *overscrollBehaviorX, important);
    addProperty(CSSPropertyOverscrollBehaviorY, CSSPropertyOverscrollBehavior, *overscrollBehaviorY, important);
    return true;
}

bool CSSPropertyParser::consumeContainerShorthand(bool important)
{
    auto name = consumeContainerName(m_range);
    if (!name)
        return false;

    bool sawSlash = false;

    auto consumeSlashType = [&]() -> RefPtr<CSSValue> {
        if (m_range.atEnd())
            return nullptr;
        if (!consumeSlashIncludingWhitespace(m_range))
            return nullptr;
        sawSlash = true;
        return parseSingleValue(CSSPropertyContainerType);
    };

    auto type = consumeSlashType();

    if (!m_range.atEnd() || (sawSlash && !type))
        return false;

    addProperty(CSSPropertyContainerName, CSSPropertyContainer, name.releaseNonNull(), important);
    addPropertyWithImplicitDefault(CSSPropertyContainerType, CSSPropertyContainer, WTFMove(type), CSSValuePool::singleton().createImplicitInitialValue(), important);
    return true;
}

bool CSSPropertyParser::consumeContainIntrinsicSizeShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyContainIntrinsicSize).length() == 2);
    ASSERT(isExposed(CSSPropertyContainIntrinsicSize, &m_context.propertySettings));

    if (m_range.atEnd())
        return false;

    RefPtr<CSSValue> containIntrinsicWidth = consumeContainIntrinsicSize(m_range);
    if (!containIntrinsicWidth)
        return false;

    RefPtr<CSSValue> containIntrinsicHeight;
    m_range.consumeWhitespace();
    if (m_range.atEnd())
        containIntrinsicHeight = containIntrinsicWidth;
    else {
        containIntrinsicHeight = consumeContainIntrinsicSize(m_range);
        m_range.consumeWhitespace();
        if (!m_range.atEnd() || !containIntrinsicHeight)
            return false;
    }

    addProperty(CSSPropertyContainIntrinsicWidth, CSSPropertyContainIntrinsicSize, *containIntrinsicWidth, important);
    addProperty(CSSPropertyContainIntrinsicHeight, CSSPropertyContainIntrinsicSize, *containIntrinsicHeight, important);
    return true;
}

bool CSSPropertyParser::consumeTransformOrigin(bool important)
{
    if (auto resultXY = consumeOneOrTwoValuedPositionCoordinates(m_range, m_context.mode, UnitlessQuirk::Forbid)) {
        m_range.consumeWhitespace();
        bool atEnd = m_range.atEnd();
        auto resultZ = consumeLength(m_range, m_context.mode, ValueRange::All);
        if ((!resultZ && !atEnd) || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyTransformOriginX, CSSPropertyTransformOrigin, WTFMove(resultXY->x), important);
        addProperty(CSSPropertyTransformOriginY, CSSPropertyTransformOrigin, WTFMove(resultXY->y), important);
        addPropertyWithImplicitDefault(CSSPropertyTransformOriginZ, CSSPropertyTransformOrigin, resultZ, CSSValuePool::singleton().createValue(0, CSSUnitType::CSS_PX), important);
        
        return true;
    }
    return false;
}

bool CSSPropertyParser::consumePerspectiveOrigin(bool important)
{
    if (auto result = consumePositionCoordinates(m_range, m_context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position)) {
        addProperty(CSSPropertyPerspectiveOriginX, CSSPropertyPerspectiveOrigin, WTFMove(result->x), important);
        addProperty(CSSPropertyPerspectiveOriginY, CSSPropertyPerspectiveOrigin, WTFMove(result->y), important);
        return true;
    }
    return false;
}

bool CSSPropertyParser::consumePrefixedPerspective(bool important)
{
    if (auto value = consumePerspective(m_range, m_context.mode)) {
        addProperty(CSSPropertyPerspective, CSSPropertyWebkitPerspective, value.releaseNonNull(), important);
        return m_range.atEnd();
    }

    if (auto perspective = consumeNumberRaw(m_range)) {
        if (perspective->value < 0)
            return false;
        auto value = CSSPrimitiveValue::create(perspective->value, CSSUnitType::CSS_PX);
        addProperty(CSSPropertyPerspective, CSSPropertyWebkitPerspective, WTFMove(value), important);
        return m_range.atEnd();
    }

    return false;
}

bool CSSPropertyParser::consumeOffset(bool important)
{
    auto& valuePool = CSSValuePool::singleton();

    // The offset shorthand is defined as:
    // [ <'offset-position'>?
    //   [ <'offset-path'>
    //     [ <'offset-distance'> || <'offset-rotate'> ]?
    //   ]?
    // ]!
    // [ / <'offset-anchor'> ]?

    // Parse out offset-position.
    auto offsetPosition = parseSingleValue(CSSPropertyOffsetPosition, CSSPropertyOffset);

    // Parse out offset-path.
    auto offsetPath = parseSingleValue(CSSPropertyOffsetPath, CSSPropertyOffset);

    // Either one of offset-position and offset-path must be present.
    if (!offsetPosition && !offsetPath)
        return false;

    // Only parse offset-distance and offset-rotate if offset-path is specified.
    RefPtr<CSSValue> offsetDistance;
    RefPtr<CSSValue> offsetRotate;
    if (offsetPath) {
        // Try to parse offset-distance first. If successful, parse the following offset-rotate.
        // Otherwise, parse in the reverse order.
        if ((offsetDistance = parseSingleValue(CSSPropertyOffsetDistance, CSSPropertyOffset)))
            offsetRotate = parseSingleValue(CSSPropertyOffsetRotate, CSSPropertyOffset);
        else {
            offsetRotate = parseSingleValue(CSSPropertyOffsetRotate, CSSPropertyOffset);
            offsetDistance = parseSingleValue(CSSPropertyOffsetDistance, CSSPropertyOffset);
        }
    }

    // Parse out offset-anchor. Only parse if the prefix slash is present.
    RefPtr<CSSValue> offsetAnchor;
    if (consumeSlashIncludingWhitespace(m_range)) {
        // offset-anchor must follow the slash.
        if (!(offsetAnchor = parseSingleValue(CSSPropertyOffsetAnchor, CSSPropertyOffset)))
            return false;
    }

    addPropertyWithImplicitDefault(CSSPropertyOffsetPath, CSSPropertyOffset, WTFMove(offsetPath), valuePool.createIdentifierValue(CSSValueNone), important);
    addPropertyWithImplicitDefault(CSSPropertyOffsetDistance, CSSPropertyOffset, WTFMove(offsetDistance), valuePool.createValue(0.0, CSSUnitType::CSS_PX), important);
    addPropertyWithImplicitDefault(CSSPropertyOffsetPosition, CSSPropertyOffset, WTFMove(offsetPosition), valuePool.createIdentifierValue(CSSValueAuto), important);
    addPropertyWithImplicitDefault(CSSPropertyOffsetAnchor, CSSPropertyOffset, WTFMove(offsetAnchor), valuePool.createIdentifierValue(CSSValueAuto), important);
    addPropertyWithImplicitDefault(CSSPropertyOffsetRotate, CSSPropertyOffset, WTFMove(offsetRotate), CSSOffsetRotateValue::initialValue(), important);

    return m_range.atEnd();
}

bool CSSPropertyParser::consumeListStyleShorthand(bool important)
{
    auto& valuePool = CSSValuePool::singleton();
    RefPtr<CSSValue> parsedPosition;
    RefPtr<CSSValue> parsedImage;
    RefPtr<CSSValue> parsedType;
    unsigned noneCount = 0;

    while (!m_range.atEnd()) {
        if (m_range.peek().id() == CSSValueNone) {
            ++noneCount;
            consumeIdent(m_range);
            continue;
        }
        if (!parsedPosition) {
            if (auto position = parseSingleValue(CSSPropertyListStylePosition, CSSPropertyListStyle)) {
                parsedPosition = position;
                continue;
            }
        }
        if (!parsedImage) {
            if (auto image = parseSingleValue(CSSPropertyListStyleImage, CSSPropertyListStyle)) {
                parsedImage = image;
                continue;
            }
        }
        if (!parsedType) {
            if (auto type = parseSingleValue(CSSPropertyListStyleType, CSSPropertyListStyle)) {
                parsedType = type;
                continue;
            }
        }
        return false;
    }

    if (noneCount > (static_cast<unsigned>(!parsedImage + !parsedType)))
        return false;

    // Use the implicit initial value for list-style-image, to serialize to "none" instead of "none none".
    if (noneCount == 2) {
        parsedImage = valuePool.createImplicitInitialValue();
        parsedType = valuePool.createIdentifierValue(CSSValueNone);
    } else if (noneCount == 1) {
        if (!parsedImage)
            parsedImage = parsedType ? valuePool.createIdentifierValue(CSSValueNone) : valuePool.createImplicitInitialValue();
        if (!parsedType)
            parsedType = valuePool.createIdentifierValue(CSSValueNone);
    }

    addPropertyWithImplicitDefault(CSSPropertyListStylePosition, CSSPropertyListStyle, WTFMove(parsedPosition), valuePool.createImplicitInitialValue(), important);
    addPropertyWithImplicitDefault(CSSPropertyListStyleImage, CSSPropertyListStyle, WTFMove(parsedImage), valuePool.createImplicitInitialValue(), important);
    addPropertyWithImplicitDefault(CSSPropertyListStyleType, CSSPropertyListStyle, WTFMove(parsedType), valuePool.createImplicitInitialValue(), important);
    return m_range.atEnd();
}

bool CSSPropertyParser::parseShorthand(CSSPropertyID property, bool important)
{
    switch (property) {
    case CSSPropertyOverflow:
        return consumeOverflowShorthand(important);
    case CSSPropertyOverscrollBehavior:
        return consumeOverscrollBehaviorShorthand(important);
    case CSSPropertyFont:
        return consumeFont(important);
    case CSSPropertyFontVariant:
        return consumeFontVariantShorthand(important);
    case CSSPropertyFontSynthesis:
        return consumeFontSynthesis(important);
    case CSSPropertyBorderSpacing:
        return consumeBorderSpacing(important);
    case CSSPropertyColumns:
        return consumeColumns(important);
    case CSSPropertyAnimation:
        return consumeAnimationShorthand(animationShorthandForParsing(), important);
    case CSSPropertyTransition:
        return consumeAnimationShorthand(transitionShorthandForParsing(), important);
    case CSSPropertyTextDecoration: {
        auto line = consumeTextDecorationLine(m_range);
        if (!line || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyTextDecorationLine, property, line.releaseNonNull(), important);
        return true;
    }
    case CSSPropertyWebkitTextDecoration:
        // FIXME-NEWPARSER: We need to unprefix -line/-style/-color ASAP and get rid
        // of -webkit-text-decoration completely.
        return consumeShorthandGreedily(webkitTextDecorationShorthand(), important);
    case CSSPropertyInset:
        return consume4ValueShorthand(insetShorthand(), important);
    case CSSPropertyInsetBlock:
        return consume2ValueShorthand(insetBlockShorthand(), important);
    case CSSPropertyInsetInline:
        return consume2ValueShorthand(insetInlineShorthand(), important);
    case CSSPropertyMargin:
        return consume4ValueShorthand(marginShorthand(), important);
    case CSSPropertyMarginBlock:
        return consume2ValueShorthand(marginBlockShorthand(), important);
    case CSSPropertyMarginInline:
        return consume2ValueShorthand(marginInlineShorthand(), important);
    case CSSPropertyPadding:
        return consume4ValueShorthand(paddingShorthand(), important);
    case CSSPropertyPaddingBlock:
        return consume2ValueShorthand(paddingBlockShorthand(), important);
    case CSSPropertyPaddingInline:
        return consume2ValueShorthand(paddingInlineShorthand(), important);
    case CSSPropertyScrollMargin:
        return consume4ValueShorthand(scrollMarginShorthand(), important);
    case CSSPropertyScrollMarginBlock:
        return consume2ValueShorthand(scrollMarginBlockShorthand(), important);
    case CSSPropertyScrollMarginInline:
        return consume2ValueShorthand(scrollMarginInlineShorthand(), important);
    case CSSPropertyScrollPadding:
        return consume4ValueShorthand(scrollPaddingShorthand(), important);
    case CSSPropertyScrollPaddingBlock:
        return consume2ValueShorthand(scrollPaddingBlockShorthand(), important);
    case CSSPropertyScrollPaddingInline:
        return consume2ValueShorthand(scrollPaddingInlineShorthand(), important);
    case CSSPropertyTextEmphasis:
        return consumeShorthandGreedily(textEmphasisShorthand(), important);
    case CSSPropertyOutline:
        return consumeShorthandGreedily(outlineShorthand(), important);
    case CSSPropertyOffset:
        return consumeOffset(important);
    case CSSPropertyBorderInline: {
        RefPtr<CSSValue> width;
        RefPtr<CSSValue> style;
        RefPtr<CSSValue> color;
        if (!consumeBorder(width, style, color))
            return false;

        addExpandedPropertyForValue(CSSPropertyBorderInlineWidth, width.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderInlineStyle, style.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderInlineColor, color.releaseNonNull(), important);
        return true;
    }
    case CSSPropertyBorderInlineColor:
        return consume2ValueShorthand(borderInlineColorShorthand(), important);
    case CSSPropertyBorderInlineStyle:
        return consume2ValueShorthand(borderInlineStyleShorthand(), important);
    case CSSPropertyBorderInlineWidth:
        return consume2ValueShorthand(borderInlineWidthShorthand(), important);
    case CSSPropertyBorderInlineStart:
        return consumeShorthandGreedily(borderInlineStartShorthand(), important);
    case CSSPropertyBorderInlineEnd:
        return consumeShorthandGreedily(borderInlineEndShorthand(), important);
    case CSSPropertyBorderBlock: {
        RefPtr<CSSValue> width;
        RefPtr<CSSValue> style;
        RefPtr<CSSValue> color;
        if (!consumeBorder(width, style, color))
            return false;

        addExpandedPropertyForValue(CSSPropertyBorderBlockWidth, width.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderBlockStyle, style.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderBlockColor, color.releaseNonNull(), important);
        return true;
    }
    case CSSPropertyBorderBlockColor:
        return consume2ValueShorthand(borderBlockColorShorthand(), important);
    case CSSPropertyBorderBlockStyle:
        return consume2ValueShorthand(borderBlockStyleShorthand(), important);
    case CSSPropertyBorderBlockWidth:
        return consume2ValueShorthand(borderBlockWidthShorthand(), important);
    case CSSPropertyBorderBlockStart:
        return consumeShorthandGreedily(borderBlockStartShorthand(), important);
    case CSSPropertyBorderBlockEnd:
        return consumeShorthandGreedily(borderBlockEndShorthand(), important);
    case CSSPropertyWebkitTextStroke:
        return consumeShorthandGreedily(webkitTextStrokeShorthand(), important);
    case CSSPropertyMarker: {
        RefPtr<CSSValue> marker = parseSingleValue(CSSPropertyMarkerStart);
        if (!marker || !m_range.atEnd())
            return false;
        auto markerRef = marker.releaseNonNull();
        addProperty(CSSPropertyMarkerStart, CSSPropertyMarker, markerRef.copyRef(), important);
        addProperty(CSSPropertyMarkerMid, CSSPropertyMarker, markerRef.copyRef(), important);
        addProperty(CSSPropertyMarkerEnd, CSSPropertyMarker, markerRef.copyRef(), important);
        return true;
    }
    case CSSPropertyFlex:
        return consumeFlex(important);
    case CSSPropertyFlexFlow:
        return consumeShorthandGreedily(flexFlowShorthand(), important);
    case CSSPropertyColumnRule:
        return consumeShorthandGreedily(columnRuleShorthand(), important);
    case CSSPropertyListStyle:
        return consumeListStyleShorthand(important);
    case CSSPropertyBorderRadius:
    case CSSPropertyWebkitBorderRadius: {
        RefPtr<CSSPrimitiveValue> horizontalRadii[4];
        RefPtr<CSSPrimitiveValue> verticalRadii[4];
        if (!consumeRadii(horizontalRadii, verticalRadii, m_range, m_context.mode, property == CSSPropertyWebkitBorderRadius))
            return false;
        addProperty(CSSPropertyBorderTopLeftRadius, CSSPropertyBorderRadius, createPrimitiveValuePair(horizontalRadii[0].releaseNonNull(), verticalRadii[0].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce), important);
        addProperty(CSSPropertyBorderTopRightRadius, CSSPropertyBorderRadius, createPrimitiveValuePair(horizontalRadii[1].releaseNonNull(), verticalRadii[1].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce), important);
        addProperty(CSSPropertyBorderBottomRightRadius, CSSPropertyBorderRadius, createPrimitiveValuePair(horizontalRadii[2].releaseNonNull(), verticalRadii[2].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce), important);
        addProperty(CSSPropertyBorderBottomLeftRadius, CSSPropertyBorderRadius, createPrimitiveValuePair(horizontalRadii[3].releaseNonNull(), verticalRadii[3].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce), important);
        return true;
    }
    case CSSPropertyBorderColor:
        return consume4ValueShorthand(borderColorShorthand(), important);
    case CSSPropertyBorderStyle:
        return consume4ValueShorthand(borderStyleShorthand(), important);
    case CSSPropertyBorderWidth:
        return consume4ValueShorthand(borderWidthShorthand(), important);
    case CSSPropertyBorderTop:
        return consumeShorthandGreedily(borderTopShorthand(), important);
    case CSSPropertyBorderRight:
        return consumeShorthandGreedily(borderRightShorthand(), important);
    case CSSPropertyBorderBottom:
        return consumeShorthandGreedily(borderBottomShorthand(), important);
    case CSSPropertyBorderLeft:
        return consumeShorthandGreedily(borderLeftShorthand(), important);
    case CSSPropertyBorder: {
        RefPtr<CSSValue> width;
        RefPtr<CSSValue> style;
        RefPtr<CSSValue> color;
        if (!consumeBorder(width, style, color))
            return false;

        addExpandedPropertyForValue(CSSPropertyBorderWidth, width.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderStyle, style.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderColor, color.releaseNonNull(), important);
        addExpandedPropertyForValue(CSSPropertyBorderImage, CSSValuePool::singleton().createImplicitInitialValue(), important);
        return true;
    }
    case CSSPropertyBorderImage:
    case CSSPropertyWebkitBorderImage:
        return consumeBorderImage(property, important);
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
        return consumeLegacyBreakProperty(property, important);
    case CSSPropertyWebkitTextOrientation:
        return consumeLegacyTextOrientation(important);
    case CSSPropertyMaskPosition:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyBackgroundPosition: {
        RefPtr<CSSValue> resultX;
        RefPtr<CSSValue> resultY;
        if (!consumeBackgroundPosition(m_range, m_context, property, resultX, resultY) || !m_range.atEnd())
            return false;
        addProperty(property == CSSPropertyBackgroundPosition ? CSSPropertyBackgroundPositionX : CSSPropertyWebkitMaskPositionX, property, resultX.releaseNonNull(), important);
        addProperty(property == CSSPropertyBackgroundPosition ? CSSPropertyBackgroundPositionY : CSSPropertyWebkitMaskPositionY, property, resultY.releaseNonNull(), important);
        return true;
    }
    case CSSPropertyBackground:
        return consumeBackgroundShorthand(backgroundShorthand(), important);
    case CSSPropertyWebkitBackgroundSize: {
        auto backgroundSize = consumeCommaSeparatedBackgroundComponent(CSSPropertyWebkitBackgroundSize, m_range, m_context.mode);
        if (!backgroundSize || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyBackgroundSize, CSSPropertyWebkitBackgroundSize, backgroundSize.releaseNonNull(), important);
        return true;
    }
    case CSSPropertyMask:
    case CSSPropertyWebkitMask:
        return consumeBackgroundShorthand(shorthandForProperty(property), important);
    case CSSPropertyTransformOrigin:
        return consumeTransformOrigin(important);
    case CSSPropertyPerspectiveOrigin:
        return consumePerspectiveOrigin(important);
    case CSSPropertyWebkitPerspective:
        return consumePrefixedPerspective(important);
    case CSSPropertyGap: {
        auto rowGap = CSSPropertyParsing::consumeRowGap(m_range, m_context);
        auto columnGap = CSSPropertyParsing::consumeColumnGap(m_range, m_context);
        if (!rowGap || !m_range.atEnd())
            return false;
        if (!columnGap)
            columnGap = rowGap;
        addProperty(CSSPropertyRowGap, CSSPropertyGap, rowGap.releaseNonNull(), important);
        addProperty(CSSPropertyColumnGap, CSSPropertyGap, columnGap.releaseNonNull(), important);
        return true;
    }
    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
        return consumeGridItemPositionShorthand(property, important);
    case CSSPropertyGridArea:
        return consumeGridAreaShorthand(important);
    case CSSPropertyGridTemplate:
        return consumeGridTemplateShorthand(CSSPropertyGridTemplate, important);
    case CSSPropertyGrid:
        return consumeGridShorthand(important);
    case CSSPropertyPlaceContent:
        return consumePlaceContentShorthand(important);
    case CSSPropertyPlaceItems:
        return consumePlaceItemsShorthand(important);
    case CSSPropertyPlaceSelf:
        return consumePlaceSelfShorthand(important);
    case CSSPropertyTextDecorationSkip:
        return consumeTextDecorationSkip(important);
    case CSSPropertyContainer:
        return consumeContainerShorthand(important);
    case CSSPropertyContainIntrinsicSize:
        return consumeContainIntrinsicSizeShorthand(important);
    default:
        return false;
    }
}

} // namespace WebCore
