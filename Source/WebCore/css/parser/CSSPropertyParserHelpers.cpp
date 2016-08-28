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
// FIXME-NEWPARSER: #include "CSSColorValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSGradientValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
// FIXME-NEWPARSER: #include "CSSPaintValue.h"
// FIXME_NEWPARSER: #include "CSSStringValue.h"
// FIXME-NEWPARSER: #include "CSSURIValue.h"
// FIXME_NEWPARSER: #include "CSSValuePair.h"

namespace WebCore {

/* FIXME-NEWPARSER: Disable for now.
 
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
    explicit CalcParser(CSSParserTokenRange& range, ValueRange valueRange = ValueRangeAll)
        : m_sourceRange(range)
        , m_range(range)
    {
        const CSSParserToken& token = range.peek();
        if (token.functionId() == CSSValueCalc || token.functionId() == CSSValueWebkitCalc)
            m_calcValue = CSSCalcValue::create(consumeFunction(m_range), valueRange);
    }

    const CSSCalcValue* value() const { return m_calcValue.get(); }
    CSSPrimitiveValue* consumeValue()
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        return CSSPrimitiveValue::create(m_calcValue.release());
    }
    CSSPrimitiveValue* consumeNumber()
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        CSSPrimitiveValue::UnitType unitType = m_calcValue->isInt() ? CSSPrimitiveValue::UnitType::Integer : CSSPrimitiveValue::UnitType::Number;
        return CSSPrimitiveValue::create(m_calcValue->doubleValue(), unitType);
    }

    bool consumeNumberRaw(double& result)
    {
        if (!m_calcValue || m_calcValue->category() != CalcNumber)
            return false;
        m_sourceRange = m_range;
        result = m_calcValue->doubleValue();
        return true;
    }

private:
    CSSParserTokenRange& m_sourceRange;
    CSSParserTokenRange m_range;
    Member<CSSCalcValue> m_calcValue;
};

CSSPrimitiveValue* consumeInteger(CSSParserTokenRange& range, double minimumValue)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == NumberToken) {
        if (token.numericValueType() == NumberValueType || token.numericValue() < minimumValue)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), CSSPrimitiveValue::UnitType::Integer);
    }
    CalcParser calcParser(range);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() != CalcNumber || !calculation->isInt())
            return nullptr;
        double value = calculation->doubleValue();
        if (value < minimumValue)
            return nullptr;
        return calcParser.consumeNumber();
    }
    return nullptr;
}

CSSPrimitiveValue* consumePositiveInteger(CSSParserTokenRange& range)
{
    return consumeInteger(range, 1);
}

bool consumeNumberRaw(CSSParserTokenRange& range, double& result)
{
    if (range.peek().type() == NumberToken) {
        result = range.consumeIncludingWhitespace().numericValue();
        return true;
    }
    CalcParser calcParser(range, ValueRangeAll);
    return calcParser.consumeNumberRaw(result);
}

// FIXME: Work out if this can just call consumeNumberRaw
CSSPrimitiveValue* consumeNumber(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == NumberToken) {
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), token.unitType());
    }
    CalcParser calcParser(range, ValueRangeAll);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        // FIXME: Calcs should not be subject to parse time range checks.
        // spec: https://drafts.csswg.org/css-values-3/#calc-range
        if (calculation->category() != CalcNumber || (valueRange == ValueRangeNonNegative && calculation->isNegative()))
            return nullptr;
        return calcParser.consumeNumber();
    }
    return nullptr;
}

inline bool shouldAcceptUnitlessLength(double value, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    // FIXME: Presentational HTML attributes shouldn't use the CSS parser for lengths
    return value == 0
        || isUnitLessLengthParsingEnabledForMode(cssParserMode)
        || (cssParserMode == HTMLQuirksMode && unitless == UnitlessQuirk::Allow);
}

CSSPrimitiveValue* consumeLength(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        switch (token.unitType()) {
        case CSSPrimitiveValue::UnitType::QuirkyEms:
            if (cssParserMode != UASheetMode)
                return nullptr;
            FALLTHROUGH;
        case CSSPrimitiveValue::UnitType::Ems:
        case CSSPrimitiveValue::UnitType::Rems:
        case CSSPrimitiveValue::UnitType::Chs:
        case CSSPrimitiveValue::UnitType::Exs:
        case CSSPrimitiveValue::UnitType::Pixels:
        case CSSPrimitiveValue::UnitType::Centimeters:
        case CSSPrimitiveValue::UnitType::Millimeters:
        case CSSPrimitiveValue::UnitType::Inches:
        case CSSPrimitiveValue::UnitType::Points:
        case CSSPrimitiveValue::UnitType::Picas:
        case CSSPrimitiveValue::UnitType::UserUnits:
        case CSSPrimitiveValue::UnitType::ViewportWidth:
        case CSSPrimitiveValue::UnitType::ViewportHeight:
        case CSSPrimitiveValue::UnitType::ViewportMin:
        case CSSPrimitiveValue::UnitType::ViewportMax:
            break;
        default:
            return nullptr;
        }
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), token.unitType());
    }
    if (token.type() == NumberToken) {
        if (!shouldAcceptUnitlessLength(token.numericValue(), cssParserMode, unitless)
            || (valueRange == ValueRangeNonNegative && token.numericValue() < 0))
            return nullptr;
        CSSPrimitiveValue::UnitType unitType = CSSPrimitiveValue::UnitType::Pixels;
        if (cssParserMode == SVGAttributeMode)
            unitType = CSSPrimitiveValue::UnitType::UserUnits;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), unitType);
    }
    if (cssParserMode == SVGAttributeMode)
        return nullptr;
    CalcParser calcParser(range, valueRange);
    if (calcParser.value() && calcParser.value()->category() == CalcLength)
        return calcParser.consumeValue();
    return nullptr;
}

CSSPrimitiveValue* consumePercent(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == PercentageToken) {
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), CSSPrimitiveValue::UnitType::Percentage);
    }
    CalcParser calcParser(range, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalcPercent)
            return calcParser.consumeValue();
    }
    return nullptr;
}

bool canConsumeCalcValue(CalculationCategory category, CSSParserMode cssParserMode)
{
    if (category == CalcLength || category == CalcPercent || category == CalcPercentLength)
        return true;

    if (cssParserMode != SVGAttributeMode)
        return false;

    if (category == CalcNumber || category == CalcPercentNumber || category == CalcLengthNumber || category == CalcPercentLengthNumber)
        return true;

    return false;
}

CSSPrimitiveValue* consumeLengthOrPercent(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken || token.type() == NumberToken)
        return consumeLength(range, cssParserMode, valueRange, unitless);
    if (token.type() == PercentageToken)
        return consumePercent(range, valueRange);
    CalcParser calcParser(range, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (canConsumeCalcValue(calculation->category(), cssParserMode))
            return calcParser.consumeValue();
    }
    return nullptr;
}

CSSPrimitiveValue* consumeAngle(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        switch (token.unitType()) {
        case CSSPrimitiveValue::UnitType::Degrees:
        case CSSPrimitiveValue::UnitType::Radians:
        case CSSPrimitiveValue::UnitType::Gradians:
        case CSSPrimitiveValue::UnitType::Turns:
            return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), token.unitType());
        default:
            return nullptr;
        }
    }
    if (token.type() == NumberToken && token.numericValue() == 0) {
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Degrees);
    }
    CalcParser calcParser(range, ValueRangeAll);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalcAngle)
            return calcParser.consumeValue();
    }
    return nullptr;
}

CSSPrimitiveValue* consumeTime(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        CSSPrimitiveValue::UnitType unit = token.unitType();
        if (unit == CSSPrimitiveValue::UnitType::Milliseconds || unit == CSSPrimitiveValue::UnitType::Seconds)
            return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), token.unitType());
        return nullptr;
    }
    CalcParser calcParser(range, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalcTime)
            return calcParser.consumeValue();
    }
    return nullptr;
}

CSSPrimitiveValue* consumeIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return nullptr;
    return CSSPrimitiveValue::createIdentifier(range.consumeIncludingWhitespace().id());
}

CSSPrimitiveValue* consumeIdentRange(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
{
    if (range.peek().id() < lower || range.peek().id() > upper)
        return nullptr;
    return consumeIdent(range);
}

CSSCustomIdentValue* consumeCustomIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken || isCSSWideKeyword(range.peek().id()))
        return nullptr;
    return CSSCustomIdentValue::create(range.consumeIncludingWhitespace().value().toString());
}

CSSStringValue* consumeString(CSSParserTokenRange& range)
{
    if (range.peek().type() != StringToken)
        return nullptr;
    return CSSStringValue::create(range.consumeIncludingWhitespace().value().toString());
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

CSSURIValue* consumeUrl(CSSParserTokenRange& range)
{
    StringView url = consumeUrlAsStringView(range);
    if (url.isNull())
        return nullptr;
    return CSSURIValue::create(url.toString());
}

static int clampRGBComponent(const CSSPrimitiveValue& value)
{
    double result = value.getDoubleValue();
    // FIXME: Multiply by 2.55 and round instead of floor.
    if (value.isPercentage())
        result *= 2.56;
    return clampTo<int>(result, 0, 255);
}

static bool parseRGBParameters(CSSParserTokenRange& range, RGBA32& result, bool parseAlpha)
{
    ASSERT(range.peek().functionId() == CSSValueRgb || range.peek().functionId() == CSSValueRgba);
    CSSParserTokenRange args = consumeFunction(range);
    CSSPrimitiveValue* colorParameter = consumeInteger(args);
    if (!colorParameter)
        colorParameter = consumePercent(args, ValueRangeAll);
    if (!colorParameter)
        return false;
    const bool isPercent = colorParameter->isPercentage();
    int colorArray[3];
    colorArray[0] = clampRGBComponent(*colorParameter);
    for (int i = 1; i < 3; i++) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;
        colorParameter = isPercent ? consumePercent(args, ValueRangeAll) : consumeInteger(args);
        if (!colorParameter)
            return false;
        colorArray[i] = clampRGBComponent(*colorParameter);
    }
    if (parseAlpha) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;
        double alpha;
        if (!consumeNumberRaw(args, alpha))
            return false;
        // Convert the floating pointer number of alpha to an integer in the range [0, 256),
        // with an equal distribution across all 256 values.
        int alphaComponent = static_cast<int>(clampTo<double>(alpha, 0.0, 1.0) * nextafter(256.0, 0.0));
        result = makeRGBA(colorArray[0], colorArray[1], colorArray[2], alphaComponent);
    } else {
        result = makeRGB(colorArray[0], colorArray[1], colorArray[2]);
    }
    return args.atEnd();
}

static bool parseHSLParameters(CSSParserTokenRange& range, RGBA32& result, bool parseAlpha)
{
    ASSERT(range.peek().functionId() == CSSValueHsl || range.peek().functionId() == CSSValueHsla);
    CSSParserTokenRange args = consumeFunction(range);
    CSSPrimitiveValue* hslValue = consumeNumber(args, ValueRangeAll);
    if (!hslValue)
        return false;
    double colorArray[3];
    colorArray[0] = (((hslValue->getIntValue() % 360) + 360) % 360) / 360.0;
    for (int i = 1; i < 3; i++) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;
        hslValue = consumePercent(args, ValueRangeAll);
        if (!hslValue)
            return false;
        double doubleValue = hslValue->getDoubleValue();
        colorArray[i] = clampTo<double>(doubleValue, 0.0, 100.0) / 100.0; // Needs to be value between 0 and 1.0.
    }
    double alpha = 1.0;
    if (parseAlpha) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;
        if (!consumeNumberRaw(args, alpha))
            return false;
        alpha = clampTo<double>(alpha, 0.0, 1.0);
    }
    result = makeRGBAFromHSLA(colorArray[0], colorArray[1], colorArray[2], alpha);
    return args.atEnd();
}

static bool parseHexColor(CSSParserTokenRange& range, RGBA32& result, bool acceptQuirkyColors)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == HashToken) {
        if (!Color::parseHexColor(token.value(), result))
            return false;
    } else if (acceptQuirkyColors) {
        String color;
        if (token.type() == NumberToken || token.type() == DimensionToken) {
            if (token.numericValueType() != IntegerValueType
                || token.numericValue() < 0. || token.numericValue() >= 1000000.)
                return false;
            if (token.type() == NumberToken) // e.g. 112233
                color = String::format("%d", static_cast<int>(token.numericValue()));
            else // e.g. 0001FF
                color = String::number(static_cast<int>(token.numericValue())) + token.value().toString();
            while (color.length() < 6)
                color = "0" + color;
        } else if (token.type() == IdentToken) { // e.g. FF0000
            color = token.value().toString();
        }
        unsigned length = color.length();
        if (length != 3 && length != 6)
            return false;
        if (!Color::parseHexColor(color, result))
            return false;
    } else {
        return false;
    }
    range.consumeIncludingWhitespace();
    return true;
}

static bool parseColorFunction(CSSParserTokenRange& range, RGBA32& result)
{
    CSSValueID functionId = range.peek().functionId();
    if (functionId < CSSValueRgb || functionId > CSSValueHsla)
        return false;
    CSSParserTokenRange colorRange = range;
    if ((functionId <= CSSValueRgba && !parseRGBParameters(colorRange, result, functionId == CSSValueRgba))
        || (functionId >= CSSValueHsl && !parseHSLParameters(colorRange, result, functionId == CSSValueHsla)))
        return false;
    range = colorRange;
    return true;
}

CSSValue* consumeColor(CSSParserTokenRange& range, CSSParserMode cssParserMode, bool acceptQuirkyColors)
{
    CSSValueID id = range.peek().id();
    if (StyleColor::isColorKeyword(id)) {
        if (!isValueAllowedInMode(id, cssParserMode))
            return nullptr;
        return consumeIdent(range);
    }
    RGBA32 color = Color::transparent;
    if (!parseHexColor(range, color, acceptQuirkyColors) && !parseColorFunction(range, color))
        return nullptr;
    return CSSColorValue::create(color);
}

static CSSPrimitiveValue* consumePositionComponent(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    if (range.peek().type() == IdentToken)
        return consumeIdent<CSSValueLeft, CSSValueTop, CSSValueBottom, CSSValueRight, CSSValueCenter>(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRangeAll, unitless);
}

static bool isHorizontalPositionKeywordOnly(const CSSPrimitiveValue& value)
{
    return value.isValueID() && (value.getValueID() == CSSValueLeft || value.getValueID() == CSSValueRight);
}

static bool isVerticalPositionKeywordOnly(const CSSPrimitiveValue& value)
{
    return value.isValueID() && (value.getValueID() == CSSValueTop || value.getValueID() == CSSValueBottom);
}

static void positionFromOneValue(CSSPrimitiveValue* value, CSSValue*& resultX, CSSValue*& resultY)
{
    bool valueAppliesToYAxisOnly = isVerticalPositionKeywordOnly(*value);
    resultX = value;
    resultY = CSSPrimitiveValue::createIdentifier(CSSValueCenter);
    if (valueAppliesToYAxisOnly)
        std::swap(resultX, resultY);
}

static bool positionFromTwoValues(CSSPrimitiveValue* value1, CSSPrimitiveValue* value2,
    CSSValue*& resultX, CSSValue*& resultY)
{
    bool mustOrderAsXY = isHorizontalPositionKeywordOnly(*value1) || isVerticalPositionKeywordOnly(*value2)
        || !value1->isValueID() || !value2->isValueID();
    bool mustOrderAsYX = isVerticalPositionKeywordOnly(*value1) || isHorizontalPositionKeywordOnly(*value2);
    if (mustOrderAsXY && mustOrderAsYX)
        return false;
    resultX = value1;
    resultY = value2;
    if (mustOrderAsYX)
        std::swap(resultX, resultY);
    return true;
}

static bool positionFromThreeOrFourValues(CSSPrimitiveValue** values, CSSValue*& resultX, CSSValue*& resultY)
{
    CSSPrimitiveValue* center = nullptr;
    for (int i = 0; values[i]; i++) {
        CSSPrimitiveValue* currentValue = values[i];
        if (!currentValue->isValueID())
            return false;
        CSSValueID id = currentValue->getValueID();

        if (id == CSSValueCenter) {
            if (center)
                return false;
            center = currentValue;
            continue;
        }

        CSSValue* result = nullptr;
        if (values[i + 1] && !values[i + 1]->isValueID()) {
            result = CSSValuePair::create(currentValue, values[++i], CSSValuePair::KeepIdenticalValues);
        } else {
            result = currentValue;
        }

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

// FIXME: This may consume from the range upon failure. The background
// shorthand works around it, but we should just fix it here.
bool consumePosition(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless, CSSValue*& resultX, CSSValue*& resultY)
{
    CSSPrimitiveValue* value1 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value1)
        return false;

    CSSPrimitiveValue* value2 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value2) {
        positionFromOneValue(value1, resultX, resultY);
        return true;
    }

    CSSPrimitiveValue* value3 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value3)
        return positionFromTwoValues(value1, value2, resultX, resultY);

    CSSPrimitiveValue* value4 = consumePositionComponent(range, cssParserMode, unitless);
    CSSPrimitiveValue* values[5];
    values[0] = value1;
    values[1] = value2;
    values[2] = value3;
    values[3] = value4;
    values[4] = nullptr;
    return positionFromThreeOrFourValues(values, resultX, resultY);
}

CSSValuePair* consumePosition(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    CSSValue* resultX = nullptr;
    CSSValue* resultY = nullptr;
    if (consumePosition(range, cssParserMode, unitless, resultX, resultY))
        return CSSValuePair::create(resultX, resultY, CSSValuePair::KeepIdenticalValues);
    return nullptr;
}

bool consumeOneOrTwoValuedPosition(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless, CSSValue*& resultX, CSSValue*& resultY)
{
    CSSPrimitiveValue* value1 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value1)
        return false;
    CSSPrimitiveValue* value2 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value2) {
        positionFromOneValue(value1, resultX, resultY);
        return true;
    }
    return positionFromTwoValues(value1, value2, resultX, resultY);
}

// This should go away once we drop support for -webkit-gradient
static CSSPrimitiveValue* consumeDeprecatedGradientPoint(CSSParserTokenRange& args, bool horizontal)
{
    if (args.peek().type() == IdentToken) {
        if ((horizontal && consumeIdent<CSSValueLeft>(args)) || (!horizontal && consumeIdent<CSSValueTop>(args)))
            return CSSPrimitiveValue::create(0., CSSPrimitiveValue::UnitType::Percentage);
        if ((horizontal && consumeIdent<CSSValueRight>(args)) || (!horizontal && consumeIdent<CSSValueBottom>(args)))
            return CSSPrimitiveValue::create(100., CSSPrimitiveValue::UnitType::Percentage);
        if (consumeIdent<CSSValueCenter>(args))
            return CSSPrimitiveValue::create(50., CSSPrimitiveValue::UnitType::Percentage);
        return nullptr;
    }
    CSSPrimitiveValue* result = consumePercent(args, ValueRangeAll);
    if (!result)
        result = consumeNumber(args, ValueRangeAll);
    return result;
}

// Used to parse colors for -webkit-gradient(...).
static CSSValue* consumeDeprecatedGradientStopColor(CSSParserTokenRange& args, CSSParserMode cssParserMode)
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
        DCHECK(id == CSSValueColorStop);
        const CSSParserToken& arg = args.consumeIncludingWhitespace();
        if (arg.type() == PercentageToken)
            position = arg.numericValue() / 100.0;
        else if (arg.type() == NumberToken)
            position = arg.numericValue();
        else
            return false;

        if (!consumeCommaIncludingWhitespace(args))
            return false;
    }

    stop.m_position = CSSPrimitiveValue::create(position, CSSPrimitiveValue::UnitType::Number);
    stop.m_color = consumeDeprecatedGradientStopColor(args, cssParserMode);
    return stop.m_color && args.atEnd();
}

static CSSValue* consumeDeprecatedGradient(CSSParserTokenRange& args, CSSParserMode cssParserMode)
{
    CSSGradientValue* result = nullptr;
    CSSValueID id = args.consumeIncludingWhitespace().id();
    bool isDeprecatedRadialGradient = (id == CSSValueRadial);
    if (isDeprecatedRadialGradient)
        result = CSSRadialGradientValue::create(NonRepeating, CSSDeprecatedRadialGradient);
    else if (id == CSSValueLinear)
        result = CSSLinearGradientValue::create(NonRepeating, CSSDeprecatedLinearGradient);
    if (!result || !consumeCommaIncludingWhitespace(args))
        return nullptr;

    CSSPrimitiveValue* point = consumeDeprecatedGradientPoint(args, true);
    if (!point)
        return nullptr;
    result->setFirstX(point);
    point = consumeDeprecatedGradientPoint(args, false);
    if (!point)
        return nullptr;
    result->setFirstY(point);

    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    // For radial gradients only, we now expect a numeric radius.
    if (isDeprecatedRadialGradient) {
        CSSPrimitiveValue* radius = consumeNumber(args, ValueRangeAll);
        if (!radius || !consumeCommaIncludingWhitespace(args))
            return nullptr;
        toCSSRadialGradientValue(result)->setFirstRadius(radius);
    }

    point = consumeDeprecatedGradientPoint(args, true);
    if (!point)
        return nullptr;
    result->setSecondX(point);
    point = consumeDeprecatedGradientPoint(args, false);
    if (!point)
        return nullptr;
    result->setSecondY(point);

    // For radial gradients only, we now expect the second radius.
    if (isDeprecatedRadialGradient) {
        if (!consumeCommaIncludingWhitespace(args))
            return nullptr;
        CSSPrimitiveValue* radius = consumeNumber(args, ValueRangeAll);
        if (!radius)
            return nullptr;
        toCSSRadialGradientValue(result)->setSecondRadius(radius);
    }

    CSSGradientColorStop stop;
    while (consumeCommaIncludingWhitespace(args)) {
        if (!consumeDeprecatedGradientColorStop(args, stop, cssParserMode))
            return nullptr;
        result->addStop(stop);
    }

    return result;
}

static bool consumeGradientColorStops(CSSParserTokenRange& range, CSSParserMode cssParserMode, CSSGradientValue* gradient)
{
    bool supportsColorHints = gradient->gradientType() == CSSLinearGradient || gradient->gradientType() == CSSRadialGradient;

    // The first color stop cannot be a color hint.
    bool previousStopWasColorHint = true;
    do {
        CSSGradientColorStop stop;
        stop.m_color = consumeColor(range, cssParserMode);
        // Two hints in a row are not allowed.
        if (!stop.m_color && (!supportsColorHints || previousStopWasColorHint))
            return false;
        previousStopWasColorHint = !stop.m_color;
        stop.m_position = consumeLengthOrPercent(range, cssParserMode, ValueRangeAll);
        if (!stop.m_color && !stop.m_position)
            return false;
        gradient->addStop(stop);
    } while (consumeCommaIncludingWhitespace(range));

    // The last color stop cannot be a color hint.
    if (previousStopWasColorHint)
        return false;

    // Must have 2 or more stops to be valid.
    return gradient->stopCount() >= 2;
}

static CSSValue* consumeDeprecatedRadialGradient(CSSParserTokenRange& args, CSSParserMode cssParserMode, CSSGradientRepeat repeating)
{
    CSSRadialGradientValue* result = CSSRadialGradientValue::create(repeating, CSSPrefixedRadialGradient);
    CSSValue* centerX = nullptr;
    CSSValue* centerY = nullptr;
    consumeOneOrTwoValuedPosition(args, cssParserMode, UnitlessQuirk::Forbid, centerX, centerY);
    if ((centerX || centerY) && !consumeCommaIncludingWhitespace(args))
        return nullptr;

    result->setFirstX(toCSSPrimitiveValue(centerX));
    result->setSecondX(toCSSPrimitiveValue(centerX));
    result->setFirstY(toCSSPrimitiveValue(centerY));
    result->setSecondY(toCSSPrimitiveValue(centerY));

    CSSPrimitiveValue* shape = consumeIdent<CSSValueCircle, CSSValueEllipse>(args);
    CSSPrimitiveValue* sizeKeyword = consumeIdent<CSSValueClosestSide, CSSValueClosestCorner, CSSValueFarthestSide, CSSValueFarthestCorner, CSSValueContain, CSSValueCover>(args);
    if (!shape)
        shape = consumeIdent<CSSValueCircle, CSSValueEllipse>(args);
    result->setShape(shape);
    result->setSizingBehavior(sizeKeyword);

    // Or, two lengths or percentages
    if (!shape && !sizeKeyword) {
        CSSPrimitiveValue* horizontalSize = consumeLengthOrPercent(args, cssParserMode, ValueRangeAll);
        CSSPrimitiveValue* verticalSize = nullptr;
        if (horizontalSize) {
            verticalSize = consumeLengthOrPercent(args, cssParserMode, ValueRangeAll);
            if (!verticalSize)
                return nullptr;
            consumeCommaIncludingWhitespace(args);
            result->setEndHorizontalSize(horizontalSize);
            result->setEndVerticalSize(verticalSize);
        }
    } else {
        consumeCommaIncludingWhitespace(args);
    }
    if (!consumeGradientColorStops(args, cssParserMode, result))
        return nullptr;

    return result;
}

static CSSValue* consumeRadialGradient(CSSParserTokenRange& args, CSSParserMode cssParserMode, CSSGradientRepeat repeating)
{
    CSSRadialGradientValue* result = CSSRadialGradientValue::create(repeating, CSSRadialGradient);

    CSSPrimitiveValue* shape = nullptr;
    CSSPrimitiveValue* sizeKeyword = nullptr;
    CSSPrimitiveValue* horizontalSize = nullptr;
    CSSPrimitiveValue* verticalSize = nullptr;

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
            CSSPrimitiveValue* center = consumeLengthOrPercent(args, cssParserMode, ValueRangeAll);
            if (!center)
                break;
            if (horizontalSize)
                return nullptr;
            horizontalSize = center;
            center = consumeLengthOrPercent(args, cssParserMode, ValueRangeAll);
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
    if (shape && shape->getValueID() == CSSValueCircle && verticalSize)
        return nullptr;
    // Ellipses must have 0 or 2 length/percentages.
    if (shape && shape->getValueID() == CSSValueEllipse && horizontalSize && !verticalSize)
        return nullptr;
    // If there's only one size, it must be a length.
    if (!verticalSize && horizontalSize && horizontalSize->isPercentage())
        return nullptr;
    if ((horizontalSize && horizontalSize->isCalculatedPercentageWithLength())
        || (verticalSize && verticalSize->isCalculatedPercentageWithLength()))
        return nullptr;

    result->setShape(shape);
    result->setSizingBehavior(sizeKeyword);
    result->setEndHorizontalSize(horizontalSize);
    result->setEndVerticalSize(verticalSize);

    CSSValue* centerX = nullptr;
    CSSValue* centerY = nullptr;
    if (args.peek().id() == CSSValueAt) {
        args.consumeIncludingWhitespace();
        consumePosition(args, cssParserMode, UnitlessQuirk::Forbid, centerX, centerY);
        if (!(centerX && centerY))
            return nullptr;
        result->setFirstX(centerX);
        result->setFirstY(centerY);
        // Right now, CSS radial gradients have the same start and end centers.
        result->setSecondX(centerX);
        result->setSecondY(centerY);
    }

    if ((shape || sizeKeyword || horizontalSize || centerX || centerY) && !consumeCommaIncludingWhitespace(args))
        return nullptr;
    if (!consumeGradientColorStops(args, cssParserMode, result))
        return nullptr;
    return result;
}

static CSSValue* consumeLinearGradient(CSSParserTokenRange& args, CSSParserMode cssParserMode, CSSGradientRepeat repeating, CSSGradientType gradientType)
{
    CSSLinearGradientValue* result = CSSLinearGradientValue::create(repeating, gradientType);

    bool expectComma = true;
    CSSPrimitiveValue* angle = consumeAngle(args);
    if (angle) {
        result->setAngle(angle);
    } else if (gradientType == CSSPrefixedLinearGradient || consumeIdent<CSSValueTo>(args)) {
        CSSPrimitiveValue* endX = consumeIdent<CSSValueLeft, CSSValueRight>(args);
        CSSPrimitiveValue* endY = consumeIdent<CSSValueBottom, CSSValueTop>(args);
        if (!endX && !endY) {
            if (gradientType == CSSLinearGradient)
                return nullptr;
            endY = CSSPrimitiveValue::createIdentifier(CSSValueTop);
            expectComma = false;
        } else if (!endX) {
            endX = consumeIdent<CSSValueLeft, CSSValueRight>(args);
        }

        result->setFirstX(endX);
        result->setFirstY(endY);
    } else {
        expectComma = false;
    }

    if (expectComma && !consumeCommaIncludingWhitespace(args))
        return nullptr;
    if (!consumeGradientColorStops(args, cssParserMode, result))
        return nullptr;
    return result;
}

CSSValue* consumeImageOrNone(CSSParserTokenRange& range, CSSParserContext context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeImage(range, context);
}

static CSSValue* consumeCrossFade(CSSParserTokenRange& args, CSSParserContext context)
{
    CSSValue* fromImageValue = consumeImageOrNone(args, context);
    if (!fromImageValue || !consumeCommaIncludingWhitespace(args))
        return nullptr;
    CSSValue* toImageValue = consumeImageOrNone(args, context);
    if (!toImageValue || !consumeCommaIncludingWhitespace(args))
        return nullptr;

    CSSPrimitiveValue* percentage = nullptr;
    const CSSParserToken& percentageArg = args.consumeIncludingWhitespace();
    if (percentageArg.type() == PercentageToken)
        percentage = CSSPrimitiveValue::create(clampTo<double>(percentageArg.numericValue() / 100, 0, 1), CSSPrimitiveValue::UnitType::Number);
    else if (percentageArg.type() == NumberToken)
        percentage = CSSPrimitiveValue::create(clampTo<double>(percentageArg.numericValue(), 0, 1), CSSPrimitiveValue::UnitType::Number);

    if (!percentage)
        return nullptr;
    return CSSCrossfadeValue::create(fromImageValue, toImageValue, percentage);
}

static CSSValue* consumePaint(CSSParserTokenRange& args, CSSParserContext context)
{
    DCHECK(RuntimeEnabledFeatures::cssPaintAPIEnabled());

    CSSCustomIdentValue* name = consumeCustomIdent(args);
    if (!name)
        return nullptr;

    return CSSPaintValue::create(name);
}

static CSSValue* consumeGeneratedImage(CSSParserTokenRange& range, CSSParserContext context)
{
    CSSValueID id = range.peek().functionId();
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    CSSValue* result = nullptr;
    if (id == CSSValueRadialGradient) {
        result = consumeRadialGradient(args, context.mode(), NonRepeating);
    } else if (id == CSSValueRepeatingRadialGradient) {
        result = consumeRadialGradient(args, context.mode(), Repeating);
    } else if (id == CSSValueWebkitLinearGradient) {
        // FIXME: This should send a deprecation message.
        if (context.useCounter())
            context.useCounter()->count(UseCounter::DeprecatedWebKitLinearGradient);
        result = consumeLinearGradient(args, context.mode(), NonRepeating, CSSPrefixedLinearGradient);
    } else if (id == CSSValueWebkitRepeatingLinearGradient) {
        // FIXME: This should send a deprecation message.
        if (context.useCounter())
            context.useCounter()->count(UseCounter::DeprecatedWebKitRepeatingLinearGradient);
        result = consumeLinearGradient(args, context.mode(), Repeating, CSSPrefixedLinearGradient);
    } else if (id == CSSValueRepeatingLinearGradient) {
        result = consumeLinearGradient(args, context.mode(), Repeating, CSSLinearGradient);
    } else if (id == CSSValueLinearGradient) {
        result = consumeLinearGradient(args, context.mode(), NonRepeating, CSSLinearGradient);
    } else if (id == CSSValueWebkitGradient) {
        // FIXME: This should send a deprecation message.
        if (context.useCounter())
            context.useCounter()->count(UseCounter::DeprecatedWebKitGradient);
        result = consumeDeprecatedGradient(args, context.mode());
    } else if (id == CSSValueWebkitRadialGradient) {
        // FIXME: This should send a deprecation message.
        if (context.useCounter())
            context.useCounter()->count(UseCounter::DeprecatedWebKitRadialGradient);
        result = consumeDeprecatedRadialGradient(args, context.mode(), NonRepeating);
    } else if (id == CSSValueWebkitRepeatingRadialGradient) {
        if (context.useCounter())
            context.useCounter()->count(UseCounter::DeprecatedWebKitRepeatingRadialGradient);
        result = consumeDeprecatedRadialGradient(args, context.mode(), Repeating);
    } else if (id == CSSValueWebkitCrossFade) {
        result = consumeCrossFade(args, context);
    } else if (id == CSSValuePaint) {
        result = RuntimeEnabledFeatures::cssPaintAPIEnabled() ? consumePaint(args, context) : nullptr;
    }
    if (!result || !args.atEnd())
        return nullptr;
    range = rangeCopy;
    return result;
}

static CSSValue* createCSSImageValueWithReferrer(const AtomicString& rawValue, const CSSParserContext& context)
{
    CSSValue* imageValue = CSSImageValue::create(rawValue, context.completeURL(rawValue));
    toCSSImageValue(imageValue)->setReferrer(context.referrer());
    return imageValue;
}

static CSSValue* consumeImageSet(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    CSSImageSetValue* imageSet = CSSImageSetValue::create();
    do {
        AtomicString urlValue = consumeUrlAsStringView(args).toAtomicString();
        if (urlValue.isNull())
            return nullptr;

        CSSValue* image = createCSSImageValueWithReferrer(urlValue, context);
        imageSet->append(*image);

        const CSSParserToken& token = args.consumeIncludingWhitespace();
        if (token.type() != DimensionToken)
            return nullptr;
        if (token.value() != "x")
            return nullptr;
        DCHECK(token.unitType() == CSSPrimitiveValue::UnitType::Unknown);
        double imageScaleFactor = token.numericValue();
        if (imageScaleFactor <= 0)
            return nullptr;
        imageSet->append(*CSSPrimitiveValue::create(imageScaleFactor, CSSPrimitiveValue::UnitType::Number));
    } while (consumeCommaIncludingWhitespace(args));
    if (!args.atEnd())
        return nullptr;
    range = rangeCopy;
    return imageSet;
}

static bool isGeneratedImage(CSSValueID id)
{
    return id == CSSValueLinearGradient || id == CSSValueRadialGradient
        || id == CSSValueRepeatingLinearGradient || id == CSSValueRepeatingRadialGradient
        || id == CSSValueWebkitLinearGradient || id == CSSValueWebkitRadialGradient
        || id == CSSValueWebkitRepeatingLinearGradient || id == CSSValueWebkitRepeatingRadialGradient
        || id == CSSValueWebkitGradient || id == CSSValueWebkitCrossFade || id == CSSValuePaint;
}

CSSValue* consumeImage(CSSParserTokenRange& range, CSSParserContext context, ConsumeGeneratedImage generatedImage)
{
    AtomicString uri = consumeUrlAsStringView(range).toAtomicString();
    if (!uri.isNull())
        return createCSSImageValueWithReferrer(uri, context);
    if (range.peek().type() == FunctionToken) {
        CSSValueID id = range.peek().functionId();
        if (id == CSSValueWebkitImageSet)
            return consumeImageSet(range, context);
        if (generatedImage == ConsumeGeneratedImage::Allow && isGeneratedImage(id))
            return consumeGeneratedImage(range, context);
    }
    return nullptr;
}

} // namespace CSSPropertyParserHelpers
*/

} // namespace WebCore
