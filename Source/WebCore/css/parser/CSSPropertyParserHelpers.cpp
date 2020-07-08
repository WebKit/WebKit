// Copyright 2016 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserHelpers.h"

#include "CSSCalculationValue.h"
#include "CSSCanvasValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSFilterImageValue.h"
#include "CSSGradientValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSNamedImageValue.h"
#include "CSSPaintImageValue.h"
#include "CSSParserIdioms.h"
#include "CSSValuePool.h"
#include "ColorConversion.h"
#include "Pair.h"
#include "RuntimeEnabledFeatures.h"
#include "StyleColor.h"
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

namespace CSSPropertyParserHelpers {

bool consumeCommaIncludingWhitespace(CSSParserTokenRange& range)
{
    CSSParserToken value = range.peek();
    if (value.type() != CommaToken)
        return false;
    range.consumeIncludingWhitespace();
    return true;
}

bool consumeSlashIncludingWhitespace(CSSParserTokenRange& range)
{
    CSSParserToken value = range.peek();
    if (value.type() != DelimiterToken || value.delimiter() != '/')
        return false;
    range.consumeIncludingWhitespace();
    return true;
}

CSSParserTokenRange consumeFunction(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == FunctionToken);
    CSSParserTokenRange contents = range.consumeBlock();
    range.consumeWhitespace();
    contents.consumeWhitespace();
    return contents;
}

// FIXME: consider pulling in the parsing logic from CSSCalculationValue.cpp.
class CalcParser {
public:
    explicit CalcParser(CSSParserTokenRange& range, CalculationCategory destinationCategory, ValueRange valueRange = ValueRangeAll)
        : m_sourceRange(range)
        , m_range(range)
    {
        const CSSParserToken& token = range.peek();
        auto functionId = token.functionId();
        if (CSSCalcValue::isCalcFunction(functionId))
            m_calcValue = CSSCalcValue::create(functionId, consumeFunction(m_range), destinationCategory, valueRange);
    }

    const CSSCalcValue* value() const { return m_calcValue.get(); }

    RefPtr<CSSPrimitiveValue> consumeValue()
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        return CSSValuePool::singleton().createValue(WTFMove(m_calcValue));
    }

    RefPtr<CSSPrimitiveValue> consumeInteger(double minimumValue)
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        double value = std::max(m_calcValue->doubleValue(), minimumValue);
        value = std::round(value);
        return CSSValuePool::singleton().createValue(value, CSSUnitType::CSS_NUMBER);
    }

    RefPtr<CSSPrimitiveValue> consumeNumber()
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        return CSSValuePool::singleton().createValue(m_calcValue->doubleValue(), CSSUnitType::CSS_NUMBER);
    }

    bool consumeNumberRaw(double& result)
    {
        if (!m_calcValue || m_calcValue->category() != CalculationCategory::Number)
            return false;
        m_sourceRange = m_range;
        result = m_calcValue->doubleValue();
        return true;
    }
    
private:
    CSSParserTokenRange& m_sourceRange;
    CSSParserTokenRange m_range;
    RefPtr<CSSCalcValue> m_calcValue;
};

RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range, double minimumValue)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == NumberToken) {
        if (token.numericValueType() == NumberValueType || token.numericValue() < minimumValue)
            return nullptr;
        return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_NUMBER);
    }

    if (token.type() != FunctionToken)
        return nullptr;

    CalcParser calcParser(range, CalculationCategory::Number);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() != CalculationCategory::Number)
            return nullptr;
        return calcParser.consumeInteger(minimumValue);
    }

    return nullptr;
}

RefPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange& range)
{
    return consumeInteger(range, 1);
}

bool consumeNumberRaw(CSSParserTokenRange& range, double& result)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == NumberToken) {
        result = range.consumeIncludingWhitespace().numericValue();
        return true;
    }

    if (token.type() != FunctionToken)
        return false;

    CalcParser calcParser(range, CalculationCategory::Number, ValueRangeAll);
    return calcParser.consumeNumberRaw(result);
}

// FIXME: Work out if this can just call consumeNumberRaw
RefPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == NumberToken) {
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().numericValue(), token.unitType());
    }

    if (token.type() != FunctionToken)
        return nullptr;

    CalcParser calcParser(range, CalculationCategory::Number, valueRange);
    if (const CSSCalcValue* calcValue = calcParser.value()) {
        if (calcValue->category() != CalculationCategory::Number)
            return nullptr;
        return calcParser.consumeValue();
    }

    return nullptr;
}

#if !ENABLE(VARIATION_FONTS)
static inline bool divisibleBy100(double value)
{
    return static_cast<int>(value / 100) * 100 == value;
}
#endif

RefPtr<CSSPrimitiveValue> consumeFontWeightNumber(CSSParserTokenRange& range)
{
    // Values less than or equal to 0 or greater than or equal to 1000 are parse errors.
    auto& token = range.peek();
    if (token.type() == NumberToken && token.numericValue() >= 1 && token.numericValue() <= 1000
#if !ENABLE(VARIATION_FONTS)
        && token.numericValueType() == IntegerValueType && divisibleBy100(token.numericValue())
#endif
    )
        return consumeNumber(range, ValueRangeAll);

    if (token.type() != FunctionToken)
        return nullptr;

    // "[For calc()], the used value resulting from an expression must be clamped to the range allowed in the target context."
    CalcParser calcParser(range, CalculationCategory::Number, ValueRangeAll);
    double result;
    if (calcParser.consumeNumberRaw(result)
#if !ENABLE(VARIATION_FONTS)
        && result > 0 && result < 1000 && divisibleBy100(result)
#endif
    ) {
        result = std::min(std::max(result, std::nextafter(0., 1.)), std::nextafter(1000., 0.));
        return CSSValuePool::singleton().createValue(result, CSSUnitType::CSS_NUMBER);
    }

    return nullptr;
}

inline bool shouldAcceptUnitlessValue(double value, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    // FIXME: Presentational HTML attributes shouldn't use the CSS parser for lengths
    return value == 0
        || isUnitLessValueParsingEnabledForMode(cssParserMode)
        || (cssParserMode == HTMLQuirksMode && unitless == UnitlessQuirk::Allow);
}

RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        switch (token.unitType()) {
        case CSSUnitType::CSS_QUIRKY_EMS:
            if (cssParserMode != UASheetMode)
                return nullptr;
            FALLTHROUGH;
        case CSSUnitType::CSS_EMS:
        case CSSUnitType::CSS_REMS:
        case CSSUnitType::CSS_LHS:
        case CSSUnitType::CSS_RLHS:
        case CSSUnitType::CSS_CHS:
        case CSSUnitType::CSS_EXS:
        case CSSUnitType::CSS_PX:
        case CSSUnitType::CSS_CM:
        case CSSUnitType::CSS_MM:
        case CSSUnitType::CSS_IN:
        case CSSUnitType::CSS_PT:
        case CSSUnitType::CSS_PC:
        case CSSUnitType::CSS_VW:
        case CSSUnitType::CSS_VH:
        case CSSUnitType::CSS_VMIN:
        case CSSUnitType::CSS_VMAX:
        case CSSUnitType::CSS_Q:
            break;
        default:
            return nullptr;
        }
        if ((valueRange == ValueRangeNonNegative && token.numericValue() < 0) || std::isinf(token.numericValue()))
            return nullptr;
        return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().numericValue(), token.unitType());
    }
    if (token.type() == NumberToken) {
        if (!shouldAcceptUnitlessValue(token.numericValue(), cssParserMode, unitless)
            || (valueRange == ValueRangeNonNegative && token.numericValue() < 0))
            return nullptr;
        if (std::isinf(token.numericValue()))
            return nullptr;
        CSSUnitType unitType = CSSUnitType::CSS_PX;
        return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().numericValue(), unitType);
    }

    if (token.type() != FunctionToken)
        return nullptr;

    CalcParser calcParser(range, CalculationCategory::Length, valueRange);
    if (calcParser.value() && calcParser.value()->category() == CalculationCategory::Length)
        return calcParser.consumeValue();

    return nullptr;
}

RefPtr<CSSPrimitiveValue> consumePercent(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == PercentageToken) {
        if ((valueRange == ValueRangeNonNegative && token.numericValue() < 0) || std::isinf(token.numericValue()))
            return nullptr;
        return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_PERCENTAGE);
    }

    if (token.type() != FunctionToken)
        return nullptr;

    CalcParser calcParser(range, CalculationCategory::Percent, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalculationCategory::Percent)
            return calcParser.consumeValue();
    }
    return nullptr;
}

static bool canConsumeCalcValue(CalculationCategory category, CSSParserMode cssParserMode)
{
    if (category == CalculationCategory::Length || category == CalculationCategory::Percent || category == CalculationCategory::PercentLength)
        return true;

    if (cssParserMode != SVGAttributeMode)
        return false;

    if (category == CalculationCategory::Number || category == CalculationCategory::PercentNumber)
        return true;

    return false;
}

RefPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken || token.type() == NumberToken)
        return consumeLength(range, cssParserMode, valueRange, unitless);
    if (token.type() == PercentageToken)
        return consumePercent(range, valueRange);

    if (token.type() != FunctionToken)
        return nullptr;

    CalcParser calcParser(range, CalculationCategory::Length, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (canConsumeCalcValue(calculation->category(), cssParserMode))
            return calcParser.consumeValue();
    }
    return nullptr;
}

RefPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        switch (token.unitType()) {
        case CSSUnitType::CSS_DEG:
        case CSSUnitType::CSS_RAD:
        case CSSUnitType::CSS_GRAD:
        case CSSUnitType::CSS_TURN:
            return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().numericValue(), token.unitType());
        default:
            return nullptr;
        }
    }

    if (token.type() == NumberToken && shouldAcceptUnitlessValue(token.numericValue(), cssParserMode, unitless))
        return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_DEG);

    if (token.type() != FunctionToken)
        return nullptr;

    CalcParser calcParser(range, CalculationCategory::Angle, ValueRangeAll);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalculationCategory::Angle)
            return calcParser.consumeValue();
    }
    return nullptr;
}

static RefPtr<CSSPrimitiveValue> consumeAngleOrPercent(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        switch (token.unitType()) {
        case CSSUnitType::CSS_DEG:
        case CSSUnitType::CSS_RAD:
        case CSSUnitType::CSS_GRAD:
        case CSSUnitType::CSS_TURN:
            return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().numericValue(), token.unitType());
        default:
            return nullptr;
        }
    }
    if (token.type() == NumberToken && shouldAcceptUnitlessValue(token.numericValue(), cssParserMode, unitless))
        return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_DEG);

    if (token.type() == PercentageToken)
        return consumePercent(range, valueRange);

     if (token.type() != FunctionToken)
         return nullptr;

    CalcParser angleCalcParser(range, CalculationCategory::Angle, valueRange);
    if (const CSSCalcValue* calculation = angleCalcParser.value()) {
        if (calculation->category() == CalculationCategory::Angle)
            return angleCalcParser.consumeValue();
    }

    CalcParser percentCalcParser(range, CalculationCategory::Percent, valueRange);
    if (const CSSCalcValue* calculation = percentCalcParser.value()) {
        if (calculation->category() == CalculationCategory::Percent)
            return percentCalcParser.consumeValue();
    }
    return nullptr;
}

RefPtr<CSSPrimitiveValue> consumeTime(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    CSSUnitType unit = token.unitType();
    bool acceptUnitless = token.type() == NumberToken && unitless == UnitlessQuirk::Allow && shouldAcceptUnitlessValue(token.numericValue(), cssParserMode, unitless);
    if (acceptUnitless)
        unit = CSSUnitType::CSS_MS;
    if (token.type() == DimensionToken || acceptUnitless) {
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        if (unit == CSSUnitType::CSS_MS || unit == CSSUnitType::CSS_S)
            return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().numericValue(), unit);
        return nullptr;
    }

    if (token.type() != FunctionToken)
        return nullptr;

    CalcParser calcParser(range, CalculationCategory::Time, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalculationCategory::Time)
            return calcParser.consumeValue();
    }
    return nullptr;
}

RefPtr<CSSPrimitiveValue> consumeResolution(CSSParserTokenRange& range, AllowXResolutionUnit allowX)
{
    const CSSParserToken& token = range.peek();
    // Unlike the other types, calc() does not work with <resolution>.
    if (token.type() != DimensionToken)
        return nullptr;
    CSSUnitType unit = token.unitType();
    if (unit == CSSUnitType::CSS_DPPX || unit == CSSUnitType::CSS_DPI || unit == CSSUnitType::CSS_DPCM)
        return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().numericValue(), unit);
    if (allowX == AllowXResolutionUnit::Allow && token.value() == "x")
        return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_DPPX);

    return nullptr;
}

RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return nullptr;
    return CSSValuePool::singleton().createIdentifierValue(range.consumeIncludingWhitespace().id());
}

RefPtr<CSSPrimitiveValue> consumeIdentRange(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
{
    if (range.peek().id() < lower || range.peek().id() > upper)
        return nullptr;
    return consumeIdent(range);
}

// FIXME-NEWPARSER: Eventually we'd like this to use CSSCustomIdentValue, but we need
// to do other plumbing work first (like changing Pair to CSSValuePair and make it not
// use only primitive values).
RefPtr<CSSPrimitiveValue> consumeCustomIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken || isCSSWideKeyword(range.peek().id()))
        return nullptr;
    return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().value().toString(), CSSUnitType::CSS_STRING);
}

RefPtr<CSSPrimitiveValue> consumeString(CSSParserTokenRange& range)
{
    if (range.peek().type() != StringToken)
        return nullptr;
    return CSSValuePool::singleton().createValue(range.consumeIncludingWhitespace().value().toString(), CSSUnitType::CSS_STRING);
}

StringView consumeUrlAsStringView(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == UrlToken) {
        range.consumeIncludingWhitespace();
        return token.value();
    }
    if (token.functionId() == CSSValueUrl) {
        CSSParserTokenRange urlRange = range;
        CSSParserTokenRange urlArgs = urlRange.consumeBlock();
        const CSSParserToken& next = urlArgs.consumeIncludingWhitespace();
        if (next.type() == BadStringToken || !urlArgs.atEnd())
            return StringView();
        ASSERT(next.type() == StringToken);
        range = urlRange;
        range.consumeWhitespace();
        return next.value();
    }

    return StringView();
}

RefPtr<CSSPrimitiveValue> consumeUrl(CSSParserTokenRange& range)
{
    StringView url = consumeUrlAsStringView(range);
    if (url.isNull())
        return nullptr;
    return CSSValuePool::singleton().createValue(url.toString(), CSSUnitType::CSS_URI);
}

static uint8_t clampRGBComponent(const CSSPrimitiveValue& value)
{
    double result = value.doubleValue();
    if (value.isPercentage())
        result = result / 100.0 * 255.0;

    return convertPrescaledToComponentByte(result);
}

static Color parseRGBParameters(CSSParserTokenRange& range)
{
    ASSERT(range.peek().functionId() == CSSValueRgb || range.peek().functionId() == CSSValueRgba);
    CSSParserTokenRange args = consumeFunction(range);
    RefPtr<CSSPrimitiveValue> colorParameter = consumeNumber(args, ValueRangeAll);
    if (!colorParameter)
        colorParameter = consumePercent(args, ValueRangeAll);
    if (!colorParameter)
        return Color();

    const bool isPercent = colorParameter->isPercentage();

    enum class ColorSyntax {
        Commas,
        WhitespaceSlash,
    };

    ColorSyntax syntax = ColorSyntax::Commas;
    auto consumeSeparator = [&] {
        if (syntax == ColorSyntax::Commas)
            return consumeCommaIncludingWhitespace(args);
        
        return true;
    };

    uint8_t colorArray[3];
    colorArray[0] = clampRGBComponent(*colorParameter);
    for (int i = 1; i < 3; i++) {
        if (i == 1)
            syntax = consumeCommaIncludingWhitespace(args) ? ColorSyntax::Commas : ColorSyntax::WhitespaceSlash;
        else if (!consumeSeparator())
            return Color();

        colorParameter = isPercent ? consumePercent(args, ValueRangeAll) : consumeNumber(args, ValueRangeAll);
        if (!colorParameter)
            return Color();
        colorArray[i] = clampRGBComponent(*colorParameter);
    }

    // Historically, alpha was only parsed for rgba(), but css-color-4 specifies that rgba() is a simple alias for rgb().
    auto consumeAlphaSeparator = [&] {
        if (syntax == ColorSyntax::Commas)
            return consumeCommaIncludingWhitespace(args);
        
        return consumeSlashIncludingWhitespace(args);
    };

    uint8_t alphaComponent = 255;
    if (consumeAlphaSeparator()) {
        double alpha;
        if (!consumeNumberRaw(args, alpha)) {
            auto alphaPercent = consumePercent(args, ValueRangeAll);
            if (!alphaPercent)
                return Color();
            alpha = alphaPercent->doubleValue() / 100.0;
        }
        alphaComponent = convertToComponentByte(alpha);
    };

    if (!args.atEnd())
        return Color();

    return makeSimpleColor(colorArray[0], colorArray[1], colorArray[2], alphaComponent);
}

static Color parseHSLParameters(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    ASSERT(range.peek().functionId() == CSSValueHsl || range.peek().functionId() == CSSValueHsla);
    CSSParserTokenRange args = consumeFunction(range);
    auto hslValue = consumeAngle(args, cssParserMode, UnitlessQuirk::Forbid);
    double angleInDegrees;
    if (!hslValue) {
        hslValue = consumeNumber(args, ValueRangeAll);
        if (!hslValue)
            return Color();
        angleInDegrees = hslValue->doubleValue();
    } else
        angleInDegrees = hslValue->computeDegrees();

    double colorArray[3];
    colorArray[0] = fmod(fmod(angleInDegrees, 360.0) + 360.0, 360.0) / 360.0;
    bool requiresCommas = false;
    for (int i = 1; i < 3; i++) {
        if (consumeCommaIncludingWhitespace(args)) {
            if (i != 1 && !requiresCommas)
                return Color();
            requiresCommas = true;
        } else if (requiresCommas || args.atEnd() || (&args.peek() - 1)->type() != WhitespaceToken)
            return Color();
        hslValue = consumePercent(args, ValueRangeAll);
        if (!hslValue)
            return Color();
        double doubleValue = hslValue->doubleValue();
        colorArray[i] = clampTo<double>(doubleValue, 0.0, 100.0) / 100.0; // Needs to be value between 0 and 1.0.
    }

    double alpha = 1.0;
    bool commaConsumed = consumeCommaIncludingWhitespace(args);
    bool slashConsumed = consumeSlashIncludingWhitespace(args);
    if ((commaConsumed && !requiresCommas) || (slashConsumed && requiresCommas))
        return Color();
    if (commaConsumed || slashConsumed) {
        if (!consumeNumberRaw(args, alpha)) {
            auto alphaPercent = consumePercent(args, ValueRangeAll);
            if (!alphaPercent)
                return Color();
            alpha = alphaPercent->doubleValue() / 100.0f;
        }
        alpha = clampTo<double>(alpha, 0.0, 1.0);
    }

    if (!args.atEnd())
        return Color();

    return makeSimpleColor(toSRGBA(HSLA<float> { static_cast<float>(colorArray[0]), static_cast<float>(colorArray[1]), static_cast<float>(colorArray[2]), static_cast<float>(alpha) }));
}

static Color parseColorFunctionParameters(CSSParserTokenRange& range)
{
    ASSERT(range.peek().functionId() == CSSValueColor);
    CSSParserTokenRange args = consumeFunction(range);

    ColorSpace colorSpace;
    switch (args.peek().id()) {
    case CSSValueSRGB:
        colorSpace = ColorSpace::SRGB;
        break;
    case CSSValueDisplayP3:
        colorSpace = ColorSpace::DisplayP3;
        break;
    default:
        return Color();
    }
    consumeIdent(args);

    double colorChannels[4] = { 0, 0, 0, 1 };
    for (int i = 0; i < 3; ++i) {
        double value;
        if (consumeNumberRaw(args, value))
            colorChannels[i] = std::max(0.0, std::min(1.0, value));
        else
            break;
    }

    if (consumeSlashIncludingWhitespace(args)) {
        auto alphaParameter = consumePercent(args, ValueRangeAll);
        if (!alphaParameter)
            alphaParameter = consumeNumber(args, ValueRangeAll);
        if (!alphaParameter)
            return Color();

        colorChannels[3] = std::max(0.0, std::min(1.0, alphaParameter->isPercentage() ? (alphaParameter->doubleValue() / 100) : alphaParameter->doubleValue()));
    }

    // FIXME: Support the comma-separated list of fallback color values.

    if (!args.atEnd())
        return Color();
    
    return makeExtendedColor(colorChannels[0], colorChannels[1], colorChannels[2], colorChannels[3], colorSpace);
}

static Optional<SRGBA<uint8_t>> parseHexColor(CSSParserTokenRange& range, bool acceptQuirkyColors)
{
    String string;
    StringView view;
    auto& token = range.peek();
    if (token.type() == HashToken)
        view = token.value();
    else {
        if (!acceptQuirkyColors)
            return WTF::nullopt;
        if (token.type() == IdentToken)
            string = token.value().toString(); // e.g. FF0000
        else if (token.type() == NumberToken || token.type() == DimensionToken) {
            if (token.numericValueType() != IntegerValueType)
                return WTF::nullopt;
            auto numericValue = token.numericValue();
            if (!(numericValue >= 0 && numericValue < 1000000))
                return WTF::nullopt;
            auto integerValue = static_cast<int>(token.numericValue());
            if (token.type() == NumberToken)
                string = String::number(integerValue); // e.g. 112233
            else
                string = makeString(integerValue, token.value()); // e.g. 0001FF
            if (string.length() < 6)
                string = makeString(&"000000"[string.length()], string);
        }
        if (string.length() != 3 && string.length() != 6)
            return WTF::nullopt;
        view = string;
    }
    auto result = CSSParser::parseHexColor(view);
    if (!result)
        return WTF::nullopt;
    range.consumeIncludingWhitespace();
    return *result;
}

static Color parseColorFunction(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    CSSParserTokenRange colorRange = range;
    CSSValueID functionId = range.peek().functionId();
    Color color;
    switch (functionId) {
    case CSSValueRgb:
    case CSSValueRgba:
        color = parseRGBParameters(colorRange);
        break;
    case CSSValueHsl:
    case CSSValueHsla:
        color = parseHSLParameters(colorRange, cssParserMode);
        break;
    case CSSValueColor:
        color = parseColorFunctionParameters(colorRange);
        break;
    default:
        return Color();
    }
    if (color.isValid())
        range = colorRange;
    return color;
}

RefPtr<CSSPrimitiveValue> consumeColor(CSSParserTokenRange& range, CSSParserMode cssParserMode, bool acceptQuirkyColors)
{
    auto keyword = range.peek().id();
    if (StyleColor::isColorKeyword(keyword)) {
        if (!isValueAllowedInMode(keyword, cssParserMode))
            return nullptr;
        return consumeIdent(range);
    }
    Color color;
    if (auto parsedColor = parseHexColor(range, acceptQuirkyColors))
        color = *parsedColor;
    else {
        color = parseColorFunction(range, cssParserMode);
        if (!color.isValid())
            return nullptr;
    }
    return CSSValuePool::singleton().createValue(color);
}

static RefPtr<CSSPrimitiveValue> consumePositionComponent(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    if (range.peek().type() == IdentToken)
        return consumeIdent<CSSValueLeft, CSSValueTop, CSSValueBottom, CSSValueRight, CSSValueCenter>(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRangeAll, unitless);
}

static bool isHorizontalPositionKeywordOnly(const CSSPrimitiveValue& value)
{
    return value.isValueID() && (value.valueID() == CSSValueLeft || value.valueID() == CSSValueRight);
}

static bool isVerticalPositionKeywordOnly(const CSSPrimitiveValue& value)
{
    return value.isValueID() && (value.valueID() == CSSValueTop || value.valueID() == CSSValueBottom);
}

static void positionFromOneValue(CSSPrimitiveValue& value, RefPtr<CSSPrimitiveValue>& resultX, RefPtr<CSSPrimitiveValue>& resultY)
{
    bool valueAppliesToYAxisOnly = isVerticalPositionKeywordOnly(value);
    resultX = &value;
    resultY = CSSPrimitiveValue::createIdentifier(CSSValueCenter);
    if (valueAppliesToYAxisOnly)
        std::swap(resultX, resultY);
}

static bool positionFromTwoValues(CSSPrimitiveValue& value1, CSSPrimitiveValue& value2,
    RefPtr<CSSPrimitiveValue>& resultX, RefPtr<CSSPrimitiveValue>& resultY)
{
    bool mustOrderAsXY = isHorizontalPositionKeywordOnly(value1) || isVerticalPositionKeywordOnly(value2)
        || !value1.isValueID() || !value2.isValueID();
    bool mustOrderAsYX = isVerticalPositionKeywordOnly(value1) || isHorizontalPositionKeywordOnly(value2);
    if (mustOrderAsXY && mustOrderAsYX)
        return false;
    resultX = &value1;
    resultY = &value2;
    if (mustOrderAsYX)
        std::swap(resultX, resultY);
    return true;
}

namespace CSSPropertyParserHelpersInternal {
template<typename... Args>
static Ref<CSSPrimitiveValue> createPrimitiveValuePair(Args&&... args)
{
    return CSSValuePool::singleton().createValue(Pair::create(std::forward<Args>(args)...));
}
}

// https://drafts.csswg.org/css-backgrounds-3/#propdef-background-position
// background-position has special parsing rules, allowing a 3-value syntax:
// <bg-position> =  [ left | center | right | top | bottom | <length-percentage> ]
// |
//   [ left | center | right | <length-percentage> ]
//   [ top | center | bottom | <length-percentage> ]
// |
//   [ center | [ left | right ] <length-percentage>? ] &&
//   [ center | [ top | bottom ] <length-percentage>? ]
//
static bool backgroundPositionFromThreeValues(const std::array<CSSPrimitiveValue*, 5>& values, RefPtr<CSSPrimitiveValue>& resultX, RefPtr<CSSPrimitiveValue>& resultY)
{
    CSSPrimitiveValue* center = nullptr;
    for (int i = 0; values[i]; i++) {
        CSSPrimitiveValue* currentValue = values[i];
        if (!currentValue->isValueID())
            return false;

        CSSValueID id = currentValue->valueID();
        if (id == CSSValueCenter) {
            if (center)
                return false;
            center = currentValue;
            continue;
        }

        RefPtr<CSSPrimitiveValue> result;
        if (values[i + 1] && !values[i + 1]->isValueID())
            result = CSSPropertyParserHelpersInternal::createPrimitiveValuePair(currentValue, values[++i]);
        else
            result = currentValue;

        if (id == CSSValueLeft || id == CSSValueRight) {
            if (resultX)
                return false;
            resultX = result;
        } else {
            ASSERT(id == CSSValueTop || id == CSSValueBottom);
            if (resultY)
                return false;
            resultY = result;
        }
    }

    if (center) {
        ASSERT(resultX || resultY);
        if (resultX && resultY)
            return false;
        if (!resultX)
            resultX = center;
        else
            resultY = center;
    }

    ASSERT(resultX && resultY);
    return true;
}

// https://drafts.csswg.org/css-values-4/#typedef-position
// <position> = [
//   [ left | center | right ] || [ top | center | bottom ]
// |
//   [ left | center | right | <length-percentage> ]
//   [ top | center | bottom | <length-percentage> ]?
// |
//   [ [ left | right ] <length-percentage> ] &&
//   [ [ top | bottom ] <length-percentage> ]
//
static bool positionFromFourValues(const std::array<CSSPrimitiveValue*, 5>& values, RefPtr<CSSPrimitiveValue>& resultX, RefPtr<CSSPrimitiveValue>& resultY)
{
    for (int i = 0; values[i]; i++) {
        CSSPrimitiveValue* currentValue = values[i];
        if (!currentValue->isValueID())
            return false;

        CSSValueID id = currentValue->valueID();
        if (id == CSSValueCenter)
            return false;

        RefPtr<CSSPrimitiveValue> result;
        if (values[i + 1] && !values[i + 1]->isValueID())
            result = CSSPropertyParserHelpersInternal::createPrimitiveValuePair(currentValue, values[++i]);
        else
            result = currentValue;

        if (id == CSSValueLeft || id == CSSValueRight) {
            if (resultX)
                return false;
            resultX = result;
        } else {
            ASSERT(id == CSSValueTop || id == CSSValueBottom);
            if (resultY)
                return false;
            resultY = result;
        }
    }

    ASSERT(resultX && resultY);
    return true;
}

// FIXME: This may consume from the range upon failure. The background
// shorthand works around it, but we should just fix it here.
bool consumePosition(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless, PositionSyntax positionSyntax, RefPtr<CSSPrimitiveValue>& resultX, RefPtr<CSSPrimitiveValue>& resultY)
{
    RefPtr<CSSPrimitiveValue> value1 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value1)
        return false;

    RefPtr<CSSPrimitiveValue> value2 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value2) {
        positionFromOneValue(*value1, resultX, resultY);
        return true;
    }

    RefPtr<CSSPrimitiveValue> value3 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value3)
        return positionFromTwoValues(*value1, *value2, resultX, resultY);

    RefPtr<CSSPrimitiveValue> value4 = consumePositionComponent(range, cssParserMode, unitless);
    
    std::array<CSSPrimitiveValue*, 5> values;
    values[0] = value1.get();
    values[1] = value2.get();
    values[2] = value3.get();
    values[3] = value4.get();
    values[4] = nullptr;
    
    if (value4)
        return positionFromFourValues(values, resultX, resultY);
    
    if (positionSyntax != PositionSyntax::BackgroundPosition)
        return false;
    
    return backgroundPositionFromThreeValues(values, resultX, resultY);
}

RefPtr<CSSPrimitiveValue> consumePosition(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless, PositionSyntax positionSyntax)
{
    RefPtr<CSSPrimitiveValue> resultX;
    RefPtr<CSSPrimitiveValue> resultY;
    if (consumePosition(range, cssParserMode, unitless, positionSyntax, resultX, resultY))
        return CSSPropertyParserHelpersInternal::createPrimitiveValuePair(resultX.releaseNonNull(), resultY.releaseNonNull());
    return nullptr;
}

bool consumeOneOrTwoValuedPosition(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless, RefPtr<CSSPrimitiveValue>& resultX, RefPtr<CSSPrimitiveValue>& resultY)
{
    RefPtr<CSSPrimitiveValue> value1 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value1)
        return false;
    RefPtr<CSSPrimitiveValue> value2 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value2) {
        positionFromOneValue(*value1, resultX, resultY);
        return true;
    }
    return positionFromTwoValues(*value1, *value2, resultX, resultY);
}

// This should go away once we drop support for -webkit-gradient
static RefPtr<CSSPrimitiveValue> consumeDeprecatedGradientPoint(CSSParserTokenRange& args, bool horizontal)
{
    if (args.peek().type() == IdentToken) {
        if ((horizontal && consumeIdent<CSSValueLeft>(args)) || (!horizontal && consumeIdent<CSSValueTop>(args)))
            return CSSValuePool::singleton().createValue(0., CSSUnitType::CSS_PERCENTAGE);
        if ((horizontal && consumeIdent<CSSValueRight>(args)) || (!horizontal && consumeIdent<CSSValueBottom>(args)))
            return CSSValuePool::singleton().createValue(100., CSSUnitType::CSS_PERCENTAGE);
        if (consumeIdent<CSSValueCenter>(args))
            return CSSValuePool::singleton().createValue(50., CSSUnitType::CSS_PERCENTAGE);
        return nullptr;
    }
    RefPtr<CSSPrimitiveValue> result = consumePercent(args, ValueRangeAll);
    if (!result)
        result = consumeNumber(args, ValueRangeAll);
    return result;
}

// Used to parse colors for -webkit-gradient(...).
static RefPtr<CSSPrimitiveValue> consumeDeprecatedGradientStopColor(CSSParserTokenRange& args, CSSParserMode cssParserMode)
{
    if (args.peek().id() == CSSValueCurrentcolor)
        return nullptr;
    return consumeColor(args, cssParserMode);
}

static bool consumeDeprecatedGradientColorStop(CSSParserTokenRange& range, CSSGradientColorStop& stop, CSSParserMode cssParserMode)
{
    CSSValueID id = range.peek().functionId();
    if (id != CSSValueFrom && id != CSSValueTo && id != CSSValueColorStop)
        return false;

    CSSParserTokenRange args = consumeFunction(range);
    double position;
    if (id == CSSValueFrom || id == CSSValueTo) {
        position = (id == CSSValueFrom) ? 0 : 1;
    } else {
        ASSERT(id == CSSValueColorStop);
        if (auto percentValue = consumePercent(args, ValueRangeAll))
            position = percentValue->doubleValue() / 100.0;
        else if (!consumeNumberRaw(args, position))
            return false;

        if (!consumeCommaIncludingWhitespace(args))
            return false;
    }

    stop.position = CSSValuePool::singleton().createValue(position, CSSUnitType::CSS_NUMBER);
    stop.color = consumeDeprecatedGradientStopColor(args, cssParserMode);
    return stop.color && args.atEnd();
}

static RefPtr<CSSValue> consumeDeprecatedGradient(CSSParserTokenRange& args, CSSParserMode cssParserMode)
{
    RefPtr<CSSGradientValue> result;
    CSSValueID id = args.consumeIncludingWhitespace().id();
    bool isDeprecatedRadialGradient = (id == CSSValueRadial);
    if (isDeprecatedRadialGradient)
        result = CSSRadialGradientValue::create(NonRepeating, CSSDeprecatedRadialGradient);
    else if (id == CSSValueLinear)
        result = CSSLinearGradientValue::create(NonRepeating, CSSDeprecatedLinearGradient);
    if (!result || !consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto point = consumeDeprecatedGradientPoint(args, true);
    if (!point)
        return nullptr;
    result->setFirstX(WTFMove(point));
    point = consumeDeprecatedGradientPoint(args, false);
    if (!point)
        return nullptr;
    result->setFirstY(WTFMove(point));

    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    // For radial gradients only, we now expect a numeric radius.
    if (isDeprecatedRadialGradient) {
        auto radius = consumeNumber(args, ValueRangeNonNegative);
        if (!radius || !consumeCommaIncludingWhitespace(args))
            return nullptr;
        downcast<CSSRadialGradientValue>(result.get())->setFirstRadius(WTFMove(radius));
    }

    point = consumeDeprecatedGradientPoint(args, true);
    if (!point)
        return nullptr;
    result->setSecondX(WTFMove(point));
    point = consumeDeprecatedGradientPoint(args, false);
    if (!point)
        return nullptr;
    result->setSecondY(WTFMove(point));

    // For radial gradients only, we now expect the second radius.
    if (isDeprecatedRadialGradient) {
        if (!consumeCommaIncludingWhitespace(args))
            return nullptr;
        auto radius = consumeNumber(args, ValueRangeNonNegative);
        if (!radius)
            return nullptr;
        downcast<CSSRadialGradientValue>(result.get())->setSecondRadius(WTFMove(radius));
    }

    CSSGradientColorStop stop;
    while (consumeCommaIncludingWhitespace(args)) {
        if (!consumeDeprecatedGradientColorStop(args, stop, cssParserMode))
            return nullptr;
        result->addStop(WTFMove(stop));
    }

    result->doneAddingStops();
    return result;
}

static bool consumeGradientColorStops(CSSParserTokenRange& range, CSSParserMode mode, CSSGradientValue& gradient)
{
    bool supportsColorHints = gradient.gradientType() == CSSLinearGradient || gradient.gradientType() == CSSRadialGradient || gradient.gradientType() == CSSConicGradient;
    
    auto consumeStopPosition = [&] {
        return gradient.gradientType() == CSSConicGradient
            ? consumeAngleOrPercent(range, mode, ValueRangeAll, UnitlessQuirk::Forbid)
            : consumeLengthOrPercent(range, mode, ValueRangeAll);
    };

    // The first color stop cannot be a color hint.
    bool previousStopWasColorHint = true;
    do {
        CSSGradientColorStop stop { consumeColor(range, mode), consumeStopPosition(), { } };
        if (!stop.color && !stop.position)
            return false;

        // Two color hints in a row are not allowed.
        if (!stop.color && (!supportsColorHints || previousStopWasColorHint))
            return false;
        previousStopWasColorHint = !stop.color;

        // Stops with both a color and a position can have a second position, which shares the same color.
        if (stop.color && stop.position) {
            if (auto secondPosition = consumeStopPosition()) {
                gradient.addStop(CSSGradientColorStop { stop });
                stop.position = WTFMove(secondPosition);
            }
        }
        gradient.addStop(WTFMove(stop));
    } while (consumeCommaIncludingWhitespace(range));

    // The last color stop cannot be a color hint.
    if (previousStopWasColorHint)
        return false;

    // Must have two or more stops to be valid.
    if (!gradient.hasAtLeastTwoStops())
        return false;

    gradient.doneAddingStops();
    return true;
}

static RefPtr<CSSValue> consumeDeprecatedRadialGradient(CSSParserTokenRange& args, CSSParserMode cssParserMode, CSSGradientRepeat repeating)
{
    auto result = CSSRadialGradientValue::create(repeating, CSSPrefixedRadialGradient);

    RefPtr<CSSPrimitiveValue> centerX;
    RefPtr<CSSPrimitiveValue> centerY;
    consumeOneOrTwoValuedPosition(args, cssParserMode, UnitlessQuirk::Forbid, centerX, centerY);
    if ((centerX || centerY) && !consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto shape = consumeIdent<CSSValueCircle, CSSValueEllipse>(args);
    auto sizeKeyword = consumeIdent<CSSValueClosestSide, CSSValueClosestCorner, CSSValueFarthestSide, CSSValueFarthestCorner, CSSValueContain, CSSValueCover>(args);
    if (!shape)
        shape = consumeIdent<CSSValueCircle, CSSValueEllipse>(args);

    // Or, two lengths or percentages
    if (!shape && !sizeKeyword) {
        auto horizontalSize = consumeLengthOrPercent(args, cssParserMode, ValueRangeNonNegative);
        RefPtr<CSSPrimitiveValue> verticalSize;
        if (horizontalSize) {
            verticalSize = consumeLengthOrPercent(args, cssParserMode, ValueRangeNonNegative);
            if (!verticalSize)
                return nullptr;
            consumeCommaIncludingWhitespace(args);
            result->setEndHorizontalSize(WTFMove(horizontalSize));
            result->setEndVerticalSize(WTFMove(verticalSize));
        }
    } else {
        consumeCommaIncludingWhitespace(args);
    }

    if (!consumeGradientColorStops(args, cssParserMode, result))
        return nullptr;

    result->setFirstX(centerX.copyRef());
    result->setFirstY(centerY.copyRef());
    result->setSecondX(WTFMove(centerX));
    result->setSecondY(WTFMove(centerY));
    result->setShape(WTFMove(shape));
    result->setSizingBehavior(WTFMove(sizeKeyword));

    return result;
}

static RefPtr<CSSValue> consumeRadialGradient(CSSParserTokenRange& args, CSSParserMode cssParserMode, CSSGradientRepeat repeating)
{
    RefPtr<CSSRadialGradientValue> result = CSSRadialGradientValue::create(repeating, CSSRadialGradient);

    RefPtr<CSSPrimitiveValue> shape;
    RefPtr<CSSPrimitiveValue> sizeKeyword;
    RefPtr<CSSPrimitiveValue> horizontalSize;
    RefPtr<CSSPrimitiveValue> verticalSize;

    // First part of grammar, the size/shape clause:
    // [ circle || <length> ] |
    // [ ellipse || [ <length> | <percentage> ]{2} ] |
    // [ [ circle | ellipse] || <size-keyword> ]
    for (int i = 0; i < 3; ++i) {
        if (args.peek().type() == IdentToken) {
            CSSValueID id = args.peek().id();
            if (id == CSSValueCircle || id == CSSValueEllipse) {
                if (shape)
                    return nullptr;
                shape = consumeIdent(args);
            } else if (id == CSSValueClosestSide || id == CSSValueClosestCorner || id == CSSValueFarthestSide || id == CSSValueFarthestCorner) {
                if (sizeKeyword)
                    return nullptr;
                sizeKeyword = consumeIdent(args);
            } else {
                break;
            }
        } else {
            auto center = consumeLengthOrPercent(args, cssParserMode, ValueRangeNonNegative);
            if (!center)
                break;
            if (horizontalSize)
                return nullptr;
            horizontalSize = center;
            center = consumeLengthOrPercent(args, cssParserMode, ValueRangeNonNegative);
            if (center) {
                verticalSize = center;
                ++i;
            }
        }
    }

    // You can specify size as a keyword or a length/percentage, not both.
    if (sizeKeyword && horizontalSize)
        return nullptr;
    // Circles must have 0 or 1 lengths.
    if (shape && shape->valueID() == CSSValueCircle && verticalSize)
        return nullptr;
    // Ellipses must have 0 or 2 length/percentages.
    if (shape && shape->valueID() == CSSValueEllipse && horizontalSize && !verticalSize)
        return nullptr;
    // If there's only one size, it must be a length.
    if (!verticalSize && horizontalSize && horizontalSize->isPercentage())
        return nullptr;
    if ((horizontalSize && horizontalSize->isCalculatedPercentageWithLength())
        || (verticalSize && verticalSize->isCalculatedPercentageWithLength()))
        return nullptr;

    RefPtr<CSSPrimitiveValue> centerX;
    RefPtr<CSSPrimitiveValue> centerY;
    if (args.peek().id() == CSSValueAt) {
        args.consumeIncludingWhitespace();
        consumePosition(args, cssParserMode, UnitlessQuirk::Forbid, PositionSyntax::Position, centerX, centerY);
        if (!(centerX && centerY))
            return nullptr;
        
        result->setFirstX(centerX.copyRef());
        result->setFirstY(centerY.copyRef());
        
        // Right now, CSS radial gradients have the same start and end centers.
        result->setSecondX(centerX.copyRef());
        result->setSecondY(centerY.copyRef());
    }

    if ((shape || sizeKeyword || horizontalSize || centerX || centerY) && !consumeCommaIncludingWhitespace(args))
        return nullptr;
    if (!consumeGradientColorStops(args, cssParserMode, *result))
        return nullptr;

    result->setShape(WTFMove(shape));
    result->setSizingBehavior(WTFMove(sizeKeyword));
    result->setEndHorizontalSize(WTFMove(horizontalSize));
    result->setEndVerticalSize(WTFMove(verticalSize));

    return result;
}

static RefPtr<CSSValue> consumeLinearGradient(CSSParserTokenRange& args, CSSParserMode cssParserMode, CSSGradientRepeat repeating, CSSGradientType gradientType)
{
    RefPtr<CSSLinearGradientValue> result = CSSLinearGradientValue::create(repeating, gradientType);

    bool expectComma = true;
    RefPtr<CSSPrimitiveValue> angle = consumeAngle(args, cssParserMode, UnitlessQuirk::Forbid);
    if (angle)
        result->setAngle(angle.releaseNonNull());
    else if (gradientType == CSSPrefixedLinearGradient || consumeIdent<CSSValueTo>(args)) {
        RefPtr<CSSPrimitiveValue> endX = consumeIdent<CSSValueLeft, CSSValueRight>(args);
        RefPtr<CSSPrimitiveValue> endY = consumeIdent<CSSValueBottom, CSSValueTop>(args);
        if (!endX && !endY) {
            if (gradientType == CSSLinearGradient)
                return nullptr;
            endY = CSSPrimitiveValue::createIdentifier(CSSValueTop);
            expectComma = false;
        } else if (!endX) {
            endX = consumeIdent<CSSValueLeft, CSSValueRight>(args);
        }

        result->setFirstX(WTFMove(endX));
        result->setFirstY(WTFMove(endY));
    } else {
        expectComma = false;
    }

    if (expectComma && !consumeCommaIncludingWhitespace(args))
        return nullptr;
    if (!consumeGradientColorStops(args, cssParserMode, *result))
        return nullptr;
    return result;
}

static RefPtr<CSSValue> consumeConicGradient(CSSParserTokenRange& args, CSSParserContext context, CSSGradientRepeat repeating)
{
#if ENABLE(CSS_CONIC_GRADIENTS)
    RefPtr<CSSConicGradientValue> result = CSSConicGradientValue::create(repeating);

    bool expectComma = false;
    if (args.peek().type() == IdentToken) {
        if (consumeIdent<CSSValueFrom>(args)) {
            auto angle = consumeAngle(args, context.mode, UnitlessQuirk::Forbid);
            if (!angle)
                return nullptr;
            result->setAngle(WTFMove(angle));
            expectComma = true;
        }

        if (consumeIdent<CSSValueAt>(args)) {
            RefPtr<CSSPrimitiveValue> centerX;
            RefPtr<CSSPrimitiveValue> centerY;
            consumePosition(args, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position, centerX, centerY);
            if (!(centerX && centerY))
                return nullptr;

            result->setFirstX(centerX.copyRef());
            result->setFirstY(centerY.copyRef());

            // Right now, conic gradients have the same start and end centers.
            result->setSecondX(WTFMove(centerX));
            result->setSecondY(WTFMove(centerY));

            expectComma = true;
        }
    }

    if (expectComma && !consumeCommaIncludingWhitespace(args))
        return nullptr;
    if (!consumeGradientColorStops(args, context.mode, *result))
        return nullptr;
    return result;
#else
    UNUSED_PARAM(args);
    UNUSED_PARAM(context);
    UNUSED_PARAM(repeating);
    return nullptr;
#endif
}

RefPtr<CSSValue> consumeImageOrNone(CSSParserTokenRange& range, CSSParserContext context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeImage(range, context);
}

static RefPtr<CSSValue> consumeCrossFade(CSSParserTokenRange& args, CSSParserContext context, bool prefixed)
{
    RefPtr<CSSValue> fromImageValue = consumeImageOrNone(args, context);
    if (!fromImageValue || !consumeCommaIncludingWhitespace(args))
        return nullptr;
    RefPtr<CSSValue> toImageValue = consumeImageOrNone(args, context);
    if (!toImageValue || !consumeCommaIncludingWhitespace(args))
        return nullptr;

    RefPtr<CSSPrimitiveValue> percentage;
    if (auto percentValue = consumePercent(args, ValueRangeAll))
        percentage = CSSValuePool::singleton().createValue(clampTo<double>(percentValue->doubleValue() / 100.0, 0, 1), CSSUnitType::CSS_NUMBER);
    else if (auto numberValue = consumeNumber(args, ValueRangeAll))
        percentage = CSSValuePool::singleton().createValue(clampTo<double>(numberValue->doubleValue(), 0, 1), CSSUnitType::CSS_NUMBER);

    if (!percentage)
        return nullptr;
    return CSSCrossfadeValue::create(fromImageValue.releaseNonNull(), toImageValue.releaseNonNull(), percentage.releaseNonNull(), prefixed);
}

static RefPtr<CSSValue> consumeWebkitCanvas(CSSParserTokenRange& args)
{
    if (args.peek().type() != IdentToken)
        return nullptr;
    auto canvasName = args.consumeIncludingWhitespace().value().toString();
    if (!args.atEnd())
        return nullptr;
    return CSSCanvasValue::create(canvasName);
}

static RefPtr<CSSValue> consumeWebkitNamedImage(CSSParserTokenRange& args)
{
    if (args.peek().type() != IdentToken)
        return nullptr;
    auto imageName = args.consumeIncludingWhitespace().value().toString();
    if (!args.atEnd())
        return nullptr;
    return CSSNamedImageValue::create(imageName);
}

static RefPtr<CSSValue> consumeFilterImage(CSSParserTokenRange& args, const CSSParserContext& context)
{
    auto imageValue = consumeImageOrNone(args, context);
    if (!imageValue || !consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto filterValue = consumeFilter(args, context, AllowedFilterFunctions::PixelFilters);

    if (!filterValue)
        return nullptr;

    if (!args.atEnd())
        return nullptr;

    return CSSFilterImageValue::create(imageValue.releaseNonNull(), filterValue.releaseNonNull());
}

#if ENABLE(CSS_PAINTING_API)
static RefPtr<CSSValue> consumeCustomPaint(CSSParserTokenRange& args)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().cssPaintingAPIEnabled())
        return nullptr;
    if (args.peek().type() != IdentToken)
        return nullptr;
    auto name = args.consumeIncludingWhitespace().value().toString();

    if (!args.atEnd() && args.peek() != CommaToken)
        return nullptr;
    if (!args.atEnd())
        args.consume();

    auto argumentList = CSSVariableData::create(args);

    while (!args.atEnd())
        args.consume();

    return CSSPaintImageValue::create(name, WTFMove(argumentList));
}
#endif

static RefPtr<CSSValue> consumeGeneratedImage(CSSParserTokenRange& range, CSSParserContext context)
{
    CSSValueID id = range.peek().functionId();
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    RefPtr<CSSValue> result;
    if (id == CSSValueRadialGradient)
        result = consumeRadialGradient(args, context.mode, NonRepeating);
    else if (id == CSSValueRepeatingRadialGradient)
        result = consumeRadialGradient(args, context.mode, Repeating);
    else if (id == CSSValueWebkitLinearGradient)
        result = consumeLinearGradient(args, context.mode, NonRepeating, CSSPrefixedLinearGradient);
    else if (id == CSSValueWebkitRepeatingLinearGradient)
        result = consumeLinearGradient(args, context.mode, Repeating, CSSPrefixedLinearGradient);
    else if (id == CSSValueRepeatingLinearGradient)
        result = consumeLinearGradient(args, context.mode, Repeating, CSSLinearGradient);
    else if (id == CSSValueLinearGradient)
        result = consumeLinearGradient(args, context.mode, NonRepeating, CSSLinearGradient);
    else if (id == CSSValueWebkitGradient)
        result = consumeDeprecatedGradient(args, context.mode);
    else if (id == CSSValueWebkitRadialGradient)
        result = consumeDeprecatedRadialGradient(args, context.mode, NonRepeating);
    else if (id == CSSValueWebkitRepeatingRadialGradient)
        result = consumeDeprecatedRadialGradient(args, context.mode, Repeating);
    else if (id == CSSValueConicGradient)
        result = consumeConicGradient(args, context, NonRepeating);
    else if (id == CSSValueRepeatingConicGradient)
        result = consumeConicGradient(args, context, Repeating);
    else if (id == CSSValueWebkitCrossFade || id == CSSValueCrossFade)
        result = consumeCrossFade(args, context, id == CSSValueWebkitCrossFade);
    else if (id == CSSValueWebkitCanvas)
        result = consumeWebkitCanvas(args);
    else if (id == CSSValueWebkitNamedImage)
        result = consumeWebkitNamedImage(args);
    else if (id == CSSValueWebkitFilter || id == CSSValueFilter)
        result = consumeFilterImage(args, context);
#if ENABLE(CSS_PAINTING_API)
    else if (id == CSSValuePaint)
        result = consumeCustomPaint(args);
#endif
    if (!result || !args.atEnd())
        return nullptr;
    range = rangeCopy;
    return result;
}

static RefPtr<CSSValue> consumeImageSet(CSSParserTokenRange& range, const CSSParserContext& context, OptionSet<AllowedImageType> allowedImageTypes)
{
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    RefPtr<CSSImageSetValue> imageSet = CSSImageSetValue::create();
    do {
        auto image = consumeImage(args, context, allowedImageTypes);
        if (!image)
            return nullptr;

        imageSet->append(image.releaseNonNull());

        auto resolution = consumeResolution(args, AllowXResolutionUnit::Allow);
        if (!resolution || resolution->floatValue() <= 0)
            return nullptr;

        imageSet->append(resolution.releaseNonNull());
    } while (consumeCommaIncludingWhitespace(args));
    if (!args.atEnd())
        return nullptr;
    range = rangeCopy;
    return imageSet;
}

static bool isGeneratedImage(CSSValueID id)
{
    return id == CSSValueLinearGradient
        || id == CSSValueRadialGradient
        || id == CSSValueConicGradient
        || id == CSSValueRepeatingLinearGradient
        || id == CSSValueRepeatingRadialGradient
        || id == CSSValueRepeatingConicGradient
        || id == CSSValueWebkitLinearGradient
        || id == CSSValueWebkitRadialGradient
        || id == CSSValueWebkitRepeatingLinearGradient
        || id == CSSValueWebkitRepeatingRadialGradient
        || id == CSSValueWebkitGradient
        || id == CSSValueWebkitCrossFade
        || id == CSSValueWebkitCanvas
        || id == CSSValueCrossFade
        || id == CSSValueWebkitNamedImage
        || id == CSSValueWebkitFilter
#if ENABLE(CSS_PAINTING_API)
        || id == CSSValuePaint
#endif
        || id == CSSValueFilter;
}
    
static bool isPixelFilterFunction(CSSValueID filterFunction)
{
    switch (filterFunction) {
    case CSSValueBlur:
    case CSSValueBrightness:
    case CSSValueContrast:
    case CSSValueDropShadow:
    case CSSValueGrayscale:
    case CSSValueHueRotate:
    case CSSValueInvert:
    case CSSValueOpacity:
    case CSSValueSaturate:
    case CSSValueSepia:
        return true;
    default:
        return false;
    }
}

static bool isColorFilterFunction(CSSValueID filterFunction)
{
    switch (filterFunction) {
    case CSSValueBrightness:
    case CSSValueContrast:
    case CSSValueGrayscale:
    case CSSValueHueRotate:
    case CSSValueInvert:
    case CSSValueOpacity:
    case CSSValueSaturate:
    case CSSValueSepia:
    case CSSValueAppleInvertLightness:
        return true;
    default:
        return false;
    }
}

static bool allowsValuesGreaterThanOne(CSSValueID filterFunction)
{
    switch (filterFunction) {
    case CSSValueBrightness:
    case CSSValueContrast:
    case CSSValueSaturate:
        return true;
    default:
        return false;
    }
}

static RefPtr<CSSFunctionValue> consumeFilterFunction(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    CSSValueID filterType = range.peek().functionId();
    switch (allowedFunctions) {
    case AllowedFilterFunctions::PixelFilters:
        if (!isPixelFilterFunction(filterType))
            return nullptr;
        break;
    case AllowedFilterFunctions::ColorFilters:
        if (!isColorFilterFunction(filterType))
            return nullptr;
        break;
    }

    CSSParserTokenRange args = consumeFunction(range);
    RefPtr<CSSFunctionValue> filterValue = CSSFunctionValue::create(filterType);

    if (filterType == CSSValueAppleInvertLightness) {
        if (!args.atEnd())
            return nullptr;
        return filterValue;
    }

    RefPtr<CSSValue> parsedValue;

    if (filterType == CSSValueDropShadow)
        parsedValue = consumeSingleShadow(args, context.mode, false, false);
    else {
        if (args.atEnd())
            return filterValue;

        if (filterType == CSSValueHueRotate)
            parsedValue = consumeAngle(args, context.mode, UnitlessQuirk::Forbid);
        else if (filterType == CSSValueBlur)
            parsedValue = consumeLength(args, HTMLStandardMode, ValueRangeNonNegative);
        else {
            parsedValue = consumePercent(args, ValueRangeNonNegative);
            if (!parsedValue)
                parsedValue = consumeNumber(args, ValueRangeNonNegative);
            if (parsedValue && !allowsValuesGreaterThanOne(filterType)) {
                bool isPercentage = downcast<CSSPrimitiveValue>(*parsedValue).isPercentage();
                double maxAllowed = isPercentage ? 100.0 : 1.0;
                if (downcast<CSSPrimitiveValue>(*parsedValue).doubleValue() > maxAllowed)
                    parsedValue = CSSPrimitiveValue::create(maxAllowed, isPercentage ? CSSUnitType::CSS_PERCENTAGE : CSSUnitType::CSS_NUMBER);
            }
        }
    }
    if (!parsedValue || !args.atEnd())
        return nullptr;
    filterValue->append(parsedValue.releaseNonNull());
    return filterValue;
}

RefPtr<CSSValue> consumeFilter(CSSParserTokenRange& range, const CSSParserContext& context, AllowedFilterFunctions allowedFunctions)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    bool referenceFiltersAllowed = allowedFunctions == AllowedFilterFunctions::PixelFilters;
    auto list = CSSValueList::createSpaceSeparated();
    do {
        RefPtr<CSSValue> filterValue = referenceFiltersAllowed ? consumeUrl(range) : nullptr;
        if (!filterValue) {
            filterValue = consumeFilterFunction(range, context, allowedFunctions);
            if (!filterValue)
                return nullptr;
        }
        list->append(filterValue.releaseNonNull());
    } while (!range.atEnd());

    return list.ptr();
}

RefPtr<CSSShadowValue> consumeSingleShadow(CSSParserTokenRange& range, CSSParserMode cssParserMode, bool allowInset, bool allowSpread)
{
    RefPtr<CSSPrimitiveValue> style;
    RefPtr<CSSPrimitiveValue> color;

    if (range.atEnd())
        return nullptr;
    if (range.peek().id() == CSSValueInset) {
        if (!allowInset)
            return nullptr;
        style = consumeIdent(range);
    }
    color = consumeColor(range, cssParserMode);

    auto horizontalOffset = consumeLength(range, cssParserMode, ValueRangeAll);
    if (!horizontalOffset)
        return nullptr;

    auto verticalOffset = consumeLength(range, cssParserMode, ValueRangeAll);
    if (!verticalOffset)
        return nullptr;

    RefPtr<CSSPrimitiveValue> blurRadius;
    RefPtr<CSSPrimitiveValue> spreadDistance;

    const CSSParserToken& token = range.peek();
    // The explicit check for calc() is unfortunate. This is ensuring that we only fail parsing if there is a length, but it fails the range check.
    if (token.type() == DimensionToken || token.type() == NumberToken || (token.type() == FunctionToken && CSSCalcValue::isCalcFunction(token.functionId()))) {
        blurRadius = consumeLength(range, cssParserMode, ValueRangeNonNegative);
        if (!blurRadius)
            return nullptr;
    }

    if (blurRadius && allowSpread)
        spreadDistance = consumeLength(range, cssParserMode, ValueRangeAll);

    if (!range.atEnd()) {
        if (!color)
            color = consumeColor(range, cssParserMode);
        if (range.peek().id() == CSSValueInset) {
            if (!allowInset || style)
                return nullptr;
            style = consumeIdent(range);
        }
    }

    return CSSShadowValue::create(WTFMove(horizontalOffset), WTFMove(verticalOffset), WTFMove(blurRadius), WTFMove(spreadDistance), WTFMove(style), WTFMove(color));
}

RefPtr<CSSValue> consumeImage(CSSParserTokenRange& range, CSSParserContext context, OptionSet<AllowedImageType> allowedImageTypes)
{
    if ((range.peek().type() == StringToken) && (allowedImageTypes.contains(AllowedImageType::RawStringAsURL))) {
        auto urlStringView = range.consumeIncludingWhitespace().value();
        return CSSImageValue::create(completeURL(context, urlStringView.toAtomString()), context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No);
    }

    if (range.peek().type() == FunctionToken) {
        CSSValueID functionId = range.peek().functionId();
        if ((allowedImageTypes.contains(AllowedImageType::GeneratedImage)) && isGeneratedImage(functionId))
            return consumeGeneratedImage(range, context);

        if (allowedImageTypes.contains(AllowedImageType::ImageSet)) {
            if (functionId == CSSValueImageSet)
                return consumeImageSet(range, context, (allowedImageTypes | AllowedImageType::RawStringAsURL) - AllowedImageType::ImageSet);
            if (functionId == CSSValueWebkitImageSet)
                return consumeImageSet(range, context, AllowedImageType::URLFunction);
        }
    }

    if (allowedImageTypes.contains(AllowedImageType::URLFunction)) {
        auto uri = consumeUrlAsStringView(range);
        if (!uri.isNull())
            return CSSImageValue::create(completeURL(context, uri.toAtomString()), context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No);
    }

    return nullptr;
}

} // namespace CSSPropertyParserHelpers

} // namespace WebCore
