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
#include "CSSFontStyleRangeValue.h"
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
#include "CSSQuadValue.h"
#include "CSSTokenizer.h"
#include "CSSTransformListValue.h"
#include "CSSValuePair.h"
#include "CSSVariableParser.h"
#include "CSSVariableReferenceValue.h"
#include "FontFace.h"
#include "ParsingUtilities.h"
#include "Rect.h"
#include "StyleBuilder.h"
#include "StyleBuilderConverter.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"
#include "TimingFunction.h"
#include "TransformFunctions.h"
#include <memory>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

bool isCustomPropertyName(StringView propertyName)
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
    ASSERT(length > 0); // Otherwise buffer[0] would access uninitialized memory below.

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

void CSSPropertyParser::addProperty(CSSPropertyID property, CSSPropertyID currentShorthand, RefPtr<CSSValue>&& value, bool important, bool implicit)
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

    if (value && !value->isImplicitInitialValue())
        m_parsedProperties->append(CSSProperty(property, WTFMove(value), important, setFromShorthand, shorthandIndex, implicit));
    else {
        ASSERT(setFromShorthand);
        m_parsedProperties->append(CSSProperty(property, Ref { CSSPrimitiveValue::implicitInitialValue() }, important, setFromShorthand, shorthandIndex, true));
    }
}

void CSSPropertyParser::addExpandedProperty(CSSPropertyID shorthand, RefPtr<CSSValue>&& value, bool important, bool implicit)
{
    for (auto longhand : shorthandForProperty(shorthand))
        addProperty(longhand, shorthand, value.copyRef(), important, implicit);
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
        parseSuccess = parser.parseCounterStyleDescriptor(propertyID);
    else if (ruleType == StyleRuleType::Keyframe)
        parseSuccess = parser.parseKeyframeDescriptor(propertyID, important);
    else if (ruleType == StyleRuleType::Property)
        parseSuccess = parser.parsePropertyDescriptor(propertyID);
    else
        parseSuccess = parser.parseValueStart(propertyID, important);

    if (!parseSuccess)
        parsedProperties.shrink(parsedPropertiesSize);

    return parseSuccess;
}

static RefPtr<CSSPrimitiveValue> maybeConsumeCSSWideKeyword(CSSParserTokenRange& range)
{
    CSSParserTokenRange rangeCopy = range;
    CSSValueID valueID = rangeCopy.consumeIncludingWhitespace().id();
    if (!rangeCopy.atEnd())
        return nullptr;

    if (!isCSSWideKeyword(valueID))
        return nullptr;

    range = rangeCopy;
    return CSSPrimitiveValue::create(valueID);
}

RefPtr<CSSValue> CSSPropertyParser::parseSingleValue(CSSPropertyID property, const CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSPropertyParser parser(range, context, nullptr);
    if (RefPtr value = maybeConsumeCSSWideKeyword(parser.m_range))
        return value;
    
    RefPtr value = parser.parseSingleValue(property);
    if (!value || !parser.m_range.atEnd())
        return nullptr;
    return value;
}

RefPtr<CSSCustomPropertyValue> CSSPropertyParser::parseTypedCustomPropertyValue(const AtomString& name, const CSSCustomPropertySyntax& syntax, const CSSParserTokenRange& tokens, Style::BuilderState& builderState, const CSSParserContext& context)
{
    CSSPropertyParser parser(tokens, context, nullptr, false);
    RefPtr value = parser.parseTypedCustomPropertyValue(name, syntax, builderState);
    if (!value || !parser.m_range.atEnd())
        return nullptr;
    return value;
}

RefPtr<CSSCustomPropertyValue> CSSPropertyParser::parseTypedCustomPropertyInitialValue(const AtomString& name, const CSSCustomPropertySyntax& syntax, CSSParserTokenRange tokens, Style::BuilderState& builderState, const CSSParserContext& context)
{
    if (syntax.isUniversal())
        return CSSVariableParser::parseInitialValueForUniversalSyntax(name, tokens);

    CSSPropertyParser parser(tokens, context, nullptr, false);
    RefPtr value = parser.parseTypedCustomPropertyValue(name, syntax, builderState);
    if (!value || !parser.m_range.atEnd())
        return nullptr;

    if (value->containsCSSWideKeyword())
        return nullptr;

    return value;
}

ComputedStyleDependencies CSSPropertyParser::collectParsedCustomPropertyValueDependencies(const CSSCustomPropertySyntax& syntax, const CSSParserTokenRange& tokens, const CSSParserContext& context)
{
    CSSPropertyParser parser(tokens, context, nullptr);
    return parser.collectParsedCustomPropertyValueDependencies(syntax);
}

bool CSSPropertyParser::isValidCustomPropertyValueForSyntax(const CSSCustomPropertySyntax& syntax, CSSParserTokenRange tokens, const CSSParserContext& context)
{
    if (syntax.isUniversal())
        return true;

    CSSPropertyParser parser { tokens, context, nullptr };
    return !!parser.consumeCustomPropertyValueWithSyntax(syntax).first;
}

bool CSSPropertyParser::parseValueStart(CSSPropertyID propertyID, bool important)
{
    if (consumeCSSWideKeyword(propertyID, important))
        return true;

    CSSParserTokenRange originalRange = m_range;
    bool isShorthand = WebCore::isShorthand(propertyID);

    if (isShorthand) {
        // Variable references will fail to parse here and will fall out to the variable ref parser below.
        if (parseShorthand(propertyID, important))
            return true;
    } else {
        RefPtr parsedValue = parseSingleValue(propertyID);
        if (parsedValue && m_range.atEnd()) {
            addProperty(propertyID, CSSPropertyInvalid, WTFMove(parsedValue), important);
            return true;
        }
    }

    if (CSSVariableParser::containsValidVariableReferences(originalRange, m_context)) {
        Ref variable = CSSVariableReferenceValue::create(originalRange, m_context);
        if (isShorthand)
            addExpandedProperty(propertyID, CSSPendingSubstitutionValue::create(propertyID, WTFMove(variable)), important);
        else
            addProperty(propertyID, CSSPropertyInvalid, WTFMove(variable), important);
        return true;
    }

    return false;
}

bool CSSPropertyParser::consumeCSSWideKeyword(CSSPropertyID propertyID, bool important)
{
    CSSParserTokenRange rangeCopy = m_range;
    RefPtr value = maybeConsumeCSSWideKeyword(rangeCopy);
    if (!value)
        return false;
    
    const StylePropertyShorthand& shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length()) {
        if (CSSProperty::isDescriptorOnly(propertyID))
            return false;
        addProperty(propertyID, CSSPropertyInvalid, value.releaseNonNull(), important);
    } else
        addExpandedProperty(propertyID, value.releaseNonNull(), important);
    m_range = rangeCopy;
    return true;
}

RefPtr<CSSValue> CSSPropertyParser::parseSingleValue(CSSPropertyID property, CSSPropertyID currentShorthand)
{
    return CSSPropertyParsing::parseStyleProperty(m_range, property, currentShorthand, m_context);
}

std::pair<RefPtr<CSSValue>, CSSCustomPropertySyntax::Type> CSSPropertyParser::consumeCustomPropertyValueWithSyntax(const CSSCustomPropertySyntax& syntax)
{
    ASSERT(!syntax.isUniversal());

    auto rangeCopy = m_range;

    auto consumeSingleValue = [&](auto& range, auto& component) -> RefPtr<CSSValue> {
        switch (component.type) {
        case CSSCustomPropertySyntax::Type::Length:
            return consumeLength(range, m_context.mode, ValueRange::All);
        case CSSCustomPropertySyntax::Type::LengthPercentage:
            return consumeLengthOrPercent(range, m_context.mode, ValueRange::All);
        case CSSCustomPropertySyntax::Type::CustomIdent:
            if (RefPtr value = consumeCustomIdent(range)) {
                if (component.ident.isNull() || value->stringValue() == component.ident)
                    return value;
            }
            return nullptr;
        case CSSCustomPropertySyntax::Type::Percentage:
            return consumePercent(range, ValueRange::All);
        case CSSCustomPropertySyntax::Type::Integer:
            return consumeInteger(range);
        case CSSCustomPropertySyntax::Type::Number:
            return consumeNumber(range, ValueRange::All);
        case CSSCustomPropertySyntax::Type::Angle:
            return consumeAngle(range, m_context.mode);
        case CSSCustomPropertySyntax::Type::Time:
            return consumeTime(range, m_context.mode, ValueRange::All);
        case CSSCustomPropertySyntax::Type::Resolution:
            return consumeResolution(range);
        case CSSCustomPropertySyntax::Type::Color:
            return consumeColor(range, m_context);
        case CSSCustomPropertySyntax::Type::Image:
            return consumeImage(range, m_context, { AllowedImageType::URLFunction, AllowedImageType::GeneratedImage });
        case CSSCustomPropertySyntax::Type::URL:
            return consumeURL(range);
        case CSSCustomPropertySyntax::Type::TransformFunction:
            return consumeTransformFunction(m_range, m_context);
        case CSSCustomPropertySyntax::Type::TransformList:
            return consumeTransform(m_range, m_context);
        case CSSCustomPropertySyntax::Type::Unknown:
            return nullptr;
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    };

    auto consumeComponent = [&](auto& range, const auto& component) -> RefPtr<CSSValue> {
        switch (component.multiplier) {
        case CSSCustomPropertySyntax::Multiplier::Single:
            return consumeSingleValue(range, component);
        case CSSCustomPropertySyntax::Multiplier::CommaList: {
            return consumeCommaSeparatedListWithoutSingleValueOptimization(range, [&](auto& range) {
                return consumeSingleValue(range, component);
            });
        }
        case CSSCustomPropertySyntax::Multiplier::SpaceList: {
            CSSValueListBuilder valueList;
            while (RefPtr value = consumeSingleValue(range, component))
                valueList.append(value.releaseNonNull());
            if (valueList.isEmpty())
                return nullptr;
            return CSSValueList::createSpaceSeparated(WTFMove(valueList));
        }
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    };

    for (auto& component : syntax.definition) {
        if (RefPtr value = consumeComponent(m_range, component)) {
            if (m_range.atEnd())
                return { value, component.type };
        }
        m_range = rangeCopy;
    }
    return { nullptr, CSSCustomPropertySyntax::Type::Unknown };
}

ComputedStyleDependencies CSSPropertyParser::collectParsedCustomPropertyValueDependencies(const CSSCustomPropertySyntax& syntax)
{
    if (syntax.isUniversal())
        return { };

    m_range.consumeWhitespace();

    auto [value, syntaxType] = consumeCustomPropertyValueWithSyntax(syntax);
    if (!value)
        return { };

    return value->computedStyleDependencies();
}

RefPtr<CSSCustomPropertyValue> CSSPropertyParser::parseTypedCustomPropertyValue(const AtomString& name, const CSSCustomPropertySyntax& syntax, Style::BuilderState& builderState)
{
    if (syntax.isUniversal())
        return CSSCustomPropertyValue::createSyntaxAll(name, CSSVariableData::create(m_range.consumeAll()));

    m_range.consumeWhitespace();

    if (RefPtr value = maybeConsumeCSSWideKeyword(m_range))
        return CSSCustomPropertyValue::createWithID(name, value->valueID());

    auto [value, syntaxType] = consumeCustomPropertyValueWithSyntax(syntax);
    if (!value)
        return nullptr;

    auto resolveSyntaxValue = [&, syntaxType = syntaxType](const CSSValue& value) -> std::optional<CSSCustomPropertyValue::SyntaxValue> {
        switch (syntaxType) {
        case CSSCustomPropertySyntax::Type::LengthPercentage:
        case CSSCustomPropertySyntax::Type::Length: {
            auto length = Style::BuilderConverter::convertLength(builderState, downcast<CSSPrimitiveValue>(value));
            return { WTFMove(length) };
        }
        case CSSCustomPropertySyntax::Type::Percentage:
        case CSSCustomPropertySyntax::Type::Integer:
        case CSSCustomPropertySyntax::Type::Number:
        case CSSCustomPropertySyntax::Type::Angle:
        case CSSCustomPropertySyntax::Type::Time:
        case CSSCustomPropertySyntax::Type::Resolution: {
            auto canonicalUnit = canonicalUnitTypeForUnitType(downcast<CSSPrimitiveValue>(value).primitiveType());
            auto doubleValue = downcast<CSSPrimitiveValue>(value).doubleValue(canonicalUnit);
            return { CSSCustomPropertyValue::NumericSyntaxValue { doubleValue, canonicalUnit } };
        }
        case CSSCustomPropertySyntax::Type::Color: {
            auto color = builderState.colorFromPrimitiveValue(downcast<CSSPrimitiveValue>(value), Style::ForVisitedLink::No);
            return { color };
        }
        case CSSCustomPropertySyntax::Type::Image: {
            auto styleImage = builderState.createStyleImage(value);
            if (!styleImage)
                return { };
            return { WTFMove(styleImage) };
        }
        case CSSCustomPropertySyntax::Type::URL: {
            auto url = m_context.completeURL(downcast<CSSPrimitiveValue>(value).stringValue());
            return { url.resolvedURL };
        }
        case CSSCustomPropertySyntax::Type::CustomIdent:
            return { downcast<CSSPrimitiveValue>(value).stringValue() };

        case CSSCustomPropertySyntax::Type::TransformFunction:
        case CSSCustomPropertySyntax::Type::TransformList:
            if (auto transform = transformForValue(value, builderState.cssToLengthConversionData()))
                return { CSSCustomPropertyValue::TransformSyntaxValue { transform } };
            return { };
        case CSSCustomPropertySyntax::Type::Unknown:
            return { };
        }
        ASSERT_NOT_REACHED();
        return { };
    };

    if (is<CSSValueList>(value.get()) || is<CSSTransformListValue>(value.get())) {
        Ref valueList = downcast<CSSValueContainingVector>(value.releaseNonNull());
        auto syntaxValueList = CSSCustomPropertyValue::SyntaxValueList { { }, valueList->separator() };
        for (Ref listValue : valueList.get()) {
            auto syntaxValue = resolveSyntaxValue(listValue);
            if (!syntaxValue)
                return nullptr;
            syntaxValueList.values.append(WTFMove(*syntaxValue));
        }
        return CSSCustomPropertyValue::createForSyntaxValueList(name, WTFMove(syntaxValueList));
    };

    auto syntaxValue = resolveSyntaxValue(*value);
    if (!syntaxValue)
        return nullptr;
    return CSSCustomPropertyValue::createForSyntaxValue(name, WTFMove(*syntaxValue));
}

RefPtr<CSSValue> CSSPropertyParser::parseCounterStyleDescriptor(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(context.propertySettings.cssCounterStyleAtRulesEnabled);

    return CSSPropertyParsing::parseCounterStyleDescriptor(range, property, context);
}

bool CSSPropertyParser::parseCounterStyleDescriptor(CSSPropertyID property)
{
    ASSERT(m_context.propertySettings.cssCounterStyleAtRulesEnabled);

    RefPtr parsedValue = CSSPropertyParsing::parseCounterStyleDescriptor(m_range, property, m_context);
    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(property, CSSPropertyInvalid, WTFMove(parsedValue), false);
    return true;
}

bool CSSPropertyParser::parseFontFaceDescriptor(CSSPropertyID property)
{
    if (isShorthand(property))
        return parseFontFaceDescriptorShorthand(property);

    RefPtr parsedValue = CSSPropertyParsing::parseFontFaceDescriptor(m_range, property, m_context);
    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(property, CSSPropertyInvalid, WTFMove(parsedValue), false);
    return true;
}

bool CSSPropertyParser::parseFontFaceDescriptorShorthand(CSSPropertyID property)
{
    ASSERT(isExposed(property, m_context.propertySettings));

    switch (property) {
    case CSSPropertyFontVariant:
        return consumeFontVariantShorthand(false);
    default:
        return false;
    }
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

bool CSSPropertyParser::parsePropertyDescriptor(CSSPropertyID property)
{
    RefPtr parsedValue = CSSPropertyParsing::parsePropertyDescriptor(m_range, property, m_context);
    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(property, CSSPropertyInvalid, WTFMove(parsedValue), false);
    return true;
}

bool CSSPropertyParser::parseFontPaletteValuesDescriptor(CSSPropertyID property)
{
    RefPtr parsedValue = CSSPropertyParsing::parseFontPaletteValuesDescriptor(m_range, property, m_context);
    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(property, CSSPropertyInvalid, WTFMove(parsedValue), false);
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
            addProperty(property, CSSPropertyFont, CSSPrimitiveValue::create(systemFont), important, true);

        return true;
    }

    auto range = m_range;

    RefPtr<CSSValue> values[7];
    auto& fontStyle = values[0];
    auto& fontVariantCaps = values[1];
    auto& fontWeight = values[2];
    auto& fontStretch = values[3];
    auto& fontSize = values[4];
    auto& lineHeight = values[5];
    auto& fontFamily = values[6];

    // Optional font-style, font-variant, font-stretch and font-weight, in any order.
    for (unsigned i = 0; i < 4 && !range.atEnd(); ++i) {
        if (consumeIdent<CSSValueNormal>(range))
            continue;
        if (!fontStyle && (fontStyle = consumeFontStyle(range, m_context.mode)))
            continue;
        if (!fontVariantCaps && (fontVariantCaps = consumeIdent<CSSValueSmallCaps>(range)))
            continue;
        if (!fontWeight && (fontWeight = consumeFontWeight(range)))
            continue;
        if (!fontStretch && (fontStretch = consumeFontStretchKeywordValue(range)))
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

    m_range = range;
    auto shorthand = fontShorthand();
    for (unsigned i = 0; i < shorthand.length(); ++i)
        addProperty(shorthand.properties()[i], CSSPropertyFont, i < std::size(values) ? WTFMove(values[i]) : nullptr, important, true);
    return true;
}

bool CSSPropertyParser::consumeTextDecorationSkip(bool important)
{
    if (RefPtr skip = consumeIdent<CSSValueNone, CSSValueAuto, CSSValueInk>(m_range)) {
        switch (skip->valueID()) {
        case CSSValueNone:
            addProperty(CSSPropertyTextDecorationSkipInk, CSSPropertyTextDecorationSkip, skip.releaseNonNull(), important);
            return m_range.atEnd();
        case CSSValueAuto:
            addProperty(CSSPropertyTextDecorationSkipInk, CSSPropertyTextDecorationSkip, skip.releaseNonNull(), important);
            return m_range.atEnd();
        case CSSValueInk:
            addProperty(CSSPropertyTextDecorationSkipInk, CSSPropertyTextDecorationSkip, CSSPrimitiveValue::create(CSSValueAuto), important);
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
        addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFontVariant, consumeIdent(m_range), important);
        addProperty(CSSPropertyFontVariantCaps, CSSPropertyFontVariant, nullptr, important);
        addProperty(CSSPropertyFontVariantAlternates, CSSPropertyFontVariant, nullptr, important);
        addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFontVariant, nullptr, important);
        addProperty(CSSPropertyFontVariantEastAsian, CSSPropertyFontVariant, nullptr, important);
        addProperty(CSSPropertyFontVariantPosition, CSSPropertyFontVariant, nullptr, important);
        addProperty(CSSPropertyFontVariantEmoji, CSSPropertyFontVariant, nullptr, important);
        return m_range.atEnd();
    }

    RefPtr<CSSValue> capsValue;
    RefPtr<CSSValue> alternatesValue;
    RefPtr<CSSValue> positionValue;
    RefPtr<CSSValue> eastAsianValue;
    RefPtr<CSSValue> emojiValue;
    CSSFontVariantLigaturesParser ligaturesParser;
    CSSFontVariantNumericParser numericParser;
    bool implicitLigatures = true;
    bool implicitNumeric = true;
    do {
        if (!capsValue && (capsValue = CSSPropertyParsing::consumeFontVariantCaps(m_range)))
            continue;
        
        if (!positionValue && (positionValue = CSSPropertyParsing::consumeFontVariantPosition(m_range)))
            continue;

        if (!alternatesValue && (alternatesValue = consumeFontVariantAlternates(m_range)))
            continue;

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

        if (!eastAsianValue && (eastAsianValue = consumeFontVariantEastAsian(m_range)))
            continue;

        if (m_context.propertySettings.cssFontVariantEmojiEnabled && !emojiValue && (emojiValue = CSSPropertyParsing::consumeFontVariantEmoji(m_range)))
            continue;

        // Saw some value that didn't match anything else.
        return false;
    } while (!m_range.atEnd());

    addProperty(CSSPropertyFontVariantLigatures, CSSPropertyFontVariant, ligaturesParser.finalizeValue().releaseNonNull(), important, implicitLigatures);
    addProperty(CSSPropertyFontVariantCaps, CSSPropertyFontVariant, WTFMove(capsValue), important);
    addProperty(CSSPropertyFontVariantAlternates, CSSPropertyFontVariant, WTFMove(alternatesValue), important);
    addProperty(CSSPropertyFontVariantNumeric, CSSPropertyFontVariant, numericParser.finalizeValue().releaseNonNull(), important, implicitNumeric);
    addProperty(CSSPropertyFontVariantEastAsian, CSSPropertyFontVariant, WTFMove(eastAsianValue), important);
    addProperty(CSSPropertyFontVariantPosition, CSSPropertyFontVariant, WTFMove(positionValue), important);
    addProperty(CSSPropertyFontVariantEmoji, CSSPropertyFontVariant, WTFMove(emojiValue), important);

    return true;
}

bool CSSPropertyParser::consumeFontSynthesis(bool important)
{
    // none | [ weight || style || small-caps ]
    if (m_range.peek().id() == CSSValueNone) {
        addProperty(CSSPropertyFontSynthesisSmallCaps, CSSPropertyFontSynthesis, consumeIdent(m_range).releaseNonNull(), important);
        addProperty(CSSPropertyFontSynthesisStyle, CSSPropertyFontSynthesis, CSSPrimitiveValue::create(CSSValueNone), important);
        addProperty(CSSPropertyFontSynthesisWeight, CSSPropertyFontSynthesis, CSSPrimitiveValue::create(CSSValueNone), important);
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
        RefPtr ident = consumeIdent<CSSValueWeight, CSSValueStyle, CSSValueSmallCaps>(m_range);
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

    addProperty(CSSPropertyFontSynthesisWeight, CSSPropertyFontSynthesis, CSSPrimitiveValue::create(foundWeight ? CSSValueAuto : CSSValueNone), important);
    addProperty(CSSPropertyFontSynthesisStyle, CSSPropertyFontSynthesis, CSSPrimitiveValue::create(foundStyle ? CSSValueAuto : CSSValueNone), important);
    addProperty(CSSPropertyFontSynthesisSmallCaps, CSSPropertyFontSynthesis, CSSPrimitiveValue::create(foundSmallCaps ? CSSValueAuto : CSSValueNone), important);
    
    return true;
}

bool CSSPropertyParser::consumeBorderSpacing(bool important)
{
    RefPtr horizontalSpacing = consumeLength(m_range, m_context.mode, ValueRange::NonNegative, UnitlessQuirk::Allow);
    if (!horizontalSpacing)
        return false;
    RefPtr verticalSpacing = horizontalSpacing;
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

    addProperty(CSSPropertyColumnWidth, CSSPropertyColumns, WTFMove(columnWidth), important);
    addProperty(CSSPropertyColumnCount, CSSPropertyColumns, WTFMove(columnCount), important);
    return true;
}

struct InitialNumericValue {
    double number;
    CSSUnitType type { CSSUnitType::CSS_NUMBER };
};
using InitialValue = std::variant<CSSValueID, InitialNumericValue>;

static constexpr InitialValue initialValueForLonghand(CSSPropertyID longhand)
{
    // Currently, this tries to cover just longhands that can be omitted from shorthands when parsing or serializing.
    // Later, we likely want to cover all properties, and generate the table from CSSProperties.json.
    switch (longhand) {
    case CSSPropertyAccentColor:
    case CSSPropertyAlignSelf:
    case CSSPropertyAspectRatio:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBlockSize:
    case CSSPropertyBottom:
    case CSSPropertyBreakAfter:
    case CSSPropertyBreakBefore:
    case CSSPropertyBreakInside:
    case CSSPropertyCaretColor:
    case CSSPropertyClip:
    case CSSPropertyColumnCount:
    case CSSPropertyColumnWidth:
    case CSSPropertyCursor:
    case CSSPropertyDominantBaseline:
    case CSSPropertyFlexBasis:
    case CSSPropertyFontKerning:
    case CSSPropertyFontSynthesisSmallCaps:
    case CSSPropertyFontSynthesisStyle:
    case CSSPropertyFontSynthesisWeight:
    case CSSPropertyGridAutoColumns:
    case CSSPropertyGridAutoRows:
    case CSSPropertyGridColumnEnd:
    case CSSPropertyGridColumnStart:
    case CSSPropertyGridRowEnd:
    case CSSPropertyGridRowStart:
    case CSSPropertyHeight:
    case CSSPropertyImageRendering:
    case CSSPropertyInlineSize:
    case CSSPropertyInputSecurity:
    case CSSPropertyInsetBlockEnd:
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetInlineEnd:
    case CSSPropertyInsetInlineStart:
    case CSSPropertyJustifySelf:
    case CSSPropertyLeft:
    case CSSPropertyLineBreak:
    case CSSPropertyMaskBorderWidth:
    case CSSPropertyMaskSize:
    case CSSPropertyOffsetAnchor:
    case CSSPropertyOffsetRotate:
    case CSSPropertyOverflowAnchor:
    case CSSPropertyOverscrollBehaviorBlock:
    case CSSPropertyOverscrollBehaviorInline:
    case CSSPropertyOverscrollBehaviorX:
    case CSSPropertyOverscrollBehaviorY:
    case CSSPropertyPage:
    case CSSPropertyPointerEvents:
    case CSSPropertyQuotes:
    case CSSPropertyRight:
    case CSSPropertyScrollBehavior:
    case CSSPropertyScrollPaddingBlockEnd:
    case CSSPropertyScrollPaddingBlockStart:
    case CSSPropertyScrollPaddingBottom:
    case CSSPropertyScrollPaddingInlineEnd:
    case CSSPropertyScrollPaddingInlineStart:
    case CSSPropertyScrollPaddingLeft:
    case CSSPropertyScrollPaddingRight:
    case CSSPropertyScrollPaddingTop:
    case CSSPropertyScrollbarColor:
    case CSSPropertyScrollbarGutter:
    case CSSPropertyScrollbarWidth:
    case CSSPropertySize:
    case CSSPropertyTableLayout:
    case CSSPropertyTextAlignLast:
    case CSSPropertyTextDecorationSkipInk:
    case CSSPropertyTextDecorationThickness:
    case CSSPropertyTextJustify:
    case CSSPropertyTextUnderlineOffset:
    case CSSPropertyTextUnderlinePosition:
    case CSSPropertyTop:
    case CSSPropertyWebkitMaskSourceType:
    case CSSPropertyWillChange:
    case CSSPropertyZIndex:
    case CSSPropertyZoom:
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyFontOpticalSizing:
#endif
        return CSSValueAuto;
    case CSSPropertyAlignContent:
    case CSSPropertyAlignItems:
    case CSSPropertyAnimationDirection:
    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyColumnGap:
    case CSSPropertyContainerType:
    case CSSPropertyContent:
    case CSSPropertyFontFeatureSettings:
    case CSSPropertyFontPalette:
    case CSSPropertyFontStretch:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariantAlternates:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontVariantEastAsian:
    case CSSPropertyFontVariantEmoji:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontVariantNumeric:
    case CSSPropertyFontVariantPosition:
    case CSSPropertyFontWeight:
    case CSSPropertyJustifyContent:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLineHeight:
    case CSSPropertyOffsetPosition:
    case CSSPropertyOverflowWrap:
    case CSSPropertyRowGap:
    case CSSPropertyScrollSnapStop:
    case CSSPropertySpeakAs:
    case CSSPropertyTextBoxTrim:
    case CSSPropertyTransitionBehavior:
    case CSSPropertyWordBreak:
    case CSSPropertyWordSpacing:
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyFontVariationSettings:
#endif
        return CSSValueNormal;
    case CSSPropertyAlignmentBaseline:
    case CSSPropertyVerticalAlign:
        return CSSValueBaseline;
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationDuration:
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
        return InitialNumericValue { 0, CSSUnitType::CSS_S };
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationTimeline:
    case CSSPropertyAppearance:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBorderBlockEndStyle:
    case CSSPropertyBorderBlockStartStyle:
    case CSSPropertyBorderBlockStyle:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderInlineEndStyle:
    case CSSPropertyBorderInlineStartStyle:
    case CSSPropertyBorderInlineStyle:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderStyle:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBoxShadow:
    case CSSPropertyClear:
    case CSSPropertyClipPath:
    case CSSPropertyColumnRuleStyle:
    case CSSPropertyColumnSpan:
    case CSSPropertyContain:
    case CSSPropertyContainIntrinsicBlockSize:
    case CSSPropertyContainIntrinsicHeight:
    case CSSPropertyContainIntrinsicInlineSize:
    case CSSPropertyContainIntrinsicWidth:
    case CSSPropertyContainerName:
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
    case CSSPropertyFilter:
    case CSSPropertyFloat:
    case CSSPropertyFontSizeAdjust:
    case CSSPropertyGridTemplateAreas:
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
    case CSSPropertyHangingPunctuation:
    case CSSPropertyListStyleImage:
    case CSSPropertyMarginTrim:
    case CSSPropertyMarkerEnd:
    case CSSPropertyMarkerMid:
    case CSSPropertyMarkerStart:
    case CSSPropertyMaskBorderSource:
    case CSSPropertyMaskImage:
    case CSSPropertyMaxBlockSize:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxInlineSize:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyOffsetPath:
    case CSSPropertyOutlineStyle:
    case CSSPropertyPerspective:
    case CSSPropertyResize:
    case CSSPropertyRotate:
    case CSSPropertyScale:
    case CSSPropertyScrollSnapAlign:
    case CSSPropertyScrollSnapType:
    case CSSPropertyShapeOutside:
    case CSSPropertyStrokeDasharray:
    case CSSPropertyTextCombineUpright:
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextEmphasisStyle:
    case CSSPropertyTextGroupAlign:
    case CSSPropertyTextShadow:
    case CSSPropertyTextTransform:
    case CSSPropertyTransform:
    case CSSPropertyTranslate:
    case CSSPropertyWidth:
        return CSSValueNone;
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyFillOpacity:
    case CSSPropertyFlexShrink:
    case CSSPropertyFloodOpacity:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyOpacity:
        return InitialNumericValue { 1, CSSUnitType::CSS_NUMBER };
    case CSSPropertyAnimationPlayState:
        return CSSValueRunning;
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
        return CSSValueEase;
    case CSSPropertyBackgroundAttachment:
        return CSSValueScroll;
    case CSSPropertyBackfaceVisibility:
    case CSSPropertyContentVisibility:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyVisibility:
        return CSSValueVisible;
    case CSSPropertyBackgroundClip:
    case CSSPropertyMaskClip:
    case CSSPropertyMaskOrigin:
    case CSSPropertyWebkitMaskClip:
        return CSSValueBorderBox;
    case CSSPropertyBackgroundColor:
        return CSSValueTransparent;
    case CSSPropertyBackgroundOrigin:
        return CSSValuePaddingBox;
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
        return InitialNumericValue { 0, CSSUnitType::CSS_PERCENTAGE };
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyMaskRepeat:
        return CSSValueRepeat;
    case CSSPropertyBorderBlockColor:
    case CSSPropertyBorderBlockEndColor:
    case CSSPropertyBorderBlockStartColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderColor:
    case CSSPropertyBorderInlineColor:
    case CSSPropertyBorderInlineEndColor:
    case CSSPropertyBorderInlineStartColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyTextEmphasisColor:
    case CSSPropertyWebkitTextStrokeColor:
        return CSSValueCurrentcolor;
    case CSSPropertyBorderBlockEndWidth:
    case CSSPropertyBorderBlockStartWidth:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderInlineEndWidth:
    case CSSPropertyBorderInlineStartWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyColumnRuleWidth:
    case CSSPropertyFontSize:
    case CSSPropertyOutlineWidth:
        return CSSValueMedium;
    case CSSPropertyBorderCollapse:
        return CSSValueSeparate;
    case CSSPropertyBorderImageOutset:
    case CSSPropertyMaskBorderOutset:
        return InitialNumericValue { 0, CSSUnitType::CSS_NUMBER };
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyMaskBorderRepeat:
        return CSSValueStretch;
    case CSSPropertyBorderImageSlice:
        return InitialNumericValue { 100, CSSUnitType::CSS_PERCENTAGE };
    case CSSPropertyBoxSizing:
        return CSSValueContentBox;
    case CSSPropertyCaptionSide:
        return CSSValueTop;
    case CSSPropertyClipRule:
    case CSSPropertyFillRule:
        return CSSValueNonzero;
    case CSSPropertyColor:
        return CSSValueCanvastext;
    case CSSPropertyColorInterpolationFilters:
        return CSSValueLinearRGB;
    case CSSPropertyColumnFill:
        return CSSValueBalance;
    case CSSPropertyDisplay:
        return CSSValueInline;
    case CSSPropertyEmptyCells:
        return CSSValueShow;
    case CSSPropertyFlexDirection:
    case CSSPropertyGridAutoFlow:
        return CSSValueRow;
    case CSSPropertyFlexWrap:
        return CSSValueNowrap;
    case CSSPropertyFloodColor:
        return CSSValueBlack;
    case CSSPropertyImageOrientation:
        return CSSValueFromImage;
    case CSSPropertyJustifyItems:
        return CSSValueLegacy;
    case CSSPropertyLightingColor:
        return CSSValueWhite;
    case CSSPropertyListStylePosition:
        return CSSValueOutside;
    case CSSPropertyListStyleType:
        return CSSValueDisc;
    case CSSPropertyMaskBorderSlice:
        return InitialNumericValue { 0, CSSUnitType::CSS_NUMBER };
    case CSSPropertyMaskComposite:
        return CSSValueAdd;
    case CSSPropertyMaskMode:
        return CSSValueMatchSource;
    case CSSPropertyMaskType:
        return CSSValueLuminance;
    case CSSPropertyObjectFit:
        return CSSValueFill;
    case CSSPropertyOffsetDistance:
    case CSSPropertyTransformOriginZ:
    case CSSPropertyWebkitTextStrokeWidth:
        return InitialNumericValue { 0, CSSUnitType::CSS_PX };
    case CSSPropertyOrphans:
    case CSSPropertyWidows:
        return InitialNumericValue { 2, CSSUnitType::CSS_NUMBER };
    case CSSPropertyPerspectiveOriginX:
    case CSSPropertyPerspectiveOriginY:
    case CSSPropertyTransformOriginX:
    case CSSPropertyTransformOriginY:
        return InitialNumericValue { 50, CSSUnitType::CSS_PERCENTAGE };
    case CSSPropertyPosition:
        return CSSValueStatic;
    case CSSPropertyPrintColorAdjust:
        return CSSValueEconomy;
    case CSSPropertyScrollTimelineAxis:
    case CSSPropertyViewTimelineAxis:
        return CSSValueBlock;
    case CSSPropertyScrollTimelineName:
    case CSSPropertyViewTimelineName:
        return CSSValueNone;
    case CSSPropertyViewTimelineInset:
        return CSSValueAuto;
    case CSSPropertyStrokeColor:
        return CSSValueTransparent;
    case CSSPropertyStrokeLinecap:
        return CSSValueButt;
    case CSSPropertyStrokeLinejoin:
        return CSSValueMiter;
    case CSSPropertyStrokeMiterlimit:
        return InitialNumericValue { 4, CSSUnitType::CSS_NUMBER };
    case CSSPropertyStrokeWidth:
        return InitialNumericValue { 1, CSSUnitType::CSS_PX };
    case CSSPropertyTabSize:
        return InitialNumericValue { 8, CSSUnitType::CSS_NUMBER };
    case CSSPropertyTextAlign:
        return CSSValueStart;
    case CSSPropertyTextDecorationStyle:
        return CSSValueSolid;
    case CSSPropertyTextBoxEdge:
        return CSSValueLeading;
    case CSSPropertyTextOrientation:
        return CSSValueMixed;
    case CSSPropertyTextOverflow:
        return CSSValueClip;
    case CSSPropertyTextWrapMode:
        return CSSValueWrap;
    case CSSPropertyTextWrapStyle:
        return CSSValueAuto;
    case CSSPropertyTransformBox:
        return CSSValueViewBox;
    case CSSPropertyTransformStyle:
        return CSSValueFlat;
    case CSSPropertyTransitionProperty:
        return CSSValueAll;
    case CSSPropertyWritingMode:
        return CSSValueHorizontalTb;
    case CSSPropertyTextSpacingTrim:
        return CSSValueSpaceAll;
    case CSSPropertyTextAutospace:
        return CSSValueNoAutospace;
    case CSSPropertyWhiteSpaceCollapse:
        return CSSValueCollapse;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static bool isValueIDPair(const CSSValue& value, CSSValueID valueID)
{
    return value.isPair() && isValueID(value.first(), valueID) && isValueID(value.second(), valueID);
}

static bool isNumber(const CSSPrimitiveValue& value, double number, CSSUnitType type)
{
    return value.primitiveType() == type && value.doubleValue() == number;
}

static bool isNumber(const CSSPrimitiveValue* value, double number, CSSUnitType type)
{
    return value && isNumber(*value, number, type);
}

static bool isNumber(const CSSValue& value, double number, CSSUnitType type)
{
    return isNumber(dynamicDowncast<CSSPrimitiveValue>(value), number, type);
}

static bool isNumber(const RectBase& quad, double number, CSSUnitType type)
{
    return isNumber(quad.top(), number, type)
        && isNumber(quad.right(), number, type)
        && isNumber(quad.bottom(), number, type)
        && isNumber(quad.left(), number, type);
}

static bool isValueID(const RectBase& quad, CSSValueID valueID)
{
    return isValueID(quad.top(), valueID)
        && isValueID(quad.right(), valueID)
        && isValueID(quad.bottom(), valueID)
        && isValueID(quad.left(), valueID);
}

static bool isNumericQuad(const CSSValue& value, double number, CSSUnitType type)
{
    return value.isQuad() && isNumber(value.quad(), number, type);
}

bool isInitialValueForLonghand(CSSPropertyID longhand, const CSSValue& value)
{
    if (value.isImplicitInitialValue())
        return true;
    switch (longhand) {
    case CSSPropertyBackgroundSize:
    case CSSPropertyMaskSize:
        if (isValueIDPair(value, CSSValueAuto))
            return true;
        break;
    case CSSPropertyBorderImageOutset:
    case CSSPropertyMaskBorderOutset:
        if (isNumericQuad(value, 0, CSSUnitType::CSS_NUMBER))
            return true;
        break;
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyMaskBorderRepeat:
        if (isValueIDPair(value, CSSValueStretch))
            return true;
        break;
    case CSSPropertyBorderImageSlice:
        if (auto sliceValue = dynamicDowncast<CSSBorderImageSliceValue>(value)) {
            if (!sliceValue->fill() && isNumber(sliceValue->slices(), 100, CSSUnitType::CSS_PERCENTAGE))
                return true;
        }
        break;
    case CSSPropertyBorderImageWidth:
        if (auto widthValue = dynamicDowncast<CSSBorderImageWidthValue>(value)) {
            if (!widthValue->overridesBorderWidths() && isNumber(widthValue->widths(), 1, CSSUnitType::CSS_NUMBER))
                return true;
        }
        break;
    case CSSPropertyOffsetRotate:
        if (auto rotateValue = dynamicDowncast<CSSOffsetRotateValue>(value)) {
            if (rotateValue->isInitialValue())
                return true;
        }
        break;
    case CSSPropertyMaskBorderSlice:
        if (auto sliceValue = dynamicDowncast<CSSBorderImageSliceValue>(value)) {
            if (!sliceValue->fill() && isNumber(sliceValue->slices(), 0, CSSUnitType::CSS_NUMBER))
                return true;
        }
        return false;
    case CSSPropertyMaskBorderWidth:
        if (auto widthValue = dynamicDowncast<CSSBorderImageWidthValue>(value)) {
            if (!widthValue->overridesBorderWidths() && isValueID(widthValue->widths(), CSSValueAuto))
                return true;
        }
        break;
    default:
        break;
    }
    return WTF::switchOn(initialValueForLonghand(longhand), [&](CSSValueID initialValue) {
        return isValueID(value, initialValue);
    }, [&](InitialNumericValue initialValue) {
        return isNumber(value, initialValue.number, initialValue.type);
    });
}

ASCIILiteral initialValueTextForLonghand(CSSPropertyID longhand)
{
    return WTF::switchOn(initialValueForLonghand(longhand), [](CSSValueID value) {
        return nameLiteral(value);
    }, [](InitialNumericValue initialValue) {
        switch (initialValue.type) {
        case CSSUnitType::CSS_NUMBER:
            if (initialValue.number == 0.0)
                return "0"_s;
            if (initialValue.number == 1.0)
                return "1"_s;
            if (initialValue.number == 2.0)
                return "2"_s;
            if (initialValue.number == 4.0)
                return "4"_s;
            if (initialValue.number == 8.0)
                return "8"_s;
            break;
        case CSSUnitType::CSS_PERCENTAGE:
            if (initialValue.number == 0.0)
                return "0%"_s;
            if (initialValue.number == 50.0)
                return "50%"_s;
            if (initialValue.number == 100.0)
                return "100%"_s;
            break;
        case CSSUnitType::CSS_PX:
            if (initialValue.number == 0.0)
                return "0px"_s;
            if (initialValue.number == 1.0)
                return "1px"_s;
            break;
        case CSSUnitType::CSS_S:
            if (initialValue.number == 0.0)
                return "0s"_s;
            break;
        default:
            break;
        }
        ASSERT_NOT_REACHED();
        return ""_s;
    });
}

CSSValueID initialValueIDForLonghand(CSSPropertyID longhand)
{
    return WTF::switchOn(initialValueForLonghand(longhand), [](CSSValueID value) {
        return value;
    }, [](InitialNumericValue) {
        return CSSValueInvalid;
    });
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

    for (size_t i = 0; i < shorthand.length(); ++i)
        addProperty(shorthandProperties[i], shorthand.id(), WTFMove(longhands[i]), important);
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
        flexBasis = CSSPrimitiveValue::create(CSSValueAuto);
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
    addProperty(CSSPropertyFlexGrow, CSSPropertyFlex, CSSPrimitiveValue::create(clampTo<float>(flexGrow)), important);
    addProperty(CSSPropertyFlexShrink, CSSPropertyFlex, CSSPrimitiveValue::create(clampTo<float>(flexShrink)), important);
    addProperty(CSSPropertyFlexBasis, CSSPropertyFlex, flexBasis.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumeBorderShorthand(CSSPropertyID widthProperty, CSSPropertyID styleProperty, CSSPropertyID colorProperty, bool important)
{
    RefPtr<CSSValue> width;
    RefPtr<CSSValue> style;
    RefPtr<CSSValue> color;
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

    if (!m_range.atEnd())
        return false;

    addExpandedProperty(widthProperty, WTFMove(width), important);
    addExpandedProperty(styleProperty, WTFMove(style), important);
    addExpandedProperty(colorProperty, WTFMove(color), important);
    return true;
}

bool CSSPropertyParser::consume2ValueShorthand(const StylePropertyShorthand& shorthand, bool important)
{
    ASSERT(shorthand.length() == 2);
    const CSSPropertyID* longhands = shorthand.properties();
    RefPtr start = parseSingleValue(longhands[0], shorthand.id());
    if (!start)
        return false;

    RefPtr end = parseSingleValue(longhands[1], shorthand.id());
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
    RefPtr top = parseSingleValue(longhands[0], shorthand.id());
    if (!top)
        return false;

    RefPtr right = parseSingleValue(longhands[1], shorthand.id());
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
    if (!consumeBorderImageComponents(property, m_range, m_context, source, slice, width, outset, repeat))
        return false;
    if (property == CSSPropertyMaskBorder || property == CSSPropertyWebkitMaskBoxImage) {
        addProperty(CSSPropertyMaskBorderSource, property, WTFMove(source), important);
        addProperty(CSSPropertyMaskBorderSlice, property, WTFMove(slice), important);
        addProperty(CSSPropertyMaskBorderWidth, property, WTFMove(width), important);
        addProperty(CSSPropertyMaskBorderOutset, property, WTFMove(outset), important);
        addProperty(CSSPropertyMaskBorderRepeat, property, WTFMove(repeat), important);
    } else {
        addProperty(CSSPropertyBorderImageSource, property, WTFMove(source), important);
        addProperty(CSSPropertyBorderImageSlice, property, WTFMove(slice), important);
        addProperty(CSSPropertyBorderImageWidth, property, WTFMove(width), important);
        addProperty(CSSPropertyBorderImageOutset, property, WTFMove(outset), important);
        addProperty(CSSPropertyBorderImageRepeat, property, WTFMove(repeat), important);
    }
    return true;
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
    RefPtr keyword = consumeIdent(m_range);
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
    addProperty(genericBreakProperty, property, CSSPrimitiveValue::create(value), important);
    return true;
}

bool CSSPropertyParser::consumeLegacyTextOrientation(bool important)
{
    // -webkit-text-orientation is a legacy shorthand for text-orientation.
    // The only difference is that it accepts 'sideways-right', which is mapped into 'sideways'.
    RefPtr<CSSPrimitiveValue> keyword;
    auto valueID = m_range.peek().id();
    if (valueID == CSSValueSidewaysRight) {
        keyword = CSSPrimitiveValue::create(CSSValueSideways);
        consumeIdentRaw(m_range);
    } else if (CSSParserFastPaths::isKeywordValidForStyleProperty(CSSPropertyTextOrientation, valueID, m_context))
        keyword = consumeIdent(m_range);
    if (!keyword || !m_range.atEnd())
        return false;
    addProperty(CSSPropertyTextOrientation, CSSPropertyWebkitTextOrientation, keyword.releaseNonNull(), important);
    return true;
}

static bool isValidAnimationPropertyList(CSSPropertyID property, const CSSValueListBuilder& valueList)
{
    // If there is more than one <single-transition> in the shorthand, and any of the transitions
    // has none as the <single-transition-property>, then the declaration is invalid.
    if (property != CSSPropertyTransitionProperty || valueList.size() < 2)
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
    case CSSPropertyAnimationTimeline:
        return consumeAnimationTimeline(range, context);
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
        return consumeTimingFunction(range, context);
    case CSSPropertyTransitionBehavior:
        return CSSPropertyParsing::consumeTransitionBehaviorValue(range);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

bool CSSPropertyParser::consumeAnimationShorthand(const StylePropertyShorthand& shorthand, bool important)
{
    const unsigned longhandCount = shorthand.length();
    CSSValueListBuilder longhands[8];
    ASSERT(longhandCount <= 8);

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
                    longhands[i].append(*value);
                    break;
                }
            }
            if (!foundProperty)
                return false;
        } while (!m_range.atEnd() && m_range.peek().type() != CommaToken);

        for (size_t i = 0; i < longhandCount; ++i) {
            if (!parsedLonghand[i])
                longhands[i].append(Ref { CSSPrimitiveValue::implicitInitialValue() });
            parsedLonghand[i] = false;
        }
    } while (consumeCommaIncludingWhitespace(m_range));

    for (size_t i = 0; i < longhandCount; ++i) {
        if (!isValidAnimationPropertyList(shorthand.properties()[i], longhands[i]))
            return false;
    }

    for (size_t i = 0; i < longhandCount; ++i)
        addProperty(shorthand.properties()[i], shorthand.id(), CSSValueList::createCommaSeparated(WTFMove(longhands[i])), important);

    return m_range.atEnd();
}

static bool consumeBackgroundPosition(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, RefPtr<CSSValue>& resultX, RefPtr<CSSValue>& resultY)
{
    CSSValueListBuilder x;
    CSSValueListBuilder y;
    do {
        auto position = consumePositionCoordinates(range, context.mode, UnitlessQuirk::Allow, property == CSSPropertyMaskPosition ? PositionSyntax::Position : PositionSyntax::BackgroundPosition, NegativePercentagePolicy::Allow);
        if (!position)
            return false;
        x.append(WTFMove(position->x));
        y.append(WTFMove(position->y));
    } while (consumeCommaIncludingWhitespace(range));
    if (x.size() == 1) {
        resultX = WTFMove(x[0]);
        resultY = WTFMove(y[0]);
    } else {
        resultX = CSSValueList::createCommaSeparated(WTFMove(x));
        resultY = CSSValueList::createCommaSeparated(WTFMove(y));
    }
    return true;
}

static RefPtr<CSSValue> consumeBackgroundComponent(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context)
{
    switch (property) {
    // background-*
    case CSSPropertyBackgroundClip:
        return CSSPropertyParsing::consumeSingleBackgroundClip(range);
    case CSSPropertyBackgroundBlendMode:
        return CSSPropertyParsing::consumeSingleBackgroundBlendMode(range);
    case CSSPropertyBackgroundAttachment:
        return CSSPropertyParsing::consumeSingleBackgroundAttachment(range);
    case CSSPropertyBackgroundOrigin:
        return CSSPropertyParsing::consumeSingleBackgroundOrigin(range);
    case CSSPropertyBackgroundImage:
        return CSSPropertyParsing::consumeSingleBackgroundImage(range, context);
    case CSSPropertyBackgroundRepeat:
        return CSSPropertyParsing::consumeSingleBackgroundRepeat(range, context);
    case CSSPropertyBackgroundPositionX:
        return CSSPropertyParsing::consumeSingleBackgroundPositionX(range, context);
    case CSSPropertyBackgroundPositionY:
        return CSSPropertyParsing::consumeSingleBackgroundPositionY(range, context);
    case CSSPropertyBackgroundSize:
        return consumeSingleBackgroundSize(range, context);
    case CSSPropertyBackgroundColor:
        return consumeColor(range, context);

    // mask-*
    case CSSPropertyMaskComposite:
        return CSSPropertyParsing::consumeSingleMaskComposite(range);
    case CSSPropertyMaskOrigin:
        return CSSPropertyParsing::consumeSingleMaskOrigin(range);
    case CSSPropertyMaskClip:
        return CSSPropertyParsing::consumeSingleMaskClip(range);
    case CSSPropertyMaskImage:
        return CSSPropertyParsing::consumeSingleMaskImage(range, context);
    case CSSPropertyMaskMode:
        return CSSPropertyParsing::consumeSingleMaskMode(range);
    case CSSPropertyMaskRepeat:
        return CSSPropertyParsing::consumeSingleMaskRepeat(range, context);
    case CSSPropertyMaskSize:
        return consumeSingleMaskSize(range, context);

    // -webkit-background-*
    case CSSPropertyWebkitBackgroundSize:
        return consumeSingleWebkitBackgroundSize(range, context);
    case CSSPropertyWebkitBackgroundClip:
        return CSSPropertyParsing::consumeSingleWebkitBackgroundClip(range);
    case CSSPropertyWebkitBackgroundOrigin:
        return CSSPropertyParsing::consumeSingleWebkitBackgroundOrigin(range);

    // -webkit-mask-*
    case CSSPropertyWebkitMaskClip:
        return CSSPropertyParsing::consumeSingleWebkitMaskClip(range);
    case CSSPropertyWebkitMaskComposite:
        return CSSPropertyParsing::consumeSingleWebkitMaskComposite(range);
    case CSSPropertyWebkitMaskSourceType:
        return CSSPropertyParsing::consumeSingleWebkitMaskSourceType(range);
    case CSSPropertyWebkitMaskPositionX:
        return CSSPropertyParsing::consumeSingleWebkitMaskPositionX(range, context);
    case CSSPropertyWebkitMaskPositionY:
        return CSSPropertyParsing::consumeSingleWebkitMaskPositionY(range, context);

    default:
        return nullptr;
    };
}

// Note: consumeBackgroundShorthand assumes y properties (for example background-position-y) follow
// the x properties in the shorthand array.
bool CSSPropertyParser::consumeBackgroundShorthand(const StylePropertyShorthand& shorthand, bool important)
{
    unsigned longhandCount = shorthand.length();

    // mask resets mask-border properties outside of this method.
    if (shorthand.id() == CSSPropertyMask)
        longhandCount -= maskBorderShorthand().length();

    CSSValueListBuilder longhands[10];
    ASSERT(longhandCount <= 10);

    do {
        bool parsedLonghand[10] = { false };
        bool lastParsedWasPosition = false;
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
                    if (!lastParsedWasPosition)
                        return false;
                    value = consumeSingleBackgroundSize(m_range, m_context);
                    if (!value)
                        return false;
                } else if (property == CSSPropertyMaskSize) {
                    if (!consumeSlashIncludingWhitespace(m_range))
                        continue;
                    if (!lastParsedWasPosition)
                        return false;
                    value = consumeSingleMaskSize(m_range, m_context);
                    if (!value)
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
                    longhands[i].append(value.releaseNonNull());
                    lastParsedWasPosition = valueY;
                    if (valueY) {
                        parsedLonghand[i + 1] = true;
                        longhands[i + 1].append(valueY.releaseNonNull());
                    }
                }
            }
            if (!foundProperty)
                return false;
        } while (!m_range.atEnd() && m_range.peek().type() != CommaToken);

        for (size_t i = 0; i < longhandCount; ++i) {
            CSSPropertyID property = shorthand.properties()[i];
            if (property == CSSPropertyBackgroundColor && !m_range.atEnd()) {
                if (parsedLonghand[i])
                    return false; // Colors are only allowed in the last layer.
                continue;
            }
            if ((property == CSSPropertyBackgroundClip || property == CSSPropertyMaskClip || property == CSSPropertyWebkitMaskClip) && !parsedLonghand[i] && originValue) {
                longhands[i].append(originValue.releaseNonNull());
                continue;
            }
            if (!parsedLonghand[i])
                longhands[i].append(Ref { CSSPrimitiveValue::implicitInitialValue() });
        }
    } while (consumeCommaIncludingWhitespace(m_range));
    if (!m_range.atEnd())
        return false;

    for (size_t i = 0; i < longhandCount; ++i) {
        CSSPropertyID property = shorthand.properties()[i];
        if (property == CSSPropertyBackgroundSize && !longhands[i].isEmpty() && m_context.useLegacyBackgroundSizeShorthandBehavior)
            continue;
        if (longhands[i].size() == 1)
            addProperty(property, shorthand.id(), WTFMove(longhands[i][0]), important);
        else
            addProperty(property, shorthand.id(), CSSValueList::createCommaSeparated(WTFMove(longhands[i])), important);
    }
    return true;
}

bool CSSPropertyParser::consumeOverflowShorthand(bool important)
{
    CSSValueID xValueID = m_range.consumeIncludingWhitespace().id();
    if (!CSSParserFastPaths::isKeywordValidForStyleProperty(CSSPropertyOverflowY, xValueID, m_context))
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

    if (!CSSParserFastPaths::isKeywordValidForStyleProperty(CSSPropertyOverflowY, yValueID, m_context))
        return false;
    if (!m_range.atEnd())
        return false;

    addProperty(CSSPropertyOverflowX, CSSPropertyOverflow, CSSPrimitiveValue::create(xValueID), important);
    addProperty(CSSPropertyOverflowY, CSSPropertyOverflow, CSSPrimitiveValue::create(yValueID), important);
    return true;
}

static bool isCustomIdentValue(const CSSValue& value)
{
    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    return primitiveValue && primitiveValue->isCustomIdent();
}

bool CSSPropertyParser::consumeGridItemPositionShorthand(CSSPropertyID shorthandId, bool important)
{
    const StylePropertyShorthand& shorthand = shorthandForProperty(shorthandId);
    ASSERT(shorthand.length() == 2);
    RefPtr startValue = consumeGridLine(m_range);
    if (!startValue)
        return false;

    RefPtr<CSSValue> endValue;
    if (consumeSlashIncludingWhitespace(m_range)) {
        endValue = consumeGridLine(m_range);
        if (!endValue)
            return false;
    } else {
        endValue = isCustomIdentValue(*startValue) ? startValue : CSSPrimitiveValue::create(CSSValueAuto);
    }
    if (!m_range.atEnd())
        return false;
    addProperty(shorthand.properties()[0], shorthandId, startValue.releaseNonNull(), important);
    addProperty(shorthand.properties()[1], shorthandId, endValue.releaseNonNull(), important);
    return true;
}

bool CSSPropertyParser::consumeGridAreaShorthand(bool important)
{
    RefPtr rowStartValue = consumeGridLine(m_range);
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
        columnStartValue = isCustomIdentValue(*rowStartValue) ? rowStartValue : CSSPrimitiveValue::create(CSSValueAuto);
    if (!rowEndValue)
        rowEndValue = isCustomIdentValue(*rowStartValue) ? rowStartValue : CSSPrimitiveValue::create(CSSValueAuto);
    if (!columnEndValue)
        columnEndValue = isCustomIdentValue(*columnStartValue) ? columnStartValue : CSSPrimitiveValue::create(CSSValueAuto);

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
    CSSValueListBuilder templateRows;

    // Persists between loop iterations so we can use the same value for
    // consecutive <line-names> values
    RefPtr<CSSGridLineNamesValue> lineNames;

    do {
        // Handle leading <custom-ident>*.
        auto previousLineNames = std::exchange(lineNames, consumeGridLineNames(m_range));
        if (lineNames) {
            if (!previousLineNames)
                templateRows.append(lineNames.releaseNonNull());
            else {
                Vector<String> combinedLineNames;
                combinedLineNames.append(previousLineNames->names());
                combinedLineNames.append(lineNames->names());
                templateRows.last() = CSSGridLineNamesValue::create(combinedLineNames);
            }
        }

        // Handle a template-area's row.
        if (m_range.peek().type() != StringToken || !parseGridTemplateAreasRow(m_range.consumeIncludingWhitespace().value(), gridAreaMap, rowCount, columnCount))
            return false;
        ++rowCount;

        // Handle template-rows's track-size.
        if (RefPtr value = consumeGridTrackSize(m_range, m_context.mode))
            templateRows.append(value.releaseNonNull());
        else
            templateRows.append(CSSPrimitiveValue::create(CSSValueAuto));

        // This will handle the trailing/leading <custom-ident>* in the grammar.
        lineNames = consumeGridLineNames(m_range);
        if (lineNames)
            templateRows.append(*lineNames);
    } while (!m_range.atEnd() && !(m_range.peek().type() == DelimiterToken && m_range.peek().delimiter() == '/'));

    RefPtr<CSSValue> columnsValue;
    if (!m_range.atEnd()) {
        if (!consumeSlashIncludingWhitespace(m_range))
            return false;
        columnsValue = consumeGridTrackList(m_range, m_context, GridTemplateNoRepeat);
        if (!columnsValue || !m_range.atEnd())
            return false;
    } else {
        columnsValue = CSSPrimitiveValue::create(CSSValueNone);
    }
    addProperty(CSSPropertyGridTemplateRows, shorthandId, CSSValueList::createSpaceSeparated(WTFMove(templateRows)), important);
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
        addProperty(CSSPropertyGridTemplateRows, shorthandId, CSSPrimitiveValue::create(CSSValueNone), important);
        addProperty(CSSPropertyGridTemplateColumns, shorthandId, CSSPrimitiveValue::create(CSSValueNone), important);
        addProperty(CSSPropertyGridTemplateAreas, shorthandId, CSSPrimitiveValue::create(CSSValueNone), important);
        return true;
    }

    // 2- <grid-template-rows> / <grid-template-columns>
    if (!rowsValue)
        rowsValue = consumeGridTrackList(m_range, m_context, GridTemplate);

    if (rowsValue) {
        if (!consumeSlashIncludingWhitespace(m_range))
            return false;
        RefPtr columnsValue = consumeGridTemplatesRowsOrColumns(m_range, m_context);
        if (!columnsValue || !m_range.atEnd())
            return false;

        addProperty(CSSPropertyGridTemplateRows, shorthandId, rowsValue.releaseNonNull(), important);
        addProperty(CSSPropertyGridTemplateColumns, shorthandId, columnsValue.releaseNonNull(), important);
        addProperty(CSSPropertyGridTemplateAreas, shorthandId, CSSPrimitiveValue::create(CSSValueNone), important);
        return true;
    }

    // 3- [ <line-names>? <string> <track-size>? <line-names>? ]+ [ / <track-list> ]?
    m_range = rangeCopy;
    return consumeGridTemplateRowsAndAreasAndColumns(shorthandId, important);
}

static RefPtr<CSSValue> consumeImplicitGridAutoFlow(CSSParserTokenRange& range, CSSValueID flowDirection)
{
    // [ auto-flow && dense? ]
    bool autoFlow = consumeIdentRaw<CSSValueAutoFlow>(range).has_value();
    bool dense = consumeIdentRaw<CSSValueDense>(range).has_value();
    if (!autoFlow && (!dense || !consumeIdentRaw<CSSValueAutoFlow>(range)))
        return nullptr;

    if (!dense)
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(flowDirection));
    if (flowDirection == CSSValueRow)
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueDense));
    return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(flowDirection),
        CSSPrimitiveValue::create(CSSValueDense));
}

bool CSSPropertyParser::consumeGridShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyGrid).length() == 6);

    CSSParserTokenRange rangeCopy = m_range;

    // 1- <grid-template>
    if (consumeGridTemplateShorthand(CSSPropertyGrid, important)) {
        // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
        // The sub-properties not specified are set to their initial value, as normal for shorthands.
        addProperty(CSSPropertyGridAutoFlow, CSSPropertyGrid, CSSPrimitiveValue::create(CSSValueRow), important);
        addProperty(CSSPropertyGridAutoColumns, CSSPropertyGrid, CSSPrimitiveValue::create(CSSValueAuto), important);
        addProperty(CSSPropertyGridAutoRows, CSSPropertyGrid, CSSPrimitiveValue::create(CSSValueAuto), important);

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
        gridAutoFlow = consumeImplicitGridAutoFlow(m_range, CSSValueRow);
        if (!gridAutoFlow || m_range.atEnd())
            return false;
        if (consumeSlashIncludingWhitespace(m_range))
            autoRowsValue = CSSPrimitiveValue::create(CSSValueAuto);
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
        templateRows = CSSPrimitiveValue::create(CSSValueNone);
        autoColumnsValue = CSSPrimitiveValue::create(CSSValueAuto);
    } else {
        // 3- <grid-template-rows> / [ auto-flow && dense? ] <grid-auto-columns>?
        templateRows = consumeGridTemplatesRowsOrColumns(m_range, m_context.mode);
        if (!templateRows)
            return false;
        if (!consumeSlashIncludingWhitespace(m_range) || m_range.atEnd())
            return false;
        gridAutoFlow = consumeImplicitGridAutoFlow(m_range, CSSValueColumn);
        if (!gridAutoFlow)
            return false;
        if (m_range.atEnd())
            autoColumnsValue = CSSPrimitiveValue::create(CSSValueAuto);
        else {
            autoColumnsValue = consumeGridTrackList(m_range, m_context, GridAuto);
            if (!autoColumnsValue)
                return false;
        }
        templateColumns = CSSPrimitiveValue::create(CSSValueNone);
        autoRowsValue = CSSPrimitiveValue::create(CSSValueAuto);
    }
    
    if (!m_range.atEnd())
        return false;
    
    // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
    // The sub-properties not specified are set to their initial value, as normal for shorthands.
    addProperty(CSSPropertyGridTemplateColumns, CSSPropertyGrid, templateColumns.releaseNonNull(), important);
    addProperty(CSSPropertyGridTemplateRows, CSSPropertyGrid, templateRows.releaseNonNull(), important);
    addProperty(CSSPropertyGridTemplateAreas, CSSPropertyGrid, CSSPrimitiveValue::create(CSSValueNone), important);
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
    RefPtr alignContentValue = consumeContentDistributionOverflowPosition(m_range, isContentPositionKeyword);
    if (!alignContentValue)
        return false;

    // justify-content property does not allow the <baseline-position> values.
    if (m_range.atEnd() && isBaseline)
        return false;
    if (isBaselineKeyword(m_range.peek().id()))
        return false;

    if (m_range.atEnd())
        m_range = rangeCopy;
    RefPtr justifyContentValue = consumeContentDistributionOverflowPosition(m_range, isContentPositionOrLeftOrRightKeyword);
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
    RefPtr alignItemsValue = consumeAlignItems(m_range);
    if (!alignItemsValue)
        return false;

    if (m_range.atEnd())
        m_range = rangeCopy;
    RefPtr justifyItemsValue = consumeJustifyItems(m_range);
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
    RefPtr alignSelfValue = consumeSelfPositionOverflowPosition(m_range, isSelfPositionKeyword);
    if (!alignSelfValue)
        return false;

    if (m_range.atEnd())
        m_range = rangeCopy;
    RefPtr justifySelfValue = consumeSelfPositionOverflowPosition(m_range, isSelfPositionOrLeftOrRightKeyword);
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

    RefPtr overscrollBehaviorX = CSSPropertyParsing::consumeOverscrollBehaviorX(m_range);
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

    addProperty(CSSPropertyOverscrollBehaviorX, CSSPropertyOverscrollBehavior, WTFMove(overscrollBehaviorX), important);
    addProperty(CSSPropertyOverscrollBehaviorY, CSSPropertyOverscrollBehavior, WTFMove(overscrollBehaviorY), important);
    return true;
}

bool CSSPropertyParser::consumeContainerShorthand(bool important)
{
    RefPtr name = consumeContainerName(m_range);
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
    addProperty(CSSPropertyContainerType, CSSPropertyContainer, WTFMove(type), important);
    return true;
}

bool CSSPropertyParser::consumeContainIntrinsicSizeShorthand(bool important)
{
    ASSERT(shorthandForProperty(CSSPropertyContainIntrinsicSize).length() == 2);
    ASSERT(isExposed(CSSPropertyContainIntrinsicSize, &m_context.propertySettings));

    if (m_range.atEnd())
        return false;

    RefPtr containIntrinsicWidth = consumeContainIntrinsicSize(m_range);
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

    addProperty(CSSPropertyContainIntrinsicWidth, CSSPropertyContainIntrinsicSize, WTFMove(containIntrinsicWidth), important);
    addProperty(CSSPropertyContainIntrinsicHeight, CSSPropertyContainIntrinsicSize, WTFMove(containIntrinsicHeight), important);
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
        addProperty(CSSPropertyTransformOriginZ, CSSPropertyTransformOrigin, resultZ, important);
        
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
    if (RefPtr value = CSSPropertyParsing::consumePerspective(m_range, m_context)) {
        addProperty(CSSPropertyPerspective, CSSPropertyWebkitPerspective, value.releaseNonNull(), important);
        return m_range.atEnd();
    }

    if (auto perspective = consumeNumberRaw(m_range)) {
        if (perspective->value < 0)
            return false;
        Ref value = CSSPrimitiveValue::create(perspective->value, CSSUnitType::CSS_PX);
        addProperty(CSSPropertyPerspective, CSSPropertyWebkitPerspective, WTFMove(value), important);
        return m_range.atEnd();
    }

    return false;
}

bool CSSPropertyParser::consumeOffset(bool important)
{
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

    addProperty(CSSPropertyOffsetPath, CSSPropertyOffset, WTFMove(offsetPath), important);
    addProperty(CSSPropertyOffsetDistance, CSSPropertyOffset, WTFMove(offsetDistance), important);
    addProperty(CSSPropertyOffsetPosition, CSSPropertyOffset, WTFMove(offsetPosition), important);
    addProperty(CSSPropertyOffsetAnchor, CSSPropertyOffset, WTFMove(offsetAnchor), important);
    addProperty(CSSPropertyOffsetRotate, CSSPropertyOffset, WTFMove(offsetRotate), important);

    return m_range.atEnd();
}

bool CSSPropertyParser::consumeListStyleShorthand(bool important)
{
    RefPtr<CSSValue> position;
    RefPtr<CSSValue> image;
    RefPtr<CSSValue> type;
    unsigned noneCount = 0;

    while (!m_range.atEnd()) {
        if (m_range.peek().id() == CSSValueNone) {
            ++noneCount;
            consumeIdent(m_range);
            continue;
        }
        if (!position && (position = parseSingleValue(CSSPropertyListStylePosition, CSSPropertyListStyle)))
            continue;

        if (!image && (image = parseSingleValue(CSSPropertyListStyleImage, CSSPropertyListStyle)))
            continue;

        if (!type && (type = parseSingleValue(CSSPropertyListStyleType, CSSPropertyListStyle)))
            continue;

        return false;
    }

    if (noneCount > (static_cast<unsigned>(!image + !type)))
        return false;

    if (noneCount == 2) {
        // Using implicit none for list-style-image is how we serialize "none" instead of "none none".
        image = nullptr;
        type = CSSPrimitiveValue::create(CSSValueNone);
    } else if (noneCount == 1) {
        // Use implicit none for list-style-image, but non-implicit for type.
        if (!type)
            type = CSSPrimitiveValue::create(CSSValueNone);
    }

    addProperty(CSSPropertyListStylePosition, CSSPropertyListStyle, WTFMove(position), important);
    addProperty(CSSPropertyListStyleImage, CSSPropertyListStyle, WTFMove(image), important);
    addProperty(CSSPropertyListStyleType, CSSPropertyListStyle, WTFMove(type), important);
    return m_range.atEnd();
}

bool CSSPropertyParser::consumeTextWrapShorthand(bool important)
{
    RefPtr<CSSValue> mode;
    RefPtr<CSSValue> style;

    for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !m_range.atEnd(); ++propertiesParsed) {
        if (!mode && (mode = CSSPropertyParsing::consumeTextWrapMode(m_range)))
            continue;
        if (m_context.propertySettings.cssTextWrapStyleEnabled && !style && (style = CSSPropertyParsing::consumeTextWrapStyle(m_range, m_context)))
            continue;
        // If we didn't find at least one match, this is an invalid shorthand and we have to ignore it.
        return false;
    }

    if (!m_range.atEnd())
        return false;

    // Fill in default values if one was missing from the multi-value syntax.
    if (!mode)
        mode = CSSPrimitiveValue::create(CSSValueWrap);
    if (!style)
        style = CSSPrimitiveValue::create(CSSValueAuto);

    addProperty(CSSPropertyTextWrapMode, CSSPropertyTextWrap, WTFMove(mode), important);
    addProperty(CSSPropertyTextWrapStyle, CSSPropertyTextWrap, WTFMove(style), important);
    return true;
}


bool CSSPropertyParser::consumeWhiteSpaceShorthand(bool important)
{
    RefPtr<CSSValue> whiteSpaceCollapse;
    RefPtr<CSSValue> textWrapMode;

    // Single value syntax.
    auto singleValueKeyword = consumeIdentRaw<
        CSSValueNormal,
        CSSValuePre,
        CSSValuePreLine,
        CSSValuePreWrap
    >(m_range);

    if (singleValueKeyword) {
        switch (*singleValueKeyword) {
        case CSSValueNormal:
            whiteSpaceCollapse = CSSPrimitiveValue::create(CSSValueCollapse);
            textWrapMode = CSSPrimitiveValue::create(CSSValueWrap);
            break;
        case CSSValuePre:
            whiteSpaceCollapse = CSSPrimitiveValue::create(CSSValuePreserve);
            textWrapMode = CSSPrimitiveValue::create(CSSValueNowrap);
            break;
        case CSSValuePreLine:
            whiteSpaceCollapse = CSSPrimitiveValue::create(CSSValuePreserveBreaks);
            textWrapMode = CSSPrimitiveValue::create(CSSValueWrap);
            break;
        case CSSValuePreWrap:
            whiteSpaceCollapse = CSSPrimitiveValue::create(CSSValuePreserve);
            textWrapMode = CSSPrimitiveValue::create(CSSValueWrap);
            break;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    } else {
        // Multi-value syntax.
        for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !m_range.atEnd(); ++propertiesParsed) {
            if (!whiteSpaceCollapse && (whiteSpaceCollapse = CSSPropertyParsing::consumeWhiteSpaceCollapse(m_range)))
                continue;
            if (!textWrapMode && (textWrapMode = CSSPropertyParsing::consumeTextWrapMode(m_range)))
                continue;
            // If we didn't find at least one match, this is an invalid shorthand and we have to ignore it.
            return false;
        }
    }

    if (!m_range.atEnd())
        return false;

    // Fill in default values if one was missing from the multi-value syntax.
    if (!whiteSpaceCollapse)
        whiteSpaceCollapse = CSSPrimitiveValue::create(CSSValueCollapse);
    if (!textWrapMode)
        textWrapMode = CSSPrimitiveValue::create(CSSValueWrap);

    addProperty(CSSPropertyWhiteSpaceCollapse, CSSPropertyWhiteSpace, WTFMove(whiteSpaceCollapse), important);
    addProperty(CSSPropertyTextWrapMode, CSSPropertyWhiteSpace, WTFMove(textWrapMode), important);
    return true;
}

bool CSSPropertyParser::consumeScrollTimelineShorthand(bool important)
{
    CSSValueListBuilder namesList;
    CSSValueListBuilder axesList;

    do {
        // A valid scroll-timeline-name is required.
        if (RefPtr name = CSSPropertyParsing::consumeSingleScrollTimelineName(m_range))
            namesList.append(name.releaseNonNull());
        else
            return false;

        // A scroll-timeline-axis is optional.
        if (m_range.peek().type() == CommaToken || m_range.atEnd())
            axesList.append(CSSPrimitiveValue::create(CSSValueBlock));
        else if (auto axis = CSSPropertyParsing::consumeAxis(m_range))
            axesList.append(axis.releaseNonNull());
        else
            return false;
    } while (consumeCommaIncludingWhitespace(m_range));

    if (namesList.isEmpty())
        return false;

    addProperty(CSSPropertyScrollTimelineName, CSSPropertyScrollTimeline, CSSValueList::createCommaSeparated(WTFMove(namesList)), important);
    if (!axesList.isEmpty())
        addProperty(CSSPropertyScrollTimelineAxis, CSSPropertyScrollTimeline, CSSValueList::createCommaSeparated(WTFMove(axesList)), important);
    return true;
}

bool CSSPropertyParser::consumeViewTimelineShorthand(bool important)
{
    CSSValueListBuilder namesList;
    CSSValueListBuilder axesList;
    CSSValueListBuilder insetsList;

    auto defaultAxis = []() -> Ref<CSSValue> { return CSSPrimitiveValue::create(CSSValueBlock); };
    auto defaultInsets = []() -> Ref<CSSValue> { return CSSPrimitiveValue::create(CSSValueAuto); };

    do {
        // A valid view-timeline-name is required.
        if (RefPtr name = CSSPropertyParsing::consumeSingleScrollTimelineName(m_range))
            namesList.append(name.releaseNonNull());
        else
            return false;

        // Both a view-timeline-axis and a view-timeline-inset are optional.
        if (m_range.peek().type() != CommaToken && !m_range.atEnd()) {
            RefPtr axis = CSSPropertyParsing::consumeAxis(m_range);
            RefPtr insets = consumeViewTimelineInsetListItem(m_range, m_context);
            // Since the order of view-timeline-axis and view-timeline-inset is not guaranteed, let's try view-timeline-axis again.
            if (!axis)
                axis = CSSPropertyParsing::consumeAxis(m_range);
            if (!axis && !insets)
                return false;
            axesList.append(axis ? axis.releaseNonNull() : defaultAxis());
            insetsList.append(insets ? insets.releaseNonNull() : defaultInsets());
        } else {
            axesList.append(defaultAxis());
            insetsList.append(defaultInsets());
        }
    } while (consumeCommaIncludingWhitespace(m_range));

    if (namesList.isEmpty())
        return false;

    addProperty(CSSPropertyViewTimelineName, CSSPropertyViewTimeline, CSSValueList::createCommaSeparated(WTFMove(namesList)), important);
    addProperty(CSSPropertyViewTimelineAxis, CSSPropertyViewTimeline, CSSValueList::createCommaSeparated(WTFMove(axesList)), important);
    addProperty(CSSPropertyViewTimelineInset, CSSPropertyViewTimeline, CSSValueList::createCommaSeparated(WTFMove(insetsList)), important);
    return true;
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
        return consumeAnimationShorthand(animationShorthand(), important);
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
    case CSSPropertyBorderInline:
        return consumeBorderShorthand(CSSPropertyBorderInlineWidth, CSSPropertyBorderInlineStyle, CSSPropertyBorderInlineColor, important);
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
    case CSSPropertyBorderBlock:
        return consumeBorderShorthand(CSSPropertyBorderBlockWidth, CSSPropertyBorderBlockStyle, CSSPropertyBorderBlockColor, important);
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
        RefPtr marker = parseSingleValue(CSSPropertyMarkerStart);
        if (!marker || !m_range.atEnd())
            return false;
        Ref markerRef = marker.releaseNonNull();
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
        std::array<RefPtr<CSSValue>, 4> horizontalRadii;
        std::array<RefPtr<CSSValue>, 4> verticalRadii;
        if (!consumeRadii(horizontalRadii, verticalRadii, m_range, m_context.mode, property == CSSPropertyWebkitBorderRadius))
            return false;
        addProperty(CSSPropertyBorderTopLeftRadius, CSSPropertyBorderRadius, CSSValuePair::create(horizontalRadii[0].releaseNonNull(), verticalRadii[0].releaseNonNull()), important);
        addProperty(CSSPropertyBorderTopRightRadius, CSSPropertyBorderRadius, CSSValuePair::create(horizontalRadii[1].releaseNonNull(), verticalRadii[1].releaseNonNull()), important);
        addProperty(CSSPropertyBorderBottomRightRadius, CSSPropertyBorderRadius, CSSValuePair::create(horizontalRadii[2].releaseNonNull(), verticalRadii[2].releaseNonNull()), important);
        addProperty(CSSPropertyBorderBottomLeftRadius, CSSPropertyBorderRadius, CSSValuePair::create(horizontalRadii[3].releaseNonNull(), verticalRadii[3].releaseNonNull()), important);
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
        if (!consumeBorderShorthand(CSSPropertyBorderWidth, CSSPropertyBorderStyle, CSSPropertyBorderColor, important))
            return false;
        for (auto longhand : borderImageShorthand())
            addProperty(longhand, CSSPropertyBorder, nullptr, important);
        return true;
    }
    case CSSPropertyBorderImage:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitMaskBoxImage:
    case CSSPropertyMaskBorder:
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
        auto backgroundSize = consumeCommaSeparatedListWithSingleValueOptimization(m_range, [] (auto& range, auto& context) {
            return consumeSingleWebkitBackgroundSize(range, context);
        }, m_context);
        if (!backgroundSize || !m_range.atEnd())
            return false;
        addProperty(CSSPropertyBackgroundSize, CSSPropertyWebkitBackgroundSize, backgroundSize.releaseNonNull(), important);
        return true;
    }
    case CSSPropertyMask:
        if (!consumeBackgroundShorthand(shorthandForProperty(property), important))
            return false;
        for (auto longhand : maskBorderShorthand())
            addProperty(longhand, CSSPropertyMask, nullptr, important);
        return true;
    case CSSPropertyWebkitMask:
        return consumeBackgroundShorthand(shorthandForProperty(property), important);
    case CSSPropertyTransformOrigin:
        return consumeTransformOrigin(important);
    case CSSPropertyPerspectiveOrigin:
        return consumePerspectiveOrigin(important);
    case CSSPropertyWebkitPerspective:
        return consumePrefixedPerspective(important);
    case CSSPropertyGap: {
        RefPtr rowGap = CSSPropertyParsing::consumeRowGap(m_range, m_context);
        RefPtr columnGap = CSSPropertyParsing::consumeColumnGap(m_range, m_context);
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
    case CSSPropertyScrollTimeline:
        return consumeScrollTimelineShorthand(important);
    case CSSPropertyTextWrap:
        return consumeTextWrapShorthand(important);
    case CSSPropertyViewTimeline:
        return consumeViewTimelineShorthand(important);
    case CSSPropertyWhiteSpace:
        return consumeWhiteSpaceShorthand(important);
    default:
        return false;
    }
}

} // namespace WebCore
