// Copyright 2016 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

static inline bool isCSSWideKeyword(CSSValueID id)
{
    return id == CSSValueInitial || id == CSSValueInherit || id == CSSValueUnset || id == CSSValueRevert || id == CSSValueDefault;
}

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

static Optional<double> consumeNumberOrPercentDividedBy100Raw(CSSParserTokenRange& range, ValueRange valueRange = ValueRangeAll)
{
    if (auto percent = consumePercentRaw(range, valueRange))
        return *percent / 100.0;
    if (auto number = consumeNumberRaw(range, valueRange))
        return *number;
    return WTF::nullopt;
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
        return CSSValuePool::singleton().createValue(std::round(std::max(m_calcValue->doubleValue(), minimumValue)), CSSUnitType::CSS_NUMBER);
    }

    template<typename IntType> Optional<IntType> consumeIntegerTypeRaw(double minimumValue)
    {
        if (!m_calcValue)
            return WTF::nullopt;
        m_sourceRange = m_range;
        return clampTo<IntType>(std::round(std::max(m_calcValue->doubleValue(), minimumValue)));
    }

    RefPtr<CSSPrimitiveValue> consumeNumber()
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        return CSSValuePool::singleton().createValue(m_calcValue->doubleValue(), CSSUnitType::CSS_NUMBER);
    }

    Optional<double> consumeNumberRaw()
    {
        if (!m_calcValue || m_calcValue->category() != CalculationCategory::Number)
            return WTF::nullopt;
        m_sourceRange = m_range;
        return m_calcValue->doubleValue();
    }

    Optional<double> consumePercentRaw()
    {
        if (!m_calcValue)
            return WTF::nullopt;
        auto category = m_calcValue->category();
        if (category != CalculationCategory::Percent)
            return WTF::nullopt;
        m_sourceRange = m_range;
        return m_calcValue->doubleValue();
    }

    Optional<AngleRaw> consumeAngleRaw()
    {
        if (!m_calcValue || m_calcValue->category() != CalculationCategory::Angle)
            return WTF::nullopt;
        m_sourceRange = m_range;
        return { { m_calcValue->primitiveType(), m_calcValue->doubleValue() } };
    }

    Optional<LengthRaw> consumeLengthRaw()
    {
        if (!m_calcValue || m_calcValue->category() != CalculationCategory::Length)
            return WTF::nullopt;
        m_sourceRange = m_range;
        return { { m_calcValue->primitiveType(), m_calcValue->doubleValue() } };
    }

    Optional<LengthOrPercentRaw> consumeLengthOrPercentRaw()
    {
        if (!m_calcValue)
            return WTF::nullopt;

        switch (m_calcValue->category()) {
        case CalculationCategory::Length:
            m_sourceRange = m_range;
            return { LengthRaw({ m_calcValue->primitiveType(), m_calcValue->doubleValue() }) };
        case CalculationCategory::Percent:
        case CalculationCategory::PercentLength:
        case CalculationCategory::PercentNumber:
            m_sourceRange = m_range;
            return { { m_calcValue->doubleValue() } };
        default:
            return WTF::nullopt;
        }
    }

private:
    CSSParserTokenRange& m_sourceRange;
    CSSParserTokenRange m_range;
    RefPtr<CSSCalcValue> m_calcValue;
};

template<typename IntType> Optional<IntType> consumeIntegerTypeRaw(CSSParserTokenRange& range, double minimumValue)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == NumberToken) {
        if (token.numericValueType() == NumberValueType || token.numericValue() < minimumValue)
            return WTF::nullopt;
        return clampTo<IntType>(range.consumeIncludingWhitespace().numericValue());
    }

    if (token.type() != FunctionToken)
        return WTF::nullopt;

    CalcParser calcParser(range, CalculationCategory::Number);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() != CalculationCategory::Number)
            return WTF::nullopt;
        return calcParser.consumeIntegerTypeRaw<IntType>(minimumValue);
    }

    return WTF::nullopt;
}

Optional<int> consumeIntegerRaw(CSSParserTokenRange& range, double minimumValue)
{
    return consumeIntegerTypeRaw<int>(range, minimumValue);
}

RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range, double minimumValue)
{
    if (auto integer = consumeIntegerRaw(range, minimumValue))
        return CSSValuePool::singleton().createValue(*integer, CSSUnitType::CSS_NUMBER);
    return nullptr;
}

Optional<unsigned> consumePositiveIntegerRaw(CSSParserTokenRange& range)
{
    return consumeIntegerTypeRaw<unsigned>(range, 1);
}

RefPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange& range)
{
    if (auto integer = consumePositiveIntegerRaw(range))
        return CSSValuePool::singleton().createValue(*integer, CSSUnitType::CSS_NUMBER);
    return nullptr;
}

Optional<double> consumeNumberRaw(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == NumberToken) {
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return WTF::nullopt;
        return range.consumeIncludingWhitespace().numericValue();
    }

    if (token.type() != FunctionToken)
        return WTF::nullopt;

    CalcParser calcParser(range, CalculationCategory::Number, valueRange);
    return calcParser.consumeNumberRaw();
}

// FIXME: Work out if this can just call consumeNumberRaw
RefPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == FunctionToken) {
        CalcParser calcParser(range, CalculationCategory::Number, valueRange);
        if (const auto* calcValue = calcParser.value()) {
            if (calcValue->category() == CalculationCategory::Number)
                return calcParser.consumeValue();
        }
        return nullptr;
    }

    if (auto number = consumeNumberRaw(range, valueRange))
        return CSSValuePool::singleton().createValue(*number, token.unitType());

    return nullptr;
}

#if !ENABLE(VARIATION_FONTS)
static inline bool divisibleBy100(double value)
{
    return static_cast<int>(value / 100) * 100 == value;
}
#endif

Optional<double> consumeFontWeightNumberRaw(CSSParserTokenRange& range)
{
    // Values less than or equal to 0 or greater than or equal to 1000 are parse errors.
    auto& token = range.peek();
    if (token.type() == NumberToken && token.numericValue() >= 1 && token.numericValue() <= 1000
#if !ENABLE(VARIATION_FONTS)
        && token.numericValueType() == IntegerValueType && divisibleBy100(token.numericValue())
#endif
    )
        return consumeNumberRaw(range);

    if (token.type() != FunctionToken)
        return WTF::nullopt;

    // "[For calc()], the used value resulting from an expression must be clamped to the range allowed in the target context."
    CalcParser calcParser(range, CalculationCategory::Number, ValueRangeAll);
    if (auto result = calcParser.consumeNumberRaw(); result
#if !ENABLE(VARIATION_FONTS)
        && *result > 0 && *result < 1000 && divisibleBy100(*result)
#endif
    )
        return std::min(std::max(*result, std::nextafter(0., 1.)), std::nextafter(1000., 0.));

    return WTF::nullopt;
}

RefPtr<CSSPrimitiveValue> consumeFontWeightNumber(CSSParserTokenRange& range)
{
    if (auto result = consumeFontWeightNumberRaw(range))
        return CSSValuePool::singleton().createValue(*result, CSSUnitType::CSS_NUMBER);
    return nullptr;
}

inline bool shouldAcceptUnitlessValue(double value, CSSParserMode cssParserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    // FIXME: Presentational HTML attributes shouldn't use the CSS parser for lengths

    if (value == 0 && unitlessZero == UnitlessZeroQuirk::Allow)
        return true;

    if (isUnitLessValueParsingEnabledForMode(cssParserMode))
        return true;
    
    return cssParserMode == HTMLQuirksMode && unitless == UnitlessQuirk::Allow;
}

Optional<LengthRaw> consumeLengthRaw(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        switch (token.unitType()) {
        case CSSUnitType::CSS_QUIRKY_EMS:
            if (cssParserMode != UASheetMode)
                return WTF::nullopt;
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
            return WTF::nullopt;
        }
        if ((valueRange == ValueRangeNonNegative && token.numericValue() < 0) || std::isinf(token.numericValue()))
            return WTF::nullopt;
        return { { token.unitType(), range.consumeIncludingWhitespace().numericValue() } };
    }
    if (token.type() == NumberToken) {
        if (!shouldAcceptUnitlessValue(token.numericValue(), cssParserMode, unitless, UnitlessZeroQuirk::Allow)
            || (valueRange == ValueRangeNonNegative && token.numericValue() < 0))
            return WTF::nullopt;
        if (std::isinf(token.numericValue()))
            return WTF::nullopt;
        return { { CSSUnitType::CSS_PX, range.consumeIncludingWhitespace().numericValue() } };
    }

    if (token.type() != FunctionToken)
        return WTF::nullopt;

    CalcParser calcParser(range, CalculationCategory::Length, valueRange);
    return calcParser.consumeLengthRaw();
}

RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == FunctionToken) {
        CalcParser calcParser(range, CalculationCategory::Length, valueRange);
        if (calcParser.value() && calcParser.value()->category() == CalculationCategory::Length)
            return calcParser.consumeValue();
    }

    if (auto result = consumeLengthRaw(range, cssParserMode, valueRange, unitless))
        return CSSValuePool::singleton().createValue(result->value, result->type);

    return nullptr;
}

Optional<double> consumePercentRaw(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == PercentageToken) {
        if (std::isinf(token.numericValue()) || (valueRange == ValueRangeNonNegative && token.numericValue() < 0))
            return WTF::nullopt;
        return range.consumeIncludingWhitespace().numericValue();
    }

    if (token.type() != FunctionToken)
        return WTF::nullopt;

    CalcParser calcParser(range, CalculationCategory::Percent, valueRange);
    return calcParser.consumePercentRaw();
}

RefPtr<CSSPrimitiveValue> consumePercent(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == FunctionToken) {
        CalcParser calcParser(range, CalculationCategory::Percent, valueRange);
        if (const CSSCalcValue* calculation = calcParser.value()) {
            if (calculation->category() == CalculationCategory::Percent)
                return calcParser.consumeValue();
        }
        return nullptr;
    }

    if (auto percent = consumePercentRaw(range, valueRange))
        return CSSValuePool::singleton().createValue(*percent, CSSUnitType::CSS_PERCENTAGE);

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

Optional<LengthOrPercentRaw> consumeLengthOrPercentRaw(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken || token.type() == NumberToken) {
        if (auto result = consumeLengthRaw(range, cssParserMode, valueRange, unitless))
            return { *result };
        return WTF::nullopt;
    }
    if (token.type() == PercentageToken) {
        if (auto result = consumePercentRaw(range, valueRange))
            return { *result };
        return WTF::nullopt;
    }

    if (token.type() != FunctionToken)
        return WTF::nullopt;

    CalcParser calcParser(range, CalculationCategory::Length, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (canConsumeCalcValue(calculation->category(), cssParserMode))
            return calcParser.consumeLengthOrPercentRaw();
    }
    return WTF::nullopt;
}

RefPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == FunctionToken) {
        CalcParser calcParser(range, CalculationCategory::Length, valueRange);
        if (const CSSCalcValue* calculation = calcParser.value()) {
            if (canConsumeCalcValue(calculation->category(), cssParserMode))
                return calcParser.consumeValue();
        }
        return nullptr;
    }

    if (auto result = consumeLengthOrPercentRaw(range, cssParserMode, valueRange, unitless)) {
        return switchOn(*result, [] (LengthRaw length) {
            return CSSValuePool::singleton().createValue(length.value, length.type);
        }, [] (double percentage) {
            return CSSValuePool::singleton().createValue(percentage, CSSUnitType::CSS_PERCENTAGE);
        });
    }
    return nullptr;
}

Optional<AngleRaw> consumeAngleRaw(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        auto unitType = token.unitType();
        switch (unitType) {
        case CSSUnitType::CSS_DEG:
        case CSSUnitType::CSS_RAD:
        case CSSUnitType::CSS_GRAD:
        case CSSUnitType::CSS_TURN:
            return { { unitType, range.consumeIncludingWhitespace().numericValue() } };
        default:
            return WTF::nullopt;
        }
    }

    if (token.type() == NumberToken && shouldAcceptUnitlessValue(token.numericValue(), cssParserMode, unitless, unitlessZero))
        return { { CSSUnitType::CSS_DEG, range.consumeIncludingWhitespace().numericValue() } };

    if (token.type() != FunctionToken)
        return WTF::nullopt;

    CalcParser calcParser(range, CalculationCategory::Angle, ValueRangeAll);
    return calcParser.consumeAngleRaw();
}

RefPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == FunctionToken) {
        CalcParser calcParser(range, CalculationCategory::Angle, ValueRangeAll);
        if (const CSSCalcValue* calculation = calcParser.value()) {
            if (calculation->category() == CalculationCategory::Angle)
                return calcParser.consumeValue();
        }
        return nullptr;
    }

    if (auto angle = consumeAngleRaw(range, cssParserMode, unitless, unitlessZero))
        return CSSValuePool::singleton().createValue(angle->value, angle->type);

    return nullptr;
}

static RefPtr<CSSPrimitiveValue> consumeAngleOrPercent(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
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
    if (token.type() == NumberToken && shouldAcceptUnitlessValue(token.numericValue(), cssParserMode, unitless, unitlessZero))
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
    bool acceptUnitless = token.type() == NumberToken && unitless == UnitlessQuirk::Allow && shouldAcceptUnitlessValue(token.numericValue(), cssParserMode, unitless, UnitlessZeroQuirk::Allow);
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

Optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return WTF::nullopt;
    return range.consumeIncludingWhitespace().id();
}

RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange& range)
{
    if (auto result = consumeIdentRaw(range))
        return CSSValuePool::singleton().createIdentifierValue(*result);
    return nullptr;
}

Optional<CSSValueID> consumeIdentRangeRaw(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
{
    if (range.peek().id() < lower || range.peek().id() > upper)
        return WTF::nullopt;
    return consumeIdentRaw(range);
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

static Optional<double> consumeOptionalAlpha(CSSParserTokenRange& range)
{
    if (!consumeSlashIncludingWhitespace(range))
        return 1.0;

    if (auto alphaParameter = consumeNumberOrPercentDividedBy100Raw(range))
        return clampTo(*alphaParameter, 0.0, 1.0);

    return WTF::nullopt;
}

static Optional<double> consumeHue(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto angle = consumeAngleRaw(range, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid))
        return CSSPrimitiveValue::computeDegrees(angle->type, angle->value);

    return consumeNumberRaw(range);
}

static double normalizeHue(double hue)
{
    return std::fmod(std::fmod(hue, 360.0) + 360.0, 360.0);
}

enum class RGBComponentType { Number, Percentage };

static uint8_t clampRGBComponent(double value, RGBComponentType componentType)
{
    if (componentType == RGBComponentType::Percentage)
        value = value / 100.0 * 255.0;

    return convertPrescaledSRGBAFloatToSRGBAByte(value);
}

enum class RGBOrHSLSeparatorSyntax { Commas, WhitespaceSlash };

static bool consumeRGBOrHSLSeparator(CSSParserTokenRange& args, RGBOrHSLSeparatorSyntax syntax)
{
    if (syntax == RGBOrHSLSeparatorSyntax::Commas)
        return consumeCommaIncludingWhitespace(args);
    return true;
}

static bool consumeRGBOrHSLAlphaSeparator(CSSParserTokenRange& args, RGBOrHSLSeparatorSyntax syntax)
{
    if (syntax == RGBOrHSLSeparatorSyntax::Commas)
        return consumeCommaIncludingWhitespace(args);
    return consumeSlashIncludingWhitespace(args);
}

static Optional<double> consumeRGBOrHSLOptionalAlpha(CSSParserTokenRange& args, RGBOrHSLSeparatorSyntax syntax)
{
    if (!consumeRGBOrHSLAlphaSeparator(args, syntax))
        return 1.0;

    return consumeNumberOrPercentDividedBy100Raw(args);
}

static Color parseRGBParameters(CSSParserTokenRange& range, const CSSParserContext&)
{
    ASSERT(range.peek().functionId() == CSSValueRgb || range.peek().functionId() == CSSValueRgba);
    auto args = consumeFunction(range);

    struct InitialComponent {
        double value;
        RGBComponentType type;
    };

    auto consumeInitialComponent = [](auto& args) -> Optional<InitialComponent> {
        if (auto number = consumeNumberRaw(args))
            return { { *number, RGBComponentType::Number } };
        if (auto percent = consumePercentRaw(args))
            return { { *percent, RGBComponentType::Percentage } };
        return WTF::nullopt;
    };

    auto consumeComponent = [](auto& args, auto componentType) {
        switch (componentType) {
        case RGBComponentType::Percentage:
            return consumePercentRaw(args);
        case RGBComponentType::Number:
            return consumeNumberRaw(args);
        }
    };

    auto initialComponent = consumeInitialComponent(args);
    if (!initialComponent)
        return { };

    auto componentType = initialComponent->type;
    auto red = initialComponent->value;

    auto syntax = consumeCommaIncludingWhitespace(args) ? RGBOrHSLSeparatorSyntax::Commas : RGBOrHSLSeparatorSyntax::WhitespaceSlash;
    
    auto green = consumeComponent(args, componentType);
    if (!green)
        return { };

    if (!consumeRGBOrHSLSeparator(args, syntax))
        return { };

    auto blue = consumeComponent(args, componentType);
    if (!blue)
        return { };

    auto alpha = consumeRGBOrHSLOptionalAlpha(args, syntax);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto normalizedRed = clampRGBComponent(red, componentType);
    auto normalizedGreen = clampRGBComponent(*green, componentType);
    auto normalizedBlue = clampRGBComponent(*blue, componentType);
    auto normalizedAlpha = convertFloatAlphaTo<uint8_t>(*alpha);

    return SRGBA<uint8_t> { normalizedRed, normalizedGreen, normalizedBlue, normalizedAlpha };
}

static Color parseHSLParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueHsl || range.peek().functionId() == CSSValueHsla);
    auto args = consumeFunction(range);

    auto hue = consumeHue(args, context);
    if (!hue)
        return { };

    auto syntax = consumeCommaIncludingWhitespace(args) ? RGBOrHSLSeparatorSyntax::Commas : RGBOrHSLSeparatorSyntax::WhitespaceSlash;

    auto saturation = consumePercentRaw(args);
    if (!saturation)
        return { };

    if (!consumeRGBOrHSLSeparator(args, syntax))
        return { };

    auto lightness = consumePercentRaw(args);
    if (!lightness)
        return { };

    auto alpha = consumeRGBOrHSLOptionalAlpha(args, syntax);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto normalizedHue = normalizeHue(*hue);
    auto normalizedSaturation = clampTo<double>(*saturation, 0.0, 100.0);
    auto normalizedLightness = clampTo<double>(*lightness, 0.0, 100.0);
    auto normalizedAlpha = clampTo<double>(*alpha, 0.0, 1.0);

    return convertColor<SRGBA<uint8_t>>(HSLA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedSaturation), static_cast<float>(normalizedLightness), static_cast<float>(normalizedAlpha) });
}

struct WhitenessBlackness {
    double whiteness;
    double blackness;
};
static WhitenessBlackness normalizeWhitenessBlackness(double whiteness, double blackness)
{
    WhitenessBlackness result;

    //   Values outside of these ranges are not invalid, but are clamped to the
    //   ranges defined here at computed-value time.
    result.whiteness = clampTo<double>(whiteness, 0, 100);
    result.blackness = clampTo<double>(blackness, 0, 100);

    //   If the sum of these two arguments is greater than 100%, then at
    //   computed-value time they are further normalized to add up to 100%, with
    //   the same relative ratio.
    if (auto sum = result.whiteness + result.blackness; sum >= 100) {
        result.whiteness *= 100.0 / sum;
        result.blackness *= 100.0 / sum;
    }

    return result;
}

static Color parseHWBParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueHwb);
    auto args = consumeFunction(range);

    auto hue = consumeHue(args, context);
    if (!hue)
        return { };

    auto whiteness = consumePercentRaw(args);
    if (!whiteness)
        return { };

    auto blackness = consumePercentRaw(args);
    if (!blackness)
        return { };

    auto alpha = consumeOptionalAlpha(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto normalizedHue = normalizeHue(*hue);
    auto [normalizedWhitness, normalizedBlackness] = normalizeWhitenessBlackness(*whiteness, *blackness);

    return convertColor<SRGBA<uint8_t>>(HWBA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedWhitness), static_cast<float>(normalizedBlackness), static_cast<float>(*alpha) });
}

static Color parseLabParameters(CSSParserTokenRange& range, const CSSParserContext&)
{
    ASSERT(range.peek().functionId() == CSSValueLab);
    auto args = consumeFunction(range);

    auto lightness = consumePercentRaw(args);
    if (!lightness)
        return { };

    auto aValue = consumeNumberRaw(args);
    if (!aValue)
        return { };

    auto bValue = consumeNumberRaw(args);
    if (!bValue)
        return { };

    auto alpha = consumeOptionalAlpha(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    return Lab<float> { static_cast<float>(*lightness), static_cast<float>(*aValue), static_cast<float>(*bValue), static_cast<float>(*alpha) };
}

static Color parseLCHParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueLch);
    auto args = consumeFunction(range);

    auto lightness = consumePercentRaw(args);
    if (!lightness)
        return { };

    auto chroma = consumeNumberRaw(args);
    if (!chroma)
        return { };

    auto hue = consumeHue(args, context);
    if (!hue)
        return { };

    auto alpha = consumeOptionalAlpha(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto normalizedHue = normalizeHue(*hue);

    return convertColor<Lab<float>>(LCHA<float> { static_cast<float>(*lightness), static_cast<float>(*chroma), static_cast<float>(normalizedHue), static_cast<float>(*alpha) });
}

template<typename ColorType> static Color parseColorFunctionForRGBTypes(CSSParserTokenRange& args)
{
    ASSERT(args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueDisplayP3 || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020 || args.peek().id() == CSSValueSRGB);
    consumeIdentRaw(args);

    double channels[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; ++i) {
        auto value = consumeNumberOrPercentDividedBy100Raw(args);
        if (!value)
            break;
        channels[i] = *value;
    }

    auto alpha = consumeOptionalAlpha(args);
    if (!alpha)
        return { };

    return ColorType { clampTo<float>(channels[0], 0.0, 1.0), clampTo<float>(channels[1], 0.0, 1.0), clampTo<float>(channels[2], 0.0, 1.0), static_cast<float>(*alpha) };
}

static Color parseColorFunctionForLabParameters(CSSParserTokenRange& args)
{
    ASSERT(args.peek().id() == CSSValueLab);
    consumeIdentRaw(args);

    double channels[3] = { 0, 0, 0 };
    [&] {
        auto lightness = consumePercentRaw(args);
        if (!lightness)
            return;
        channels[0] = *lightness;

        auto aValue = consumeNumberRaw(args);
        if (!aValue)
            return;
        channels[1] = *aValue;

        auto bValue = consumeNumberRaw(args);
        if (!bValue)
            return;
        channels[2] = *bValue;
    }();

    auto alpha = consumeOptionalAlpha(args);
    if (!alpha)
        return { };

    return Lab<float> { static_cast<float>(channels[0]), static_cast<float>(channels[1]), static_cast<float>(channels[2]), static_cast<float>(*alpha) };
}

static Color parseColorFunctionForXYZParameters(CSSParserTokenRange& args)
{
    ASSERT(args.peek().id() == CSSValueXyz);
    consumeIdentRaw(args);

    double channels[3] = { 0, 0, 0 };
    [&] {
        auto x = consumeNumberRaw(args);
        if (!x)
            return;
        channels[0] = *x;

        auto y = consumeNumberRaw(args);
        if (!y)
            return;
        channels[1] = *y;

        auto z = consumeNumberRaw(args);
        if (!z)
            return;
        channels[2] = *z;
    }();

    auto alpha = consumeOptionalAlpha(args);
    if (!alpha)
        return { };

    return XYZA<float, WhitePoint::D50> { static_cast<float>(channels[0]), static_cast<float>(channels[1]), static_cast<float>(channels[2]), static_cast<float>(*alpha) };
}

static Color parseColorFunctionParameters(CSSParserTokenRange& range)
{
    ASSERT(range.peek().functionId() == CSSValueColor);
    auto args = consumeFunction(range);

    Color color;
    switch (args.peek().id()) {
    case CSSValueA98Rgb:
        color = parseColorFunctionForRGBTypes<A98RGB<float>>(args);
        break;
    case CSSValueDisplayP3:
        color = parseColorFunctionForRGBTypes<DisplayP3<float>>(args);
        break;
    case CSSValueLab:
        color = parseColorFunctionForLabParameters(args);
        break;
    case CSSValueProphotoRgb:
        color = parseColorFunctionForRGBTypes<ProPhotoRGB<float>>(args);
        break;
    case CSSValueRec2020:
        color = parseColorFunctionForRGBTypes<Rec2020<float>>(args);
        break;
    case CSSValueSRGB:
        color = parseColorFunctionForRGBTypes<SRGBA<float>>(args);
        break;
    case CSSValueXyz:
        color = parseColorFunctionForXYZParameters(args);
        break;
    default:
        return { };
    }

    if (!color.isValid())
        return { };

    // FIXME: Support the comma-separated list of fallback color values.

    if (!args.atEnd())
        return { };

    return color;
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

static Color parseColorFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSParserTokenRange colorRange = range;
    CSSValueID functionId = range.peek().functionId();
    Color color;
    switch (functionId) {
    case CSSValueRgb:
    case CSSValueRgba:
        color = parseRGBParameters(colorRange, context);
        break;
    case CSSValueHsl:
    case CSSValueHsla:
        color = parseHSLParameters(colorRange, context);
        break;
    case CSSValueHwb:
        color = parseHWBParameters(colorRange, context);
        break;
    case CSSValueLab:
        color = parseLabParameters(colorRange, context);
        break;
    case CSSValueLch:
        color = parseLCHParameters(colorRange, context);
        break;
    case CSSValueColor:
        color = parseColorFunctionParameters(colorRange);
        break;
    default:
        return { };
    }
    if (color.isValid())
        range = colorRange;
    return color;
}

Color consumeColorWorkerSafe(CSSParserTokenRange& range, const CSSParserContext& context)
{
    Color result;
    auto keyword = range.peek().id();
    if (StyleColor::isColorKeyword(keyword)) {
        // FIXME: Need a worker-safe way to compute the system colors.
        //        For now, we detect the system color, but then intentionally fail parsing.
        if (StyleColor::isSystemColor(keyword))
            return { };
        if (!isValueAllowedInMode(keyword, context.mode))
            return { };
        result = StyleColor::colorFromKeyword(keyword, { });
        range.consumeIncludingWhitespace();
    }

    if (auto parsedColor = parseHexColor(range, false))
        result = *parsedColor;
    else
        result = parseColorFunction(range, context);

    if (!range.atEnd())
        return { };

    return result;
}

RefPtr<CSSPrimitiveValue> consumeColor(CSSParserTokenRange& range, const CSSParserContext& context, bool acceptQuirkyColors)
{
    auto keyword = range.peek().id();
    if (StyleColor::isColorKeyword(keyword)) {
        if (!isValueAllowedInMode(keyword, context.mode))
            return nullptr;
        return consumeIdent(range);
    }
    Color color;
    if (auto parsedColor = parseHexColor(range, acceptQuirkyColors))
        color = *parsedColor;
    else {
        color = parseColorFunction(range, context);
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

static PositionCoordinates positionFromOneValue(CSSPrimitiveValue& value)
{
    bool valueAppliesToYAxisOnly = isVerticalPositionKeywordOnly(value);
    if (valueAppliesToYAxisOnly)
        return { CSSPrimitiveValue::createIdentifier(CSSValueCenter), value };
    return { value, CSSPrimitiveValue::createIdentifier(CSSValueCenter) };
}

static Optional<PositionCoordinates> positionFromTwoValues(CSSPrimitiveValue& value1, CSSPrimitiveValue& value2)
{
    bool mustOrderAsXY = isHorizontalPositionKeywordOnly(value1) || isVerticalPositionKeywordOnly(value2) || !value1.isValueID() || !value2.isValueID();
    bool mustOrderAsYX = isVerticalPositionKeywordOnly(value1) || isHorizontalPositionKeywordOnly(value2);
    if (mustOrderAsXY && mustOrderAsYX)
        return WTF::nullopt;
    if (mustOrderAsYX)
        return PositionCoordinates { value2, value1 };
    return PositionCoordinates { value1, value2 };
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
static Optional<PositionCoordinates> backgroundPositionFromThreeValues(const std::array<CSSPrimitiveValue*, 5>& values)
{
    RefPtr<CSSPrimitiveValue> resultX;
    RefPtr<CSSPrimitiveValue> resultY;

    CSSPrimitiveValue* center = nullptr;
    for (int i = 0; values[i]; i++) {
        CSSPrimitiveValue* currentValue = values[i];
        if (!currentValue->isValueID())
            return WTF::nullopt;

        CSSValueID id = currentValue->valueID();
        if (id == CSSValueCenter) {
            if (center)
                return WTF::nullopt;
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
                return WTF::nullopt;
            resultX = result;
        } else {
            ASSERT(id == CSSValueTop || id == CSSValueBottom);
            if (resultY)
                return WTF::nullopt;
            resultY = result;
        }
    }

    if (center) {
        ASSERT(resultX || resultY);
        if (resultX && resultY)
            return WTF::nullopt;
        if (!resultX)
            resultX = center;
        else
            resultY = center;
    }

    ASSERT(resultX && resultY);
    return PositionCoordinates { resultX.releaseNonNull(), resultY.releaseNonNull() };
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
static Optional<PositionCoordinates> positionFromFourValues(const std::array<CSSPrimitiveValue*, 5>& values)
{
    RefPtr<CSSPrimitiveValue> resultX;
    RefPtr<CSSPrimitiveValue> resultY;

    for (int i = 0; values[i]; i++) {
        CSSPrimitiveValue* currentValue = values[i];
        if (!currentValue->isValueID())
            return WTF::nullopt;

        CSSValueID id = currentValue->valueID();
        if (id == CSSValueCenter)
            return WTF::nullopt;

        RefPtr<CSSPrimitiveValue> result;
        if (values[i + 1] && !values[i + 1]->isValueID())
            result = CSSPropertyParserHelpersInternal::createPrimitiveValuePair(currentValue, values[++i]);
        else
            result = currentValue;

        if (id == CSSValueLeft || id == CSSValueRight) {
            if (resultX)
                return WTF::nullopt;
            resultX = result;
        } else {
            ASSERT(id == CSSValueTop || id == CSSValueBottom);
            if (resultY)
                return WTF::nullopt;
            resultY = result;
        }
    }

    ASSERT(resultX && resultY);
    return PositionCoordinates { resultX.releaseNonNull(), resultY.releaseNonNull() };
}

// FIXME: This may consume from the range upon failure. The background
// shorthand works around it, but we should just fix it here.
Optional<PositionCoordinates> consumePositionCoordinates(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless, PositionSyntax positionSyntax)
{
    auto value1 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value1)
        return WTF::nullopt;

    auto value2 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value2)
        return positionFromOneValue(*value1);

    auto value3 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value3)
        return positionFromTwoValues(*value1, *value2);

    auto value4 = consumePositionComponent(range, cssParserMode, unitless);
    
    std::array<CSSPrimitiveValue*, 5> values {
        value1.get(),
        value2.get(),
        value3.get(),
        value4.get(),
        nullptr
    };
    
    if (value4)
        return positionFromFourValues(values);
    
    if (positionSyntax != PositionSyntax::BackgroundPosition)
        return WTF::nullopt;
    
    return backgroundPositionFromThreeValues(values);
}

RefPtr<CSSPrimitiveValue> consumePosition(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless, PositionSyntax positionSyntax)
{
    if (auto coordinates = consumePositionCoordinates(range, cssParserMode, unitless, positionSyntax))
        return CSSPropertyParserHelpersInternal::createPrimitiveValuePair(WTFMove(coordinates->x), WTFMove(coordinates->y));
    return nullptr;
}

Optional<PositionCoordinates> consumeOneOrTwoValuedPositionCoordinates(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    auto value1 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value1)
        return WTF::nullopt;
    auto value2 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value2)
        return positionFromOneValue(*value1);
    return positionFromTwoValues(*value1, *value2);
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
static RefPtr<CSSPrimitiveValue> consumeDeprecatedGradientStopColor(CSSParserTokenRange& args, const CSSParserContext& context)
{
    if (args.peek().id() == CSSValueCurrentcolor)
        return nullptr;
    return consumeColor(args, context);
}

static bool consumeDeprecatedGradientColorStop(CSSParserTokenRange& range, CSSGradientColorStop& stop, const CSSParserContext& context)
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
        auto value = consumeNumberOrPercentDividedBy100Raw(args);
        if (!value)
            return false;
        position = *value;

        if (!consumeCommaIncludingWhitespace(args))
            return false;
    }

    stop.position = CSSValuePool::singleton().createValue(position, CSSUnitType::CSS_NUMBER);
    stop.color = consumeDeprecatedGradientStopColor(args, context);
    return stop.color && args.atEnd();
}

static RefPtr<CSSValue> consumeDeprecatedGradient(CSSParserTokenRange& args, const CSSParserContext& context)
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
        if (!consumeDeprecatedGradientColorStop(args, stop, context))
            return nullptr;
        result->addStop(WTFMove(stop));
    }

    result->doneAddingStops();
    return result;
}

static bool consumeGradientColorStops(CSSParserTokenRange& range, const CSSParserContext& context, CSSGradientValue& gradient)
{
    bool supportsColorHints = gradient.gradientType() == CSSLinearGradient || gradient.gradientType() == CSSRadialGradient || gradient.gradientType() == CSSConicGradient;
    
    auto consumeStopPosition = [&] {
        return gradient.gradientType() == CSSConicGradient
            ? consumeAngleOrPercent(range, context.mode, ValueRangeAll, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow)
            : consumeLengthOrPercent(range, context.mode, ValueRangeAll);
    };

    // The first color stop cannot be a color hint.
    bool previousStopWasColorHint = true;
    do {
        CSSGradientColorStop stop { consumeColor(range, context), consumeStopPosition(), { } };
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

static RefPtr<CSSValue> consumeDeprecatedRadialGradient(CSSParserTokenRange& args, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    auto result = CSSRadialGradientValue::create(repeating, CSSPrefixedRadialGradient);

    auto centerCoordinate = consumeOneOrTwoValuedPositionCoordinates(args, context.mode, UnitlessQuirk::Forbid);
    if (centerCoordinate && !consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto shape = consumeIdent<CSSValueCircle, CSSValueEllipse>(args);
    auto sizeKeyword = consumeIdent<CSSValueClosestSide, CSSValueClosestCorner, CSSValueFarthestSide, CSSValueFarthestCorner, CSSValueContain, CSSValueCover>(args);
    if (!shape)
        shape = consumeIdent<CSSValueCircle, CSSValueEllipse>(args);

    // Or, two lengths or percentages
    if (!shape && !sizeKeyword) {
        auto horizontalSize = consumeLengthOrPercent(args, context.mode, ValueRangeNonNegative);
        RefPtr<CSSPrimitiveValue> verticalSize;
        if (horizontalSize) {
            verticalSize = consumeLengthOrPercent(args, context.mode, ValueRangeNonNegative);
            if (!verticalSize)
                return nullptr;
            consumeCommaIncludingWhitespace(args);
            result->setEndHorizontalSize(WTFMove(horizontalSize));
            result->setEndVerticalSize(WTFMove(verticalSize));
        }
    } else {
        consumeCommaIncludingWhitespace(args);
    }

    if (!consumeGradientColorStops(args, context, result))
        return nullptr;

    if (centerCoordinate) {
        result->setFirstX(centerCoordinate->x.copyRef());
        result->setFirstY(centerCoordinate->y.copyRef());
        result->setSecondX(WTFMove(centerCoordinate->x));
        result->setSecondY(WTFMove(centerCoordinate->y));
    }
    result->setShape(WTFMove(shape));
    result->setSizingBehavior(WTFMove(sizeKeyword));

    return result;
}

static RefPtr<CSSValue> consumeRadialGradient(CSSParserTokenRange& args, const CSSParserContext& context, CSSGradientRepeat repeating)
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
            auto center = consumeLengthOrPercent(args, context.mode, ValueRangeNonNegative);
            if (!center)
                break;
            if (horizontalSize)
                return nullptr;
            horizontalSize = center;
            center = consumeLengthOrPercent(args, context.mode, ValueRangeNonNegative);
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
        
        auto centerCoordinate = consumePositionCoordinates(args, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
        if (!centerCoordinate)
            return nullptr;
        
        centerX = WTFMove(centerCoordinate->x);
        centerY = WTFMove(centerCoordinate->y);
        
        result->setFirstX(centerX.copyRef());
        result->setFirstY(centerY.copyRef());
        
        // Right now, CSS radial gradients have the same start and end centers.
        result->setSecondX(centerX.copyRef());
        result->setSecondY(centerY.copyRef());
    }

    if ((shape || sizeKeyword || horizontalSize || centerX || centerY) && !consumeCommaIncludingWhitespace(args))
        return nullptr;
    if (!consumeGradientColorStops(args, context, *result))
        return nullptr;

    result->setShape(WTFMove(shape));
    result->setSizingBehavior(WTFMove(sizeKeyword));
    result->setEndHorizontalSize(WTFMove(horizontalSize));
    result->setEndVerticalSize(WTFMove(verticalSize));

    return result;
}

static RefPtr<CSSValue> consumeLinearGradient(CSSParserTokenRange& args, const CSSParserContext& context, CSSGradientRepeat repeating, CSSGradientType gradientType)
{
    RefPtr<CSSLinearGradientValue> result = CSSLinearGradientValue::create(repeating, gradientType);

    bool expectComma = true;
    RefPtr<CSSPrimitiveValue> angle = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
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
    if (!consumeGradientColorStops(args, context, *result))
        return nullptr;
    return result;
}

static RefPtr<CSSValue> consumeConicGradient(CSSParserTokenRange& args, const CSSParserContext& context, CSSGradientRepeat repeating)
{
#if ENABLE(CSS_CONIC_GRADIENTS)
    RefPtr<CSSConicGradientValue> result = CSSConicGradientValue::create(repeating);

    bool expectComma = false;
    if (args.peek().type() == IdentToken) {
        if (consumeIdent<CSSValueFrom>(args)) {
            // FIXME: Unlike linear-gradient, conic-gradients are not specified to allow unitless 0 angles - https://www.w3.org/TR/css-images-4/#valdef-conic-gradient-angle.
            auto angle = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
            if (!angle)
                return nullptr;
            result->setAngle(WTFMove(angle));
            expectComma = true;
        }

        if (consumeIdent<CSSValueAt>(args)) {
            auto centerCoordinate = consumePositionCoordinates(args, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
            if (!centerCoordinate)
                return nullptr;

            result->setFirstX(centerCoordinate->x.copyRef());
            result->setFirstY(centerCoordinate->y.copyRef());

            // Right now, conic gradients have the same start and end centers.
            result->setSecondX(WTFMove(centerCoordinate->x));
            result->setSecondY(WTFMove(centerCoordinate->y));

            expectComma = true;
        }
    }

    if (expectComma && !consumeCommaIncludingWhitespace(args))
        return nullptr;
    if (!consumeGradientColorStops(args, context, *result))
        return nullptr;
    return result;
#else
    UNUSED_PARAM(args);
    UNUSED_PARAM(context);
    UNUSED_PARAM(repeating);
    return nullptr;
#endif
}

RefPtr<CSSValue> consumeImageOrNone(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeImage(range, context);
}

static RefPtr<CSSValue> consumeCrossFade(CSSParserTokenRange& args, const CSSParserContext& context, bool prefixed)
{
    auto fromImageValue = consumeImageOrNone(args, context);
    if (!fromImageValue || !consumeCommaIncludingWhitespace(args))
        return nullptr;
    auto toImageValue = consumeImageOrNone(args, context);
    if (!toImageValue || !consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto percentage = consumeNumberOrPercentDividedBy100Raw(args);
    if (!percentage)
        return nullptr;
    auto percentageValue = CSSValuePool::singleton().createValue(clampTo<double>(*percentage, 0, 1), CSSUnitType::CSS_NUMBER);
    return CSSCrossfadeValue::create(fromImageValue.releaseNonNull(), toImageValue.releaseNonNull(), WTFMove(percentageValue), prefixed);
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

static RefPtr<CSSValue> consumeGeneratedImage(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSValueID id = range.peek().functionId();
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    RefPtr<CSSValue> result;
    if (id == CSSValueRadialGradient)
        result = consumeRadialGradient(args, context, NonRepeating);
    else if (id == CSSValueRepeatingRadialGradient)
        result = consumeRadialGradient(args, context, Repeating);
    else if (id == CSSValueWebkitLinearGradient)
        result = consumeLinearGradient(args, context, NonRepeating, CSSPrefixedLinearGradient);
    else if (id == CSSValueWebkitRepeatingLinearGradient)
        result = consumeLinearGradient(args, context, Repeating, CSSPrefixedLinearGradient);
    else if (id == CSSValueRepeatingLinearGradient)
        result = consumeLinearGradient(args, context, Repeating, CSSLinearGradient);
    else if (id == CSSValueLinearGradient)
        result = consumeLinearGradient(args, context, NonRepeating, CSSLinearGradient);
    else if (id == CSSValueWebkitGradient)
        result = consumeDeprecatedGradient(args, context);
    else if (id == CSSValueWebkitRadialGradient)
        result = consumeDeprecatedRadialGradient(args, context, NonRepeating);
    else if (id == CSSValueWebkitRepeatingRadialGradient)
        result = consumeDeprecatedRadialGradient(args, context, Repeating);
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
        parsedValue = consumeSingleShadow(args, context, false, false);
    else {
        if (args.atEnd())
            return filterValue;

        if (filterType == CSSValueHueRotate)
            parsedValue = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
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

RefPtr<CSSShadowValue> consumeSingleShadow(CSSParserTokenRange& range, const CSSParserContext& context, bool allowInset, bool allowSpread)
{
    RefPtr<CSSPrimitiveValue> style;
    RefPtr<CSSPrimitiveValue> color;
    RefPtr<CSSPrimitiveValue> horizontalOffset;
    RefPtr<CSSPrimitiveValue> verticalOffset;
    RefPtr<CSSPrimitiveValue> blurRadius;
    RefPtr<CSSPrimitiveValue> spreadDistance;
    
    for (size_t i = 0; i < 3; i++) {
        if (range.atEnd())
            break;
        
        const CSSParserToken& nextToken = range.peek();
        // If we have come to a comma (e.g. if this range represents a comma-separated list of <shadow>s), we are done parsing this <shadow>.
        if (nextToken.type() == CommaToken)
            break;
        
        if (nextToken.id() == CSSValueInset) {
            if (!allowInset || style)
                return nullptr;
            style = consumeIdent(range);
            continue;
        }
        
        auto maybeColor = consumeColor(range, context);
        if (maybeColor) {
            // If we just parsed a color but already had one, the given token range is not a valid <shadow>.
            if (color)
                return nullptr;
            color = maybeColor;
            continue;
        }
        
        // If the current token is neither a color nor the `inset` keyword, it must be the lengths component of this value.
        if (horizontalOffset || verticalOffset || blurRadius || spreadDistance) {
            // If we've already parsed these lengths, the given value is invalid as there cannot be two lengths components in a single <shadow> value.
            return nullptr;
        }
        horizontalOffset = consumeLength(range, context.mode, ValueRangeAll);
        if (!horizontalOffset)
            return nullptr;
        verticalOffset = consumeLength(range, context.mode, ValueRangeAll);
        if (!verticalOffset)
            return nullptr;

        const CSSParserToken& token = range.peek();
        // The explicit check for calc() is unfortunate. This is ensuring that we only fail parsing if there is a length, but it fails the range check.
        if (token.type() == DimensionToken || token.type() == NumberToken || (token.type() == FunctionToken && CSSCalcValue::isCalcFunction(token.functionId()))) {
            blurRadius = consumeLength(range, context.mode, ValueRangeNonNegative);
            if (!blurRadius)
                return nullptr;
        }

        if (blurRadius && allowSpread)
            spreadDistance = consumeLength(range, context.mode, ValueRangeAll);
    }
    
    // In order for this to be a valid <shadow>, at least these lengths must be present.
    if (!horizontalOffset || !verticalOffset)
        return nullptr;
    return CSSShadowValue::create(WTFMove(horizontalOffset), WTFMove(verticalOffset), WTFMove(blurRadius), WTFMove(spreadDistance), WTFMove(style), WTFMove(color));
}

RefPtr<CSSValue> consumeImage(CSSParserTokenRange& range, const CSSParserContext& context, OptionSet<AllowedImageType> allowedImageTypes)
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

Optional<CSSValueID> consumeFontVariantCSS21Raw(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueNormal, CSSValueSmallCaps>(range);
}

Optional<CSSValueID> consumeFontWeightKeywordValueRaw(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueNormal, CSSValueBold, CSSValueBolder, CSSValueLighter>(range);
}

Optional<FontWeightRaw> consumeFontWeightRaw(CSSParserTokenRange& range)
{
    if (auto result = consumeFontWeightKeywordValueRaw(range))
        return { *result };
    if (auto result = consumeFontWeightNumberRaw(range))
        return { *result };
    return WTF::nullopt;
}

Optional<CSSValueID> consumeFontStretchKeywordValueRaw(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueUltraCondensed, CSSValueExtraCondensed, CSSValueCondensed, CSSValueSemiCondensed, CSSValueNormal, CSSValueSemiExpanded, CSSValueExpanded, CSSValueExtraExpanded, CSSValueUltraExpanded>(range);
}

Optional<CSSValueID> consumeFontStyleKeywordValueRaw(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range);
}

Optional<FontStyleRaw> consumeFontStyleRaw(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    auto result = consumeFontStyleKeywordValueRaw(range);
    if (!result)
        return WTF::nullopt;

    auto ident = *result;
    if (ident == CSSValueNormal || ident == CSSValueItalic)
        return { { ident, WTF::nullopt } };
    ASSERT(ident == CSSValueOblique);
#if ENABLE(VARIATION_FONTS)
    if (!range.atEnd()) {
        // FIXME: This angle does specify that unitless 0 is allowed - see https://drafts.csswg.org/css-fonts-4/#valdef-font-style-oblique-angle
        if (auto angle = consumeAngleRaw(range, cssParserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow)) {
            if (isFontStyleAngleInRange(CSSPrimitiveValue::computeDegrees(angle->type, angle->value)))
                return { { CSSValueOblique, WTFMove(angle) } };
            return WTF::nullopt;
        }
    }
#else
    UNUSED_PARAM(cssParserMode);
#endif
    return { { CSSValueOblique, WTF::nullopt } };
}

String concatenateFamilyName(CSSParserTokenRange& range)
{
    StringBuilder builder;
    bool addedSpace = false;
    const CSSParserToken& firstToken = range.peek();
    while (range.peek().type() == IdentToken) {
        if (!builder.isEmpty()) {
            builder.append(' ');
            addedSpace = true;
        }
        builder.append(range.consumeIncludingWhitespace().value());
    }
    if (!addedSpace && isCSSWideKeyword(firstToken.id()))
        return String();
    return builder.toString();
}

String consumeFamilyNameRaw(CSSParserTokenRange& range)
{
    if (range.peek().type() == StringToken)
        return range.consumeIncludingWhitespace().value().toString();
    if (range.peek().type() != IdentToken)
        return String();
    return concatenateFamilyName(range);
}

Optional<CSSValueID> consumeGenericFamilyRaw(CSSParserTokenRange& range)
{
    return consumeIdentRangeRaw(range, CSSValueSerif, CSSValueWebkitBody);
}

Optional<WTF::Vector<FontFamilyRaw>> consumeFontFamilyRaw(CSSParserTokenRange& range)
{
    WTF::Vector<FontFamilyRaw> list;
    do {
        if (auto ident = consumeGenericFamilyRaw(range))
            list.append({ *ident });
        else {
            auto familyName = consumeFamilyNameRaw(range);
            if (familyName.isNull())
                return WTF::nullopt;
            list.append({ familyName });
        }
    } while (consumeCommaIncludingWhitespace(range));
    return list;
}

Optional<FontSizeRaw> consumeFontSizeRaw(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    if (range.peek().id() >= CSSValueXxSmall && range.peek().id() <= CSSValueLarger) {
        if (auto ident = consumeIdentRaw(range))
            return { *ident };
        return WTF::nullopt;
    }

    if (auto result = consumeLengthOrPercentRaw(range, cssParserMode, ValueRangeNonNegative, unitless))
        return { *result };

    return WTF::nullopt;
}

Optional<LineHeightRaw> consumeLineHeightRaw(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNormal) {
        if (auto ident = consumeIdentRaw(range))
            return { *ident };
        return WTF::nullopt;
    }

    if (auto number = consumeNumberRaw(range, ValueRangeNonNegative))
        return { *number };

    if (auto lengthOrPercent = consumeLengthOrPercentRaw(range, cssParserMode, ValueRangeNonNegative))
        return { *lengthOrPercent };

    return WTF::nullopt;
}

Optional<FontRaw> consumeFontWorkerSafe(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    // Let's check if there is an inherit or initial somewhere in the shorthand.
    CSSParserTokenRange rangeCopy = range;
    while (!rangeCopy.atEnd()) {
        CSSValueID id = rangeCopy.consumeIncludingWhitespace().id();
        if (id == CSSValueInherit || id == CSSValueInitial)
            return WTF::nullopt;
    }

    FontRaw result;

    while (!range.atEnd()) {
        CSSValueID id = range.peek().id();
        if (!result.style) {
            if ((result.style = consumeFontStyleRaw(range, cssParserMode)))
                continue;
        }
        if (!result.variantCaps && (id == CSSValueNormal || id == CSSValueSmallCaps)) {
            // Font variant in the shorthand is particular, it only accepts normal or small-caps.
            // See https://drafts.csswg.org/css-fonts/#propdef-font
            if ((result.variantCaps = consumeFontVariantCSS21Raw(range)))
                continue;
        }
        if (!result.weight) {
            if ((result.weight = consumeFontWeightRaw(range)))
                continue;
        }
        if (!result.stretch) {
            if ((result.stretch = consumeFontStretchKeywordValueRaw(range)))
                continue;
        }
        break;
    }

    if (range.atEnd())
        return WTF::nullopt;

    // Now a font size _must_ come.
    if (auto size = consumeFontSizeRaw(range, cssParserMode))
        result.size = *size;
    else
        return WTF::nullopt;

    if (range.atEnd())
        return WTF::nullopt;

    if (consumeSlashIncludingWhitespace(range)) {
        if (!(result.lineHeight = consumeLineHeightRaw(range, cssParserMode)))
            return WTF::nullopt;
    }

    // Font family must come now.
    if (auto family = consumeFontFamilyRaw(range))
        result.family = *family;
    else
        return WTF::nullopt;

    if (!range.atEnd())
        return WTF::nullopt;

    return result;
}

const AtomString& genericFontFamilyFromValueID(CSSValueID ident)
{
    switch (ident) {
    case CSSValueSerif:
        return serifFamily.get();
    case CSSValueSansSerif:
        return sansSerifFamily.get();
    case CSSValueCursive:
        return cursiveFamily.get();
    case CSSValueFantasy:
        return fantasyFamily.get();
    case CSSValueMonospace:
        return monospaceFamily.get();
    case CSSValueWebkitPictograph:
        return pictographFamily.get();
    case CSSValueSystemUi:
        return systemUiFamily.get();
    default:
        return emptyAtom();
    }
}

} // namespace CSSPropertyParserHelpers

} // namespace WebCore
