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

#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
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
#include "CalculationCategory.h"
#include "ColorConversion.h"
#include "ColorLuminance.h"
#include "Pair.h"
#include "RuntimeEnabledFeatures.h"
#include "StyleColor.h"
#include "WebKitFontFamilyNames.h"
#include <wtf/SortedArrayMap.h>
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

inline bool shouldAcceptUnitlessValue(double value, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    // FIXME: Presentational HTML attributes shouldn't use the CSS parser for lengths.

    if (value == 0 && unitlessZero == UnitlessZeroQuirk::Allow)
        return true;

    if (isUnitlessValueParsingEnabledForMode(parserMode))
        return true;
    
    return parserMode == HTMLQuirksMode && unitless == UnitlessQuirk::Allow;
}

static bool canConsumeCalcValue(CalculationCategory category, CSSParserMode parserMode)
{
    if (category == CalculationCategory::Length || category == CalculationCategory::Percent || category == CalculationCategory::PercentLength)
        return true;

    if (parserMode != SVGAttributeMode)
        return false;

    if (category == CalculationCategory::Number || category == CalculationCategory::PercentNumber)
        return true;

    return false;
}

// FIXME: consider pulling in the parsing logic from CSSCalcExpressionNodeParser.
class CalcParser {
public:
    explicit CalcParser(CSSParserTokenRange& range, CalculationCategory destinationCategory, ValueRange valueRange = ValueRange::All, const CSSCalcSymbolTable& symbolTable = { }, CSSValuePool& pool = CSSValuePool::singleton())
        : m_sourceRange(range)
        , m_range(range)
        , m_pool(pool)
    {
        const CSSParserToken& token = range.peek();
        auto functionId = token.functionId();
        if (CSSCalcValue::isCalcFunction(functionId))
            m_calcValue = CSSCalcValue::create(functionId, consumeFunction(m_range), destinationCategory, valueRange, symbolTable);
    }

    const CSSCalcValue* value() const { return m_calcValue.get(); }

    RefPtr<CSSPrimitiveValue> consumeValue()
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        return m_pool.createValue(WTFMove(m_calcValue));
    }

    RefPtr<CSSPrimitiveValue> consumeValueIfCategory(CalculationCategory category)
    {
        if (!m_calcValue || m_calcValue->category() != category)
            return nullptr;
        m_sourceRange = m_range;
        return m_pool.createValue(WTFMove(m_calcValue));
    }

    RefPtr<CSSPrimitiveValue> consumeInteger(double minimumValue)
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        return m_pool.createValue(std::round(std::max(m_calcValue->doubleValue(), minimumValue)), CSSUnitType::CSS_NUMBER);
    }

    template<typename IntType> std::optional<IntType> consumeIntegerTypeRaw(double minimumValue)
    {
        if (!m_calcValue)
            return std::nullopt;
        m_sourceRange = m_range;
        return clampTo<IntType>(std::round(std::max(m_calcValue->doubleValue(), minimumValue)));
    }

    RefPtr<CSSPrimitiveValue> consumeNumber()
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        return m_pool.createValue(m_calcValue->doubleValue(), CSSUnitType::CSS_NUMBER);
    }

    std::optional<double> consumeNumberRaw()
    {
        if (!m_calcValue || m_calcValue->category() != CalculationCategory::Number)
            return std::nullopt;
        m_sourceRange = m_range;
        return m_calcValue->doubleValue();
    }

    std::optional<double> consumePercentRaw()
    {
        if (!m_calcValue)
            return std::nullopt;
        auto category = m_calcValue->category();
        if (category != CalculationCategory::Percent)
            return std::nullopt;
        m_sourceRange = m_range;
        return m_calcValue->doubleValue();
    }

    std::optional<AngleRaw> consumeAngleRaw()
    {
        if (!m_calcValue || m_calcValue->category() != CalculationCategory::Angle)
            return std::nullopt;
        m_sourceRange = m_range;
        return { { m_calcValue->primitiveType(), m_calcValue->doubleValue() } };
    }

    std::optional<LengthRaw> consumeLengthRaw()
    {
        if (!m_calcValue || m_calcValue->category() != CalculationCategory::Length)
            return std::nullopt;
        m_sourceRange = m_range;
        return { { m_calcValue->primitiveType(), m_calcValue->doubleValue() } };
    }

    std::optional<LengthOrPercentRaw> consumeLengthOrPercentRaw()
    {
        if (!m_calcValue)
            return std::nullopt;

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
            return std::nullopt;
        }
    }

private:
    CSSParserTokenRange& m_sourceRange;
    CSSParserTokenRange m_range;
    RefPtr<CSSCalcValue> m_calcValue;
    CSSValuePool& m_pool;
};

// MARK: - Primitive value consumers for callers that know the token type.

// MARK: Integer (Raw)

template<typename IntType> static std::optional<IntType> consumeIntegerTypeRawWithKnownTokenTypeFunction(CSSParserTokenRange& range, double minimumValue)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Number);
    if (auto calculation = parser.value(); calculation && calculation->category() == CalculationCategory::Number)
        return parser.consumeIntegerTypeRaw<IntType>(minimumValue);

    return std::nullopt;
}

template<typename IntType> static std::optional<IntType> consumeIntegerTypeRawWithKnownTokenTypeNumber(CSSParserTokenRange& range, double minimumValue)
{
    ASSERT(range.peek().type() == NumberToken);
    
    if (range.peek().numericValueType() == NumberValueType || range.peek().numericValue() < minimumValue)
        return std::nullopt;
    return clampTo<IntType>(range.consumeIncludingWhitespace().numericValue());
}

// MARK: Integer (CSSPrimitiveValue - not maintaing calc)

template<typename IntType> static RefPtr<CSSPrimitiveValue> consumeIntegerTypeCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(CSSParserTokenRange& range, double minimumValue, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == FunctionToken);

    if (auto integer = consumeIntegerTypeRawWithKnownTokenTypeFunction<IntType>(range, minimumValue))
        return pool.createValue(*integer, CSSUnitType::CSS_NUMBER);
    return nullptr;
}

template<typename IntType> static RefPtr<CSSPrimitiveValue> consumeIntegerTypeCSSPrimitiveValueWithCalcWithKnownTokenTypeNumber(CSSParserTokenRange& range, double minimumValue, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == NumberToken);

    if (auto integer = consumeIntegerTypeRawWithKnownTokenTypeNumber<IntType>(range, minimumValue))
        return pool.createValue(*integer, CSSUnitType::CSS_NUMBER);
    return nullptr;
}

// MARK: Number (Raw)

static std::optional<double> validatedNumberRaw(double value, ValueRange valueRange)
{
    if (valueRange == ValueRange::NonNegative && value < 0)
        return std::nullopt;
    return value;
}

static std::optional<double> consumeNumberRawWithKnownTokenTypeFunction(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Number, valueRange, symbolTable);
    return parser.consumeNumberRaw();
}

static std::optional<double> consumeNumberRawWithKnownTokenTypeNumber(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange)
{
    ASSERT(range.peek().type() == NumberToken);
    
    if (auto validatedValue = validatedNumberRaw(range.peek().numericValue(), valueRange)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }
    return std::nullopt;
}

static std::optional<double> consumeNumberRawWithKnownTokenTypeIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange)
{
    ASSERT(range.peek().type() == IdentToken);

    if (auto variable = symbolTable.get(range.peek().id())) {
        switch (variable->type) {
        case CSSUnitType::CSS_NUMBER:
            if (auto validatedValue = validatedNumberRaw(variable->value, valueRange)) {
                range.consumeIncludingWhitespace();
                return validatedValue;
            }
            break;

        default:
            break;
        }
    }
    return std::nullopt;
}

// MARK: Number (CSSPrimitiveValue - maintaing calc)

static RefPtr<CSSPrimitiveValue> consumeNumberCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Number, valueRange, symbolTable, pool);
    return parser.consumeValueIfCategory(CalculationCategory::Number);
}

static RefPtr<CSSPrimitiveValue> consumeNumberCSSPrimitiveValueWithCalcWithKnownTokenTypeNumber(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == NumberToken);
    
    auto token = range.peek();
    
    if (auto validatedValue = validatedNumberRaw(token.numericValue(), valueRange)) {
        auto unitType = token.unitType();
        range.consumeIncludingWhitespace();
        return pool.createValue(*validatedValue, unitType);
    }
    return nullptr;
}


// MARK: Percent (raw)

static std::optional<double> validatedPercentRaw(double value, ValueRange valueRange)
{
    if (valueRange == ValueRange::NonNegative && value < 0)
        return std::nullopt;
    if (std::isinf(value))
        return std::nullopt;
    return value;
}

static std::optional<double> consumePercentRawWithKnownTokenTypeFunction(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Percent, valueRange, symbolTable);
    return parser.consumePercentRaw();
}

static std::optional<double> consumePercentRawWithKnownTokenTypePercentage(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange)
{
    ASSERT(range.peek().type() == PercentageToken);

    if (auto validatedValue = validatedPercentRaw(range.peek().numericValue(), valueRange)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }
    return std::nullopt;
}

static std::optional<double> consumePercentRawWithKnownTokenTypeIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange)
{
    ASSERT(range.peek().type() == IdentToken);

    if (auto variable = symbolTable.get(range.peek().id())) {
        switch (variable->type) {
        case CSSUnitType::CSS_PERCENTAGE:
            if (auto validatedValue = validatedPercentRaw(variable->value, valueRange)) {
                range.consumeIncludingWhitespace();
                return validatedValue;
            }
            break;

        default:
            break;
        }
    }
    return std::nullopt;
}


// MARK: Percent (CSSPrimitiveValue - maintaing calc)

static RefPtr<CSSPrimitiveValue> consumePercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Percent, valueRange, symbolTable, pool);
    return parser.consumeValueIfCategory(CalculationCategory::Percent);
}

static RefPtr<CSSPrimitiveValue> consumePercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentage(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == PercentageToken);

    if (auto validatedValue = validatedPercentRaw(range.peek().numericValue(), valueRange)) {
        range.consumeIncludingWhitespace();
        return pool.createValue(*validatedValue, CSSUnitType::CSS_PERCENTAGE);
    }
    return nullptr;
}

// MARK: Length (raw)

static std::optional<double> validatedLengthRaw(double value, ValueRange valueRange)
{
    if (valueRange == ValueRange::NonNegative && value < 0)
        return std::nullopt;
    if (std::isinf(value))
        return std::nullopt;
    return value;
}

static std::optional<LengthRaw> consumeLengthRawWithKnownTokenTypeFunction(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Length, valueRange, symbolTable);
    return parser.consumeLengthRaw();
}

static std::optional<LengthRaw> consumeLengthRawWithKnownTokenTypeDimension(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk)
{
    ASSERT(range.peek().type() == DimensionToken);

    auto& token = range.peek();

    auto unitType = token.unitType();
    switch (unitType) {
    case CSSUnitType::CSS_QUIRKY_EMS:
        if (parserMode != UASheetMode)
            return std::nullopt;
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
        return std::nullopt;
    }

    if (auto validatedValue = validatedLengthRaw(token.numericValue(), valueRange)) {
        range.consumeIncludingWhitespace();
        return { { unitType, *validatedValue } };
    }
    return std::nullopt;
}

static std::optional<LengthRaw> consumeLengthRawWithKnownTokenTypeNumber(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless)
{
    ASSERT(range.peek().type() == NumberToken);

    auto& token = range.peek();

    if (!shouldAcceptUnitlessValue(token.numericValue(), parserMode, unitless, UnitlessZeroQuirk::Allow))
        return std::nullopt;

    if (auto validatedValue = validatedLengthRaw(token.numericValue(), valueRange)) {
        range.consumeIncludingWhitespace();
        return { { CSSUnitType::CSS_PX, *validatedValue } };
    }
    return std::nullopt;
}

// MARK: Length (CSSPrimitiveValue - maintaing calc)

static RefPtr<CSSPrimitiveValue> consumeLengthCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Length, valueRange, symbolTable, pool);
    return parser.consumeValueIfCategory(CalculationCategory::Length);
}

static RefPtr<CSSPrimitiveValue> consumeLengthCSSPrimitiveValueWithCalcWithKnownTokenTypeDimension(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == DimensionToken);

    if (auto lengthRaw = consumeLengthRawWithKnownTokenTypeDimension(range, symbolTable, valueRange, parserMode, unitless))
        return pool.createValue(lengthRaw->value, lengthRaw->type);
    return nullptr;
}

static RefPtr<CSSPrimitiveValue> consumeLengthCSSPrimitiveValueWithCalcWithKnownTokenTypeNumber(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == NumberToken);

    if (auto lengthRaw = consumeLengthRawWithKnownTokenTypeNumber(range, symbolTable, valueRange, parserMode, unitless))
        return pool.createValue(lengthRaw->value, lengthRaw->type);
    return nullptr;
}

// MARK: Angle (raw)

static std::optional<AngleRaw> consumeAngleRawWithKnownTokenTypeFunction(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Angle, valueRange, symbolTable);
    return parser.consumeAngleRaw();
}

static std::optional<AngleRaw> consumeAngleRawWithKnownTokenTypeDimension(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == DimensionToken);

    auto unitType = range.peek().unitType();
    switch (unitType) {
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
        return { { unitType, range.consumeIncludingWhitespace().numericValue() } };
    default:
        break;
    }

    return std::nullopt;
}

static std::optional<AngleRaw> consumeAngleRawWithKnownTokenTypeNumber(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    ASSERT(range.peek().type() == NumberToken);

    if (shouldAcceptUnitlessValue(range.peek().numericValue(), parserMode, unitless, unitlessZero))
        return { { CSSUnitType::CSS_DEG, range.consumeIncludingWhitespace().numericValue() } };
    return std::nullopt;
}

// MARK: Angle (CSSPrimitiveValue - maintaing calc)

static RefPtr<CSSPrimitiveValue> consumeAngleCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Angle, valueRange, symbolTable, pool);
    return parser.consumeValueIfCategory(CalculationCategory::Angle);
}

static RefPtr<CSSPrimitiveValue> consumeAngleCSSPrimitiveValueWithCalcWithKnownTokenTypeDimension(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == DimensionToken);

    if (auto angleRaw = consumeAngleRawWithKnownTokenTypeDimension(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
        return pool.createValue(angleRaw->value, angleRaw->type);
    return nullptr;
}

static RefPtr<CSSPrimitiveValue> consumeAngleCSSPrimitiveValueWithCalcWithKnownTokenTypeNumber(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == NumberToken);

    if (auto angleRaw = consumeAngleRawWithKnownTokenTypeNumber(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
        return pool.createValue(angleRaw->value, angleRaw->type);
    return nullptr;
}


// MARK: Time (CSSPrimitiveValue - maintaing calc)

static RefPtr<CSSPrimitiveValue> consumeTimeCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Time, valueRange, symbolTable, pool);
    return parser.consumeValueIfCategory(CalculationCategory::Time);
}

static RefPtr<CSSPrimitiveValue> consumeTimeCSSPrimitiveValueWithCalcWithKnownTokenTypeDimension(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == DimensionToken);

    if (valueRange == ValueRange::NonNegative && range.peek().numericValue() < 0)
        return nullptr;

    if (auto unit = range.peek().unitType(); unit == CSSUnitType::CSS_MS || unit == CSSUnitType::CSS_S)
        return pool.createValue(range.consumeIncludingWhitespace().numericValue(), unit);

    return nullptr;
}

static RefPtr<CSSPrimitiveValue> consumeTimeCSSPrimitiveValueWithCalcWithKnownTokenTypeNumber(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == NumberToken);

    if (unitless == UnitlessQuirk::Allow && shouldAcceptUnitlessValue(range.peek().numericValue(), parserMode, unitless, UnitlessZeroQuirk::Allow)) {
        if (valueRange == ValueRange::NonNegative && range.peek().numericValue() < 0)
            return nullptr;
        return pool.createValue(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_MS);
    }
    return nullptr;
}

// MARK: Resolution (CSSPrimitiveValue - no calc)

static RefPtr<CSSPrimitiveValue> consumeResolutionCSSPrimitiveValueWithKnownTokenTypeDimension(CSSParserTokenRange& range, AllowXResolutionUnit allowX, CSSValuePool& pool)
{
    ASSERT(range.peek().type() == DimensionToken);

    if (auto unit = range.peek().unitType(); unit == CSSUnitType::CSS_DPPX || unit == CSSUnitType::CSS_DPI || unit == CSSUnitType::CSS_DPCM)
        return pool.createValue(range.consumeIncludingWhitespace().numericValue(), unit);
    if (allowX == AllowXResolutionUnit::Allow && range.peek().unitString() == "x")
        return pool.createValue(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_DPPX);

    return nullptr;
}


// MARK: - Primitive value consumers for callers that do NOT know the token type.

template<typename IntType> std::optional<IntType> consumeIntegerTypeRaw(CSSParserTokenRange& range, double minimumValue)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        return consumeIntegerTypeRawWithKnownTokenTypeFunction<IntType>(range, minimumValue);

    case NumberToken:
        return consumeIntegerTypeRawWithKnownTokenTypeNumber<IntType>(range, minimumValue);
    
    default:
        break;
    }

    return std::nullopt;
}

template<typename IntType> RefPtr<CSSPrimitiveValue> consumeIntegerType(CSSParserTokenRange& range, double minimumValue, CSSValuePool& pool)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        return consumeIntegerTypeCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction<IntType>(range, minimumValue, pool);

    case NumberToken:
        return consumeIntegerTypeCSSPrimitiveValueWithCalcWithKnownTokenTypeNumber<IntType>(range, minimumValue, pool);

    default:
        break;
    }

    return nullptr;
}

std::optional<int> consumeIntegerRaw(CSSParserTokenRange& range, double minimumValue)
{
    return consumeIntegerTypeRaw<int>(range, minimumValue);
}

RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range, double minimumValue)
{
    return consumeIntegerType<int>(range, minimumValue, CSSValuePool::singleton());
}

std::optional<unsigned> consumePositiveIntegerRaw(CSSParserTokenRange& range)
{
    return consumeIntegerTypeRaw<unsigned>(range, 1);
}

RefPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange& range)
{
    return consumeIntegerType<unsigned>(range, 1, CSSValuePool::singleton());
}

std::optional<double> consumeNumberRaw(CSSParserTokenRange& range, ValueRange valueRange)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        return consumeNumberRawWithKnownTokenTypeFunction(range, { }, valueRange);

    case NumberToken:
        return consumeNumberRawWithKnownTokenTypeNumber(range, { }, valueRange);

    default:
        break;
    }

    return std::nullopt;
}

RefPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange& range, ValueRange valueRange)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        return consumeNumberCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(range, { }, valueRange, CSSValuePool::singleton());

    case NumberToken:
        return consumeNumberCSSPrimitiveValueWithCalcWithKnownTokenTypeNumber(range, { }, valueRange, CSSValuePool::singleton());

    default:
        break;
    }

    return nullptr;
}

std::optional<LengthRaw> consumeLengthRaw(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    auto& token = range.peek();
    
    switch (token.type()) {
    case FunctionToken:
        return consumeLengthRawWithKnownTokenTypeFunction(range, { }, valueRange, parserMode, unitless);

    case DimensionToken:
        return consumeLengthRawWithKnownTokenTypeDimension(range, { }, valueRange, parserMode, unitless);
    
    case NumberToken:
        return consumeLengthRawWithKnownTokenTypeNumber(range, { }, valueRange, parserMode, unitless);

    default:
        break;
    }

    return std::nullopt;
}

RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        return consumeLengthCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(range, { }, valueRange, parserMode, unitless, CSSValuePool::singleton());

    case DimensionToken:
        return consumeLengthCSSPrimitiveValueWithCalcWithKnownTokenTypeDimension(range, { }, valueRange, parserMode, unitless, CSSValuePool::singleton());
    
    case NumberToken:
        return consumeLengthCSSPrimitiveValueWithCalcWithKnownTokenTypeNumber(range, { }, valueRange, parserMode, unitless, CSSValuePool::singleton());

    default:
        break;
    }

    return nullptr;
}

std::optional<double> consumePercentRaw(CSSParserTokenRange& range, ValueRange valueRange)
{
    const auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        return consumePercentRawWithKnownTokenTypeFunction(range, { }, valueRange);

    case PercentageToken:
        return consumePercentRawWithKnownTokenTypePercentage(range, { }, valueRange);

    default:
        break;
    }

    return std::nullopt;
}

RefPtr<CSSPrimitiveValue> consumePercent(CSSParserTokenRange& range, ValueRange valueRange)
{
    return consumePercentWorkerSafe(range, valueRange, CSSValuePool::singleton());
}

RefPtr<CSSPrimitiveValue> consumePercentWorkerSafe(CSSParserTokenRange& range, ValueRange valueRange, CSSValuePool& pool)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        return consumePercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(range, { }, valueRange, pool);

    case PercentageToken:
        return consumePercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentage(range, { }, valueRange, pool);

    default:
        break;
    }

    return nullptr;
}

std::optional<AngleRaw> consumeAngleRaw(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        return consumeAngleRawWithKnownTokenTypeFunction(range, { }, ValueRange::All, parserMode, unitless, unitlessZero);

    case DimensionToken:
        return consumeAngleRawWithKnownTokenTypeDimension(range, { }, ValueRange::All, parserMode, unitless, unitlessZero);

    case NumberToken:
        return consumeAngleRawWithKnownTokenTypeNumber(range, { }, ValueRange::All, parserMode, unitless, unitlessZero);
        break;

    default:
        break;
    }

    return std::nullopt;
}

RefPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    return consumeAngleWorkerSafe(range, parserMode, CSSValuePool::singleton(), unitless, unitlessZero);
}

RefPtr<CSSPrimitiveValue> consumeAngleWorkerSafe(CSSParserTokenRange& range, CSSParserMode parserMode, CSSValuePool& pool, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        return consumeAngleCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(range, { }, ValueRange::All, parserMode, unitless, unitlessZero, pool);

    case DimensionToken:
        return consumeAngleCSSPrimitiveValueWithCalcWithKnownTokenTypeDimension(range, { }, ValueRange::All, parserMode, unitless, unitlessZero, pool);

    case NumberToken:
        return consumeAngleCSSPrimitiveValueWithCalcWithKnownTokenTypeNumber(range, { }, ValueRange::All, parserMode, unitless, unitlessZero, pool);
    
    default:
        break;
    }
    
    return nullptr;
}

RefPtr<CSSPrimitiveValue> consumeTime(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        return consumeTimeCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(range, { }, valueRange, parserMode, unitless, CSSValuePool::singleton());

    case DimensionToken:
        return consumeTimeCSSPrimitiveValueWithCalcWithKnownTokenTypeDimension(range, { }, valueRange, parserMode, unitless, CSSValuePool::singleton());

    case NumberToken:
        return consumeTimeCSSPrimitiveValueWithCalcWithKnownTokenTypeNumber(range, { }, valueRange, parserMode, unitless, CSSValuePool::singleton());

    default:
        break;
    }
    
    return nullptr;
}

RefPtr<CSSPrimitiveValue> consumeResolution(CSSParserTokenRange& range, AllowXResolutionUnit allowX)
{
    // NOTE: Unlike the other types, calc() does not work with <resolution>.

    auto& token = range.peek();

    switch (token.type()) {
    case DimensionToken:
        return consumeResolutionCSSPrimitiveValueWithKnownTokenTypeDimension(range, allowX, CSSValuePool::singleton());
    
    default:
        break;
    }

    return nullptr;
}

// MARK: - Combination consumers (token type unknown by caller).

static RefPtr<CSSPrimitiveValue> consumeAngleOrPercent(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, CSSValuePool& pool = CSSValuePool::singleton())
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        if (auto value = consumeAngleCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(range, { }, valueRange, parserMode, unitless, unitlessZero, pool))
            return value;
        return consumePercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunction(range, { }, valueRange, pool);

    case DimensionToken:
        return consumeAngleCSSPrimitiveValueWithCalcWithKnownTokenTypeDimension(range, { }, valueRange, parserMode, unitless, unitlessZero, pool);
    
    case NumberToken:
        return consumeAngleCSSPrimitiveValueWithCalcWithKnownTokenTypeNumber(range, { }, valueRange, parserMode, unitless, unitlessZero, pool);

    case PercentageToken:
        return consumePercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentage(range, { }, valueRange, pool);
    
    default:
        break;
    }

    return nullptr;
}

std::optional<LengthOrPercentRaw> consumeLengthOrPercentRaw(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    auto convertToLengthOrPercentRaw = [](auto result) -> std::optional<LengthOrPercentRaw> {
        if (result)
            return { *result };
        return std::nullopt;
    };

    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken: {
        // FIXME: Should this be using trying to generate the calc with both Length and Percent destination category types?
        CalcParser parser(range, CalculationCategory::Length, valueRange);
        if (auto calculation = parser.value(); calculation && canConsumeCalcValue(calculation->category(), parserMode))
            return parser.consumeLengthOrPercentRaw();
        break;
    }

    case DimensionToken:
        return convertToLengthOrPercentRaw(consumeLengthRawWithKnownTokenTypeDimension(range, { }, valueRange, parserMode, unitless));
    
    case NumberToken:
        return convertToLengthOrPercentRaw(consumeLengthRawWithKnownTokenTypeNumber(range, { }, valueRange, parserMode, unitless));

    case PercentageToken:
        return convertToLengthOrPercentRaw(consumePercentRawWithKnownTokenTypePercentage(range, { }, valueRange));

    default:
        break;
    }

    return std::nullopt;
}

RefPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken: {
        // FIXME: Should this be using trying to generate the calc with both Length and Percent destination category types?
        CalcParser parser(range, CalculationCategory::Length, valueRange);
        if (auto calculation = parser.value(); calculation && canConsumeCalcValue(calculation->category(), parserMode))
            return parser.consumeValue();
        break;
    }

    case DimensionToken:
        return consumeLengthCSSPrimitiveValueWithCalcWithKnownTokenTypeDimension(range, { }, valueRange, parserMode, unitless, CSSValuePool::singleton());
    
    case NumberToken:
        return consumeLengthCSSPrimitiveValueWithCalcWithKnownTokenTypeNumber(range, { }, valueRange, parserMode, unitless, CSSValuePool::singleton());

    case PercentageToken:
        return consumePercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentage(range, { }, valueRange, CSSValuePool::singleton());

    default:
        break;
    }

    return nullptr;
}

// MARK: - Combination consumers that allow lookup in the symbol table for IdentTokens (by default, other consumers only lookup in the symbol table for calc()).

static std::optional<double> consumeNumberAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange = ValueRange::All)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        return consumeNumberRawWithKnownTokenTypeFunction(range, symbolTable, valueRange);

    case NumberToken:
        return consumeNumberRawWithKnownTokenTypeNumber(range, symbolTable, valueRange);

    case IdentToken:
        return consumeNumberRawWithKnownTokenTypeIdent(range, symbolTable, valueRange);

    default:
        break;
    }

    return std::nullopt;
}

static std::optional<double> consumePercentAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange = ValueRange::All)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        return consumePercentRawWithKnownTokenTypeFunction(range, symbolTable, valueRange);

    case PercentageToken:
        return consumePercentRawWithKnownTokenTypePercentage(range, symbolTable, valueRange);

    case IdentToken:
        return consumePercentRawWithKnownTokenTypeIdent(range, symbolTable, valueRange);

    default:
        break;
    }

    return std::nullopt;
}

static std::optional<double> consumeAngleRawOrNumberRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    auto computeDegrees = [](auto angleRaw) -> std::optional<double> {
        if (angleRaw)
            return CSSPrimitiveValue::computeDegrees(angleRaw->type, angleRaw->value);
        return std::nullopt;
    };

    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        if (auto angleRaw = consumeAngleRawWithKnownTokenTypeFunction(range, { }, ValueRange::All, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid))
            return computeDegrees(angleRaw);
        return consumeNumberRawWithKnownTokenTypeFunction(range, { }, ValueRange::All);
    
    case DimensionToken:
        return computeDegrees(consumeAngleRawWithKnownTokenTypeDimension(range, { }, ValueRange::All, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid));
    
    case NumberToken:
        return consumeNumberRawWithKnownTokenTypeNumber(range, { }, ValueRange::All);

    default:
        break;
    }

    return std::nullopt;
}

static std::optional<double> consumeAngleRawOrNumberRawAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, CSSParserMode parserMode)
{
    auto computeDegrees = [](auto angleRaw) -> std::optional<double> {
        if (angleRaw)
            return CSSPrimitiveValue::computeDegrees(angleRaw->type, angleRaw->value);
        return std::nullopt;
    };

    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        if (auto angleRaw = consumeAngleRawWithKnownTokenTypeFunction(range, symbolTable, ValueRange::All, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid))
            return computeDegrees(angleRaw);
        return consumeNumberRawWithKnownTokenTypeFunction(range, symbolTable, ValueRange::All);
    
    case DimensionToken:
        return computeDegrees(consumeAngleRawWithKnownTokenTypeDimension(range, { }, ValueRange::All, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid));
    
    case NumberToken:
        return consumeNumberRawWithKnownTokenTypeNumber(range, { }, ValueRange::All);

    case IdentToken:
        if (auto variable = symbolTable.get(token.id())) {
            switch (variable->type) {
            case CSSUnitType::CSS_DEG:
            case CSSUnitType::CSS_RAD:
            case CSSUnitType::CSS_GRAD:
            case CSSUnitType::CSS_TURN:
                range.consumeIncludingWhitespace();
                return CSSPrimitiveValue::computeDegrees(variable->type, variable->value);

            case CSSUnitType::CSS_NUMBER:
                range.consumeIncludingWhitespace();
                return variable->value;

            default:
                break;
            }
        }
        break;

    default:
        break;
    }

    return std::nullopt;
}

// MARK: - Combination consumers with transformations based on result type.

template<typename NumberTransformer, typename PercentTransformer>
static auto consumeNumberRawOrPercentRaw(CSSParserTokenRange& range, ValueRange valueRange, NumberTransformer&& numberTranformer, PercentTransformer&& percentTranformer) -> std::optional<decltype(numberTranformer(std::declval<double>()))>
{
    const auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken: {
        if (auto number = consumeNumberRawWithKnownTokenTypeFunction(range, { }, valueRange))
            return numberTranformer(*number);
        if (auto percent = consumePercentRawWithKnownTokenTypeFunction(range, { }, valueRange))
            return percentTranformer(*percent);
        break;
    }

    case PercentageToken:
        if (auto percent = consumePercentRawWithKnownTokenTypePercentage(range, { }, valueRange))
            return percentTranformer(*percent);
        break;

    case NumberToken:
        if (auto number = consumeNumberRawWithKnownTokenTypeNumber(range, { }, valueRange))
            return numberTranformer(*number);
        break;

    default:
        break;
    }

    return std::nullopt;
}

static std::optional<double> consumeNumberRawOrPercentDividedBy100Raw(CSSParserTokenRange& range, ValueRange valueRange = ValueRange::All)
{
    return consumeNumberRawOrPercentRaw(range, valueRange, [](double number) { return number; }, [](double percent) { return percent / 100.0; });
}

// MARK: - Combination consumers that both allow lookup in the symbol table for IdentTokens and transformations based on result type.

template<typename NumberTransformer, typename PercentTransformer>
static auto consumeNumberRawOrPercentRawAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, NumberTransformer&& numberTranformer, PercentTransformer&& percentTranformer) -> std::optional<decltype(numberTranformer(std::declval<double>()))>
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken: {
        if (auto number = consumeNumberRawWithKnownTokenTypeFunction(range, symbolTable, valueRange))
            return numberTranformer(*number);
        if (auto percent = consumePercentRawWithKnownTokenTypeFunction(range, symbolTable, valueRange))
            return percentTranformer(*percent);
        break;
    }

    case PercentageToken:
        if (auto percent = consumePercentRawWithKnownTokenTypePercentage(range, symbolTable, valueRange))
            return percentTranformer(*percent);
        break;

    case NumberToken:
        if (auto number = consumeNumberRawWithKnownTokenTypeNumber(range, symbolTable, valueRange))
            return numberTranformer(*number);
        break;

    case IdentToken:
        if (auto variable = symbolTable.get(range.peek().id())) {
            switch (variable->type) {
            case CSSUnitType::CSS_PERCENTAGE:
                if (auto validatedValue = validatedPercentRaw(variable->value, valueRange)) {
                    range.consumeIncludingWhitespace();
                    return percentTranformer(*validatedValue);
                }
                break;

            case CSSUnitType::CSS_NUMBER:
                if (auto validatedValue = validatedNumberRaw(variable->value, valueRange)) {
                    range.consumeIncludingWhitespace();
                    return numberTranformer(*validatedValue);
                }
                break;

            default:
                break;
            }
        }
        break;

    default:
        break;
    }

    return std::nullopt;
}

static std::optional<double> consumeNumberRawOrPercentDividedBy100RawAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange)
{
    return consumeNumberRawOrPercentRawAllowingSymbolTableIdent(range, symbolTable, valueRange, [](double number) { return number; }, [](double percent) { return percent / 100.0; });
}

std::optional<double> consumeFontWeightNumberRaw(CSSParserTokenRange& range)
{
    // Values less than or equal to 0 or greater than or equal to 1000 are parse errors.

#if !ENABLE(VARIATION_FONTS)
    auto isIntegerAndDivisibleBy100 = [](double value) {
        ASSERT(value > 0 && value <= 1000);
        return static_cast<int>(value / 100) * 100 == value;
    };
#endif

    auto& token = range.peek();
    switch (token.type()) {
    case FunctionToken: {
        // "[For calc()], the used value resulting from an expression must be clamped to the range allowed in the target context."
        auto result = consumeNumberRawWithKnownTokenTypeFunction(range, { }, ValueRange::All);
        if (!result)
            break;
#if !ENABLE(VARIATION_FONTS)
        if (!(*result > 0 && *result < 1000) || !isIntegerAndDivisibleBy100(*result))
            break;
#endif
        return std::clamp(*result, std::nextafter(0.0, 1.0), std::nextafter(1000.0, 0.0));
    }

    case NumberToken: {
        auto result = token.numericValue();
        
        // FIXME: This allows value of 1000, unlike the comment above and the behavior of the FunctionToken parsing path.
        if (!(result >= 1 && result <= 1000))
            break;
#if !ENABLE(VARIATION_FONTS)
        if (token.numericValueType() != IntegerValueType || !isIntegerAndDivisibleBy100(result))
            break;
#endif
        range.consumeIncludingWhitespace();
        return result;
    }
    
    default:
        break;
    }

    return std::nullopt;
}

RefPtr<CSSPrimitiveValue> consumeFontWeightNumber(CSSParserTokenRange& range)
{
    return consumeFontWeightNumberWorkerSafe(range, CSSValuePool::singleton());
}

RefPtr<CSSPrimitiveValue> consumeFontWeightNumberWorkerSafe(CSSParserTokenRange& range, CSSValuePool& pool)
{
    if (auto result = consumeFontWeightNumberRaw(range))
        return pool.createValue(*result, CSSUnitType::CSS_NUMBER);
    return nullptr;
}

std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return std::nullopt;
    return range.consumeIncludingWhitespace().id();
}

RefPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange& range)
{
    return consumeIdentWorkerSafe(range, CSSValuePool::singleton());
}

RefPtr<CSSPrimitiveValue> consumeIdentWorkerSafe(CSSParserTokenRange& range, CSSValuePool& pool)
{
    if (auto result = consumeIdentRaw(range))
        return pool.createIdentifierValue(*result);
    return nullptr;
}

std::optional<CSSValueID> consumeIdentRangeRaw(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
{
    if (range.peek().id() < lower || range.peek().id() > upper)
        return std::nullopt;
    return consumeIdentRaw(range);
}

RefPtr<CSSPrimitiveValue> consumeIdentRange(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
{
    if (range.peek().id() < lower || range.peek().id() > upper)
        return nullptr;
    return consumeIdent(range);
}

RefPtr<CSSPrimitiveValue> consumeCustomIdent(CSSParserTokenRange& range, bool shouldLowercase)
{
    if (range.peek().type() != IdentToken || isCSSWideKeyword(range.peek().id()))
        return nullptr;
    auto identifier = range.consumeIncludingWhitespace().value();
    return CSSValuePool::singleton().createCustomIdent(shouldLowercase ? identifier.convertToASCIILowercase() : identifier.toString());
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

    return { };
}

RefPtr<CSSPrimitiveValue> consumeUrl(CSSParserTokenRange& range)
{
    StringView url = consumeUrlAsStringView(range);
    if (url.isNull())
        return nullptr;
    return CSSValuePool::singleton().createValue(url.toString(), CSSUnitType::CSS_URI);
}

static Color consumeOriginColor(CSSParserTokenRange& args, const CSSParserContext& context)
{
    auto value = consumeColor(args, context);
    if (!value)
        return { };

    if (value->isRGBColor())
        return value->color();

    ASSERT(value->isValueID());
    auto keyword = value->valueID();

    // FIXME: We don't have enough context in the parser to resolving a system keyword
    // correctly. We should package up the relative color parameters and resolve the
    // whole thing at the appropriate time when the origin color is a system keyword.
    if (StyleColor::isSystemColor(keyword))
        return { };

    return StyleColor::colorFromKeyword(keyword, { });
}

static std::optional<double> consumeOptionalAlpha(CSSParserTokenRange& range)
{
    if (!consumeSlashIncludingWhitespace(range))
        return 1.0;

    if (auto alphaParameter = consumeNumberRawOrPercentDividedBy100Raw(range))
        return std::clamp(*alphaParameter, 0.0, 1.0);

    return std::nullopt;
}

static std::optional<double> consumeOptionalAlphaAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable)
{
    if (!consumeSlashIncludingWhitespace(range))
        return 1.0;

    if (auto alphaParameter = consumeNumberRawOrPercentDividedBy100RawAllowingSymbolTableIdent(range, symbolTable, ValueRange::All))
        return std::clamp(*alphaParameter, 0.0, 1.0);

    return std::nullopt;
}

static double normalizeHue(double hue)
{
    return std::fmod(std::fmod(hue, 360.0) + 360.0, 360.0);
}

static uint8_t normalizeRGBComponentNumber(double value)
{
    return convertPrescaledSRGBAFloatToSRGBAByte(value);
}

static uint8_t normalizeRGBComponentPercentage(double value)
{
    return convertPrescaledSRGBAFloatToSRGBAByte(value / 100.0 * 255.0);
}

static std::optional<uint8_t> consumeRelativeRGBComponent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable)
{
    return consumeNumberRawOrPercentRawAllowingSymbolTableIdent(range, symbolTable, ValueRange::All, normalizeRGBComponentNumber, normalizeRGBComponentPercentage);
}

enum class RGBComponentType { Number, Percentage };
static uint8_t clampRGBComponent(double value, RGBComponentType componentType)
{
    switch (componentType) {
    case RGBComponentType::Number:
        return normalizeRGBComponentNumber(value);
    case RGBComponentType::Percentage:
        return normalizeRGBComponentPercentage(value);
    }
    
    ASSERT_NOT_REACHED();
    return 0;
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

static std::optional<double> consumeRGBOrHSLOptionalAlpha(CSSParserTokenRange& args, RGBOrHSLSeparatorSyntax syntax)
{
    if (!consumeRGBOrHSLAlphaSeparator(args, syntax))
        return 1.0;

    return consumeNumberRawOrPercentDividedBy100Raw(args);
}

static Color parseRelativeRGBParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColor(args, context);
    if (!originColor.isValid())
        return { };

    auto originColorAsSRGB = originColor.toColorTypeLossy<SRGBA<float>>();

    CSSCalcSymbolTable symbolTable {
        { CSSValueR, CSSUnitType::CSS_PERCENTAGE, originColorAsSRGB.red * 100.0 },
        { CSSValueG, CSSUnitType::CSS_PERCENTAGE, originColorAsSRGB.green * 100.0 },
        { CSSValueB, CSSUnitType::CSS_PERCENTAGE, originColorAsSRGB.blue * 100.0 },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsSRGB.alpha * 100.0 }
    };

    auto red = consumeRelativeRGBComponent(args, symbolTable);
    if (!red)
        return { };

    auto green = consumeRelativeRGBComponent(args, symbolTable);
    if (!green)
        return { };

    auto blue = consumeRelativeRGBComponent(args, symbolTable);
    if (!blue)
        return { };

    auto alpha = consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto normalizedAlpha = convertFloatAlphaTo<uint8_t>(*alpha);

    return SRGBA<uint8_t> { *red, *green, *blue, normalizedAlpha };
}

static Color parseNonRelativeRGBParameters(CSSParserTokenRange& args)
{
    struct InitialComponent {
        double value;
        RGBComponentType type;
    };

    auto consumeInitialComponent = [](auto& args) -> std::optional<InitialComponent> {
        if (auto number = consumeNumberRaw(args))
            return { { *number, RGBComponentType::Number } };
        if (auto percent = consumePercentRaw(args))
            return { { *percent, RGBComponentType::Percentage } };
        return std::nullopt;
    };

    auto consumeComponent = [](auto& args, auto componentType) {
        switch (componentType) {
        case RGBComponentType::Percentage:
            return consumePercentRaw(args);
        case RGBComponentType::Number:
            return consumeNumberRaw(args);
        }
        RELEASE_ASSERT_NOT_REACHED();
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

enum class RGBFunctionMode { RGB, RGBA };

template<RGBFunctionMode Mode> static Color parseRGBParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == (Mode == RGBFunctionMode::RGB ? CSSValueRgb : CSSValueRgba));
    auto args = consumeFunction(range);

    if constexpr (Mode == RGBFunctionMode::RGB) {
        if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
            return parseRelativeRGBParameters(args, context);
    }
    return parseNonRelativeRGBParameters(args);
}

static Color colorByNormalizingHSLComponents(double hue, double saturation, double lightness, double alpha)
{
    auto normalizedHue = normalizeHue(hue);
    auto normalizedSaturation = std::clamp(saturation, 0.0, 100.0);
    auto normalizedLightness = std::clamp(lightness, 0.0, 100.0);
    auto normalizedAlpha = std::clamp(alpha, 0.0, 1.0);

    // NOTE: The explicit conversion to SRGBA<uint8_t> is intentional for performance (no extra allocation for
    // the extended color) and compatability, forcing serialiazation to use the rgb()/rgba() form.
    return convertColor<SRGBA<uint8_t>>(HSLA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedSaturation), static_cast<float>(normalizedLightness), static_cast<float>(normalizedAlpha) });
}

static Color parseRelativeHSLParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColor(args, context);
    if (!originColor.isValid())
        return { };

    auto originColorAsHSL = originColor.toColorTypeLossy<HSLA<float>>();

    CSSCalcSymbolTable symbolTable {
        { CSSValueH, CSSUnitType::CSS_DEG, originColorAsHSL.hue },
        { CSSValueS, CSSUnitType::CSS_PERCENTAGE, originColorAsHSL.saturation },
        { CSSValueL, CSSUnitType::CSS_PERCENTAGE, originColorAsHSL.lightness },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsHSL.alpha * 100.0 }
    };

    auto hue = consumeAngleRawOrNumberRawAllowingSymbolTableIdent(args, symbolTable, context.mode);
    if (!hue)
        return { };

    auto saturation = consumePercentAllowingSymbolTableIdent(args, symbolTable);
    if (!saturation)
        return { };

    auto lightness = consumePercentAllowingSymbolTableIdent(args, symbolTable);
    if (!lightness)
        return { };

    auto alpha = consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    return colorByNormalizingHSLComponents(*hue, *saturation, *lightness, *alpha);
}

static Color parseNonRelativeHSLParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    auto hue = consumeAngleRawOrNumberRaw(args, context.mode);
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

    return colorByNormalizingHSLComponents(*hue, *saturation, *lightness, *alpha);
}

enum class HSLFunctionMode { HSL, HSLA };

template<HSLFunctionMode Mode> static Color parseHSLParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == (Mode == HSLFunctionMode::HSL ? CSSValueHsl : CSSValueHsla));
    auto args = consumeFunction(range);

    if constexpr (Mode == HSLFunctionMode::HSL) {
        if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
            return parseRelativeHSLParameters(args, context);
    }

    return parseNonRelativeHSLParameters(args, context);
}

template<typename ComponentType> struct WhitenessBlackness {
    ComponentType whiteness;
    ComponentType blackness;
};

template<typename ComponentType> static auto normalizeWhitenessBlackness(ComponentType whiteness, ComponentType blackness) -> WhitenessBlackness<ComponentType>
{
    //   Values outside of these ranges are not invalid, but are clamped to the
    //   ranges defined here at computed-value time.
    WhitenessBlackness<ComponentType> result {
        clampTo<ComponentType>(whiteness, 0.0, 100.0),
        clampTo<ComponentType>(blackness, 0.0, 100.0)
    };

    //   If the sum of these two arguments is greater than 100%, then at
    //   computed-value time they are further normalized to add up to 100%, with
    //   the same relative ratio.
    if (auto sum = result.whiteness + result.blackness; sum >= 100) {
        result.whiteness *= 100.0 / sum;
        result.blackness *= 100.0 / sum;
    }

    return result;
}

template<typename ConsumerForHue, typename ConsumerForWhitenessAndBlackness, typename ConsumerForAlpha>
static Color parseHWBParameters(CSSParserTokenRange& args, ConsumerForHue&& hueConsumer, ConsumerForWhitenessAndBlackness&& whitenessAndBlacknessConsumer, ConsumerForAlpha&& alphaConsumer)
{
    auto hue = hueConsumer(args);
    if (!hue)
        return { };

    auto whiteness = whitenessAndBlacknessConsumer(args);
    if (!whiteness)
        return { };

    auto blackness = whitenessAndBlacknessConsumer(args);
    if (!blackness)
        return { };

    auto alpha = alphaConsumer(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto normalizedHue = normalizeHue(*hue);
    auto [normalizedWhitness, normalizedBlackness] = normalizeWhitenessBlackness(*whiteness, *blackness);

    // NOTE: The explicit conversion to SRGBA<uint8_t> is intentional for performance (no extra allocation for
    // the extended color) and compatability, forcing serialiazation to use the rgb()/rgba() form.
    return convertColor<SRGBA<uint8_t>>(HWBA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedWhitness), static_cast<float>(normalizedBlackness), static_cast<float>(*alpha) });
}

static Color parseRelativeHWBParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColor(args, context);
    if (!originColor.isValid())
        return { };

    auto originColorAsHWB = originColor.toColorTypeLossy<HWBA<float>>();

    CSSCalcSymbolTable symbolTable {
        { CSSValueH, CSSUnitType::CSS_DEG, originColorAsHWB.hue },
        { CSSValueW, CSSUnitType::CSS_PERCENTAGE, originColorAsHWB.whiteness },
        { CSSValueB, CSSUnitType::CSS_PERCENTAGE, originColorAsHWB.blackness },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsHWB.alpha * 100.0 }
    };

    auto hueConsumer = [&symbolTable, &context](auto& args) { return consumeAngleRawOrNumberRawAllowingSymbolTableIdent(args, symbolTable, context.mode); };
    auto whitenessAndBlacknessConsumer = [&symbolTable](auto& args) { return consumePercentAllowingSymbolTableIdent(args, symbolTable); };
    auto alphaConsumer = [&symbolTable](auto& args) { return consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable); };

    return parseHWBParameters(args, WTFMove(hueConsumer), WTFMove(whitenessAndBlacknessConsumer), WTFMove(alphaConsumer));
}

static Color parseNonRelativeHWBParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    auto hueConsumer = [&context](auto& args) { return consumeAngleRawOrNumberRaw(args, context.mode); };
    auto whitenessAndBlacknessConsumer = [](auto& args) { return consumePercentRaw(args); };
    auto alphaConsumer = [](auto& args) { return consumeOptionalAlpha(args); };

    return parseHWBParameters(args, WTFMove(hueConsumer), WTFMove(whitenessAndBlacknessConsumer), WTFMove(alphaConsumer));
}

static Color parseHWBParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueHwb);

    if (!context.cssColor4)
        return { };

    auto args = consumeFunction(range);

    if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
        return parseRelativeHWBParameters(args, context);
    return parseNonRelativeHWBParameters(args, context);
}

template<typename ConsumerForLightness, typename ConsumerForAB, typename ConsumerForAlpha>
static Color parseLabParameters(CSSParserTokenRange& args, ConsumerForLightness&& lightnessConsumer, ConsumerForAB&& abConsumer, ConsumerForAlpha&& alphaConsumer)
{
    auto lightness = lightnessConsumer(args);
    if (!lightness)
        return { };

    auto aValue = abConsumer(args);
    if (!aValue)
        return { };

    auto bValue = abConsumer(args);
    if (!bValue)
        return { };

    auto alpha = alphaConsumer(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto normalizedLightness = std::max(0.0, *lightness);

    return Lab<float> { static_cast<float>(normalizedLightness), static_cast<float>(*aValue), static_cast<float>(*bValue), static_cast<float>(*alpha) };
}

static Color parseRelativeLabParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColor(args, context);
    if (!originColor.isValid())
        return { };

    auto originColorAsLab = originColor.toColorTypeLossy<Lab<float>>();

    CSSCalcSymbolTable symbolTable {
        { CSSValueL, CSSUnitType::CSS_PERCENTAGE, originColorAsLab.lightness },
        { CSSValueA, CSSUnitType::CSS_NUMBER, originColorAsLab.a },
        { CSSValueB, CSSUnitType::CSS_NUMBER, originColorAsLab.b },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsLab.alpha * 100.0 }
    };

    auto lightnessConsumer = [&symbolTable](auto& args) { return consumePercentAllowingSymbolTableIdent(args, symbolTable); };
    auto abConsumer = [&symbolTable](auto& args) { return consumeNumberAllowingSymbolTableIdent(args, symbolTable); };
    auto alphaConsumer = [&symbolTable](auto& args) { return consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable); };

    return parseLabParameters(args, WTFMove(lightnessConsumer), WTFMove(abConsumer), WTFMove(alphaConsumer));
}

static Color parseNonRelativeLabParameters(CSSParserTokenRange& args)
{
    auto lightnessConsumer = [](auto& args) { return consumePercentRaw(args); };
    auto abConsumer = [](auto& args) { return consumeNumberRaw(args); };
    auto alphaConsumer = [](auto& args) { return consumeOptionalAlpha(args); };

    return parseLabParameters(args, WTFMove(lightnessConsumer), WTFMove(abConsumer), WTFMove(alphaConsumer));
}

static Color parseLabParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueLab);

    if (!context.cssColor4)
        return { };

    auto args = consumeFunction(range);

    if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
        return parseRelativeLabParameters(args, context);
    return parseNonRelativeLabParameters(args);
}

template<typename ConsumerForLightness, typename ConsumerForChroma, typename ConsumerForHue, typename ConsumerForAlpha>
static Color parseLCHParameters(CSSParserTokenRange& args, ConsumerForLightness&& lightnessConsumer, ConsumerForChroma&& chromaConsumer, ConsumerForHue&& hueConsumer, ConsumerForAlpha&& alphaConsumer)
{
    auto lightness = lightnessConsumer(args);
    if (!lightness)
        return { };

    auto chroma = chromaConsumer(args);
    if (!chroma)
        return { };

    auto hue = hueConsumer(args);
    if (!hue)
        return { };

    auto alpha = alphaConsumer(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto normalizedLightness = std::max(0.0, *lightness);
    auto normalizedChroma = std::max(0.0, *chroma);
    auto normalizedHue = normalizeHue(*hue);

    return LCHA<float> { static_cast<float>(normalizedLightness), static_cast<float>(normalizedChroma), static_cast<float>(normalizedHue), static_cast<float>(*alpha) };
}

static Color parseRelativeLCHParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColor(args, context);
    if (!originColor.isValid())
        return { };

    auto originColorAsLCH = originColor.toColorTypeLossy<LCHA<float>>();

    CSSCalcSymbolTable symbolTable {
        { CSSValueL, CSSUnitType::CSS_PERCENTAGE, originColorAsLCH.lightness },
        { CSSValueC, CSSUnitType::CSS_NUMBER, originColorAsLCH.chroma },
        { CSSValueH, CSSUnitType::CSS_DEG, originColorAsLCH.hue },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsLCH.alpha * 100.0 }
    };

    auto lightnessConsumer = [&symbolTable](auto& args) { return consumePercentAllowingSymbolTableIdent(args, symbolTable); };
    auto chromaConsumer = [&symbolTable](auto& args) { return consumeNumberAllowingSymbolTableIdent(args, symbolTable); };
    auto hueConsumer = [&symbolTable, &context](auto& args) { return consumeAngleRawOrNumberRawAllowingSymbolTableIdent(args, symbolTable, context.mode); };
    auto alphaConsumer = [&symbolTable](auto& args) { return consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable); };

    return parseLCHParameters(args, WTFMove(lightnessConsumer), WTFMove(chromaConsumer), WTFMove(hueConsumer), WTFMove(alphaConsumer));
}

static Color parseNonRelativeLCHParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    auto lightnessConsumer = [](auto& args) { return consumePercentRaw(args); };
    auto chromaConsumer = [](auto& args) { return consumeNumberRaw(args); };
    auto hueConsumer = [&context](auto& args) { return consumeAngleRawOrNumberRaw(args, context.mode); };
    auto alphaConsumer = [](auto& args) { return consumeOptionalAlpha(args); };

    return parseLCHParameters(args, WTFMove(lightnessConsumer), WTFMove(chromaConsumer), WTFMove(hueConsumer), WTFMove(alphaConsumer));
}

static Color parseLCHParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueLch);

    if (!context.cssColor4)
        return { };

    auto args = consumeFunction(range);

    if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
        return parseRelativeLCHParameters(args, context);
    return parseNonRelativeLCHParameters(args, context);
}

template<typename ColorType, typename ConsumerForRGB, typename ConsumerForAlpha>
static Color parseColorFunctionForRGBTypes(CSSParserTokenRange& args, ConsumerForRGB&& rgbConsumer, ConsumerForAlpha&& alphaConsumer)
{
    double channels[3] = { 0, 0, 0 };
    for (auto& channel : channels) {
        auto value = rgbConsumer(args);
        if (!value)
            break;
        channel = *value;
    }

    auto alpha = alphaConsumer(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    auto normalizedRed = std::clamp(channels[0], 0.0, 1.0);
    auto normalizedGreen = std::clamp(channels[1], 0.0, 1.0);
    auto normalizedBlue = std::clamp(channels[2], 0.0, 1.0);

    return { ColorType { static_cast<float>(normalizedRed), static_cast<float>(normalizedGreen), static_cast<float>(normalizedBlue), static_cast<float>(*alpha) }, Color::Flags::UseColorFunctionSerialization };
}

template<typename ColorType> static Color parseRelativeColorFunctionForRGBTypes(CSSParserTokenRange& args, Color originColor, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueDisplayP3 || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020 || args.peek().id() == CSSValueSRGB);

    // Support sRGB and Display-P3 regardless of the setting as we have shipped support for them for a while.
    if (!context.cssColor4 && (args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020))
        return { };

    consumeIdentRaw(args);

    auto originColorAsColorType = originColor.toColorTypeLossy<ColorType>();

    CSSCalcSymbolTable symbolTable {
        { CSSValueR, CSSUnitType::CSS_PERCENTAGE, originColorAsColorType.red * 100.0 },
        { CSSValueG, CSSUnitType::CSS_PERCENTAGE, originColorAsColorType.green * 100.0 },
        { CSSValueB, CSSUnitType::CSS_PERCENTAGE, originColorAsColorType.blue * 100.0 },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsColorType.alpha * 100.0 }
    };

    auto consumeRGB = [&symbolTable](auto& args) { return consumeNumberRawOrPercentDividedBy100RawAllowingSymbolTableIdent(args, symbolTable, ValueRange::All); };
    auto consumeAlpha = [&symbolTable](auto& args) { return consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable); };

    return parseColorFunctionForRGBTypes<ColorType>(args, WTFMove(consumeRGB), WTFMove(consumeAlpha));
}

template<typename ColorType> static Color parseColorFunctionForRGBTypes(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueDisplayP3 || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020 || args.peek().id() == CSSValueSRGB);

    // Support sRGB and Display-P3 regardless of the setting as we have shipped support for them for a while.
    if (!context.cssColor4 && (args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020))
        return { };

    consumeIdentRaw(args);

    auto consumeRGB = [](auto& args) { return consumeNumberRawOrPercentDividedBy100Raw(args); };
    auto consumeAlpha = [](auto& args) { return consumeOptionalAlpha(args); };

    return parseColorFunctionForRGBTypes<ColorType>(args, WTFMove(consumeRGB), WTFMove(consumeAlpha));
}

template<typename ConsumerForXYZ, typename ConsumerForAlpha>
static Color parseColorFunctionForXYZParameters(CSSParserTokenRange& args, ConsumerForXYZ&& xyzConsumer, ConsumerForAlpha&& alphaConsumer)
{
    double channels[3] = { 0, 0, 0 };
    for (auto& channel : channels) {
        auto value = xyzConsumer(args);
        if (!value)
            break;
        channel = *value;
    }

    auto alpha = alphaConsumer(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    return { XYZA<float, WhitePoint::D50> { static_cast<float>(channels[0]), static_cast<float>(channels[1]), static_cast<float>(channels[2]), static_cast<float>(*alpha) }, Color::Flags::UseColorFunctionSerialization };
}

static Color parseRelativeColorFunctionForXYZParameters(CSSParserTokenRange& args, Color originColor, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueXyz);

    if (!context.cssColor4)
        return { };

    consumeIdentRaw(args);

    auto originColorAsXYZ = originColor.toColorTypeLossy<XYZA<float, WhitePoint::D50>>();

    CSSCalcSymbolTable symbolTable {
        { CSSValueX, CSSUnitType::CSS_NUMBER, originColorAsXYZ.x },
        { CSSValueY, CSSUnitType::CSS_NUMBER, originColorAsXYZ.y },
        { CSSValueZ, CSSUnitType::CSS_NUMBER, originColorAsXYZ.z },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsXYZ.alpha * 100.0 }
    };

    auto consumeXYZ = [&symbolTable](auto& args) { return consumeNumberAllowingSymbolTableIdent(args, symbolTable); };
    auto consumeAlpha = [&symbolTable](auto& args) { return consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable); };

    return parseColorFunctionForXYZParameters(args, WTFMove(consumeXYZ), WTFMove(consumeAlpha));
}

static Color parseColorFunctionForXYZParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueXyz);

    if (!context.cssColor4)
        return { };

    consumeIdentRaw(args);

    auto consumeXYZ = [](auto& args) { return consumeNumberRaw(args); };
    auto consumeAlpha = [](auto& args) { return consumeOptionalAlpha(args); };

    return parseColorFunctionForXYZParameters(args, WTFMove(consumeXYZ), WTFMove(consumeAlpha));
}

static Color parseRelativeColorFunctionParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColor(args, context);
    if (!originColor.isValid())
        return { };

    switch (args.peek().id()) {
    case CSSValueA98Rgb:
        return parseRelativeColorFunctionForRGBTypes<A98RGB<float>>(args, WTFMove(originColor), context);
    case CSSValueDisplayP3:
        return parseRelativeColorFunctionForRGBTypes<DisplayP3<float>>(args, WTFMove(originColor), context);
    case CSSValueProphotoRgb:
        return parseRelativeColorFunctionForRGBTypes<ProPhotoRGB<float>>(args, WTFMove(originColor), context);
    case CSSValueRec2020:
        return parseRelativeColorFunctionForRGBTypes<Rec2020<float>>(args, WTFMove(originColor), context);
    case CSSValueSRGB:
        return parseRelativeColorFunctionForRGBTypes<SRGBA<float>>(args, WTFMove(originColor), context);
    case CSSValueXyz:
        return parseRelativeColorFunctionForXYZParameters(args, WTFMove(originColor), context);
    default:
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

static Color parseNonRelativeColorFunctionParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    switch (args.peek().id()) {
    case CSSValueA98Rgb:
        return parseColorFunctionForRGBTypes<A98RGB<float>>(args, context);
    case CSSValueDisplayP3:
        return parseColorFunctionForRGBTypes<DisplayP3<float>>(args, context);
    case CSSValueProphotoRgb:
        return parseColorFunctionForRGBTypes<ProPhotoRGB<float>>(args, context);
    case CSSValueRec2020:
        return parseColorFunctionForRGBTypes<Rec2020<float>>(args, context);
    case CSSValueSRGB:
        return parseColorFunctionForRGBTypes<SRGBA<float>>(args, context);
    case CSSValueXyz:
        return parseColorFunctionForXYZParameters(args, context);
    default:
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

static Color parseColorFunctionParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueColor);
    auto args = consumeFunction(range);

    auto color = [&] {
        if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
            return parseRelativeColorFunctionParameters(args, context);
        return parseNonRelativeColorFunctionParameters(args, context);
    }();

    ASSERT(!color.isValid() || color.usesColorFunctionSerialization());
    return color;
}

static Color selectFirstColorThatMeetsOrExceedsTargetContrast(const Color& originBackgroundColor, Vector<Color>&& colorsToCompareAgainst, double targetContrast)
{
    auto originBackgroundColorLuminance = originBackgroundColor.luminance();

    for (auto& color : colorsToCompareAgainst) {
        if (contrastRatio(originBackgroundColorLuminance, color.luminance()) >= targetContrast)
            return WTFMove(color);
    }

    // If there is a target contrast, and the end of the list is reached without meeting that target,
    // either white or black is returned, whichever has the higher contrast.
    auto contrastWithWhite = contrastRatio(originBackgroundColorLuminance, 1.0);
    auto contrastWithBlack = contrastRatio(originBackgroundColorLuminance, 0.0);
    return contrastWithWhite > contrastWithBlack ? Color::white : Color::black;
}

static Color selectFirstColorWithHighestContrast(const Color& originBackgroundColor, Vector<Color>&& colorsToCompareAgainst)
{
    auto originBackgroundColorLuminance = originBackgroundColor.luminance();

    auto* colorWithGreatestContrast = &colorsToCompareAgainst[0];
    double greatestContrastSoFar = 0;
    for (auto& color : colorsToCompareAgainst) {
        auto contrast = contrastRatio(originBackgroundColorLuminance, color.luminance());
        if (contrast > greatestContrastSoFar) {
            greatestContrastSoFar = contrast;
            colorWithGreatestContrast = &color;
        }
    }

    return WTFMove(*colorWithGreatestContrast);
}

static Color parseColorContrastFunctionParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueColorContrast);

    if (!context.colorContrastEnabled)
        return { };

    auto args = consumeFunction(range);

    auto originBackgroundColor = consumeOriginColor(args, context);
    if (!originBackgroundColor.isValid())
        return { };

    if (!consumeIdentRaw<CSSValueVs>(args))
        return { };

    Vector<Color> colorsToCompareAgainst;
    bool consumedTo = false;
    do {
        auto colorToCompareAgainst = consumeOriginColor(args, context);
        if (!colorToCompareAgainst.isValid())
            return { };

        colorsToCompareAgainst.append(WTFMove(colorToCompareAgainst));

        if (consumeIdentRaw<CSSValueTo>(args)) {
            consumedTo = true;
            break;
        }
    } while (consumeCommaIncludingWhitespace(args));

    if (colorsToCompareAgainst.size() == 1)
        return { };

    if (consumedTo) {
        auto targetContrast = [&] () -> std::optional<double> {
            if (args.peek().type() == IdentToken) {
                static constexpr std::pair<CSSValueID, double> targetContrastMappings[] {
                    { CSSValueAA, 4.5 },
                    { CSSValueAALarge, 3.0 },
                    { CSSValueAAA, 7.0 },
                    { CSSValueAAALarge, 4.5 },
                };
                static constexpr SortedArrayMap targetContrastMap { targetContrastMappings };
                auto value = targetContrastMap.tryGet(args.consumeIncludingWhitespace().id());
                return value ? std::make_optional(*value) : std::nullopt;
            }
            return consumeNumberRaw(args);
        }();

        if (!targetContrast)
            return { };
        
        // When a target constast is specified, we select "the first color color to meet or exceed the target contrast."
        return selectFirstColorThatMeetsOrExceedsTargetContrast(originBackgroundColor, WTFMove(colorsToCompareAgainst), *targetContrast);
    }

    // When a target constast is NOT specified, we select "the first color with the highest contrast to the single color."
    return selectFirstColorWithHighestContrast(originBackgroundColor, WTFMove(colorsToCompareAgainst));
}

enum class ColorMixColorSpace {
    Srgb,
    Hsl,
    Hwb,
    Xyz,
    Lab,
    Lch
};

static std::optional<ColorMixColorSpace> consumeColorMixColorSpaceAndComma(CSSParserTokenRange& args)
{
    auto consumeIdentAndComma = [](CSSParserTokenRange& args, ColorMixColorSpace colorSpace) -> std::optional<ColorMixColorSpace> {
        consumeIdentRaw(args);
        if (!consumeCommaIncludingWhitespace(args))
            return std::nullopt;
        return colorSpace;
    };

    switch (args.peek().id()) {
    case CSSValueHsl:
        return consumeIdentAndComma(args, ColorMixColorSpace::Hsl);
    case CSSValueHwb:
        return consumeIdentAndComma(args, ColorMixColorSpace::Hwb);
    case CSSValueLch:
        return consumeIdentAndComma(args, ColorMixColorSpace::Lch);
    case CSSValueLab:
        return consumeIdentAndComma(args, ColorMixColorSpace::Lab);
    case CSSValueXyz:
        return consumeIdentAndComma(args, ColorMixColorSpace::Xyz);
    case CSSValueSRGB:
        return consumeIdentAndComma(args, ColorMixColorSpace::Srgb);
    default:
        return std::nullopt;
    }
}

struct ColorMixComponent {
    Color color;
    std::optional<double> percentage;
};

static std::optional<ColorMixComponent> consumeColorMixComponent(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ColorMixComponent result;

    if (auto percentage = consumePercentRaw(args))
        result.percentage = percentage;

    result.color = consumeOriginColor(args, context);
    if (!result.color.isValid())
        return std::nullopt;

    if (!result.percentage) {
        if (auto percentage = consumePercentRaw(args))
            result.percentage = percentage;
    }

    return result;
}

struct ColorMixPercentages {
    double p1;
    double p2;
};

static ColorMixPercentages normalizedMixPercentages(const ColorMixComponent& mixComponents1, const ColorMixComponent& mixComponents2)
{
    // The percentages are normalized as follows:

    // 1. Let p1 be the first percentage and p2 the second one.

    // 2. If both percentages are omitted, they each default to 50% (an equal mix of the two colors).
    if (!mixComponents1.percentage && !mixComponents2.percentage)
        return { 50.0, 50.0 };
    
    ColorMixPercentages result;

    if (!mixComponents2.percentage) {
        // 3. Otherwise, if p2 is omitted, it becomes 100% - p1
        result.p1 = *mixComponents1.percentage;
        result.p2 = 100.0 - result.p1;
    } else if (!mixComponents1.percentage) {
        // 4. Otherwise, if p1 is omitted, it becomes 100% - p2
        result.p2 = *mixComponents2.percentage;
        result.p1 = 100.0 - result.p2;
    } else {
        result.p1 = *mixComponents1.percentage;
        result.p2 = *mixComponents2.percentage;
    }

    auto sum = result.p1 + result.p2;

    // 5. If the percentages sum to zero do something, tbd. (FIXME: We just use 50 / 50 for this case for now).
    if (sum == 0)
        return { 50.0, 50.0 };

    if (sum != 100.0) {
        // 6. Otherwise, if both are provided but do not add up to 100%, they are scaled accordingly so that they
        //    add up to 100%. This means that p1 becomes p1 / (p1 + p2) and p2 becomes p2 / (p1 + p2).
        result.p1 *= 100.0 / sum;
        result.p2 *= 100.0 / sum;
    }

    return result;
}

// Normalization is special cased for HWBA, which needs to normalize the whiteness and blackness components and convert to sRGB
// and HSLA, which just needs to be converted to sRGB. All other color types can go through this non-specialized case.

template<typename ColorType> inline Color makeColorTypeByNormalizingComponentsAfterMix(const ColorComponents<float, 4>& colorComponents)
{
    return makeFromComponents<ColorType>(colorComponents);
}

template<> inline Color makeColorTypeByNormalizingComponentsAfterMix<HWBA<float>>(const ColorComponents<float, 4>& colorComponents)
{
    auto [hue, whiteness, blackness, alpha] = colorComponents;
    auto [normalizedWhitness, normalizedBlackness] = normalizeWhitenessBlackness(whiteness, blackness);

    return convertColor<SRGBA<uint8_t>>(HWBA<float> { hue, normalizedWhitness, normalizedBlackness, alpha });
}

template<> inline Color makeColorTypeByNormalizingComponentsAfterMix<HSLA<float>>(const ColorComponents<float, 4>& colorComponents)
{
    return convertColor<SRGBA<uint8_t>>(makeFromComponents<HSLA<float>>(colorComponents));
}

template<size_t I, typename ComponentType> static void fixupHueComponentsPriorToMix(ColorComponents<ComponentType, 4>& colorComponents1, ColorComponents<ComponentType, 4>& colorComponents2)
{
    auto normalizeAnglesUsingShorterAlgorithm = [] (auto theta1, auto theta2) -> std::pair<ComponentType, ComponentType> {
        // https://drafts.csswg.org/css-color-4/#hue-shorter
        auto difference = theta2 - theta1;
        if (difference > 180.0)
            return { theta1 + 360.0, theta2 };
        if (difference < -180.0)
            return { theta1, theta2 + 360.0 };
        return { theta1, theta2 };
    };

    // As no other interpolation type was specified, all angles should be normalized to use the "shorter" algorithm.
    auto [theta1, theta2] = normalizeAnglesUsingShorterAlgorithm(colorComponents1[I], colorComponents2[I]);
    colorComponents1[I] = theta1;
    colorComponents2[I] = theta2;
}

template<typename ColorType> static Color mixColorComponentsInColorSpace(ColorMixPercentages mixPercentages, const Color& color1, const Color& color2)
{
    auto colorComponents1 = asColorComponents(color1.template toColorTypeLossy<ColorType>());
    auto colorComponents2 = asColorComponents(color2.template toColorTypeLossy<ColorType>());

    // Perform fixups on any hue/angle components.
    constexpr auto componentInfo = ColorType::Model::componentInfo;
    if constexpr (componentInfo[0].type == ColorComponentType::Angle)
        fixupHueComponentsPriorToMix<0>(colorComponents1, colorComponents2);
    if constexpr (componentInfo[1].type == ColorComponentType::Angle)
        fixupHueComponentsPriorToMix<1>(colorComponents1, colorComponents2);
    if constexpr (componentInfo[2].type == ColorComponentType::Angle)
        fixupHueComponentsPriorToMix<2>(colorComponents1, colorComponents2);

    auto colorComponentsMixed = mapColorComponents([&] (auto componentFromColor1, auto componentFromColor2) -> float {
        return (componentFromColor1 * mixPercentages.p1 / 100.0) + (componentFromColor2 * mixPercentages.p2 / 100.0);
    }, colorComponents1, colorComponents2);

    return makeColorTypeByNormalizingComponentsAfterMix<ColorType>(colorComponentsMixed);
}

static Color mixColorComponents(ColorMixColorSpace colorSpace, const ColorMixComponent& mixComponents1, const ColorMixComponent& mixComponents2)
{
    auto mixPercentages = normalizedMixPercentages(mixComponents1, mixComponents2);

    switch (colorSpace) {
    case ColorMixColorSpace::Hsl:
        return mixColorComponentsInColorSpace<HSLA<float>>(mixPercentages, mixComponents1.color, mixComponents2.color);
    case ColorMixColorSpace::Hwb:
        return mixColorComponentsInColorSpace<HWBA<float>>(mixPercentages, mixComponents1.color, mixComponents2.color);
    case ColorMixColorSpace::Lch:
        return mixColorComponentsInColorSpace<LCHA<float>>(mixPercentages, mixComponents1.color, mixComponents2.color);
    case ColorMixColorSpace::Lab:
        return mixColorComponentsInColorSpace<Lab<float>>(mixPercentages, mixComponents1.color, mixComponents2.color);
    case ColorMixColorSpace::Xyz:
        return mixColorComponentsInColorSpace<XYZA<float, WhitePoint::D50>>(mixPercentages, mixComponents1.color, mixComponents2.color);
    case ColorMixColorSpace::Srgb:
        return mixColorComponentsInColorSpace<SRGBA<float>>(mixPercentages, mixComponents1.color, mixComponents2.color);
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static Color parseColorMixFunctionParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueColorMix);

    if (!context.colorMixEnabled)
        return { };

    auto args = consumeFunction(range);

    if (!consumeIdentRaw<CSSValueIn>(args))
        return { };
    
    auto colorSpace = consumeColorMixColorSpaceAndComma(args);
    if (!colorSpace)
        return { };

    auto mixComponent1 = consumeColorMixComponent(args, context);
    if (!mixComponent1)
        return { };

    if (!consumeCommaIncludingWhitespace(args))
        return { };

    auto mixComponent2 = consumeColorMixComponent(args, context);
    if (!mixComponent2)
        return { };

    if (!args.atEnd())
        return { };

    return mixColorComponents(*colorSpace, *mixComponent1, *mixComponent2);
}

static std::optional<SRGBA<uint8_t>> parseHexColor(CSSParserTokenRange& range, bool acceptQuirkyColors)
{
    String string;
    StringView view;
    auto& token = range.peek();
    if (token.type() == HashToken)
        view = token.value();
    else {
        if (!acceptQuirkyColors)
            return std::nullopt;
        if (token.type() == IdentToken) {
            view = token.value(); // e.g. FF0000
            if (view.length() != 3 && view.length() != 6)
                return std::nullopt;
        } else if (token.type() == NumberToken || token.type() == DimensionToken) {
            if (token.numericValueType() != IntegerValueType)
                return std::nullopt;
            auto numericValue = token.numericValue();
            if (!(numericValue >= 0 && numericValue < 1000000))
                return std::nullopt;
            auto integerValue = static_cast<int>(token.numericValue());
            if (token.type() == NumberToken)
                string = String::number(integerValue); // e.g. 112233
            else
                string = makeString(integerValue, token.unitString()); // e.g. 0001FF
            if (string.length() < 6)
                string = makeString(&"000000"[string.length()], string);

            if (string.length() != 3 && string.length() != 6)
                return std::nullopt;
            view = string;
        } else
            return std::nullopt;
    }
    auto result = CSSParser::parseHexColor(view);
    if (!result)
        return std::nullopt;
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
        color = parseRGBParameters<RGBFunctionMode::RGB>(colorRange, context);
        break;
    case CSSValueRgba:
        color = parseRGBParameters<RGBFunctionMode::RGBA>(colorRange, context);
        break;
    case CSSValueHsl:
        color = parseHSLParameters<HSLFunctionMode::HSL>(colorRange, context);
        break;
    case CSSValueHsla:
        color = parseHSLParameters<HSLFunctionMode::HSLA>(colorRange, context);
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
        color = parseColorFunctionParameters(colorRange, context);
        break;
    case CSSValueColorContrast:
        color = parseColorContrastFunctionParameters(colorRange, context);
        break;
    case CSSValueColorMix:
        color = parseColorMixFunctionParameters(colorRange, context);
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

static RefPtr<CSSPrimitiveValue> consumePositionComponent(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless)
{
    if (range.peek().type() == IdentToken)
        return consumeIdent<CSSValueLeft, CSSValueTop, CSSValueBottom, CSSValueRight, CSSValueCenter>(range);
    return consumeLengthOrPercent(range, parserMode, ValueRange::All, unitless);
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

static std::optional<PositionCoordinates> positionFromTwoValues(CSSPrimitiveValue& value1, CSSPrimitiveValue& value2)
{
    bool mustOrderAsXY = isHorizontalPositionKeywordOnly(value1) || isVerticalPositionKeywordOnly(value2) || !value1.isValueID() || !value2.isValueID();
    bool mustOrderAsYX = isVerticalPositionKeywordOnly(value1) || isHorizontalPositionKeywordOnly(value2);
    if (mustOrderAsXY && mustOrderAsYX)
        return std::nullopt;
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
static std::optional<PositionCoordinates> backgroundPositionFromThreeValues(const std::array<CSSPrimitiveValue*, 5>& values)
{
    RefPtr<CSSPrimitiveValue> resultX;
    RefPtr<CSSPrimitiveValue> resultY;

    CSSPrimitiveValue* center = nullptr;
    for (int i = 0; values[i]; i++) {
        CSSPrimitiveValue* currentValue = values[i];
        if (!currentValue->isValueID())
            return std::nullopt;

        CSSValueID id = currentValue->valueID();
        if (id == CSSValueCenter) {
            if (center)
                return std::nullopt;
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
                return std::nullopt;
            resultX = result;
        } else {
            ASSERT(id == CSSValueTop || id == CSSValueBottom);
            if (resultY)
                return std::nullopt;
            resultY = result;
        }
    }

    if (center) {
        ASSERT(resultX || resultY);
        if (resultX && resultY)
            return std::nullopt;
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
static std::optional<PositionCoordinates> positionFromFourValues(const std::array<CSSPrimitiveValue*, 5>& values)
{
    RefPtr<CSSPrimitiveValue> resultX;
    RefPtr<CSSPrimitiveValue> resultY;

    for (int i = 0; values[i]; i++) {
        CSSPrimitiveValue* currentValue = values[i];
        if (!currentValue->isValueID())
            return std::nullopt;

        CSSValueID id = currentValue->valueID();
        if (id == CSSValueCenter)
            return std::nullopt;

        RefPtr<CSSPrimitiveValue> result;
        if (values[i + 1] && !values[i + 1]->isValueID())
            result = CSSPropertyParserHelpersInternal::createPrimitiveValuePair(currentValue, values[++i]);
        else
            result = currentValue;

        if (id == CSSValueLeft || id == CSSValueRight) {
            if (resultX)
                return std::nullopt;
            resultX = result;
        } else {
            ASSERT(id == CSSValueTop || id == CSSValueBottom);
            if (resultY)
                return std::nullopt;
            resultY = result;
        }
    }

    ASSERT(resultX && resultY);
    return PositionCoordinates { resultX.releaseNonNull(), resultY.releaseNonNull() };
}

// FIXME: This may consume from the range upon failure. The background
// shorthand works around it, but we should just fix it here.
std::optional<PositionCoordinates> consumePositionCoordinates(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, PositionSyntax positionSyntax)
{
    auto value1 = consumePositionComponent(range, parserMode, unitless);
    if (!value1)
        return std::nullopt;

    auto value2 = consumePositionComponent(range, parserMode, unitless);
    if (!value2)
        return positionFromOneValue(*value1);

    auto value3 = consumePositionComponent(range, parserMode, unitless);
    if (!value3)
        return positionFromTwoValues(*value1, *value2);

    auto value4 = consumePositionComponent(range, parserMode, unitless);
    
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
        return std::nullopt;
    
    return backgroundPositionFromThreeValues(values);
}

RefPtr<CSSPrimitiveValue> consumePosition(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, PositionSyntax positionSyntax)
{
    if (auto coordinates = consumePositionCoordinates(range, parserMode, unitless, positionSyntax))
        return CSSPropertyParserHelpersInternal::createPrimitiveValuePair(WTFMove(coordinates->x), WTFMove(coordinates->y));
    return nullptr;
}

std::optional<PositionCoordinates> consumeOneOrTwoValuedPositionCoordinates(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless)
{
    auto value1 = consumePositionComponent(range, parserMode, unitless);
    if (!value1)
        return std::nullopt;
    auto value2 = consumePositionComponent(range, parserMode, unitless);
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
    RefPtr<CSSPrimitiveValue> result = consumePercent(args, ValueRange::All);
    if (!result)
        result = consumeNumber(args, ValueRange::All);
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
        auto value = consumeNumberRawOrPercentDividedBy100Raw(args);
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
        auto radius = consumeNumber(args, ValueRange::NonNegative);
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
        auto radius = consumeNumber(args, ValueRange::NonNegative);
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
            ? consumeAngleOrPercent(range, context.mode, ValueRange::All, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow)
            : consumeLengthOrPercent(range, context.mode, ValueRange::All);
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
        auto horizontalSize = consumeLengthOrPercent(args, context.mode, ValueRange::NonNegative);
        RefPtr<CSSPrimitiveValue> verticalSize;
        if (horizontalSize) {
            verticalSize = consumeLengthOrPercent(args, context.mode, ValueRange::NonNegative);
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
            auto center = consumeLengthOrPercent(args, context.mode, ValueRange::NonNegative);
            if (!center)
                break;
            if (horizontalSize)
                return nullptr;
            horizontalSize = center;
            center = consumeLengthOrPercent(args, context.mode, ValueRange::NonNegative);
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

    auto percentage = consumeNumberRawOrPercentDividedBy100Raw(args);
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
            parsedValue = consumeLength(args, HTMLStandardMode, ValueRange::NonNegative);
        else {
            parsedValue = consumePercent(args, ValueRange::NonNegative);
            if (!parsedValue)
                parsedValue = consumeNumber(args, ValueRange::NonNegative);
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
        horizontalOffset = consumeLength(range, context.mode, ValueRange::All);
        if (!horizontalOffset)
            return nullptr;
        verticalOffset = consumeLength(range, context.mode, ValueRange::All);
        if (!verticalOffset)
            return nullptr;

        const CSSParserToken& token = range.peek();
        // The explicit check for calc() is unfortunate. This is ensuring that we only fail parsing if there is a length, but it fails the range check.
        if (token.type() == DimensionToken || token.type() == NumberToken || (token.type() == FunctionToken && CSSCalcValue::isCalcFunction(token.functionId()))) {
            blurRadius = consumeLength(range, context.mode, ValueRange::NonNegative);
            if (!blurRadius)
                return nullptr;
        }

        if (blurRadius && allowSpread)
            spreadDistance = consumeLength(range, context.mode, ValueRange::All);
    }
    
    // In order for this to be a valid <shadow>, at least these lengths must be present.
    if (!horizontalOffset || !verticalOffset)
        return nullptr;
    return CSSShadowValue::create(WTFMove(horizontalOffset), WTFMove(verticalOffset), WTFMove(blurRadius), WTFMove(spreadDistance), WTFMove(style), WTFMove(color));
}

RefPtr<CSSValue> consumeImage(CSSParserTokenRange& range, const CSSParserContext& context, OptionSet<AllowedImageType> allowedImageTypes)
{
    if (range.peek().type() == StringToken && allowedImageTypes.contains(AllowedImageType::RawStringAsURL)) {
        return CSSImageValue::create(context.completeURL(range.consumeIncludingWhitespace().value().toAtomString().string()),
            context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No);
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
        if (auto string = consumeUrlAsStringView(range); !string.isNull()) {
            return CSSImageValue::create(context.completeURL(string.toAtomString().string()),
                context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No);
        }
    }

    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#predefined-counters
bool isPredefinedCounterStyle(CSSValueID valueID)
{
    return valueID >= CSSValueDisc && valueID <= CSSValueEthiopicNumeric;
}

// https://www.w3.org/TR/css-counter-styles-3/#typedef-counter-style-name
RefPtr<CSSPrimitiveValue> consumeCounterStyleName(CSSParserTokenRange& range)
{
    // <counter-style-name> is a <custom-ident> that is not an ASCII case-insensitive match for "none".
    auto valueID = range.peek().id();
    if (valueID == CSSValueNone)
        return nullptr;
    // If the value is an ASCII case-insensitive match for any of the predefined counter styles, lowercase it.
    if (auto name = consumeCustomIdent(range, isPredefinedCounterStyle(valueID)))
        return name;
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#typedef-counter-style-name
AtomString consumeCounterStyleNameInPrelude(CSSParserTokenRange& prelude)
{
    auto nameToken = prelude.consumeIncludingWhitespace();
    if (!prelude.atEnd())
        return AtomString();
    // Ensure this token is a valid <custom-ident>.
    if (nameToken.type() != IdentToken || isCSSWideKeyword(nameToken.id()))
        return AtomString();
    // In the context of the prelude of an @counter-style rule, a <counter-style-name> must not be an ASCII
    // case-insensitive match for "decimal" or "disc". No <counter-style-name>, prelude or not, may be an ASCII
    // case-insensitive match for "none".
    if (identMatches<CSSValueDecimal, CSSValueDisc, CSSValueNone>(nameToken.id()))
        return AtomString();
    auto name = nameToken.value();
    return isPredefinedCounterStyle(nameToken.id()) ? name.convertToASCIILowercaseAtom() : name.toAtomString();
}

std::optional<CSSValueID> consumeFontVariantCSS21Raw(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueNormal, CSSValueSmallCaps>(range);
}

std::optional<CSSValueID> consumeFontWeightKeywordValueRaw(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueNormal, CSSValueBold, CSSValueBolder, CSSValueLighter>(range);
}

std::optional<FontWeightRaw> consumeFontWeightRaw(CSSParserTokenRange& range)
{
    if (auto result = consumeFontWeightKeywordValueRaw(range))
        return { *result };
    if (auto result = consumeFontWeightNumberRaw(range))
        return { *result };
    return std::nullopt;
}

std::optional<CSSValueID> consumeFontStretchKeywordValueRaw(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueUltraCondensed, CSSValueExtraCondensed, CSSValueCondensed, CSSValueSemiCondensed, CSSValueNormal, CSSValueSemiExpanded, CSSValueExpanded, CSSValueExtraExpanded, CSSValueUltraExpanded>(range);
}

std::optional<CSSValueID> consumeFontStyleKeywordValueRaw(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range);
}

std::optional<FontStyleRaw> consumeFontStyleRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    auto result = consumeFontStyleKeywordValueRaw(range);
    if (!result)
        return std::nullopt;

    auto ident = *result;
    if (ident == CSSValueNormal || ident == CSSValueItalic)
        return { { ident, std::nullopt } };
    ASSERT(ident == CSSValueOblique);
#if ENABLE(VARIATION_FONTS)
    if (!range.atEnd()) {
        // FIXME: This angle does specify that unitless 0 is allowed - see https://drafts.csswg.org/css-fonts-4/#valdef-font-style-oblique-angle
        if (auto angle = consumeAngleRaw(range, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow)) {
            if (isFontStyleAngleInRange(CSSPrimitiveValue::computeDegrees(angle->type, angle->value)))
                return { { CSSValueOblique, WTFMove(angle) } };
            return std::nullopt;
        }
    }
#else
    UNUSED_PARAM(parserMode);
#endif
    return { { CSSValueOblique, std::nullopt } };
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

std::optional<CSSValueID> consumeGenericFamilyRaw(CSSParserTokenRange& range)
{
    return consumeIdentRangeRaw(range, CSSValueSerif, CSSValueWebkitBody);
}

std::optional<WTF::Vector<FontFamilyRaw>> consumeFontFamilyRaw(CSSParserTokenRange& range)
{
    WTF::Vector<FontFamilyRaw> list;
    do {
        if (auto ident = consumeGenericFamilyRaw(range))
            list.append({ *ident });
        else {
            auto familyName = consumeFamilyNameRaw(range);
            if (familyName.isNull())
                return std::nullopt;
            list.append({ familyName });
        }
    } while (consumeCommaIncludingWhitespace(range));
    return list;
}

std::optional<FontSizeRaw> consumeFontSizeRaw(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless)
{
    if (range.peek().id() >= CSSValueXxSmall && range.peek().id() <= CSSValueLarger) {
        if (auto ident = consumeIdentRaw(range))
            return { *ident };
        return std::nullopt;
    }

    if (auto result = consumeLengthOrPercentRaw(range, parserMode, ValueRange::NonNegative, unitless))
        return { *result };

    return std::nullopt;
}

std::optional<LineHeightRaw> consumeLineHeightRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    if (range.peek().id() == CSSValueNormal) {
        if (auto ident = consumeIdentRaw(range))
            return { *ident };
        return std::nullopt;
    }

    if (auto number = consumeNumberRaw(range, ValueRange::NonNegative))
        return { *number };

    if (auto lengthOrPercent = consumeLengthOrPercentRaw(range, parserMode, ValueRange::NonNegative))
        return { *lengthOrPercent };

    return std::nullopt;
}

std::optional<FontRaw> consumeFontRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    // Let's check if there is an inherit or initial somewhere in the shorthand.
    CSSParserTokenRange rangeCopy = range;
    while (!rangeCopy.atEnd()) {
        CSSValueID id = rangeCopy.consumeIncludingWhitespace().id();
        if (id == CSSValueInherit || id == CSSValueInitial)
            return std::nullopt;
    }

    FontRaw result;

    while (!range.atEnd()) {
        CSSValueID id = range.peek().id();
        if (!result.style) {
            if ((result.style = consumeFontStyleRaw(range, parserMode)))
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
        return std::nullopt;

    // Now a font size _must_ come.
    if (auto size = consumeFontSizeRaw(range, parserMode))
        result.size = *size;
    else
        return std::nullopt;

    if (range.atEnd())
        return std::nullopt;

    if (consumeSlashIncludingWhitespace(range)) {
        if (!(result.lineHeight = consumeLineHeightRaw(range, parserMode)))
            return std::nullopt;
    }

    // Font family must come now.
    if (auto family = consumeFontFamilyRaw(range))
        result.family = *family;
    else
        return std::nullopt;

    if (!range.atEnd())
        return std::nullopt;

    return result;
}

const AtomString& genericFontFamily(CSSValueID ident)
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

WebKitFontFamilyNames::FamilyNamesIndex genericFontFamilyIndex(CSSValueID ident)
{
    switch (ident) {
    case CSSValueSerif:
        return WebKitFontFamilyNames::FamilyNamesIndex::SerifFamily;
    case CSSValueSansSerif:
        return WebKitFontFamilyNames::FamilyNamesIndex::SansSerifFamily;
    case CSSValueCursive:
        return WebKitFontFamilyNames::FamilyNamesIndex::CursiveFamily;
    case CSSValueFantasy:
        return WebKitFontFamilyNames::FamilyNamesIndex::FantasyFamily;
    case CSSValueMonospace:
        return WebKitFontFamilyNames::FamilyNamesIndex::MonospaceFamily;
    case CSSValueWebkitPictograph:
        return WebKitFontFamilyNames::FamilyNamesIndex::PictographFamily;
    case CSSValueSystemUi:
        return WebKitFontFamilyNames::FamilyNamesIndex::SystemUiFamily;
    default:
        ASSERT_NOT_REACHED();
        return WebKitFontFamilyNames::FamilyNamesIndex::StandardFamily;
    }
}

} // namespace CSSPropertyParserHelpers

} // namespace WebCore
