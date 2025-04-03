// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2022 Apple Inc. All rights reserved.
// Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFontStyleRangeValue.h"
#include "CSSFontVariantLigaturesParser.h"
#include "CSSFontVariantNumericParser.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSMarkup.h"
#include "CSSOffsetRotateValue.h"
#include "CSSParserFastPaths.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRangeGuard.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPrimitiveNumericTypes+CSSValueCreation.h"
#include "CSSPropertyParserConsumer+Align.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+AppleVisualEffect.h"
#include "CSSPropertyParserConsumer+Background.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSPropertyParserConsumer+Easing.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSPropertyParserConsumer+Grid.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Image.h"
#include "CSSPropertyParserConsumer+Inline.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Position.h"
#include "CSSPropertyParserConsumer+ResolutionDefinitions.h"
#include "CSSPropertyParserConsumer+String.h"
#include "CSSPropertyParserConsumer+TextDecoration.h"
#include "CSSPropertyParserConsumer+TimeDefinitions.h"
#include "CSSPropertyParserConsumer+Timeline.h"
#include "CSSPropertyParserConsumer+Transform.h"
#include "CSSPropertyParserConsumer+Transitions.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParsing.h"
#include "CSSQuadValue.h"
#include "CSSTokenizer.h"
#include "CSSTransformListValue.h"
#include "CSSURLValue.h"
#include "CSSValuePair.h"
#include "CSSVariableParser.h"
#include "CSSVariableReferenceValue.h"
#include "ComputedStyleDependencies.h"
#include "FontFace.h"
#include "Rect.h"
#include "StyleBuilder.h"
#include "StyleBuilderConverter.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"
#include "StyleURL.h"
#include "TimingFunction.h"
#include "TransformOperationsBuilder.h"
#include <memory>
#include <wtf/IndexedRange.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ZippedRange.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

bool isCustomPropertyName(StringView propertyName)
{
    return propertyName.length() > 2 && propertyName.characterAt(0) == '-' && propertyName.characterAt(1) == '-';
}

template<typename CharacterType> static CSSPropertyID cssPropertyID(std::span<const CharacterType> characters)
{
    std::array<char, maxCSSPropertyNameLength> buffer;
    for (size_t i = 0; i != characters.size(); ++i) {
        auto character = characters[i];
        if (!character || !isASCII(character))
            return CSSPropertyInvalid;
        buffer[i] = toASCIILower(character);
    }
    return findCSSProperty(buffer.data(), characters.size());
}

// FIXME: Remove this mechanism entirely once we can do it without breaking the web.
static bool isAppleLegacyCSSValueKeyword(std::span<const char> characters)
{
    return spanHasPrefix(characters.subspan(1), "apple-"_span)
        && !spanHasPrefix(characters.subspan(7), "system"_span)
        && !spanHasPrefix(characters.subspan(7), "pay"_span)
        && !spanHasPrefix(characters.subspan(7), "wireless"_span);
}

template<typename CharacterType> static CSSValueID cssValueKeywordID(std::span<const CharacterType> characters)
{
    ASSERT(!characters.empty()); // Otherwise buffer[0] would access uninitialized memory below.

    std::array<char, maxCSSValueKeywordLength + 1> buffer; // 1 to turn "apple" into "webkit"
    
    for (unsigned i = 0; i != characters.size(); ++i) {
        auto character = characters[i];
        if (!character || !isASCII(character))
            return CSSValueInvalid;
        buffer[i] = toASCIILower(character);
    }

    // In most cases, if the prefix is -apple-, change it to -webkit-. This makes the string one character longer.
    auto length = characters.size();
    std::span bufferSpan { buffer };
    if (buffer[0] == '-' && isAppleLegacyCSSValueKeyword(bufferSpan.first(length))) {
        memmoveSpan(bufferSpan.subspan(7), bufferSpan.subspan(6, length - 6));
        memcpySpan(bufferSpan.subspan(1), "webkit"_span);
        ++length;
    }

    return findCSSValueKeyword(bufferSpan.first(length));
}

CSSValueID cssValueKeywordID(StringView string)
{
    unsigned length = string.length();
    if (!length)
        return CSSValueInvalid;
    if (length > maxCSSValueKeywordLength)
        return CSSValueInvalid;
    
    return string.is8Bit() ? cssValueKeywordID(string.span8()) : cssValueKeywordID(string.span16());
}

CSSPropertyID cssPropertyID(StringView string)
{
    unsigned length = string.length();
    
    if (!length)
        return CSSPropertyInvalid;
    if (length > maxCSSPropertyNameLength)
        return CSSPropertyInvalid;
    
    return string.is8Bit() ? cssPropertyID(string.span8()) : cssPropertyID(string.span16());
}

using namespace CSSPropertyParserHelpers;

CSSPropertyParser::CSSPropertyParser(const CSSParserTokenRange& range, const CSSParserContext& context, Vector<CSSProperty, 256>* parsedProperties, bool consumeWhitespace)
    : m_range(range)
    , m_context(context)
    , m_parsedProperties(parsedProperties)
{
    if (consumeWhitespace)
        m_range.consumeWhitespace();
}

void CSSPropertyParser::addProperty(CSSPropertyID property, CSSPropertyID currentShorthand, RefPtr<CSSValue>&& value, IsImportant important, IsImplicit implicit)
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
        m_parsedProperties->append(CSSProperty(property, value.releaseNonNull(), important, setFromShorthand, shorthandIndex, implicit));
    else {
        ASSERT(setFromShorthand);
        m_parsedProperties->append(CSSProperty(property, Ref { CSSPrimitiveValue::implicitInitialValue() }, important, setFromShorthand, shorthandIndex, IsImplicit::Yes));
    }
}

void CSSPropertyParser::addPropertyForCurrentShorthand(CSS::PropertyParserState& state, CSSPropertyID longhand, RefPtr<CSSValue>&& value, IsImplicit implicit)
{
    addProperty(longhand, state.currentProperty, WTFMove(value), state.important, implicit);
}

void CSSPropertyParser::addPropertyForAllLonghandsOfShorthand(CSSPropertyID shorthand, RefPtr<CSSValue>&& value, IsImportant important, IsImplicit implicit)
{
    for (auto longhand : shorthandForProperty(shorthand))
        addProperty(longhand, shorthand, value.copyRef(), important, implicit);
}

void CSSPropertyParser::addPropertyForAllLonghandsOfCurrentShorthand(CSS::PropertyParserState& state, RefPtr<CSSValue>&& value, IsImplicit implicit)
{
    addPropertyForAllLonghandsOfShorthand(state.currentProperty, WTFMove(value), state.important, implicit);
}

bool CSSPropertyParser::parseValue(CSSPropertyID property, IsImportant important, const CSSParserTokenRange& range, const CSSParserContext& context, ParsedPropertyVector& parsedProperties, StyleRuleType ruleType)
{
    int parsedPropertiesSize = parsedProperties.size();
    
    CSSPropertyParser parser(range, context, &parsedProperties);

    bool parseSuccess;
    switch (ruleType) {
    case StyleRuleType::CounterStyle:
        parseSuccess = parser.parseCounterStyleDescriptor(property);
        break;
    case StyleRuleType::FontFace:
        parseSuccess = parser.parseFontFaceDescriptor(property);
        break;
    case StyleRuleType::FontPaletteValues:
        parseSuccess = parser.parseFontPaletteValuesDescriptor(property);
        break;
    case StyleRuleType::Keyframe:
        parseSuccess = parser.parseKeyframeDescriptor(property, important);
        break;
    case StyleRuleType::Page:
        parseSuccess = parser.parsePageDescriptor(property, important);
        break;
    case StyleRuleType::Property:
        parseSuccess = parser.parsePropertyDescriptor(property);
        break;
    case StyleRuleType::ViewTransition:
        parseSuccess = parser.parseViewTransitionDescriptor(property);
        break;
    case StyleRuleType::PositionTry:
        parseSuccess = parser.parsePositionTryDescriptor(property, important);
        break;
    default:
        parseSuccess = parser.parseStyleProperty(property, important, ruleType);
        break;
    }

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

RefPtr<CSSValue> CSSPropertyParser::parseStylePropertyLonghand(CSSPropertyID property, const String& string, const CSSParserContext& context)
{
    ASSERT(!WebCore::isShorthand(property));

    if (string.isEmpty())
        return nullptr;

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Style,
        .currentProperty = property,
        .important = IsImportant::No,
    };
    if (RefPtr value = CSSParserFastPaths::maybeParseValue(property, string, state))
        return value;

    CSSTokenizer tokenizer(string);
    CSSPropertyParser parser(tokenizer.tokenRange(), context, nullptr);
    if (RefPtr value = maybeConsumeCSSWideKeyword(parser.m_range))
        return value;

    RefPtr value = parser.parseStylePropertyLonghand(property, state);
    if (!value || !parser.m_range.atEnd())
        return nullptr;

    return value;
}

RefPtr<CSSValue> CSSPropertyParser::parseStylePropertyLonghand(CSSPropertyID property, const CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(!WebCore::isShorthand(property));

    CSSPropertyParser parser(range, context, nullptr);
    if (RefPtr value = maybeConsumeCSSWideKeyword(parser.m_range))
        return value;

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Style,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr value = parser.parseStylePropertyLonghand(property, state);
    if (!value || !parser.m_range.atEnd())
        return nullptr;

    return value;
}

RefPtr<CSSCustomPropertyValue> CSSPropertyParser::parseTypedCustomPropertyValue(const AtomString& name, const CSSCustomPropertySyntax& syntax, const CSSParserTokenRange& tokens, Style::BuilderState& builderState, const CSSParserContext& context)
{
    CSSPropertyParser parser(tokens, context, nullptr, false);

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Style,
        .currentProperty = CSSPropertyCustom,
        .important = IsImportant::No,
    };

    RefPtr value = parser.parseTypedCustomPropertyValue(state, name, syntax, builderState);
    if (!value || !parser.m_range.atEnd())
        return nullptr;
    return value;
}

RefPtr<CSSCustomPropertyValue> CSSPropertyParser::parseTypedCustomPropertyInitialValue(const AtomString& name, const CSSCustomPropertySyntax& syntax, CSSParserTokenRange tokens, Style::BuilderState& builderState, const CSSParserContext& context)
{
    if (syntax.isUniversal())
        return CSSVariableParser::parseInitialValueForUniversalSyntax(name, tokens);

    CSSPropertyParser parser(tokens, context, nullptr, false);

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Style,
        .currentProperty = CSSPropertyCustom,
        .important = IsImportant::No,
    };

    RefPtr value = parser.parseTypedCustomPropertyValue(state, name, syntax, builderState);
    if (!value || !parser.m_range.atEnd())
        return nullptr;

    if (value->containsCSSWideKeyword())
        return nullptr;

    return value;
}

ComputedStyleDependencies CSSPropertyParser::collectParsedCustomPropertyValueDependencies(const CSSCustomPropertySyntax& syntax, const CSSParserTokenRange& tokens, const CSSParserContext& context)
{
    CSSPropertyParser parser(tokens, context, nullptr);

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Style,
        .currentProperty = CSSPropertyCustom,
        .important = IsImportant::No,
    };

    return parser.collectParsedCustomPropertyValueDependencies(state, syntax);
}

bool CSSPropertyParser::isValidCustomPropertyValueForSyntax(const CSSCustomPropertySyntax& syntax, CSSParserTokenRange tokens, const CSSParserContext& context)
{
    if (syntax.isUniversal())
        return true;

    CSSPropertyParser parser { tokens, context, nullptr };

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Style,
        .currentProperty = CSSPropertyCustom,
        .important = IsImportant::No,
    };

    return !!parser.consumeCustomPropertyValueWithSyntax(state, syntax).first;
}

bool CSSPropertyParser::parseStyleProperty(CSSPropertyID property, IsImportant important, StyleRuleType ruleType)
{
    if (CSSProperty::isDescriptorOnly(property))
        return false;

    auto state = CSS::PropertyParserState {
        .context = m_context,
        .currentRule = ruleType,
        .currentProperty = property,
        .important = important,
    };

    if (WebCore::isShorthand(property)) {
        auto rangeCopy = m_range;
        if (RefPtr keywordValue = maybeConsumeCSSWideKeyword(rangeCopy)) {
            addPropertyForAllLonghandsOfCurrentShorthand(state, WTFMove(keywordValue));
            m_range = rangeCopy;
            return true;
        }

        auto originalRange = m_range;

        if (parseStylePropertyShorthand(property, state))
            return true;

        if (CSSVariableParser::containsValidVariableReferences(originalRange, m_context)) {
            addPropertyForAllLonghandsOfCurrentShorthand(state, CSSPendingSubstitutionValue::create(property, CSSVariableReferenceValue::create(originalRange, m_context)));
            return true;
        }
    } else {
        auto rangeCopy = m_range;
        if (RefPtr keywordValue = maybeConsumeCSSWideKeyword(rangeCopy)) {
            addProperty(property, CSSPropertyInvalid, WTFMove(keywordValue), important);
            m_range = rangeCopy;
            return true;
        }

        auto originalRange = m_range;

        RefPtr parsedValue = parseStylePropertyLonghand(property, state);
        if (parsedValue && m_range.atEnd()) {
            addProperty(property, CSSPropertyInvalid, WTFMove(parsedValue), important);
            return true;
        }

        if (CSSVariableParser::containsValidVariableReferences(originalRange, m_context)) {
            addProperty(property, CSSPropertyInvalid, CSSVariableReferenceValue::create(originalRange, m_context), important);
            return true;
        }
    }

    return false;
}

std::pair<RefPtr<CSSValue>, CSSCustomPropertySyntax::Type> CSSPropertyParser::consumeCustomPropertyValueWithSyntax(CSS::PropertyParserState& state, const CSSCustomPropertySyntax& syntax)
{
    ASSERT(!syntax.isUniversal());

    auto rangeCopy = m_range;

    auto consumeSingleValue = [&](auto& range, auto& component) -> RefPtr<CSSValue> {
        switch (component.type) {
        case CSSCustomPropertySyntax::Type::Length:
            return CSSPrimitiveValueResolver<CSS::Length<>>::consumeAndResolve(range, state);
        case CSSCustomPropertySyntax::Type::LengthPercentage:
            return CSSPrimitiveValueResolver<CSS::LengthPercentage<>>::consumeAndResolve(range, state);
        case CSSCustomPropertySyntax::Type::CustomIdent:
            if (RefPtr value = consumeCustomIdent(range)) {
                if (component.ident.isNull() || value->stringValue() == component.ident)
                    return value;
            }
            return nullptr;
        case CSSCustomPropertySyntax::Type::Percentage:
            return CSSPrimitiveValueResolver<CSS::Percentage<>>::consumeAndResolve(range, state);
        case CSSCustomPropertySyntax::Type::Integer:
            return CSSPrimitiveValueResolver<CSS::Integer<>>::consumeAndResolve(range, state);
        case CSSCustomPropertySyntax::Type::Number:
            return CSSPrimitiveValueResolver<CSS::Number<>>::consumeAndResolve(range, state);
        case CSSCustomPropertySyntax::Type::Angle:
            return CSSPrimitiveValueResolver<CSS::Angle<>>::consumeAndResolve(range, state);
        case CSSCustomPropertySyntax::Type::Time:
            return CSSPrimitiveValueResolver<CSS::Time<>>::consumeAndResolve(range, state);
        case CSSCustomPropertySyntax::Type::Resolution:
            return CSSPrimitiveValueResolver<CSS::Resolution<>>::consumeAndResolve(range, state);
        case CSSCustomPropertySyntax::Type::Color:
            return consumeColor(range, state);
        case CSSCustomPropertySyntax::Type::Image:
            return consumeImage(range, state, { AllowedImageType::URLFunction, AllowedImageType::GeneratedImage });
        case CSSCustomPropertySyntax::Type::URL:
            return consumeURL(range, state, { });
        case CSSCustomPropertySyntax::Type::String:
            return consumeString(range);
        case CSSCustomPropertySyntax::Type::TransformFunction:
            return CSSPropertyParsing::consumeTransformFunction(m_range, state);
        case CSSCustomPropertySyntax::Type::TransformList:
            return CSSPropertyParsing::consumeTransformList(m_range, state);
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
        case CSSCustomPropertySyntax::Multiplier::CommaList:
            return consumeListSeparatedBy<',', OneOrMore>(range, [&](auto& range) {
                return consumeSingleValue(range, component);
            });
        case CSSCustomPropertySyntax::Multiplier::SpaceList:
            return consumeListSeparatedBy<' ', OneOrMore>(range, [&](auto& range) {
                return consumeSingleValue(range, component);
            });
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

ComputedStyleDependencies CSSPropertyParser::collectParsedCustomPropertyValueDependencies(CSS::PropertyParserState& state, const CSSCustomPropertySyntax& syntax)
{
    if (syntax.isUniversal())
        return { };

    m_range.consumeWhitespace();

    auto [value, syntaxType] = consumeCustomPropertyValueWithSyntax(state, syntax);
    if (!value)
        return { };

    return value->computedStyleDependencies();
}

RefPtr<CSSCustomPropertyValue> CSSPropertyParser::parseTypedCustomPropertyValue(CSS::PropertyParserState& state, const AtomString& name, const CSSCustomPropertySyntax& syntax, Style::BuilderState& builderState)
{
    if (syntax.isUniversal())
        return CSSCustomPropertyValue::createSyntaxAll(name, CSSVariableData::create(m_range.consumeAll()));

    m_range.consumeWhitespace();

    if (RefPtr value = maybeConsumeCSSWideKeyword(m_range))
        return CSSCustomPropertyValue::createWithID(name, value->valueID());

    auto [value, syntaxType] = consumeCustomPropertyValueWithSyntax(state, syntax);
    if (!value)
        return nullptr;

    auto resolveSyntaxValue = [&, syntaxType = syntaxType](const CSSValue& value) -> std::optional<CSSCustomPropertyValue::SyntaxValue> {
        switch (syntaxType) {
        case CSSCustomPropertySyntax::Type::LengthPercentage:
        case CSSCustomPropertySyntax::Type::Length: {
            auto length = Style::BuilderConverter::convertLength(builderState, downcast<CSSPrimitiveValue>(value));
            return { WTFMove(length) };
        }
        case CSSCustomPropertySyntax::Type::Integer:
        case CSSCustomPropertySyntax::Type::Number: {
            auto doubleValue = downcast<CSSPrimitiveValue>(value).resolveAsNumber(builderState.cssToLengthConversionData());
            return { CSSCustomPropertyValue::NumericSyntaxValue { doubleValue, CSSUnitType::CSS_NUMBER } };
        }
        case CSSCustomPropertySyntax::Type::Percentage: {
            auto doubleValue = downcast<CSSPrimitiveValue>(value).resolveAsPercentage(builderState.cssToLengthConversionData());
            return { CSSCustomPropertyValue::NumericSyntaxValue { doubleValue, CSSUnitType::CSS_PERCENTAGE } };
        }
        case CSSCustomPropertySyntax::Type::Angle: {
            auto doubleValue = downcast<CSSPrimitiveValue>(value).resolveAsAngle(builderState.cssToLengthConversionData());
            return { CSSCustomPropertyValue::NumericSyntaxValue { doubleValue, CSSUnitType::CSS_DEG } };
        }
        case CSSCustomPropertySyntax::Type::Time: {
            auto doubleValue = downcast<CSSPrimitiveValue>(value).resolveAsTime(builderState.cssToLengthConversionData());
            return { CSSCustomPropertyValue::NumericSyntaxValue { doubleValue, CSSUnitType::CSS_S } };
        }
        case CSSCustomPropertySyntax::Type::Resolution: {
            auto doubleValue = downcast<CSSPrimitiveValue>(value).resolveAsResolution(builderState.cssToLengthConversionData());
            return { CSSCustomPropertyValue::NumericSyntaxValue { doubleValue, CSSUnitType::CSS_DPPX } };
        }
        case CSSCustomPropertySyntax::Type::Color: {
            auto color = builderState.createStyleColor(value, Style::ForVisitedLink::No);
            return { WTFMove(color) };
        }
        case CSSCustomPropertySyntax::Type::Image: {
            auto styleImage = builderState.createStyleImage(value);
            if (!styleImage)
                return { };
            return { WTFMove(styleImage) };
        }
        case CSSCustomPropertySyntax::Type::URL:
            return { Style::toStyle(downcast<CSSURLValue>(value).url(), builderState) };
        case CSSCustomPropertySyntax::Type::CustomIdent:
            return { downcast<CSSPrimitiveValue>(value).stringValue() };
        case CSSCustomPropertySyntax::Type::String:
            return { serializeString(downcast<CSSPrimitiveValue>(value).stringValue()) };
        case CSSCustomPropertySyntax::Type::TransformFunction:
        case CSSCustomPropertySyntax::Type::TransformList:
            return { CSSCustomPropertyValue::TransformSyntaxValue { Style::createTransformOperation(value, builderState.cssToLengthConversionData()) } };
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

RefPtr<CSSValue> CSSPropertyParser::parseCounterStyleDescriptor(CSSPropertyID property, const String& string, const CSSParserContext& context)
{
    ASSERT(context.propertySettings.cssCounterStyleAtRulesEnabled);

    auto tokenizer = CSSTokenizer(string);
    auto range = tokenizer.tokenRange();

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::CounterStyle,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    auto result = CSSPropertyParsing::parseCounterStyleDescriptor(range, property, state);

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return nullptr;

    return result;
}

bool CSSPropertyParser::parseCounterStyleDescriptor(CSSPropertyID property)
{
    ASSERT(m_context.propertySettings.cssCounterStyleAtRulesEnabled);

    auto state = CSS::PropertyParserState {
        .context = m_context,
        .currentRule = StyleRuleType::CounterStyle,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr parsedValue = CSSPropertyParsing::parseCounterStyleDescriptor(m_range, property, state);
    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
    return true;
}

bool CSSPropertyParser::parseViewTransitionDescriptor(CSSPropertyID property)
{
    ASSERT(m_context.propertySettings.crossDocumentViewTransitionsEnabled);

    auto state = CSS::PropertyParserState {
        .context = m_context,
        .currentRule = StyleRuleType::ViewTransition,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr parsedValue = CSSPropertyParsing::parseViewTransitionDescriptor(m_range, property, state);
    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
    return true;
}

// Checks whether a CSS property is allowed in @position-try.
static bool propertyAllowedInPositionTryRule(CSSPropertyID property)
{
    return CSSProperty::isInsetProperty(property)
        || CSSProperty::isMarginProperty(property)
        || CSSProperty::isSizingProperty(property)
        || property == CSSPropertyAlignSelf
        || property == CSSPropertyJustifySelf
        || property == CSSPropertyPlaceSelf
        || property == CSSPropertyPositionAnchor
        || property == CSSPropertyPositionArea;
}

bool CSSPropertyParser::parsePositionTryDescriptor(CSSPropertyID property, IsImportant important)
{
    ASSERT(m_context.propertySettings.cssAnchorPositioningEnabled);

    // Per spec, !important is not allowed and makes the whole declaration invalid.
    if (important == IsImportant::Yes)
        return false;

    if (!propertyAllowedInPositionTryRule(property))
        return false;

    return parseStyleProperty(property, important, StyleRuleType::PositionTry);
}

bool CSSPropertyParser::parseFontFaceDescriptor(CSSPropertyID property)
{
    auto state = CSS::PropertyParserState {
        .context = m_context,
        .currentRule = StyleRuleType::FontFace,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr parsedValue = CSSPropertyParsing::parseFontFaceDescriptor(m_range, property, state);
    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
    return true;
}

bool CSSPropertyParser::parseKeyframeDescriptor(CSSPropertyID property, IsImportant important)
{
    // https://www.w3.org/TR/css-animations-1/#keyframes
    // The <declaration-list> inside of <keyframe-block> accepts any CSS property except those
    // defined in this specification, but does accept the animation-timing-function property and
    // interprets it specially.
    switch (property) {
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
        return parseStyleProperty(property, important, StyleRuleType::Keyframe);
    }
}

bool CSSPropertyParser::parsePropertyDescriptor(CSSPropertyID property)
{
    auto state = CSS::PropertyParserState {
        .context = m_context,
        .currentRule = StyleRuleType::Property,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr parsedValue = CSSPropertyParsing::parsePropertyDescriptor(m_range, property, state);
    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
    return true;
}

bool CSSPropertyParser::parseFontPaletteValuesDescriptor(CSSPropertyID property)
{
    auto state = CSS::PropertyParserState {
        .context = m_context,
        .currentRule = StyleRuleType::FontPaletteValues,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr parsedValue = CSSPropertyParsing::parseFontPaletteValuesDescriptor(m_range, property, state);
    if (!parsedValue || !m_range.atEnd())
        return false;

    addProperty(property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
    return true;
}

bool CSSPropertyParser::parsePageDescriptor(CSSPropertyID property, IsImportant important)
{
    // Does not apply in @page per-spec.
    if (property == CSSPropertyPage)
        return false;

    auto state = CSS::PropertyParserState {
        .context = m_context,
        .currentRule = StyleRuleType::Page,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    if (RefPtr parsedValue = CSSPropertyParsing::parsePageDescriptor(m_range, property, state)) {
        if (!m_range.atEnd())
            return false;

        addProperty(property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
        return true;
    }

    return parseStyleProperty(property, important, StyleRuleType::Page);
}

bool CSSPropertyParser::consumeFontShorthand(CSS::PropertyParserState& state)
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
        addPropertyForAllLonghandsOfCurrentShorthand(state, CSSPrimitiveValue::create(systemFont), IsImplicit::Yes);
        return true;
    }

    CSSParserTokenRangeGuard guard { m_range };

    std::array<RefPtr<CSSValue>, 7> values;
    auto& fontStyle = values[0];
    auto& fontVariantCaps = values[1];
    auto& fontWeight = values[2];
    auto& fontWidth = values[3];
    auto& fontSize = values[4];
    auto& lineHeight = values[5];
    auto& fontFamily = values[6];

    // Optional font-style, font-variant, font-width and font-weight, in any order.
    for (unsigned i = 0; i < 4 && !m_range.atEnd(); ++i) {
        if (consumeIdent<CSSValueNormal>(m_range))
            continue;
        if (!fontStyle && (fontStyle = parseStylePropertyLonghand(CSSPropertyFontStyle, state)))
            continue;
        if (!fontVariantCaps && (fontVariantCaps = consumeIdent<CSSValueSmallCaps>(m_range)))
            continue;
        if (!fontWeight && (fontWeight = parseStylePropertyLonghand(CSSPropertyFontWeight, state)))
            continue;
        if (!fontWidth && (fontWidth = CSSPropertyParsing::consumeFontWidthAbsolute(m_range)))
            continue;
        break;
    }

    if (m_range.atEnd())
        return false;

    fontSize = parseStylePropertyLonghand(CSSPropertyFontSize, state);
    if (!fontSize || m_range.atEnd())
        return false;

    if (consumeSlashIncludingWhitespace(m_range)) {
        if (!consumeIdent<CSSValueNormal>(m_range)) {
            lineHeight = parseStylePropertyLonghand(CSSPropertyLineHeight, state);
            if (!lineHeight)
                return false;
        }
        if (m_range.atEnd())
            return false;
    }

    fontFamily = parseStylePropertyLonghand(CSSPropertyFontFamily, state);
    if (!fontFamily || !m_range.atEnd())
        return false;

    guard.commit();

    auto shorthandProperties = fontShorthand().properties();
    for (auto [value, longhand] : zippedRange(values, shorthandProperties.first(values.size())))
        addPropertyForCurrentShorthand(state, longhand, WTFMove(value), IsImplicit::Yes);
    for (auto longhand : shorthandProperties.subspan(values.size()))
        addPropertyForCurrentShorthand(state, longhand, nullptr, IsImplicit::Yes);

    return true;
}

bool CSSPropertyParser::consumeFontVariantShorthand(CSS::PropertyParserState& state)
{
    if (identMatches<CSSValueNormal, CSSValueNone>(m_range.peek().id())) {
        addPropertyForCurrentShorthand(state, CSSPropertyFontVariantLigatures, consumeIdent(m_range));
        addPropertyForCurrentShorthand(state, CSSPropertyFontVariantCaps, nullptr);
        addPropertyForCurrentShorthand(state, CSSPropertyFontVariantAlternates, nullptr);
        addPropertyForCurrentShorthand(state, CSSPropertyFontVariantNumeric, nullptr);
        addPropertyForCurrentShorthand(state, CSSPropertyFontVariantEastAsian, nullptr);
        addPropertyForCurrentShorthand(state, CSSPropertyFontVariantPosition, nullptr);
        addPropertyForCurrentShorthand(state, CSSPropertyFontVariantEmoji, nullptr);
        return m_range.atEnd();
    }

    RefPtr<CSSValue> capsValue;
    RefPtr<CSSValue> alternatesValue;
    RefPtr<CSSValue> positionValue;
    RefPtr<CSSValue> eastAsianValue;
    RefPtr<CSSValue> emojiValue;
    CSSFontVariantLigaturesParser ligaturesParser;
    CSSFontVariantNumericParser numericParser;
    auto implicitLigatures = IsImplicit::Yes;
    auto implicitNumeric = IsImplicit::Yes;
    do {
        if (m_range.peek().id() == CSSValueNormal)
            return false;

        if (!capsValue && (capsValue = parseStylePropertyLonghand(CSSPropertyFontVariantCaps, state)))
            continue;

        if (!positionValue && (positionValue = parseStylePropertyLonghand(CSSPropertyFontVariantPosition, state)))
            continue;

        if (!alternatesValue && (alternatesValue = parseStylePropertyLonghand(CSSPropertyFontVariantAlternates, state)))
            continue;

        auto ligaturesParseResult = ligaturesParser.consumeLigature(m_range);
        auto numericParseResult = numericParser.consumeNumeric(m_range);
        if (ligaturesParseResult == CSSFontVariantLigaturesParser::ParseResult::ConsumedValue) {
            implicitLigatures = IsImplicit::No;
            continue;
        }
        if (numericParseResult == CSSFontVariantNumericParser::ParseResult::ConsumedValue) {
            implicitNumeric = IsImplicit::No;
            continue;
        }

        if (ligaturesParseResult == CSSFontVariantLigaturesParser::ParseResult::DisallowedValue
            || numericParseResult == CSSFontVariantNumericParser::ParseResult::DisallowedValue)
            return false;

        if (!eastAsianValue && (eastAsianValue = parseStylePropertyLonghand(CSSPropertyFontVariantEastAsian, state)))
            continue;

        if (m_context.propertySettings.cssFontVariantEmojiEnabled && !emojiValue && (emojiValue = parseStylePropertyLonghand(CSSPropertyFontVariantEmoji, state)))
            continue;

        // Saw some value that didn't match anything else.
        return false;
    } while (!m_range.atEnd());

    addPropertyForCurrentShorthand(state, CSSPropertyFontVariantLigatures, ligaturesParser.finalizeValue().releaseNonNull(), implicitLigatures);
    addPropertyForCurrentShorthand(state, CSSPropertyFontVariantCaps, WTFMove(capsValue));
    addPropertyForCurrentShorthand(state, CSSPropertyFontVariantAlternates, WTFMove(alternatesValue));
    addPropertyForCurrentShorthand(state, CSSPropertyFontVariantNumeric, numericParser.finalizeValue().releaseNonNull(), implicitNumeric);
    addPropertyForCurrentShorthand(state, CSSPropertyFontVariantEastAsian, WTFMove(eastAsianValue));
    addPropertyForCurrentShorthand(state, CSSPropertyFontVariantPosition, WTFMove(positionValue));
    addPropertyForCurrentShorthand(state, CSSPropertyFontVariantEmoji, WTFMove(emojiValue));
    return true;
}

bool CSSPropertyParser::consumeFontSynthesisShorthand(CSS::PropertyParserState& state)
{
    // none | [ weight || style || small-caps ]
    if (m_range.peek().id() == CSSValueNone) {
        addPropertyForCurrentShorthand(state, CSSPropertyFontSynthesisSmallCaps, consumeIdent(m_range).releaseNonNull());
        addPropertyForCurrentShorthand(state, CSSPropertyFontSynthesisStyle, CSSPrimitiveValue::create(CSSValueNone));
        addPropertyForCurrentShorthand(state, CSSPropertyFontSynthesisWeight, CSSPrimitiveValue::create(CSSValueNone));
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

    addPropertyForCurrentShorthand(state, CSSPropertyFontSynthesisWeight, CSSPrimitiveValue::create(foundWeight ? CSSValueAuto : CSSValueNone));
    addPropertyForCurrentShorthand(state, CSSPropertyFontSynthesisStyle, CSSPrimitiveValue::create(foundStyle ? CSSValueAuto : CSSValueNone));
    addPropertyForCurrentShorthand(state, CSSPropertyFontSynthesisSmallCaps, CSSPrimitiveValue::create(foundSmallCaps ? CSSValueAuto : CSSValueNone));
    return true;
}

bool CSSPropertyParser::consumeTextDecorationShorthand(CSS::PropertyParserState& state)
{
    auto line = consumeTextDecorationLine(m_range, state);
    if (!line || !m_range.atEnd())
        return false;
    addPropertyForCurrentShorthand(state, CSSPropertyTextDecorationLine, line.releaseNonNull());
    return true;
}

bool CSSPropertyParser::consumeTextDecorationSkipShorthand(CSS::PropertyParserState& state)
{
    if (auto skip = consumeIdentRaw<CSSValueNone, CSSValueAuto, CSSValueInk>(m_range)) {
        switch (*skip) {
        case CSSValueNone:
            addPropertyForCurrentShorthand(state, CSSPropertyTextDecorationSkipInk, CSSPrimitiveValue::create(CSSValueNone));
            return m_range.atEnd();
        case CSSValueAuto:
        case CSSValueInk:
            addPropertyForCurrentShorthand(state, CSSPropertyTextDecorationSkipInk, CSSPrimitiveValue::create(CSSValueAuto));
            return m_range.atEnd();
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }
    return false;
}

bool CSSPropertyParser::consumeBorderSpacingShorthand(CSS::PropertyParserState& state)
{
    RefPtr horizontalSpacing = CSSPrimitiveValueResolver<CSS::Length<CSS::Nonnegative>>::consumeAndResolve(m_range, state);
    if (!horizontalSpacing)
        return false;
    RefPtr verticalSpacing = horizontalSpacing;
    if (!m_range.atEnd())
        verticalSpacing = CSSPrimitiveValueResolver<CSS::Length<CSS::Nonnegative>>::consumeAndResolve(m_range, state);
    if (!verticalSpacing || !m_range.atEnd())
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyWebkitBorderHorizontalSpacing, horizontalSpacing.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyWebkitBorderVerticalSpacing, verticalSpacing.releaseNonNull());
    return true;
}

bool CSSPropertyParser::consumeColumnsShorthand(CSS::PropertyParserState& state)
{
    RefPtr<CSSValue> columnWidth;
    RefPtr<CSSValue> columnCount;

    for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !m_range.atEnd(); ++propertiesParsed) {
        if (m_range.peek().id() == CSSValueAuto) {
            // 'auto' is a valid value for any of the two longhands, and at this point
            // we don't know which one(s) it is meant for. We need to see if there are other values first.
            consumeIdent(m_range);
        } else {
            if (!columnWidth && (columnWidth = parseStylePropertyLonghand(CSSPropertyColumnWidth, state)))
                continue;
            if (!columnCount && (columnCount = parseStylePropertyLonghand(CSSPropertyColumnCount, state)))
                continue;
            // If we didn't find at least one match, this is an invalid shorthand and we have to ignore it.
            return false;
        }
    }

    if (!m_range.atEnd())
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyColumnWidth, WTFMove(columnWidth));
    addPropertyForCurrentShorthand(state, CSSPropertyColumnCount, WTFMove(columnCount));
    return true;
}

struct InitialNumericValue {
    double number;
    CSSUnitType type { CSSUnitType::CSS_NUMBER };
};
using InitialValue = Variant<CSSValueID, InitialNumericValue>;

static constexpr InitialValue initialValueForLonghand(CSSPropertyID longhand)
{
    // Currently, this tries to cover just longhands that can be omitted from shorthands when parsing or serializing.
    // Later, we likely want to cover all properties, and generate the table from CSSProperties.json.
    switch (longhand) {
    case CSSPropertyAccentColor:
    case CSSPropertyAlignSelf:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationTimeline:
    case CSSPropertyAspectRatio:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBlockSize:
    case CSSPropertyBlockStepAlign:
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
    case CSSPropertyAnimationRangeEnd:
    case CSSPropertyAnimationRangeStart:
    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyColumnGap:
    case CSSPropertyContainerType:
    case CSSPropertyContent:
    case CSSPropertyFontFeatureSettings:
    case CSSPropertyFontPalette:
    case CSSPropertyFontWidth:
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
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
        return InitialNumericValue { 0, CSSUnitType::CSS_S };
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationName:
    case CSSPropertyAppearance:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBlockEllipsis:
    case CSSPropertyBlockStepSize:
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
    case CSSPropertyMaxLines:
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
    case CSSPropertyBlockStepInsert:
        return CSSValueMarginBox;
    case CSSPropertyBlockStepRound:
        return CSSValueUp;
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
    case CSSPropertyLineFitEdge:
        return CSSValueLeading;
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
    case CSSPropertyPositionTryOrder:
        return CSSValueNormal;
    case CSSPropertyPositionTryFallbacks:
        return CSSValueNone;
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
        return CSSValueAuto;
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
    case CSSPropertyFieldSizing:
        return CSSValueFixed;
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
    return value.primitiveType() == type && !value.isCalculated() && value.valueNoConversionDataRequired<double>() == number;
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
    return isNumber(quad.protectedTop(), number, type)
        && isNumber(quad.protectedRight(), number, type)
        && isNumber(quad.protectedBottom(), number, type)
        && isNumber(quad.protectedLeft(), number, type);
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

bool CSSPropertyParser::consumeShorthandGreedily(const StylePropertyShorthand& shorthand, CSS::PropertyParserState& state)
{
    ASSERT(state.currentProperty == shorthand.id());
    ASSERT(shorthand.length() <= 6); // Existing shorthands have at most 6 longhands.
    std::array<RefPtr<CSSValue>, 6> longhands;
    auto shorthandProperties = shorthand.properties();
    do {
        bool foundLonghand = false;
        for (size_t i = 0; !foundLonghand && i < shorthand.length(); ++i) {
            if (longhands[i])
                continue;

            longhands[i] = parseStylePropertyLonghand(shorthandProperties[i], state);
            if (longhands[i])
                foundLonghand = true;
        }
        if (!foundLonghand)
            return false;
    } while (!m_range.atEnd());

    for (size_t i = 0; i < shorthand.length(); ++i)
        addPropertyForCurrentShorthand(state, shorthandProperties[i], WTFMove(longhands[i]));
    return true;
}

bool CSSPropertyParser::consumeFlexShorthand(CSS::PropertyParserState& state)
{
    // <'flex'>        = none | [ <'flex-grow'> <'flex-shrink'>? || <'flex-basis'> ]
    // <'flex-grow'>   = <number [0,∞]>
    //     NOTE: When omitted from shorthand, it is set to 1.
    // <'flex-shrink'> = <number [0,∞]>
    //     NOTE: When omitted from shorthand, it is set to 1.
    // <'flex-basis'>  = content | <'width'>
    //    NOTE: When omitted from shorthand, it is set to 0.
    // https://drafts.csswg.org/css-flexbox/#propdef-flex

    auto isFlexBasisIdent = [](CSSValueID id) {
        switch (id) {
        case CSSValueAuto:
        case CSSValueContent:
        case CSSValueIntrinsic:
        case CSSValueMinIntrinsic:
        case CSSValueMinContent:
        case CSSValueWebkitMinContent:
        case CSSValueMaxContent:
        case CSSValueWebkitMaxContent:
        case CSSValueWebkitFillAvailable:
        case CSSValueFitContent:
        case CSSValueWebkitFitContent:
            return true;
        default:
            return false;
        }
    };

    RefPtr<CSSPrimitiveValue> flexGrow;
    RefPtr<CSSPrimitiveValue> flexShrink;
    RefPtr<CSSPrimitiveValue> flexBasis;

    if (m_range.peek().id() == CSSValueNone) {
        flexGrow = CSSPrimitiveValue::create(0);
        flexShrink = CSSPrimitiveValue::create(0);
        flexBasis = CSSPrimitiveValue::create(CSSValueAuto);
        m_range.consumeIncludingWhitespace();
    } else {
        unsigned index = 0;
        while (!m_range.atEnd() && index++ < 3) {
            if (auto number = CSSPrimitiveValueResolver<CSS::Number<CSS::Nonnegative>>::consumeAndResolve(m_range, state)) {
                if (!flexGrow)
                    flexGrow = WTFMove(number);
                else if (!flexShrink)
                    flexShrink = WTFMove(number);
                else if (number->isZero() == true) // flex only allows a basis of 0 (sans units) if flex-grow and flex-shrink values have already been set.
                    flexBasis = CSSPrimitiveValue::create(0, CSSUnitType::CSS_PX);
                else
                    return false;
            } else if (!flexBasis) {
                if (isFlexBasisIdent(m_range.peek().id()))
                    flexBasis = consumeIdent(m_range);
                if (!flexBasis)
                    flexBasis = CSSPrimitiveValueResolver<CSS::LengthPercentage<CSS::Nonnegative>>::consumeAndResolve(m_range, state);
                if (index == 2 && !m_range.atEnd())
                    return false;
            }
        }
        if (index == 0)
            return false;
        if (!flexGrow)
            flexGrow = CSSPrimitiveValue::create(1);
        if (!flexShrink)
            flexShrink = CSSPrimitiveValue::create(1);
        
        // FIXME: Using % here is a hack to work around intrinsic sizing implementation being
        // a mess (e.g., turned off for nested column flexboxes, failing to relayout properly even
        // if turned back on for nested columns, etc.). We have layout test coverage of both
        // scenarios.
        if (!flexBasis)
            flexBasis = CSSPrimitiveValue::create(0, CSSUnitType::CSS_PERCENTAGE);
    }

    if (!m_range.atEnd())
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyFlexGrow, flexGrow.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyFlexShrink, flexShrink.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyFlexBasis, flexBasis.releaseNonNull());
    return true;
}

struct BorderShorthandComponents {
    RefPtr<CSSValue> width;
    RefPtr<CSSValue> style;
    RefPtr<CSSValue> color;
};

static std::optional<BorderShorthandComponents> consumeBorderShorthandComponents(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    BorderShorthandComponents components { };

    while (!components.width || !components.style || !components.color) {
        if (!components.width) {
            components.width = CSSPropertyParsing::parseStyleProperty(range, CSSPropertyBorderLeftWidth, state);
            if (components.width)
                continue;
        }
        if (!components.style) {
            components.style = CSSPropertyParsing::parseStyleProperty(range, CSSPropertyBorderLeftStyle, state);
            if (components.style)
                continue;
        }
        if (!components.color) {
            components.color = CSSPropertyParsing::parseStyleProperty(range, CSSPropertyBorderLeftColor, state);
            if (components.color)
                continue;
        }
        break;
    }

    if (!components.width && !components.style && !components.color)
        return { };

    if (!range.atEnd())
        return { };

    return components;
}

bool CSSPropertyParser::consumeBorderShorthand(CSS::PropertyParserState& state)
{
    auto components = consumeBorderShorthandComponents(m_range, state);
    if (!components)
        return false;

    addPropertyForAllLonghandsOfShorthand(CSSPropertyBorderWidth, WTFMove(components->width), state.important);
    addPropertyForAllLonghandsOfShorthand(CSSPropertyBorderStyle, WTFMove(components->style), state.important);
    addPropertyForAllLonghandsOfShorthand(CSSPropertyBorderColor, WTFMove(components->color), state.important);

    for (auto longhand : borderImageShorthand())
        addPropertyForCurrentShorthand(state, longhand, nullptr);
    return true;
}

bool CSSPropertyParser::consumeBorderInlineShorthand(CSS::PropertyParserState& state)
{
    auto components = consumeBorderShorthandComponents(m_range, state);
    if (!components)
        return false;

    addPropertyForAllLonghandsOfShorthand(CSSPropertyBorderInlineWidth, WTFMove(components->width), state.important);
    addPropertyForAllLonghandsOfShorthand(CSSPropertyBorderInlineStyle, WTFMove(components->style), state.important);
    addPropertyForAllLonghandsOfShorthand(CSSPropertyBorderInlineColor, WTFMove(components->color), state.important);
    return true;
}

bool CSSPropertyParser::consumeBorderBlockShorthand(CSS::PropertyParserState& state)
{
    auto components = consumeBorderShorthandComponents(m_range, state);
    if (!components)
        return false;

    addPropertyForAllLonghandsOfShorthand(CSSPropertyBorderBlockWidth, WTFMove(components->width), state.important);
    addPropertyForAllLonghandsOfShorthand(CSSPropertyBorderBlockStyle, WTFMove(components->style), state.important);
    addPropertyForAllLonghandsOfShorthand(CSSPropertyBorderBlockColor, WTFMove(components->color), state.important);
    return true;
}

bool CSSPropertyParser::consume2ValueShorthand(const StylePropertyShorthand& shorthand, CSS::PropertyParserState& state)
{
    ASSERT(state.currentProperty == shorthand.id());
    ASSERT(shorthand.length() == 2);
    auto longhands = shorthand.properties();
    RefPtr start = parseStylePropertyLonghand(longhands[0], state);
    if (!start)
        return false;

    RefPtr end = parseStylePropertyLonghand(longhands[1], state);
    auto endImplicit = !end ? IsImplicit::Yes : IsImplicit::No;
    if (endImplicit == IsImplicit::Yes)
        end = start;

    addPropertyForCurrentShorthand(state, longhands[0], start.releaseNonNull());
    addPropertyForCurrentShorthand(state, longhands[1], end.releaseNonNull(), endImplicit);
    return m_range.atEnd();
}

bool CSSPropertyParser::consume4ValueShorthand(const StylePropertyShorthand& shorthand, CSS::PropertyParserState& state)
{
    ASSERT(state.currentProperty == shorthand.id());
    ASSERT(shorthand.length() == 4);
    auto longhands = shorthand.properties();
    RefPtr top = parseStylePropertyLonghand(longhands[0], state);
    if (!top)
        return false;

    RefPtr right = parseStylePropertyLonghand(longhands[1], state);
    RefPtr<CSSValue> bottom;
    RefPtr<CSSValue> left;
    if (right) {
        bottom = parseStylePropertyLonghand(longhands[2], state);
        if (bottom)
            left = parseStylePropertyLonghand(longhands[3], state);
    }

    auto rightImplicit = !right ? IsImplicit::Yes : IsImplicit::No;
    auto bottomImplicit = !bottom ? IsImplicit::Yes : IsImplicit::No;
    auto leftImplicit = !left ? IsImplicit::Yes : IsImplicit::No;

    if (rightImplicit == IsImplicit::Yes)
        right = top;
    if (bottomImplicit == IsImplicit::Yes)
        bottom = top;
    if (leftImplicit == IsImplicit::Yes)
        left = right;

    addPropertyForCurrentShorthand(state, longhands[0], top.releaseNonNull());
    addPropertyForCurrentShorthand(state, longhands[1], right.releaseNonNull(), rightImplicit);
    addPropertyForCurrentShorthand(state, longhands[2], bottom.releaseNonNull(), bottomImplicit);
    addPropertyForCurrentShorthand(state, longhands[3], left.releaseNonNull(), leftImplicit);
    return m_range.atEnd();
}

bool CSSPropertyParser::consumeBorderRadiusShorthand(CSS::PropertyParserState& state)
{
    auto borderRadius = consumeUnresolvedBorderRadius(m_range, state);
    if (!borderRadius)
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyBorderTopLeftRadius, WebCore::CSS::createCSSValue(borderRadius->topLeft()));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderTopRightRadius, WebCore::CSS::createCSSValue(borderRadius->topRight()));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderBottomRightRadius, WebCore::CSS::createCSSValue(borderRadius->bottomRight()));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderBottomLeftRadius, WebCore::CSS::createCSSValue(borderRadius->bottomLeft()));
    return true;
}

bool CSSPropertyParser::consumeWebkitBorderRadiusShorthand(CSS::PropertyParserState& state)
{
    auto borderRadius = consumeUnresolvedWebKitBorderRadius(m_range, state);
    if (!borderRadius)
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyBorderTopLeftRadius, WebCore::CSS::createCSSValue(borderRadius->topLeft()));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderTopRightRadius, WebCore::CSS::createCSSValue(borderRadius->topRight()));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderBottomRightRadius, WebCore::CSS::createCSSValue(borderRadius->bottomRight()));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderBottomLeftRadius, WebCore::CSS::createCSSValue(borderRadius->bottomLeft()));
    return true;
}

bool CSSPropertyParser::consumeBorderImageShorthand(CSS::PropertyParserState& state)
{
    auto components = consumeBorderImageComponents(m_range, state);
    if (!components)
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyBorderImageSource, WTFMove(components->source));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderImageSlice, WTFMove(components->slice));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderImageWidth, WTFMove(components->width));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderImageOutset, WTFMove(components->outset));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderImageRepeat, WTFMove(components->repeat));
    return true;
}

bool CSSPropertyParser::consumeWebkitBorderImageShorthand(CSS::PropertyParserState& state)
{
    // NOTE: -webkit-border-image has a legacy behavior that makes border image slices default to `fill`.
    // NOTE: -webkit-border-image has a legacy behavior that makes border image widths with length values also set the border widths.

    auto components = consumeBorderImageComponents(m_range, state, BorderImageSliceFillDefault::Yes, BorderImageWidthOverridesWidthForLength::Yes);
    if (!components)
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyBorderImageSource, WTFMove(components->source));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderImageSlice, WTFMove(components->slice));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderImageWidth, WTFMove(components->width));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderImageOutset, WTFMove(components->outset));
    addPropertyForCurrentShorthand(state, CSSPropertyBorderImageRepeat, WTFMove(components->repeat));
    return true;
}

bool CSSPropertyParser::consumeMaskBorderShorthand(CSS::PropertyParserState& state)
{
    auto components = consumeBorderImageComponents(m_range, state);
    if (!components)
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderSource, WTFMove(components->source));
    addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderSlice, WTFMove(components->slice));
    addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderWidth, WTFMove(components->width));
    addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderOutset, WTFMove(components->outset));
    addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderRepeat, WTFMove(components->repeat));
    return true;
}

bool CSSPropertyParser::consumeWebkitMaskBoxImageShorthand(CSS::PropertyParserState& state)
{
    // NOTE: -webkit-mask-box-image has a legacy behavior that makes border image slices default to `fill`.

    auto components = consumeBorderImageComponents(m_range, state, BorderImageSliceFillDefault::Yes);
    if (!components)
        return false;

    if (!components->slice)
        components->slice = CSSBorderImageSliceValue::create({ CSSPrimitiveValue::create(0), CSSPrimitiveValue::create(0), CSSPrimitiveValue::create(0), CSSPrimitiveValue::create(0) }, true);

    addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderSource, WTFMove(components->source));
    addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderSlice, WTFMove(components->slice));
    addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderWidth, WTFMove(components->width));
    addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderOutset, WTFMove(components->outset));
    addPropertyForCurrentShorthand(state, CSSPropertyMaskBorderRepeat, WTFMove(components->repeat));
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

bool CSSPropertyParser::consumePageBreakAfterShorthand(CSS::PropertyParserState& state)
{
    auto keyword = consumeIdentRaw(m_range);
    if (!keyword || !m_range.atEnd())
        return false;
    auto value = mapFromPageBreakBetween(*keyword);
    if (value == CSSValueInvalid)
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyBreakAfter, CSSPrimitiveValue::create(value));
    return true;
}

bool CSSPropertyParser::consumePageBreakBeforeShorthand(CSS::PropertyParserState& state)
{
    auto keyword = consumeIdentRaw(m_range);
    if (!keyword || !m_range.atEnd())
        return false;
    auto value = mapFromPageBreakBetween(*keyword);
    if (value == CSSValueInvalid)
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyBreakBefore, CSSPrimitiveValue::create(value));
    return true;
}

bool CSSPropertyParser::consumePageBreakInsideShorthand(CSS::PropertyParserState& state)
{
    auto keyword = consumeIdentRaw(m_range);
    if (!keyword || !m_range.atEnd())
        return false;
    auto value = mapFromColumnRegionOrPageBreakInside(*keyword);
    if (value == CSSValueInvalid)
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyBreakInside, CSSPrimitiveValue::create(value));
    return true;
}

bool CSSPropertyParser::consumeWebkitColumnBreakAfterShorthand(CSS::PropertyParserState& state)
{
    // The fragmentation spec says that page-break-(after|before|inside) are to be treated as
    // shorthands for their break-(after|before|inside) counterparts. We'll do the same for the
    // non-standard properties -webkit-column-break-(after|before|inside).

    auto keyword = consumeIdentRaw(m_range);
    if (!keyword || !m_range.atEnd())
        return false;
    auto value = mapFromColumnBreakBetween(*keyword);
    if (value == CSSValueInvalid)
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyBreakAfter, CSSPrimitiveValue::create(value));
    return true;
}

bool CSSPropertyParser::consumeWebkitColumnBreakBeforeShorthand(CSS::PropertyParserState& state)
{
    // The fragmentation spec says that page-break-(after|before|inside) are to be treated as
    // shorthands for their break-(after|before|inside) counterparts. We'll do the same for the
    // non-standard properties -webkit-column-break-(after|before|inside).

    auto keyword = consumeIdentRaw(m_range);
    if (!keyword || !m_range.atEnd())
        return false;
    auto value = mapFromColumnBreakBetween(*keyword);
    if (value == CSSValueInvalid)
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyBreakBefore, CSSPrimitiveValue::create(value));
    return true;
}

bool CSSPropertyParser::consumeWebkitColumnBreakInsideShorthand(CSS::PropertyParserState& state)
{
    // The fragmentation spec says that page-break-(after|before|inside) are to be treated as
    // shorthands for their break-(after|before|inside) counterparts. We'll do the same for the
    // non-standard properties -webkit-column-break-(after|before|inside).

    auto keyword = consumeIdentRaw(m_range);
    if (!keyword || !m_range.atEnd())
        return false;
    auto value = mapFromColumnRegionOrPageBreakInside(*keyword);
    if (value == CSSValueInvalid)
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyBreakInside, CSSPrimitiveValue::create(value));
    return true;
}

bool CSSPropertyParser::consumeWebkitTextOrientationShorthand(CSS::PropertyParserState& state)
{
    // -webkit-text-orientation is a legacy shorthand for text-orientation.
    // The only difference is that it accepts 'sideways-right', which is mapped into 'sideways'.
    RefPtr<CSSPrimitiveValue> keyword;
    auto valueID = m_range.peek().id();
    if (valueID == CSSValueSidewaysRight) {
        keyword = CSSPrimitiveValue::create(CSSValueSideways);
        consumeIdentRaw(m_range);
    } else if (CSSPropertyParsing::isKeywordValidForStyleProperty(CSSPropertyTextOrientation, valueID, state))
        keyword = consumeIdent(m_range);
    if (!keyword || !m_range.atEnd())
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyTextOrientation, keyword.releaseNonNull());
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

static RefPtr<CSSValue> consumeAnimationValueForShorthand(CSSPropertyID property, CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    switch (property) {
    case CSSPropertyAnimationDelay:
    case CSSPropertyTransitionDelay:
        return CSSPrimitiveValueResolver<CSS::Time<>>::consumeAndResolve(range, state);
    case CSSPropertyAnimationDirection:
        return CSSPropertyParsing::consumeSingleAnimationDirection(range);
    case CSSPropertyAnimationDuration:
        return CSSPropertyParsing::consumeSingleAnimationDuration(range, state);
    case CSSPropertyTransitionDuration:
        return CSSPrimitiveValueResolver<CSS::Time<CSS::Nonnegative>>::consumeAndResolve(range, state);
    case CSSPropertyAnimationFillMode:
        return CSSPropertyParsing::consumeSingleAnimationFillMode(range);
    case CSSPropertyAnimationIterationCount:
        return CSSPropertyParsing::consumeSingleAnimationIterationCount(range, state);
    case CSSPropertyAnimationName:
        return CSSPropertyParsing::consumeSingleAnimationName(range, state);
    case CSSPropertyAnimationPlayState:
        return CSSPropertyParsing::consumeSingleAnimationPlayState(range);
    case CSSPropertyAnimationComposition:
        return CSSPropertyParsing::consumeSingleAnimationComposition(range);
    case CSSPropertyAnimationTimeline:
    case CSSPropertyAnimationRangeStart:
    case CSSPropertyAnimationRangeEnd:
        return nullptr; // reset-only longhands
    case CSSPropertyTransitionProperty:
        return consumeSingleTransitionPropertyOrNone(range, state);
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
        return consumeEasingFunction(range, state);
    case CSSPropertyTransitionBehavior:
        return CSSPropertyParsing::consumeTransitionBehaviorValue(range);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

bool CSSPropertyParser::consumeAnimationShorthand(const StylePropertyShorthand& shorthand, CSS::PropertyParserState& state)
{
    auto shorthandProperties = shorthand.properties();

    const size_t longhandCount = shorthand.length();
    const size_t maxLonghandCount = 11;
    std::array<CSSValueListBuilder, maxLonghandCount> longhands;
    ASSERT(longhandCount <= maxLonghandCount);

    auto isResetOnlyLonghand = [](CSSPropertyID longhand) {
        switch (longhand) {
        case CSSPropertyAnimationTimeline:
        case CSSPropertyAnimationRangeStart:
        case CSSPropertyAnimationRangeEnd:
            return true;
        default:
            return false;
        }
    };

    do {
        std::array<bool, maxLonghandCount> parsedLonghand = { };
        do {
            bool foundProperty = false;
            for (size_t i = 0; i < longhandCount; ++i) {
                if (parsedLonghand[i])
                    continue;

                if (auto value = consumeAnimationValueForShorthand(shorthandProperties[i], m_range, state)) {
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
            if (!parsedLonghand[i] && !isResetOnlyLonghand(shorthandProperties[i]))
                longhands[i].append(Ref { CSSPrimitiveValue::implicitInitialValue() });
            parsedLonghand[i] = false;
        }
    } while (consumeCommaIncludingWhitespace(m_range));

    for (size_t i = 0; i < longhandCount; ++i) {
        if (!isValidAnimationPropertyList(shorthandProperties[i], longhands[i]))
            return false;
    }

    for (size_t i = 0; i < longhandCount; ++i) {
        auto& list = longhands[i];
        if (list.isEmpty()) // reset-only property
            addPropertyForCurrentShorthand(state, shorthandProperties[i], nullptr);
        else
            addPropertyForCurrentShorthand(state, shorthandProperties[i], CSSValueList::createCommaSeparated(WTFMove(list)));
    }

    return m_range.atEnd();
}

static RefPtr<CSSValue> consumeBackgroundComponent(CSSPropertyID property, CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    switch (property) {
    // background-*
    case CSSPropertyBackgroundClip:
        return CSSPropertyParsing::consumeSingleBackgroundClip(range, state);
    case CSSPropertyBackgroundBlendMode:
        return CSSPropertyParsing::consumeSingleBackgroundBlendMode(range);
    case CSSPropertyBackgroundAttachment:
        return CSSPropertyParsing::consumeSingleBackgroundAttachment(range);
    case CSSPropertyBackgroundOrigin:
        return CSSPropertyParsing::consumeSingleBackgroundOrigin(range);
    case CSSPropertyBackgroundImage:
        return CSSPropertyParsing::consumeSingleBackgroundImage(range, state);
    case CSSPropertyBackgroundRepeat:
        return CSSPropertyParsing::consumeSingleBackgroundRepeat(range, state);
    case CSSPropertyBackgroundPositionX:
        return CSSPropertyParsing::consumeSingleBackgroundPositionX(range, state);
    case CSSPropertyBackgroundPositionY:
        return CSSPropertyParsing::consumeSingleBackgroundPositionY(range, state);
    case CSSPropertyBackgroundSize:
        return consumeSingleBackgroundSize(range, state);
    case CSSPropertyBackgroundColor:
        return consumeColor(range, state);

    // mask-*
    case CSSPropertyMaskComposite:
        return CSSPropertyParsing::consumeSingleMaskComposite(range);
    case CSSPropertyMaskOrigin:
        return CSSPropertyParsing::consumeSingleMaskOrigin(range);
    case CSSPropertyMaskClip:
        return CSSPropertyParsing::consumeSingleMaskClip(range);
    case CSSPropertyMaskImage:
        return CSSPropertyParsing::consumeSingleMaskImage(range, state);
    case CSSPropertyMaskMode:
        return CSSPropertyParsing::consumeSingleMaskMode(range);
    case CSSPropertyMaskRepeat:
        return CSSPropertyParsing::consumeSingleMaskRepeat(range, state);
    case CSSPropertyMaskSize:
        return consumeSingleMaskSize(range, state);

    // -webkit-background-*
    case CSSPropertyWebkitBackgroundSize:
        return consumeSingleWebkitBackgroundSize(range, state);
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
        return CSSPropertyParsing::consumeSingleWebkitMaskPositionX(range, state);
    case CSSPropertyWebkitMaskPositionY:
        return CSSPropertyParsing::consumeSingleWebkitMaskPositionY(range, state);

    default:
        return nullptr;
    };
}

bool CSSPropertyParser::consumeBackgroundShorthand(const StylePropertyShorthand& shorthand, CSS::PropertyParserState& state)
{
    ASSERT(shorthand.id() == state.currentProperty);

    auto shorthandProperties = shorthand.properties();
    unsigned longhandCount = shorthand.length();

    // mask resets mask-border properties outside of this method.
    if (shorthand.id() == CSSPropertyMask)
        longhandCount -= maskBorderShorthand().length();

    std::array<CSSValueListBuilder, 10> longhands;
    ASSERT(longhandCount <= 10);

    do {
        std::array<bool, 10> parsedLonghand = { };
        bool lastParsedWasPosition = false;
        bool clipIsBorderArea = false;
        RefPtr<CSSValue> originValue;
        do {
            bool foundProperty = false;
            for (size_t i = 0; i < longhandCount; ++i) {
                if (parsedLonghand[i])
                    continue;

                RefPtr<CSSValue> value;
                RefPtr<CSSValue> valueY;
                CSSPropertyID property = shorthandProperties[i];

                if (property == CSSPropertyBackgroundPositionX || property == CSSPropertyWebkitMaskPositionX) {
                    // Note: This assumes y properties (for example background-position-y) follow the x properties in the shorthand array.
                    auto position = consumeBackgroundPositionCoordinates(m_range, state);
                    if (!position)
                        continue;
                    value = WTFMove(position->x);
                    valueY = WTFMove(position->y);
                } else if (property == CSSPropertyBackgroundSize) {
                    if (!consumeSlashIncludingWhitespace(m_range))
                        continue;
                    if (!lastParsedWasPosition)
                        return false;
                    value = consumeSingleBackgroundSize(m_range, state);
                    if (!value)
                        return false;
                } else if (property == CSSPropertyMaskSize) {
                    if (!consumeSlashIncludingWhitespace(m_range))
                        continue;
                    if (!lastParsedWasPosition)
                        return false;
                    value = consumeSingleMaskSize(m_range, state);
                    if (!value)
                        return false;
                } else if (property == CSSPropertyBackgroundPositionY || property == CSSPropertyWebkitMaskPositionY) {
                    continue;
                } else {
                    value = consumeBackgroundComponent(property, m_range, state);
                }
                if (value) {
                    if (property == CSSPropertyBackgroundOrigin || property == CSSPropertyMaskOrigin)
                        originValue = value;
                    else if (property == CSSPropertyBackgroundClip)
                        clipIsBorderArea = value->valueID() == CSSValueBorderArea;
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
            CSSPropertyID property = shorthandProperties[i];
            if (property == CSSPropertyBackgroundColor && !m_range.atEnd()) {
                if (parsedLonghand[i])
                    return false; // Colors are only allowed in the last layer.
                continue;
            }
            if ((property == CSSPropertyBackgroundClip || property == CSSPropertyMaskClip || property == CSSPropertyWebkitMaskClip) && !parsedLonghand[i] && originValue) {
                longhands[i].append(originValue.releaseNonNull());
                continue;
            }
            if (clipIsBorderArea && (property == CSSPropertyBackgroundOrigin) && !parsedLonghand[i]) {
                longhands[i].append(CSSPrimitiveValue::create(CSSValueBorderBox));
                continue;
            }
            if (!parsedLonghand[i])
                longhands[i].append(Ref { CSSPrimitiveValue::implicitInitialValue() });
        }
    } while (consumeCommaIncludingWhitespace(m_range));
    if (!m_range.atEnd())
        return false;

    for (size_t i = 0; i < longhandCount; ++i) {
        CSSPropertyID property = shorthandProperties[i];
        if (longhands[i].size() == 1)
            addPropertyForCurrentShorthand(state, property, WTFMove(longhands[i][0]));
        else
            addPropertyForCurrentShorthand(state, property, CSSValueList::createCommaSeparated(WTFMove(longhands[i])));
    }
    return true;
}

bool CSSPropertyParser::consumeBackgroundPositionShorthand(const StylePropertyShorthand& shorthand, CSS::PropertyParserState& state)
{
    ASSERT(shorthand.id() == state.currentProperty);

    CSSValueListBuilder x;
    CSSValueListBuilder y;
    do {
        auto position = consumeBackgroundPositionCoordinates(m_range, state);
        if (!position)
            return false;
        x.append(WTFMove(position->x));
        y.append(WTFMove(position->y));
    } while (consumeCommaIncludingWhitespace(m_range));

    if (!m_range.atEnd())
        return false;

    RefPtr<CSSValue> resultX;
    RefPtr<CSSValue> resultY;
    if (x.size() == 1) {
        resultX = WTFMove(x[0]);
        resultY = WTFMove(y[0]);
    } else {
        resultX = CSSValueList::createCommaSeparated(WTFMove(x));
        resultY = CSSValueList::createCommaSeparated(WTFMove(y));
    }

    auto longhands = shorthand.properties();
    addPropertyForCurrentShorthand(state, longhands[0], resultX.releaseNonNull());
    addPropertyForCurrentShorthand(state, longhands[1], resultY.releaseNonNull());
    return true;
}

bool CSSPropertyParser::consumeWebkitBackgroundSizeShorthand(CSS::PropertyParserState& state)
{
    auto backgroundSize = consumeListSeparatedBy<',', OneOrMore, ListOptimization::SingleValue>(m_range, [](auto& range, auto& state) {
        return consumeSingleWebkitBackgroundSize(range, state);
    }, state);
    if (!backgroundSize || !m_range.atEnd())
        return false;
    addPropertyForCurrentShorthand(state, CSSPropertyBackgroundSize, backgroundSize.releaseNonNull());
    return true;
}

bool CSSPropertyParser::consumeMaskShorthand(CSS::PropertyParserState& state)
{
    if (!consumeBackgroundShorthand(maskShorthand(), state))
        return false;
    for (auto longhand : maskBorderShorthand())
        addPropertyForCurrentShorthand(state, longhand, nullptr);
    return true;
}

bool CSSPropertyParser::consumeMaskPositionShorthand(CSS::PropertyParserState& state)
{
    CSSValueListBuilder x;
    CSSValueListBuilder y;
    do {
        auto position = consumePositionCoordinates(m_range, state);
        if (!position)
            return false;
        x.append(WTFMove(position->x));
        y.append(WTFMove(position->y));
    } while (consumeCommaIncludingWhitespace(m_range));

    if (!m_range.atEnd())
        return false;

    RefPtr<CSSValue> resultX;
    RefPtr<CSSValue> resultY;
    if (x.size() == 1) {
        resultX = WTFMove(x[0]);
        resultY = WTFMove(y[0]);
    } else {
        resultX = CSSValueList::createCommaSeparated(WTFMove(x));
        resultY = CSSValueList::createCommaSeparated(WTFMove(y));
    }

    addPropertyForCurrentShorthand(state, CSSPropertyWebkitMaskPositionX, resultX.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyWebkitMaskPositionY, resultY.releaseNonNull());
    return true;
}

bool CSSPropertyParser::consumeOverflowShorthand(CSS::PropertyParserState& state)
{
    CSSValueID xValueID = m_range.consumeIncludingWhitespace().id();
    if (!CSSPropertyParsing::isKeywordValidForStyleProperty(CSSPropertyOverflowY, xValueID, state))
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

    if (!CSSPropertyParsing::isKeywordValidForStyleProperty(CSSPropertyOverflowY, yValueID, state))
        return false;
    if (!m_range.atEnd())
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyOverflowX, CSSPrimitiveValue::create(xValueID));
    addPropertyForCurrentShorthand(state, CSSPropertyOverflowY, CSSPrimitiveValue::create(yValueID));
    return true;
}

static bool isCustomIdentValue(const CSSValue& value)
{
    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    return primitiveValue && primitiveValue->isCustomIdent();
}

bool CSSPropertyParser::consumeGridItemPositionShorthand(const StylePropertyShorthand& shorthand, CSS::PropertyParserState& state)
{
    ASSERT(shorthand.id() == state.currentProperty);
    ASSERT(shorthand.length() == 2);

    RefPtr startValue = consumeGridLine(m_range, state);
    if (!startValue)
        return false;

    RefPtr<CSSValue> endValue;
    if (consumeSlashIncludingWhitespace(m_range)) {
        endValue = consumeGridLine(m_range, state);
        if (!endValue)
            return false;
    } else {
        endValue = isCustomIdentValue(*startValue) ? startValue : CSSPrimitiveValue::create(CSSValueAuto);
    }
    if (!m_range.atEnd())
        return false;

    auto longhands = shorthand.properties();
    addPropertyForCurrentShorthand(state, longhands[0], startValue.releaseNonNull());
    addPropertyForCurrentShorthand(state, longhands[1], endValue.releaseNonNull());
    return true;
}

bool CSSPropertyParser::consumeGridAreaShorthand(CSS::PropertyParserState& state)
{
    RefPtr rowStartValue = consumeGridLine(m_range, state);
    if (!rowStartValue)
        return false;
    RefPtr<CSSValue> columnStartValue;
    RefPtr<CSSValue> rowEndValue;
    RefPtr<CSSValue> columnEndValue;
    if (consumeSlashIncludingWhitespace(m_range)) {
        columnStartValue = consumeGridLine(m_range, state);
        if (!columnStartValue)
            return false;
        if (consumeSlashIncludingWhitespace(m_range)) {
            rowEndValue = consumeGridLine(m_range, state);
            if (!rowEndValue)
                return false;
            if (consumeSlashIncludingWhitespace(m_range)) {
                columnEndValue = consumeGridLine(m_range, state);
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

    addPropertyForCurrentShorthand(state, CSSPropertyGridRowStart, rowStartValue.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyGridColumnStart, columnStartValue.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyGridRowEnd, rowEndValue.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyGridColumnEnd, columnEndValue.releaseNonNull());
    return true;
}

bool CSSPropertyParser::consumeGridTemplateRowsAndAreasAndColumns(CSS::PropertyParserState& state)
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
        auto previousLineNames = std::exchange(lineNames, consumeGridLineNames(m_range, state));
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
        if (RefPtr value = consumeGridTrackSize(m_range, state))
            templateRows.append(value.releaseNonNull());
        else
            templateRows.append(CSSPrimitiveValue::create(CSSValueAuto));

        // This will handle the trailing/leading <custom-ident>* in the grammar.
        lineNames = consumeGridLineNames(m_range, state);
        if (lineNames)
            templateRows.append(*lineNames);
    } while (!m_range.atEnd() && !(m_range.peek().type() == DelimiterToken && m_range.peek().delimiter() == '/'));

    RefPtr<CSSValue> columnsValue;
    if (!m_range.atEnd()) {
        if (!consumeSlashIncludingWhitespace(m_range))
            return false;
        columnsValue = consumeGridTrackList(m_range, state, GridTemplateNoRepeat);
        if (!columnsValue || !m_range.atEnd())
            return false;
    } else {
        columnsValue = CSSPrimitiveValue::create(CSSValueNone);
    }
    addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateRows, CSSValueList::createSpaceSeparated(WTFMove(templateRows)));
    addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateColumns, columnsValue.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateAreas, CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount));
    return true;
}

bool CSSPropertyParser::consumeGridTemplateShorthand(CSS::PropertyParserState& state)
{
    CSSParserTokenRange rangeCopy = m_range;
    RefPtr<CSSValue> rowsValue = consumeIdent<CSSValueNone>(m_range);

    // 1- 'none' case.
    if (rowsValue && m_range.atEnd()) {
        addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateRows, CSSPrimitiveValue::create(CSSValueNone));
        addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateColumns, CSSPrimitiveValue::create(CSSValueNone));
        addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateAreas, CSSPrimitiveValue::create(CSSValueNone));
        return true;
    }

    // 2- <grid-template-rows> / <grid-template-columns>
    if (!rowsValue)
        rowsValue = consumeGridTrackList(m_range, state, GridTemplate);

    if (rowsValue) {
        if (!consumeSlashIncludingWhitespace(m_range))
            return false;
        RefPtr columnsValue = consumeGridTemplatesRowsOrColumns(m_range, state);
        if (!columnsValue || !m_range.atEnd())
            return false;

        addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateRows, rowsValue.releaseNonNull());
        addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateColumns, columnsValue.releaseNonNull());
        addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateAreas, CSSPrimitiveValue::create(CSSValueNone));
        return true;
    }

    // 3- [ <line-names>? <string> <track-size>? <line-names>? ]+ [ / <track-list> ]?
    m_range = rangeCopy;
    return consumeGridTemplateRowsAndAreasAndColumns(state);
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

bool CSSPropertyParser::consumeGridShorthand(CSS::PropertyParserState& state)
{
    ASSERT(shorthandForProperty(CSSPropertyGrid).length() == 6);

    CSSParserTokenRange rangeCopy = m_range;

    // 1- <grid-template>
    if (consumeGridTemplateShorthand(state)) {
        // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
        // The sub-properties not specified are set to their initial value, as normal for shorthands.
        addPropertyForCurrentShorthand(state, CSSPropertyGridAutoFlow, CSSPrimitiveValue::create(CSSValueRow));
        addPropertyForCurrentShorthand(state, CSSPropertyGridAutoColumns, CSSPrimitiveValue::create(CSSValueAuto));
        addPropertyForCurrentShorthand(state, CSSPropertyGridAutoRows, CSSPrimitiveValue::create(CSSValueAuto));

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
            autoRowsValue = consumeGridTrackList(m_range, state, GridAuto);
            if (!autoRowsValue)
                return false;
            if (!consumeSlashIncludingWhitespace(m_range))
                return false;
        }
        if (m_range.atEnd())
            return false;
        templateColumns = consumeGridTemplatesRowsOrColumns(m_range, state);
        if (!templateColumns)
            return false;
        templateRows = CSSPrimitiveValue::create(CSSValueNone);
        autoColumnsValue = CSSPrimitiveValue::create(CSSValueAuto);
    } else {
        // 3- <grid-template-rows> / [ auto-flow && dense? ] <grid-auto-columns>?
        templateRows = consumeGridTemplatesRowsOrColumns(m_range, state);
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
            autoColumnsValue = consumeGridTrackList(m_range, state, GridAuto);
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
    addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateColumns, templateColumns.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateRows, templateRows.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyGridTemplateAreas, CSSPrimitiveValue::create(CSSValueNone));
    addPropertyForCurrentShorthand(state, CSSPropertyGridAutoFlow, gridAutoFlow.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyGridAutoColumns, autoColumnsValue.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyGridAutoRows, autoRowsValue.releaseNonNull());

    return true;
}

bool CSSPropertyParser::consumeAlignShorthand(const StylePropertyShorthand& shorthand, CSS::PropertyParserState& state)
{
    // Used to implement the rules in CSS Align for the following shorthands:
    //   <'place-content'> https://drafts.csswg.org/css-align/#propdef-place-content
    //   <'place-items'>   https://drafts.csswg.org/css-align/#propdef-place-items
    //   <'place-self'>    https://drafts.csswg.org/css-align/#propdef-place-self
    //   <'gap'>           https://drafts.csswg.org/css-align/#propdef-gap

    ASSERT(shorthand.id() == state.currentProperty);
    ASSERT(shorthand.length() == 2);
    auto longhands = shorthand.properties();

    auto rangeCopy = m_range;

    RefPtr prop1 = parseStylePropertyLonghand(longhands[0], state);
    if (!prop1)
        return false;

    // If there are no more tokens, that prop2 should use re-use the original range. This is the equivalent of copying and validating prop1.
    if (m_range.atEnd())
        m_range = rangeCopy;

    RefPtr prop2 = parseStylePropertyLonghand(longhands[1], state);
    if (!prop2 || !m_range.atEnd())
        return false;

    addPropertyForCurrentShorthand(state, longhands[0], prop1.releaseNonNull());
    addPropertyForCurrentShorthand(state, longhands[1], prop2.releaseNonNull());
    return true;
}

bool CSSPropertyParser::consumeBlockStepShorthand(CSS::PropertyParserState& state)
{
    // https://drafts.csswg.org/css-rhythm/#block-step
    RefPtr<CSSValue> size;
    RefPtr<CSSValue> insert;
    RefPtr<CSSValue> align;
    RefPtr<CSSValue> round;

    for (unsigned propertiesParsed = 0; propertiesParsed < 4 && !m_range.atEnd(); ++propertiesParsed) {
        if (!size && (size = CSSPropertyParsing::consumeBlockStepSize(m_range, state)))
            continue;
        if (!insert && (insert = CSSPropertyParsing::consumeBlockStepInsert(m_range)))
            continue;
        if (!align && (align = CSSPropertyParsing::consumeBlockStepAlign(m_range)))
            continue;
        if (!round && (round = CSSPropertyParsing::consumeBlockStepRound(m_range)))
            continue;

        // There has to be at least one valid longhand.
        return false;
    }

    if (!m_range.atEnd())
        return false;

    // Fill in default values if one was missing.
    if (!size)
        size = CSSPrimitiveValue::create(CSSValueNone);
    if (!insert)
        insert = CSSPrimitiveValue::create(CSSValueMarginBox);
    if (!align)
        align = CSSPrimitiveValue::create(CSSValueAuto);
    if (!round)
        round = CSSPrimitiveValue::create(CSSValueUp);

    addPropertyForCurrentShorthand(state, CSSPropertyBlockStepSize, WTFMove(size));
    addPropertyForCurrentShorthand(state, CSSPropertyBlockStepInsert, WTFMove(insert));
    addPropertyForCurrentShorthand(state, CSSPropertyBlockStepAlign, WTFMove(align));
    addPropertyForCurrentShorthand(state, CSSPropertyBlockStepRound, WTFMove(round));
    return true;
}

bool CSSPropertyParser::consumeOverscrollBehaviorShorthand(CSS::PropertyParserState& state)
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

    addPropertyForCurrentShorthand(state, CSSPropertyOverscrollBehaviorX, WTFMove(overscrollBehaviorX));
    addPropertyForCurrentShorthand(state, CSSPropertyOverscrollBehaviorY, WTFMove(overscrollBehaviorY));
    return true;
}

bool CSSPropertyParser::consumeContainerShorthand(CSS::PropertyParserState& state)
{
    RefPtr name = parseStylePropertyLonghand(CSSPropertyContainerName, state);
    if (!name)
        return false;

    bool sawSlash = false;

    auto consumeSlashType = [&]() -> RefPtr<CSSValue> {
        if (m_range.atEnd())
            return nullptr;
        if (!consumeSlashIncludingWhitespace(m_range))
            return nullptr;
        sawSlash = true;
        return parseStylePropertyLonghand(CSSPropertyContainerType, state);
    };

    auto type = consumeSlashType();

    if (!m_range.atEnd() || (sawSlash && !type))
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyContainerName, name.releaseNonNull());
    addPropertyForCurrentShorthand(state, CSSPropertyContainerType, WTFMove(type));
    return true;
}

bool CSSPropertyParser::consumeContainIntrinsicSizeShorthand(CSS::PropertyParserState& state)
{
    ASSERT(shorthandForProperty(CSSPropertyContainIntrinsicSize).length() == 2);
    ASSERT(isExposed(CSSPropertyContainIntrinsicSize, &m_context.propertySettings));

    if (m_range.atEnd())
        return false;

    RefPtr containIntrinsicWidth = CSSPropertyParsing::consumeContainIntrinsicWidth(m_range, state);
    if (!containIntrinsicWidth)
        return false;

    RefPtr<CSSValue> containIntrinsicHeight;
    m_range.consumeWhitespace();
    if (m_range.atEnd())
        containIntrinsicHeight = containIntrinsicWidth;
    else {
        containIntrinsicHeight = CSSPropertyParsing::consumeContainIntrinsicHeight(m_range, state);
        m_range.consumeWhitespace();
        if (!m_range.atEnd() || !containIntrinsicHeight)
            return false;
    }

    addPropertyForCurrentShorthand(state, CSSPropertyContainIntrinsicWidth, WTFMove(containIntrinsicWidth));
    addPropertyForCurrentShorthand(state, CSSPropertyContainIntrinsicHeight, WTFMove(containIntrinsicHeight));
    return true;
}

bool CSSPropertyParser::consumeTransformOriginShorthand(CSS::PropertyParserState& state)
{
    if (auto resultXY = consumeOneOrTwoValuedPositionCoordinates(m_range, state)) {
        m_range.consumeWhitespace();
        bool atEnd = m_range.atEnd();
        auto resultZ = CSSPrimitiveValueResolver<CSS::Length<>>::consumeAndResolve(m_range, state);
        if ((!resultZ && !atEnd) || !m_range.atEnd())
            return false;
        addPropertyForCurrentShorthand(state, CSSPropertyTransformOriginX, WTFMove(resultXY->x));
        addPropertyForCurrentShorthand(state, CSSPropertyTransformOriginY, WTFMove(resultXY->y));
        addPropertyForCurrentShorthand(state, CSSPropertyTransformOriginZ, resultZ);

        return true;
    }
    return false;
}

bool CSSPropertyParser::consumePerspectiveOriginShorthand(CSS::PropertyParserState& state)
{
    if (auto result = consumePositionCoordinates(m_range, state)) {
        addPropertyForCurrentShorthand(state, CSSPropertyPerspectiveOriginX, WTFMove(result->x));
        addPropertyForCurrentShorthand(state, CSSPropertyPerspectiveOriginY, WTFMove(result->y));
        return true;
    }
    return false;
}

bool CSSPropertyParser::consumePrefixedPerspectiveShorthand(CSS::PropertyParserState& state)
{
    if (RefPtr value = CSSPropertyParsing::consumePerspective(m_range, state)) {
        addPropertyForCurrentShorthand(state, CSSPropertyPerspective, value.releaseNonNull());
        return m_range.atEnd();
    }

    if (auto perspective = CSSPrimitiveValueResolver<CSS::Number<CSS::Nonnegative>>::consumeAndResolve(m_range, state)) {
        addPropertyForCurrentShorthand(state, CSSPropertyPerspective, WTFMove(perspective));
        return m_range.atEnd();
    }

    return false;
}

bool CSSPropertyParser::consumeOffsetShorthand(CSS::PropertyParserState& state)
{
    // The offset shorthand is defined as:
    // [ <'offset-position'>?
    //   [ <'offset-path'>
    //     [ <'offset-distance'> || <'offset-rotate'> ]?
    //   ]?
    // ]!
    // [ / <'offset-anchor'> ]?

    // Parse out offset-position.
    auto offsetPosition = parseStylePropertyLonghand(CSSPropertyOffsetPosition, state);

    // Parse out offset-path.
    auto offsetPath = parseStylePropertyLonghand(CSSPropertyOffsetPath, state);

    // Either one of offset-position and offset-path must be present.
    if (!offsetPosition && !offsetPath)
        return false;

    // Only parse offset-distance and offset-rotate if offset-path is specified.
    RefPtr<CSSValue> offsetDistance;
    RefPtr<CSSValue> offsetRotate;
    if (offsetPath) {
        // Try to parse offset-distance first. If successful, parse the following offset-rotate.
        // Otherwise, parse in the reverse order.
        if ((offsetDistance = parseStylePropertyLonghand(CSSPropertyOffsetDistance, state)))
            offsetRotate = parseStylePropertyLonghand(CSSPropertyOffsetRotate, state);
        else {
            offsetRotate = parseStylePropertyLonghand(CSSPropertyOffsetRotate, state);
            offsetDistance = parseStylePropertyLonghand(CSSPropertyOffsetDistance, state);
        }
    }

    // Parse out offset-anchor. Only parse if the prefix slash is present.
    RefPtr<CSSValue> offsetAnchor;
    if (consumeSlashIncludingWhitespace(m_range)) {
        // offset-anchor must follow the slash.
        if (!(offsetAnchor = parseStylePropertyLonghand(CSSPropertyOffsetAnchor, state)))
            return false;
    }

    addPropertyForCurrentShorthand(state, CSSPropertyOffsetPath, WTFMove(offsetPath));
    addPropertyForCurrentShorthand(state, CSSPropertyOffsetDistance, WTFMove(offsetDistance));
    addPropertyForCurrentShorthand(state, CSSPropertyOffsetPosition, WTFMove(offsetPosition));
    addPropertyForCurrentShorthand(state, CSSPropertyOffsetAnchor, WTFMove(offsetAnchor));
    addPropertyForCurrentShorthand(state, CSSPropertyOffsetRotate, WTFMove(offsetRotate));

    return m_range.atEnd();
}

bool CSSPropertyParser::consumeListStyleShorthand(CSS::PropertyParserState& state)
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
        if (!position && (position = parseStylePropertyLonghand(CSSPropertyListStylePosition, state)))
            continue;

        if (!image && (image = parseStylePropertyLonghand(CSSPropertyListStyleImage, state)))
            continue;

        if (!type && (type = parseStylePropertyLonghand(CSSPropertyListStyleType, state)))
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

    addPropertyForCurrentShorthand(state, CSSPropertyListStylePosition, WTFMove(position));
    addPropertyForCurrentShorthand(state, CSSPropertyListStyleImage, WTFMove(image));
    addPropertyForCurrentShorthand(state, CSSPropertyListStyleType, WTFMove(type));
    return m_range.atEnd();
}

bool CSSPropertyParser::consumeLineClampShorthand(CSS::PropertyParserState& state)
{
    ASSERT(m_context.propertySettings.cssLineClampEnabled);

    if (m_range.peek().id() == CSSValueNone) {
        // Sets max-lines to none, continue to auto, and block-ellipsis to none.
        addPropertyForCurrentShorthand(state, CSSPropertyMaxLines, CSSPrimitiveValue::create(CSSValueNone));
        addPropertyForCurrentShorthand(state, CSSPropertyContinue, CSSPrimitiveValue::create(CSSValueAuto));
        addPropertyForCurrentShorthand(state, CSSPropertyBlockEllipsis, CSSPrimitiveValue::create(CSSValueNone));
        consumeIdent(m_range);
        return m_range.atEnd();
    }

    RefPtr<CSSValue> maxLines;
    RefPtr<CSSValue> blockEllipsis;

    for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !m_range.atEnd(); ++propertiesParsed) {
        if (!maxLines && (maxLines = CSSPropertyParsing::consumeMaxLines(m_range, state)))
            continue;
        if (!blockEllipsis && (blockEllipsis = CSSPropertyParsing::consumeBlockEllipsis(m_range)))
            continue;
        // There has to be at least one valid longhand.
        return false;
    }

    if (!blockEllipsis)
        blockEllipsis = CSSPrimitiveValue::create(CSSValueAuto);

    if (!maxLines)
        maxLines = CSSPrimitiveValue::create(CSSValueNone);

    addPropertyForCurrentShorthand(state, CSSPropertyMaxLines, WTFMove(maxLines));
    addPropertyForCurrentShorthand(state, CSSPropertyContinue, CSSPrimitiveValue::create(CSSValueDiscard));
    addPropertyForCurrentShorthand(state, CSSPropertyBlockEllipsis, WTFMove(blockEllipsis));
    return m_range.atEnd();
}

bool CSSPropertyParser::consumeTextBoxShorthand(CSS::PropertyParserState& state)
{
    if (m_range.peek().id() == CSSValueNormal) {
        // if the single keyword normal is specified, it sets text-box-trim to none and text-box-edge to auto.
        addPropertyForCurrentShorthand(state, CSSPropertyTextBoxTrim, CSSPrimitiveValue::create(CSSValueNone));
        addPropertyForCurrentShorthand(state, CSSPropertyTextBoxEdge, CSSPrimitiveValue::create(CSSValueAuto));
        consumeIdent(m_range);
        return m_range.atEnd();
    }

    RefPtr<CSSValue> textBoxTrim;
    RefPtr<CSSValue> textBoxEdge;

    for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !m_range.atEnd(); ++propertiesParsed) {
        if (!textBoxTrim && (textBoxTrim = CSSPropertyParsing::consumeTextBoxTrim(m_range)))
            continue;
        if (!textBoxEdge && (textBoxEdge = consumeTextBoxEdge(m_range, state)))
            continue;
        // There has to be at least one valid longhand.
        return false;
    }

    if (!m_range.atEnd())
        return false;

    // Omitting the text-box-edge value sets it to auto (the initial value)
    if (!textBoxEdge)
        textBoxEdge = CSSPrimitiveValue::create(CSSValueAuto);

    // Omitting the text-box-trim value sets it to both (not the initial value)
    if (!textBoxTrim)
        textBoxTrim = CSSPrimitiveValue::create(CSSValueTrimBoth);

    addPropertyForCurrentShorthand(state, CSSPropertyTextBoxTrim, WTFMove(textBoxTrim));
    addPropertyForCurrentShorthand(state, CSSPropertyTextBoxEdge, WTFMove(textBoxEdge));
    return true;
}

bool CSSPropertyParser::consumeTextWrapShorthand(CSS::PropertyParserState& state)
{
    RefPtr<CSSValue> mode;
    RefPtr<CSSValue> style;

    for (unsigned propertiesParsed = 0; propertiesParsed < 2 && !m_range.atEnd(); ++propertiesParsed) {
        if (!mode && (mode = CSSPropertyParsing::consumeTextWrapMode(m_range)))
            continue;
        if (m_context.propertySettings.cssTextWrapStyleEnabled && !style && (style = CSSPropertyParsing::consumeTextWrapStyle(m_range, state)))
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

    addPropertyForCurrentShorthand(state, CSSPropertyTextWrapMode, WTFMove(mode));
    addPropertyForCurrentShorthand(state, CSSPropertyTextWrapStyle, WTFMove(style));
    return true;
}


bool CSSPropertyParser::consumeWhiteSpaceShorthand(CSS::PropertyParserState& state)
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

    addPropertyForCurrentShorthand(state, CSSPropertyWhiteSpaceCollapse, WTFMove(whiteSpaceCollapse));
    addPropertyForCurrentShorthand(state, CSSPropertyTextWrapMode, WTFMove(textWrapMode));
    return true;
}


bool CSSPropertyParser::consumeAnimationRangeShorthand(CSS::PropertyParserState& state)
{
    CSSValueListBuilder startList;
    CSSValueListBuilder endList;
    do {
        RefPtr start = consumeSingleAnimationRangeStart(m_range, state);
        if (!start)
            return false;

        RefPtr<CSSValue> end;
        m_range.consumeWhitespace();
        if (m_range.atEnd() || m_range.peek().type() == CommaToken) {
            // From the spec: If <'animation-range-end'> is omitted and <'animation-range-start'> includes a component, then
            // animation-range-end is set to that same and 100%. Otherwise, any omitted longhand is set to its initial value.
            auto rangeEndValueForStartValue = [](const CSSValue& value) {
                RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
                if (primitiveValue && SingleTimelineRange::isOffsetValue(downcast<CSSPrimitiveValue>(value)))
                    return CSSPrimitiveValue::create(CSSValueNormal);
                return CSSPrimitiveValue::create(value.valueID());
            };

            if (RefPtr startPrimitiveValue = dynamicDowncast<CSSPrimitiveValue>(start))
                end = rangeEndValueForStartValue(*startPrimitiveValue);
            else {
                RefPtr startPair = downcast<CSSValuePair>(start);
                end = rangeEndValueForStartValue(startPair->protectedFirst());
            }
        } else {
            end = consumeSingleAnimationRangeEnd(m_range, state);
            m_range.consumeWhitespace();
            if (!end)
                return false;
        }
        startList.append(start.releaseNonNull());
        endList.append(end.releaseNonNull());
    } while (consumeCommaIncludingWhitespace(m_range));

    if (!m_range.atEnd())
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyAnimationRangeStart, CSSValueList::createCommaSeparated(WTFMove(startList)));
    addPropertyForCurrentShorthand(state, CSSPropertyAnimationRangeEnd, CSSValueList::createCommaSeparated(WTFMove(endList)));
    return true;
}

bool CSSPropertyParser::consumeScrollTimelineShorthand(CSS::PropertyParserState& state)
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

    addPropertyForCurrentShorthand(state, CSSPropertyScrollTimelineName, CSSValueList::createCommaSeparated(WTFMove(namesList)));
    if (!axesList.isEmpty())
        addPropertyForCurrentShorthand(state, CSSPropertyScrollTimelineAxis, CSSValueList::createCommaSeparated(WTFMove(axesList)));
    return true;
}

bool CSSPropertyParser::consumeViewTimelineShorthand(CSS::PropertyParserState& state)
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
            RefPtr insets = consumeSingleViewTimelineInsetItem(m_range, state);
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

    addPropertyForCurrentShorthand(state, CSSPropertyViewTimelineName, CSSValueList::createCommaSeparated(WTFMove(namesList)));
    addPropertyForCurrentShorthand(state, CSSPropertyViewTimelineAxis, CSSValueList::createCommaSeparated(WTFMove(axesList)));
    addPropertyForCurrentShorthand(state, CSSPropertyViewTimelineInset, CSSValueList::createCommaSeparated(WTFMove(insetsList)));
    return true;
}

bool CSSPropertyParser::consumePositionTryShorthand(CSS::PropertyParserState& state)
{
    auto order = parseStylePropertyLonghand(CSSPropertyPositionTryOrder, state);
    auto fallbacks = parseStylePropertyLonghand(CSSPropertyPositionTryFallbacks, state);
    if (!fallbacks)
        return false;

    addPropertyForCurrentShorthand(state, CSSPropertyPositionTryOrder, WTFMove(order));
    addPropertyForCurrentShorthand(state, CSSPropertyPositionTryFallbacks, WTFMove(fallbacks));
    return m_range.atEnd();
}

bool CSSPropertyParser::consumeMarkerShorthand(CSS::PropertyParserState& state)
{
    RefPtr marker = parseStylePropertyLonghand(CSSPropertyMarkerStart, state);
    if (!marker || !m_range.atEnd())
        return false;

    Ref markerRef = marker.releaseNonNull();
    addPropertyForCurrentShorthand(state, CSSPropertyMarkerStart, markerRef.copyRef());
    addPropertyForCurrentShorthand(state, CSSPropertyMarkerMid, markerRef.copyRef());
    addPropertyForCurrentShorthand(state, CSSPropertyMarkerEnd, WTFMove(markerRef));
    return true;
}

// MARK: - Property specific parsing dispatch

RefPtr<CSSValue> CSSPropertyParser::parseStylePropertyLonghand(CSSPropertyID property, CSS::PropertyParserState& state)
{
    return CSSPropertyParsing::parseStyleProperty(m_range, property, state);
}

bool CSSPropertyParser::parseStylePropertyShorthand(CSSPropertyID property, CSS::PropertyParserState& state)
{
    switch (property) {
    case CSSPropertyOverflow:
        return consumeOverflowShorthand(state);
    case CSSPropertyOverscrollBehavior:
        return consumeOverscrollBehaviorShorthand(state);
    case CSSPropertyFont:
        return consumeFontShorthand(state);
    case CSSPropertyFontVariant:
        return consumeFontVariantShorthand(state);
    case CSSPropertyFontSynthesis:
        return consumeFontSynthesisShorthand(state);
    case CSSPropertyBorderSpacing:
        return consumeBorderSpacingShorthand(state);
    case CSSPropertyColumns:
        return consumeColumnsShorthand(state);
    case CSSPropertyAnimation:
        return consumeAnimationShorthand(animationShorthand(), state);
    case CSSPropertyTransition:
        return consumeAnimationShorthand(transitionShorthandForParsing(), state);
    case CSSPropertyTextDecoration:
        return consumeTextDecorationShorthand(state);
    case CSSPropertyWebkitTextDecoration:
        return consumeShorthandGreedily(webkitTextDecorationShorthand(), state);
    case CSSPropertyInset:
        return consume4ValueShorthand(insetShorthand(), state);
    case CSSPropertyInsetBlock:
        return consume2ValueShorthand(insetBlockShorthand(), state);
    case CSSPropertyInsetInline:
        return consume2ValueShorthand(insetInlineShorthand(), state);
    case CSSPropertyMargin:
        return consume4ValueShorthand(marginShorthand(), state);
    case CSSPropertyMarginBlock:
        return consume2ValueShorthand(marginBlockShorthand(), state);
    case CSSPropertyMarginInline:
        return consume2ValueShorthand(marginInlineShorthand(), state);
    case CSSPropertyPadding:
        return consume4ValueShorthand(paddingShorthand(), state);
    case CSSPropertyPaddingBlock:
        return consume2ValueShorthand(paddingBlockShorthand(), state);
    case CSSPropertyPaddingInline:
        return consume2ValueShorthand(paddingInlineShorthand(), state);
    case CSSPropertyScrollMargin:
        return consume4ValueShorthand(scrollMarginShorthand(), state);
    case CSSPropertyScrollMarginBlock:
        return consume2ValueShorthand(scrollMarginBlockShorthand(), state);
    case CSSPropertyScrollMarginInline:
        return consume2ValueShorthand(scrollMarginInlineShorthand(), state);
    case CSSPropertyScrollPadding:
        return consume4ValueShorthand(scrollPaddingShorthand(), state);
    case CSSPropertyScrollPaddingBlock:
        return consume2ValueShorthand(scrollPaddingBlockShorthand(), state);
    case CSSPropertyScrollPaddingInline:
        return consume2ValueShorthand(scrollPaddingInlineShorthand(), state);
    case CSSPropertyTextEmphasis:
        return consumeShorthandGreedily(textEmphasisShorthand(), state);
    case CSSPropertyOutline:
        return consumeShorthandGreedily(outlineShorthand(), state);
    case CSSPropertyOffset:
        return consumeOffsetShorthand(state);
    case CSSPropertyBorderInline:
        return consumeBorderInlineShorthand(state);
    case CSSPropertyBorderInlineColor:
        return consume2ValueShorthand(borderInlineColorShorthand(), state);
    case CSSPropertyBorderInlineStyle:
        return consume2ValueShorthand(borderInlineStyleShorthand(), state);
    case CSSPropertyBorderInlineWidth:
        return consume2ValueShorthand(borderInlineWidthShorthand(), state);
    case CSSPropertyBorderInlineStart:
        return consumeShorthandGreedily(borderInlineStartShorthand(), state);
    case CSSPropertyBorderInlineEnd:
        return consumeShorthandGreedily(borderInlineEndShorthand(), state);
    case CSSPropertyBorderBlock:
        return consumeBorderBlockShorthand(state);
    case CSSPropertyBorderBlockColor:
        return consume2ValueShorthand(borderBlockColorShorthand(), state);
    case CSSPropertyBorderBlockStyle:
        return consume2ValueShorthand(borderBlockStyleShorthand(), state);
    case CSSPropertyBorderBlockWidth:
        return consume2ValueShorthand(borderBlockWidthShorthand(), state);
    case CSSPropertyBorderBlockStart:
        return consumeShorthandGreedily(borderBlockStartShorthand(), state);
    case CSSPropertyBorderBlockEnd:
        return consumeShorthandGreedily(borderBlockEndShorthand(), state);
    case CSSPropertyWebkitTextStroke:
        return consumeShorthandGreedily(webkitTextStrokeShorthand(), state);
    case CSSPropertyMarker:
        return consumeMarkerShorthand(state);
    case CSSPropertyFlex:
        return consumeFlexShorthand(state);
    case CSSPropertyFlexFlow:
        return consumeShorthandGreedily(flexFlowShorthand(), state);
    case CSSPropertyColumnRule:
        return consumeShorthandGreedily(columnRuleShorthand(), state);
    case CSSPropertyLineClamp:
        return consumeLineClampShorthand(state);
    case CSSPropertyListStyle:
        return consumeListStyleShorthand(state);
    case CSSPropertyBorderRadius:
        return consumeBorderRadiusShorthand(state);
    case CSSPropertyWebkitBorderRadius:
        return consumeWebkitBorderRadiusShorthand(state);
    case CSSPropertyBorderColor:
        return consume4ValueShorthand(borderColorShorthand(), state);
    case CSSPropertyBorderStyle:
        return consume4ValueShorthand(borderStyleShorthand(), state);
    case CSSPropertyBorderWidth:
        return consume4ValueShorthand(borderWidthShorthand(), state);
    case CSSPropertyBorderTop:
        return consumeShorthandGreedily(borderTopShorthand(), state);
    case CSSPropertyBorderRight:
        return consumeShorthandGreedily(borderRightShorthand(), state);
    case CSSPropertyBorderBottom:
        return consumeShorthandGreedily(borderBottomShorthand(), state);
    case CSSPropertyBorderLeft:
        return consumeShorthandGreedily(borderLeftShorthand(), state);
    case CSSPropertyBorder:
        return consumeBorderShorthand(state);
    case CSSPropertyCornerShape:
        return consume4ValueShorthand(cornerShapeShorthand(), state);
    case CSSPropertyBorderImage:
        return consumeBorderImageShorthand(state);
    case CSSPropertyWebkitBorderImage:
        return consumeWebkitBorderImageShorthand(state);
    case CSSPropertyMaskBorder:
        return consumeMaskBorderShorthand(state);
    case CSSPropertyWebkitMaskBoxImage:
        return consumeWebkitMaskBoxImageShorthand(state);
    case CSSPropertyPageBreakAfter:
        return consumePageBreakAfterShorthand(state);
    case CSSPropertyPageBreakBefore:
        return consumePageBreakBeforeShorthand(state);
    case CSSPropertyPageBreakInside:
        return consumePageBreakInsideShorthand(state);
    case CSSPropertyWebkitColumnBreakAfter:
        return consumeWebkitColumnBreakAfterShorthand(state);
    case CSSPropertyWebkitColumnBreakBefore:
        return consumeWebkitColumnBreakBeforeShorthand(state);
    case CSSPropertyWebkitColumnBreakInside:
        return consumeWebkitColumnBreakInsideShorthand(state);
    case CSSPropertyWebkitTextOrientation:
        return consumeWebkitTextOrientationShorthand(state);
    case CSSPropertyMaskPosition:
        return consumeMaskPositionShorthand(state);
    case CSSPropertyWebkitMaskPosition:
        return consumeBackgroundPositionShorthand(webkitMaskPositionShorthand(), state);
    case CSSPropertyBackgroundPosition:
        return consumeBackgroundPositionShorthand(backgroundPositionShorthand(), state);
    case CSSPropertyBackground:
        return consumeBackgroundShorthand(backgroundShorthand(), state);
    case CSSPropertyWebkitBackgroundSize:
        return consumeWebkitBackgroundSizeShorthand(state);
    case CSSPropertyMask:
        return consumeMaskShorthand(state);
    case CSSPropertyWebkitMask:
        return consumeBackgroundShorthand(webkitMaskShorthand(), state);
    case CSSPropertyTransformOrigin:
        return consumeTransformOriginShorthand(state);
    case CSSPropertyPerspectiveOrigin:
        return consumePerspectiveOriginShorthand(state);
    case CSSPropertyWebkitPerspective:
        return consumePrefixedPerspectiveShorthand(state);
    case CSSPropertyBlockStep:
        return consumeBlockStepShorthand(state);
    case CSSPropertyGap:
        return consumeAlignShorthand(gapShorthand(), state);
    case CSSPropertyGridColumn:
        return consumeGridItemPositionShorthand(gridColumnShorthand(), state);
    case CSSPropertyGridRow:
        return consumeGridItemPositionShorthand(gridRowShorthand(), state);
    case CSSPropertyGridArea:
        return consumeGridAreaShorthand(state);
    case CSSPropertyGridTemplate:
        return consumeGridTemplateShorthand(state);
    case CSSPropertyGrid:
        return consumeGridShorthand(state);
    case CSSPropertyPlaceContent:
        return consumeAlignShorthand(placeContentShorthand(), state);
    case CSSPropertyPlaceItems:
        return consumeAlignShorthand(placeItemsShorthand(), state);
    case CSSPropertyPlaceSelf:
        return consumeAlignShorthand(placeSelfShorthand(), state);
    case CSSPropertyTextDecorationSkip:
        return consumeTextDecorationSkipShorthand(state);
    case CSSPropertyContainer:
        return consumeContainerShorthand(state);
    case CSSPropertyContainIntrinsicSize:
        return consumeContainIntrinsicSizeShorthand(state);
    case CSSPropertyScrollTimeline:
        return consumeScrollTimelineShorthand(state);
    case CSSPropertyTextBox:
        return consumeTextBoxShorthand(state);
    case CSSPropertyTextWrap:
        return consumeTextWrapShorthand(state);
    case CSSPropertyViewTimeline:
        return consumeViewTimelineShorthand(state);
    case CSSPropertyWhiteSpace:
        return consumeWhiteSpaceShorthand(state);
    case CSSPropertyAnimationRange:
        return consumeAnimationRangeShorthand(state);
    case CSSPropertyPositionTry:
        return consumePositionTryShorthand(state);
    default:
        return false;
    }
}

} // namespace WebCore
