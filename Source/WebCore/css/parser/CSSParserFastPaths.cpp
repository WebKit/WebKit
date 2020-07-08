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
#include "CSSParserFastPaths.h"

#include "CSSFunctionValue.h"
#include "CSSParserContext.h"
#include "CSSParserIdioms.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParser.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "HTMLParserIdioms.h"
#include "HashTools.h"
#include "RuntimeEnabledFeatures.h"
#include "StyleColor.h"
#include "StylePropertyShorthand.h"

namespace WebCore {

static inline bool isSimpleLengthPropertyID(CSSPropertyID propertyId, bool& acceptsNegativeNumbers)
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

template <typename CharacterType>
static inline bool parseSimpleLength(const CharacterType* characters, unsigned length, CSSUnitType& unit, double& number)
{
    if (length > 2 && (characters[length - 2] | 0x20) == 'p' && (characters[length - 1] | 0x20) == 'x') {
        length -= 2;
        unit = CSSUnitType::CSS_PX;
    } else if (length > 1 && characters[length - 1] == '%') {
        length -= 1;
        unit = CSSUnitType::CSS_PERCENTAGE;
    }

    // We rely on charactersToDouble for validation as well. The function
    // will set "ok" to "false" if the entire passed-in character range does
    // not represent a double.
    bool ok;
    number = charactersToDouble(characters, length, &ok);
    if (!ok)
        return false;
    return true;
}

template <typename CharacterType>
static inline bool parseSimpleAngle(const CharacterType* characters, unsigned length, CSSUnitType& unit, double& number)
{
    // Just support deg and rad for now.
    if (length < 4)
        return false;

    if ((characters[length - 3] | 0x20) == 'd' && (characters[length - 2] | 0x20) == 'e' && (characters[length - 1] | 0x20) == 'g') {
        length -= 3;
        unit = CSSUnitType::CSS_DEG;
    } else if ((characters[length - 3] | 0x20) == 'r' && (characters[length - 2] | 0x20) == 'a' && (characters[length - 1] | 0x20) == 'd') {
        length -= 3;
        unit = CSSUnitType::CSS_RAD;
    } else
        return false;

    // We rely on charactersToDouble for validation as well. The function
    // will set "ok" to "false" if the entire passed-in character range does
    // not represent a double.
    bool ok;
    number = charactersToDouble(characters, length, &ok);
    if (!ok)
        return false;
    return true;
}

static RefPtr<CSSValue> parseSimpleLengthValue(CSSPropertyID propertyId, StringView string, CSSParserMode cssParserMode)
{
    ASSERT(!string.isEmpty());
    bool acceptsNegativeNumbers = false;

    // In @viewport, width and height are shorthands, not simple length values.
    if (isCSSViewportParsingEnabledForMode(cssParserMode) || !isSimpleLengthPropertyID(propertyId, acceptsNegativeNumbers))
        return nullptr;

    unsigned length = string.length();
    double number;
    CSSUnitType unit = CSSUnitType::CSS_NUMBER;

    if (string.is8Bit()) {
        if (!parseSimpleLength(string.characters8(), length, unit, number))
            return nullptr;
    } else {
        if (!parseSimpleLength(string.characters16(), length, unit, number))
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
static int checkForValidDouble(const CharacterType* string, const CharacterType* end, const char terminator)
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
static int parseDouble(const CharacterType* string, const CharacterType* end, const char terminator, double& value)
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

    const double maxScale = 1000000;
    while (position < length && scale < maxScale) {
        fraction = fraction * 10 + string[position++] - '0';
        scale *= 10;
    }

    value = localValue + fraction / scale;
    return length;
}

template <typename CharacterType>
static bool parseColorIntOrPercentage(const CharacterType*& string, const CharacterType* end, const char terminator, CSSUnitType& expect, int& value)
{
    const CharacterType* current = string;
    double localValue = 0;
    bool negative = false;
    while (current != end && isHTMLSpace<CharacterType>(*current))
        current++;
    if (current != end && *current == '-') {
        negative = true;
        current++;
    }
    if (current == end || !isASCIIDigit(*current))
        return false;
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
        return false;

    if (expect == CSSUnitType::CSS_NUMBER && (*current == '.' || *current == '%'))
        return false;

    if (*current == '.') {
        // We already parsed the integral part, try to parse
        // the fraction part of the percentage value.
        double percentage = 0;
        int numCharactersParsed = parseDouble(current, end, '%', percentage);
        if (!numCharactersParsed)
            return false;
        current += numCharactersParsed;
        if (*current != '%')
            return false;
        localValue += percentage;
    }

    if (expect == CSSUnitType::CSS_PERCENTAGE && *current != '%')
        return false;

    if (*current == '%') {
        expect = CSSUnitType::CSS_PERCENTAGE;
        localValue = localValue / 100.0 * 255.0;
        // Clamp values at 255 for percentages over 100%
        if (localValue > 255)
            localValue = 255;
        current++;
    } else {
        expect = CSSUnitType::CSS_NUMBER;
    }

    while (current != end && isHTMLSpace<CharacterType>(*current))
        current++;
    if (current == end || *current++ != terminator)
        return false;
    // Clamp negative values at zero.
    value = negative ? 0 : static_cast<int>(localValue);
    string = current;
    return true;
}

template <typename CharacterType>
static inline bool isTenthAlpha(const CharacterType* string, const int length)
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
static inline bool parseAlphaValue(const CharacterType*& string, const CharacterType* end, const char terminator, int& value)
{
    while (string != end && isHTMLSpace<CharacterType>(*string))
        string++;

    bool negative = false;

    if (string != end && *string == '-') {
        negative = true;
        string++;
    }

    value = 0;

    int length = end - string;
    if (length < 2)
        return false;

    if (string[length - 1] != terminator || !isASCIIDigit(string[length - 2]))
        return false;

    if (string[0] != '0' && string[0] != '1' && string[0] != '.') {
        if (checkForValidDouble(string, end, terminator)) {
            value = negative ? 0 : 255;
            string = end;
            return true;
        }
        return false;
    }

    if (length == 2 && string[0] != '.') {
        value = !negative && string[0] == '1' ? 255 : 0;
        string = end;
        return true;
    }

    if (isTenthAlpha(string, length - 1)) {
        static const int tenthAlphaValues[] = { 0, 26, 51, 77, 102, 128, 153, 179, 204, 230 };
        value = negative ? 0 : tenthAlphaValues[string[length - 2] - '0'];
        string = end;
        return true;
    }

    double alpha = 0;
    if (!parseDouble(string, end, terminator, alpha))
        return false;

    // W3 standard stipulates a 2.55 alpha value multiplication factor.
    value = negative ? 0 : static_cast<int>(lroundf(clampTo<double>(alpha, 0.0, 1.0) * 255.0f));
    string = end;
    return true;
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

static Optional<SRGBA<uint8_t>> finishParsingHexColor(uint32_t value, unsigned length)
{
    switch (length) {
    case 3:
        // #abc converts to #aabbcc
        // FIXME: Replace conversion to Packed::ARGB with simpler bit math to construct
        // the SRGBA<uint8_t> directly.
        return asSRGBA(Packed::ARGB {
               0xFF000000
            | (value & 0xF00) << 12 | (value & 0xF00) << 8
            | (value & 0xF0) << 8 | (value & 0xF0) << 4
            | (value & 0xF) << 4 | (value & 0xF) });
    case 4:
        // #abcd converts to ddaabbcc since alpha bytes are the high bytes.
        // FIXME: Replace conversion to Packed::ARGB with simpler bit math to construct
        // the SRGBA<uint8_t> directly.
        return asSRGBA(Packed::ARGB {
              (value & 0xF) << 28 | (value & 0xF) << 24
            | (value & 0xF000) << 8 | (value & 0xF000) << 4
            | (value & 0xF00) << 4 | (value & 0xF00)
            | (value & 0xF0) | (value & 0xF0) >> 4 });
    case 6:
        // FIXME: Replace conversion to Packed::ARGB with simpler bit math to construct
        // the SRGBA<uint8_t> directly.
        return asSRGBA(Packed::ARGB { 0xFF000000 | value });
    case 8:
        return asSRGBA(Packed::RGBA { value });
    }
    return WTF::nullopt;
}

template<typename CharacterType> static Optional<SRGBA<uint8_t>> parseHexColorInternal(const CharacterType* characters, unsigned length)
{
    if (length != 3 && length != 4 && length != 6 && length != 8)
        return WTF::nullopt;
    uint32_t value = 0;
    for (unsigned i = 0; i < length; ++i) {
        auto digit = characters[i];
        if (!isASCIIHexDigit(digit))
            return WTF::nullopt;
        value <<= 4;
        value |= toASCIIHexValue(digit);
    }
    return finishParsingHexColor(value, length);
}

template<typename CharacterType> static Optional<SRGBA<uint8_t>> parseNumericColor(const CharacterType* characters, unsigned length, bool strict)
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
        int red;
        if (!parseColorIntOrPercentage(current, end, ',', expect, red))
            return WTF::nullopt;
        int green;
        if (!parseColorIntOrPercentage(current, end, ',', expect, green))
            return WTF::nullopt;
        int blue;
        if (!parseColorIntOrPercentage(current, end, ',', expect, blue))
            return WTF::nullopt;
        int alpha;
        if (!parseAlphaValue(current, end, ')', alpha))
            return WTF::nullopt;
        if (current != end)
            return WTF::nullopt;
        return makeSimpleColor(red, green, blue, alpha); // FIXME: Already clamped, don't need to re-clamp as makeSimpleColor does.
    }

    // Try rgb() syntax.
    if (mightBeRGB(characters, length)) {
        auto current = characters + 4;
        auto end = characters + length;
        int red;
        if (!parseColorIntOrPercentage(current, end, ',', expect, red))
            return WTF::nullopt;
        int green;
        if (!parseColorIntOrPercentage(current, end, ',', expect, green))
            return WTF::nullopt;
        int blue;
        if (!parseColorIntOrPercentage(current, end, ')', expect, blue))
            return WTF::nullopt;
        if (current != end)
            return WTF::nullopt;
        return makeSimpleColor(red, green, blue); // FIXME: Already clamped, don't need to re-clamp as makeSimpleColor does.
    }

    return WTF::nullopt;
}

static Optional<SRGBA<uint8_t>> parseNumericColor(StringView string, const CSSParserContext& context)
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
        if (!isValueAllowedInMode(valueID, context.mode))
            return nullptr;
        return CSSValuePool::singleton().createIdentifierValue(valueID);
    }
    if (auto color = parseNumericColor(string, context))
        return CSSValuePool::singleton().createColorValue(*color);
    return nullptr;
}

static Optional<SRGBA<uint8_t>> finishParsingNamedColor(char* buffer, unsigned length)
{
    buffer[length] = '\0';
    auto namedColor = findColor(buffer, length);
    if (!namedColor)
        return WTF::nullopt;
    return asSRGBA(Packed::ARGB { namedColor->ARGBValue });
}

template<typename CharacterType> static Optional<SRGBA<uint8_t>> parseNamedColorInternal(const CharacterType* characters, unsigned length)
{
    char buffer[64]; // Easily big enough for the longest color name.
    if (length > sizeof(buffer) - 1)
        return WTF::nullopt;
    for (unsigned i = 0; i < length; ++i) {
        auto character = characters[i];
        if (!character || !isASCII(character))
            return WTF::nullopt;
        buffer[i] = toASCIILower(static_cast<char>(character));
    }
    return finishParsingNamedColor(buffer, length);
}

template<typename CharacterType> static Optional<SRGBA<uint8_t>> parseSimpleColorInternal(const CharacterType* characters, unsigned length, bool strict)
{
    if (auto color = parseNumericColor(characters, length, strict))
        return color;
    return parseNamedColorInternal(characters, length);
}

Optional<SRGBA<uint8_t>> CSSParserFastPaths::parseSimpleColor(StringView string, bool strict)
{
    if (string.is8Bit())
        return parseSimpleColorInternal(string.characters8(), string.length(), strict);
    return parseSimpleColorInternal(string.characters16(), string.length(), strict);
}

Optional<SRGBA<uint8_t>> CSSParserFastPaths::parseHexColor(StringView string)
{
    if (string.is8Bit())
        return parseHexColorInternal(string.characters8(), string.length());
    return parseHexColorInternal(string.characters16(), string.length());
}

Optional<SRGBA<uint8_t>> CSSParserFastPaths::parseNamedColor(StringView string)
{
    if (string.is8Bit())
        return parseNamedColorInternal(string.characters8(), string.length());
    return parseNamedColorInternal(string.characters16(), string.length());
}

bool CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyID propertyId, CSSValueID valueID, const CSSParserContext& context)
{
#if !ENABLE(OVERFLOW_SCROLLING_TOUCH)
    UNUSED_PARAM(context);
#endif

    if (valueID == CSSValueInvalid || !isValueAllowedInMode(valueID, context.mode))
        return false;

    switch (propertyId) {
    case CSSPropertyAlignmentBaseline:
        // auto | baseline | before-edge | text-before-edge | middle |
        // central | after-edge | text-after-edge | ideographic | alphabetic |
        // hanging | mathematical
        return valueID == CSSValueAuto || valueID == CSSValueAlphabetic || valueID == CSSValueBaseline
            || valueID == CSSValueMiddle || (valueID >= CSSValueBeforeEdge && valueID <= CSSValueMathematical);
    case CSSPropertyAll:
        return false; // Only accepts css-wide keywords
    case CSSPropertyBackgroundRepeatX: // repeat | no-repeat
    case CSSPropertyBackgroundRepeatY: // repeat | no-repeat
        return valueID == CSSValueRepeat || valueID == CSSValueNoRepeat;
    case CSSPropertyBorderCollapse: // collapse | separate
        return valueID == CSSValueCollapse || valueID == CSSValueSeparate;
    case CSSPropertyBorderTopStyle: // <border-style>
    case CSSPropertyBorderRightStyle: // Defined as: none | hidden | dotted | dashed |
    case CSSPropertyBorderBottomStyle: // solid | double | groove | ridge | inset | outset
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderBlockEndStyle:
    case CSSPropertyBorderBlockStartStyle:
    case CSSPropertyBorderInlineEndStyle:
    case CSSPropertyBorderInlineStartStyle:
    case CSSPropertyColumnRuleStyle:
        return valueID >= CSSValueNone && valueID <= CSSValueDouble;
    case CSSPropertyBoxSizing:
        return valueID == CSSValueBorderBox || valueID == CSSValueContentBox;
    case CSSPropertyBufferedRendering:
        return valueID == CSSValueAuto || valueID == CSSValueDynamic || valueID == CSSValueStatic;
    case CSSPropertyCaptionSide: // top | bottom | left | right
        return valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueTop || valueID == CSSValueBottom;
    case CSSPropertyClear: // none | left | right | both
        return valueID == CSSValueNone || valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueBoth;
    case CSSPropertyClipRule:
    case CSSPropertyFillRule:
        return valueID == CSSValueNonzero || valueID == CSSValueEvenodd;
    case CSSPropertyColorInterpolation:
    case CSSPropertyColorInterpolationFilters:
        return valueID == CSSValueAuto || valueID == CSSValueSRGB || valueID == CSSValueLinearRGB;
    case CSSPropertyColorRendering:
        return valueID == CSSValueAuto || valueID == CSSValueOptimizeSpeed || valueID == CSSValueOptimizeQuality;
    case CSSPropertyDirection: // ltr | rtl
        return valueID == CSSValueLtr || valueID == CSSValueRtl;
    case CSSPropertyDisplay:
        // inline | block | list-item | inline-block | table |
        // inline-table | table-row-group | table-header-group | table-footer-group | table-row |
        // table-column-group | table-column | table-cell | table-caption | -webkit-box | -webkit-inline-box | none
        // flex | inline-flex | -webkit-flex | -webkit-inline-flex | grid | inline-grid
        return (valueID >= CSSValueInline && valueID <= CSSValueContents) || valueID == CSSValueNone
            || valueID == CSSValueGrid || valueID == CSSValueInlineGrid || valueID == CSSValueFlowRoot;
    case CSSPropertyDominantBaseline:
        // auto | use-script | no-change | reset-size | ideographic |
        // alphabetic | hanging | mathematical | central | middle |
        // text-after-edge | text-before-edge
        return valueID == CSSValueAuto || valueID == CSSValueAlphabetic || valueID == CSSValueMiddle
            || (valueID >= CSSValueUseScript && valueID <= CSSValueResetSize)
            || (valueID >= CSSValueCentral && valueID <= CSSValueMathematical);
    case CSSPropertyEmptyCells: // show | hide
        return valueID == CSSValueShow || valueID == CSSValueHide;
    case CSSPropertyFloat: // left | right | none
        return valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueNone;
    case CSSPropertyImageOrientation: // from-image | none
        return valueID == CSSValueFromImage || valueID == CSSValueNone;
    case CSSPropertyImageRendering: // auto | optimizeContrast | pixelated | optimizeSpeed | crispEdges | optimizeQuality | webkit-crispEdges
        return valueID == CSSValueAuto || valueID == CSSValueOptimizeSpeed || valueID == CSSValueOptimizeQuality || valueID == CSSValueWebkitCrispEdges || valueID == CSSValueWebkitOptimizeContrast || valueID == CSSValueCrispEdges || valueID == CSSValuePixelated;
#if ENABLE(CSS_COMPOSITING)
    case CSSPropertyIsolation: // auto | isolate
        return valueID == CSSValueAuto || valueID == CSSValueIsolate;
#endif
    case CSSPropertyListStylePosition: // inside | outside
        return valueID == CSSValueInside || valueID == CSSValueOutside;
    case CSSPropertyListStyleType:
        // See section CSS_PROP_LIST_STYLE_TYPE of file CSSValueKeywords.in
        // for the list of supported list-style-types.
        return (valueID >= CSSValueDisc && valueID <= CSSValueKatakanaIroha) || valueID == CSSValueNone;
    case CSSPropertyMaskType:
        return valueID == CSSValueLuminance || valueID == CSSValueAlpha;
    case CSSPropertyObjectFit:
        return valueID == CSSValueFill || valueID == CSSValueContain || valueID == CSSValueCover || valueID == CSSValueNone || valueID == CSSValueScaleDown;
    case CSSPropertyOutlineStyle: // (<border-style> except hidden) | auto
        return valueID == CSSValueAuto || valueID == CSSValueNone || (valueID >= CSSValueInset && valueID <= CSSValueDouble);
    // FIXME-NEWPARSER: Support?
    // case CSSPropertyOverflowAnchor:
    //    return valueID == CSSValueVisible || valueID == CSSValueNone || valueID == CSSValueAuto;
    case CSSPropertyOverflowWrap: // normal | break-word
    case CSSPropertyWordWrap:
        return valueID == CSSValueNormal || valueID == CSSValueBreakWord;
    case CSSPropertyOverflowX: // visible | hidden | scroll | auto | overlay (overlay is a synonym for auto)
        return valueID == CSSValueVisible || valueID == CSSValueHidden || valueID == CSSValueScroll || valueID == CSSValueAuto || valueID == CSSValueOverlay;
    case CSSPropertyOverflowY: // visible | hidden | scroll | auto | overlay | -webkit-paged-x | -webkit-paged-y (overlay is a synonym for auto)
        return valueID == CSSValueVisible || valueID == CSSValueHidden || valueID == CSSValueScroll || valueID == CSSValueAuto || valueID == CSSValueOverlay || valueID == CSSValueWebkitPagedX || valueID == CSSValueWebkitPagedY;
    case CSSPropertyBreakAfter:
    case CSSPropertyBreakBefore:
        return valueID == CSSValueAuto || valueID == CSSValueAvoid || valueID == CSSValueAvoidPage || valueID == CSSValuePage || valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueRecto || valueID == CSSValueVerso || valueID == CSSValueAvoidColumn || valueID == CSSValueColumn;
    case CSSPropertyBreakInside:
        return valueID == CSSValueAuto || valueID == CSSValueAvoid || valueID == CSSValueAvoidPage || valueID == CSSValueAvoidColumn;
    case CSSPropertyPointerEvents:
        // none | visiblePainted | visibleFill | visibleStroke | visible |
        // painted | fill | stroke | auto | all | bounding-box
        return valueID == CSSValueVisible || valueID == CSSValueNone || valueID == CSSValueAll || valueID == CSSValueAuto || valueID == CSSValueBoundingBox || (valueID >= CSSValueVisiblePainted && valueID <= CSSValueStroke);
    case CSSPropertyPosition: // static | relative | absolute | fixed | sticky
        return valueID == CSSValueStatic
            || valueID == CSSValueRelative
            || valueID == CSSValueAbsolute
            || valueID == CSSValueFixed
            || valueID == CSSValueSticky || valueID == CSSValueWebkitSticky;
    case CSSPropertyResize: // none | both | horizontal | vertical | auto
        return valueID == CSSValueNone || valueID == CSSValueBoth || valueID == CSSValueHorizontal || valueID == CSSValueVertical || valueID == CSSValueAuto;
    case CSSPropertyShapeRendering:
        return valueID == CSSValueAuto || valueID == CSSValueOptimizeSpeed || valueID == CSSValueCrispedges || valueID == CSSValueGeometricPrecision;
    case CSSPropertyStrokeLinejoin:
        return valueID == CSSValueMiter || valueID == CSSValueRound || valueID == CSSValueBevel;
    case CSSPropertyStrokeLinecap:
        return valueID == CSSValueButt || valueID == CSSValueRound || valueID == CSSValueSquare;
    case CSSPropertyTableLayout: // auto | fixed
        return valueID == CSSValueAuto || valueID == CSSValueFixed;
    case CSSPropertyTextAlign:
        return (valueID >= CSSValueWebkitAuto && valueID <= CSSValueWebkitMatchParent) || valueID == CSSValueStart || valueID == CSSValueEnd;
#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextAlignLast:
        // auto | start | end | left | right | center | justify
        return (valueID >= CSSValueLeft && valueID <= CSSValueJustify) || valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueAuto;
#endif
    case CSSPropertyTextAnchor:
        return valueID == CSSValueStart || valueID == CSSValueMiddle || valueID == CSSValueEnd;
// FIXME-NEWPARSER: Support
//    case CSSPropertyTextCombineUpright:
//        return valueID == CSSValueNone || valueID == CSSValueAll;
    case CSSPropertyTextDecorationStyle:
        // solid | double | dotted | dashed | wavy
        return valueID == CSSValueSolid || valueID == CSSValueDouble || valueID == CSSValueDotted || valueID == CSSValueDashed || valueID == CSSValueWavy;
#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextJustify:
        // auto | none | inter-word | distribute
        return valueID == CSSValueInterWord || valueID == CSSValueDistribute || valueID == CSSValueAuto || valueID == CSSValueNone;
#endif
    case CSSPropertyWebkitTextOrientation: // mixed | upright | sideways | sideways-right
        return valueID == CSSValueMixed || valueID == CSSValueUpright || valueID == CSSValueSideways || valueID == CSSValueSidewaysRight;
    case CSSPropertyTextOrientation:
        return valueID == CSSValueMixed || valueID == CSSValueUpright || valueID == CSSValueSideways;
    case CSSPropertyTextOverflow: // clip | ellipsis
        return valueID == CSSValueClip || valueID == CSSValueEllipsis;
    case CSSPropertyTextRendering: // auto | optimizeSpeed | optimizeLegibility | geometricPrecision
        return valueID == CSSValueAuto || valueID == CSSValueOptimizeSpeed || valueID == CSSValueOptimizeLegibility || valueID == CSSValueGeometricPrecision;
    case CSSPropertyTextTransform: // capitalize | uppercase | lowercase | none
        return (valueID >= CSSValueCapitalize && valueID <= CSSValueLowercase) || valueID == CSSValueNone;
    case CSSPropertyUnicodeBidi:
        return valueID == CSSValueNormal || valueID == CSSValueEmbed
            || valueID == CSSValueBidiOverride
            || valueID == CSSValueIsolate || valueID == CSSValueWebkitIsolate
            || valueID == CSSValueIsolateOverride || valueID == CSSValueWebkitIsolateOverride
            || valueID == CSSValuePlaintext || valueID == CSSValueWebkitPlaintext;
    case CSSPropertyVectorEffect:
        return valueID == CSSValueNone || valueID == CSSValueNonScalingStroke;
    case CSSPropertyVisibility: // visible | hidden | collapse
        return valueID == CSSValueVisible || valueID == CSSValueHidden || valueID == CSSValueCollapse;
    case CSSPropertyWebkitAppearance:
        return (valueID >= CSSValueCheckbox && valueID <= CSSValueCapsLockIndicator) || valueID == CSSValueNone;
    case CSSPropertyWebkitBackfaceVisibility:
        return valueID == CSSValueVisible || valueID == CSSValueHidden;
#if ENABLE(CSS_COMPOSITING)
    case CSSPropertyMixBlendMode:
        return valueID == CSSValueNormal || valueID == CSSValueMultiply || valueID == CSSValueScreen || valueID == CSSValueOverlay
            || valueID == CSSValueDarken || valueID == CSSValueLighten || valueID == CSSValueColorDodge || valueID == CSSValueColorBurn
            || valueID == CSSValueHardLight || valueID == CSSValueSoftLight || valueID == CSSValueDifference || valueID == CSSValueExclusion
            || valueID == CSSValueHue || valueID == CSSValueSaturation || valueID == CSSValueColor || valueID == CSSValueLuminosity || valueID == CSSValuePlusDarker || valueID == CSSValuePlusLighter;
#endif
    case CSSPropertyWebkitBoxAlign:
        return valueID == CSSValueStretch || valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueCenter || valueID == CSSValueBaseline;
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    case CSSPropertyWebkitBoxDecorationBreak:
        return valueID == CSSValueClone || valueID == CSSValueSlice;
    case CSSPropertyWebkitBoxDirection:
        return valueID == CSSValueNormal || valueID == CSSValueReverse;
#endif
    case CSSPropertyWebkitBoxLines:
        return valueID == CSSValueSingle || valueID == CSSValueMultiple;
    case CSSPropertyWebkitBoxOrient:
        return valueID == CSSValueHorizontal || valueID == CSSValueVertical || valueID == CSSValueInlineAxis || valueID == CSSValueBlockAxis;
    case CSSPropertyWebkitBoxPack:
        return valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueCenter || valueID == CSSValueJustify;
#if ENABLE(CURSOR_VISIBILITY)
    case CSSPropertyWebkitCursorVisibility:
        return valueID == CSSValueAuto || valueID == CSSValueAutoHide;
#endif
    case CSSPropertyColumnFill:
        return valueID == CSSValueAuto || valueID == CSSValueBalance;
    case CSSPropertyWebkitColumnAxis:
        return valueID == CSSValueHorizontal || valueID == CSSValueVertical || valueID == CSSValueAuto;
    case CSSPropertyWebkitColumnProgression:
        return valueID == CSSValueNormal || valueID == CSSValueReverse;
    case CSSPropertyFlexDirection:
        return valueID == CSSValueRow || valueID == CSSValueRowReverse || valueID == CSSValueColumn || valueID == CSSValueColumnReverse;
    case CSSPropertyFlexWrap:
        return valueID == CSSValueNowrap || valueID == CSSValueWrap || valueID == CSSValueWrapReverse;
    case CSSPropertyWebkitHyphens:
        return valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueManual;
    case CSSPropertyWebkitFontKerning:
        return valueID == CSSValueAuto || valueID == CSSValueNormal || valueID == CSSValueNone;
    case CSSPropertyWebkitFontSmoothing:
        return valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueAntialiased || valueID == CSSValueSubpixelAntialiased;
    case CSSPropertyWebkitLineAlign:
        return valueID == CSSValueNone || valueID == CSSValueEdges;
    case CSSPropertyLineBreak: // auto | loose | normal | strict | after-white-space | anywhere
        return valueID == CSSValueAuto || valueID == CSSValueLoose || valueID == CSSValueNormal || valueID == CSSValueStrict || valueID == CSSValueAfterWhiteSpace || valueID == CSSValueAnywhere;
    case CSSPropertyWebkitLineSnap:
        return valueID == CSSValueNone || valueID == CSSValueBaseline || valueID == CSSValueContain;
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
        return valueID == CSSValueCollapse || valueID == CSSValueSeparate || valueID == CSSValueDiscard;
    case CSSPropertyWebkitPrintColorAdjust:
        return valueID == CSSValueExact || valueID == CSSValueEconomy;
    case CSSPropertyWebkitRtlOrdering:
        return valueID == CSSValueLogical || valueID == CSSValueVisual;
    case CSSPropertyWebkitRubyPosition:
        return valueID == CSSValueBefore || valueID == CSSValueAfter || valueID == CSSValueInterCharacter;
    case CSSPropertyWebkitTextCombine:
        return valueID == CSSValueNone || valueID == CSSValueHorizontal;
    case CSSPropertyWebkitTextSecurity: // disc | circle | square | none
        return valueID == CSSValueDisc || valueID == CSSValueCircle || valueID == CSSValueSquare || valueID == CSSValueNone;
    case CSSPropertyTransformStyle:
    case CSSPropertyWebkitTransformStyle:
        return valueID == CSSValueFlat || valueID == CSSValuePreserve3d;
    case CSSPropertyWebkitUserDrag: // auto | none | element
        return valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueElement;
    case CSSPropertyWebkitUserModify: // read-only | read-write
        return valueID == CSSValueReadOnly || valueID == CSSValueReadWrite || valueID == CSSValueReadWritePlaintextOnly;
    case CSSPropertyWebkitUserSelect: // auto | none | text | all
        return valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueText || valueID == CSSValueAll;
    case CSSPropertyWritingMode:
        // Note that horizontal-bt is not supported by the unprefixed version of
        // the property, only by the -webkit- version.
        return (valueID >= CSSValueHorizontalTb && valueID <= CSSValueHorizontalBt)
            || valueID == CSSValueLrTb || valueID == CSSValueRlTb || valueID == CSSValueTbRl
            || valueID == CSSValueLr || valueID == CSSValueRl || valueID == CSSValueTb;
    case CSSPropertyWhiteSpace: // normal | pre | nowrap | pre-line | nowrap | break-spacess
        return valueID == CSSValueNormal || valueID == CSSValuePre || valueID == CSSValuePreWrap || valueID == CSSValuePreLine || valueID == CSSValueNowrap || valueID == CSSValueBreakSpaces;
    case CSSPropertyWordBreak: // normal | break-all | keep-all | break-word (this is a custom extension)
        return valueID == CSSValueNormal || valueID == CSSValueBreakAll || valueID == CSSValueKeepAll || valueID == CSSValueBreakWord;
    case CSSPropertyWebkitBorderFit:
        return valueID == CSSValueBorder || valueID == CSSValueLines;
#if ENABLE(CSS_TRAILING_WORD)
    case CSSPropertyAppleTrailingWord: // auto | -apple-partially-balanced
        return valueID == CSSValueAuto || valueID == CSSValueWebkitPartiallyBalanced;
#endif
#if ENABLE(APPLE_PAY)
    case CSSPropertyApplePayButtonStyle: // white | white-outline | black
        return valueID == CSSValueWhite || valueID == CSSValueWhiteOutline || valueID == CSSValueBlack;
    case CSSPropertyApplePayButtonType: // plain | buy | set-up | donate | check-out | book | subscribe | reload | add-money | top-up | order | rent | support | contribute | tip
        return (valueID == CSSValuePlain
            || valueID == CSSValueBuy
            || valueID == CSSValueSetUp
            || valueID == CSSValueDonate
            || valueID == CSSValueCheckOut
            || valueID == CSSValueBook
            || valueID == CSSValueSubscribe
#if ENABLE(APPLE_PAY_NEW_BUTTON_TYPES)
            || valueID == CSSValueReload
            || valueID == CSSValueAddMoney
            || valueID == CSSValueTopUp
            || valueID == CSSValueOrder
            || valueID == CSSValueRent
            || valueID == CSSValueSupport
            || valueID == CSSValueContribute
            || valueID == CSSValueTip
#endif
        );
#endif
    case CSSPropertyWebkitNbspMode: // normal | space
        return valueID == CSSValueNormal || valueID == CSSValueSpace;
    case CSSPropertyWebkitTextZoom:
        return valueID == CSSValueNormal || valueID == CSSValueReset;
#if PLATFORM(IOS_FAMILY)
    // Apple specific property. These will never be standardized and is purely to
    // support custom WebKit-based Apple applications.
    case CSSPropertyWebkitTouchCallout:
        return valueID == CSSValueDefault || valueID == CSSValueNone;
#endif
    case CSSPropertyWebkitMarqueeDirection:
        return valueID == CSSValueForwards || valueID == CSSValueBackwards || valueID == CSSValueAhead || valueID == CSSValueReverse || valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueDown
            || valueID == CSSValueUp || valueID == CSSValueAuto;
    case CSSPropertyWebkitMarqueeStyle:
        return valueID == CSSValueNone || valueID == CSSValueSlide || valueID == CSSValueScroll || valueID == CSSValueAlternate;
    case CSSPropertyFontVariantPosition: // normal | sub | super
        return valueID == CSSValueNormal || valueID == CSSValueSub || valueID == CSSValueSuper;
    case CSSPropertyFontVariantCaps: // normal | small-caps | all-small-caps | petite-caps | all-petite-caps | unicase | titling-caps
        return valueID == CSSValueNormal || valueID == CSSValueSmallCaps || valueID == CSSValueAllSmallCaps || valueID == CSSValuePetiteCaps || valueID == CSSValueAllPetiteCaps || valueID == CSSValueUnicase || valueID == CSSValueTitlingCaps;
    case CSSPropertyFontVariantAlternates: // We only support the normal and historical-forms values.
        return valueID == CSSValueNormal || valueID == CSSValueHistoricalForms;
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    case CSSPropertyWebkitOverflowScrolling:
        if (!context.legacyOverflowScrollingTouchEnabled)
            return false;
        return valueID == CSSValueAuto || valueID == CSSValueTouch;
#endif
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyFontOpticalSizing:
        return valueID == CSSValueAuto || valueID == CSSValueNone;
#endif
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool CSSParserFastPaths::isKeywordPropertyID(CSSPropertyID propertyId)
{
    switch (propertyId) {
    case CSSPropertyBorderBlockEndStyle:
    case CSSPropertyBorderBlockStartStyle:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderCollapse:
    case CSSPropertyBorderInlineEndStyle:
    case CSSPropertyBorderInlineStartStyle:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBoxSizing:
    case CSSPropertyBreakAfter:
    case CSSPropertyBreakBefore:
    case CSSPropertyBreakInside:
    case CSSPropertyCaptionSide:
    case CSSPropertyClear:
    case CSSPropertyColumnFill:
    case CSSPropertyWebkitColumnProgression:
    case CSSPropertyColumnRuleStyle:
    case CSSPropertyDirection:
    case CSSPropertyDisplay:
    case CSSPropertyEmptyCells:
    case CSSPropertyFlexDirection:
    case CSSPropertyFlexWrap:
    case CSSPropertyFloat:
    case CSSPropertyFontVariantAlternates:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontVariantPosition:
    case CSSPropertyImageOrientation:
    case CSSPropertyImageRendering:
    case CSSPropertyListStylePosition:
    case CSSPropertyListStyleType:
    case CSSPropertyObjectFit:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOverflowWrap:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyPointerEvents:
    case CSSPropertyPosition:
    case CSSPropertyResize:
    case CSSPropertyTableLayout:
    case CSSPropertyTextAlign:
    case CSSPropertyTextOrientation:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextRendering:
    case CSSPropertyTextTransform:
    case CSSPropertyTransformStyle:
    case CSSPropertyUnicodeBidi:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAppearance:
    case CSSPropertyWebkitBackfaceVisibility:
    case CSSPropertyWebkitBorderFit:
    case CSSPropertyWebkitBoxAlign:
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitBoxLines:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyWebkitColumnAxis:
    case CSSPropertyWebkitFontKerning:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitHyphens:
    case CSSPropertyWebkitLineAlign:
    case CSSPropertyLineBreak:
    case CSSPropertyWebkitLineSnap:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitMarqueeDirection:
    case CSSPropertyWebkitMarqueeStyle:
    case CSSPropertyWebkitNbspMode:
    case CSSPropertyWebkitPrintColorAdjust:
    case CSSPropertyWebkitRtlOrdering:
    case CSSPropertyWebkitRubyPosition:
    case CSSPropertyWebkitTextCombine:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyWebkitTextOrientation:
    case CSSPropertyWebkitTextSecurity:
    case CSSPropertyWebkitTextZoom:
    case CSSPropertyWebkitTransformStyle:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyWebkitUserSelect:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWordBreak:
    case CSSPropertyWordWrap:

    // SVG CSS properties from SVG 1.1, Appendix N: Property Index.
    case CSSPropertyAlignmentBaseline:
    case CSSPropertyBufferedRendering:
    case CSSPropertyClipRule:
    case CSSPropertyColorInterpolation:
    case CSSPropertyColorInterpolationFilters:
    case CSSPropertyColorRendering:
    case CSSPropertyDominantBaseline:
    case CSSPropertyFillRule:
    case CSSPropertyMaskType:
    case CSSPropertyShapeRendering:
    case CSSPropertyStrokeLinecap:
    case CSSPropertyStrokeLinejoin:
    case CSSPropertyTextAnchor:
    case CSSPropertyVectorEffect:
    case CSSPropertyWritingMode:

    // FIXME-NEWPARSER: Treat all as a keyword property.
    // case CSSPropertyAll:

    // FIXME-NEWPARSER: Add the following unprefixed properties:
    // case CSSPropertyBackfaceVisibility:
    // case CSSPropertyFontKerning:
    // case CSSPropertyHyphens:
    // case CSSPropertyOverflowAnchor:
    // case CSSPropertyScrollSnapType:
    // case CSSPropertyTextAlignLast:
    // case CSSPropertyTextCombineUpright:
    // case CSSPropertyTextDecorationStyle:
    // case CSSPropertyTextJustify:
    // case CSSPropertyUserSelect:
#if ENABLE(CSS_TRAILING_WORD)
    case CSSPropertyAppleTrailingWord:
#endif
#if ENABLE(CSS_COMPOSITING)
    case CSSPropertyIsolation:
    case CSSPropertyMixBlendMode:
#endif
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    case CSSPropertyWebkitBoxDecorationBreak:
#endif
#if ENABLE(CURSOR_VISIBILITY)
    case CSSPropertyWebkitCursorVisibility:
#endif
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    case CSSPropertyWebkitOverflowScrolling:
#endif
#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextAlignLast:
    case CSSPropertyWebkitTextJustify:
#endif
#if PLATFORM(IOS_FAMILY)
    // Apple specific property. This will never be standardized and is purely to
    // support custom WebKit-based Apple applications.
    case CSSPropertyWebkitTouchCallout:
#endif
#if ENABLE(APPLE_PAY)
    case CSSPropertyApplePayButtonStyle:
    case CSSPropertyApplePayButtonType:
#endif
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyFontOpticalSizing:
#endif
        return true;
    default:
        return false;
    }
}

static bool isUniversalKeyword(StringView string)
{
    // These keywords can be used for all properties.
    return equalLettersIgnoringASCIICase(string, "initial")
        || equalLettersIgnoringASCIICase(string, "inherit")
        || equalLettersIgnoringASCIICase(string, "unset")
        || equalLettersIgnoringASCIICase(string, "revert");
}

static RefPtr<CSSValue> parseKeywordValue(CSSPropertyID propertyId, StringView string, const CSSParserContext& context)
{
    ASSERT(!string.isEmpty());

    if (!CSSParserFastPaths::isKeywordPropertyID(propertyId)) {
        // All properties accept the values of "initial" and "inherit".
        if (!isUniversalKeyword(string))
            return nullptr;

        // Parse initial/inherit shorthands using the CSSPropertyParser.
        if (shorthandForProperty(propertyId).length())
            return nullptr;

        // Descriptors do not support css wide keywords.
        if (CSSProperty::isDescriptorOnly(propertyId))
            return nullptr;
    }

    CSSValueID valueID = cssValueKeywordID(string);

    if (!valueID)
        return nullptr;

    if (valueID == CSSValueInherit)
        return CSSValuePool::singleton().createInheritedValue();
    if (valueID == CSSValueInitial)
        return CSSValuePool::singleton().createExplicitInitialValue();
    if (valueID == CSSValueUnset)
        return CSSValuePool::singleton().createUnsetValue();
    if (valueID == CSSValueRevert)
        return CSSValuePool::singleton().createRevertValue();
    
    if (CSSParserFastPaths::isValidKeywordPropertyAndValue(propertyId, valueID, context))
        return CSSPrimitiveValue::createIdentifier(valueID);
    return nullptr;
}

template <typename CharType>
static bool parseTransformTranslateArguments(CharType*& pos, CharType* end, unsigned expectedCount, CSSFunctionValue* transformValue)
{
    while (expectedCount) {
        size_t delimiter = WTF::find(pos, end - pos, expectedCount == 1 ? ')' : ',');
        if (delimiter == notFound)
            return false;
        unsigned argumentLength = static_cast<unsigned>(delimiter);
        CSSUnitType unit = CSSUnitType::CSS_NUMBER;
        double number;
        if (!parseSimpleLength(pos, argumentLength, unit, number))
            return false;
        if (!number && unit == CSSUnitType::CSS_NUMBER)
            unit = CSSUnitType::CSS_PX;
        if (unit == CSSUnitType::CSS_NUMBER || (unit == CSSUnitType::CSS_PERCENTAGE && (transformValue->name() == CSSValueTranslateZ || (transformValue->name() == CSSValueTranslate3d && expectedCount == 1))))
            return false;
        transformValue->append(CSSPrimitiveValue::create(number, unit));
        pos += argumentLength + 1;
        --expectedCount;
    }
    return true;
}

template <typename CharType>
static bool parseTransformAngleArgument(CharType*& pos, CharType* end, CSSFunctionValue* transformValue)
{
    size_t delimiter = WTF::find(pos, end - pos, ')');
    if (delimiter == notFound)
        return false;

    unsigned argumentLength = static_cast<unsigned>(delimiter);
    CSSUnitType unit = CSSUnitType::CSS_NUMBER;
    double number;
    if (!parseSimpleAngle(pos, argumentLength, unit, number))
        return false;
    if (!number && unit == CSSUnitType::CSS_NUMBER)
        unit = CSSUnitType::CSS_DEG;

    transformValue->append(CSSPrimitiveValue::create(number, unit));
    pos += argumentLength + 1;

    return true;
}

template <typename CharType>
static bool parseTransformNumberArguments(CharType*& pos, CharType* end, unsigned expectedCount, CSSFunctionValue* transformValue)
{
    while (expectedCount) {
        size_t delimiter = WTF::find(pos, end - pos, expectedCount == 1 ? ')' : ',');
        if (delimiter == notFound)
            return false;
        unsigned argumentLength = static_cast<unsigned>(delimiter);
        bool ok;
        double number = charactersToDouble(pos, argumentLength, &ok);
        if (!ok)
            return false;
        transformValue->append(CSSPrimitiveValue::create(number, CSSUnitType::CSS_NUMBER));
        pos += argumentLength + 1;
        --expectedCount;
    }
    return true;
}

static const int kShortestValidTransformStringLength = 9; // "rotate(0)"

template <typename CharType>
static RefPtr<CSSFunctionValue> parseSimpleTransformValue(CharType*& pos, CharType* end)
{
    if (end - pos < kShortestValidTransformStringLength)
        return nullptr;

    const bool isTranslate = toASCIILower(pos[0]) == 't'
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

        pos += argumentStart;
        RefPtr<CSSFunctionValue> transformValue = CSSFunctionValue::create(transformType);
        if (!parseTransformTranslateArguments(pos, end, expectedArgumentCount, transformValue.get()))
            return nullptr;
        return transformValue;
    }

    const bool isMatrix3d = toASCIILower(pos[0]) == 'm'
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
        RefPtr<CSSFunctionValue> transformValue = CSSFunctionValue::create(CSSValueMatrix3d);
        if (!parseTransformNumberArguments(pos, end, 16, transformValue.get()))
            return nullptr;
        return transformValue;
    }

    const bool isScale3d = toASCIILower(pos[0]) == 's'
        && toASCIILower(pos[1]) == 'c'
        && toASCIILower(pos[2]) == 'a'
        && toASCIILower(pos[3]) == 'l'
        && toASCIILower(pos[4]) == 'e'
        && pos[5] == '3'
        && toASCIILower(pos[6]) == 'd'
        && pos[7] == '(';

    if (isScale3d) {
        pos += 8;
        RefPtr<CSSFunctionValue> transformValue = CSSFunctionValue::create(CSSValueScale3d);
        if (!parseTransformNumberArguments(pos, end, 3, transformValue.get()))
            return nullptr;
        return transformValue;
    }

    const bool isRotate = toASCIILower(pos[0]) == 'r'
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
        RefPtr<CSSFunctionValue> transformValue = CSSFunctionValue::create(transformType);
        if (!parseTransformAngleArgument(pos, end, transformValue.get()))
            return nullptr;
        return transformValue;
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
        size_t argumentsEnd = WTF::find(chars, length, ')', i);
        if (argumentsEnd == notFound)
            return false;
        // Advance to the end of the arguments.
        i = argumentsEnd + 1;
    }
    return i == length;
}

template <typename CharType>
static RefPtr<CSSValueList> parseSimpleTransformList(const CharType* chars, unsigned length)
{
    if (!transformCanLikelyUseFastPath(chars, length))
        return nullptr;
    const CharType*& pos = chars;
    const CharType* end = chars + length;
    RefPtr<CSSValueList> transformList;
    while (pos < end) {
        while (pos < end && isCSSSpace(*pos))
            ++pos;
        if (pos >= end)
            break;
        RefPtr<CSSFunctionValue> transformValue = parseSimpleTransformValue(pos, end);
        if (!transformValue)
            return nullptr;
        if (!transformList)
            transformList = CSSValueList::createSpaceSeparated();
        transformList->append(*transformValue);
    }
    return transformList;
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

static RefPtr<CSSValue> parseCaretColor(StringView string, const CSSParserContext& context)
{
    ASSERT(!string.isEmpty());
    if (cssValueKeywordID(string) == CSSValueAuto)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    return parseColor(string, context);
}

RefPtr<CSSValue> CSSParserFastPaths::maybeParseValue(CSSPropertyID propertyID, StringView string, const CSSParserContext& context)
{
    if (auto result = parseSimpleLengthValue(propertyID, string, context.mode))
        return result;
    if (propertyID == CSSPropertyCaretColor)
        return parseCaretColor(string, context);
    if (CSSProperty::isColorProperty(propertyID))
        return parseColor(string, context);
    if (auto result = parseKeywordValue(propertyID, string, context))
        return result;
    return parseSimpleTransform(propertyID, string);
}

} // namespace WebCore
