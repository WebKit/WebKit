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
#include "CSSParserFastPaths.h"

#include "CSSFunctionValue.h"
#include "CSSParserContext.h"
#include "CSSParserIdioms.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSPropertyParsing.h"
#include "CSSTransformListValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "HashTools.h"
#include "StyleColor.h"
#include "StylePropertyShorthand.h"

namespace WebCore {

bool CSSParserFastPaths::isSimpleLengthPropertyID(CSSPropertyID propertyId, bool& acceptsNegativeNumbers)
{
    switch (propertyId) {
    case CSSPropertyFontSize:
    case CSSPropertyHeight:
    case CSSPropertyWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyInlineSize:
    case CSSPropertyBlockSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyMinBlockSize:
    case CSSPropertyPaddingBlockEnd:
    case CSSPropertyPaddingBlockStart:
    case CSSPropertyPaddingInlineEnd:
    case CSSPropertyPaddingInlineStart:
    case CSSPropertyR:
    case CSSPropertyRx:
    case CSSPropertyRy:
    case CSSPropertyShapeMargin:
        acceptsNegativeNumbers = false;
        return true;
    case CSSPropertyBottom:
    case CSSPropertyCx:
    case CSSPropertyCy:
    case CSSPropertyLeft:
    case CSSPropertyInsetBlockEnd:
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetInlineEnd:
    case CSSPropertyInsetInlineStart:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyRight:
    case CSSPropertyTop:
    case CSSPropertyMarginBlockEnd:
    case CSSPropertyMarginBlockStart:
    case CSSPropertyMarginInlineEnd:
    case CSSPropertyMarginInlineStart:
    case CSSPropertyX:
    case CSSPropertyY:
        acceptsNegativeNumbers = true;
        return true;
    default:
        return false;
    }
}

template<typename CharacterType> static inline std::optional<double> parseCSSNumber(std::span<const CharacterType> characters)
{
    // The charactersToDouble() function allows a trailing '.' but that is not allowed in CSS number values.
    if (!characters.empty() && characters.back() == '.')
        return std::nullopt;
    // FIXME: If we don't want to skip over leading spaces, we should use parseDouble, not charactersToDouble.
    bool ok;
    auto number = charactersToDouble(characters, &ok);
    if (!ok)
        return std::nullopt;
    return number;
}

template <typename CharacterType>
static inline bool parseSimpleLength(std::span<const CharacterType> characters, CSSUnitType& unit, double& number)
{
    if (characters.size() > 2 && isASCIIAlphaCaselessEqual(characters[characters.size() - 2], 'p') && isASCIIAlphaCaselessEqual(characters[characters.size() - 1], 'x')) {
        characters = characters.first(characters.size() - 2);
        unit = CSSUnitType::CSS_PX;
    } else if (!characters.empty() && characters.back() == '%') {
        characters = characters.first(characters.size() - 1);
        unit = CSSUnitType::CSS_PERCENTAGE;
    }

    auto parsedNumber = parseCSSNumber(characters);
    number = parsedNumber.value_or(0);
    return parsedNumber.has_value();
}

template <typename CharacterType>
static inline bool parseSimpleAngle(std::span<const CharacterType> characters, CSSUnitType& unit, double& number)
{
    // Just support deg and rad for now.
    if (characters.size() < 4)
        return false;

    if (isASCIIAlphaCaselessEqual(characters[characters.size() - 3], 'd') && isASCIIAlphaCaselessEqual(characters[characters.size() - 2], 'e') && isASCIIAlphaCaselessEqual(characters[characters.size() - 1], 'g')) {
        characters = characters.first(characters.size() - 3);
        unit = CSSUnitType::CSS_DEG;
    } else if (isASCIIAlphaCaselessEqual(characters[characters.size() - 3], 'r') && isASCIIAlphaCaselessEqual(characters[characters.size() - 2], 'a') && isASCIIAlphaCaselessEqual(characters[characters.size() - 1], 'd')) {
        characters = characters.first(characters.size() - 3);
        unit = CSSUnitType::CSS_RAD;
    } else
        return false;

    auto parsedNumber = parseCSSNumber(characters);
    number = parsedNumber.value_or(0);
    return parsedNumber.has_value();
}

static RefPtr<CSSValue> parseSimpleLengthValue(CSSPropertyID propertyId, StringView string, CSSParserMode cssParserMode)
{
    ASSERT(!string.isEmpty());
    bool acceptsNegativeNumbers = false;

    // In @viewport, width and height are shorthands, not simple length values.
    if (isCSSViewportParsingEnabledForMode(cssParserMode) || !CSSParserFastPaths::isSimpleLengthPropertyID(propertyId, acceptsNegativeNumbers))
        return nullptr;

    double number;
    CSSUnitType unit = CSSUnitType::CSS_NUMBER;

    if (string.is8Bit()) {
        if (!parseSimpleLength(string.span8(), unit, number))
            return nullptr;
    } else {
        if (!parseSimpleLength(string.span16(), unit, number))
            return nullptr;
    }

    if (unit == CSSUnitType::CSS_NUMBER) {
        if (number && cssParserMode != SVGAttributeMode)
            return nullptr;
        unit = CSSUnitType::CSS_PX;
    }

    if (number < 0 && !acceptsNegativeNumbers)
        return nullptr;
    if (std::isinf(number))
        return nullptr;

    return CSSPrimitiveValue::create(number, unit);
}

// Returns the number of characters which form a valid double
// and are terminated by the given terminator character
template <typename CharacterType>
static int checkForValidDouble(const CharacterType* string, const CharacterType* end, char terminator)
{
    int length = end - string;
    if (length < 1)
        return 0;

    bool decimalMarkSeen = false;
    int processedLength = 0;

    for (int i = 0; i < length; ++i) {
        if (string[i] == terminator) {
            processedLength = i;
            break;
        }
        if (!isASCIIDigit(string[i])) {
            if (!decimalMarkSeen && string[i] == '.')
                decimalMarkSeen = true;
            else
                return 0;
        }
    }

    if (decimalMarkSeen && processedLength == 1)
        return 0;

    return processedLength;
}

// Returns the number of characters consumed for parsing a valid double
// terminated by the given terminator character
template <typename CharacterType>
static int parseDouble(const CharacterType* string, const CharacterType* end, char terminator, double& value)
{
    int length = checkForValidDouble(string, end, terminator);
    if (!length)
        return 0;

    int position = 0;
    double localValue = 0;

    // The consumed characters here are guaranteed to be
    // ASCII digits with or without a decimal mark
    for (; position < length; ++position) {
        if (string[position] == '.')
            break;
        localValue = localValue * 10 + string[position] - '0';
    }

    if (++position == length) {
        value = localValue;
        return length;
    }

    double fraction = 0;
    double scale = 1;

    double maxScale = 1000000;
    while (position < length && scale < maxScale) {
        fraction = fraction * 10 + string[position++] - '0';
        scale *= 10;
    }

    value = localValue + fraction / scale;
    return length;
}

template <typename CharacterType>
static std::optional<uint8_t> parseColorIntOrPercentage(const CharacterType*& string, const CharacterType* end, char terminator, CSSUnitType& expect)
{
    auto* current = string;
    double localValue = 0;
    bool negative = false;
    while (current != end && isASCIIWhitespace<CharacterType>(*current))
        current++;
    if (current != end && *current == '-') {
        negative = true;
        current++;
    }
    if (current == end || !isASCIIDigit(*current))
        return std::nullopt;
    while (current != end && isASCIIDigit(*current)) {
        double newValue = localValue * 10 + *current++ - '0';
        if (newValue >= 255) {
            // Clamp values at 255.
            localValue = 255;
            while (current != end && isASCIIDigit(*current))
                ++current;
            break;
        }
        localValue = newValue;
    }

    if (current == end)
        return std::nullopt;

    if (expect == CSSUnitType::CSS_NUMBER && (*current == '.' || *current == '%'))
        return std::nullopt;

    if (*current == '.') {
        // We already parsed the integral part, try to parse
        // the fraction part of the percentage value.
        double percentage = 0;
        int numCharactersParsed = parseDouble(current, end, '%', percentage);
        if (!numCharactersParsed)
            return std::nullopt;
        current += numCharactersParsed;
        if (*current != '%')
            return std::nullopt;
        localValue += percentage;
    }

    if (expect == CSSUnitType::CSS_PERCENTAGE && *current != '%')
        return std::nullopt;

    if (*current == '%') {
        expect = CSSUnitType::CSS_PERCENTAGE;
        localValue = localValue / 100.0 * 255.0;
        // Clamp values at 255 for percentages over 100%
        if (localValue > 255)
            localValue = 255;
        current++;
    } else
        expect = CSSUnitType::CSS_NUMBER;

    while (current != end && isASCIIWhitespace<CharacterType>(*current))
        current++;
    if (current == end || *current++ != terminator)
        return std::nullopt;
    string = current;

    // Clamp negative values at zero.
    ASSERT(localValue <= 255);
    return negative ? 0 : convertPrescaledSRGBAFloatToSRGBAByte(localValue);
}

template <typename CharacterType>
static inline bool isTenthAlpha(const CharacterType* string, int length)
{
    // "0.X"
    if (length == 3 && string[0] == '0' && string[1] == '.' && isASCIIDigit(string[2]))
        return true;

    // ".X"
    if (length == 2 && string[0] == '.' && isASCIIDigit(string[1]))
        return true;

    return false;
}

template <typename CharacterType>
static inline std::optional<uint8_t> parseAlphaValue(const CharacterType*& string, const CharacterType* end, char terminator)
{
    while (string != end && isASCIIWhitespace<CharacterType>(*string))
        string++;

    bool negative = false;

    if (string != end && *string == '-') {
        negative = true;
        string++;
    }

    int length = end - string;
    if (length < 2)
        return std::nullopt;

    if (string[length - 1] != terminator || !isASCIIDigit(string[length - 2]))
        return std::nullopt;

    if (string[0] != '0' && string[0] != '1' && string[0] != '.') {
        if (checkForValidDouble(string, end, terminator)) {
            string = end;
            return negative ? 0 : 255;
        }
        return std::nullopt;
    }

    if (length == 2 && string[0] != '.') {
        uint8_t result = !negative && string[0] == '1' ? 255 : 0;
        string = end;
        return result;
    }

    if (isTenthAlpha(string, length - 1)) {
        static constexpr uint8_t tenthAlphaValues[] = { 0, 26, 51, 77, 102, 128, 153, 179, 204, 230 };
        uint8_t result = negative ? 0 : tenthAlphaValues[string[length - 2] - '0'];
        string = end;
        return result;
    }

    double alpha = 0;
    if (!parseDouble(string, end, terminator, alpha))
        return std::nullopt;

    string = end;
    return negative ? 0 : convertFloatAlphaTo<uint8_t>(alpha);
}

template <typename CharacterType>
static inline bool mightBeRGBA(const CharacterType* characters, unsigned length)
{
    if (length < 5)
        return false;
    return characters[4] == '('
        && isASCIIAlphaCaselessEqual(characters[0], 'r')
        && isASCIIAlphaCaselessEqual(characters[1], 'g')
        && isASCIIAlphaCaselessEqual(characters[2], 'b')
        && isASCIIAlphaCaselessEqual(characters[3], 'a');
}

template <typename CharacterType>
static inline bool mightBeRGB(const CharacterType* characters, unsigned length)
{
    if (length < 4)
        return false;
    return characters[3] == '('
        && isASCIIAlphaCaselessEqual(characters[0], 'r')
        && isASCIIAlphaCaselessEqual(characters[1], 'g')
        && isASCIIAlphaCaselessEqual(characters[2], 'b');
}

static std::optional<SRGBA<uint8_t>> finishParsingHexColor(uint32_t value, unsigned length)
{
    switch (length) {
    case 3:
        // #abc converts to #aabbcc
        // FIXME: Replace conversion to PackedColor::ARGB with simpler bit math to construct
        // the SRGBA<uint8_t> directly.
        return asSRGBA(PackedColor::ARGB {
               0xFF000000
            | (value & 0xF00) << 12 | (value & 0xF00) << 8
            | (value & 0xF0) << 8 | (value & 0xF0) << 4
            | (value & 0xF) << 4 | (value & 0xF) });
    case 4:
        // #abcd converts to ddaabbcc since alpha bytes are the high bytes.
        // FIXME: Replace conversion to PackedColor::ARGB with simpler bit math to construct
        // the SRGBA<uint8_t> directly.
        return asSRGBA(PackedColor::ARGB {
              (value & 0xF) << 28 | (value & 0xF) << 24
            | (value & 0xF000) << 8 | (value & 0xF000) << 4
            | (value & 0xF00) << 4 | (value & 0xF00)
            | (value & 0xF0) | (value & 0xF0) >> 4 });
    case 6:
        // FIXME: Replace conversion to PackedColor::ARGB with simpler bit math to construct
        // the SRGBA<uint8_t> directly.
        return asSRGBA(PackedColor::ARGB { 0xFF000000 | value });
    case 8:
        return asSRGBA(PackedColor::RGBA { value });
    }
    return std::nullopt;
}

template<typename CharacterType> static std::optional<SRGBA<uint8_t>> parseHexColorInternal(const CharacterType* characters, unsigned length)
{
    if (length != 3 && length != 4 && length != 6 && length != 8)
        return std::nullopt;
    uint32_t value = 0;
    for (unsigned i = 0; i < length; ++i) {
        auto digit = characters[i];
        if (!isASCIIHexDigit(digit))
            return std::nullopt;
        value <<= 4;
        value |= toASCIIHexValue(digit);
    }
    return finishParsingHexColor(value, length);
}

template<typename CharacterType> static std::optional<SRGBA<uint8_t>> parseNumericColor(const CharacterType* characters, unsigned length, bool strict)
{
    if (length >= 4 && characters[0] == '#') {
        if (auto hexColor = parseHexColorInternal(characters + 1, length - 1))
            return *hexColor;
    }

    if (!strict && (length == 3 || length == 6)) {
        if (auto hexColor = parseHexColorInternal(characters, length))
            return *hexColor;
    }

    auto expect = CSSUnitType::CSS_UNKNOWN;

    // Try rgba() syntax.
    if (mightBeRGBA(characters, length)) {
        auto current = characters + 5;
        auto end = characters + length;
        auto red = parseColorIntOrPercentage(current, end, ',', expect);
        if (!red)
            return std::nullopt;
        auto green = parseColorIntOrPercentage(current, end, ',', expect);
        if (!green)
            return std::nullopt;
        auto blue = parseColorIntOrPercentage(current, end, ',', expect);
        if (!blue)
            return std::nullopt;
        auto alpha = parseAlphaValue(current, end, ')');
        if (!alpha)
            return std::nullopt;
        if (current != end)
            return std::nullopt;
        return SRGBA<uint8_t> { *red, *green, *blue, *alpha };
    }

    // Try rgb() syntax.
    if (mightBeRGB(characters, length)) {
        auto current = characters + 4;
        auto end = characters + length;
        auto red = parseColorIntOrPercentage(current, end, ',', expect);
        if (!red)
            return std::nullopt;
        auto green = parseColorIntOrPercentage(current, end, ',', expect);
        if (!green)
            return std::nullopt;
        auto blue = parseColorIntOrPercentage(current, end, ')', expect);
        if (!blue)
            return std::nullopt;
        if (current != end)
            return std::nullopt;
        return SRGBA<uint8_t> { *red, *green, *blue };
    }

    return std::nullopt;
}

static std::optional<SRGBA<uint8_t>> parseNumericColor(StringView string, const CSSParserContext& context)
{
    bool strict = !isQuirksModeBehavior(context.mode);
    if (string.is8Bit())
        return parseNumericColor(string.characters8(), string.length(), strict);
    return parseNumericColor(string.characters16(), string.length(), strict);
}

static RefPtr<CSSValue> parseColor(StringView string, const CSSParserContext& context)
{
    ASSERT(!string.isEmpty());
    auto valueID = cssValueKeywordID(string);
    if (StyleColor::isColorKeyword(valueID)) {
        if (!isColorKeywordAllowedInMode(valueID, context.mode))
            return nullptr;
        return CSSPrimitiveValue::create(valueID);
    }
    if (auto color = parseNumericColor(string, context))
        return CSSValuePool::singleton().createColorValue(*color);
    return nullptr;
}

static std::optional<SRGBA<uint8_t>> finishParsingNamedColor(char* buffer, unsigned length)
{
    buffer[length] = '\0';
    auto namedColor = findColor(buffer, length);
    if (!namedColor)
        return std::nullopt;
    return asSRGBA(PackedColor::ARGB { namedColor->ARGBValue });
}

template<typename CharacterType> static std::optional<SRGBA<uint8_t>> parseNamedColorInternal(const CharacterType* characters, unsigned length)
{
    char buffer[64]; // Easily big enough for the longest color name.
    if (length > sizeof(buffer) - 1)
        return std::nullopt;
    for (unsigned i = 0; i < length; ++i) {
        auto character = characters[i];
        if (!character || !isASCII(character))
            return std::nullopt;
        buffer[i] = toASCIILower(static_cast<char>(character));
    }
    return finishParsingNamedColor(buffer, length);
}

template<typename CharacterType> static std::optional<SRGBA<uint8_t>> parseSimpleColorInternal(const CharacterType* characters, unsigned length, bool strict)
{
    if (auto color = parseNumericColor(characters, length, strict))
        return color;
    return parseNamedColorInternal(characters, length);
}

std::optional<SRGBA<uint8_t>> CSSParserFastPaths::parseSimpleColor(StringView string, bool strict)
{
    if (string.is8Bit())
        return parseSimpleColorInternal(string.characters8(), string.length(), strict);
    return parseSimpleColorInternal(string.characters16(), string.length(), strict);
}

std::optional<SRGBA<uint8_t>> CSSParserFastPaths::parseHexColor(StringView string)
{
    if (string.is8Bit())
        return parseHexColorInternal(string.characters8(), string.length());
    return parseHexColorInternal(string.characters16(), string.length());
}

std::optional<SRGBA<uint8_t>> CSSParserFastPaths::parseNamedColor(StringView string)
{
    if (string.is8Bit())
        return parseNamedColorInternal(string.characters8(), string.length());
    return parseNamedColorInternal(string.characters16(), string.length());
}

bool CSSParserFastPaths::isKeywordValidForStyleProperty(CSSPropertyID property, CSSValueID value, const CSSParserContext& context)
{
    return CSSPropertyParsing::isKeywordValidForStyleProperty(property, value, context);
}

bool CSSParserFastPaths::isKeywordFastPathEligibleStyleProperty(CSSPropertyID property)
{
    return CSSPropertyParsing::isKeywordFastPathEligibleStyleProperty(property);
}

static bool isUniversalKeyword(StringView string)
{
    // These keywords can be used for all properties.
    return equalLettersIgnoringASCIICase(string, "initial"_s)
        || equalLettersIgnoringASCIICase(string, "inherit"_s)
        || equalLettersIgnoringASCIICase(string, "unset"_s)
        || equalLettersIgnoringASCIICase(string, "revert"_s)
        || equalLettersIgnoringASCIICase(string, "revert-layer"_s);
}

static RefPtr<CSSValue> parseKeywordValue(CSSPropertyID propertyId, StringView string, const CSSParserContext& context)
{
    ASSERT(!string.isEmpty());

    bool parsingDescriptor = context.enclosingRuleType && *context.enclosingRuleType != StyleRuleType::Style;
    // FIXME: The "!context.enclosingRuleType" is suspicious.
    ASSERT(!CSSProperty::isDescriptorOnly(propertyId) || parsingDescriptor || !context.enclosingRuleType);

    if (!CSSParserFastPaths::isKeywordFastPathEligibleStyleProperty(propertyId)) {
        // All properties, including non-keyword properties, accept the CSS-wide keywords.
        if (!isUniversalKeyword(string))
            return nullptr;

        // Leave shorthands to parse CSS-wide keywords using CSSPropertyParser.
        if (shorthandForProperty(propertyId).length())
            return nullptr;

        // Descriptors do not support the CSS-wide keywords.
        if (parsingDescriptor)
            return nullptr;
    }

    CSSValueID valueID = cssValueKeywordID(string);

    if (!valueID)
        return nullptr;

    if (!parsingDescriptor && isCSSWideKeyword(valueID))
        return CSSPrimitiveValue::create(valueID);

    if (CSSParserFastPaths::isKeywordValidForStyleProperty(propertyId, valueID, context))
        return CSSPrimitiveValue::create(valueID);
    return nullptr;
}

template <typename CharType>
static bool parseTransformTranslateArguments(CharType*& pos, CharType* end, unsigned expectedCount, CSSValueID transformType, CSSValueListBuilder& arguments)
{
    while (expectedCount) {
        size_t delimiter = find({ pos, end }, expectedCount == 1 ? ')' : ',');
        if (delimiter == notFound)
            return false;
        unsigned argumentLength = static_cast<unsigned>(delimiter);
        CSSUnitType unit = CSSUnitType::CSS_NUMBER;
        double number;
        if (!parseSimpleLength(std::span<const CharType> { pos, argumentLength }, unit, number))
            return false;
        if (!number && unit == CSSUnitType::CSS_NUMBER)
            unit = CSSUnitType::CSS_PX;
        if (unit == CSSUnitType::CSS_NUMBER || (unit == CSSUnitType::CSS_PERCENTAGE && (transformType == CSSValueTranslateZ || (transformType == CSSValueTranslate3d && expectedCount == 1))))
            return false;
        arguments.append(CSSPrimitiveValue::create(number, unit));
        pos += argumentLength + 1;
        --expectedCount;
    }
    return true;
}

template <typename CharType>
static RefPtr<CSSValue> parseTransformAngleArgument(CharType*& pos, CharType* end)
{
    size_t delimiter = find({ pos, end }, ')');
    if (delimiter == notFound)
        return nullptr;

    unsigned argumentLength = static_cast<unsigned>(delimiter);
    CSSUnitType unit = CSSUnitType::CSS_NUMBER;
    double number;
    if (!parseSimpleAngle(std::span<const CharType> { pos, argumentLength }, unit, number))
        return nullptr;
    if (!number && unit == CSSUnitType::CSS_NUMBER)
        unit = CSSUnitType::CSS_DEG;

    pos += argumentLength + 1;

    return CSSPrimitiveValue::create(number, unit);
}

template <typename CharType>
static bool parseTransformNumberArguments(CharType*& pos, CharType* end, unsigned expectedCount, CSSValueListBuilder& arguments)
{
    while (expectedCount) {
        size_t delimiter = find({ pos, end }, expectedCount == 1 ? ')' : ',');
        if (delimiter == notFound)
            return false;
        unsigned argumentLength = static_cast<unsigned>(delimiter);
        auto number = parseCSSNumber(std::span<const CharType> { pos, argumentLength });
        if (!number)
            return false;
        arguments.append(CSSPrimitiveValue::create(*number, CSSUnitType::CSS_NUMBER));
        pos += argumentLength + 1;
        --expectedCount;
    }
    return true;
}

static constexpr int kShortestValidTransformStringLength = 9; // "rotate(0)"

template <typename CharType>
static RefPtr<CSSFunctionValue> parseSimpleTransformValue(CharType*& pos, CharType* end)
{
    if (end - pos < kShortestValidTransformStringLength)
        return nullptr;

    bool isTranslate = toASCIILower(pos[0]) == 't'
        && toASCIILower(pos[1]) == 'r'
        && toASCIILower(pos[2]) == 'a'
        && toASCIILower(pos[3]) == 'n'
        && toASCIILower(pos[4]) == 's'
        && toASCIILower(pos[5]) == 'l'
        && toASCIILower(pos[6]) == 'a'
        && toASCIILower(pos[7]) == 't'
        && toASCIILower(pos[8]) == 'e';

    if (isTranslate) {
        CSSValueID transformType;
        unsigned expectedArgumentCount = 1;
        unsigned argumentStart = 11;
        CharType c9 = toASCIILower(pos[9]);
        if (c9 == 'x' && pos[10] == '(') {
            transformType = CSSValueTranslateX;
        } else if (c9 == 'y' && pos[10] == '(') {
            transformType = CSSValueTranslateY;
        } else if (c9 == 'z' && pos[10] == '(') {
            transformType = CSSValueTranslateZ;
        } else if (c9 == '(') {
            transformType = CSSValueTranslate;
            expectedArgumentCount = 2;
            argumentStart = 10;
        } else if (c9 == '3' && toASCIILower(pos[10]) == 'd' && pos[11] == '(') {
            transformType = CSSValueTranslate3d;
            expectedArgumentCount = 3;
            argumentStart = 12;
        } else
            return nullptr;

        CSSValueListBuilder arguments;
        pos += argumentStart;
        if (!parseTransformTranslateArguments(pos, end, expectedArgumentCount, transformType, arguments))
            return nullptr;
        return CSSFunctionValue::create(transformType, WTFMove(arguments));
    }

    bool isMatrix3d = toASCIILower(pos[0]) == 'm'
        && toASCIILower(pos[1]) == 'a'
        && toASCIILower(pos[2]) == 't'
        && toASCIILower(pos[3]) == 'r'
        && toASCIILower(pos[4]) == 'i'
        && toASCIILower(pos[5]) == 'x'
        && pos[6] == '3'
        && toASCIILower(pos[7]) == 'd'
        && pos[8] == '(';

    if (isMatrix3d) {
        pos += 9;
        CSSValueListBuilder arguments;
        if (!parseTransformNumberArguments(pos, end, 16, arguments))
            return nullptr;
        return CSSFunctionValue::create(CSSValueMatrix3d, WTFMove(arguments));
    }

    bool isScale3d = toASCIILower(pos[0]) == 's'
        && toASCIILower(pos[1]) == 'c'
        && toASCIILower(pos[2]) == 'a'
        && toASCIILower(pos[3]) == 'l'
        && toASCIILower(pos[4]) == 'e'
        && pos[5] == '3'
        && toASCIILower(pos[6]) == 'd'
        && pos[7] == '(';

    if (isScale3d) {
        pos += 8;
        CSSValueListBuilder arguments;
        if (!parseTransformNumberArguments(pos, end, 3, arguments))
            return nullptr;
        return CSSFunctionValue::create(CSSValueScale3d, WTFMove(arguments));
    }

    bool isRotate = toASCIILower(pos[0]) == 'r'
        && toASCIILower(pos[1]) == 'o'
        && toASCIILower(pos[2]) == 't'
        && toASCIILower(pos[3]) == 'a'
        && toASCIILower(pos[4]) == 't'
        && toASCIILower(pos[5]) == 'e';

    if (isRotate) {
        CSSValueID transformType;
        unsigned argumentStart = 7;
        CharType c6 = toASCIILower(pos[6]);
        if (c6 == '(') {
            transformType = CSSValueRotate;
        } else if (c6 == 'z' && pos[7] == '(') {
            transformType = CSSValueRotateZ;
            argumentStart = 8;
        } else
            return nullptr;

        pos += argumentStart;
        RefPtr angle = parseTransformAngleArgument(pos, end);
        if (!angle)
            return nullptr;
        return CSSFunctionValue::create(transformType, angle.releaseNonNull());
    }

    return nullptr;
}

template <typename CharType>
static bool transformCanLikelyUseFastPath(const CharType* chars, unsigned length)
{
    // Very fast scan that attempts to reject most transforms that couldn't
    // take the fast path. This avoids doing the malloc and string->double
    // conversions in parseSimpleTransformValue only to discard them when we
    // run into a transform component we don't understand.
    unsigned i = 0;
    while (i < length) {
        if (isCSSSpace(chars[i])) {
            ++i;
            continue;
        }

        if (length - i < kShortestValidTransformStringLength)
            return false;
        
        switch (toASCIILower(chars[i])) {
        case 't':
            // translate, translateX, translateY, translateZ, translate3d.
            if (toASCIILower(chars[i + 8]) != 'e')
                return false;
            i += 9;
            break;
        case 'm':
            // matrix3d.
            if (toASCIILower(chars[i + 7]) != 'd')
                return false;
            i += 8;
            break;
        case 's':
            // scale3d.
            if (toASCIILower(chars[i + 6]) != 'd')
                return false;
            i += 7;
            break;
        case 'r':
            // rotate.
            if (toASCIILower(chars[i + 5]) != 'e')
                return false;
            i += 6;
            // rotateZ
            if (toASCIILower(chars[i]) == 'z')
                ++i;
            break;

        default:
            return false;
        }
        size_t argumentsEnd = find({ chars, length }, ')', i);
        if (argumentsEnd == notFound)
            return false;
        // Advance to the end of the arguments.
        i = argumentsEnd + 1;
    }
    return i == length;
}

template<typename CharacterType> static RefPtr<CSSValue> parseSimpleTransformList(const CharacterType* characters, unsigned length)
{
    if (!transformCanLikelyUseFastPath(characters, length))
        return nullptr;
    auto* pos = characters;
    auto* end = characters + length;
    CSSValueListBuilder builder;
    while (pos < end) {
        while (pos < end && isCSSSpace(*pos))
            ++pos;
        if (pos >= end)
            break;
        auto transformValue = parseSimpleTransformValue(pos, end);
        if (!transformValue)
            return nullptr;
        builder.append(transformValue.releaseNonNull());
    }
    if (builder.isEmpty())
        return nullptr;
    return CSSTransformListValue::create(WTFMove(builder));
}

static RefPtr<CSSValue> parseSimpleTransform(CSSPropertyID propertyID, StringView string)
{
    ASSERT(!string.isEmpty());
    if (propertyID != CSSPropertyTransform)
        return nullptr;
    if (string.is8Bit())
        return parseSimpleTransformList(string.characters8(), string.length());
    return parseSimpleTransformList(string.characters16(), string.length());
}

static RefPtr<CSSValue> parseDisplay(StringView string)
{
    ASSERT(!string.isEmpty());
    auto valueID = cssValueKeywordID(string);

    switch (valueID) {
    // <display-outside>
    case CSSValueBlock:
    case CSSValueInline:
    // <display-inside> (except for CSSValueFlow since it becomes "block")
    case CSSValueFlex:
    case CSSValueFlowRoot:
    case CSSValueGrid:
    case CSSValueTable:
    // <display-internal>
    case CSSValueTableCaption:
    case CSSValueTableCell:
    case CSSValueTableColumnGroup:
    case CSSValueTableColumn:
    case CSSValueTableHeaderGroup:
    case CSSValueTableFooterGroup:
    case CSSValueTableRow:
    case CSSValueTableRowGroup:
    // <display-legacy>
    case CSSValueInlineBlock:
    case CSSValueInlineFlex:
    case CSSValueInlineGrid:
    case CSSValueInlineTable:
    // Prefixed values
    case CSSValueWebkitInlineBox:
    case CSSValueWebkitBox:
    // No layout support for the full <display-listitem> syntax, so treat it as <display-legacy>
    case CSSValueListItem:
        return CSSPrimitiveValue::create(valueID);
    default:
        if (isCSSWideKeyword(valueID))
            return CSSPrimitiveValue::create(valueID);
        return nullptr;
    }
}

static RefPtr<CSSValue> parseColorWithAuto(StringView string, const CSSParserContext& context)
{
    ASSERT(!string.isEmpty());
    if (cssValueKeywordID(string) == CSSValueAuto)
        return CSSPrimitiveValue::create(CSSValueAuto);
    return parseColor(string, context);
}

RefPtr<CSSValue> CSSParserFastPaths::maybeParseValue(CSSPropertyID propertyID, StringView string, const CSSParserContext& context)
{
    if (auto result = parseSimpleLengthValue(propertyID, string, context.mode))
        return result;
    if (propertyID == CSSPropertyDisplay)
        return parseDisplay(string);
    if ((propertyID == CSSPropertyCaretColor || propertyID == CSSPropertyAccentColor) && isExposed(propertyID, &context.propertySettings))
        return parseColorWithAuto(string, context);
    if (CSSProperty::isColorProperty(propertyID))
        return parseColor(string, context);
    if (auto result = parseKeywordValue(propertyID, string, context))
        return result;
    return parseSimpleTransform(propertyID, string);
}

} // namespace WebCore
