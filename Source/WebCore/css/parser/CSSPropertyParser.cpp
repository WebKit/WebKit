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

#include "CSSCustomPropertySyntax.h"
#include "CSSCustomPropertyValue.h"
#include "CSSMarkup.h"
#include "CSSParserContext.h"
#include "CSSParserFastPaths.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Image.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+ResolutionDefinitions.h"
#include "CSSPropertyParserConsumer+String.h"
#include "CSSPropertyParserConsumer+TimeDefinitions.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParserResult.h"
#include "CSSPropertyParserState.h"
#include "CSSPropertyParsing.h"
#include "CSSTokenizer.h"
#include "CSSTransformListValue.h"
#include "CSSVariableParser.h"
#include "CSSVariableReferenceValue.h"
#include "CSSWideKeyword.h"
#include "ComputedStyleDependencies.h"
#include "StyleBuilder.h"
#include "StyleBuilderConverter.h"
#include "StyleCustomProperty.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"
#include "StyleRuleType.h"
#include <memory>
#include <wtf/StdLibExtras.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringView.h>

namespace WebCore {

// MARK: - Custom properties

static std::pair<RefPtr<CSSValue>, CSSCustomPropertySyntax::Type> consumeCustomPropertyValueWithSyntax(CSSParserTokenRange&, CSS::PropertyParserState&, const CSSCustomPropertySyntax&);
static std::optional<Variant<Ref<const Style::CustomProperty>, CSSWideKeyword>> consumeTypedCustomPropertyValue(CSSParserTokenRange&, CSS::PropertyParserState&, const AtomString& name, const CSSCustomPropertySyntax&, Style::BuilderState&);

// MARK: - Root consumers

// Style properties.
static bool consumeStyleProperty(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID, IsImportant, StyleRuleType, CSS::PropertyParserResult&);

// @font-face descriptors.
static bool consumeFontFaceDescriptor(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID, CSS::PropertyParserResult&);

// @font-palette-values descriptors.
static bool consumeFontPaletteValuesDescriptor(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID, CSS::PropertyParserResult&);

// @counter-style descriptors.
static bool consumeCounterStyleDescriptor(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID, CSS::PropertyParserResult&);

// @keyframe descriptors.
static bool consumeKeyframeDescriptor(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID, IsImportant, CSS::PropertyParserResult&);

// @page descriptors.
static bool consumePageDescriptor(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID, IsImportant, CSS::PropertyParserResult&);

// @property descriptors.
static bool consumePropertyDescriptor(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID, CSS::PropertyParserResult&);

// @view-transition descriptors.
static bool consumeViewTransitionDescriptor(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID, CSS::PropertyParserResult&);

// @position-try descriptors.
static bool consumePositionTryDescriptor(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID, IsImportant, CSS::PropertyParserResult&);

// @function descriptors.
static bool consumeFunctionDescriptor(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID, CSS::PropertyParserResult&);

// @-internal-base-appearance descriptors.
static bool consumeInternalBaseAppearanceDescriptor(CSSParserTokenRange&, const CSSParserContext&, CSSPropertyID, IsImportant, CSS::PropertyParserResult&);

// MARK: - CSSPropertyID parsing

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

CSSPropertyID cssPropertyID(StringView string)
{
    auto length = string.length();
    if (!length)
        return CSSPropertyInvalid;
    if (length > maxCSSPropertyNameLength)
        return CSSPropertyInvalid;
    return string.is8Bit() ? cssPropertyID(string.span8()) : cssPropertyID(string.span16());
}

// MARK: - CSSValueID parsing

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

// MARK: - Custom property name validation

bool isCustomPropertyName(StringView propertyName)
{
    return propertyName.length() > 2 && propertyName.characterAt(0) == '-' && propertyName.characterAt(1) == '-';
}

// MARK: - CSS-wide keyword value consumer

static RefPtr<CSSPrimitiveValue> consumeCSSWideKeywordValue(CSSParserTokenRange& range)
{
    auto rangeCopy = range;
    auto valueID = rangeCopy.consumeIncludingWhitespace().id();
    if (!rangeCopy.atEnd())
        return nullptr;

    if (!isCSSWideKeyword(valueID))
        return nullptr;

    range = rangeCopy;
    return CSSPrimitiveValue::create(valueID);
}

static std::optional<CSSWideKeyword> consumeCSSWideKeyword(CSSParserTokenRange& range)
{
    auto rangeCopy = range;
    auto valueID = rangeCopy.consumeIncludingWhitespace().id();
    if (!rangeCopy.atEnd())
        return { };

    auto keyword = parseCSSWideKeyword(valueID);
    if (!keyword)
        return { };

    range = rangeCopy;
    return keyword;
}

// MARK: - function value consumer

static bool consumeFunctionArgument(CSSParserTokenRange& range, unsigned index, CSSPropertyID property, CSS::PropertyParserState& state, CSS::PropertyParserResult& result)
{
    auto argument = CSSPropertyParserHelpers::consumeArgument(range, index);
    if (!argument)
        return false;

    const auto& context = state.context;
    auto important = state.important;
    auto ruleType = state.currentRule;

    return consumeStyleProperty(*argument, context, property, important, ruleType, result);
}

static bool consumeInternalAutoBaseFunction(CSSParserTokenRange& range, CSSPropertyID property, CSS::PropertyParserState& state, CSS::PropertyParserResult& result)
{
    // -internal-auto-base() = -internal-auto-base( <auto value>, <base value> )

    if (!state.context.cssInternalAutoBaseParsingEnabled)
        return false;

    if (range.peek().functionId() != CSSValueInternalAutoBase)
        return false;

    auto args = CSSPropertyParserHelpers::consumeFunction(range);

    Vector<CSSProperty, 256> autoProperties;
    CSS::PropertyParserResult autoResult { autoProperties };

    if (!consumeFunctionArgument(args, 0, property, state, autoResult))
        return false;

    Vector<CSSProperty, 256> baseProperties;
    CSS::PropertyParserResult baseResult { baseProperties };

    if (!consumeFunctionArgument(args, 1, property, state, baseResult))
        return false;

    if (autoProperties.size() != baseProperties.size())
        return false;

    for (unsigned index = 0; index < autoProperties.size(); ++index) {
        const auto& autoProperty = autoProperties[index];
        const auto& baseProperty = baseProperties[index];

        Ref value = CSSFunctionValue::create(CSSValueInternalAutoBase, autoProperty.protectedValue(), baseProperty.protectedValue());
        result.addProperty(CSSProperty(autoProperty.metadata(), WTFMove(value)));
    }

    return true;
}

// MARK: - Parser entry points

using namespace CSSPropertyParserHelpers;

bool CSSPropertyParser::parseValue(CSSPropertyID property, IsImportant important, CSSParserTokenRange range, const CSSParserContext& context, ParsedPropertyVector& parsedProperties, StyleRuleType ruleType)
{
    int initialParsedPropertiesSize = parsedProperties.size();

    range.consumeWhitespace();

    CSS::PropertyParserResult result { parsedProperties };

    bool parseSuccess;
    switch (ruleType) {
    case StyleRuleType::CounterStyle:
        parseSuccess = consumeCounterStyleDescriptor(range, context, property, result);
        break;
    case StyleRuleType::FontFace:
        parseSuccess = consumeFontFaceDescriptor(range, context, property, result);
        break;
    case StyleRuleType::FontPaletteValues:
        parseSuccess = consumeFontPaletteValuesDescriptor(range, context, property, result);
        break;
    case StyleRuleType::Keyframe:
        parseSuccess = consumeKeyframeDescriptor(range, context, property, important, result);
        break;
    case StyleRuleType::Page:
        parseSuccess = consumePageDescriptor(range, context, property, important, result);
        break;
    case StyleRuleType::Property:
        parseSuccess = consumePropertyDescriptor(range, context, property, result);
        break;
    case StyleRuleType::ViewTransition:
        parseSuccess = consumeViewTransitionDescriptor(range, context, property, result);
        break;
    case StyleRuleType::PositionTry:
        parseSuccess = consumePositionTryDescriptor(range, context, property, important, result);
        break;
    case StyleRuleType::Function:
        parseSuccess = consumeFunctionDescriptor(range, context, property, result);
        break;
    case StyleRuleType::InternalBaseAppearance:
        parseSuccess = consumeInternalBaseAppearanceDescriptor(range, context, property, important, result);
        break;
    default:
        parseSuccess = consumeStyleProperty(range, context, property, important, ruleType, result);
        break;
    }

    if (!parseSuccess)
        parsedProperties.shrink(initialParsedPropertiesSize);

    return parseSuccess;
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

    auto range = tokenizer.tokenRange();
    range.consumeWhitespace();

    if (RefPtr value = consumeCSSWideKeywordValue(range))
        return value;

    RefPtr value = CSSPropertyParsing::parseStylePropertyLonghand(range, property, state);
    if (!value || !range.atEnd())
        return nullptr;

    return value;
}

RefPtr<CSSValue> CSSPropertyParser::parseStylePropertyLonghand(CSSPropertyID property, CSSParserTokenRange range, const CSSParserContext& context)
{
    ASSERT(!WebCore::isShorthand(property));

    range.consumeWhitespace();

    if (RefPtr value = consumeCSSWideKeywordValue(range))
        return value;

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Style,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr value = CSSPropertyParsing::parseStylePropertyLonghand(range, property, state);
    if (!value || !range.atEnd())
        return nullptr;

    return value;
}

RefPtr<CSSValue> CSSPropertyParser::parseCounterStyleDescriptor(CSSPropertyID property, const String& string, const CSSParserContext& context)
{
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



// MARK: - Custom properties

std::optional<Variant<Ref<const Style::CustomProperty>, CSSWideKeyword>> CSSPropertyParser::parseTypedCustomPropertyValue(const AtomString& name, const CSSCustomPropertySyntax& syntax, CSSParserTokenRange range, Style::BuilderState& builderState, const CSSParserContext& context)
{
    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Style,
        .currentProperty = CSSPropertyCustom,
        .important = IsImportant::No,
    };

    auto value = consumeTypedCustomPropertyValue(range, state, name, syntax, builderState);
    if (!value || !range.atEnd())
        return { };
    return value;
}

RefPtr<const Style::CustomProperty> CSSPropertyParser::parseTypedCustomPropertyInitialValue(const AtomString& name, const CSSCustomPropertySyntax& syntax, CSSParserTokenRange range, Style::BuilderState& builderState, const CSSParserContext& context)
{
    if (syntax.isUniversal())
        return CSSVariableParser::parseInitialValueForUniversalSyntax(name, range);

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Style,
        .currentProperty = CSSPropertyCustom,
        .important = IsImportant::No,
    };

    auto value = consumeTypedCustomPropertyValue(range, state, name, syntax, builderState);
    if (!value || !range.atEnd())
        return { };

    return WTF::switchOn(*value,
        [](const Ref<const Style::CustomProperty>& resolved) -> RefPtr<const Style::CustomProperty> {
            return resolved.copyRef();
        },
        [](const CSSWideKeyword&) -> RefPtr<const Style::CustomProperty> {
            return nullptr;
        }
    );
}

ComputedStyleDependencies CSSPropertyParser::collectParsedCustomPropertyValueDependencies(const CSSCustomPropertySyntax& syntax, CSSParserTokenRange range, const CSSParserContext& context)
{
    if (syntax.isUniversal())
        return { };

    range.consumeWhitespace();

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Style,
        .currentProperty = CSSPropertyCustom,
        .important = IsImportant::No,
    };

    auto [value, syntaxType] = consumeCustomPropertyValueWithSyntax(range, state, syntax);
    if (!value)
        return { };

    return value->computedStyleDependencies();
}

bool CSSPropertyParser::isValidCustomPropertyValueForSyntax(const CSSCustomPropertySyntax& syntax, CSSParserTokenRange range, const CSSParserContext& context)
{
    if (syntax.isUniversal())
        return true;

    range.consumeWhitespace();

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Style,
        .currentProperty = CSSPropertyCustom,
        .important = IsImportant::No,
    };

    return !!consumeCustomPropertyValueWithSyntax(range, state, syntax).first;
}

std::optional<CSSWideKeyword> CSSPropertyParser::parseCSSWideKeyword(CSSParserTokenRange range)
{
    return consumeCSSWideKeyword(range);
}

std::pair<RefPtr<CSSValue>, CSSCustomPropertySyntax::Type> consumeCustomPropertyValueWithSyntax(CSSParserTokenRange& range, CSS::PropertyParserState& state, const CSSCustomPropertySyntax& syntax)
{
    ASSERT(!syntax.isUniversal());

    auto rangeCopy = range;

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
            return CSSPropertyParsing::consumeTransformFunction(range, state);
        case CSSCustomPropertySyntax::Type::TransformList:
            return CSSPropertyParsing::consumeTransformList(range, state);
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
        if (RefPtr value = consumeComponent(range, component)) {
            if (range.atEnd())
                return { value, component.type };
        }
        range = rangeCopy;
    }
    return { nullptr, CSSCustomPropertySyntax::Type::Unknown };
}

std::optional<Variant<Ref<const Style::CustomProperty>, CSSWideKeyword>> consumeTypedCustomPropertyValue(CSSParserTokenRange& range, CSS::PropertyParserState& state, const AtomString& name, const CSSCustomPropertySyntax& syntax, Style::BuilderState& builderState)
{
    if (syntax.isUniversal())
        return { { Style::CustomProperty::createForVariableData(name, CSSVariableData::create(range.consumeAll())) } };

    range.consumeWhitespace();

    if (auto keyword = consumeCSSWideKeyword(range))
        return { { *keyword } };

    auto [value, syntaxType] = consumeCustomPropertyValueWithSyntax(range, state, syntax);
    if (!value)
        return { };

    auto resolveSyntaxValue = [&, syntaxType = syntaxType](const CSSValue& value) -> std::optional<Style::CustomProperty::Value> {
        switch (syntaxType) {
        case CSSCustomPropertySyntax::Type::LengthPercentage:
            return Style::toStyleFromCSSValue<Style::LengthPercentage<>>(builderState, downcast<CSSPrimitiveValue>(value));
        case CSSCustomPropertySyntax::Type::Length:
            return Style::toStyleFromCSSValue<Style::Length<>>(builderState, downcast<CSSPrimitiveValue>(value));
        case CSSCustomPropertySyntax::Type::Integer:
        case CSSCustomPropertySyntax::Type::Number:
            return Style::toStyleFromCSSValue<Style::Number<>>(builderState, downcast<CSSPrimitiveValue>(value));
        case CSSCustomPropertySyntax::Type::Percentage:
            return Style::toStyleFromCSSValue<Style::Percentage<>>(builderState, downcast<CSSPrimitiveValue>(value));
        case CSSCustomPropertySyntax::Type::Angle:
            return Style::toStyleFromCSSValue<Style::Angle<>>(builderState, downcast<CSSPrimitiveValue>(value));
        case CSSCustomPropertySyntax::Type::Time:
            return Style::toStyleFromCSSValue<Style::Time<>>(builderState, downcast<CSSPrimitiveValue>(value));
        case CSSCustomPropertySyntax::Type::Resolution:
            return Style::toStyleFromCSSValue<Style::Resolution<>>(builderState, downcast<CSSPrimitiveValue>(value));
        case CSSCustomPropertySyntax::Type::Color:
            return Style::toStyleFromCSSValue<Style::Color>(builderState, value, Style::ForVisitedLink::No);
        case CSSCustomPropertySyntax::Type::Image: {
            auto styleImage = builderState.createStyleImage(value);
            if (!styleImage)
                return { };
            return Style::ImageWrapper { styleImage.releaseNonNull() };
        }
        case CSSCustomPropertySyntax::Type::URL:
            return Style::toStyle(downcast<CSSURLValue>(value).url(), builderState);
        case CSSCustomPropertySyntax::Type::CustomIdent:
            return CustomIdentifier { AtomString { downcast<CSSPrimitiveValue>(value).stringValue() } };
        case CSSCustomPropertySyntax::Type::String:
            return downcast<CSSPrimitiveValue>(value).stringValue();
        case CSSCustomPropertySyntax::Type::TransformFunction:
        case CSSCustomPropertySyntax::Type::TransformList:
            return Style::toStyleFromCSSValue<Style::TransformFunction>(builderState, value);
        case CSSCustomPropertySyntax::Type::Unknown:
            return { };
        }
        ASSERT_NOT_REACHED();
        return { };
    };

    if (is<CSSValueList>(value.get()) || is<CSSTransformListValue>(value.get())) {
        Ref valueList = downcast<CSSValueContainingVector>(value.releaseNonNull());
        auto syntaxValueList = Style::CustomProperty::ValueList { { }, valueList->separator() };
        for (Ref listValue : valueList.get()) {
            auto syntaxValue = resolveSyntaxValue(listValue);
            if (!syntaxValue)
                return { };
            syntaxValueList.values.append(WTFMove(*syntaxValue));
        }
        return { { Style::CustomProperty::createForValueList(name, WTFMove(syntaxValueList)) } };
    };

    auto syntaxValue = resolveSyntaxValue(*value);
    if (!syntaxValue)
        return { };

    return { { Style::CustomProperty::createForValue(name, WTFMove(*syntaxValue)) } };
}

// MARK: - Root consumers

bool consumeStyleProperty(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, IsImportant important, StyleRuleType ruleType, CSS::PropertyParserResult& result)
{
    if (CSSProperty::isDescriptorOnly(property))
        return false;

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = ruleType,
        .currentProperty = property,
        .important = important,
    };

    if (consumeInternalAutoBaseFunction(range, property, state, result))
        return true;

    if (WebCore::isShorthand(property)) {
        auto rangeCopy = range;
        if (RefPtr keywordValue = consumeCSSWideKeywordValue(rangeCopy)) {
            result.addPropertyForAllLonghandsOfCurrentShorthand(state, WTFMove(keywordValue));
            range = rangeCopy;
            return true;
        }

        auto originalRange = range;

        if (CSSPropertyParsing::parseStylePropertyShorthand(range, property, state, result))
            return true;

        if (CSSVariableParser::containsValidVariableReferences(originalRange, context)) {
            result.addPropertyForAllLonghandsOfCurrentShorthand(state, CSSPendingSubstitutionValue::create(property, CSSVariableReferenceValue::create(originalRange, context)));
            return true;
        }
    } else {
        auto rangeCopy = range;
        if (RefPtr keywordValue = consumeCSSWideKeywordValue(rangeCopy)) {
            result.addProperty(state, property, CSSPropertyInvalid, WTFMove(keywordValue), important);
            range = rangeCopy;
            return true;
        }

        auto originalRange = range;

        RefPtr parsedValue = CSSPropertyParsing::parseStylePropertyLonghand(range, property, state);
        if (parsedValue && range.atEnd()) {
            result.addProperty(state, property, CSSPropertyInvalid, WTFMove(parsedValue), important);
            return true;
        }

        if (CSSVariableParser::containsValidVariableReferences(originalRange, context)) {
            result.addProperty(state, property, CSSPropertyInvalid, CSSVariableReferenceValue::create(originalRange, context), important);
            return true;
        }
    }

    return false;
}

bool consumeFontFaceDescriptor(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, CSS::PropertyParserResult& result)
{
    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::FontFace,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr parsedValue = CSSPropertyParsing::parseFontFaceDescriptor(range, property, state);
    if (!parsedValue || !range.atEnd())
        return false;

    result.addProperty(state, property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
    return true;
}

bool consumeFontPaletteValuesDescriptor(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, CSS::PropertyParserResult& result)
{
    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::FontPaletteValues,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr parsedValue = CSSPropertyParsing::parseFontPaletteValuesDescriptor(range, property, state);
    if (!parsedValue || !range.atEnd())
        return false;

    result.addProperty(state, property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
    return true;
}

bool consumeCounterStyleDescriptor(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, CSS::PropertyParserResult& result)
{
    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::CounterStyle,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr parsedValue = CSSPropertyParsing::parseCounterStyleDescriptor(range, property, state);
    if (!parsedValue || !range.atEnd())
        return false;

    result.addProperty(state, property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
    return true;
}

bool consumeKeyframeDescriptor(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, IsImportant important, CSS::PropertyParserResult& result)
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
        return consumeStyleProperty(range, context, property, important, StyleRuleType::Keyframe, result);
    }
}

bool consumePageDescriptor(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, IsImportant important, CSS::PropertyParserResult& result)
{
    // Does not apply in @page per-spec.
    if (property == CSSPropertyPage)
        return false;

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Page,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    if (RefPtr parsedValue = CSSPropertyParsing::parsePageDescriptor(range, property, state)) {
        if (!range.atEnd())
            return false;

        result.addProperty(state, property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
        return true;
    }

    return consumeStyleProperty(range, context, property, important, StyleRuleType::Page, result);
}

bool consumePropertyDescriptor(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, CSS::PropertyParserResult& result)
{
    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Property,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr parsedValue = CSSPropertyParsing::parsePropertyDescriptor(range, property, state);
    if (!parsedValue || !range.atEnd())
        return false;

    result.addProperty(state, property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
    return true;
}

bool consumeViewTransitionDescriptor(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, CSS::PropertyParserResult& result)
{
    ASSERT(context.propertySettings.crossDocumentViewTransitionsEnabled);

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::ViewTransition,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr parsedValue = CSSPropertyParsing::parseViewTransitionDescriptor(range, property, state);
    if (!parsedValue || !range.atEnd())
        return false;

    result.addProperty(state, property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
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

bool consumePositionTryDescriptor(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, IsImportant important, CSS::PropertyParserResult& result)
{
    ASSERT(context.propertySettings.cssAnchorPositioningEnabled);

    // Per spec, !important is not allowed and makes the whole declaration invalid.
    if (important == IsImportant::Yes)
        return false;

    if (!propertyAllowedInPositionTryRule(property))
        return false;

    return consumeStyleProperty(range, context, property, important, StyleRuleType::PositionTry, result);
}

bool consumeFunctionDescriptor(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, CSS::PropertyParserResult& result)
{
    ASSERT(context.propertySettings.cssFunctionAtRuleEnabled);

    auto state = CSS::PropertyParserState {
        .context = context,
        .currentRule = StyleRuleType::Function,
        .currentProperty = property,
        .important = IsImportant::No,
    };

    RefPtr parsedValue = CSSPropertyParsing::parseFunctionDescriptor(range, property, state);
    if (!parsedValue || !range.atEnd())
        return false;

    result.addProperty(state, property, CSSPropertyInvalid, WTFMove(parsedValue), IsImportant::No);
    return true;
}

bool consumeInternalBaseAppearanceDescriptor(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID property, IsImportant important, CSS::PropertyParserResult& result)
{
    ASSERT(context.mode == UASheetMode);

    if (property == CSSPropertyAppearance)
        return false;

    return consumeStyleProperty(range, context, property, important, StyleRuleType::InternalBaseAppearance, result);
}

} // namespace WebCore
