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
#include "ColorInterpolation.h"
#include "ColorLuminance.h"
#include "ColorNormalization.h"
#include "DeprecatedGlobalSettings.h"
#include "Logging.h"
#include "Pair.h"
#include "RenderStyleConstants.h"
#include "StyleColor.h"
#include "WebKitFontFamilyNames.h"
#include <wtf/SortedArrayMap.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

namespace CSSPropertyParserHelpers {

using NumberOrPercentRaw = std::variant<NumberRaw, PercentRaw>;
using NumberOrNoneRaw = std::variant<NumberRaw, NoneRaw>;
using PercentOrNoneRaw = std::variant<PercentRaw, NoneRaw>;
using NumberOrPercentOrNoneRaw = std::variant<NumberRaw, PercentRaw, NoneRaw>;
using AngleOrNumberRaw = std::variant<AngleRaw, NumberRaw>;
using AngleOrNumberOrNoneRaw = std::variant<AngleRaw, NumberRaw, NoneRaw>;

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
    explicit CalcParser(CSSParserTokenRange& range, CalculationCategory destinationCategory, ValueRange valueRange = ValueRange::All, const CSSCalcSymbolTable& symbolTable = { }, CSSValuePool& pool = CSSValuePool::singleton(), NegativePercentagePolicy negativePercentagePolicy = NegativePercentagePolicy::Forbid)
        : m_sourceRange(range)
        , m_range(range)
        , m_pool(pool)
    {
        const CSSParserToken& token = range.peek();
        auto functionId = token.functionId();
        if (CSSCalcValue::isCalcFunction(functionId))
            m_calcValue = CSSCalcValue::create(functionId, consumeFunction(m_range), destinationCategory, valueRange, symbolTable, negativePercentagePolicy == NegativePercentagePolicy::Allow);
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
        if (!m_calcValue)
            return nullptr;

        if (m_calcValue->category() != category) {
            LOG_WITH_STREAM(Calc, stream << "CalcParser::consumeValueIfCategory - failing because calc category " << m_calcValue->category() << " does not match requested category " << category);
            return nullptr;
        }
        m_sourceRange = m_range;
        return m_pool.createValue(WTFMove(m_calcValue));
    }

private:
    CSSParserTokenRange& m_sourceRange;
    CSSParserTokenRange m_range;
    RefPtr<CSSCalcValue> m_calcValue;
    CSSValuePool& m_pool;
};

// MARK: - Primitive value consumers for callers that know the token type.

static RefPtr<CSSCalcValue> consumeCalcRawWithKnownTokenTypeFunction(CSSParserTokenRange& range, CalculationCategory category, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange)
{
    ASSERT(range.peek().type() == FunctionToken);

    const auto& token = range.peek();
    auto functionId = token.functionId();
    if (!CSSCalcValue::isCalcFunction(functionId))
        return nullptr;

    auto calcValue = CSSCalcValue::create(functionId, consumeFunction(range), category, valueRange, symbolTable);
    if (calcValue && calcValue->category() == category)
        return calcValue;

    return nullptr;
}

// MARK: Integer (Raw)

enum class IntegerRange { All, ZeroAndGreater, OneAndGreater };

static constexpr double computeMinimumValue(IntegerRange range)
{
    switch (range) {
    case IntegerRange::All:
        return -std::numeric_limits<double>::infinity();
    case IntegerRange::ZeroAndGreater:
        return 0.0;
    case IntegerRange::OneAndGreater:
        return 1.0;
    }

    RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();

    return 0.0;
}
// MARK: Integer (Raw)

template<typename IntType, IntegerRange integerRange>
struct IntegerTypeRawKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<IntType> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == FunctionToken);

        auto rangeCopy = range;
        if (auto value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Number, { }, ValueRange::All)) {
            range = rangeCopy;
            return clampTo<IntType>(std::round(std::max(value->doubleValue(), computeMinimumValue(integerRange))));
        }

        return std::nullopt;
    }
};

template<typename IntType, IntegerRange integerRange>
struct IntegerTypeRawKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static std::optional<IntType> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == NumberToken);
        
        if (range.peek().numericValueType() == NumberValueType || range.peek().numericValue() < computeMinimumValue(integerRange))
            return std::nullopt;
        return clampTo<IntType>(range.consumeIncludingWhitespace().numericValue());
    }
};

// MARK: Integer (CSSPrimitiveValue - maintaining calc)

template<typename IntType, IntegerRange integerRange>
struct IntegerTypeKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == FunctionToken);

        if (auto integer = IntegerTypeRawKnownTokenTypeFunctionConsumer<IntType, integerRange>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return pool.createValue(*integer, CSSUnitType::CSS_INTEGER);
        return nullptr;
    }
};

template<typename IntType, IntegerRange integerRange>
struct IntegerTypeKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == NumberToken);
        
        if (auto integer = IntegerTypeRawKnownTokenTypeNumberConsumer<IntType, integerRange>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return pool.createValue(*integer, CSSUnitType::CSS_INTEGER);
        return nullptr;
    }
};

// MARK: Number (Raw)

static std::optional<NumberRaw> validatedNumberRaw(double value, ValueRange valueRange)
{
    if (valueRange == ValueRange::NonNegative && value < 0)
        return std::nullopt;
    return {{ value }};
}

struct NumberRawKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<NumberRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == FunctionToken);

        auto rangeCopy = range;
        if (auto value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Number, symbolTable, valueRange)) {
            if (auto validatedValue = validatedNumberRaw(value->doubleValue(), valueRange)) {
                range = rangeCopy;
                return validatedValue;
            }
        }

        return std::nullopt;
    }
};

struct NumberRawKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static std::optional<NumberRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == NumberToken);
        
        if (auto validatedValue = validatedNumberRaw(range.peek().numericValue(), valueRange)) {
            range.consumeIncludingWhitespace();
            return validatedValue;
        }
        return std::nullopt;
    }
};

struct NumberRawKnownTokenTypeIdentConsumer {
    static constexpr CSSParserTokenType tokenType = IdentToken;
    static std::optional<NumberRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
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
};

// MARK: Number (CSSPrimitiveValue - maintaining calc)

struct NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == FunctionToken);

        CalcParser parser(range, CalculationCategory::Number, valueRange, symbolTable, pool);
        return parser.consumeValueIfCategory(CalculationCategory::Number);
    }
};

struct NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == NumberToken);
        
        auto token = range.peek();
        
        if (auto validatedValue = validatedNumberRaw(token.numericValue(), valueRange)) {
            auto unitType = token.unitType();
            range.consumeIncludingWhitespace();
            return pool.createValue(validatedValue->value, unitType);
        }
        return nullptr;
    }
};

// MARK: Percent (raw)

static std::optional<PercentRaw> validatedPercentRaw(double value, ValueRange valueRange)
{
    if (valueRange == ValueRange::NonNegative && value < 0)
        return std::nullopt;
    if (std::isinf(value))
        return std::nullopt;
    return {{ value }};
}

struct PercentRawKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<PercentRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == FunctionToken);

        auto rangeCopy = range;
        if (auto value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Percent, symbolTable, valueRange)) {
            range = rangeCopy;

            // FIXME: Should this validate the calc value as is done for the NumberRaw variant?
            return {{ value->doubleValue() }};
        }

        return std::nullopt;
    }
};

struct PercentRawKnownTokenTypePercentConsumer {
    static constexpr CSSParserTokenType tokenType = PercentageToken;
    static std::optional<PercentRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == PercentageToken);

        if (auto validatedValue = validatedPercentRaw(range.peek().numericValue(), valueRange)) {
            range.consumeIncludingWhitespace();
            return validatedValue;
        }
        return std::nullopt;
    }
};

struct PercentRawKnownTokenTypeIdentConsumer {
    static constexpr CSSParserTokenType tokenType = IdentToken;
    static std::optional<PercentRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
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
};

// MARK: Percent (CSSPrimitiveValue - maintaining calc)

struct PercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == FunctionToken);

        CalcParser parser(range, CalculationCategory::Percent, valueRange, symbolTable, pool);
        return parser.consumeValueIfCategory(CalculationCategory::Percent);
    }
};

struct PercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentConsumer {
    static constexpr CSSParserTokenType tokenType = PercentageToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == PercentageToken);
        
        if (auto validatedValue = validatedPercentRaw(range.peek().numericValue(), valueRange)) {
            range.consumeIncludingWhitespace();
            return pool.createValue(validatedValue->value, CSSUnitType::CSS_PERCENTAGE);
        }
        return nullptr;
    }
};

// MARK: Length (raw)

static std::optional<double> validatedLengthRaw(double value, ValueRange valueRange)
{
    if (valueRange == ValueRange::NonNegative && value < 0)
        return std::nullopt;
    if (std::isinf(value))
        return std::nullopt;
    return value;
}

struct LengthRawKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<LengthRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == FunctionToken);

        auto rangeCopy = range;
        if (auto value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Length, symbolTable, valueRange)) {
            range = rangeCopy;

            // FIXME: Should this validate the calc value as is done for the NumberRaw variant?
            return { { value->primitiveType(), value->doubleValue() } };
        }
        return std::nullopt;
    }
};

struct LengthRawKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static std::optional<LengthRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk, UnitlessZeroQuirk)
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
        case CSSUnitType::CSS_IC:
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
        case CSSUnitType::CSS_VB:
        case CSSUnitType::CSS_VI:
        case CSSUnitType::CSS_SVW:
        case CSSUnitType::CSS_SVH:
        case CSSUnitType::CSS_SVMIN:
        case CSSUnitType::CSS_SVMAX:
        case CSSUnitType::CSS_SVB:
        case CSSUnitType::CSS_SVI:
        case CSSUnitType::CSS_LVW:
        case CSSUnitType::CSS_LVH:
        case CSSUnitType::CSS_LVMIN:
        case CSSUnitType::CSS_LVMAX:
        case CSSUnitType::CSS_LVB:
        case CSSUnitType::CSS_LVI:
        case CSSUnitType::CSS_DVW:
        case CSSUnitType::CSS_DVH:
        case CSSUnitType::CSS_DVMIN:
        case CSSUnitType::CSS_DVMAX:
        case CSSUnitType::CSS_DVB:
        case CSSUnitType::CSS_DVI:
        case CSSUnitType::CSS_Q:
        case CSSUnitType::CSS_CQW:
        case CSSUnitType::CSS_CQH:
        case CSSUnitType::CSS_CQI:
        case CSSUnitType::CSS_CQB:
        case CSSUnitType::CSS_CQMIN:
        case CSSUnitType::CSS_CQMAX:
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
};

struct LengthRawKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static std::optional<LengthRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk)
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
};

// MARK: Length (CSSPrimitiveValue - maintaining calc)

struct LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == FunctionToken);

        CalcParser parser(range, CalculationCategory::Length, valueRange, symbolTable, pool);
        return parser.consumeValueIfCategory(CalculationCategory::Length);
    }
};

struct LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == DimensionToken);

        if (auto lengthRaw = LengthRawKnownTokenTypeDimensionConsumer::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return pool.createValue(lengthRaw->value, lengthRaw->type);
        return nullptr;
    }
};

struct LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (auto lengthRaw = LengthRawKnownTokenTypeNumberConsumer::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return pool.createValue(lengthRaw->value, lengthRaw->type);
        return nullptr;
    }
};

// MARK: Angle (raw)

struct AngleRawKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<AngleRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        auto rangeCopy = range;
        if (auto value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Angle, symbolTable, valueRange)) {
            range = rangeCopy;
            return { { value->primitiveType(), value->doubleValue() } };
        }
        return std::nullopt;
    }
};

struct AngleRawKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static std::optional<AngleRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
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
};

struct AngleRawKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static std::optional<AngleRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (shouldAcceptUnitlessValue(range.peek().numericValue(), parserMode, unitless, unitlessZero))
            return { { CSSUnitType::CSS_DEG, range.consumeIncludingWhitespace().numericValue() } };
        return std::nullopt;
    }
};

// MARK: Angle (CSSPrimitiveValue - maintaining calc)

struct AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == FunctionToken);

        CalcParser parser(range, CalculationCategory::Angle, valueRange, symbolTable, pool);
        return parser.consumeValueIfCategory(CalculationCategory::Angle);
    }
};

struct AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == DimensionToken);

        if (auto angleRaw = AngleRawKnownTokenTypeDimensionConsumer::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return pool.createValue(angleRaw->value, angleRaw->type);
        return nullptr;
    }
};

struct AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (auto angleRaw = AngleRawKnownTokenTypeNumberConsumer::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return pool.createValue(angleRaw->value, angleRaw->type);
        return nullptr;
    }
};

// MARK: Time (CSSPrimitiveValue - maintaining calc)

struct TimeCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == FunctionToken);

        CalcParser parser(range, CalculationCategory::Time, valueRange, symbolTable, pool);
        return parser.consumeValueIfCategory(CalculationCategory::Time);
    }
};

struct TimeCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == DimensionToken);

        if (valueRange == ValueRange::NonNegative && range.peek().numericValue() < 0)
            return nullptr;

        if (auto unit = range.peek().unitType(); unit == CSSUnitType::CSS_MS || unit == CSSUnitType::CSS_S)
            return pool.createValue(range.consumeIncludingWhitespace().numericValue(), unit);

        return nullptr;
    }
};

struct TimeCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (unitless == UnitlessQuirk::Allow && shouldAcceptUnitlessValue(range.peek().numericValue(), parserMode, unitless, UnitlessZeroQuirk::Allow)) {
            if (valueRange == ValueRange::NonNegative && range.peek().numericValue() < 0)
                return nullptr;
            return pool.createValue(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_MS);
        }
        return nullptr;
    }
};

// MARK: Resolution (CSSPrimitiveValue - no calc)

struct ResolutionCSSPrimitiveValueWithKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk, CSSValuePool& pool)
    {
        ASSERT(range.peek().type() == DimensionToken);

        if (auto unit = range.peek().unitType(); unit == CSSUnitType::CSS_DPPX || unit == CSSUnitType::CSS_X || unit == CSSUnitType::CSS_DPI || unit == CSSUnitType::CSS_DPCM)
            return pool.createValue(range.consumeIncludingWhitespace().numericValue(), unit);

        return nullptr;
    }
};

// MARK: None (Raw)

struct NoneRawKnownTokenTypeIdentConsumer {
    static constexpr CSSParserTokenType tokenType = IdentToken;
    static std::optional<NoneRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == IdentToken);

        if (range.peek().id() == CSSValueNone) {
            range.consumeIncludingWhitespace();
            return NoneRaw { };
        }

        return std::nullopt;
    }
};

// MARK: Specialized combination consumers.

// FIXME: It would be good to find a way to synthesize this from an angle and number specific variants.
struct AngleOrNumberRawKnownTokenTypeIdentConsumer {
    static constexpr CSSParserTokenType tokenType = IdentToken;
    static std::optional<AngleOrNumberRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == IdentToken);

        if (auto variable = symbolTable.get(range.peek().id())) {
            switch (variable->type) {
            case CSSUnitType::CSS_DEG:
            case CSSUnitType::CSS_RAD:
            case CSSUnitType::CSS_GRAD:
            case CSSUnitType::CSS_TURN:
                range.consumeIncludingWhitespace();
                return AngleRaw { variable->type, variable->value };

            case CSSUnitType::CSS_NUMBER:
                range.consumeIncludingWhitespace();
                return NumberRaw { variable->value };

            default:
                break;
            }
        }

        return std::nullopt;
    }
};

// FIXME: It would be good to find a way to synthesize this from an number and percent specific variants.
struct NumberOrPercentRawKnownTokenTypeIdentConsumer {
    static constexpr CSSParserTokenType tokenType = IdentToken;
    static std::optional<NumberOrPercentRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == IdentToken);

        if (auto variable = symbolTable.get(range.peek().id())) {
            switch (variable->type) {
            case CSSUnitType::CSS_PERCENTAGE:
                if (auto validatedValue = validatedPercentRaw(variable->value, valueRange)) {
                    range.consumeIncludingWhitespace();
                    return {{ *validatedValue }};
                }
                break;

            case CSSUnitType::CSS_NUMBER:
                if (auto validatedValue = validatedNumberRaw(variable->value, valueRange)) {
                    range.consumeIncludingWhitespace();
                    return {{ *validatedValue }};
                }
                break;

            default:
                break;
            }
        }

        return std::nullopt;
    }
};

// MARK: - Meta Consumers

template<typename Consumer, typename = void>
struct TransformApplier {
    template<typename T> static decltype(auto) apply(T value) { return value; }
};

template<typename Consumer>
struct TransformApplier<Consumer, typename std::void_t<typename Consumer::Transformer>> {
    template<typename T> static decltype(auto) apply(T value) { return Consumer::Transformer::transform(value); }
};

template<typename Consumer, typename T>
static decltype(auto) applyTransform(T value)
{
    return TransformApplier<Consumer>::apply(value);
}

// Transformers:

template<typename ResultType>
struct IdentityTransformer {
    using Result = ResultType;
    static Result transform(Result value) { return value; }
};

template<typename ResultType>
struct RawIdentityTransformer {
    using Result = std::optional<ResultType>;

    static Result transform(Result value) { return value; }

    template<typename... Types>
    static Result transform(const std::variant<Types...>& value)
    {
        return WTF::switchOn(value, [] (auto value) -> Result { return value; });
    }

    template<typename T>
    static Result transform(std::optional<T> value)
    {
        if (value)
            return transform(*value);
        return std::nullopt;
    }
};

template<typename Parent, typename ResultType>
struct RawVariantTransformerBase {
    using Result = std::optional<ResultType>;

    template<typename... Types>
    static Result transform(const std::variant<Types...>& value)
    {
        return WTF::switchOn(value, [] (auto value) { return Parent::transform(value); });
    }

    static typename Result::value_type transform(typename Result::value_type value)
    {
        return value;
    }

    template<typename T>
    static Result transform(std::optional<T> value)
    {
        if (value)
            return Parent::transform(*value);
        return std::nullopt;
    }
};

struct NumberOrPercentDividedBy100Transformer : RawVariantTransformerBase<NumberOrPercentDividedBy100Transformer, double> {
    using RawVariantTransformerBase<NumberOrPercentDividedBy100Transformer, double>::transform;

    static double transform(NumberRaw value)
    {
        return value.value;
    }

    static double transform(PercentRaw value)
    {
        return value.value / 100.0;
    }
};

// MARK: MetaConsumerDispatcher

template<CSSParserTokenType tokenType, typename Consumer, typename = void>
struct MetaConsumerDispatcher {
    template<typename... Args>
    static typename Consumer::Result consume(Args&&...)
    {
        return { };
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<FunctionToken, Consumer, typename std::void_t<typename Consumer::FunctionToken>> {
    template<typename... Args>
    static typename Consumer::Result consume(Args&&... args)
    {
        return applyTransform<Consumer>(Consumer::FunctionToken::consume(std::forward<Args>(args)...));
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<NumberToken, Consumer, typename std::void_t<typename Consumer::NumberToken>> {
    template<typename... Args>
    static typename Consumer::Result consume(Args&&... args)
    {
        return applyTransform<Consumer>(Consumer::NumberToken::consume(std::forward<Args>(args)...));
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<PercentageToken, Consumer, typename std::void_t<typename Consumer::PercentageToken>> {
    template<typename... Args>
    static typename Consumer::Result consume(Args&&... args)
    {
        return applyTransform<Consumer>(Consumer::PercentageToken::consume(std::forward<Args>(args)...));
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<DimensionToken, Consumer, typename std::void_t<typename Consumer::DimensionToken>> {
    template<typename... Args>
    static typename Consumer::Result consume(Args&&... args)
    {
        return applyTransform<Consumer>(Consumer::DimensionToken::consume(std::forward<Args>(args)...));
    }
};

template<typename Consumer>
struct MetaConsumerDispatcher<IdentToken, Consumer, typename std::void_t<typename Consumer::IdentToken>> {
    template<typename... Args>
    static typename Consumer::Result consume(Args&&... args)
    {
        return applyTransform<Consumer>(Consumer::IdentToken::consume(std::forward<Args>(args)...));
    }
};

template<typename Consumer, typename... Args>
auto consumeMetaConsumer(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, Args&&... args) -> typename Consumer::Result
{
    switch (range.peek().type()) {
    case FunctionToken:
        return MetaConsumerDispatcher<FunctionToken, Consumer>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero, std::forward<Args>(args)...);

    case NumberToken:
        return MetaConsumerDispatcher<NumberToken, Consumer>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero, std::forward<Args>(args)...);

    case PercentageToken:
        return MetaConsumerDispatcher<PercentageToken, Consumer>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero, std::forward<Args>(args)...);

    case DimensionToken:
        return MetaConsumerDispatcher<DimensionToken, Consumer>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero, std::forward<Args>(args)...);

    case IdentToken:
        return MetaConsumerDispatcher<IdentToken, Consumer>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero, std::forward<Args>(args)...);
    
    default:
        return { };
    }
}

// MARK: SameTokenMetaConsumer

template<typename Transformer, typename ConsumersTuple, size_t I>
struct SameTokenMetaConsumerApplier {
    template<typename... Args>
    static typename Transformer::Result consume(Args&&... args)
    {
        using SelectedConsumer = std::tuple_element_t<I - 1, ConsumersTuple>;
        if (auto result = Transformer::transform(SelectedConsumer::consume(std::forward<Args>(args)...)))
            return result;
        return SameTokenMetaConsumerApplier<Transformer, ConsumersTuple, I - 1>::consume(std::forward<Args>(args)...);
    }
};

template<typename Transformer, typename ConsumersTuple>
struct SameTokenMetaConsumerApplier<Transformer, ConsumersTuple, 0> {
    template<typename... Args>
    static typename Transformer::Result consume(Args&&...)
    {
        return typename Transformer::Result { };
    }
};

template<typename Transformer, typename T, typename... Ts>
struct SameTokenMetaConsumer {
    static_assert(std::conjunction_v<std::bool_constant<Ts::tokenType == T::tokenType>...>, "All Consumers passed to SameTokenMetaConsumer must have the same tokenType");
    using ConsumersTuple = std::tuple<T, Ts...>;

    static constexpr CSSParserTokenType tokenType = T::tokenType;
    template<typename... Args>
    static typename Transformer::Result consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, Args&&... args)
    {
        ASSERT(range.peek().type() == tokenType);
        return SameTokenMetaConsumerApplier<Transformer, ConsumersTuple, std::tuple_size_v<ConsumersTuple>>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero, std::forward<Args>(args)...);
    }
};


// MARK: - Consumer definitions.

// MARK: Integer

template<typename IntType, IntegerRange integerRange>
struct IntegerTypeRawConsumer {
    using Result = std::optional<IntType>;

    using FunctionToken = IntegerTypeRawKnownTokenTypeFunctionConsumer<IntType, integerRange>;
    using NumberToken = IntegerTypeRawKnownTokenTypeNumberConsumer<IntType, integerRange>;
};

template<typename IntType, IntegerRange integerRange>
struct IntegerTypeConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = IntegerTypeKnownTokenTypeFunctionConsumer<IntType, integerRange>;
    using NumberToken = IntegerTypeKnownTokenTypeNumberConsumer<IntType, integerRange>;
};

// MARK: Number

template<typename T>
struct NumberRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = NumberRawKnownTokenTypeFunctionConsumer;
    using NumberToken = NumberRawKnownTokenTypeNumberConsumer;
};

template<typename Transformer>
struct NumberRawAllowingSymbolTableIdentConsumer : NumberRawConsumer<Transformer> {
    using IdentToken = NumberRawKnownTokenTypeIdentConsumer;
};

struct NumberConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer;
    using NumberToken = NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer;
};

// MARK: Percent

template<typename T>
struct PercentRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = PercentRawKnownTokenTypeFunctionConsumer;
    using PercentageToken = PercentRawKnownTokenTypePercentConsumer;
};

template<typename Transformer>
struct PercentRawAllowingSymbolTableIdentConsumer : PercentRawConsumer<Transformer> {
    using IdentToken = PercentRawKnownTokenTypeIdentConsumer;
};

struct PercentConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = PercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer;
    using PercentageToken = PercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentConsumer;
};

// MARK: Length

template<typename T>
struct LengthRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = LengthRawKnownTokenTypeFunctionConsumer;
    using DimensionToken = LengthRawKnownTokenTypeDimensionConsumer;
    using NumberToken = LengthRawKnownTokenTypeNumberConsumer;
};

struct LengthConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer;
    using NumberToken = LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer;
    using DimensionToken = LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer;
};

// MARK: Angle

template<typename T>
struct AngleRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = AngleRawKnownTokenTypeFunctionConsumer;
    using NumberToken = AngleRawKnownTokenTypeNumberConsumer;
    using DimensionToken = AngleRawKnownTokenTypeDimensionConsumer;
};

struct AngleConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer;
    using NumberToken = AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer;
    using DimensionToken = AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer;
};

// MARK: Time

struct TimeConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = TimeCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer;
    using NumberToken = TimeCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer;
    using DimensionToken = TimeCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer;
};

// MARK: Resolution

struct ResolutionConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    // NOTE: Unlike the other types, calc() does not work with <resolution>.
    using DimensionToken = ResolutionCSSPrimitiveValueWithKnownTokenTypeDimensionConsumer;
};

// MARK: - Combination consumer definitions.

// MARK: Angle + Percent

struct AngleOrPercentConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = SameTokenMetaConsumer<
        IdentityTransformer<RefPtr<CSSPrimitiveValue>>,
        AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer,
        PercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer
    >;
    using NumberToken = AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer;
    using PercentageToken = PercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentConsumer;
    using DimensionToken = AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer;
};

// MARK: Length + Percent

template<typename T>
struct LengthOrPercentRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = SameTokenMetaConsumer<
        Transformer,
        PercentRawKnownTokenTypeFunctionConsumer,
        LengthRawKnownTokenTypeFunctionConsumer
    >;
    using NumberToken = LengthRawKnownTokenTypeNumberConsumer;
    using PercentageToken = PercentRawKnownTokenTypePercentConsumer;
    using DimensionToken = LengthRawKnownTokenTypeDimensionConsumer;
};

// MARK: Angle + Number

template<typename T>
struct AngleOrNumberRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = SameTokenMetaConsumer<
        Transformer,
        NumberRawKnownTokenTypeFunctionConsumer,
        AngleRawKnownTokenTypeFunctionConsumer
    >;
    using NumberToken = NumberRawKnownTokenTypeNumberConsumer;
    using DimensionToken = AngleRawKnownTokenTypeDimensionConsumer;
};

template<typename Transformer>
struct AngleOrNumberRawAllowingSymbolTableIdentConsumer : AngleOrNumberRawConsumer<Transformer> {
    using IdentToken = AngleOrNumberRawKnownTokenTypeIdentConsumer;
};

// MARK: Angle + Number + None

template<typename Transformer>
struct AngleOrNumberOrNoneRawConsumer : AngleOrNumberRawConsumer<Transformer> {
    using IdentToken = NoneRawKnownTokenTypeIdentConsumer;
};

template<typename Transformer>
struct AngleOrNumberOrNoneRawAllowingSymbolTableIdentConsumer : AngleOrNumberRawConsumer<Transformer> {
    using IdentToken = SameTokenMetaConsumer<
        Transformer,
        NoneRawKnownTokenTypeIdentConsumer,
        AngleOrNumberRawKnownTokenTypeIdentConsumer
    >;
};

// MARK: Number + Percent

template<typename T>
struct NumberOrPercentRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = SameTokenMetaConsumer<
        Transformer,
        PercentRawKnownTokenTypeFunctionConsumer,
        NumberRawKnownTokenTypeFunctionConsumer
    >;
    using NumberToken = NumberRawKnownTokenTypeNumberConsumer;
    using PercentageToken = PercentRawKnownTokenTypePercentConsumer;
};


template<typename Transformer>
struct NumberOrPercentRawAllowingSymbolTableIdentConsumer : NumberOrPercentRawConsumer<Transformer> {
    using IdentToken = NumberOrPercentRawKnownTokenTypeIdentConsumer;
};

// MARK: Number + None

template<typename Transformer>
struct NumberOrNoneRawConsumer : NumberRawConsumer<Transformer> {
    using IdentToken = NoneRawKnownTokenTypeIdentConsumer;
};

template<typename Transformer>
struct NumberOrNoneRawAllowingSymbolTableIdentConsumer : NumberRawConsumer<Transformer> {
    using IdentToken = SameTokenMetaConsumer<
        Transformer,
        NoneRawKnownTokenTypeIdentConsumer,
        NumberRawKnownTokenTypeIdentConsumer
    >;
};

// MARK: Percent + None

template<typename Transformer>
struct PercentOrNoneRawConsumer : PercentRawConsumer<Transformer> {
    using IdentToken = NoneRawKnownTokenTypeIdentConsumer;
};

template<typename Transformer>
struct PercentOrNoneRawAllowingSymbolTableIdentConsumer : PercentRawConsumer<Transformer> {
    using IdentToken = SameTokenMetaConsumer<
        Transformer,
        NoneRawKnownTokenTypeIdentConsumer,
        PercentRawKnownTokenTypeIdentConsumer
    >;
};

// MARK: Number + Percent + None

template<typename Transformer>
struct NumberOrPercentOrNoneRawConsumer : NumberOrPercentRawConsumer<Transformer> {
    using IdentToken = NoneRawKnownTokenTypeIdentConsumer;
};

template<typename Transformer>
struct NumberOrPercentOrNoneRawAllowingSymbolTableIdentConsumer : NumberOrPercentRawConsumer<Transformer> {
    using IdentToken = SameTokenMetaConsumer<
        Transformer,
        NoneRawKnownTokenTypeIdentConsumer,
        NumberOrPercentRawKnownTokenTypeIdentConsumer
    >;
};

// MARK: - Consumer functions - utilize consumer definitions above, giving more targetted interfaces and allowing exposure to other files.

template<typename IntType, IntegerRange integerRange> std::optional<IntType> consumeIntegerTypeRaw(CSSParserTokenRange& range)
{
    return consumeMetaConsumer<IntegerTypeRawConsumer<IntType, integerRange>>(range, { }, ValueRange::All, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename IntType, IntegerRange integerRange> RefPtr<CSSPrimitiveValue> consumeIntegerType(CSSParserTokenRange& range, CSSValuePool& pool)
{
    return consumeMetaConsumer<IntegerTypeConsumer<IntType, integerRange>>(range, { }, ValueRange::All, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid, pool);
}

std::optional<int> consumeIntegerRaw(CSSParserTokenRange& range)
{
    return consumeIntegerTypeRaw<int, IntegerRange::All>(range);
}

RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range)
{
    return consumeIntegerType<int, IntegerRange::All>(range, CSSValuePool::singleton());
}

std::optional<int> consumeIntegerZeroAndGreaterRaw(CSSParserTokenRange& range)
{
    return consumeIntegerTypeRaw<int, IntegerRange::ZeroAndGreater>(range);
}

RefPtr<CSSPrimitiveValue> consumeIntegerZeroAndGreater(CSSParserTokenRange& range)
{
    return consumeIntegerType<int, IntegerRange::ZeroAndGreater>(range, CSSValuePool::singleton());
}

std::optional<unsigned> consumePositiveIntegerRaw(CSSParserTokenRange& range)
{
    return consumeIntegerTypeRaw<unsigned, IntegerRange::OneAndGreater>(range);
}

RefPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange& range)
{
    return consumeIntegerType<unsigned, IntegerRange::OneAndGreater>(range, CSSValuePool::singleton());
}

std::optional<NumberRaw> consumeNumberRaw(CSSParserTokenRange& range, ValueRange valueRange)
{
    return consumeMetaConsumer<NumberRawConsumer<RawIdentityTransformer<NumberRaw>>>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

RefPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange& range, ValueRange valueRange)
{
    return consumeMetaConsumer<NumberConsumer>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid, CSSValuePool::singleton());
}

static std::optional<PercentRaw> consumePercentRaw(CSSParserTokenRange& range)
{
    return consumeMetaConsumer<PercentRawConsumer<RawIdentityTransformer<PercentRaw>>>(range, { }, ValueRange::All, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

RefPtr<CSSPrimitiveValue> consumePercent(CSSParserTokenRange& range, ValueRange valueRange)
{
    return consumePercentWorkerSafe(range, valueRange, CSSValuePool::singleton());
}

RefPtr<CSSPrimitiveValue> consumePercentWorkerSafe(CSSParserTokenRange& range, ValueRange valueRange, CSSValuePool& pool)
{
    return consumeMetaConsumer<PercentConsumer>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid, pool);
}

RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    return consumeMetaConsumer<LengthConsumer>(range, { }, valueRange, parserMode, unitless, UnitlessZeroQuirk::Forbid, CSSValuePool::singleton());
}

#if ENABLE(VARIATION_FONTS)
static std::optional<AngleRaw> consumeAngleRaw(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    return consumeMetaConsumer<AngleRawConsumer<RawIdentityTransformer<AngleRaw>>>(range, { }, ValueRange::All, parserMode, unitless, unitlessZero);
}
#endif

RefPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    return consumeAngleWorkerSafe(range, parserMode, CSSValuePool::singleton(), unitless, unitlessZero);
}

RefPtr<CSSPrimitiveValue> consumeAngleWorkerSafe(CSSParserTokenRange& range, CSSParserMode parserMode, CSSValuePool& pool, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    return consumeMetaConsumer<AngleConsumer>(range, { }, ValueRange::All, parserMode, unitless, unitlessZero, pool);
}

RefPtr<CSSPrimitiveValue> consumeTime(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    return consumeMetaConsumer<TimeConsumer>(range, { }, valueRange, parserMode, unitless, UnitlessZeroQuirk::Forbid, CSSValuePool::singleton());
}

RefPtr<CSSPrimitiveValue> consumeResolution(CSSParserTokenRange& range)
{
    return consumeMetaConsumer<ResolutionConsumer>(range, { }, ValueRange::All, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid, CSSValuePool::singleton());
}

static RefPtr<CSSPrimitiveValue> consumeAngleOrPercent(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    return consumeMetaConsumer<AngleOrPercentConsumer>(range, { }, ValueRange::All, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow, CSSValuePool::singleton());
}

static std::optional<LengthOrPercentRaw> consumeLengthOrPercentRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    return consumeMetaConsumer<LengthOrPercentRawConsumer<RawIdentityTransformer<LengthOrPercentRaw>>>(range, { }, ValueRange::NonNegative, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

// FIXME: This doesn't work with the current scheme due to the NegativePercentagePolicy parameter
RefPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless, NegativePercentagePolicy negativePercentagePolicy)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken: {
        // FIXME: Should this be using trying to generate the calc with both Length and Percent destination category types?
        CalcParser parser(range, CalculationCategory::Length, valueRange, { }, CSSValuePool::singleton(), negativePercentagePolicy);
        if (auto calculation = parser.value(); calculation && canConsumeCalcValue(calculation->category(), parserMode))
            return parser.consumeValue();
        break;
    }

    case DimensionToken:
        return LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer::consume(range, { }, valueRange, parserMode, unitless, UnitlessZeroQuirk::Forbid, CSSValuePool::singleton());

    case NumberToken:
        return LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer::consume(range, { }, valueRange, parserMode, unitless, UnitlessZeroQuirk::Forbid, CSSValuePool::singleton());

    case PercentageToken:
        return PercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentConsumer::consume(range, { }, valueRange, parserMode, unitless, UnitlessZeroQuirk::Forbid, CSSValuePool::singleton());

    default:
        break;
    }

    return nullptr;
}

template<typename Transformer = RawIdentityTransformer<AngleOrNumberOrNoneRaw>>
static auto consumeAngleOrNumberOrNoneRaw(CSSParserTokenRange& range, CSSParserMode parserMode) -> typename Transformer::Result
{
    return consumeMetaConsumer<AngleOrNumberOrNoneRawConsumer<Transformer>>(range, { }, ValueRange::All, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename Transformer = RawIdentityTransformer<AngleOrNumberOrNoneRaw>>
static auto consumeAngleOrNumberOrNoneRawAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, CSSParserMode parserMode) -> typename Transformer::Result
{
    return consumeMetaConsumer<AngleOrNumberOrNoneRawAllowingSymbolTableIdentConsumer<Transformer>>(range, symbolTable, ValueRange::All, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename Transformer = RawIdentityTransformer<NumberOrPercentRaw>>
static auto consumeNumberOrPercentRaw(CSSParserTokenRange& range, ValueRange valueRange = ValueRange::All) -> typename Transformer::Result
{
    return consumeMetaConsumer<NumberOrPercentRawConsumer<Transformer>>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename Transformer = RawIdentityTransformer<NumberOrNoneRaw>>
static auto consumeNumberOrNoneRaw(CSSParserTokenRange& range, ValueRange valueRange = ValueRange::All) -> typename Transformer::Result
{
    return consumeMetaConsumer<NumberOrNoneRawConsumer<Transformer>>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename Transformer = RawIdentityTransformer<PercentOrNoneRaw>>
static auto consumePercentOrNoneRaw(CSSParserTokenRange& range, ValueRange valueRange = ValueRange::All) -> typename Transformer::Result
{
    return consumeMetaConsumer<PercentOrNoneRawConsumer<Transformer>>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename Transformer = RawIdentityTransformer<PercentOrNoneRaw>>
static auto consumePercentOrNoneRawAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange = ValueRange::All) -> typename Transformer::Result
{
    return consumeMetaConsumer<PercentOrNoneRawAllowingSymbolTableIdentConsumer<Transformer>>(range, symbolTable, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename Transformer = RawIdentityTransformer<NumberOrPercentOrNoneRaw>>
static auto consumeNumberOrPercentOrNoneRaw(CSSParserTokenRange& range, ValueRange valueRange = ValueRange::All) -> typename Transformer::Result
{
    return consumeMetaConsumer<NumberOrPercentOrNoneRawConsumer<Transformer>>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename Transformer = RawIdentityTransformer<NumberOrPercentOrNoneRaw>>
static auto consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange = ValueRange::All) -> typename Transformer::Result
{
    return consumeMetaConsumer<NumberOrPercentOrNoneRawAllowingSymbolTableIdentConsumer<Transformer>>(range, symbolTable, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

// FIXME: This needs a more clear name to indicate its behavior of dividing percents by 100 only if an explicit percent token, not if a result of a calc().
RefPtr<CSSPrimitiveValue> consumeNumberOrPercent(CSSParserTokenRange& range, ValueRange valueRange)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        if (auto value = NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer::consume(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid, CSSValuePool::singleton()))
            return value;
        return PercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer::consume(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid, CSSValuePool::singleton());

    case NumberToken:
        return NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer::consume(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid, CSSValuePool::singleton());

    case PercentageToken:
        if (auto percentRaw = PercentRawKnownTokenTypePercentConsumer::consume(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid))
            return CSSValuePool::singleton().createValue(percentRaw->value / 100.0, CSSUnitType::CSS_NUMBER);
        break;

    default:
        break;
    }

    return nullptr;
}

// MARK: - Non-primitive consumers.

static std::optional<double> consumeFontWeightNumberRaw(CSSParserTokenRange& range)
{
#if !ENABLE(VARIATION_FONTS)
    auto isIntegerAndDivisibleBy100 = [](double value) {
        ASSERT(value >= 1 && value <= 1000);
        return static_cast<int>(value / 100) * 100 == value;
    };
#endif

    auto& token = range.peek();
    switch (token.type()) {
    case FunctionToken: {
        // "[For calc()], the used value resulting from an expression must be clamped to the range allowed in the target context."
        auto result = NumberRawKnownTokenTypeFunctionConsumer::consume(range, { }, ValueRange::All, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
        if (!result)
            return std::nullopt;
#if !ENABLE(VARIATION_FONTS)
        if (!(result->value >= 1 && result->value <= 1000) || !isIntegerAndDivisibleBy100(result->value))
            return std::nullopt;
#endif
        return std::clamp(result->value, 1.0, 1000.0);
    }

    case NumberToken: {
        auto result = token.numericValue();
        if (!(result >= 1 && result <= 1000))
            return std::nullopt;
#if !ENABLE(VARIATION_FONTS)
        if (token.numericValueType() != IntegerValueType || !isIntegerAndDivisibleBy100(result))
            return std::nullopt;
#endif
        range.consumeIncludingWhitespace();
        return result;
    }

    default:
        return std::nullopt;
    }
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

static std::optional<CSSValueID> consumeIdentRangeRaw(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
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
    if (range.peek().type() != IdentToken || !isValidCustomIdentifier(range.peek().id()))
        return nullptr;
    auto identifier = range.consumeIncludingWhitespace().value();
    return CSSValuePool::singleton().createCustomIdent(shouldLowercase ? identifier.convertToASCIILowercase() : identifier.toString());
}

RefPtr<CSSPrimitiveValue> consumeDashedIdent(CSSParserTokenRange& range, bool shouldLowercase)
{
    auto result = consumeCustomIdent(range, shouldLowercase);
    if (result && result->stringValue().startsWith("--"_s))
        return result;
    return nullptr;
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
    if (StyleColor::isSystemColorKeyword(keyword))
        return { };

    return StyleColor::colorFromKeyword(keyword, { });
}

static std::optional<double> consumeOptionalAlpha(CSSParserTokenRange& range)
{
    if (!consumeSlashIncludingWhitespace(range))
        return 1.0;

    if (auto alphaParameter = consumeNumberOrPercentOrNoneRaw(range)) {
        return WTF::switchOn(*alphaParameter,
            [] (NumberRaw number) { return std::clamp(number.value, 0.0, 1.0); },
            [] (PercentRaw percent) { return std::clamp(percent.value / 100.0, 0.0, 1.0); },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
    }

    return std::nullopt;
}

static std::optional<double> consumeOptionalAlphaAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable)
{
    if (!consumeSlashIncludingWhitespace(range))
        return 1.0;

    if (auto alphaParameter = consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(range, symbolTable, ValueRange::All)) {
        return WTF::switchOn(*alphaParameter,
            [] (NumberRaw number) { return std::clamp(number.value, 0.0, 1.0); },
            [] (PercentRaw percent) { return std::clamp(percent.value / 100.0, 0.0, 1.0); },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
    }

    return std::nullopt;
}

static uint8_t normalizeRGBComponentToSRGBAByte(NumberRaw value)
{
    return convertPrescaledSRGBAFloatToSRGBAByte(value.value);
}

static uint8_t normalizeRGBComponentToSRGBAByte(PercentRaw value)
{
    return convertPrescaledSRGBAFloatToSRGBAByte(value.value / 100.0 * 255.0);
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

    if (auto alphaParameter = consumeNumberOrPercentOrNoneRaw(args)) {
        return WTF::switchOn(*alphaParameter,
            [] (NumberRaw number) { return std::clamp(number.value, 0.0, 1.0); },
            [] (PercentRaw percent) { return std::clamp(percent.value / 100.0, 0.0, 1.0); },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
    }

    return std::nullopt;
}

static Color parseRelativeRGBParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColor(args, context);
    if (!originColor.isValid())
        return { };

    auto originColorAsSRGB = originColor.toColorTypeLossy<SRGBA<float>>().resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueR, CSSUnitType::CSS_PERCENTAGE, originColorAsSRGB.red * 100.0 },
        { CSSValueG, CSSUnitType::CSS_PERCENTAGE, originColorAsSRGB.green * 100.0 },
        { CSSValueB, CSSUnitType::CSS_PERCENTAGE, originColorAsSRGB.blue * 100.0 },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsSRGB.alpha * 100.0 }
    };

    auto red = consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable);
    if (!red)
        return { };

    auto green = consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable);
    if (!green)
        return { };

    auto blue = consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable);
    if (!blue)
        return { };

    auto alpha = consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    if (std::holds_alternative<NoneRaw>(*red) || std::holds_alternative<NoneRaw>(*green) || std::holds_alternative<NoneRaw>(*blue) || std::isnan(*alpha)) {
        auto normalizeComponentAllowingNone = [] (auto component) {
            return WTF::switchOn(component,
                [] (PercentRaw percent) { return std::clamp(percent.value / 100.0, 0.0, 1.0); },
                [] (NumberRaw number) { return std::clamp(number.value / 255.0, 0.0, 1.0); },
                [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
            );
        };

        auto normalizedRed = normalizeComponentAllowingNone(*red);
        auto normalizedGreen = normalizeComponentAllowingNone(*green);
        auto normalizedBlue = normalizeComponentAllowingNone(*blue);

        // If any component uses "none", we store the value as a SRGBA<float> to allow for storage of the special value as NaN.
        return SRGBA<float> { static_cast<float>(normalizedRed), static_cast<float>(normalizedGreen), static_cast<float>(normalizedBlue), static_cast<float>(*alpha) };
    }

    auto normalizeComponentDisallowingNone = [] (auto component) {
        return WTF::switchOn(component,
            [] (NumberRaw number) -> uint8_t { return normalizeRGBComponentToSRGBAByte(number); },
            [] (PercentRaw percent) -> uint8_t { return normalizeRGBComponentToSRGBAByte(percent); },
            [] (NoneRaw) -> uint8_t { ASSERT_NOT_REACHED(); return 0; }
        );
    };

    auto normalizedRed = normalizeComponentDisallowingNone(*red);
    auto normalizedGreen = normalizeComponentDisallowingNone(*green);
    auto normalizedBlue = normalizeComponentDisallowingNone(*blue);
    auto normalizedAlpha = convertFloatAlphaTo<uint8_t>(*alpha);

    return SRGBA<uint8_t> { normalizedRed, normalizedGreen, normalizedBlue, normalizedAlpha };
}

static Color parseNonRelativeRGBParameters(CSSParserTokenRange& args)
{
    struct Component {
        enum class Type { Number, Percentage, Unknown };
        
        double value;
        Type type;
    };

    auto consumeComponent = [](auto& args, auto previousComponentType) -> std::optional<Component> {
        switch (previousComponentType) {
        case Component::Type::Number:
            if (auto component = consumeNumberOrNoneRaw(args)) {
                return WTF::switchOn(*component,
                    [] (NumberRaw number) -> Component { return { number.value, Component::Type::Number }; },
                    [] (NoneRaw) -> Component { return { std::numeric_limits<double>::quiet_NaN(), Component::Type::Number }; }
                );
            }
            return std::nullopt;
        case Component::Type::Percentage:
            if (auto component = consumePercentOrNoneRaw(args)) {
                return WTF::switchOn(*component,
                    [] (PercentRaw percent) -> Component  { return { percent.value, Component::Type::Percentage }; },
                    [] (NoneRaw) -> Component { return { std::numeric_limits<double>::quiet_NaN(), Component::Type::Percentage }; }
                );
            }
            return std::nullopt;
        case Component::Type::Unknown:
            if (auto component = consumeNumberOrPercentOrNoneRaw(args)) {
                return WTF::switchOn(*component,
                    [] (NumberRaw number) -> Component { return { number.value, Component::Type::Number }; },
                    [] (PercentRaw percent) -> Component  { return { percent.value, Component::Type::Percentage }; },
                    [] (NoneRaw) -> Component { return { std::numeric_limits<double>::quiet_NaN(), Component::Type::Unknown }; }
                );
            }
            return std::nullopt;
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto red = consumeComponent(args, Component::Type::Unknown);
    if (!red)
        return { };

    auto syntax = consumeCommaIncludingWhitespace(args) ? RGBOrHSLSeparatorSyntax::Commas : RGBOrHSLSeparatorSyntax::WhitespaceSlash;

    auto green = consumeComponent(args, red->type);
    if (!green)
        return { };

    if (!consumeRGBOrHSLSeparator(args, syntax))
        return { };

    auto blue = consumeComponent(args, green->type);
    if (!blue)
        return { };

    auto resolvedComponentType = blue->type;

    auto alpha = consumeRGBOrHSLOptionalAlpha(args, syntax);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    if (std::isnan(red->value) || std::isnan(green->value) || std::isnan(blue->value) || std::isnan(*alpha)) {
        // "none" values are only allowed with the WhitespaceSlash syntax.
        if (syntax != RGBOrHSLSeparatorSyntax::WhitespaceSlash)
            return { };

        auto normalizeNumber = [] (double number) { return std::isnan(number) ? number : std::clamp(number / 255.0, 0.0, 1.0); };
        auto normalizePercent = [] (double percent) { return std::isnan(percent) ? percent : std::clamp(percent / 100.0, 0.0, 1.0); };

        // If any component uses "none", we store the value as a SRGBA<float> to allow for storage of the special value as NaN.
        switch (resolvedComponentType) {
        case Component::Type::Number:
            return SRGBA<float> { static_cast<float>(normalizeNumber(red->value)), static_cast<float>(normalizeNumber(green->value)), static_cast<float>(normalizeNumber(blue->value)), static_cast<float>(*alpha) };
        case Component::Type::Percentage:
            return SRGBA<float> { static_cast<float>(normalizePercent(red->value)), static_cast<float>(normalizePercent(green->value)), static_cast<float>(normalizePercent(blue->value)), static_cast<float>(*alpha) };
        case Component::Type::Unknown:
            return SRGBA<float> { static_cast<float>(red->value), static_cast<float>(green->value), static_cast<float>(blue->value), static_cast<float>(*alpha) };
        }

        ASSERT_NOT_REACHED();
        return { };
    }

    switch (resolvedComponentType) {
    case Component::Type::Number:
        return SRGBA<uint8_t> { normalizeRGBComponentToSRGBAByte(NumberRaw { red->value }), normalizeRGBComponentToSRGBAByte(NumberRaw { green->value }), normalizeRGBComponentToSRGBAByte(NumberRaw { blue->value }), convertFloatAlphaTo<uint8_t>(*alpha) };
    case Component::Type::Percentage:
        return SRGBA<uint8_t> { normalizeRGBComponentToSRGBAByte(PercentRaw { red->value }), normalizeRGBComponentToSRGBAByte(PercentRaw { green->value }), normalizeRGBComponentToSRGBAByte(PercentRaw { blue->value }), convertFloatAlphaTo<uint8_t>(*alpha) };
    case Component::Type::Unknown:
        // The only way the resolvedComponentType can be Component::Type::Unknown is if all the components are "none", which is handled above.
        ASSERT_NOT_REACHED();
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
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

static Color colorByNormalizingHSLComponents(AngleOrNumberOrNoneRaw hue, PercentOrNoneRaw saturation, PercentOrNoneRaw lightness, double alpha, RGBOrHSLSeparatorSyntax syntax)
{
    auto normalizedHue = WTF::switchOn(hue,
        [] (AngleRaw angle) { return CSSPrimitiveValue::computeDegrees(angle.type, angle.value); },
        [] (NumberRaw number) { return number.value; },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto normalizedSaturation = WTF::switchOn(saturation,
        [] (PercentRaw percent) { return std::clamp(percent.value, 0.0, 100.0); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto normalizedLightness = WTF::switchOn(lightness,
        [] (PercentRaw percent) { return std::clamp(percent.value, 0.0, 100.0); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );

    if (std::isnan(normalizedHue) || std::isnan(normalizedSaturation) || std::isnan(normalizedLightness) || std::isnan(alpha)) {
        // "none" values are only allowed with the WhitespaceSlash syntax.
        if (syntax != RGBOrHSLSeparatorSyntax::WhitespaceSlash)
            return { };

        // If any component uses "none", we store the value as a HSLA<float> to allow for storage of the special value as NaN.
        return HSLA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedSaturation), static_cast<float>(normalizedLightness), static_cast<float>(alpha) };
    }

    if (normalizedHue < 0.0 || normalizedHue > 360.0) {
        // If hue is not in the [0, 360] range, we store the value as a HSLA<float> to allow for correct interpolation
        // using the "specified" hue interpolation method.
        return HSLA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedSaturation), static_cast<float>(normalizedLightness), static_cast<float>(alpha) };
    }

    // NOTE: The explicit conversion to SRGBA<uint8_t> is an intentional performance optimization that allows storing the
    // color with no extra allocation for an extended color object. This is permissible due to the historical requirement
    // that HSLA colors serialize using the legacy color syntax (rgb()/rgba()) and historically have used the 8-bit rgba
    // internal representation in engines.
    return convertColor<SRGBA<uint8_t>>(HSLA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedSaturation), static_cast<float>(normalizedLightness), static_cast<float>(alpha) });
}

static Color parseRelativeHSLParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColor(args, context);
    if (!originColor.isValid())
        return { };

    auto originColorAsHSL = originColor.toColorTypeLossy<HSLA<float>>().resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueH, CSSUnitType::CSS_DEG, originColorAsHSL.hue },
        { CSSValueS, CSSUnitType::CSS_PERCENTAGE, originColorAsHSL.saturation },
        { CSSValueL, CSSUnitType::CSS_PERCENTAGE, originColorAsHSL.lightness },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsHSL.alpha * 100.0 }
    };

    auto hue = consumeAngleOrNumberOrNoneRawAllowingSymbolTableIdent(args, symbolTable, context.mode);
    if (!hue)
        return { };

    auto saturation = consumePercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable);
    if (!saturation)
        return { };

    auto lightness = consumePercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable);
    if (!lightness)
        return { };

    auto alpha = consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    return colorByNormalizingHSLComponents(*hue, *saturation, *lightness, *alpha, RGBOrHSLSeparatorSyntax::WhitespaceSlash);
}

static Color parseNonRelativeHSLParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    auto hue = consumeAngleOrNumberOrNoneRaw(args, context.mode);
    if (!hue)
        return { };

    auto syntax = consumeCommaIncludingWhitespace(args) ? RGBOrHSLSeparatorSyntax::Commas : RGBOrHSLSeparatorSyntax::WhitespaceSlash;

    auto saturation = consumePercentOrNoneRaw(args);
    if (!saturation)
        return { };

    if (!consumeRGBOrHSLSeparator(args, syntax))
        return { };

    auto lightness = consumePercentOrNoneRaw(args);
    if (!lightness)
        return { };

    auto alpha = consumeRGBOrHSLOptionalAlpha(args, syntax);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    return colorByNormalizingHSLComponents(*hue, *saturation, *lightness, *alpha, syntax);
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

    auto normalizedHue = WTF::switchOn(*hue,
        [] (AngleRaw angle) { return CSSPrimitiveValue::computeDegrees(angle.type, angle.value); },
        [] (NumberRaw number) { return number.value; },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto clampedWhiteness = WTF::switchOn(*whiteness,
        [] (PercentRaw percent) { return std::clamp(percent.value, 0.0, 100.0); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto clampedBlackness = WTF::switchOn(*blackness,
        [] (PercentRaw percent) { return std::clamp(percent.value, 0.0, 100.0); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );

    if (std::isnan(normalizedHue) || std::isnan(clampedWhiteness) || std::isnan(clampedBlackness) || std::isnan(*alpha)) {
        auto [normalizedWhitness, normalizedBlackness] = normalizeClampedWhitenessBlacknessAllowingNone(clampedWhiteness, clampedBlackness);

        // If any component uses "none", we store the value as a HWBA<float> to allow for storage of the special value as NaN.
        return HWBA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedWhitness), static_cast<float>(normalizedBlackness), static_cast<float>(*alpha) };
    }

    auto [normalizedWhitness, normalizedBlackness] = normalizeClampedWhitenessBlacknessDisallowingNone(clampedWhiteness, clampedBlackness);

    if (normalizedHue < 0.0 || normalizedHue > 360.0) {
        // If 'hue' is not in the [0, 360] range, we store the value as a HWBA<float> to allow for correct interpolation
        // using the "specified" hue interpolation method.
        return HWBA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedWhitness), static_cast<float>(normalizedBlackness), static_cast<float>(*alpha) };
    }

    // NOTE: The explicit conversion to SRGBA<uint8_t> is an intentional performance optimization that allows storing the
    // color with no extra allocation for an extended color object. This is permissible due to the historical requirement
    // that HWBA colors serialize using the legacy color syntax (rgb()/rgba()) and historically have used the 8-bit rgba
    // internal representation in engines.
    return convertColor<SRGBA<uint8_t>>(HWBA<float> { static_cast<float>(normalizedHue), static_cast<float>(normalizedWhitness), static_cast<float>(normalizedBlackness), static_cast<float>(*alpha) });
}

static Color parseRelativeHWBParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColor(args, context);
    if (!originColor.isValid())
        return { };

    auto originColorAsHWB = originColor.toColorTypeLossy<HWBA<float>>().resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueH, CSSUnitType::CSS_DEG, originColorAsHWB.hue },
        { CSSValueW, CSSUnitType::CSS_PERCENTAGE, originColorAsHWB.whiteness },
        { CSSValueB, CSSUnitType::CSS_PERCENTAGE, originColorAsHWB.blackness },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsHWB.alpha * 100.0 }
    };

    auto hueConsumer = [&symbolTable, &context](auto& args) { return consumeAngleOrNumberOrNoneRawAllowingSymbolTableIdent(args, symbolTable, context.mode); };
    auto whitenessAndBlacknessConsumer = [&symbolTable](auto& args) { return consumePercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable); };
    auto alphaConsumer = [&symbolTable](auto& args) { return consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable); };

    return parseHWBParameters(args, WTFMove(hueConsumer), WTFMove(whitenessAndBlacknessConsumer), WTFMove(alphaConsumer));
}

static Color parseNonRelativeHWBParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    auto hueConsumer = [&context](auto& args) { return consumeAngleOrNumberOrNoneRaw(args, context.mode); };
    auto whitenessAndBlacknessConsumer = [](auto& args) { return consumePercentOrNoneRaw(args); };
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

// The NormalizePercentage structs are specialized for the color types
// that have specified rules for normalizing percentages by component.
// The structs contains static members for the component that describe
// the normalized value when the percent is 100%. As all treat 0% as
// normalizing to 0, that is not encoded in the struct.
template<typename ColorType> struct NormalizePercentage;

template<> struct NormalizePercentage<Lab<float>> {
    //  for L: 0% = 0.0, 100% = 100.0
    //  for a and b: -100% = -125, 100% = 125 (NOTE: 0% is 0)

    static constexpr double lightnessScaleFactor = 100.0 / 100.0;
    static constexpr double abScaleFactor = 125.0 / 100.0;
};

template<> struct NormalizePercentage<OKLab<float>> {
    //  for L: 0% = 0.0, 100% = 1.0
    //  for a and b: -100% = -0.4, 100% = 0.4 (NOTE: 0% is 0)

    static constexpr double lightnessScaleFactor = 1.0 / 100.0;
    static constexpr double abScaleFactor = 0.4 / 100.0;
};

template<> struct NormalizePercentage<LCHA<float>> {
    //  for L: 0% = 0.0, 100% = 100.0
    //  for C: 0% = 0, 100% = 150

    static constexpr double lightnessScaleFactor = 100.0 / 100.0;
    static constexpr double chromaScaleFactor = 150.0 / 100.0;
};

template<> struct NormalizePercentage<OKLCHA<float>> {
    //  for L: 0% = 0.0, 100% = 1.0
    //  for C: 0% = 0.0 100% = 0.4

    static constexpr double lightnessScaleFactor = 1.0 / 100.0;
    static constexpr double chromaScaleFactor = 0.4 / 100.0;
};

template<> struct NormalizePercentage<XYZA<float, WhitePoint::D50>> {
    //  for X,Y,Z: 0% = 0.0, 100% = 1.0

    static constexpr double xyzScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<XYZA<float, WhitePoint::D65>> {
    //  for X,Y,Z: 0% = 0.0, 100% = 1.0

    static constexpr double xyzScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<ExtendedA98RGB<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<ExtendedDisplayP3<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<ExtendedProPhotoRGB<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<ExtendedRec2020<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<ExtendedSRGBA<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<> struct NormalizePercentage<ExtendedLinearSRGBA<float>> {
    //  for R,G,B: 0% = 0.0, 100% = 1.0

    static constexpr double rgbScaleFactor = 1.0 / 100.0;
};

template<typename ColorType>
static double normalizeLightnessPercent(double percent)
{
    return NormalizePercentage<ColorType>::lightnessScaleFactor * percent;
}

template<typename ColorType>
static double normalizeABPercent(double percent)
{
    return NormalizePercentage<ColorType>::abScaleFactor * percent;
}

template<typename ColorType>
static double normalizeChromaPercent(double percent)
{
    return NormalizePercentage<ColorType>::chromaScaleFactor * percent;
}

template<typename ColorType>
static double normalizeXYZPercent(double percent)
{
    return NormalizePercentage<ColorType>::xyzScaleFactor * percent;
}

template<typename ColorType>
static double normalizeRGBPercent(double percent)
{
    return NormalizePercentage<ColorType>::rgbScaleFactor * percent;
}

template<typename ColorType, typename ConsumerForLightness, typename ConsumerForAB, typename ConsumerForAlpha>
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

    auto normalizedLightness = WTF::switchOn(*lightness,
        [] (NumberRaw number) { return std::max(0.0, number.value); },
        [] (PercentRaw percent) { return std::max(0.0, normalizeLightnessPercent<ColorType>(percent.value)); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto normalizedA = WTF::switchOn(*aValue,
        [] (NumberRaw number) { return number.value; },
        [] (PercentRaw percent) { return normalizeABPercent<ColorType>(percent.value); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto normalizedB = WTF::switchOn(*bValue,
        [] (NumberRaw number) { return number.value; },
        [] (PercentRaw percent) { return normalizeABPercent<ColorType>(percent.value); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );

    return ColorType { static_cast<float>(normalizedLightness), static_cast<float>(normalizedA), static_cast<float>(normalizedB), static_cast<float>(*alpha) };
}

template<typename ColorType>
static Color parseRelativeLabParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColor(args, context);
    if (!originColor.isValid())
        return { };

    auto originColorAsLab = originColor.toColorTypeLossy<ColorType>().resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueL, CSSUnitType::CSS_NUMBER, originColorAsLab.lightness },
        { CSSValueA, CSSUnitType::CSS_NUMBER, originColorAsLab.a },
        { CSSValueB, CSSUnitType::CSS_NUMBER, originColorAsLab.b },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsLab.alpha * 100.0 }
    };

    auto lightnessConsumer = [&symbolTable](auto& args) { return consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable); };
    auto abConsumer = [&symbolTable](auto& args) { return consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable); };
    auto alphaConsumer = [&symbolTable](auto& args) { return consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable); };

    return parseLabParameters<ColorType>(args, WTFMove(lightnessConsumer), WTFMove(abConsumer), WTFMove(alphaConsumer));
}

template<typename ColorType>
static Color parseNonRelativeLabParameters(CSSParserTokenRange& args)
{
    auto lightnessConsumer = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto abConsumer = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto alphaConsumer = [](auto& args) { return consumeOptionalAlpha(args); };

    return parseLabParameters<ColorType>(args, WTFMove(lightnessConsumer), WTFMove(abConsumer), WTFMove(alphaConsumer));
}

template<typename ColorType>
static Color parseLabParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueLab || range.peek().functionId() == CSSValueOklab);

    if (!context.cssColor4)
        return { };

    auto args = consumeFunction(range);

    if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
        return parseRelativeLabParameters<ColorType>(args, context);
    return parseNonRelativeLabParameters<ColorType>(args);
}

template<typename ColorType, typename ConsumerForLightness, typename ConsumerForChroma, typename ConsumerForHue, typename ConsumerForAlpha>
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

    auto normalizedLightness = WTF::switchOn(*lightness,
        [] (NumberRaw number) { return std::max(0.0, number.value); },
        [] (PercentRaw percent) { return std::max(0.0, normalizeLightnessPercent<ColorType>(percent.value)); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto normalizedChroma = WTF::switchOn(*chroma,
        [] (NumberRaw number) { return std::max(0.0, number.value); },
        [] (PercentRaw percent) { return std::max(0.0, normalizeChromaPercent<ColorType>(percent.value)); },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );
    auto normalizedHue = WTF::switchOn(*hue,
        [] (AngleRaw angle) { return CSSPrimitiveValue::computeDegrees(angle.type, angle.value); },
        [] (NumberRaw number) { return number.value; },
        [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
    );

    return ColorType { static_cast<float>(normalizedLightness), static_cast<float>(normalizedChroma), static_cast<float>(normalizedHue), static_cast<float>(*alpha) };
}

template<typename ColorType>
static Color parseRelativeLCHParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColor(args, context);
    if (!originColor.isValid())
        return { };

    auto originColorAsLCH = originColor.toColorTypeLossy<ColorType>().resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueL, CSSUnitType::CSS_NUMBER, originColorAsLCH.lightness },
        { CSSValueC, CSSUnitType::CSS_NUMBER, originColorAsLCH.chroma },
        { CSSValueH, CSSUnitType::CSS_DEG, originColorAsLCH.hue },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsLCH.alpha * 100.0 }
    };

    auto lightnessConsumer = [&symbolTable](auto& args) { return consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable); };
    auto chromaConsumer = [&symbolTable](auto& args) { return consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable); };
    auto hueConsumer = [&symbolTable, &context](auto& args) { return consumeAngleOrNumberOrNoneRawAllowingSymbolTableIdent(args, symbolTable, context.mode); };
    auto alphaConsumer = [&symbolTable](auto& args) { return consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable); };

    return parseLCHParameters<ColorType>(args, WTFMove(lightnessConsumer), WTFMove(chromaConsumer), WTFMove(hueConsumer), WTFMove(alphaConsumer));
}

template<typename ColorType>
static Color parseNonRelativeLCHParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    auto lightnessConsumer = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto chromaConsumer = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto hueConsumer = [&context](auto& args) { return consumeAngleOrNumberOrNoneRaw(args, context.mode); };
    auto alphaConsumer = [](auto& args) { return consumeOptionalAlpha(args); };

    return parseLCHParameters<ColorType>(args, WTFMove(lightnessConsumer), WTFMove(chromaConsumer), WTFMove(hueConsumer), WTFMove(alphaConsumer));
}

template<typename ColorType>
static Color parseLCHParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueLch || range.peek().functionId() == CSSValueOklch);

    if (!context.cssColor4)
        return { };

    auto args = consumeFunction(range);

    if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
        return parseRelativeLCHParameters<ColorType>(args, context);
    return parseNonRelativeLCHParameters<ColorType>(args, context);
}

template<typename ColorType, typename ConsumerForRGB, typename ConsumerForAlpha>
static Color parseColorFunctionForRGBTypes(CSSParserTokenRange& args, ConsumerForRGB&& rgbConsumer, ConsumerForAlpha&& alphaConsumer)
{
    double channels[3] = { 0, 0, 0 };
    for (auto& channel : channels) {
        auto value = rgbConsumer(args);
        if (!value)
            break;

        channel = WTF::switchOn(*value,
            [] (NumberRaw number) { return number.value; },
            [] (PercentRaw percent) { return normalizeRGBPercent<ColorType>(percent.value); },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
    }

    auto alpha = alphaConsumer(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    return { ColorType { static_cast<float>(channels[0]), static_cast<float>(channels[1]), static_cast<float>(channels[2]), static_cast<float>(*alpha) }, Color::Flags::UseColorFunctionSerialization };
}

template<typename ColorType> static Color parseRelativeColorFunctionForRGBTypes(CSSParserTokenRange& args, Color originColor, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueDisplayP3 || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020 || args.peek().id() == CSSValueSRGB || args.peek().id() == CSSValueSrgbLinear);

    // Support sRGB and Display-P3 regardless of the setting as we have shipped support for them for a while.
    if (!context.cssColor4 && (args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020 || args.peek().id() == CSSValueSrgbLinear))
        return { };

    consumeIdentRaw(args);

    auto originColorAsColorType = originColor.toColorTypeLossy<ColorType>().resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueR, CSSUnitType::CSS_PERCENTAGE, originColorAsColorType.red * 100.0 },
        { CSSValueG, CSSUnitType::CSS_PERCENTAGE, originColorAsColorType.green * 100.0 },
        { CSSValueB, CSSUnitType::CSS_PERCENTAGE, originColorAsColorType.blue * 100.0 },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsColorType.alpha * 100.0 }
    };

    auto consumeRGB = [&symbolTable](auto& args) { return consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable, ValueRange::All); };
    auto consumeAlpha = [&symbolTable](auto& args) { return consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable); };

    return parseColorFunctionForRGBTypes<ColorType>(args, WTFMove(consumeRGB), WTFMove(consumeAlpha));
}

template<typename ColorType> static Color parseColorFunctionForRGBTypes(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueDisplayP3 || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020 || args.peek().id() == CSSValueSRGB || args.peek().id() == CSSValueSrgbLinear);

    // Support sRGB and Display-P3 regardless of the setting as we have shipped support for them for a while.
    if (!context.cssColor4 && (args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020 || args.peek().id() == CSSValueSrgbLinear))
        return { };

    consumeIdentRaw(args);

    auto consumeRGB = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto consumeAlpha = [](auto& args) { return consumeOptionalAlpha(args); };

    return parseColorFunctionForRGBTypes<ColorType>(args, WTFMove(consumeRGB), WTFMove(consumeAlpha));
}

template<typename ColorType, typename ConsumerForXYZ, typename ConsumerForAlpha>
static Color parseColorFunctionForXYZTypes(CSSParserTokenRange& args, ConsumerForXYZ&& xyzConsumer, ConsumerForAlpha&& alphaConsumer)
{
    double channels[3] = { 0, 0, 0 };
    for (auto& channel : channels) {
        auto value = xyzConsumer(args);
        if (!value)
            break;

        channel = WTF::switchOn(*value,
            [] (NumberRaw number) { return number.value; },
            [] (PercentRaw percent) { return normalizeXYZPercent<ColorType>(percent.value); },
            [] (NoneRaw) { return std::numeric_limits<double>::quiet_NaN(); }
        );
    }

    auto alpha = alphaConsumer(args);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    return { ColorType { static_cast<float>(channels[0]), static_cast<float>(channels[1]), static_cast<float>(channels[2]), static_cast<float>(*alpha) }, Color::Flags::UseColorFunctionSerialization };
}

template<typename ColorType> static Color parseRelativeColorFunctionForXYZTypes(CSSParserTokenRange& args, Color originColor, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueXyz || args.peek().id() == CSSValueXyzD50 || args.peek().id() == CSSValueXyzD65);

    if (!context.cssColor4)
        return { };

    consumeIdentRaw(args);

    auto originColorAsXYZ = originColor.toColorTypeLossy<ColorType>().resolved();

    CSSCalcSymbolTable symbolTable {
        { CSSValueX, CSSUnitType::CSS_NUMBER, originColorAsXYZ.x },
        { CSSValueY, CSSUnitType::CSS_NUMBER, originColorAsXYZ.y },
        { CSSValueZ, CSSUnitType::CSS_NUMBER, originColorAsXYZ.z },
        { CSSValueAlpha, CSSUnitType::CSS_PERCENTAGE, originColorAsXYZ.alpha * 100.0 }
    };

    auto consumeXYZ = [&symbolTable](auto& args) { return consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(args, symbolTable); };
    auto consumeAlpha = [&symbolTable](auto& args) { return consumeOptionalAlphaAllowingSymbolTableIdent(args, symbolTable); };

    return parseColorFunctionForXYZTypes<ColorType>(args, WTFMove(consumeXYZ), WTFMove(consumeAlpha));
}

template<typename ColorType> static Color parseColorFunctionForXYZTypes(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueXyz || args.peek().id() == CSSValueXyzD50 || args.peek().id() == CSSValueXyzD65);

    if (!context.cssColor4)
        return { };

    consumeIdentRaw(args);

    auto consumeXYZ = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto consumeAlpha = [](auto& args) { return consumeOptionalAlpha(args); };

    return parseColorFunctionForXYZTypes<ColorType>(args, WTFMove(consumeXYZ), WTFMove(consumeAlpha));
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
        return parseRelativeColorFunctionForRGBTypes<ExtendedA98RGB<float>>(args, WTFMove(originColor), context);
    case CSSValueDisplayP3:
        return parseRelativeColorFunctionForRGBTypes<ExtendedDisplayP3<float>>(args, WTFMove(originColor), context);
    case CSSValueProphotoRgb:
        return parseRelativeColorFunctionForRGBTypes<ExtendedProPhotoRGB<float>>(args, WTFMove(originColor), context);
    case CSSValueRec2020:
        return parseRelativeColorFunctionForRGBTypes<ExtendedRec2020<float>>(args, WTFMove(originColor), context);
    case CSSValueSRGB:
        return parseRelativeColorFunctionForRGBTypes<ExtendedSRGBA<float>>(args, WTFMove(originColor), context);
    case CSSValueSrgbLinear:
        return parseRelativeColorFunctionForRGBTypes<ExtendedLinearSRGBA<float>>(args, WTFMove(originColor), context);
    case CSSValueXyzD50:
        return parseRelativeColorFunctionForXYZTypes<XYZA<float, WhitePoint::D50>>(args, WTFMove(originColor), context);
    case CSSValueXyz:
    case CSSValueXyzD65:
        return parseRelativeColorFunctionForXYZTypes<XYZA<float, WhitePoint::D65>>(args, WTFMove(originColor), context);
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
        return parseColorFunctionForRGBTypes<ExtendedA98RGB<float>>(args, context);
    case CSSValueDisplayP3:
        return parseColorFunctionForRGBTypes<ExtendedDisplayP3<float>>(args, context);
    case CSSValueProphotoRgb:
        return parseColorFunctionForRGBTypes<ExtendedProPhotoRGB<float>>(args, context);
    case CSSValueRec2020:
        return parseColorFunctionForRGBTypes<ExtendedRec2020<float>>(args, context);
    case CSSValueSRGB:
        return parseColorFunctionForRGBTypes<ExtendedSRGBA<float>>(args, context);
    case CSSValueSrgbLinear:
        return parseColorFunctionForRGBTypes<ExtendedLinearSRGBA<float>>(args, context);
    case CSSValueXyzD50:
        return parseColorFunctionForXYZTypes<XYZA<float, WhitePoint::D50>>(args, context);
    case CSSValueXyz:
    case CSSValueXyzD65:
        return parseColorFunctionForXYZTypes<XYZA<float, WhitePoint::D65>>(args, context);
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

    // Handle the invalid case where there is only one color in the "compare against" color list.
    if (colorsToCompareAgainst.size() == 1)
        return { };

    if (consumedTo) {
        auto targetContrast = [&] () -> std::optional<NumberRaw> {
            if (args.peek().type() == IdentToken) {
                static constexpr std::pair<CSSValueID, NumberRaw> targetContrastMappings[] {
                    { CSSValueAA, NumberRaw { 4.5 } },
                    { CSSValueAALarge, NumberRaw { 3.0 } },
                    { CSSValueAAA, NumberRaw { 7.0 } },
                    { CSSValueAAALarge, NumberRaw { 4.5 } },
                };
                static constexpr SortedArrayMap targetContrastMap { targetContrastMappings };
                auto value = targetContrastMap.tryGet(args.consumeIncludingWhitespace().id());
                return value ? std::make_optional(*value) : std::nullopt;
            }
            return consumeNumberRaw(args);
        }();

        if (!targetContrast)
            return { };

        // Handle the invalid case where there are additional tokens after the target contrast.
        if (!args.atEnd())
            return { };

        // When a target constast is specified, we select "the first color color to meet or exceed the target contrast."
        return selectFirstColorThatMeetsOrExceedsTargetContrast(originBackgroundColor, WTFMove(colorsToCompareAgainst), targetContrast->value);
    }

    // Handle the invalid case where there are additional tokens after the "compare against" color list that are not the 'to' identifier.
    if (!args.atEnd())
        return { };

    // When a target constast is NOT specified, we select "the first color with the highest contrast to the single color."
    return selectFirstColorWithHighestContrast(originBackgroundColor, WTFMove(colorsToCompareAgainst));
}

static std::optional<HueInterpolationMethod> consumeHueInterpolationMethod(CSSParserTokenRange& args)
{
    switch (args.peek().id()) {
    case CSSValueShorter:
        args.consumeIncludingWhitespace();
        return HueInterpolationMethod::Shorter;
    case CSSValueLonger:
        args.consumeIncludingWhitespace();
        return HueInterpolationMethod::Longer;
    case CSSValueIncreasing:
        args.consumeIncludingWhitespace();
        return HueInterpolationMethod::Increasing;
    case CSSValueDecreasing:
        args.consumeIncludingWhitespace();
        return HueInterpolationMethod::Decreasing;
    case CSSValueSpecified:
        args.consumeIncludingWhitespace();
        return HueInterpolationMethod::Specified;
    default:
        return { };
    }
}

static std::optional<ColorInterpolationMethod> consumeColorInterpolationMethod(CSSParserTokenRange& args)
{
    // <rectangular-color-space> = srgb | srgb-linear | lab | oklab | xyz | xyz-d50 | xyz-d65
    // <polar-color-space> = hsl | hwb | lch | oklch
    // <hue-interpolation-method> = [ shorter | longer | increasing | decreasing | specified ] hue
    // <color-interpolation-method> = in [ <rectangular-color-space> | <polar-color-space> <hue-interpolation-method>? ]

    ASSERT(args.peek().id() == CSSValueIn);
    consumeIdentRaw(args);

    auto consumePolarColorSpace = [](CSSParserTokenRange& args, auto colorInterpolationMethod) -> std::optional<ColorInterpolationMethod> {
        // Consume the color space identifier.
        args.consumeIncludingWhitespace();

        // <hue-interpolation-method> is optional, so if it is not provided, we just use the default value
        // specified in the passed in 'colorInterpolationMethod' parameter.
        auto hueInterpolationMethod = consumeHueInterpolationMethod(args);
        if (!hueInterpolationMethod)
            return {{ colorInterpolationMethod, AlphaPremultiplication::Premultiplied }};
        
        // If the hue-interpolation-method was provided it must be followed immediately by the 'hue' identifier.
        if (!consumeIdentRaw<CSSValueHue>(args))
            return { };

        colorInterpolationMethod.hueInterpolationMethod = *hueInterpolationMethod;

        return {{ colorInterpolationMethod, AlphaPremultiplication::Premultiplied }};
    };

    auto consumeRectangularColorSpace = [](CSSParserTokenRange& args, auto colorInterpolationMethod) -> std::optional<ColorInterpolationMethod> {
        // Consume the color space identifier.
        args.consumeIncludingWhitespace();

        return {{ colorInterpolationMethod, AlphaPremultiplication::Premultiplied }};
    };

    switch (args.peek().id()) {
    case CSSValueHsl:
        return consumePolarColorSpace(args, ColorInterpolationMethod::HSL { });
    case CSSValueHwb:
        return consumePolarColorSpace(args, ColorInterpolationMethod::HWB { });
    case CSSValueLch:
        return consumePolarColorSpace(args, ColorInterpolationMethod::LCH { });
    case CSSValueLab:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::Lab { });
    case CSSValueOklch:
        return consumePolarColorSpace(args, ColorInterpolationMethod::OKLCH { });
    case CSSValueOklab:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::OKLab { });
    case CSSValueSRGB:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::SRGB { });
    case CSSValueSrgbLinear:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::SRGBLinear { });
    case CSSValueXyzD50:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::XYZD50 { });
    case CSSValueXyz:
    case CSSValueXyzD65:
        return consumeRectangularColorSpace(args, ColorInterpolationMethod::XYZD65 { });
    default:
        return { };
    }
}

struct ColorMixComponent {
    Color color;
    std::optional<double> percentage;
};

static std::optional<ColorMixComponent> consumeColorMixComponent(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ColorMixComponent result;

    if (auto percentage = consumePercentRaw(args)) {
        if (percentage->value < 0.0 || percentage->value > 100.0)
            return { };
        result.percentage = percentage->value;
    }

    result.color = consumeOriginColor(args, context);
    if (!result.color.isValid())
        return std::nullopt;

    if (!result.percentage) {
        if (auto percentage = consumePercentRaw(args)) {
            if (percentage->value < 0.0 || percentage->value > 100.0)
                return { };
            result.percentage = percentage->value;
        }
    }

    return result;
}

struct ColorMixPercentages {
    double p1;
    double p2;
    std::optional<double> alphaMultiplier = std::nullopt;
};

static std::optional<ColorMixPercentages> normalizedMixPercentages(const ColorMixComponent& mixComponents1, const ColorMixComponent& mixComponents2)
{
    // The percentages are normalized as follows:

    // 1. Let p1 be the first percentage and p2 the second one.

    // 2. If both percentages are omitted, they each default to 50% (an equal mix of the two colors).
    if (!mixComponents1.percentage && !mixComponents2.percentage)
        return {{ 50.0, 50.0 }};
    
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

    // 5.If the percentages sum to zero, the function is invalid.
    if (sum == 0)
        return { };

    if (sum > 100.0) {
        // 6. Otherwise, if both are provided but do not add up to 100%, they are scaled accordingly so that they
        //    add up to 100%.
        result.p1 *= 100.0 / sum;
        result.p2 *= 100.0 / sum;
    } else if (sum < 100.0) {
        // 7. Otherwise, if both are provided and add up to less than 100%, the sum is saved as an alpha multiplier.
        //    They are then scaled accordingly so that they add up to 100%.
        result.p1 *= 100.0 / sum;
        result.p2 *= 100.0 / sum;
        result.alphaMultiplier = sum;
    }

    return result;
}

template<typename InterpolationMethod> static Color mixColorComponentsUsingColorInterpolationMethod(InterpolationMethod interpolationMethod, ColorMixPercentages mixPercentages, const Color& color1, const Color& color2)
{
    using ColorType = typename InterpolationMethod::ColorType;

    // 1. Both colors are converted to the specified <color-space>. If the specified color space has a smaller gamut than
    //    the one in which the color to be adjusted is specified, gamut mapping will occur.
    auto convertedColor1 = color1.template toColorTypeLossy<ColorType>();
    auto convertedColor2 = color2.template toColorTypeLossy<ColorType>();

    // 2. Colors are then interpolated in the specified color space, as described in CSS Color 4 § 13 Interpolation. [...]
    auto mixedColor = interpolateColorComponents<AlphaPremultiplication::Premultiplied>(interpolationMethod, convertedColor1, mixPercentages.p1 / 100.0, convertedColor2, mixPercentages.p2 / 100.0).unresolved();

    // 3. If an alpha multiplier was produced during percentage normalization, the alpha component of the interpolated result
    //    is multiplied by the alpha multiplier.
    if (mixPercentages.alphaMultiplier && !std::isnan(mixedColor.alpha))
        mixedColor.alpha *= (*mixPercentages.alphaMultiplier / 100.0);

    return makeCanonicalColor(mixedColor);
}

static Color mixColorComponents(ColorInterpolationMethod colorInterpolationMethod, const ColorMixComponent& mixComponents1, const ColorMixComponent& mixComponents2)
{
    auto mixPercentages = normalizedMixPercentages(mixComponents1, mixComponents2);
    if (!mixPercentages)
        return { };

    return WTF::switchOn(colorInterpolationMethod.colorSpace,
        [&] (auto colorSpace) {
            return mixColorComponentsUsingColorInterpolationMethod<decltype(colorSpace)>(colorSpace, *mixPercentages, mixComponents1.color, mixComponents2.color);
        }
    );
}

static Color parseColorMixFunctionParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // color-mix() = color-mix( <color-interpolation-method> , [ <color> && <percentage [0,100]>? ]#{2})

    ASSERT(range.peek().functionId() == CSSValueColorMix);

    if (!context.colorMixEnabled)
        return { };

    auto args = consumeFunction(range);
    
    if (args.peek().id() != CSSValueIn)
        return { };
    
    auto colorInterpolationMethod = consumeColorInterpolationMethod(args);
    if (!colorInterpolationMethod)
        return { };

    if (!consumeCommaIncludingWhitespace(args))
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

    return mixColorComponents(*colorInterpolationMethod, *mixComponent1, *mixComponent2);
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
        color = parseLabParameters<Lab<float>>(colorRange, context);
        break;
    case CSSValueLch:
        color = parseLCHParameters<LCHA<float>>(colorRange, context);
        break;
    case CSSValueOklab:
        color = parseLabParameters<OKLab<float>>(colorRange, context);
        break;
    case CSSValueOklch:
        color = parseLCHParameters<OKLCHA<float>>(colorRange, context);
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
        if (StyleColor::isSystemColorKeyword(keyword))
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

RefPtr<CSSPrimitiveValue> consumeColor(CSSParserTokenRange& range, const CSSParserContext& context, bool acceptQuirkyColors, OptionSet<StyleColor::CSSColorType> allowedColorTypes)
{
    auto keyword = range.peek().id();
    if (StyleColor::isColorKeyword(keyword, allowedColorTypes)) {
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

static RefPtr<CSSPrimitiveValue> consumePositionComponent(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, NegativePercentagePolicy negativePercentagePolicy = NegativePercentagePolicy::Forbid)
{
    if (range.peek().type() == IdentToken)
        return consumeIdent<CSSValueLeft, CSSValueTop, CSSValueBottom, CSSValueRight, CSSValueCenter>(range);
    return consumeLengthOrPercent(range, parserMode, ValueRange::All, unitless, negativePercentagePolicy);
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

// FIXME: Merge this with the identical function in CSSPropertyParser.cpp.
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
std::optional<PositionCoordinates> consumePositionCoordinates(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, PositionSyntax positionSyntax, NegativePercentagePolicy negativePercentagePolicy)
{
    auto value1 = consumePositionComponent(range, parserMode, unitless, negativePercentagePolicy);
    if (!value1)
        return std::nullopt;

    auto value2 = consumePositionComponent(range, parserMode, unitless, negativePercentagePolicy);
    if (!value2)
        return positionFromOneValue(*value1);
    
    auto value3 = consumePositionComponent(range, parserMode, unitless, negativePercentagePolicy);
    if (!value3)
        return positionFromTwoValues(*value1, *value2);
    
    auto value4 = consumePositionComponent(range, parserMode, unitless, negativePercentagePolicy);
    
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

RefPtr<CSSPrimitiveValue> consumeSingleAxisPosition(CSSParserTokenRange& range, CSSParserMode parserMode, BoxOrient orientation)
{
    RefPtr<CSSPrimitiveValue> value1;

    if (range.peek().type() == IdentToken) {
        switch (orientation) {
        case BoxOrient::Horizontal:
            value1 = consumeIdent<CSSValueLeft, CSSValueRight, CSSValueCenter>(range);
            break;
        case BoxOrient::Vertical:
            value1 = consumeIdent<CSSValueTop, CSSValueBottom, CSSValueCenter>(range);
            break;
        }
        if (!value1)
            return nullptr;

        if (value1->valueID() == CSSValueCenter)
            return value1;
    }

    auto value2 = consumeLengthOrPercent(range, parserMode, ValueRange::All, UnitlessQuirk::Forbid);
    if (value1 && value2)
        return CSSPropertyParserHelpersInternal::createPrimitiveValuePair(WTFMove(value1), WTFMove(value2));

    return value1 ? value1 : value2;
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
        auto value = consumeNumberOrPercentRaw<NumberOrPercentDividedBy100Transformer>(args);
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

static AlphaPremultiplication gradientAlphaPremultiplication(const CSSParserContext& context)
{
    return context.gradientPremultipliedAlphaInterpolationEnabled ? AlphaPremultiplication::Premultiplied : AlphaPremultiplication::Unpremultiplied;
}

static RefPtr<CSSValue> consumeDeprecatedGradient(CSSParserTokenRange& args, const CSSParserContext& context)
{
    auto id = args.consumeIncludingWhitespace().id();
    if (id != CSSValueRadial && id != CSSValueLinear)
        return nullptr;
    
    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto firstX = consumeDeprecatedGradientPoint(args, true);
    if (!firstX)
        return nullptr;
    auto firstY = consumeDeprecatedGradientPoint(args, false);
    if (!firstY)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    // For radial gradients only, we now expect a numeric radius.
    RefPtr<CSSPrimitiveValue> firstRadius;
    if (id == CSSValueRadial) {
        firstRadius = consumeNumber(args, ValueRange::NonNegative);
        if (!firstRadius || !consumeCommaIncludingWhitespace(args))
            return nullptr;
    }

    auto secondX = consumeDeprecatedGradientPoint(args, true);
    if (!secondX)
        return nullptr;
    auto secondY = consumeDeprecatedGradientPoint(args, false);
    if (!secondY)
        return nullptr;

    // For radial gradients only, we now expect the second radius.
    RefPtr<CSSPrimitiveValue> secondRadius;
    if (id == CSSValueRadial) {
        if (!consumeCommaIncludingWhitespace(args))
            return nullptr;
        secondRadius = consumeNumber(args, ValueRange::NonNegative);
        if (!secondRadius)
            return nullptr;
    }

    CSSGradientColorStopList stops;
    while (consumeCommaIncludingWhitespace(args)) {
        CSSGradientColorStop stop;
        if (!consumeDeprecatedGradientColorStop(args, stop, context))
            return nullptr;
        stops.append(WTFMove(stop));
    }
    stops.shrinkToFit();

    auto colorInterpolationMethod = CSSGradientColorInterpolationMethod::legacyMethod(gradientAlphaPremultiplication(context));

    RefPtr<CSSGradientValue> result;
    if (id == CSSValueRadial)
        result = CSSRadialGradientValue::create(NonRepeating, CSSDeprecatedRadialGradient, colorInterpolationMethod, WTFMove(stops));
    else if (id == CSSValueLinear)
        result = CSSLinearGradientValue::create(NonRepeating, CSSDeprecatedLinearGradient, colorInterpolationMethod, WTFMove(stops));

    result->setFirstX(WTFMove(firstX));
    result->setFirstY(WTFMove(firstY));
    result->setSecondX(WTFMove(secondX));
    result->setSecondY(WTFMove(secondY));
    if (id == CSSValueRadial) {
        downcast<CSSRadialGradientValue>(*result).setFirstRadius(WTFMove(firstRadius));
        downcast<CSSRadialGradientValue>(*result).setSecondRadius(WTFMove(secondRadius));
    }

    return result;
}

static std::optional<CSSGradientColorStopList> consumeGradientColorStops(CSSParserTokenRange& range, const CSSParserContext& context, CSSGradientType gradientType)
{
    bool supportsColorHints = gradientType == CSSLinearGradient || gradientType == CSSRadialGradient || gradientType == CSSConicGradient;
    
    auto consumeStopPosition = [&] {
        return gradientType == CSSConicGradient
            ? consumeAngleOrPercent(range, context.mode)
            : consumeLengthOrPercent(range, context.mode, ValueRange::All);
    };

    CSSGradientColorStopList stops;

    // The first color stop cannot be a color hint.
    bool previousStopWasColorHint = true;
    do {
        CSSGradientColorStop stop { consumeColor(range, context), consumeStopPosition() };
        if (!stop.color && !stop.position)
            return std::nullopt;

        // Two color hints in a row are not allowed.
        if (!stop.color && (!supportsColorHints || previousStopWasColorHint))
            return std::nullopt;
        previousStopWasColorHint = !stop.color;

        // Stops with both a color and a position can have a second position, which shares the same color.
        if (stop.color && stop.position) {
            if (auto secondPosition = consumeStopPosition()) {
                stops.append(stop);
                stop.position = WTFMove(secondPosition);
            }
        }
        stops.append(WTFMove(stop));
    } while (consumeCommaIncludingWhitespace(range));

    // The last color stop cannot be a color hint.
    if (previousStopWasColorHint)
        return std::nullopt;

    // Must have two or more stops to be valid.
    if (stops.size() < 2)
        return std::nullopt;

    stops.shrinkToFit();

    return { WTFMove(stops) };
}

static RefPtr<CSSValue> consumePrefixedRadialGradient(CSSParserTokenRange& args, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    auto centerCoordinate = consumeOneOrTwoValuedPositionCoordinates(args, context.mode, UnitlessQuirk::Forbid);
    if (centerCoordinate && !consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto shape = consumeIdent<CSSValueCircle, CSSValueEllipse>(args);
    auto sizeKeyword = consumeIdent<CSSValueClosestSide, CSSValueClosestCorner, CSSValueFarthestSide, CSSValueFarthestCorner, CSSValueContain, CSSValueCover>(args);
    if (!shape)
        shape = consumeIdent<CSSValueCircle, CSSValueEllipse>(args);

    // Or, two lengths or percentages
    RefPtr<CSSPrimitiveValue> horizontalSize;
    RefPtr<CSSPrimitiveValue> verticalSize;
    if (!shape && !sizeKeyword) {
        horizontalSize = consumeLengthOrPercent(args, context.mode, ValueRange::NonNegative);
        if (horizontalSize) {
            verticalSize = consumeLengthOrPercent(args, context.mode, ValueRange::NonNegative);
            if (!verticalSize)
                return nullptr;
            consumeCommaIncludingWhitespace(args);
        }
    } else
        consumeCommaIncludingWhitespace(args);

    auto stops = consumeGradientColorStops(args, context, CSSPrefixedRadialGradient);
    if (!stops)
        return nullptr;

    auto colorInterpolationMethod = CSSGradientColorInterpolationMethod::legacyMethod(gradientAlphaPremultiplication(context));
    auto result = CSSRadialGradientValue::create(repeating, CSSPrefixedRadialGradient, colorInterpolationMethod, WTFMove(*stops));

    result->setEndHorizontalSize(WTFMove(horizontalSize));
    result->setEndVerticalSize(WTFMove(verticalSize));

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

static CSSGradientColorInterpolationMethod computeGradientColorInterpolationMethod(const CSSParserContext& context, std::optional<ColorInterpolationMethod> parsedColorInterpolationMethod, const CSSGradientColorStopList& stops)
{
    if (!context.gradientInterpolationColorSpacesEnabled)
        return CSSGradientColorInterpolationMethod::legacyMethod(gradientAlphaPremultiplication(context));

    // We detect whether stops use legacy vs. non-legacy CSS color syntax using the following rules:
    //  - A CSSValueID is always considered legacy since all keyword based colors are considered legacy by the spec.
    //  - An actual Color value is considered legacy if it is stored as 8-bit sRGB.
    //
    // While this is accurate now, we should consider a more robust mechanism to detect this at parse
    // time, perhaps keeping this information in the CSSPrimitiveValue itself.

    auto defaultColorInterpolationMethod = CSSGradientColorInterpolationMethod::Default::SRGB;
    for (auto& stop : stops) {
        if (!stop.color)
            continue;
        if (stop.color->isValueID())
            continue;
        if (stop.color->isRGBColor() && stop.color->color().tryGetAsSRGBABytes())
            continue;

        defaultColorInterpolationMethod = CSSGradientColorInterpolationMethod::Default::OKLab;
        break;
    }

    if (parsedColorInterpolationMethod)
        return { *parsedColorInterpolationMethod, defaultColorInterpolationMethod };

    switch (defaultColorInterpolationMethod) {
    case CSSGradientColorInterpolationMethod::Default::SRGB:
        return { { ColorInterpolationMethod::SRGB { }, gradientAlphaPremultiplication(context) }, defaultColorInterpolationMethod };

    case CSSGradientColorInterpolationMethod::Default::OKLab:
        return { { ColorInterpolationMethod::OKLab { }, AlphaPremultiplication::Premultiplied }, defaultColorInterpolationMethod };
    }

    ASSERT_NOT_REACHED();
    return { { ColorInterpolationMethod::SRGB { }, gradientAlphaPremultiplication(context) }, defaultColorInterpolationMethod };
}

static RefPtr<CSSValue> consumeRadialGradient(CSSParserTokenRange& args, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    // radial-gradient() = radial-gradient(
    //   [[ <ending-shape> || <size> ]? [ at <position> ]? ] || <color-interpolation-method>,
    //   <color-stop-list>
    // )

    std::optional<ColorInterpolationMethod> colorInterpolationMethod;

    if (context.gradientInterpolationColorSpacesEnabled) {
        if (args.peek().id() == CSSValueIn) {
            colorInterpolationMethod = consumeColorInterpolationMethod(args);
            if (!colorInterpolationMethod)
                return nullptr;
        }
    }

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

    std::optional<PositionCoordinates> position;
    if (consumeIdent<CSSValueAt>(args)) {
        position = consumePositionCoordinates(args, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
        if (!position)
            return nullptr;
    }

    if (context.gradientInterpolationColorSpacesEnabled) {
        if ((shape || sizeKeyword || horizontalSize || position) && !colorInterpolationMethod && args.peek().id() == CSSValueIn) {
            colorInterpolationMethod = consumeColorInterpolationMethod(args);
            if (!colorInterpolationMethod)
                return nullptr;
        }
    }

    if ((shape || sizeKeyword || horizontalSize || position || colorInterpolationMethod) && !consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto stops = consumeGradientColorStops(args, context, CSSRadialGradient);
    if (!stops)
        return nullptr;

    auto computedColorInterpolationMethod = computeGradientColorInterpolationMethod(context, colorInterpolationMethod, *stops);
    auto result = CSSRadialGradientValue::create(repeating, CSSRadialGradient, computedColorInterpolationMethod, WTFMove(*stops));

    result->setShape(WTFMove(shape));
    result->setSizingBehavior(WTFMove(sizeKeyword));
    result->setEndHorizontalSize(WTFMove(horizontalSize));
    result->setEndVerticalSize(WTFMove(verticalSize));

    if (position) {
        result->setFirstX(position->x.copyRef());
        result->setFirstY(position->y.copyRef());
        
        // Right now, CSS radial gradients have the same start and end centers.
        result->setSecondX(WTFMove(position->x));
        result->setSecondY(WTFMove(position->y));
    }

    return result;
}

struct AngleOrToSideOrCorner {
    struct Angle {
        Ref<CSSPrimitiveValue> angle;
    };
    struct ToSideOrCorner {
        RefPtr<CSSPrimitiveValue> leftOrRight;
        RefPtr<CSSPrimitiveValue> topOrBottom;
    };
};

static RefPtr<CSSValue> consumePrefixedLinearGradient(CSSParserTokenRange& args, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    auto consumeToSideOrCorner = [](CSSParserTokenRange& args) -> std::optional<AngleOrToSideOrCorner::ToSideOrCorner> {
        auto leftOrRight = consumeIdent<CSSValueLeft, CSSValueRight>(args);
        auto topOrBottom = consumeIdent<CSSValueTop, CSSValueBottom>(args);
        if (!leftOrRight && !topOrBottom)
            return { };

        if (!leftOrRight)
            leftOrRight = consumeIdent<CSSValueLeft, CSSValueRight>(args);

        return {{ WTFMove(leftOrRight), WTFMove(topOrBottom) }};
    };

    std::optional<std::variant<AngleOrToSideOrCorner::Angle, AngleOrToSideOrCorner::ToSideOrCorner>> angleOrToSideOrCorner;

    if (auto angle = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow))
        angleOrToSideOrCorner = AngleOrToSideOrCorner::Angle { angle.releaseNonNull() };
    else if (auto sideOrCorner = consumeToSideOrCorner(args))
        angleOrToSideOrCorner = WTFMove(*sideOrCorner);

    if (angleOrToSideOrCorner && !consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto stops = consumeGradientColorStops(args, context, CSSPrefixedLinearGradient);
    if (!stops)
        return nullptr;

    auto colorInterpolationMethod = CSSGradientColorInterpolationMethod::legacyMethod(gradientAlphaPremultiplication(context));
    auto result = CSSLinearGradientValue::create(repeating, CSSPrefixedLinearGradient, colorInterpolationMethod, WTFMove(*stops));

    if (angleOrToSideOrCorner) {
        WTF::switchOn(*angleOrToSideOrCorner,
            [&] (AngleOrToSideOrCorner::Angle& angle) {
                result->setAngle(WTFMove(angle.angle));
            },
            [&] (AngleOrToSideOrCorner::ToSideOrCorner& toSideOrCorner) {
                result->setFirstX(WTFMove(toSideOrCorner.leftOrRight));
                result->setFirstY(WTFMove(toSideOrCorner.topOrBottom));
            }
        );
    } else
        result->setFirstY(CSSValuePool::singleton().createIdentifierValue(CSSValueTop));

    return result;
}

static RefPtr<CSSValue> consumeLinearGradient(CSSParserTokenRange& args, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    // <side-or-corner> = [left | right] || [top | bottom]
    // linear-gradient() = linear-gradient(
    //   [ <angle> | to <side-or-corner> ]? || <color-interpolation-method>,
    //   <color-stop-list>
    // )

    auto consumeToSideOrCorner = [](CSSParserTokenRange& args) -> std::optional<AngleOrToSideOrCorner::ToSideOrCorner> {
        ASSERT(args.peek().id() == CSSValueTo);
        consumeIdentRaw(args);

        auto leftOrRight = consumeIdent<CSSValueLeft, CSSValueRight>(args);
        auto topOrBottom = consumeIdent<CSSValueTop, CSSValueBottom>(args);
        if (!leftOrRight && !topOrBottom)
            return { };

        if (!leftOrRight)
            leftOrRight = consumeIdent<CSSValueLeft, CSSValueRight>(args);

        return {{ WTFMove(leftOrRight), WTFMove(topOrBottom) }};
    };


    std::optional<ColorInterpolationMethod> colorInterpolationMethod;

    if (context.gradientInterpolationColorSpacesEnabled) {
        if (args.peek().id() == CSSValueIn) {
            colorInterpolationMethod = consumeColorInterpolationMethod(args);
            if (!colorInterpolationMethod)
                return nullptr;
        }
    }

    std::optional<std::variant<AngleOrToSideOrCorner::Angle, AngleOrToSideOrCorner::ToSideOrCorner>> angleOrToSideOrCorner;

    if (auto angle = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow))
        angleOrToSideOrCorner = AngleOrToSideOrCorner::Angle { angle.releaseNonNull() };
    else if (args.peek().id() == CSSValueTo) {
        auto toSideOrCorner = consumeToSideOrCorner(args);
        if (!toSideOrCorner)
            return nullptr;
        angleOrToSideOrCorner = WTFMove(*toSideOrCorner);
    }

    if (context.gradientInterpolationColorSpacesEnabled) {
        if (angleOrToSideOrCorner && !colorInterpolationMethod && args.peek().id() == CSSValueIn) {
            colorInterpolationMethod = consumeColorInterpolationMethod(args);
            if (!colorInterpolationMethod)
                return nullptr;
        }
    }

    if (angleOrToSideOrCorner || colorInterpolationMethod) {
        if (!consumeCommaIncludingWhitespace(args))
            return nullptr;
    }

    auto stops = consumeGradientColorStops(args, context, CSSLinearGradient);
    if (!stops)
        return nullptr;

    auto computedColorInterpolationMethod = computeGradientColorInterpolationMethod(context, colorInterpolationMethod, *stops);
    auto result = CSSLinearGradientValue::create(repeating, CSSLinearGradient, computedColorInterpolationMethod, WTFMove(*stops));
    
    if (angleOrToSideOrCorner) {
        WTF::switchOn(*angleOrToSideOrCorner,
            [&] (AngleOrToSideOrCorner::Angle& angle) {
                result->setAngle(WTFMove(angle.angle));
            },
            [&] (AngleOrToSideOrCorner::ToSideOrCorner& toSideOrCorner) {
                result->setFirstX(WTFMove(toSideOrCorner.leftOrRight));
                result->setFirstY(WTFMove(toSideOrCorner.topOrBottom));
            }
        );
    }

    return result;
}

static RefPtr<CSSValue> consumeConicGradient(CSSParserTokenRange& args, const CSSParserContext& context, CSSGradientRepeat repeating)
{
#if ENABLE(CSS_CONIC_GRADIENTS)
    // conic-gradient() = conic-gradient(
    //   [ [ from <angle> ]? [ at <position> ]? ] || <color-interpolation-method>,
    //   <angular-color-stop-list>
    // )

    std::optional<ColorInterpolationMethod> colorInterpolationMethod;

    if (context.gradientInterpolationColorSpacesEnabled) {
        if (args.peek().id() == CSSValueIn) {
            colorInterpolationMethod = consumeColorInterpolationMethod(args);
            if (!colorInterpolationMethod)
                return nullptr;
        }
    }

    RefPtr<CSSPrimitiveValue> angle;
    if (consumeIdent<CSSValueFrom>(args)) {
        // FIXME: Unlike linear-gradient, conic-gradients are not specified to allow unitless 0 angles - https://www.w3.org/TR/css-images-4/#valdef-conic-gradient-angle.
        angle = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
        if (!angle)
            return nullptr;
    }
    
    std::optional<PositionCoordinates> position;
    if (consumeIdent<CSSValueAt>(args)) {
        position = consumePositionCoordinates(args, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
        if (!position)
            return nullptr;
    }

    if (context.gradientInterpolationColorSpacesEnabled) {
        if ((angle || position) && !colorInterpolationMethod && args.peek().id() == CSSValueIn) {
            colorInterpolationMethod = consumeColorInterpolationMethod(args);
            if (!colorInterpolationMethod)
                return nullptr;
        }
    }

    if (angle || position || colorInterpolationMethod) {
        if (!consumeCommaIncludingWhitespace(args))
            return nullptr;
    }

    auto stops = consumeGradientColorStops(args, context, CSSConicGradient);
    if (!stops)
        return nullptr;

    auto computedColorInterpolationMethod = computeGradientColorInterpolationMethod(context, colorInterpolationMethod, *stops);
    auto result = CSSConicGradientValue::create(repeating, computedColorInterpolationMethod, WTFMove(*stops));

    if (angle)
        result->setAngle(WTFMove(angle));
    if (position) {
        result->setFirstX(position->x.copyRef());
        result->setFirstY(position->y.copyRef());

        // Right now, conic gradients have the same start and end centers.
        result->setSecondX(WTFMove(position->x));
        result->setSecondY(WTFMove(position->y));
    }

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
    // FIXME: The current CSS Images spec has a pretty different construction than is being parsed here:
    //
    //    cross-fade() = cross-fade( <cf-image># )
    //    <cf-image> = <percentage [0,100]>? && [ <image> | <color> ]
    //
    //  https://drafts.csswg.org/css-images-4/#funcdef-cross-fade

    auto fromImageValueOrNone = consumeImageOrNone(args, context);
    if (!fromImageValueOrNone || !consumeCommaIncludingWhitespace(args))
        return nullptr;
    auto toImageValueOrNone = consumeImageOrNone(args, context);
    if (!toImageValueOrNone || !consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto percentage = consumeNumberOrPercentRaw<NumberOrPercentDividedBy100Transformer>(args);
    if (!percentage)
        return nullptr;
    auto percentageValue = CSSValuePool::singleton().createValue(clampTo<double>(*percentage, 0, 1), CSSUnitType::CSS_NUMBER);
    return CSSCrossfadeValue::create(fromImageValueOrNone.releaseNonNull(), toImageValueOrNone.releaseNonNull(), WTFMove(percentageValue), prefixed);
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
    // FIXME: The current Filter Effects spec has a different construction than is being parsed here:
    //
    //    filter() = filter( [ <image> | <string> ], <filter-value-list> )
    //
    //  https://drafts.fxtf.org/filter-effects/#funcdef-filter
    //
    // Importantly, `none` is not a valid value for the first parameter.

    auto imageValueOrNone = consumeImageOrNone(args, context);
    if (!imageValueOrNone || !consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto filterValue = consumeFilter(args, context, AllowedFilterFunctions::PixelFilters);

    if (!filterValue)
        return nullptr;

    if (!args.atEnd())
        return nullptr;

    return CSSFilterImageValue::create(imageValueOrNone.releaseNonNull(), filterValue.releaseNonNull());
}

#if ENABLE(CSS_PAINTING_API)
static RefPtr<CSSValue> consumeCustomPaint(CSSParserTokenRange& args)
{
    if (!DeprecatedGlobalSettings::cssPaintingAPIEnabled())
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
        result = consumePrefixedLinearGradient(args, context, NonRepeating);
    else if (id == CSSValueWebkitRepeatingLinearGradient)
        result = consumePrefixedLinearGradient(args, context, Repeating);
    else if (id == CSSValueRepeatingLinearGradient)
        result = consumeLinearGradient(args, context, Repeating);
    else if (id == CSSValueLinearGradient)
        result = consumeLinearGradient(args, context, NonRepeating);
    else if (id == CSSValueWebkitGradient)
        result = consumeDeprecatedGradient(args, context);
    else if (id == CSSValueWebkitRadialGradient)
        result = consumePrefixedRadialGradient(args, context, NonRepeating);
    else if (id == CSSValueWebkitRepeatingRadialGradient)
        result = consumePrefixedRadialGradient(args, context, Repeating);
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

        auto resolution = consumeResolution(args);
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
    if (nameToken.type() != IdentToken || !isValidCustomIdentifier(nameToken.id()))
        return AtomString();
    // In the context of the prelude of an @counter-style rule, a <counter-style-name> must not be an ASCII
    // case-insensitive match for "decimal" or "disc". No <counter-style-name>, prelude or not, may be an ASCII
    // case-insensitive match for "none".
    if (identMatches<CSSValueDecimal, CSSValueDisc, CSSValueNone>(nameToken.id()))
        return AtomString();
    auto name = nameToken.value();
    return isPredefinedCounterStyle(nameToken.id()) ? name.convertToASCIILowercaseAtom() : name.toAtomString();
}

RefPtr<CSSPrimitiveValue> consumeSingleContainerName(CSSParserTokenRange& range)
{
    switch (range.peek().id()) {
    case CSSValueNormal:
    case CSSValueNone:
    case CSSValueAuto:
    case CSSValueAnd:
    case CSSValueOr:
    case CSSValueNot:
        return nullptr;
    default:
        if (auto ident = consumeCustomIdent(range))
            return ident;
        return nullptr;
    }
}

std::optional<FontWeightRaw> consumeFontWeightRaw(CSSParserTokenRange& range)
{
    if (auto result = consumeIdentRaw<CSSValueNormal, CSSValueBold, CSSValueBolder, CSSValueLighter>(range))
        return { *result };
    if (auto result = consumeFontWeightNumberRaw(range))
        return { *result };
    return std::nullopt;
}

std::optional<CSSValueID> consumeFontStretchKeywordValueRaw(CSSParserTokenRange& range)
{
    return consumeIdentRaw<CSSValueUltraCondensed, CSSValueExtraCondensed, CSSValueCondensed, CSSValueSemiCondensed, CSSValueNormal, CSSValueSemiExpanded, CSSValueExpanded, CSSValueExtraExpanded, CSSValueUltraExpanded>(range);
}

std::optional<FontStyleRaw> consumeFontStyleRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
#if ENABLE(VARIATION_FONTS)
    CSSParserTokenRange rangeBeforeKeyword = range;
#endif

    auto keyword = consumeIdentRaw<CSSValueNormal, CSSValueItalic, CSSValueOblique>(range);
    if (!keyword)
        return std::nullopt;

#if ENABLE(VARIATION_FONTS)
    if (*keyword == CSSValueOblique && !range.atEnd()) {
        // FIXME: This angle does specify that unitless 0 is allowed - see https://drafts.csswg.org/css-fonts-4/#valdef-font-style-oblique-angle
        if (auto angle = consumeAngleRaw(range, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow)) {
            if (isFontStyleAngleInRange(CSSPrimitiveValue::computeDegrees(angle->type, angle->value)))
                return { { CSSValueOblique, WTFMove(angle) } };
            // Must not consume anything on error.
            range = rangeBeforeKeyword;
            return std::nullopt;
        }
    }
#else
    UNUSED_PARAM(parserMode);
#endif

    return { { *keyword, std::nullopt } };
}

AtomString concatenateFamilyName(CSSParserTokenRange& range)
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
    if (!addedSpace && !isValidCustomIdentifier(firstToken.id()))
        return nullAtom();
    return builder.toAtomString();
}

AtomString consumeFamilyNameRaw(CSSParserTokenRange& range)
{
    if (range.peek().type() == StringToken)
        return range.consumeIncludingWhitespace().value().toAtomString();
    if (range.peek().type() != IdentToken)
        return nullAtom();
    return concatenateFamilyName(range);
}

Vector<AtomString> consumeFamilyNameList(CSSParserTokenRange& range)
{
    Vector<AtomString> result;
    do {
        auto name = consumeFamilyNameRaw(range);
        if (name.isNull())
            return { };
        
        result.append(name);
    } while (consumeCommaIncludingWhitespace(range));
    return result;
}

static std::optional<Vector<FontFamilyRaw>> consumeFontFamilyRaw(CSSParserTokenRange& range)
{
    Vector<FontFamilyRaw> list;
    do {
        if (auto ident = consumeIdentRangeRaw(range, CSSValueSerif, CSSValueWebkitBody))
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

static std::optional<FontSizeRaw> consumeFontSizeRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    // -webkit-xxx-large is a parse-time alias.
    if (range.peek().id() == CSSValueWebkitXxxLarge) {
        if (auto ident = consumeIdentRaw(range); ident && ident == CSSValueWebkitXxxLarge)
            return { CSSValueXxxLarge };
        return std::nullopt;
    }

    if (range.peek().id() >= CSSValueXxSmall && range.peek().id() <= CSSValueLarger) {
        if (auto ident = consumeIdentRaw(range))
            return { *ident };
        return std::nullopt;
    }

    if (auto result = consumeLengthOrPercentRaw(range, parserMode))
        return { *result };

    return std::nullopt;
}

static std::optional<LineHeightRaw> consumeLineHeightRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    if (range.peek().id() == CSSValueNormal) {
        if (auto ident = consumeIdentRaw(range))
            return { *ident };
        return std::nullopt;
    }

    if (auto number = consumeNumberRaw(range, ValueRange::NonNegative))
        return { number->value };

    if (auto lengthOrPercent = consumeLengthOrPercentRaw(range, parserMode))
        return { *lengthOrPercent };

    return std::nullopt;
}

std::optional<FontRaw> consumeFontRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    FontRaw result;

    for (unsigned i = 0; i < 4 && !range.atEnd(); ++i) {
        if (consumeIdentRaw<CSSValueNormal>(range))
            continue;
        if (!result.style && (result.style = consumeFontStyleRaw(range, parserMode)))
            continue;
        if (!result.variantCaps && (result.variantCaps = consumeIdentRaw<CSSValueSmallCaps>(range)))
            continue;
        if (!result.weight && (result.weight = consumeFontWeightRaw(range)))
            continue;
        if (!result.stretch && (result.stretch = consumeFontStretchKeywordValueRaw(range)))
            continue;
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

RefPtr<CSSValueList> consumeAspectRatioValue(CSSParserTokenRange& range)
{
    auto leftValue = consumeNumber(range, ValueRange::NonNegative);
    if (!leftValue)
        return nullptr;

    bool slashSeen = consumeSlashIncludingWhitespace(range);
    auto rightValue = slashSeen
        ? consumeNumber(range, ValueRange::NonNegative)
        : CSSValuePool::singleton().createValue(1, CSSUnitType::CSS_NUMBER);

    if (!rightValue)
        return nullptr;

    auto ratioList = CSSValueList::createSlashSeparated();
    ratioList->append(leftValue.releaseNonNull());
    ratioList->append(rightValue.releaseNonNull());

    return ratioList;
}

} // namespace CSSPropertyParserHelpers

} // namespace WebCore
