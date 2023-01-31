// Copyright 2016 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "CSSPropertyParser.h"
#include "CSSPropertyParsing.h"

#include "CSSBackgroundRepeatValue.h"
#include "CSSBasicShapes.h"
#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSCanvasValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFilterImageValue.h"
#include "CSSFontPaletteValuesOverrideColorsValue.h"
#include "CSSFontVariantAlternatesValue.h"
#include "CSSFontVariantLigaturesParser.h"
#include "CSSFontVariantNumericParser.h"
#include "CSSFunctionValue.h"
#include "CSSGradientValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSNamedImageValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPaintImageValue.h"
#include "CSSParser.h"
#include "CSSParserIdioms.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSRayValue.h"
#include "CSSReflectValue.h"
#include "CSSResolvedColorMix.h"
#include "CSSSubgridValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSTransformListValue.h"
#include "CSSUnresolvedColor.h"
#include "CSSValuePool.h"
#include "CSSVariableData.h"
#include "CSSVariableParser.h"
#include "CalculationCategory.h"
#include "ColorConversion.h"
#include "ColorInterpolation.h"
#include "ColorLuminance.h"
#include "ColorNormalization.h"
#include "Counter.h"
#include "FontFace.h"
#include "Logging.h"
#include "Pair.h"
#include "Rect.h"
#include "RenderStyleConstants.h"
#include "SVGPathByteStream.h"
#include "SVGPathUtilities.h"
#include "StyleColor.h"
#include "TimingFunction.h"
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
using ColorOrUnresolvedColor = std::variant<Color, Ref<CSSUnresolvedColor>>;

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

template<typename Map>
static std::optional<typename Map::ValueType> consumeIdentUsingMapping(CSSParserTokenRange& range, Map& map)
{
    if (auto value = map.tryGet(range.peek().id())) {
        range.consumeIncludingWhitespace();
        return std::make_optional(*value);
    }
    return std::nullopt;
}

template<typename Map>
static std::optional<typename Map::ValueType> peekIdentUsingMapping(CSSParserTokenRange& range, Map& map)
{
    if (auto value = map.tryGet(range.peek().id()))
        return std::make_optional(*value);
    return std::nullopt;
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
    explicit CalcParser(CSSParserTokenRange& range, CalculationCategory destinationCategory, ValueRange valueRange = ValueRange::All, const CSSCalcSymbolTable& symbolTable = { }, NegativePercentagePolicy negativePercentagePolicy = NegativePercentagePolicy::Forbid)
        : m_sourceRange(range)
        , m_range(range)
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
        return CSSPrimitiveValue::create(m_calcValue.releaseNonNull());
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
        return CSSPrimitiveValue::create(m_calcValue.releaseNonNull());
    }

private:
    CSSParserTokenRange& m_sourceRange;
    CSSParserTokenRange m_range;
    RefPtr<CSSCalcValue> m_calcValue;
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

static constexpr double computeMinimumValue(IntegerValueRange range)
{
    switch (range) {
    case IntegerValueRange::All:
        return -std::numeric_limits<double>::infinity();
    case IntegerValueRange::NonNegative:
        return 0.0;
    case IntegerValueRange::Positive:
        return 1.0;
    }

    RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();

    return 0.0;
}

// MARK: Integer (Raw)

template<typename IntType, IntegerValueRange integerRange>
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

template<typename IntType, IntegerValueRange integerRange>
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

template<typename IntType, IntegerValueRange integerRange>
struct IntegerTypeKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
    {
        ASSERT(range.peek().type() == FunctionToken);

        if (auto integer = IntegerTypeRawKnownTokenTypeFunctionConsumer<IntType, integerRange>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return CSSPrimitiveValue::create(*integer, CSSUnitType::CSS_INTEGER);
        return nullptr;
    }
};

template<typename IntType, IntegerValueRange integerRange>
struct IntegerTypeKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
    {
        ASSERT(range.peek().type() == NumberToken);
        
        if (auto integer = IntegerTypeRawKnownTokenTypeNumberConsumer<IntType, integerRange>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return CSSPrimitiveValue::create(*integer, CSSUnitType::CSS_INTEGER);
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
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == FunctionToken);

        CalcParser parser(range, CalculationCategory::Number, valueRange, symbolTable);
        return parser.consumeValueIfCategory(CalculationCategory::Number);
    }
};

struct NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == NumberToken);
        
        auto token = range.peek();
        
        if (auto validatedValue = validatedNumberRaw(token.numericValue(), valueRange)) {
            auto unitType = token.unitType();
            range.consumeIncludingWhitespace();
            return CSSPrimitiveValue::create(validatedValue->value, unitType);
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
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == FunctionToken);

        CalcParser parser(range, CalculationCategory::Percent, valueRange, symbolTable);
        return parser.consumeValueIfCategory(CalculationCategory::Percent);
    }
};

struct PercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentConsumer {
    static constexpr CSSParserTokenType tokenType = PercentageToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == PercentageToken);
        
        if (auto validatedValue = validatedPercentRaw(range.peek().numericValue(), valueRange)) {
            range.consumeIncludingWhitespace();
            return CSSPrimitiveValue::create(validatedValue->value, CSSUnitType::CSS_PERCENTAGE);
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
    static std::optional<LengthRaw> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
    {
        ASSERT(range.peek().type() == NumberToken);

        auto& token = range.peek();

        if (!shouldAcceptUnitlessValue(token.numericValue(), parserMode, unitless, unitlessZero))
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
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == FunctionToken);

        CalcParser parser(range, CalculationCategory::Length, valueRange, symbolTable);
        return parser.consumeValueIfCategory(CalculationCategory::Length);
    }
};

struct LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
    {
        ASSERT(range.peek().type() == DimensionToken);

        if (auto lengthRaw = LengthRawKnownTokenTypeDimensionConsumer::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return CSSPrimitiveValue::create(lengthRaw->value, lengthRaw->type);
        return nullptr;
    }
};

struct LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (auto lengthRaw = LengthRawKnownTokenTypeNumberConsumer::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return CSSPrimitiveValue::create(lengthRaw->value, lengthRaw->type);
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
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == FunctionToken);

        CalcParser parser(range, CalculationCategory::Angle, valueRange, symbolTable);
        return parser.consumeValueIfCategory(CalculationCategory::Angle);
    }
};

struct AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
    {
        ASSERT(range.peek().type() == DimensionToken);

        if (auto angleRaw = AngleRawKnownTokenTypeDimensionConsumer::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return CSSPrimitiveValue::create(angleRaw->value, angleRaw->type);
        return nullptr;
    }
};

struct AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (auto angleRaw = AngleRawKnownTokenTypeNumberConsumer::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return CSSPrimitiveValue::create(angleRaw->value, angleRaw->type);
        return nullptr;
    }
};

// MARK: Time (CSSPrimitiveValue - maintaining calc)

struct TimeCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == FunctionToken);

        CalcParser parser(range, CalculationCategory::Time, valueRange, symbolTable);
        return parser.consumeValueIfCategory(CalculationCategory::Time);
    }
};

struct TimeCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == DimensionToken);

        if (valueRange == ValueRange::NonNegative && range.peek().numericValue() < 0)
            return nullptr;

        if (auto unit = range.peek().unitType(); unit == CSSUnitType::CSS_MS || unit == CSSUnitType::CSS_S)
            return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), unit);

        return nullptr;
    }
};

struct TimeCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (unitless == UnitlessQuirk::Allow && shouldAcceptUnitlessValue(range.peek().numericValue(), parserMode, unitless, UnitlessZeroQuirk::Allow)) {
            if (valueRange == ValueRange::NonNegative && range.peek().numericValue() < 0)
                return nullptr;
            return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_MS);
        }
        return nullptr;
    }
};

// MARK: Resolution (CSSPrimitiveValue - maintaining calc)

struct ResolutionCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == FunctionToken);

        CalcParser parser(range, CalculationCategory::Resolution, valueRange, symbolTable);
        return parser.consumeValueIfCategory(CalculationCategory::Resolution);
    }
};

struct ResolutionCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == DimensionToken);

        if (auto unit = range.peek().unitType(); unit == CSSUnitType::CSS_DPPX || unit == CSSUnitType::CSS_X || unit == CSSUnitType::CSS_DPI || unit == CSSUnitType::CSS_DPCM)
            return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), unit);

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

template<typename IntType, IntegerValueRange integerRange>
struct IntegerTypeRawConsumer {
    using Result = std::optional<IntType>;

    using FunctionToken = IntegerTypeRawKnownTokenTypeFunctionConsumer<IntType, integerRange>;
    using NumberToken = IntegerTypeRawKnownTokenTypeNumberConsumer<IntType, integerRange>;
};

template<typename IntType, IntegerValueRange integerRange>
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

    using FunctionToken = ResolutionCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer;
    using DimensionToken = ResolutionCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer;
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

template<typename IntType, IntegerValueRange integerRange> std::optional<IntType> consumeIntegerTypeRaw(CSSParserTokenRange& range)
{
    return consumeMetaConsumer<IntegerTypeRawConsumer<IntType, integerRange>>(range, { }, ValueRange::All, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename IntType, IntegerValueRange integerRange> RefPtr<CSSPrimitiveValue> consumeIntegerType(CSSParserTokenRange& range)
{
    return consumeMetaConsumer<IntegerTypeConsumer<IntType, integerRange>>(range, { }, ValueRange::All, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

std::optional<int> consumeIntegerRaw(CSSParserTokenRange& range)
{
    return consumeIntegerTypeRaw<int, IntegerValueRange::All>(range);
}

RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range)
{
    return consumeIntegerType<int, IntegerValueRange::All>(range);
}

std::optional<int> consumeNonNegativeIntegerRaw(CSSParserTokenRange& range)
{
    return consumeIntegerTypeRaw<int, IntegerValueRange::NonNegative>(range);
}

RefPtr<CSSPrimitiveValue> consumeNonNegativeInteger(CSSParserTokenRange& range)
{
    return consumeIntegerType<int, IntegerValueRange::NonNegative>(range);
}

std::optional<unsigned> consumePositiveIntegerRaw(CSSParserTokenRange& range)
{
    return consumeIntegerTypeRaw<unsigned, IntegerValueRange::Positive>(range);
}

RefPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange& range)
{
    return consumeIntegerType<unsigned, IntegerValueRange::Positive>(range);
}

RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange& range, IntegerValueRange valueRange)
{
    switch (valueRange) {
    case IntegerValueRange::All:
        return consumeInteger(range);
    case IntegerValueRange::Positive:
        return consumePositiveInteger(range);
    case IntegerValueRange::NonNegative:
        return consumeNonNegativeInteger(range);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

std::optional<NumberRaw> consumeNumberRaw(CSSParserTokenRange& range, ValueRange valueRange)
{
    return consumeMetaConsumer<NumberRawConsumer<RawIdentityTransformer<NumberRaw>>>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

RefPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange& range, ValueRange valueRange)
{
    return consumeMetaConsumer<NumberConsumer>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

static std::optional<PercentRaw> consumePercentRaw(CSSParserTokenRange& range)
{
    return consumeMetaConsumer<PercentRawConsumer<RawIdentityTransformer<PercentRaw>>>(range, { }, ValueRange::All, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

RefPtr<CSSPrimitiveValue> consumePercent(CSSParserTokenRange& range, ValueRange valueRange)
{
    return consumeMetaConsumer<PercentConsumer>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    return consumeMetaConsumer<LengthConsumer>(range, { }, valueRange, parserMode, unitless, UnitlessZeroQuirk::Allow);
}

#if ENABLE(VARIATION_FONTS)
static std::optional<AngleRaw> consumeAngleRaw(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    return consumeMetaConsumer<AngleRawConsumer<RawIdentityTransformer<AngleRaw>>>(range, { }, ValueRange::All, parserMode, unitless, unitlessZero);
}
#endif

RefPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    return consumeMetaConsumer<AngleConsumer>(range, { }, ValueRange::All, parserMode, unitless, unitlessZero);
}

RefPtr<CSSPrimitiveValue> consumeTime(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    return consumeMetaConsumer<TimeConsumer>(range, { }, valueRange, parserMode, unitless, UnitlessZeroQuirk::Forbid);
}

RefPtr<CSSPrimitiveValue> consumeResolution(CSSParserTokenRange& range)
{
    return consumeMetaConsumer<ResolutionConsumer>(range, { }, ValueRange::All, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

#if ENABLE(CSS_CONIC_GRADIENTS)
static RefPtr<CSSPrimitiveValue> consumeAngleOrPercent(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    return consumeMetaConsumer<AngleOrPercentConsumer>(range, { }, ValueRange::All, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
}
#endif

static std::optional<LengthOrPercentRaw> consumeLengthOrPercentRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    return consumeMetaConsumer<LengthOrPercentRawConsumer<RawIdentityTransformer<LengthOrPercentRaw>>>(range, { }, ValueRange::NonNegative, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

// FIXME: This doesn't work with the current scheme due to the NegativePercentagePolicy parameter
RefPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, NegativePercentagePolicy negativePercentagePolicy)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken: {
        // FIXME: Should this be using trying to generate the calc with both Length and Percent destination category types?
        CalcParser parser(range, CalculationCategory::Length, valueRange, { }, negativePercentagePolicy);
        if (auto calculation = parser.value(); calculation && canConsumeCalcValue(calculation->category(), parserMode))
            return parser.consumeValue();
        break;
    }

    case DimensionToken:
        return LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer::consume(range, { }, valueRange, parserMode, unitless, unitlessZero);

    case NumberToken:
        return LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer::consume(range, { }, valueRange, parserMode, unitless, unitlessZero);

    case PercentageToken:
        return PercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentConsumer::consume(range, { }, valueRange, parserMode, unitless, unitlessZero);

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
        if (auto value = NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer::consume(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid))
            return value;
        return PercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer::consume(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);

    case NumberToken:
        return NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer::consume(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);

    case PercentageToken:
        if (auto percentRaw = PercentRawKnownTokenTypePercentConsumer::consume(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid))
            return CSSPrimitiveValue::create(percentRaw->value / 100.0, CSSUnitType::CSS_NUMBER);
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

RefPtr<CSSPrimitiveValue> consumeFontWeightNumber(CSSParserTokenRange& range)
{
    if (auto result = consumeFontWeightNumberRaw(range))
        return CSSPrimitiveValue::create(*result, CSSUnitType::CSS_NUMBER);
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
    if (auto result = consumeIdentRaw(range))
        return CSSPrimitiveValue::create(*result);
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

static String consumeCustomIdentRaw(CSSParserTokenRange& range, bool shouldLowercase = false)
{
    if (range.peek().type() != IdentToken || !isValidCustomIdentifier(range.peek().id()))
        return String();
    auto identifier = range.consumeIncludingWhitespace().value();
    return shouldLowercase ? identifier.convertToASCIILowercase() : identifier.toString();
}

RefPtr<CSSPrimitiveValue> consumeCustomIdent(CSSParserTokenRange& range, bool shouldLowercase)
{
    auto identifier = consumeCustomIdentRaw(range, shouldLowercase);
    if (identifier.isNull())
        return nullptr;
    return CSSPrimitiveValue::createCustomIdent(WTFMove(identifier));
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
    return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().value().toString(), CSSUnitType::CSS_STRING);
}

StringView consumeURLRaw(CSSParserTokenRange& range)
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

RefPtr<CSSPrimitiveValue> consumeURL(CSSParserTokenRange& range)
{
    StringView url = consumeURLRaw(range);
    if (url.isNull())
        return nullptr;
    return CSSPrimitiveValue::create(url.toString(), CSSUnitType::CSS_URI);
}

static Color consumeOriginColorRaw(CSSParserTokenRange& args, const CSSParserContext& context)
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

static std::optional<double> consumeOptionalAlphaRaw(CSSParserTokenRange& range)
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

static std::optional<double> consumeOptionalAlphaRawAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable)
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

static Color parseRelativeRGBParametersRaw(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColorRaw(args, context);
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

    auto alpha = consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable);
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

static Color parseNonRelativeRGBParametersRaw(CSSParserTokenRange& args)
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

template<RGBFunctionMode Mode> static Color parseRGBParametersRaw(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == (Mode == RGBFunctionMode::RGB ? CSSValueRgb : CSSValueRgba));
    auto args = consumeFunction(range);

    if constexpr (Mode == RGBFunctionMode::RGB) {
        if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
            return parseRelativeRGBParametersRaw(args, context);
    }
    return parseNonRelativeRGBParametersRaw(args);
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

static Color parseRelativeHSLParametersRaw(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColorRaw(args, context);
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

    auto alpha = consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable);
    if (!alpha)
        return { };

    if (!args.atEnd())
        return { };

    return colorByNormalizingHSLComponents(*hue, *saturation, *lightness, *alpha, RGBOrHSLSeparatorSyntax::WhitespaceSlash);
}

static Color parseNonRelativeHSLParametersRaw(CSSParserTokenRange& args, const CSSParserContext& context)
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

template<HSLFunctionMode Mode> static Color parseHSLParametersRaw(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == (Mode == HSLFunctionMode::HSL ? CSSValueHsl : CSSValueHsla));
    auto args = consumeFunction(range);

    if constexpr (Mode == HSLFunctionMode::HSL) {
        if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
            return parseRelativeHSLParametersRaw(args, context);
    }

    return parseNonRelativeHSLParametersRaw(args, context);
}

template<typename ConsumerForHue, typename ConsumerForWhitenessAndBlackness, typename ConsumerForAlpha>
static Color parseHWBParametersRaw(CSSParserTokenRange& args, ConsumerForHue&& hueConsumer, ConsumerForWhitenessAndBlackness&& whitenessAndBlacknessConsumer, ConsumerForAlpha&& alphaConsumer)
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

static Color parseRelativeHWBParametersRaw(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColorRaw(args, context);
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
    auto alphaConsumer = [&symbolTable](auto& args) { return consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable); };

    return parseHWBParametersRaw(args, WTFMove(hueConsumer), WTFMove(whitenessAndBlacknessConsumer), WTFMove(alphaConsumer));
}

static Color parseNonRelativeHWBParametersRaw(CSSParserTokenRange& args, const CSSParserContext& context)
{
    auto hueConsumer = [&context](auto& args) { return consumeAngleOrNumberOrNoneRaw(args, context.mode); };
    auto whitenessAndBlacknessConsumer = [](auto& args) { return consumePercentOrNoneRaw(args); };
    auto alphaConsumer = [](auto& args) { return consumeOptionalAlphaRaw(args); };

    return parseHWBParametersRaw(args, WTFMove(hueConsumer), WTFMove(whitenessAndBlacknessConsumer), WTFMove(alphaConsumer));
}

static Color parseHWBParametersRaw(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueHwb);

    if (!context.cssColor4)
        return { };

    auto args = consumeFunction(range);

    if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
        return parseRelativeHWBParametersRaw(args, context);
    return parseNonRelativeHWBParametersRaw(args, context);
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
static Color parseLabParametersRaw(CSSParserTokenRange& args, ConsumerForLightness&& lightnessConsumer, ConsumerForAB&& abConsumer, ConsumerForAlpha&& alphaConsumer)
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
static Color parseRelativeLabParametersRaw(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColorRaw(args, context);
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
    auto alphaConsumer = [&symbolTable](auto& args) { return consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable); };

    return parseLabParametersRaw<ColorType>(args, WTFMove(lightnessConsumer), WTFMove(abConsumer), WTFMove(alphaConsumer));
}

template<typename ColorType>
static Color parseNonRelativeLabParametersRaw(CSSParserTokenRange& args)
{
    auto lightnessConsumer = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto abConsumer = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto alphaConsumer = [](auto& args) { return consumeOptionalAlphaRaw(args); };

    return parseLabParametersRaw<ColorType>(args, WTFMove(lightnessConsumer), WTFMove(abConsumer), WTFMove(alphaConsumer));
}

template<typename ColorType>
static Color parseLabParametersRaw(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueLab || range.peek().functionId() == CSSValueOklab);

    if (!context.cssColor4)
        return { };

    auto args = consumeFunction(range);

    if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
        return parseRelativeLabParametersRaw<ColorType>(args, context);
    return parseNonRelativeLabParametersRaw<ColorType>(args);
}

template<typename ColorType, typename ConsumerForLightness, typename ConsumerForChroma, typename ConsumerForHue, typename ConsumerForAlpha>
static Color parseLCHParametersRaw(CSSParserTokenRange& args, ConsumerForLightness&& lightnessConsumer, ConsumerForChroma&& chromaConsumer, ConsumerForHue&& hueConsumer, ConsumerForAlpha&& alphaConsumer)
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
static Color parseRelativeLCHParametersRaw(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColorRaw(args, context);
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
    auto alphaConsumer = [&symbolTable](auto& args) { return consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable); };

    return parseLCHParametersRaw<ColorType>(args, WTFMove(lightnessConsumer), WTFMove(chromaConsumer), WTFMove(hueConsumer), WTFMove(alphaConsumer));
}

template<typename ColorType>
static Color parseNonRelativeLCHParametersRaw(CSSParserTokenRange& args, const CSSParserContext& context)
{
    auto lightnessConsumer = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto chromaConsumer = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto hueConsumer = [&context](auto& args) { return consumeAngleOrNumberOrNoneRaw(args, context.mode); };
    auto alphaConsumer = [](auto& args) { return consumeOptionalAlphaRaw(args); };

    return parseLCHParametersRaw<ColorType>(args, WTFMove(lightnessConsumer), WTFMove(chromaConsumer), WTFMove(hueConsumer), WTFMove(alphaConsumer));
}

template<typename ColorType>
static Color parseLCHParametersRaw(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueLch || range.peek().functionId() == CSSValueOklch);

    if (!context.cssColor4)
        return { };

    auto args = consumeFunction(range);

    if (context.relativeColorSyntaxEnabled && args.peek().id() == CSSValueFrom)
        return parseRelativeLCHParametersRaw<ColorType>(args, context);
    return parseNonRelativeLCHParametersRaw<ColorType>(args, context);
}

template<typename ColorType, typename ConsumerForRGB, typename ConsumerForAlpha>
static Color parseColorFunctionForRGBTypesRaw(CSSParserTokenRange& args, ConsumerForRGB&& rgbConsumer, ConsumerForAlpha&& alphaConsumer)
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
    auto consumeAlpha = [&symbolTable](auto& args) { return consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable); };

    return parseColorFunctionForRGBTypesRaw<ColorType>(args, WTFMove(consumeRGB), WTFMove(consumeAlpha));
}

template<typename ColorType> static Color parseColorFunctionForRGBTypesRaw(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueDisplayP3 || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020 || args.peek().id() == CSSValueSRGB || args.peek().id() == CSSValueSrgbLinear);

    // Support sRGB and Display-P3 regardless of the setting as we have shipped support for them for a while.
    if (!context.cssColor4 && (args.peek().id() == CSSValueA98Rgb || args.peek().id() == CSSValueProphotoRgb || args.peek().id() == CSSValueRec2020 || args.peek().id() == CSSValueSrgbLinear))
        return { };

    consumeIdentRaw(args);

    auto consumeRGB = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto consumeAlpha = [](auto& args) { return consumeOptionalAlphaRaw(args); };

    return parseColorFunctionForRGBTypesRaw<ColorType>(args, WTFMove(consumeRGB), WTFMove(consumeAlpha));
}

template<typename ColorType, typename ConsumerForXYZ, typename ConsumerForAlpha>
static Color parseColorFunctionForXYZTypesRaw(CSSParserTokenRange& args, ConsumerForXYZ&& xyzConsumer, ConsumerForAlpha&& alphaConsumer)
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
    auto consumeAlpha = [&symbolTable](auto& args) { return consumeOptionalAlphaRawAllowingSymbolTableIdent(args, symbolTable); };

    return parseColorFunctionForXYZTypesRaw<ColorType>(args, WTFMove(consumeXYZ), WTFMove(consumeAlpha));
}

template<typename ColorType> static Color parseColorFunctionForXYZTypesRaw(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueXyz || args.peek().id() == CSSValueXyzD50 || args.peek().id() == CSSValueXyzD65);

    if (!context.cssColor4)
        return { };

    consumeIdentRaw(args);

    auto consumeXYZ = [](auto& args) { return consumeNumberOrPercentOrNoneRaw(args); };
    auto consumeAlpha = [](auto& args) { return consumeOptionalAlphaRaw(args); };

    return parseColorFunctionForXYZTypesRaw<ColorType>(args, WTFMove(consumeXYZ), WTFMove(consumeAlpha));
}

static Color parseRelativeColorFunctionParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    ASSERT(args.peek().id() == CSSValueFrom);
    consumeIdentRaw(args);

    auto originColor = consumeOriginColorRaw(args, context);
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
        return parseColorFunctionForRGBTypesRaw<ExtendedA98RGB<float>>(args, context);
    case CSSValueDisplayP3:
        return parseColorFunctionForRGBTypesRaw<ExtendedDisplayP3<float>>(args, context);
    case CSSValueProphotoRgb:
        return parseColorFunctionForRGBTypesRaw<ExtendedProPhotoRGB<float>>(args, context);
    case CSSValueRec2020:
        return parseColorFunctionForRGBTypesRaw<ExtendedRec2020<float>>(args, context);
    case CSSValueSRGB:
        return parseColorFunctionForRGBTypesRaw<ExtendedSRGBA<float>>(args, context);
    case CSSValueSrgbLinear:
        return parseColorFunctionForRGBTypesRaw<ExtendedLinearSRGBA<float>>(args, context);
    case CSSValueXyzD50:
        return parseColorFunctionForXYZTypesRaw<XYZA<float, WhitePoint::D50>>(args, context);
    case CSSValueXyz:
    case CSSValueXyzD65:
        return parseColorFunctionForXYZTypesRaw<XYZA<float, WhitePoint::D65>>(args, context);
    default:
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

static Color parseColorFunctionParametersRaw(CSSParserTokenRange& range, const CSSParserContext& context)
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

static Color parseColorContrastFunctionParametersRaw(CSSParserTokenRange& range, const CSSParserContext& context)
{
    ASSERT(range.peek().functionId() == CSSValueColorContrast);

    if (!context.colorContrastEnabled)
        return { };

    auto args = consumeFunction(range);

    auto originBackgroundColor = consumeOriginColorRaw(args, context);
    if (!originBackgroundColor.isValid())
        return { };

    if (!consumeIdentRaw<CSSValueVs>(args))
        return { };

    Vector<Color> colorsToCompareAgainst;
    bool consumedTo = false;
    do {
        auto colorToCompareAgainst = consumeOriginColorRaw(args, context);
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

static std::optional<HueInterpolationMethod> consumeHueInterpolationMethod(CSSParserTokenRange& range)
{
    static constexpr std::pair<CSSValueID, HueInterpolationMethod> hueInterpolationMethodMappings[] {
        { CSSValueShorter, HueInterpolationMethod::Shorter },
        { CSSValueLonger, HueInterpolationMethod::Longer },
        { CSSValueIncreasing, HueInterpolationMethod::Increasing },
        { CSSValueDecreasing, HueInterpolationMethod::Decreasing },
    };
    static constexpr SortedArrayMap hueInterpolationMethodMap { hueInterpolationMethodMappings };

    return consumeIdentUsingMapping(range, hueInterpolationMethodMap);
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

static std::optional<CSSResolvedColorMix::Component> consumeColorMixComponentRaw(CSSParserTokenRange& args, const CSSParserContext& context)
{
    CSSResolvedColorMix::Component result;

    if (auto percentage = consumePercentRaw(args)) {
        if (percentage->value < 0.0 || percentage->value > 100.0)
            return { };
        result.percentage = percentage->value;
    }

    result.color = consumeOriginColorRaw(args, context);
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

static std::optional<CSSUnresolvedColorMix::Component> consumeColorMixComponent(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // [ <color> && <percentage [0,100]>? ]

    RefPtr<CSSPrimitiveValue> color;
    RefPtr<CSSPrimitiveValue> percentage;

    if (auto percent = consumePercent(args, ValueRange::All)) {
        if (!percent->isCalculated()) {
            auto value = percent->doubleValue();
            if (value < 0.0 || value > 100.0)
                return std::nullopt;
        }
        percentage = percent;
    }

    auto originColor = consumeColor(args, context);
    if (!originColor)
        return std::nullopt;

    if (!percentage) {
        if (auto percent = consumePercent(args, ValueRange::All)) {
            if (!percent->isCalculated()) {
                auto value = percent->doubleValue();
                if (value < 0.0 || value > 100.0)
                    return std::nullopt;
            }
            percentage = percent;
        }
    }

    return CSSUnresolvedColorMix::Component {
        originColor.releaseNonNull(),
        WTFMove(percentage)
    };
}

static Color parseColorMixFunctionParametersRaw(CSSParserTokenRange& range, const CSSParserContext& context)
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

    auto mixComponent1 = consumeColorMixComponentRaw(args, context);
    if (!mixComponent1)
        return { };

    if (!consumeCommaIncludingWhitespace(args))
        return { };

    auto mixComponent2 = consumeColorMixComponentRaw(args, context);
    if (!mixComponent2)
        return { };

    if (!args.atEnd())
        return { };

    return mix(CSSResolvedColorMix {
        WTFMove(*colorInterpolationMethod),
        WTFMove(*mixComponent1),
        WTFMove(*mixComponent2)
    });
}

static bool hasNonCalculatedZeroPercentage(const CSSUnresolvedColorMix::Component& mixComponent)
{
    return mixComponent.percentage && !mixComponent.percentage->isCalculated() && mixComponent.percentage->doubleValue() == 0.0;
}

static std::optional<ColorOrUnresolvedColor> parseColorMixFunctionParameters(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // color-mix() = color-mix( <color-interpolation-method> , [ <color> && <percentage [0,100]>? ]#{2})

    ASSERT(range.peek().functionId() == CSSValueColorMix);

    if (!context.colorMixEnabled)
        return std::nullopt;

    auto args = consumeFunction(range);

    if (args.peek().id() != CSSValueIn)
        return std::nullopt;

    auto colorInterpolationMethod = consumeColorInterpolationMethod(args);
    if (!colorInterpolationMethod)
        return std::nullopt;

    if (!consumeCommaIncludingWhitespace(args))
        return std::nullopt;

    auto mixComponent1 = consumeColorMixComponent(args, context);
    if (!mixComponent1)
        return std::nullopt;

    if (!consumeCommaIncludingWhitespace(args))
        return std::nullopt;

    auto mixComponent2 = consumeColorMixComponent(args, context);
    if (!mixComponent2)
        return std::nullopt;

    if (!args.atEnd())
        return std::nullopt;

    if (hasNonCalculatedZeroPercentage(*mixComponent1) && hasNonCalculatedZeroPercentage(*mixComponent2)) {
        // This eagerly marks the parse as invalid if both percentage components are non-calc
        // and equal to 0. This satisfies step 5 of section 2.1. Percentage Normalization in
        // https://w3c.github.io/csswg-drafts/css-color-5/#color-mix which states:
        //    "If the percentages sum to zero, the function is invalid."
        //
        // The only way the percentages can sum to zero is both are zero, since we reject any
        // percentages less than zero, and missing percentages are treated as "100% - p(other)".
        //
        // FIXME: Should we also do this for calculated values? Or should we let the parse be
        // valid and fail to produce a valid color at style building time.
        return std::nullopt;
    }

    return { CSSUnresolvedColor::create(CSSUnresolvedColorMix {
        WTFMove(*colorInterpolationMethod),
        WTFMove(*mixComponent1),
        WTFMove(*mixComponent2)
    }) };
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

static Color parseColorFunctionRaw(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSParserTokenRange colorRange = range;
    CSSValueID functionId = range.peek().functionId();
    Color color;
    switch (functionId) {
    case CSSValueRgb:
        color = parseRGBParametersRaw<RGBFunctionMode::RGB>(colorRange, context);
        break;
    case CSSValueRgba:
        color = parseRGBParametersRaw<RGBFunctionMode::RGBA>(colorRange, context);
        break;
    case CSSValueHsl:
        color = parseHSLParametersRaw<HSLFunctionMode::HSL>(colorRange, context);
        break;
    case CSSValueHsla:
        color = parseHSLParametersRaw<HSLFunctionMode::HSLA>(colorRange, context);
        break;
    case CSSValueHwb:
        color = parseHWBParametersRaw(colorRange, context);
        break;
    case CSSValueLab:
        color = parseLabParametersRaw<Lab<float>>(colorRange, context);
        break;
    case CSSValueLch:
        color = parseLCHParametersRaw<LCHA<float>>(colorRange, context);
        break;
    case CSSValueOklab:
        color = parseLabParametersRaw<OKLab<float>>(colorRange, context);
        break;
    case CSSValueOklch:
        color = parseLCHParametersRaw<OKLCHA<float>>(colorRange, context);
        break;
    case CSSValueColor:
        color = parseColorFunctionParametersRaw(colorRange, context);
        break;
    case CSSValueColorContrast:
        color = parseColorContrastFunctionParametersRaw(colorRange, context);
        break;
    case CSSValueColorMix:
        color = parseColorMixFunctionParametersRaw(colorRange, context);
        break;
    default:
        return { };
    }
    if (color.isValid())
        range = colorRange;
    return color;
}

static std::optional<ColorOrUnresolvedColor> parseColorFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto checkColor = [] (Color&& color) -> std::optional<ColorOrUnresolvedColor> {
        if (!color.isValid())
            return std::nullopt;
        return ColorOrUnresolvedColor { WTFMove(color) };
    };

    CSSParserTokenRange colorRange = range;
    CSSValueID functionId = range.peek().functionId();
    std::optional<ColorOrUnresolvedColor> color;
    switch (functionId) {
    case CSSValueRgb:
        color = checkColor(parseRGBParametersRaw<RGBFunctionMode::RGB>(colorRange, context));
        break;
    case CSSValueRgba:
        color = checkColor(parseRGBParametersRaw<RGBFunctionMode::RGBA>(colorRange, context));
        break;
    case CSSValueHsl:
        color = checkColor(parseHSLParametersRaw<HSLFunctionMode::HSL>(colorRange, context));
        break;
    case CSSValueHsla:
        color = checkColor(parseHSLParametersRaw<HSLFunctionMode::HSLA>(colorRange, context));
        break;
    case CSSValueHwb:
        color = checkColor(parseHWBParametersRaw(colorRange, context));
        break;
    case CSSValueLab:
        color = checkColor(parseLabParametersRaw<Lab<float>>(colorRange, context));
        break;
    case CSSValueLch:
        color = checkColor(parseLCHParametersRaw<LCHA<float>>(colorRange, context));
        break;
    case CSSValueOklab:
        color = checkColor(parseLabParametersRaw<OKLab<float>>(colorRange, context));
        break;
    case CSSValueOklch:
        color = checkColor(parseLCHParametersRaw<OKLCHA<float>>(colorRange, context));
        break;
    case CSSValueColor:
        color = checkColor(parseColorFunctionParametersRaw(colorRange, context));
        break;
    case CSSValueColorContrast:
        color = checkColor(parseColorContrastFunctionParametersRaw(colorRange, context));
        break;
    case CSSValueColorMix:
        color = parseColorMixFunctionParameters(colorRange, context);
        break;
    default:
        return { };
    }
    if (color)
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
        result = parseColorFunctionRaw(range, context);

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

    if (auto parsedColor = parseHexColor(range, acceptQuirkyColors))
        return CSSValuePool::singleton().createColorValue(Color { *parsedColor });

    if (auto colorOrUnresolvedColor = parseColorFunction(range, context)) {
        return WTF::switchOn(WTFMove(*colorOrUnresolvedColor),
            [] (Color&& color) -> RefPtr<CSSPrimitiveValue> {
                return CSSValuePool::singleton().createColorValue(WTFMove(color));
            },
            [] (Ref<CSSUnresolvedColor>&& color) -> RefPtr<CSSPrimitiveValue> {
                return CSSPrimitiveValue::create(WTFMove(color));
            }
        );
    }

    return nullptr;
}

static RefPtr<CSSPrimitiveValue> consumePositionComponent(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, NegativePercentagePolicy negativePercentagePolicy = NegativePercentagePolicy::Forbid)
{
    if (range.peek().type() == IdentToken)
        return consumeIdent<CSSValueLeft, CSSValueTop, CSSValueBottom, CSSValueRight, CSSValueCenter>(range);
    return consumeLengthOrPercent(range, parserMode, ValueRange::All, unitless, UnitlessZeroQuirk::Allow, negativePercentagePolicy);
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
        return { CSSPrimitiveValue::create(CSSValueCenter), value };
    return { value, CSSPrimitiveValue::create(CSSValueCenter) };
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
            result = createPrimitiveValuePair(currentValue, values[++i]);
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
            result = createPrimitiveValuePair(currentValue, values[++i]);
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
        return createPrimitiveValuePair(WTFMove(coordinates->x), WTFMove(coordinates->y));
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

static RefPtr<CSSPrimitiveValue> consumeSingleAxisPosition(CSSParserTokenRange& range, CSSParserMode parserMode, BoxOrient orientation)
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
        return createPrimitiveValuePair(WTFMove(value1), WTFMove(value2));

    return value1 ? value1 : value2;
}

// This should go away once we drop support for -webkit-gradient
static RefPtr<CSSPrimitiveValue> consumeDeprecatedGradientPointValue(CSSParserTokenRange& range, bool horizontal)
{
    if (range.peek().type() == IdentToken) {
        if ((horizontal && consumeIdent<CSSValueLeft>(range)) || (!horizontal && consumeIdent<CSSValueTop>(range)))
            return CSSPrimitiveValue::create(0., CSSUnitType::CSS_PERCENTAGE);
        if ((horizontal && consumeIdent<CSSValueRight>(range)) || (!horizontal && consumeIdent<CSSValueBottom>(range)))
            return CSSPrimitiveValue::create(100., CSSUnitType::CSS_PERCENTAGE);
        if (consumeIdent<CSSValueCenter>(range))
            return CSSPrimitiveValue::create(50., CSSUnitType::CSS_PERCENTAGE);
        return nullptr;
    }
    RefPtr<CSSPrimitiveValue> result = consumePercent(range, ValueRange::All);
    if (!result)
        result = consumeNumber(range, ValueRange::All);
    return result;
}

static std::optional<std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>>> consumeDeprecatedGradientPoint(CSSParserTokenRange& range)
{
    auto x = consumeDeprecatedGradientPointValue(range, true);
    if (!x)
        return std::nullopt;

    auto y = consumeDeprecatedGradientPointValue(range, false);
    if (!y)
        return std::nullopt;

    return std::make_pair(x.releaseNonNull(), y.releaseNonNull());
}

// Used to parse colors for -webkit-gradient(...).
static RefPtr<CSSPrimitiveValue> consumeDeprecatedGradientStopColor(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueCurrentcolor)
        return nullptr;
    return consumeColor(range, context);
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

    stop.position = CSSPrimitiveValue::create(position, CSSUnitType::CSS_NUMBER);
    stop.color = consumeDeprecatedGradientStopColor(args, context);
    return stop.color && args.atEnd();
}

static std::optional<CSSGradientColorStopList> consumeDeprecatedGradientColorStops(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSGradientColorStopList stops;
    while (consumeCommaIncludingWhitespace(range)) {
        CSSGradientColorStop stop;
        if (!consumeDeprecatedGradientColorStop(range, stop, context))
            return std::nullopt;
        stops.append(WTFMove(stop));
    }
    stops.shrinkToFit();

    return { WTFMove(stops) };
}

static AlphaPremultiplication gradientAlphaPremultiplication(const CSSParserContext& context)
{
    return context.gradientPremultipliedAlphaInterpolationEnabled ? AlphaPremultiplication::Premultiplied : AlphaPremultiplication::Unpremultiplied;
}

static RefPtr<CSSValue> consumeDeprecatedLinearGradient(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto first = consumeDeprecatedGradientPoint(range);
    if (!first)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto second = consumeDeprecatedGradientPoint(range);
    if (!second)
        return nullptr;

    auto stops = consumeDeprecatedGradientColorStops(range, context);
    if (!stops)
        return nullptr;

    auto& [firstX, firstY] = *first;
    auto& [secondX, secondY] = *second;

    return CSSDeprecatedLinearGradientValue::create({
            WTFMove(firstX),
            WTFMove(firstY),
            WTFMove(secondX),
            WTFMove(secondY)
        },
        CSSGradientColorInterpolationMethod::legacyMethod(gradientAlphaPremultiplication(context)),
        WTFMove(*stops)
    );
}

static RefPtr<CSSValue> consumeDeprecatedRadialGradient(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto first = consumeDeprecatedGradientPoint(range);
    if (!first)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto firstRadius = consumeNumber(range, ValueRange::NonNegative);
    if (!firstRadius)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto second = consumeDeprecatedGradientPoint(range);
    if (!second)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto secondRadius = consumeNumber(range, ValueRange::NonNegative);
    if (!secondRadius)
        return nullptr;

    auto stops = consumeDeprecatedGradientColorStops(range, context);
    if (!stops)
        return nullptr;

    auto& [firstX, firstY] = *first;
    auto& [secondX, secondY] = *second;

    return CSSDeprecatedRadialGradientValue::create({
            WTFMove(firstX),
            WTFMove(firstY),
            WTFMove(secondX),
            WTFMove(secondY),
            firstRadius.releaseNonNull(),
            secondRadius.releaseNonNull()
        },
        CSSGradientColorInterpolationMethod::legacyMethod(gradientAlphaPremultiplication(context)),
        WTFMove(*stops)
    );
}

static RefPtr<CSSValue> consumeDeprecatedGradient(CSSParserTokenRange& range, const CSSParserContext& context)
{
    switch (range.consumeIncludingWhitespace().id()) {
    case CSSValueLinear:
        return consumeDeprecatedLinearGradient(range, context);
    case CSSValueRadial:
        return consumeDeprecatedRadialGradient(range, context);
    default:
        return nullptr;
    }
}

enum class SupportsColorHints : bool { Yes, No };

template<typename Consumer>
static std::optional<CSSGradientColorStopList> consumeColorStopList(CSSParserTokenRange& range, const CSSParserContext& context, SupportsColorHints supportsColorHints, Consumer&& consumeStopPosition)
{
    CSSGradientColorStopList stops;

    // The first color stop cannot be a color hint.
    bool previousStopWasColorHint = true;
    do {
        CSSGradientColorStop stop { consumeColor(range, context), consumeStopPosition() };
        if (!stop.color && !stop.position)
            return std::nullopt;

        // Two color hints in a row are not allowed.
        if (!stop.color && (supportsColorHints == SupportsColorHints::No || previousStopWasColorHint))
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

static std::optional<CSSGradientColorStopList> consumeLengthColorStopList(CSSParserTokenRange& range, const CSSParserContext& context, SupportsColorHints supportsColorHints)
{
    return consumeColorStopList(range, context, supportsColorHints, [&] { return consumeLengthOrPercent(range, context.mode, ValueRange::All); });
}

#if ENABLE(CSS_CONIC_GRADIENTS)
static std::optional<CSSGradientColorStopList> consumeAngularColorStopList(CSSParserTokenRange& range, const CSSParserContext& context, SupportsColorHints supportsColorHints)
{
    return consumeColorStopList(range, context, supportsColorHints, [&] { return consumeAngleOrPercent(range, context.mode); });
}
#endif

static RefPtr<CSSValue> consumePrefixedRadialGradient(CSSParserTokenRange& range, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    // https://compat.spec.whatwg.org/#css-gradients-webkit-radial-gradient/ states that -webkit-radial-gradient() and
    // -webkit-repeating-radial-gradient() must be "treated as an alias of radial-gradient as defined in [css3-images-20110217]."
    // In [css3-images-20110217] the grammar was defined as:
    //
    //   <radial-gradient> = radial-gradient([<bg-position>,]? [ [<shape> || <size>] | [<length> | <percentage>]{2} ,]? <color-stop>[, <color-stop>]+);
    //   <shape> = [ circle | ellipse ]
    //   <size> = [ closest-side | closest-corner | farthest-side | farthest-corner | contain | cover ]
    //
    //      defaults to ‘ellipse cover’.
    //
    // see https://www.w3.org/TR/2011/WD-css3-images-20110217/#radial-gradients.

    static constexpr std::pair<CSSValueID, CSSPrefixedRadialGradientValue::ShapeKeyword> shapeMappings[] {
        { CSSValueCircle, CSSPrefixedRadialGradientValue::ShapeKeyword::Circle },
        { CSSValueEllipse, CSSPrefixedRadialGradientValue::ShapeKeyword::Ellipse },
    };
    static constexpr SortedArrayMap shapeMap { shapeMappings };
    
    static constexpr std::pair<CSSValueID, CSSPrefixedRadialGradientValue::ExtentKeyword> extentMappings[] {
        { CSSValueContain, CSSPrefixedRadialGradientValue::ExtentKeyword::Contain },
        { CSSValueCover, CSSPrefixedRadialGradientValue::ExtentKeyword::Cover },
        { CSSValueClosestSide, CSSPrefixedRadialGradientValue::ExtentKeyword::ClosestSide },
        { CSSValueClosestCorner, CSSPrefixedRadialGradientValue::ExtentKeyword::ClosestCorner },
        { CSSValueFarthestSide, CSSPrefixedRadialGradientValue::ExtentKeyword::FarthestSide },
        { CSSValueFarthestCorner, CSSPrefixedRadialGradientValue::ExtentKeyword::FarthestCorner },
    };
    static constexpr SortedArrayMap extentMap { extentMappings };

    std::optional<CSSGradientPosition> position;
    if (auto centerCoordinate = consumeOneOrTwoValuedPositionCoordinates(range, context.mode, UnitlessQuirk::Forbid)) {
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
        position = CSSGradientPosition { WTFMove(centerCoordinate->x), WTFMove(centerCoordinate->y) };
    }

    std::optional<CSSPrefixedRadialGradientValue::ShapeKeyword> shapeKeyword;
    std::optional<CSSPrefixedRadialGradientValue::ExtentKeyword> extentKeyword;
    if (range.peek().type() == IdentToken) {
        shapeKeyword = consumeIdentUsingMapping(range, shapeMap);
        extentKeyword = consumeIdentUsingMapping(range, extentMap);
        if (!shapeKeyword)
            shapeKeyword = consumeIdentUsingMapping(range, shapeMap);
        if (shapeKeyword || extentKeyword) {
            if (!consumeCommaIncludingWhitespace(range))
                return nullptr;
        }
    }

    CSSPrefixedRadialGradientValue::GradientBox gradientBox = std::monostate { };

    if (shapeKeyword && extentKeyword) {
        gradientBox = CSSPrefixedRadialGradientValue::ShapeAndExtent { *shapeKeyword, *extentKeyword };
    } else if (shapeKeyword) {
        gradientBox = *shapeKeyword;
    } else if (extentKeyword) {
        gradientBox = *extentKeyword;
    } else {
        if (auto length1 = consumeLengthOrPercent(range, context.mode, ValueRange::NonNegative)) {
            auto length2 = consumeLengthOrPercent(range, context.mode, ValueRange::NonNegative);
            if (!length2)
                return nullptr;
            if (!consumeCommaIncludingWhitespace(range))
                return nullptr;
            gradientBox = CSSPrefixedRadialGradientValue::MeasuredSize { { length1.releaseNonNull(), length2.releaseNonNull() } };
        }
    }

    auto stops = consumeLengthColorStopList(range, context, SupportsColorHints::No);
    if (!stops)
        return nullptr;

    auto colorInterpolationMethod = CSSGradientColorInterpolationMethod::legacyMethod(gradientAlphaPremultiplication(context));
    return CSSPrefixedRadialGradientValue::create({ WTFMove(gradientBox), WTFMove(position) },
        repeating, colorInterpolationMethod, WTFMove(*stops));
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

static RefPtr<CSSValue> consumeRadialGradient(CSSParserTokenRange& range, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    // radial-gradient() = radial-gradient(
    //   [[ <ending-shape> || <size> ]? [ at <position> ]? ] || <color-interpolation-method>,
    //   <color-stop-list>
    // )

    static constexpr std::pair<CSSValueID, CSSRadialGradientValue::ShapeKeyword> shapeMappings[] {
        { CSSValueCircle, CSSRadialGradientValue::ShapeKeyword::Circle },
        { CSSValueEllipse, CSSRadialGradientValue::ShapeKeyword::Ellipse },
    };
    static constexpr SortedArrayMap shapeMap { shapeMappings };
    
    static constexpr std::pair<CSSValueID, CSSRadialGradientValue::ExtentKeyword> extentMappings[] {
        { CSSValueClosestSide, CSSRadialGradientValue::ExtentKeyword::ClosestSide },
        { CSSValueClosestCorner, CSSRadialGradientValue::ExtentKeyword::ClosestCorner },
        { CSSValueFarthestSide, CSSRadialGradientValue::ExtentKeyword::FarthestSide },
        { CSSValueFarthestCorner, CSSRadialGradientValue::ExtentKeyword::FarthestCorner },
    };
    static constexpr SortedArrayMap extentMap { extentMappings };

    std::optional<ColorInterpolationMethod> colorInterpolationMethod;

    if (context.gradientInterpolationColorSpacesEnabled) {
        if (range.peek().id() == CSSValueIn) {
            colorInterpolationMethod = consumeColorInterpolationMethod(range);
            if (!colorInterpolationMethod)
                return nullptr;
        }
    }

    std::optional<CSSRadialGradientValue::ShapeKeyword> shape;

    using Size = std::variant<CSSRadialGradientValue::ExtentKeyword, Ref<CSSPrimitiveValue>, std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>>>;
    std::optional<Size> size;

    // First part of grammar, the size/shape clause:
    //
    //   [ [ circle               || <length [0,∞]> ]                          [ at <position> ]? , |
    //     [ ellipse              || <length-percentage [0,∞]>{2} ]            [ at <position> ]? , |
    //     [ [ circle | ellipse ] || <extent-keyword> ]                        [ at <position> ]? , |
    //     at <position> ,
    //   ]?
    for (int i = 0; i < 3; ++i) {
        if (range.peek().type() == IdentToken) {
            if (auto peekedShape = peekIdentUsingMapping(range, shapeMap)) {
                if (shape)
                    return nullptr;
                shape = *peekedShape;
                range.consumeIncludingWhitespace();
            } else if (auto peekedExtent = peekIdentUsingMapping(range, extentMap)) {
                if (size)
                    return nullptr;
                size = *peekedExtent;
                range.consumeIncludingWhitespace();
            }

            if (!shape && !size)
                break;
        } else {
            auto length1 = consumeLengthOrPercent(range, context.mode, ValueRange::NonNegative);
            if (!length1)
                break;
            if (size)
                return nullptr;
            if (auto length2 = consumeLengthOrPercent(range, context.mode, ValueRange::NonNegative)) {
                size = std::make_pair(length1.releaseNonNull(), length2.releaseNonNull());

                // Additional increment is necessary since we consumed a second token.
                ++i;
            } else {
                // If there is only one, it has to be a Length, not a percentage.
                if (length1->isPercentage())
                    return nullptr;
                size = length1.releaseNonNull();
            }
        }
    }

    std::optional<CSSGradientPosition> position;
    if (consumeIdent<CSSValueAt>(range)) {
        auto positionCoordinates = consumePositionCoordinates(range, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
        if (!positionCoordinates)
            return nullptr;
        position = CSSGradientPosition { WTFMove(positionCoordinates->x), WTFMove(positionCoordinates->y) };
    }

    if (context.gradientInterpolationColorSpacesEnabled) {
        if ((shape || size || position) && !colorInterpolationMethod && range.peek().id() == CSSValueIn) {
            colorInterpolationMethod = consumeColorInterpolationMethod(range);
            if (!colorInterpolationMethod)
                return nullptr;
        }
    }

    if ((shape || size || position || colorInterpolationMethod) && !consumeCommaIncludingWhitespace(range))
        return nullptr;

    auto stops = consumeLengthColorStopList(range, context, SupportsColorHints::Yes);
    if (!stops)
        return nullptr;

    auto computedColorInterpolationMethod = computeGradientColorInterpolationMethod(context, colorInterpolationMethod, *stops);

    CSSRadialGradientValue::Data data;
    if (shape) {
        switch (*shape) {
        case CSSRadialGradientValue::ShapeKeyword::Circle:
            if (!size)
                data.gradientBox = CSSRadialGradientValue::Shape { CSSRadialGradientValue::ShapeKeyword::Circle, WTFMove(position) };
            else {
                bool validSize = WTF::switchOn(WTFMove(*size),
                    [&] (CSSRadialGradientValue::ExtentKeyword&& extent) -> bool {
                        data.gradientBox = CSSRadialGradientValue::CircleOfExtent { WTFMove(extent), WTFMove(position) };
                        return true;
                    },
                    [&] (Ref<CSSPrimitiveValue>&& length) -> bool {
                        data.gradientBox = CSSRadialGradientValue::CircleOfLength { WTFMove(length), WTFMove(position) };
                        return true;
                    },
                    [&] (std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>>&&) -> bool {
                        return false;
                    }
                );
                if (!validSize)
                    return nullptr;
            }
            break;

        case CSSRadialGradientValue::ShapeKeyword::Ellipse:
            if (!size)
                data.gradientBox = CSSRadialGradientValue::Shape { CSSRadialGradientValue::ShapeKeyword::Ellipse, WTFMove(position) };
            else {
                bool validSize = WTF::switchOn(WTFMove(*size),
                    [&] (CSSRadialGradientValue::ExtentKeyword&& extent) -> bool {
                        data.gradientBox = CSSRadialGradientValue::EllipseOfExtent { WTFMove(extent), WTFMove(position) };
                        return true;
                    },
                    [&] (Ref<CSSPrimitiveValue>&&) -> bool {
                        // Ellipses must have two length-percentages specified.
                        return false;
                    },
                    [&] (std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>>&& size) -> bool {
                        data.gradientBox = CSSRadialGradientValue::EllipseOfSize { WTFMove(size), WTFMove(position) };
                        return true;
                    }
                );
                if (!validSize)
                    return nullptr;
            }
            break;
        }
    } else {
        if (!size) {
            if (position)
                data.gradientBox = WTFMove(*position);
            else
                data.gradientBox = std::monostate { };
        } else {
            WTF::switchOn(WTFMove(*size),
                [&] (CSSRadialGradientValue::ExtentKeyword&& extent) {
                    data.gradientBox = CSSRadialGradientValue::Extent { WTFMove(extent), WTFMove(position) };
                },
                [&] (Ref<CSSPrimitiveValue>&& length) {
                    data.gradientBox = CSSRadialGradientValue::Length { WTFMove(length), WTFMove(position) };
                },
                [&] (std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>>&& size) {
                    data.gradientBox = CSSRadialGradientValue::Size { WTFMove(size), WTFMove(position) };
                }
            );
        }
    }

    return CSSRadialGradientValue::create(WTFMove(data), repeating, computedColorInterpolationMethod, WTFMove(*stops));
}

static RefPtr<CSSValue> consumePrefixedLinearGradient(CSSParserTokenRange& range, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    // https://compat.spec.whatwg.org/#css-gradients-webkit-linear-gradient/ states that -webkit-linear-gradient() and
    // -webkit-repeating-linear-gradient() must be "treated as an alias of linear-gradient as defined in [css3-images-20110217]."
    // In [css3-images-20110217] the grammar was defined as:
    //
    //   <linear-gradient> = linear-gradient([ [ [top | bottom] || [left | right] ] | <angle> ,]? <color-stop>[, <color-stop>]+);
    //
    // see https://www.w3.org/TR/2011/WD-css3-images-20110217/#linear-gradients.

    static constexpr std::pair<CSSValueID, CSSPrefixedLinearGradientValue::Vertical> verticalMappings[] {
        { CSSValueTop, CSSPrefixedLinearGradientValue::Vertical::Top },
        { CSSValueBottom, CSSPrefixedLinearGradientValue::Vertical::Bottom },
    };
    static constexpr SortedArrayMap verticalMap { verticalMappings };
    
    static constexpr std::pair<CSSValueID, CSSPrefixedLinearGradientValue::Horizontal> horizontalMappings[] {
        { CSSValueLeft, CSSPrefixedLinearGradientValue::Horizontal::Left },
        { CSSValueRight, CSSPrefixedLinearGradientValue::Horizontal::Right },
    };
    static constexpr SortedArrayMap horizontalMap { horizontalMappings };

    auto consumeKeywordGradientLineKnownHorizontal = [&](CSSParserTokenRange& range, CSSPrefixedLinearGradientValue::Horizontal knownHorizontal) -> CSSPrefixedLinearGradientValue::GradientLine {
        if (auto vertical = consumeIdentUsingMapping(range, verticalMap))
            return std::make_pair(knownHorizontal, *vertical);
        return knownHorizontal;
    };

    auto consumeKeywordGradientLineKnownVertical = [&](CSSParserTokenRange& range, CSSPrefixedLinearGradientValue::Vertical knownVertical) -> CSSPrefixedLinearGradientValue::GradientLine {
        if (auto horizontal = consumeIdentUsingMapping(range, horizontalMap))
            return std::make_pair(*horizontal, knownVertical);
        return knownVertical;
    };

    auto consumeKeywordGradientLine = [&](CSSParserTokenRange& range) -> std::optional<CSSPrefixedLinearGradientValue::GradientLine> {
        switch (range.peek().id()) {
        case CSSValueLeft:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSSPrefixedLinearGradientValue::Horizontal::Left);
        case CSSValueRight:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSSPrefixedLinearGradientValue::Horizontal::Right);
        case CSSValueTop:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSSPrefixedLinearGradientValue::Vertical::Top);
        case CSSValueBottom:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSSPrefixedLinearGradientValue::Vertical::Bottom);
        default:
            return { };
        }
    };

    std::optional<CSSPrefixedLinearGradientValue::GradientLine> gradientLine;

    if (auto angle = consumeAngle(range, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow)) {
        gradientLine = CSSPrefixedLinearGradientValue::Angle { angle.releaseNonNull() };
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    } else if (auto keywordGradientLine = consumeKeywordGradientLine(range)) {
        gradientLine = WTFMove(*keywordGradientLine);
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    auto stops = consumeLengthColorStopList(range, context, SupportsColorHints::No);
    if (!stops)
        return nullptr;

    auto colorInterpolationMethod = CSSGradientColorInterpolationMethod::legacyMethod(gradientAlphaPremultiplication(context));

    return CSSPrefixedLinearGradientValue::create(
        CSSPrefixedLinearGradientValue::Data { gradientLine.value_or(std::monostate { }) },
        repeating,
        colorInterpolationMethod,
        WTFMove(*stops)
    );
}

static RefPtr<CSSValue> consumeLinearGradient(CSSParserTokenRange& range, const CSSParserContext& context, CSSGradientRepeat repeating)
{
    // <side-or-corner> = [left | right] || [top | bottom]
    // linear-gradient() = linear-gradient(
    //   [ <angle> | to <side-or-corner> ]? || <color-interpolation-method>,
    //   <color-stop-list>
    // )

    static constexpr std::pair<CSSValueID, CSSLinearGradientValue::Vertical> verticalMappings[] {
        { CSSValueTop, CSSLinearGradientValue::Vertical::Top },
        { CSSValueBottom, CSSLinearGradientValue::Vertical::Bottom },
    };
    static constexpr SortedArrayMap verticalMap { verticalMappings };
    
    static constexpr std::pair<CSSValueID, CSSLinearGradientValue::Horizontal> horizontalMappings[] {
        { CSSValueLeft, CSSLinearGradientValue::Horizontal::Left },
        { CSSValueRight, CSSLinearGradientValue::Horizontal::Right },
    };
    static constexpr SortedArrayMap horizontalMap { horizontalMappings };

    auto consumeKeywordGradientLineKnownHorizontal = [&](CSSParserTokenRange& range, CSSLinearGradientValue::Horizontal knownHorizontal) -> CSSLinearGradientValue::GradientLine {
        if (auto vertical = consumeIdentUsingMapping(range, verticalMap))
            return std::make_pair(knownHorizontal, *vertical);
        return knownHorizontal;
    };

    auto consumeKeywordGradientLineKnownVertical = [&](CSSParserTokenRange& range, CSSLinearGradientValue::Vertical knownVertical) -> CSSLinearGradientValue::GradientLine {
        if (auto horizontal = consumeIdentUsingMapping(range, horizontalMap))
            return std::make_pair(*horizontal, knownVertical);
        return knownVertical;
    };

    auto consumeKeywordGradientLine = [&](CSSParserTokenRange& range) -> std::optional<CSSLinearGradientValue::GradientLine> {
        ASSERT(range.peek().id() == CSSValueTo);
        range.consumeIncludingWhitespace();

        switch (range.peek().id()) {
        case CSSValueLeft:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSSLinearGradientValue::Horizontal::Left);
        case CSSValueRight:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownHorizontal(range, CSSLinearGradientValue::Horizontal::Right);
        case CSSValueTop:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSSLinearGradientValue::Vertical::Top);
        case CSSValueBottom:
            range.consumeIncludingWhitespace();
            return consumeKeywordGradientLineKnownVertical(range, CSSLinearGradientValue::Vertical::Bottom);
        default:
            return { };
        }
    };

    std::optional<ColorInterpolationMethod> colorInterpolationMethod;

    if (context.gradientInterpolationColorSpacesEnabled) {
        if (range.peek().id() == CSSValueIn) {
            colorInterpolationMethod = consumeColorInterpolationMethod(range);
            if (!colorInterpolationMethod)
                return nullptr;
        }
    }

    std::optional<CSSLinearGradientValue::GradientLine> gradientLine;

    if (auto angle = consumeAngle(range, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow))
        gradientLine = CSSLinearGradientValue::Angle { angle.releaseNonNull() };
    else if (range.peek().id() == CSSValueTo) {
        auto keywordGradientLine = consumeKeywordGradientLine(range);
        if (!keywordGradientLine)
            return nullptr;
        gradientLine = WTFMove(*keywordGradientLine);
    }

    if (context.gradientInterpolationColorSpacesEnabled) {
        if (gradientLine && !colorInterpolationMethod && range.peek().id() == CSSValueIn) {
            colorInterpolationMethod = consumeColorInterpolationMethod(range);
            if (!colorInterpolationMethod)
                return nullptr;
        }
    }

    if (gradientLine || colorInterpolationMethod) {
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    auto stops = consumeLengthColorStopList(range, context, SupportsColorHints::Yes);
    if (!stops)
        return nullptr;

    auto computedColorInterpolationMethod = computeGradientColorInterpolationMethod(context, colorInterpolationMethod, *stops);

    return CSSLinearGradientValue::create(
        CSSLinearGradientValue::Data { gradientLine.value_or(std::monostate { }) },
        repeating,
        computedColorInterpolationMethod,
        WTFMove(*stops)
    );
}

static RefPtr<CSSValue> consumeConicGradient(CSSParserTokenRange& range, const CSSParserContext& context, CSSGradientRepeat repeating)
{
#if ENABLE(CSS_CONIC_GRADIENTS)
    // conic-gradient() = conic-gradient(
    //   [ [ from <angle> ]? [ at <position> ]? ] || <color-interpolation-method>,
    //   <angular-color-stop-list>
    // )

    std::optional<ColorInterpolationMethod> colorInterpolationMethod;

    if (context.gradientInterpolationColorSpacesEnabled) {
        if (range.peek().id() == CSSValueIn) {
            colorInterpolationMethod = consumeColorInterpolationMethod(range);
            if (!colorInterpolationMethod)
                return nullptr;
        }
    }

    RefPtr<CSSPrimitiveValue> angle;
    if (consumeIdent<CSSValueFrom>(range)) {
        // FIXME: Unlike linear-gradient, conic-gradients are not specified to allow unitless 0 angles - https://www.w3.org/TR/css-images-4/#valdef-conic-gradient-angle.
        angle = consumeAngle(range, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
        if (!angle)
            return nullptr;
    }
    
    std::optional<CSSGradientPosition> position;
    if (consumeIdent<CSSValueAt>(range)) {
        auto positionCoordinate = consumePositionCoordinates(range, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
        if (!positionCoordinate)
            return nullptr;
        position = CSSGradientPosition { WTFMove(positionCoordinate->x), WTFMove(positionCoordinate->y) };
    }

    if (context.gradientInterpolationColorSpacesEnabled) {
        if ((angle || position) && !colorInterpolationMethod && range.peek().id() == CSSValueIn) {
            colorInterpolationMethod = consumeColorInterpolationMethod(range);
            if (!colorInterpolationMethod)
                return nullptr;
        }
    }

    if (angle || position || colorInterpolationMethod) {
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    auto stops = consumeAngularColorStopList(range, context, SupportsColorHints::Yes);
    if (!stops)
        return nullptr;

    auto computedColorInterpolationMethod = computeGradientColorInterpolationMethod(context, colorInterpolationMethod, *stops);

    return CSSConicGradientValue::create({
            CSSConicGradientValue::Angle { WTFMove(angle) },
            WTFMove(position)
        },
        repeating,
        computedColorInterpolationMethod,
        WTFMove(*stops)
    );
#else
    UNUSED_PARAM(range);
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
    auto percentageValue = CSSPrimitiveValue::create(clampTo<double>(*percentage, 0, 1), CSSUnitType::CSS_NUMBER);
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
static RefPtr<CSSValue> consumeCustomPaint(CSSParserTokenRange& args, const CSSParserContext& context)
{
    if (!context.cssPaintingAPIEnabled)
        return nullptr;
    if (args.peek().type() != IdentToken)
        return nullptr;
    auto name = args.consumeIncludingWhitespace().value().toString();

    if (!args.atEnd() && args.peek() != CommaToken)
        return nullptr;
    if (!args.atEnd())
        args.consume();

    auto argumentList = CSSVariableData::create(args.consumeAll());
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
        result = consumeRadialGradient(args, context, CSSGradientRepeat::NonRepeating);
    else if (id == CSSValueRepeatingRadialGradient)
        result = consumeRadialGradient(args, context, CSSGradientRepeat::Repeating);
    else if (id == CSSValueWebkitLinearGradient)
        result = consumePrefixedLinearGradient(args, context, CSSGradientRepeat::NonRepeating);
    else if (id == CSSValueWebkitRepeatingLinearGradient)
        result = consumePrefixedLinearGradient(args, context, CSSGradientRepeat::Repeating);
    else if (id == CSSValueRepeatingLinearGradient)
        result = consumeLinearGradient(args, context, CSSGradientRepeat::Repeating);
    else if (id == CSSValueLinearGradient)
        result = consumeLinearGradient(args, context, CSSGradientRepeat::NonRepeating);
    else if (id == CSSValueWebkitGradient)
        result = consumeDeprecatedGradient(args, context);
    else if (id == CSSValueWebkitRadialGradient)
        result = consumePrefixedRadialGradient(args, context, CSSGradientRepeat::NonRepeating);
    else if (id == CSSValueWebkitRepeatingRadialGradient)
        result = consumePrefixedRadialGradient(args, context, CSSGradientRepeat::Repeating);
    else if (id == CSSValueConicGradient)
        result = consumeConicGradient(args, context, CSSGradientRepeat::NonRepeating);
    else if (id == CSSValueRepeatingConicGradient)
        result = consumeConicGradient(args, context, CSSGradientRepeat::Repeating);
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
        result = consumeCustomPaint(args, context);
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
        RefPtr<CSSValue> filterValue = referenceFiltersAllowed ? consumeURL(range) : nullptr;
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
        if (auto string = consumeURLRaw(range); !string.isNull()) {
            return CSSImageValue::create(context.completeURL(string.toAtomString().string()),
                context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No);
        }
    }

    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#predefined-counters
static bool isPredefinedCounterStyle(CSSValueID valueID)
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
    case CSSValueNone:
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

static std::optional<FontWeightRaw> consumeFontWeightRaw(CSSParserTokenRange& range)
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

static std::optional<FontStyleRaw> consumeFontStyleRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
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
        return WebKitFontFamilyNames::serifFamily.get();
    case CSSValueSansSerif:
        return WebKitFontFamilyNames::sansSerifFamily.get();
    case CSSValueCursive:
        return WebKitFontFamilyNames::cursiveFamily.get();
    case CSSValueFantasy:
        return WebKitFontFamilyNames::fantasyFamily.get();
    case CSSValueMonospace:
        return WebKitFontFamilyNames::monospaceFamily.get();
    case CSSValueWebkitPictograph:
        return WebKitFontFamilyNames::pictographFamily.get();
    case CSSValueSystemUi:
        return WebKitFontFamilyNames::systemUiFamily.get();
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
        : CSSPrimitiveValue::create(1, CSSUnitType::CSS_NUMBER);

    if (!rightValue)
        return nullptr;

    auto ratioList = CSSValueList::createSlashSeparated();
    ratioList->append(leftValue.releaseNonNull());
    ratioList->append(rightValue.releaseNonNull());

    return ratioList;
}

RefPtr<CSSValue> consumeAspectRatio(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> autoValue;
    if (range.peek().type() == IdentToken)
        autoValue = consumeIdent<CSSValueAuto>(range);

    if (range.atEnd())
        return autoValue;

    auto ratioList = consumeAspectRatioValue(range);
    if (!ratioList)
        return nullptr;

    if (!autoValue)
        autoValue = consumeIdent<CSSValueAuto>(range);

    if (!autoValue)
        return ratioList;

    auto list = CSSValueList::createSpaceSeparated();
    list->append(CSSPrimitiveValue::create(CSSValueAuto));
    list->append(ratioList.releaseNonNull());

    return list;
}

RefPtr<CSSValue> consumeDisplay(CSSParserTokenRange& range)
{
    // Parse single keyword values
    auto singleKeyword = consumeIdent<
        // <display-box>
        CSSValueContents,
        CSSValueNone,
        // <display-internal>
        CSSValueTableCaption,
        CSSValueTableCell,
        CSSValueTableColumnGroup,
        CSSValueTableColumn,
        CSSValueTableHeaderGroup,
        CSSValueTableFooterGroup,
        CSSValueTableRow,
        CSSValueTableRowGroup,
        // <display-legacy>
        CSSValueInlineBlock,
        CSSValueInlineFlex,
        CSSValueInlineGrid,
        CSSValueInlineTable,
        // Prefixed values
        CSSValueWebkitInlineBox,
        CSSValueWebkitBox,
        // No layout support for the full <display-listitem> syntax, so treat it as <display-legacy>
        CSSValueListItem
    >(range);

    if (singleKeyword)
        return singleKeyword;

    // Empty value, stop parsing
    if (range.atEnd())
        return nullptr;

    // Convert -webkit-flex/-webkit-inline-flex to flex/inline-flex
    CSSValueID nextValueID = range.peek().id();
    if (nextValueID == CSSValueWebkitInlineFlex || nextValueID == CSSValueWebkitFlex) {
        consumeIdent(range);
        return CSSPrimitiveValue::create(
            nextValueID == CSSValueWebkitInlineFlex ? CSSValueInlineFlex : CSSValueFlex);
    }

    // Parse [ <display-outside> || <display-inside> ]
    std::optional<CSSValueID> parsedDisplayOutside;
    std::optional<CSSValueID> parsedDisplayInside;
    while (!range.atEnd()) {
        auto nextValueID = range.peek().id();
        switch (nextValueID) {
        // <display-outside>
        case CSSValueBlock:
        case CSSValueInline:
            if (parsedDisplayOutside)
                return nullptr;
            parsedDisplayOutside = nextValueID;
            break;
        // <display-inside>
        case CSSValueFlex:
        case CSSValueFlow:
        case CSSValueFlowRoot:
        case CSSValueGrid:
        case CSSValueTable:
            if (parsedDisplayInside)
                return nullptr;
            parsedDisplayInside = nextValueID;
            break;
        default:
            return nullptr;
        }
        consumeIdent(range);
    }

    // Set defaults when one of the two values are unspecified
    CSSValueID displayOutside = parsedDisplayOutside.value_or(CSSValueBlock);
    CSSValueID displayInside = parsedDisplayInside.value_or(CSSValueFlow);

    auto selectShortValue = [&]() -> CSSValueID {
        if (displayOutside == CSSValueBlock) {
            // Alias display: flow to display: block
            return displayInside == CSSValueFlow ? CSSValueBlock : displayInside;
        }

        // Convert `display: inline <display-inside>` to the equivalent short value
        switch (displayInside) {
        case CSSValueFlex:
            return CSSValueInlineFlex;
        case CSSValueFlow:
            return CSSValueInline;
        case CSSValueFlowRoot:
            return CSSValueInlineBlock;
        case CSSValueGrid:
            return CSSValueInlineGrid;
        case CSSValueTable:
            return CSSValueInlineTable;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueInline;
        }
    };

    return CSSPrimitiveValue::create(selectShortValue());
}

RefPtr<CSSValue> consumeWillChange(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    RefPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    // Every comma-separated list of identifiers is a valid will-change value,
    // unless the list includes an explicitly disallowed identifier.
    while (!range.atEnd()) {
        switch (range.peek().id()) {
        case CSSValueContents:
        case CSSValueScrollPosition:
            values->append(consumeIdent(range).releaseNonNull());
            break;
        case CSSValueNone:
        case CSSValueAll:
        case CSSValueAuto:
            return nullptr;
        default:
            if (range.peek().type() != IdentToken)
                return nullptr;
            CSSPropertyID propertyID = cssPropertyID(range.peek().value());
            if (propertyID == CSSPropertyWillChange)
                return nullptr;
            if (!isExposed(propertyID, &context.propertySettings))
                propertyID = CSSPropertyInvalid;
            if (propertyID != CSSPropertyInvalid) {
                values->append(CSSPrimitiveValue::create(propertyID));
                range.consumeIncludingWhitespace();
                break;
            }
            if (auto customIdent = consumeCustomIdent(range)) {
                // Append properties we don't recognize, but that are legal, as strings.
                values->append(customIdent.releaseNonNull());
                break;
            }
            return nullptr;
        }

        // This is a comma separated list
        if (!range.atEnd() && !consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    return values;
}

RefPtr<CSSValue> consumeQuotes(CSSParserTokenRange& range)
{
    auto id = range.peek().id();
    if (id == CSSValueNone || id == CSSValueAuto)
        return consumeIdent(range);
    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    while (!range.atEnd()) {
        RefPtr<CSSPrimitiveValue> parsedValue = consumeString(range);
        if (!parsedValue)
            return nullptr;
        values->append(parsedValue.releaseNonNull());
    }
    if (values->length() && values->length() % 2 == 0)
        return values;
    return nullptr;
}

RefPtr<CSSValue> consumeFontVariantLigatures(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal || range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    CSSFontVariantLigaturesParser ligaturesParser;
    do {
        if (ligaturesParser.consumeLigature(range) != CSSFontVariantLigaturesParser::ParseResult::ConsumedValue)
            return nullptr;
    } while (!range.atEnd());

    return ligaturesParser.finalizeValue();
}

RefPtr<CSSValue> consumeFontVariantEastAsian(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);
    
    std::optional<FontVariantEastAsianVariant> variant;
    std::optional<FontVariantEastAsianWidth> width;
    std::optional<FontVariantEastAsianRuby> ruby;
    
    auto parseSomethingWithoutError = [&range, &variant, &width, &ruby] () {
        bool hasParsedSomething = false;

        while (true) {
            if (range.peek().type() != IdentToken)
                return hasParsedSomething;

            switch (range.peek().id()) {
            case CSSValueJis78:
                if (variant)
                    return false;
                variant = FontVariantEastAsianVariant::Jis78;
                break;
            case CSSValueJis83:
                if (variant)
                    return false;
                variant = FontVariantEastAsianVariant::Jis83;
                break;
            case CSSValueJis90:
                if (variant)
                    return false;
                variant = FontVariantEastAsianVariant::Jis90;
                break;
            case CSSValueJis04:
                if (variant)
                    return false;
                variant = FontVariantEastAsianVariant::Jis04;
                break;
            case CSSValueSimplified:
                if (variant)
                    return false;
                variant = FontVariantEastAsianVariant::Simplified;
                break;
            case CSSValueTraditional:
                if (variant)
                    return false;
                variant = FontVariantEastAsianVariant::Traditional;
                break;
            case CSSValueFullWidth:
                if (width)
                    return false;
                width = FontVariantEastAsianWidth::Full;
                break;
            case CSSValueProportionalWidth:
                if (width)
                    return false;
                width = FontVariantEastAsianWidth::Proportional;
                break;
            case CSSValueRuby:
                if (ruby)
                    return false;
                ruby = FontVariantEastAsianRuby::Yes;
                break;
            default:
                return hasParsedSomething;
            }
        
            range.consumeIncludingWhitespace();
            hasParsedSomething = true;
        }
    };
    
    if (!parseSomethingWithoutError())
        return nullptr;

    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    switch (variant.value_or(FontVariantEastAsianVariant::Normal)) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        values->append(CSSPrimitiveValue::create(CSSValueJis78));
        break;
    case FontVariantEastAsianVariant::Jis83:
        values->append(CSSPrimitiveValue::create(CSSValueJis83));
        break;
    case FontVariantEastAsianVariant::Jis90:
        values->append(CSSPrimitiveValue::create(CSSValueJis90));
        break;
    case FontVariantEastAsianVariant::Jis04:
        values->append(CSSPrimitiveValue::create(CSSValueJis04));
        break;
    case FontVariantEastAsianVariant::Simplified:
        values->append(CSSPrimitiveValue::create(CSSValueSimplified));
        break;
    case FontVariantEastAsianVariant::Traditional:
        values->append(CSSPrimitiveValue::create(CSSValueTraditional));
        break;
    }

    switch (width.value_or(FontVariantEastAsianWidth::Normal)) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        values->append(CSSPrimitiveValue::create(CSSValueFullWidth));
        break;
    case FontVariantEastAsianWidth::Proportional:
        values->append(CSSPrimitiveValue::create(CSSValueProportionalWidth));
        break;
    }
        
    switch (ruby.value_or(FontVariantEastAsianRuby::Normal)) {
    case FontVariantEastAsianRuby::Normal:
        break;
    case FontVariantEastAsianRuby::Yes:
        values->append(CSSPrimitiveValue::create(CSSValueRuby));
    }

    if (!values->length())
        return nullptr;

    return values;
}

RefPtr<CSSValue> consumeFontVariantAlternates(CSSParserTokenRange& range)
{
    if (range.atEnd())
        return nullptr;

    if (range.peek().id() == CSSValueNormal) {
        consumeIdent<CSSValueNormal>(range);
        return CSSPrimitiveValue::create(CSSValueNormal);
    }

    auto result = FontVariantAlternates::Normal();

    auto parseSomethingWithoutError = [&range, &result]() {
        bool hasParsedSomething = false;
        auto parseAndSetArgument = [&range, &hasParsedSomething] (std::optional<String>& value) {
            if (value)
                return false;

            CSSParserTokenRange args = consumeFunction(range);
            auto ident = consumeCustomIdent(args);
            if (!args.atEnd())
                return false;
        
            if (!ident)
                return false;
        
            hasParsedSomething = true;
            value = ident->stringValue();
            return true;
        };
        auto parseListAndSetArgument = [&range, &hasParsedSomething] (Vector<String>& value) {
            if (!value.isEmpty())
                return false;

            CSSParserTokenRange args = consumeFunction(range);
            do {
                auto ident = consumeCustomIdent(args);
                if (!ident)
                    return false;
                value.append(ident->stringValue());
            } while (consumeCommaIncludingWhitespace(args));

            if (!args.atEnd())
                return false;
        
            hasParsedSomething = true;
            return true;
        };
        while (true) {
            const CSSParserToken& token = range.peek();
            if (token.id() == CSSValueHistoricalForms) {
                consumeIdent<CSSValueHistoricalForms>(range);
                
                if (result.valuesRef().historicalForms)
                    return false;

                if (result.isNormal())
                    result.setValues();

                hasParsedSomething = true;
                result.valuesRef().historicalForms = true;
            } else if (token.functionId() == CSSValueSwash) {
                if (!parseAndSetArgument(result.valuesRef().swash))
                    return false;
            } else if (token.functionId() == CSSValueStylistic) {
                if (!parseAndSetArgument(result.valuesRef().stylistic))
                    return false;
            } else if (token.functionId() == CSSValueStyleset) {
                if (!parseListAndSetArgument(result.valuesRef().styleset))
                    return false;
            } else if (token.functionId() == CSSValueCharacterVariant) {
                if (!parseListAndSetArgument(result.valuesRef().characterVariant))
                    return false;
            } else if (token.functionId() == CSSValueOrnaments) {
                if (!parseAndSetArgument(result.valuesRef().ornaments))
                    return false;
            } else if (token.functionId() == CSSValueAnnotation) {
                if (!parseAndSetArgument(result.valuesRef().annotation))
                    return false;
            } else
                return hasParsedSomething;
        }
    };

    if (parseSomethingWithoutError())
        return CSSFontVariantAlternatesValue::create(WTFMove(result));

    return nullptr;
}

RefPtr<CSSValue> consumeFontVariantNumeric(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    CSSFontVariantNumericParser numericParser;
    do {
        if (numericParser.consumeNumeric(range) != CSSFontVariantNumericParser::ParseResult::ConsumedValue)
            return nullptr;
    } while (!range.atEnd());

    return numericParser.finalizeValue();
}

RefPtr<CSSValue> consumeFontWeight(CSSParserTokenRange& range)
{
    if (auto result = consumeFontWeightRaw(range)) {
        return WTF::switchOn(*result, [] (CSSValueID valueID) {
            return CSSPrimitiveValue::create(valueID);
        }, [] (double weightNumber) {
            return CSSPrimitiveValue::create(weightNumber, CSSUnitType::CSS_NUMBER);
        });
    }
    return nullptr;
}

RefPtr<CSSValue> consumeFamilyName(CSSParserTokenRange& range)
{
    auto familyName = consumeFamilyNameRaw(range);
    if (familyName.isNull())
        return nullptr;
    return CSSValuePool::singleton().createFontFamilyValue(familyName);
}

static RefPtr<CSSValue> consumeGenericFamily(CSSParserTokenRange& range)
{
    return consumeIdentRange(range, CSSValueSerif, CSSValueWebkitBody);
}

RefPtr<CSSValue> consumeFontFamily(CSSParserTokenRange& range)
{
    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, [] (auto& range) -> RefPtr<CSSValue> {
        if (auto parsedValue = consumeGenericFamily(range))
            return parsedValue;
        return consumeFamilyName(range);
    });
}

static RefPtr<CSSValue> consumeCounter(CSSParserTokenRange& range, int defaultValue)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    do {
        RefPtr<CSSPrimitiveValue> counterName = consumeCustomIdent(range);
        if (!counterName)
            return nullptr;
        int i = defaultValue;
        if (auto counterValue = consumeIntegerRaw(range))
            i = *counterValue;
        list->append(createPrimitiveValuePair(counterName.releaseNonNull(), CSSPrimitiveValue::create(i, CSSUnitType::CSS_INTEGER), Pair::IdenticalValueEncoding::Coalesce));
    } while (!range.atEnd());
    return list;
}

RefPtr<CSSValue> consumeCounterIncrement(CSSParserTokenRange& range)
{
    return consumeCounter(range, 1);
}

RefPtr<CSSValue> consumeCounterReset(CSSParserTokenRange& range)
{
    return consumeCounter(range, 0);
}

static RefPtr<CSSValue> consumePageSize(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueA3, CSSValueA4, CSSValueA5, CSSValueB4, CSSValueB5, CSSValueLedger, CSSValueLegal, CSSValueLetter>(range);
}

RefPtr<CSSValue> consumeSize(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    RefPtr<CSSValueList> result = CSSValueList::createSpaceSeparated();

    if (range.peek().id() == CSSValueAuto) {
        result->append(consumeIdent(range).releaseNonNull());
        return result;
    }

    if (RefPtr<CSSValue> width = consumeLength(range, cssParserMode, ValueRange::NonNegative)) {
        RefPtr<CSSValue> height = consumeLength(range, cssParserMode, ValueRange::NonNegative);
        result->append(width.releaseNonNull());
        if (height)
            result->append(height.releaseNonNull());
        return result;
    }

    RefPtr<CSSValue> pageSize = consumePageSize(range);
    RefPtr<CSSValue> orientation = consumeIdent<CSSValuePortrait, CSSValueLandscape>(range);
    if (!pageSize)
        pageSize = consumePageSize(range);

    if (!orientation && !pageSize)
        return nullptr;
    if (pageSize)
        result->append(pageSize.releaseNonNull());
    if (orientation)
        result->append(orientation.releaseNonNull());
    return result;
}

RefPtr<CSSValue> consumeTextIndent(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    // [ <length> | <percentage> ] && hanging? && each-line?
    RefPtr<CSSValue> lengthOrPercentage;
    RefPtr<CSSPrimitiveValue> eachLine;
    RefPtr<CSSPrimitiveValue> hanging;

    do {
        if (!lengthOrPercentage) {
            if (RefPtr<CSSValue> textIndent = consumeLengthOrPercent(range, cssParserMode, ValueRange::All, UnitlessQuirk::Allow)) {
                lengthOrPercentage = textIndent;
                continue;
            }
        }

        CSSValueID id = range.peek().id();
        if (!eachLine && id == CSSValueEachLine) {
            eachLine = consumeIdent(range);
            continue;
        }

        if (!hanging && id == CSSValueHanging) {
            hanging = consumeIdent(range);
            continue;
        }

        return nullptr;
    } while (!range.atEnd());

    if (!lengthOrPercentage)
        return nullptr;

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(*lengthOrPercentage);
    if (hanging)
        list->append(hanging.releaseNonNull());
    if (eachLine)
        list->append(eachLine.releaseNonNull());

    return list;
}

static bool validWidthOrHeightKeyword(CSSValueID id)
{
    switch (id) {
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
}

static RefPtr<CSSValue> consumeAutoOrLengthOrPercent(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRange::All, unitless);
}

RefPtr<CSSValue> consumeMarginSide(CSSParserTokenRange& range, CSSPropertyID currentShorthand, CSSParserMode cssParserMode)
{
    UnitlessQuirk unitless = currentShorthand != CSSPropertyInset ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
    return consumeAutoOrLengthOrPercent(range, cssParserMode, unitless);
}

RefPtr<CSSValue> consumeMarginTrim(CSSParserTokenRange& range)
{
    auto firstValue = range.peek().id();
    if (firstValue == CSSValueBlock || firstValue == CSSValueInline || firstValue == CSSValueNone)
        return consumeIdent(range).releaseNonNull();

    auto list = CSSValueList::createSpaceSeparated();
    while (true) {
        auto ident = consumeIdent<CSSValueBlockStart, CSSValueBlockEnd, CSSValueInlineStart, CSSValueInlineEnd>(range);
        if (!ident)
            break;
        if (list->hasValue(*ident))
            return nullptr;
        list->append(ident.releaseNonNull());
    }

    // Try to serialize into either block or inline form
    if (list->size() == 2) {
        if (list->hasValue(CSSValueBlockStart) && list->hasValue(CSSValueBlockEnd))
            return CSSPrimitiveValue::create(CSSValueBlock);
        if (list->hasValue(CSSValueInlineStart) && list->hasValue(CSSValueInlineEnd))
            return CSSPrimitiveValue::create(CSSValueInline);
    }
    return list;
}

RefPtr<CSSValue> consumeSide(CSSParserTokenRange& range, CSSPropertyID currentShorthand, CSSParserMode cssParserMode)
{
    UnitlessQuirk unitless = currentShorthand != CSSPropertyInset ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
    return consumeAutoOrLengthOrPercent(range, cssParserMode, unitless);
}

static RefPtr<CSSPrimitiveValue> consumeClipComponent(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLength(range, cssParserMode, ValueRange::All, UnitlessQuirk::Allow);
}

RefPtr<CSSValue> consumeClip(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    if (range.peek().functionId() != CSSValueRect)
        return nullptr;

    CSSParserTokenRange args = consumeFunction(range);
    // rect(t, r, b, l) || rect(t r b l)
    RefPtr<CSSPrimitiveValue> top = consumeClipComponent(args, cssParserMode);
    if (!top)
        return nullptr;
    bool needsComma = consumeCommaIncludingWhitespace(args);
    RefPtr<CSSPrimitiveValue> right = consumeClipComponent(args, cssParserMode);
    if (!right || (needsComma && !consumeCommaIncludingWhitespace(args)))
        return nullptr;
    RefPtr<CSSPrimitiveValue> bottom = consumeClipComponent(args, cssParserMode);
    if (!bottom || (needsComma && !consumeCommaIncludingWhitespace(args)))
        return nullptr;
    RefPtr<CSSPrimitiveValue> left = consumeClipComponent(args, cssParserMode);
    if (!left || !args.atEnd())
        return nullptr;
    
    auto rect = Rect::create();
    rect->setLeft(left.releaseNonNull());
    rect->setTop(top.releaseNonNull());
    rect->setRight(right.releaseNonNull());
    rect->setBottom(bottom.releaseNonNull());
    return CSSPrimitiveValue::create(WTFMove(rect));
}

RefPtr<CSSValue> consumeTouchAction(CSSParserTokenRange& range)
{
    if (auto ident = consumeIdent<CSSValueNone, CSSValueAuto, CSSValueManipulation>(range))
        return ident;

    auto list = CSSValueList::createSpaceSeparated();
    while (true) {
        auto ident = consumeIdent<CSSValuePanX, CSSValuePanY, CSSValuePinchZoom>(range);
        if (!ident)
            break;
        if (list->hasValue(*ident))
            return nullptr;
        list->append(ident.releaseNonNull());
    }

    if (!list->length())
        return nullptr;
    return list;
}

RefPtr<CSSValue> consumeKeyframesName(CSSParserTokenRange& range, const CSSParserContext&)
{
    // https://www.w3.org/TR/css-animations-1/#typedef-keyframes-name
    //
    // <keyframes-name> = <custom-ident> | <string>

    if (range.peek().type() == StringToken) {
        const CSSParserToken& token = range.consumeIncludingWhitespace();
        if (equalLettersIgnoringASCIICase(token.value(), "none"_s))
            return CSSPrimitiveValue::create(CSSValueNone);
        return CSSPrimitiveValue::create(token.value().toString(), CSSUnitType::CSS_STRING);
    }

    return consumeCustomIdent(range);
}

static RefPtr<CSSValue> consumeSingleTransitionPropertyIdent(CSSParserTokenRange& range, const CSSParserToken& token)
{
    if (auto property = token.parseAsCSSPropertyID()) {
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(property);
    }
    return consumeCustomIdent(range);
}

RefPtr<CSSValue> consumeSingleTransitionPropertyOrNone(CSSParserTokenRange& range)
{
    // This variant of consumeSingleTransitionProperty is used for the slightly different
    // parse rules used for the 'transition' shorthand which allows 'none':
    //
    // <single-transition> = [ none | <single-transition-property> ] || <time> || <timing-function> || <time>

    auto& token = range.peek();
    if (token.type() != IdentToken)
        return nullptr;
    if (token.id() == CSSValueNone)
        return consumeIdent(range);

    return consumeSingleTransitionPropertyIdent(range, token);
}

RefPtr<CSSValue> consumeSingleTransitionProperty(CSSParserTokenRange& range, const CSSParserContext&)
{
    // https://www.w3.org/TR/css-transitions-1/#single-transition-property
    //
    // <single-transition-property> = all | <custom-ident>;
    //
    // "The <custom-ident> production in <single-transition-property> also excludes the keyword
    //  none, in addition to the keywords always excluded from <custom-ident>."

    auto& token = range.peek();
    if (token.type() != IdentToken)
        return nullptr;
    if (token.id() == CSSValueNone)
        return nullptr;

    return consumeSingleTransitionPropertyIdent(range, token);
}

static RefPtr<CSSValue> consumeSteps(CSSParserTokenRange& range)
{
    // https://drafts.csswg.org/css-easing-1/#funcdef-step-easing-function-steps

    ASSERT(range.peek().functionId() == CSSValueSteps);
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    
    auto stepsValue = consumePositiveIntegerRaw(args);
    if (!stepsValue)
        return nullptr;
    
    std::optional<StepsTimingFunction::StepPosition> stepPosition;
    if (consumeCommaIncludingWhitespace(args)) {
        switch (args.consumeIncludingWhitespace().id()) {
        case CSSValueJumpStart:
            stepPosition = StepsTimingFunction::StepPosition::JumpStart;
            break;

        case CSSValueJumpEnd:
            stepPosition = StepsTimingFunction::StepPosition::JumpEnd;
            break;

        case CSSValueJumpNone:
            stepPosition = StepsTimingFunction::StepPosition::JumpNone;
            break;

        case CSSValueJumpBoth:
            stepPosition = StepsTimingFunction::StepPosition::JumpBoth;
            break;

        case CSSValueStart:
            stepPosition = StepsTimingFunction::StepPosition::Start;
            break;

        case CSSValueEnd:
            stepPosition = StepsTimingFunction::StepPosition::End;
            break;

        default:
            return nullptr;
        }
    }
    
    if (!args.atEnd())
        return nullptr;

    if (*stepsValue == 1 && stepPosition == StepsTimingFunction::StepPosition::JumpNone)
        return nullptr;

    range = rangeCopy;
    return CSSStepsTimingFunctionValue::create(*stepsValue, stepPosition);
}

static RefPtr<CSSValue> consumeCubicBezier(CSSParserTokenRange& range)
{
    ASSERT(range.peek().functionId() == CSSValueCubicBezier);
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);

    auto x1 = consumeNumberRaw(args);
    if (!x1 || x1->value < 0 || x1->value > 1)
        return nullptr;
    
    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto y1 = consumeNumberRaw(args);
    if (!y1)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto x2 = consumeNumberRaw(args);
    if (!x2 || x2->value < 0 || x2->value > 1)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto y2 = consumeNumberRaw(args);
    if (!y2)
        return nullptr;

    if (!args.atEnd())
        return nullptr;

    range = rangeCopy;
    return CSSCubicBezierTimingFunctionValue::create(x1->value, y1->value, x2->value, y2->value);
}

static RefPtr<CSSValue> consumeSpringFunction(CSSParserTokenRange& range)
{
    ASSERT(range.peek().functionId() == CSSValueSpring);
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);

    // Mass must be greater than 0.
    auto mass = consumeNumberRaw(args);
    if (!mass || mass->value <= 0)
        return nullptr;
    
    // Stiffness must be greater than 0.
    auto stiffness = consumeNumberRaw(args);
    if (!stiffness || stiffness->value <= 0)
        return nullptr;
    
    // Damping coefficient must be greater than or equal to 0.
    auto damping = consumeNumberRaw(args);
    if (!damping || damping->value < 0)
        return nullptr;
    
    // Initial velocity may have any value.
    auto initialVelocity = consumeNumberRaw(args);
    if (!initialVelocity)
        return nullptr;

    if (!args.atEnd())
        return nullptr;

    range = rangeCopy;

    return CSSSpringTimingFunctionValue::create(mass->value, stiffness->value, damping->value, initialVelocity->value);
}

RefPtr<CSSValue> consumeTimingFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    switch (range.peek().id()) {
    case CSSValueLinear:
    case CSSValueEase:
    case CSSValueEaseIn:
    case CSSValueEaseOut:
    case CSSValueEaseInOut:
        return consumeIdent(range);

    case CSSValueStepStart:
        range.consumeIncludingWhitespace();
        return CSSStepsTimingFunctionValue::create(1, StepsTimingFunction::StepPosition::Start);

    case CSSValueStepEnd:
        range.consumeIncludingWhitespace();
        return CSSStepsTimingFunctionValue::create(1, StepsTimingFunction::StepPosition::End);

    default:
        break;
    }

    CSSValueID function = range.peek().functionId();
    if (function == CSSValueCubicBezier)
        return consumeCubicBezier(range);
    if (function == CSSValueSteps)
        return consumeSteps(range);
    if (context.springTimingFunctionEnabled && function == CSSValueSpring)
        return consumeSpringFunction(range);
    return nullptr;
}

static RefPtr<CSSValue> consumeShadow(CSSParserTokenRange& range, const CSSParserContext& context, bool isBoxShadowProperty)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, consumeSingleShadow, context, isBoxShadowProperty, isBoxShadowProperty);
}

RefPtr<CSSValue> consumeTextShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeShadow(range, context, false);
}

RefPtr<CSSValue> consumeBoxShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeShadow(range, context, true);
}

RefPtr<CSSValue> consumeWebkitBoxShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeShadow(range, context, true);
}

RefPtr<CSSValue> consumeTextDecorationLine(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    auto list = CSSValueList::createSpaceSeparated();
    while (true) {
        auto ident = consumeIdent<CSSValueBlink, CSSValueUnderline, CSSValueOverline, CSSValueLineThrough>(range);
        if (!ident)
            break;
        if (list->hasValue(*ident))
            return nullptr;
        list->append(ident.releaseNonNull());
    }

    if (!list->length())
        return nullptr;
    return list;
}

RefPtr<CSSValue> consumeTextEmphasisStyle(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    if (RefPtr<CSSValue> textEmphasisStyle = consumeString(range))
        return textEmphasisStyle;

    RefPtr<CSSPrimitiveValue> fill = consumeIdent<CSSValueFilled, CSSValueOpen>(range);
    RefPtr<CSSPrimitiveValue> shape = consumeIdent<CSSValueDot, CSSValueCircle, CSSValueDoubleCircle, CSSValueTriangle, CSSValueSesame>(range);
    if (!fill)
        fill = consumeIdent<CSSValueFilled, CSSValueOpen>(range);
    if (fill && shape) {
        RefPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();
        parsedValues->append(fill.releaseNonNull());
        parsedValues->append(shape.releaseNonNull());
        return parsedValues;
    }
    if (fill)
        return fill;
    if (shape)
        return shape;
    return nullptr;
}

RefPtr<CSSValue> consumeBorderWidth(CSSParserTokenRange& range, CSSPropertyID currentShorthand, const CSSParserContext& context)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueThin || id == CSSValueMedium || id == CSSValueThick)
        return consumeIdent(range);

    bool allowQuirkyLengths = (context.mode == HTMLQuirksMode) && (currentShorthand == CSSPropertyInvalid || currentShorthand == CSSPropertyBorderWidth);
    UnitlessQuirk unitless = allowQuirkyLengths ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
    return consumeLength(range, context.mode, ValueRange::NonNegative, unitless);
}

RefPtr<CSSValue> consumeBorderColor(CSSParserTokenRange& range, CSSPropertyID currentShorthand, const CSSParserContext& context)
{
    bool allowQuirkyColors = (context.mode == HTMLQuirksMode) && (currentShorthand == CSSPropertyInvalid || currentShorthand == CSSPropertyBorderColor);
    return consumeColor(range, context, allowQuirkyColors);
}

static bool consumeTranslate3d(CSSParserTokenRange& args, CSSParserMode cssParserMode, RefPtr<CSSFunctionValue>& transformValue)
{
    unsigned numberOfArguments = 2;
    RefPtr<CSSValue> parsedValue;
    do {
        parsedValue = consumeLengthOrPercent(args, cssParserMode, ValueRange::All);
        if (!parsedValue)
            return false;
        transformValue->append(*parsedValue);
        if (!consumeCommaIncludingWhitespace(args))
            return false;
    } while (--numberOfArguments);
    parsedValue = consumeLength(args, cssParserMode, ValueRange::All);
    if (!parsedValue)
        return false;
    transformValue->append(*parsedValue);
    return true;
}

static bool consumeNumbers(CSSParserTokenRange& args, RefPtr<CSSFunctionValue>& transformValue, unsigned numberOfArguments)
{
    do {
        RefPtr<CSSPrimitiveValue> parsedValue = consumeNumber(args, ValueRange::All);
        if (!parsedValue)
            return false;
        transformValue->append(parsedValue.releaseNonNull());
        if (--numberOfArguments && !consumeCommaIncludingWhitespace(args))
            return false;
    } while (numberOfArguments);
    return true;
}

static bool consumeNumbersOrPercents(CSSParserTokenRange& args, RefPtr<CSSFunctionValue>& transformValue, unsigned numberOfArguments)
{
    auto parseNumberAndAppend = [&] {
        auto parsedValue = consumeNumberOrPercent(args, ValueRange::All);
        if (!parsedValue)
            return false;

        transformValue->append(parsedValue.releaseNonNull());
        --numberOfArguments;
        return true;
    };

    if (!parseNumberAndAppend())
        return false;

    while (numberOfArguments) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;

        if (!parseNumberAndAppend())
            return false;
    }

    return true;
}

static bool consumePerspectiveFunctionArgument(CSSParserTokenRange& range, const CSSParserContext& context, RefPtr<CSSFunctionValue>& transformValue)
{
    if (auto perspective = CSSPropertyParsing::consumePerspective(range, context)) {
        transformValue->append(perspective.releaseNonNull());
        return true;
    }

    if (auto perspective = consumeNumberRaw(range, ValueRange::NonNegative)) {
        transformValue->append(CSSPrimitiveValue::create(perspective->value, CSSUnitType::CSS_PX));
        return true;
    }

    return false;
}

RefPtr<CSSValue> consumeTransformFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSValueID functionId = range.peek().functionId();
    if (functionId == CSSValueInvalid)
        return nullptr;
    CSSParserTokenRange args = consumeFunction(range);
    if (args.atEnd())
        return nullptr;
    
    RefPtr<CSSFunctionValue> transformValue = CSSFunctionValue::create(functionId);
    RefPtr<CSSValue> parsedValue;
    switch (functionId) {
    case CSSValueRotate:
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
    case CSSValueSkewX:
    case CSSValueSkewY:
    case CSSValueSkew:
        parsedValue = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
        if (!parsedValue)
            return nullptr;
        if (functionId == CSSValueSkew && consumeCommaIncludingWhitespace(args)) {
            transformValue->append(*parsedValue);
            parsedValue = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
            if (!parsedValue)
                return nullptr;
        }
        break;
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
    case CSSValueScale:
        parsedValue = consumeNumberOrPercent(args, ValueRange::All);
        if (!parsedValue)
            return nullptr;
        if (functionId == CSSValueScale && consumeCommaIncludingWhitespace(args)) {
            transformValue->append(*parsedValue);
            parsedValue = consumeNumberOrPercent(args, ValueRange::All);
            if (!parsedValue)
                return nullptr;
        }
        break;
    case CSSValuePerspective:
        if (!consumePerspectiveFunctionArgument(args, context, transformValue))
            return nullptr;
        break;
    case CSSValueTranslateX:
    case CSSValueTranslateY:
    case CSSValueTranslate:
        parsedValue = consumeLengthOrPercent(args, context.mode, ValueRange::All);
        if (!parsedValue)
            return nullptr;
        if (functionId == CSSValueTranslate && consumeCommaIncludingWhitespace(args)) {
            transformValue->append(*parsedValue);
            parsedValue = consumeLengthOrPercent(args, context.mode, ValueRange::All);
            if (!parsedValue)
                return nullptr;
            if (is<CSSPrimitiveValue>(parsedValue)) {
                auto isZero = downcast<CSSPrimitiveValue>(*parsedValue).isZero();
                if (isZero && *isZero)
                    parsedValue = nullptr;
            }
        }
        break;
    case CSSValueTranslateZ:
        parsedValue = consumeLength(args, context.mode, ValueRange::All);
        break;
    case CSSValueMatrix:
    case CSSValueMatrix3d:
        if (!consumeNumbers(args, transformValue, (functionId == CSSValueMatrix3d) ? 16 : 6))
            return nullptr;
        break;
    case CSSValueScale3d:
        if (!consumeNumbersOrPercents(args, transformValue, 3))
            return nullptr;
        break;
    case CSSValueRotate3d:
        if (!consumeNumbers(args, transformValue, 3) || !consumeCommaIncludingWhitespace(args))
            return nullptr;
        parsedValue = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
        if (!parsedValue)
            return nullptr;
        break;
    case CSSValueTranslate3d:
        if (!consumeTranslate3d(args, context.mode, transformValue))
            return nullptr;
        break;
    default:
        return nullptr;
    }
    if (parsedValue)
        transformValue->append(*parsedValue);
    if (!args.atEnd())
        return nullptr;
    return transformValue;
}

RefPtr<CSSValue> consumeTransform(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    RefPtr<CSSTransformListValue> list = CSSTransformListValue::create();
    do {
        auto parsedTransformValue = consumeTransformFunction(range, context);
        if (!parsedTransformValue)
            return nullptr;
        list->append(parsedTransformValue.releaseNonNull());
    } while (!range.atEnd());

    return list;
}

RefPtr<CSSValue> consumeTranslate(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    // https://drafts.csswg.org/css-transforms-2/#propdef-translate
    //
    // The translate property accepts 1-3 values, each specifying a translation against one axis, in the order X, Y, then Z.
    // If only one or two values are given, this specifies a 2d translation, equivalent to the translate() function. If the second
    // value is missing, it defaults to 0px. If three values are given, this specifies a 3d translation, equivalent to the
    // translate3d() function.

    RefPtr<CSSValue> x = consumeLengthOrPercent(range, cssParserMode, ValueRange::All);
    if (!x)
        return list;

    // If we got this far we have a valid x value, so we can directly add it to the list.
    list->append(*x);

    range.consumeWhitespace();
    RefPtr<CSSValue> y = consumeLengthOrPercent(range, cssParserMode, ValueRange::All);
    if (!y)
        return list;

    // If we have a calc() or non-zero y value, we can directly add it to the list. We only
    // want to add a zero y value if a non-zero z value is specified.
    // Always include 0% in serialization per-spec.
    if (is<CSSPrimitiveValue>(y)) {
        auto& yPrimitiveValue = downcast<CSSPrimitiveValue>(*y);
        if (yPrimitiveValue.isCalculated() || yPrimitiveValue.isPercentage() || !*yPrimitiveValue.isZero())
            list->append(*y);
    }

    range.consumeWhitespace();
    RefPtr<CSSValue> z = consumeLength(range, cssParserMode, ValueRange::All);

    if (is<CSSPrimitiveValue>(z)) {
        auto& zPrimitiveValue = downcast<CSSPrimitiveValue>(*z);
        // If the z value is a zero value and not a percent value, we have nothing left to add to the list.
        if (!zPrimitiveValue.isCalculated() && !zPrimitiveValue.isPercentage() && *zPrimitiveValue.isZero())
            return list;
        // Add the zero value for y if we did not already add a y value.
        if (list->length() == 1)
            list->append(*y);
        list->append(*z);
    }

    return list;
}

RefPtr<CSSValue> consumeScale(CSSParserTokenRange& range, CSSParserMode)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    // https://www.w3.org/TR/css-transforms-2/#propdef-scale
    //
    // The scale property accepts 1-3 values, each specifying a scale along one axis, in order X, Y, then Z.
    //
    // If only the X value is given, the Y value defaults to the same value.
    //
    // If one or two values are given, this specifies a 2d scaling, equivalent to the scale() function.
    // If three values are given, this specifies a 3d scaling, equivalent to the scale3d() function.

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    RefPtr<CSSValue> x = consumeNumberOrPercent(range, ValueRange::All);
    if (!x)
        return list;
    list->append(*x);
    range.consumeWhitespace();

    RefPtr<CSSValue> y = consumeNumberOrPercent(range, ValueRange::All);
    if (!y)
        return list;

    // If the x and y values are the same, the y value is not needed.
    if (downcast<CSSPrimitiveValue>(*x).doubleValue() != downcast<CSSPrimitiveValue>(*y).doubleValue())
        list->append(*y);
    range.consumeWhitespace();

    RefPtr<CSSValue> z = consumeNumberOrPercent(range, ValueRange::All);
    if (!z)
        return list;
    if (downcast<CSSPrimitiveValue>(*z).doubleValue() != 1.0) {
        // We only need to append the z value if it's set to something other than 1.
        // In case y was not added yet, because it was equal to x, we must append it
        // prior to appending z.
        if (list->length() == 1)
            list->append(*y);
        list->append(*z);
    }

    return list;
}

RefPtr<CSSValue> consumeRotate(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    // https://www.w3.org/TR/css-transforms-2/#propdef-rotate
    //
    // The rotate property accepts an angle to rotate an element, and optionally an axis to rotate it around.
    //
    // If the axis is omitted, this specifies a 2d rotation, equivalent to the rotate() function.
    //
    // Otherwise, it specifies a 3d rotation: if x, y, or z is given, it specifies a rotation around that axis,
    // equivalent to the rotateX()/etc 3d transform functions. Alternately, the axis can be specified explicitly
    // by giving three numbers representing the x, y, and z components of an origin-centered vector, equivalent
    // to the rotate3d() function.

    RefPtr<CSSValue> angle;
    RefPtr<CSSValue> axisIdentifier;

    while (!range.atEnd()) {
        // First, attempt to parse a number, which might be in a series of 3 specifying the rotation axis.
        RefPtr<CSSValue> parsedValue = consumeNumber(range, ValueRange::All);
        if (parsedValue) {
            // If we've encountered an axis identifier, then this valus is invalid.
            if (axisIdentifier)
                return nullptr;
            list->append(*parsedValue);
            range.consumeWhitespace();
            continue;
        }

        // Then, attempt to parse an angle. We try this as a fallback rather than the first option because
        // a unitless 0 angle would be consumed as an angle.
        parsedValue = consumeAngle(range, cssParserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
        if (parsedValue) {
            // If we had already parsed an angle or numbers but not 3 in a row, this value is invalid.
            if (angle || (list->length() && list->length() != 3))
                return nullptr;
            angle = parsedValue;
            range.consumeWhitespace();
            continue;
        }

        // Finally, attempt to parse one of the axis identifiers.
        parsedValue = consumeIdent<CSSValueX, CSSValueY, CSSValueZ>(range);
        // If we failed to find one of those identifiers or one was already specified, or we'd previously
        // encountered numbers to specify a rotation axis, then this value is invalid.
        if (!parsedValue || axisIdentifier || list->length())
            return nullptr;
        axisIdentifier = parsedValue;
        range.consumeWhitespace();
    }

    // We must have an angle to have a valid value.
    if (!angle)
        return nullptr;

    if (list->length() == 3) {
        // The first valid case is if we have 3 items in the list, meaning we parsed three consecutive number values
        // to specify the rotation axis. In that case, we must not also have encountered an axis identifier.
        ASSERT(!axisIdentifier);

        // No we must check the values since if we have a vector in the x, y or z axis alone we must serialize to the
        // matching identifier.
        auto x = downcast<CSSPrimitiveValue>(*list->itemWithoutBoundsCheck(0)).doubleValue();
        auto y = downcast<CSSPrimitiveValue>(*list->itemWithoutBoundsCheck(1)).doubleValue();
        auto z = downcast<CSSPrimitiveValue>(*list->itemWithoutBoundsCheck(2)).doubleValue();

        if (x && !y && !z) {
            list = CSSValueList::createSpaceSeparated();
            list->append(CSSPrimitiveValue::create(CSSValueX));
        } else if (!x && y && !z) {
            list = CSSValueList::createSpaceSeparated();
            list->append(CSSPrimitiveValue::create(CSSValueY));
        } else if (!x && !y && z)
            list = CSSValueList::createSpaceSeparated();

        // Finally, we must append the angle.
        list->append(*angle);
    } else if (!list->length()) {
        // The second valid case is if we have no item in the list, meaning we have either an optional rotation axis
        // using an identifier. In that case, we must add the axis identifier is specified and then add the angle.
        if (is<CSSPrimitiveValue>(axisIdentifier) && downcast<CSSPrimitiveValue>(*axisIdentifier).valueID() != CSSValueZ)
            list->append(*axisIdentifier);
        list->append(*angle);
    } else
        return nullptr;

    return list;
}

RefPtr<CSSValue> consumePositionX(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeSingleAxisPosition(range, context.mode, BoxOrient::Horizontal);
}

RefPtr<CSSValue> consumePositionY(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeSingleAxisPosition(range, context.mode, BoxOrient::Vertical);
}

RefPtr<CSSValue> consumePaintStroke(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    auto url = consumeURL(range);
    if (url) {
        RefPtr<CSSValue> parsedValue;
        if (range.peek().id() == CSSValueNone)
            parsedValue = consumeIdent(range);
        else
            parsedValue = consumeColor(range, context);
        if (parsedValue) {
            RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
            values->append(url.releaseNonNull());
            values->append(parsedValue.releaseNonNull());
            return values;
        }
        return url;
    }
    return consumeColor(range, context);
}

RefPtr<CSSValue> consumePaintOrder(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    Vector<CSSValueID, 3> paintTypeList;
    RefPtr<CSSPrimitiveValue> fill;
    RefPtr<CSSPrimitiveValue> stroke;
    RefPtr<CSSPrimitiveValue> markers;
    do {
        CSSValueID id = range.peek().id();
        if (id == CSSValueFill && !fill)
            fill = consumeIdent(range);
        else if (id == CSSValueStroke && !stroke)
            stroke = consumeIdent(range);
        else if (id == CSSValueMarkers && !markers)
            markers = consumeIdent(range);
        else
            return nullptr;
        paintTypeList.append(id);
    } while (!range.atEnd());

    // After parsing we serialize the paint-order list. Since it is not possible to
    // pop a last list items from CSSValueList without bigger cost, we create the
    // list after parsing.
    CSSValueID firstPaintOrderType = paintTypeList.at(0);
    RefPtr<CSSValueList> paintOrderList = CSSValueList::createSpaceSeparated();
    switch (firstPaintOrderType) {
    case CSSValueFill:
    case CSSValueStroke:
        paintOrderList->append(firstPaintOrderType == CSSValueFill ? fill.releaseNonNull() : stroke.releaseNonNull());
        if (paintTypeList.size() > 1) {
            if (paintTypeList.at(1) == CSSValueMarkers)
                paintOrderList->append(markers.releaseNonNull());
        }
        break;
    case CSSValueMarkers:
        paintOrderList->append(markers.releaseNonNull());
        if (paintTypeList.size() > 1) {
            if (paintTypeList.at(1) == CSSValueStroke)
                paintOrderList->append(stroke.releaseNonNull());
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return paintOrderList;
}

bool isFlexBasisIdent(CSSValueID id)
{
    return identMatches<CSSValueAuto, CSSValueContent>(id) || validWidthOrHeightKeyword(id);
}

RefPtr<CSSValue> consumeStrokeDasharray(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    RefPtr<CSSValueList> dashes = CSSValueList::createCommaSeparated();
    do {
        RefPtr<CSSPrimitiveValue> dash = consumeLengthOrPercent(range, HTMLStandardMode, ValueRange::NonNegative, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
        if (!dash)
            dash = consumeNumber(range, ValueRange::NonNegative);
        if (!dash || (consumeCommaIncludingWhitespace(range) && range.atEnd()))
            return nullptr;
        dashes->append(dash.releaseNonNull());
    } while (!range.atEnd());
    return dashes;
}

RefPtr<CSSValue> consumeCursor(CSSParserTokenRange& range, const CSSParserContext& context, bool inQuirksMode)
{
    RefPtr<CSSValueList> list;
    while (auto image = consumeImage(range, context, { AllowedImageType::URLFunction, AllowedImageType::ImageSet })) {
        std::optional<IntPoint> hotSpot;
        if (auto x = consumeNumberRaw(range)) {
            auto y = consumeNumberRaw(range);
            if (!y)
                return nullptr;
            // FIXME: Should we clamp or round instead of just casting from double to int?
            hotSpot = IntPoint { static_cast<int>(x->value), static_cast<int>(y->value) };
        }

        if (!list)
            list = CSSValueList::createCommaSeparated();

        list->append(CSSCursorImageValue::create(image.releaseNonNull(), hotSpot, context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No));
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    CSSValueID id = range.peek().id();
    RefPtr<CSSValue> cursorType;
    if (id == CSSValueHand) {
        if (!inQuirksMode) // Non-standard behavior
            return nullptr;
        cursorType = CSSPrimitiveValue::create(CSSValuePointer);
        range.consumeIncludingWhitespace();
    } else if ((id >= CSSValueAuto && id <= CSSValueWebkitZoomOut) || id == CSSValueCopy || id == CSSValueNone)
        cursorType = consumeIdent(range);
    else
        return nullptr;

    if (!list)
        return cursorType;
    list->append(cursorType.releaseNonNull());
    return list;
}

RefPtr<CSSValue> consumeAttr(CSSParserTokenRange args, const CSSParserContext& context)
{
    if (args.peek().type() != IdentToken)
        return nullptr;
    
    CSSParserToken token = args.consumeIncludingWhitespace();
    AtomString attrName;
    if (context.isHTMLDocument)
        attrName = token.value().convertToASCIILowercaseAtom();
    else
        attrName = token.value().toAtomString();

    if (!args.atEnd())
        return nullptr;

    // FIXME: Consider moving to a CSSFunctionValue with a custom-ident rather than a special CSS_ATTR primitive value.
    return CSSPrimitiveValue::create(attrName, CSSUnitType::CSS_ATTR);
}

static RefPtr<CSSPrimitiveValue> consumeCounterContent(CSSParserTokenRange args, bool counters)
{
    AtomString identifier { consumeCustomIdentRaw(args) };
    if (identifier.isNull())
        return nullptr;

    AtomString separator;
    if (counters) {
        if (!consumeCommaIncludingWhitespace(args) || args.peek().type() != StringToken)
            return nullptr;
        separator = args.consumeIncludingWhitespace().value().toAtomString();
    }

    auto listStyle = CSSValueDecimal;
    if (consumeCommaIncludingWhitespace(args)) {
        listStyle = args.peek().id();
        if (listStyle != CSSValueNone && !isPredefinedCounterStyle(listStyle))
            return nullptr;
        args.consumeIncludingWhitespace();
    }

    if (!args.atEnd())
        return nullptr;

    return CSSPrimitiveValue::create(Counter::create(WTFMove(identifier), WTFMove(separator), listStyle));
}

RefPtr<CSSValue> consumeContent(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (identMatches<CSSValueNone, CSSValueNormal>(range.peek().id()))
        return consumeIdent(range);

    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();

    do {
        RefPtr<CSSValue> parsedValue = consumeImage(range, context);
        if (!parsedValue)
            parsedValue = consumeIdent<CSSValueOpenQuote, CSSValueCloseQuote, CSSValueNoOpenQuote, CSSValueNoCloseQuote>(range);
        if (!parsedValue)
            parsedValue = consumeString(range);
        if (!parsedValue) {
            if (range.peek().functionId() == CSSValueAttr)
                parsedValue = consumeAttr(consumeFunction(range), context);
            else if (range.peek().functionId() == CSSValueCounter)
                parsedValue = consumeCounterContent(consumeFunction(range), false);
            else if (range.peek().functionId() == CSSValueCounters)
                parsedValue = consumeCounterContent(consumeFunction(range), true);
            if (!parsedValue)
                return nullptr;
        }
        values->append(parsedValue.releaseNonNull());
    } while (!range.atEnd());

    return values;
}

RefPtr<CSSValue> consumeScrollSnapAlign(CSSParserTokenRange& range)
{
    auto firstValue = consumeIdent<CSSValueNone, CSSValueStart, CSSValueCenter, CSSValueEnd>(range);
    if (!firstValue)
        return nullptr;

    auto secondValue = consumeIdent<CSSValueNone, CSSValueStart, CSSValueCenter, CSSValueEnd>(range);
    bool shouldAddSecondValue = secondValue && !secondValue->equals(*firstValue);

    RefPtr<CSSValueList> alignmentValue = CSSValueList::createSpaceSeparated();
    alignmentValue->append(firstValue.releaseNonNull());

    // Only add the second value if it differs from the first so that we produce the canonical
    // serialization of this CSSValueList.
    if (shouldAddSecondValue)
        alignmentValue->append(secondValue.releaseNonNull());

    return alignmentValue;
}

RefPtr<CSSValue> consumeScrollSnapType(CSSParserTokenRange& range)
{
    auto typeValue = CSSValueList::createSpaceSeparated();

    auto firstValue = consumeIdent<CSSValueNone, CSSValueX, CSSValueY, CSSValueBlock, CSSValueInline, CSSValueBoth>(range);
    if (!firstValue)
        return nullptr;
    typeValue->append(firstValue.releaseNonNull());

    // We only add the second value if it is not the initial value as described in specification
    // so that serialization of this CSSValueList produces the canonical serialization.
    auto secondValue = consumeIdent<CSSValueProximity, CSSValueMandatory>(range);
    if (secondValue && secondValue->valueID() != CSSValueProximity)
        typeValue->append(secondValue.releaseNonNull());

    return typeValue;
}

RefPtr<CSSValue> consumeTextEdge(CSSParserTokenRange& range)
{
    auto typeValue = CSSValueList::createSpaceSeparated();
    if (range.peek().id() == CSSValueLeading) {
        typeValue->append(consumeIdent(range).releaseNonNull());
        return typeValue;
    }

    auto firstGroupValue = consumeIdent<CSSValueText, CSSValueCap, CSSValueEx, CSSValueIdeographic, CSSValueIdeographicInk>(range);
    if (!firstGroupValue)
        return nullptr;
    typeValue->append(firstGroupValue.releaseNonNull());

    auto secondGroupValue = consumeIdent<CSSValueText, CSSValueAlphabetic, CSSValueIdeographic, CSSValueIdeographicInk>(range);
    if (secondGroupValue)
        typeValue->append(secondGroupValue.releaseNonNull());

    return typeValue;
}

    
RefPtr<CSSValue> consumeBorderRadiusCorner(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    RefPtr<CSSPrimitiveValue> parsedValue1 = consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
    if (!parsedValue1)
        return nullptr;
    RefPtr<CSSPrimitiveValue> parsedValue2 = consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
    if (!parsedValue2)
        parsedValue2 = parsedValue1;
    return createPrimitiveValuePair(parsedValue1.releaseNonNull(), parsedValue2.releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce);
}

static RefPtr<CSSPrimitiveValue> consumeShapeRadius(CSSParserTokenRange& args, CSSParserMode cssParserMode)
{
    if (identMatches<CSSValueClosestSide, CSSValueFarthestSide>(args.peek().id()))
        return consumeIdent(args);
    return consumeLengthOrPercent(args, cssParserMode, ValueRange::NonNegative);
}

static RefPtr<CSSBasicShapeCircle> consumeBasicShapeCircle(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
    // circle( [<shape-radius>]? [at <position>]? )
    RefPtr<CSSBasicShapeCircle> shape = CSSBasicShapeCircle::create();
    if (RefPtr<CSSPrimitiveValue> radius = consumeShapeRadius(args, context.mode))
        shape->setRadius(radius.releaseNonNull());
    if (consumeIdent<CSSValueAt>(args)) {
        auto centerCoordinates = consumePositionCoordinates(args, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
        if (!centerCoordinates)
            return nullptr;
        shape->setCenterX(WTFMove(centerCoordinates->x));
        shape->setCenterY(WTFMove(centerCoordinates->y));
    }
    return shape;
}

static RefPtr<CSSBasicShapeEllipse> consumeBasicShapeEllipse(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
    // ellipse( [<shape-radius>{2}]? [at <position>]? )
    auto shape = CSSBasicShapeEllipse::create();
    if (auto radiusX = consumeShapeRadius(args, context.mode)) {
        auto radiusY = consumeShapeRadius(args, context.mode);
        if (!radiusY)
            return nullptr;
        shape->setRadiusX(radiusX.releaseNonNull());
        shape->setRadiusY(radiusY.releaseNonNull());
    }
    if (consumeIdent<CSSValueAt>(args)) {
        auto centerCoordinates = consumePositionCoordinates(args, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
        if (!centerCoordinates)
            return nullptr;
        shape->setCenterX(WTFMove(centerCoordinates->x));
        shape->setCenterY(WTFMove(centerCoordinates->y));
    }
    return shape;
}

static RefPtr<CSSBasicShapePolygon> consumeBasicShapePolygon(CSSParserTokenRange& args, const CSSParserContext& context)
{
    RefPtr<CSSBasicShapePolygon> shape = CSSBasicShapePolygon::create();
    if (identMatches<CSSValueEvenodd, CSSValueNonzero>(args.peek().id())) {
        shape->setWindRule(args.consumeIncludingWhitespace().id() == CSSValueEvenodd ? WindRule::EvenOdd : WindRule::NonZero);
        if (!consumeCommaIncludingWhitespace(args))
            return nullptr;
    }

    do {
        RefPtr<CSSPrimitiveValue> xLength = consumeLengthOrPercent(args, context.mode, ValueRange::All);
        if (!xLength)
            return nullptr;
        RefPtr<CSSPrimitiveValue> yLength = consumeLengthOrPercent(args, context.mode, ValueRange::All);
        if (!yLength)
            return nullptr;
        shape->appendPoint(xLength.releaseNonNull(), yLength.releaseNonNull());
    } while (consumeCommaIncludingWhitespace(args));
    return shape;
}

static RefPtr<CSSBasicShapePath> consumeBasicShapePath(CSSParserTokenRange& args)
{
    WindRule windRule = WindRule::NonZero;
    if (identMatches<CSSValueEvenodd, CSSValueNonzero>(args.peek().id())) {
        windRule = args.consumeIncludingWhitespace().id() == CSSValueEvenodd ? WindRule::EvenOdd : WindRule::NonZero;
        if (!consumeCommaIncludingWhitespace(args))
            return nullptr;
    }

    if (args.peek().type() != StringToken)
        return nullptr;
    
    auto byteStream = makeUnique<SVGPathByteStream>();
    if (!buildSVGPathByteStreamFromString(args.consumeIncludingWhitespace().value().toString(), *byteStream, UnalteredParsing))
        return nullptr;
    
    auto shape = CSSBasicShapePath::create(WTFMove(byteStream));
    shape->setWindRule(windRule);
    
    return shape;
}

static void complete4Sides(RefPtr<CSSPrimitiveValue> side[4])
{
    if (side[3])
        return;
    if (!side[2]) {
        if (!side[1])
            side[1] = side[0];
        side[2] = side[0];
    }
    side[3] = side[1];
}

bool consumeRadii(RefPtr<CSSPrimitiveValue> horizontalRadii[4], RefPtr<CSSPrimitiveValue> verticalRadii[4], CSSParserTokenRange& range, CSSParserMode cssParserMode, bool useLegacyParsing)
{
    unsigned i = 0;
    for (; i < 4 && !range.atEnd() && range.peek().type() != DelimiterToken; ++i) {
        horizontalRadii[i] = consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
        if (!horizontalRadii[i])
            return false;
    }
    if (!horizontalRadii[0])
        return false;
    if (range.atEnd()) {
        // Legacy syntax: -webkit-border-radius: l1 l2; is equivalent to border-radius: l1 / l2;
        if (useLegacyParsing && i == 2) {
            verticalRadii[0] = horizontalRadii[1];
            horizontalRadii[1] = nullptr;
        } else {
            complete4Sides(horizontalRadii);
            for (unsigned i = 0; i < 4; ++i)
                verticalRadii[i] = horizontalRadii[i];
            return true;
        }
    } else {
        if (!consumeSlashIncludingWhitespace(range))
            return false;
        for (i = 0; i < 4 && !range.atEnd(); ++i) {
            verticalRadii[i] = consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
            if (!verticalRadii[i])
                return false;
        }
        if (!verticalRadii[0] || !range.atEnd())
            return false;
    }
    complete4Sides(horizontalRadii);
    complete4Sides(verticalRadii);
    return true;
}

static RefPtr<CSSBasicShapeInset> consumeBasicShapeInset(CSSParserTokenRange& args, const CSSParserContext& context)
{
    RefPtr<CSSBasicShapeInset> shape = CSSBasicShapeInset::create();
    RefPtr<CSSPrimitiveValue> top = consumeLengthOrPercent(args, context.mode, ValueRange::All);
    if (!top)
        return nullptr;
    RefPtr<CSSPrimitiveValue> right = consumeLengthOrPercent(args, context.mode, ValueRange::All);
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;
    if (right) {
        bottom = consumeLengthOrPercent(args, context.mode, ValueRange::All);
        if (bottom)
            left = consumeLengthOrPercent(args, context.mode, ValueRange::All);
    }
    if (left)
        shape->updateShapeSize4Values(top.releaseNonNull(), right.releaseNonNull(), bottom.releaseNonNull(), left.releaseNonNull());
    else if (bottom)
        shape->updateShapeSize3Values(top.releaseNonNull(), right.releaseNonNull(), bottom.releaseNonNull());
    else if (right)
        shape->updateShapeSize2Values(top.releaseNonNull(), right.releaseNonNull());
    else
        shape->updateShapeSize1Value(top.releaseNonNull());

    if (consumeIdent<CSSValueRound>(args)) {
        RefPtr<CSSPrimitiveValue> horizontalRadii[4] = { nullptr };
        RefPtr<CSSPrimitiveValue> verticalRadii[4] = { nullptr };
        if (!consumeRadii(horizontalRadii, verticalRadii, args, context.mode, false))
            return nullptr;
        shape->setTopLeftRadius(createPrimitiveValuePair(horizontalRadii[0].releaseNonNull(), verticalRadii[0].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce));
        shape->setTopRightRadius(createPrimitiveValuePair(horizontalRadii[1].releaseNonNull(), verticalRadii[1].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce));
        shape->setBottomRightRadius(createPrimitiveValuePair(horizontalRadii[2].releaseNonNull(), verticalRadii[2].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce));
        shape->setBottomLeftRadius(createPrimitiveValuePair(horizontalRadii[3].releaseNonNull(), verticalRadii[3].releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce));
    }
    return shape;
}

static RefPtr<CSSPrimitiveValue> consumeBasicShape(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().type() != FunctionToken)
        return nullptr;
    CSSValueID id = range.peek().functionId();
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    
    // FIXME-NEWPARSER: CSSBasicShape should be a CSSValue, and shapes should not be primitive values.
    RefPtr<CSSBasicShape> shape;
    if (id == CSSValueCircle)
        shape = consumeBasicShapeCircle(args, context);
    else if (id == CSSValueEllipse)
        shape = consumeBasicShapeEllipse(args, context);
    else if (id == CSSValuePolygon)
        shape = consumeBasicShapePolygon(args, context);
    else if (id == CSSValueInset)
        shape = consumeBasicShapeInset(args, context);
    else if (id == CSSValuePath)
        shape = consumeBasicShapePath(args);
    if (!shape)
        return nullptr;
    range = rangeCopy;
    
    if (!args.atEnd())
        return nullptr;

    return CSSPrimitiveValue::create(shape.releaseNonNull());
}

static RefPtr<CSSValue> consumeBasicShapeOrBox(CSSParserTokenRange& range, const CSSParserContext& context)
{
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    bool shapeFound = false;
    bool boxFound = false;
    while (!range.atEnd() && !(shapeFound && boxFound)) {
        RefPtr<CSSValue> componentValue;
        if (range.peek().type() == FunctionToken && !shapeFound) {
            componentValue = consumeBasicShape(range, context);
            shapeFound = true;
        } else if (range.peek().type() == IdentToken && !boxFound) {
            // FIXME: The current Motion Path spec calls for this to be a <coord-box>, not a <geometry-box>, the difference being that the former does not contain "margin-box" as a valid term.
            // However, the spec also has a few examples using "margin-box", so there seems to be some abiguity to be resolved. Tracked at https://github.com/w3c/fxtf-drafts/issues/481.
            componentValue = CSSPropertyParsing::consumeGeometryBox(range);
            boxFound = true;
        }
        if (!componentValue)
            break;
        list->append(componentValue.releaseNonNull());
    }
    
    if (!list->length())
        return nullptr;
    
    return list;
}

// Parses the ray() definition as defined in https://drafts.fxtf.org/motion-1/#funcdef-offset-path-ray
// ray( [ <angle> && <size> && contain? ] )
static RefPtr<CSSRayValue> consumeRayShape(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().type() != FunctionToken || range.peek().functionId() != CSSValueRay)
        return nullptr;

    CSSParserTokenRange args = consumeFunction(range);

    RefPtr<CSSPrimitiveValue> angle;
    RefPtr<CSSPrimitiveValue> size;
    bool isContaining = false;
    while (!args.atEnd()) {
        if (!angle && (angle = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid)))
            continue;

        if (!size && (size = consumeIdent<CSSValueClosestSide, CSSValueClosestCorner, CSSValueFarthestSide, CSSValueFarthestCorner, CSSValueSides>(args)))
            continue;

        if (!isContaining && (isContaining = consumeIdent<CSSValueContain>(args)))
            continue;

        return nullptr;
    }

    // <angle> and <size> must be present.
    if (!angle || !size)
        return nullptr;

    return CSSRayValue::create(angle.releaseNonNull(), size.releaseNonNull(), isContaining);
}

// Consumes shapes accepted by clip-path and offset-path.
RefPtr<CSSValue> consumePathOperation(CSSParserTokenRange& range, const CSSParserContext& context, ConsumeRay consumeRay)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    if (auto url = consumeURL(range))
        return url;

    if (consumeRay == ConsumeRay::Include) {
        if (auto ray = consumeRayShape(range, context))
            return ray;
    }

    return consumeBasicShapeOrBox(range, context);
}

RefPtr<CSSValue> consumeShapeOutside(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (RefPtr<CSSValue> imageValue = consumeImageOrNone(range, context))
        return imageValue;
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (RefPtr<CSSValue> boxValue = consumeIdent<CSSValueContentBox, CSSValuePaddingBox, CSSValueBorderBox, CSSValueMarginBox>(range))
        list->append(boxValue.releaseNonNull());
    if (RefPtr<CSSPrimitiveValue> shapeValue = consumeBasicShape(range, context)) {
        if (shapeValue->shapeValue()->type() == CSSBasicShapeCircle::CSSBasicShapePathType)
            return nullptr;
        list->append(shapeValue.releaseNonNull());
        if (list->length() < 2) {
            if (RefPtr<CSSValue> boxValue = consumeIdent<CSSValueContentBox, CSSValuePaddingBox, CSSValueBorderBox, CSSValueMarginBox>(range))
                list->append(boxValue.releaseNonNull());
        }
    }
    if (!list->length())
        return nullptr;
    return list;
}

static bool isAuto(CSSValueID id)
{
    return identMatches<CSSValueAuto>(id);
}

static bool isNormalOrStretch(CSSValueID id)
{
    return identMatches<CSSValueNormal, CSSValueStretch>(id);
}

static bool isLeftOrRightKeyword(CSSValueID id)
{
    return identMatches<CSSValueLeft, CSSValueRight>(id);
}

bool isBaselineKeyword(CSSValueID id)
{
    return identMatches<CSSValueFirst, CSSValueLast, CSSValueBaseline>(id);
}

bool isContentPositionKeyword(CSSValueID id)
{
    return identMatches<CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueFlexStart, CSSValueFlexEnd>(id);
}

bool isContentPositionOrLeftOrRightKeyword(CSSValueID id)
{
    return isContentPositionKeyword(id) || isLeftOrRightKeyword(id);
}

static bool isContentDistributionKeyword(CSSValueID id)
{
    return identMatches<CSSValueSpaceBetween, CSSValueSpaceAround, CSSValueSpaceEvenly, CSSValueStretch>(id);
}

static bool isOverflowKeyword(CSSValueID id)
{
    return identMatches<CSSValueUnsafe, CSSValueSafe>(id);
}

static RefPtr<CSSPrimitiveValue> consumeOverflowPositionKeyword(CSSParserTokenRange& range)
{
    return isOverflowKeyword(range.peek().id()) ? consumeIdent(range) : nullptr;
}

static CSSValueID getBaselineKeyword(const CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.pairValue()) {
        ASSERT(primitiveValue.pairValue()->first()->valueID() == CSSValueLast);
        ASSERT(primitiveValue.pairValue()->second()->valueID() == CSSValueBaseline);
        return CSSValueLastBaseline;
    }
    ASSERT(primitiveValue.valueID() == CSSValueBaseline);
    return CSSValueBaseline;
}

static RefPtr<CSSValue> consumeBaselineKeyword(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> preference = consumeIdent<CSSValueFirst, CSSValueLast>(range);
    RefPtr<CSSPrimitiveValue> baseline = consumeIdent<CSSValueBaseline>(range);
    if (!baseline)
        return nullptr;
    if (preference && preference->valueID() == CSSValueLast)
        return createPrimitiveValuePair(preference.releaseNonNull(), baseline.releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce);
    return baseline;
}

RefPtr<CSSValue> consumeContentDistributionOverflowPosition(CSSParserTokenRange& range, IsPositionKeyword isPositionKeyword)
{
    ASSERT(isPositionKeyword);
    CSSValueID id = range.peek().id();
    if (identMatches<CSSValueNormal>(id))
        return CSSContentDistributionValue::create(CSSValueInvalid, range.consumeIncludingWhitespace().id(), CSSValueInvalid);

    if (isBaselineKeyword(id)) {
        RefPtr<CSSValue> baseline = consumeBaselineKeyword(range);
        if (!baseline)
            return nullptr;
        return CSSContentDistributionValue::create(CSSValueInvalid, getBaselineKeyword(*baseline), CSSValueInvalid);
    }

    if (isContentDistributionKeyword(id))
        return CSSContentDistributionValue::create(range.consumeIncludingWhitespace().id(), CSSValueInvalid, CSSValueInvalid);

    CSSValueID overflow = isOverflowKeyword(id) ? range.consumeIncludingWhitespace().id() : CSSValueInvalid;
    if (isPositionKeyword(range.peek().id()))
        return CSSContentDistributionValue::create(CSSValueInvalid, range.consumeIncludingWhitespace().id(), overflow);

    return nullptr;
}

RefPtr<CSSValue> consumeJustifyContent(CSSParserTokenRange& range)
{
    // justify-content property does not allow the <baseline-position> values.
    if (isBaselineKeyword(range.peek().id()))
        return nullptr;
    return consumeContentDistributionOverflowPosition(range, isContentPositionOrLeftOrRightKeyword);
}

static RefPtr<CSSPrimitiveValue> consumeBorderImageRepeatKeyword(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueStretch, CSSValueRepeat, CSSValueSpace, CSSValueRound>(range);
}

RefPtr<CSSValue> consumeBorderImageRepeat(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> horizontal = consumeBorderImageRepeatKeyword(range);
    if (!horizontal)
        return nullptr;
    RefPtr<CSSPrimitiveValue> vertical = consumeBorderImageRepeatKeyword(range);
    if (!vertical)
        vertical = horizontal;
    return createPrimitiveValuePair(horizontal.releaseNonNull(), vertical.releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce);
}

RefPtr<CSSValue> consumeBorderImageSlice(CSSPropertyID property, CSSParserTokenRange& range)
{
    bool fill = consumeIdent<CSSValueFill>(range);
    RefPtr<CSSPrimitiveValue> slices[4] = { nullptr };

    for (size_t index = 0; index < 4; ++index) {
        RefPtr<CSSPrimitiveValue> value = consumePercent(range, ValueRange::NonNegative);
        if (!value)
            value = consumeNumber(range, ValueRange::NonNegative);
        if (!value)
            break;
        slices[index] = value;
    }
    if (!slices[0])
        return nullptr;
    if (consumeIdent<CSSValueFill>(range)) {
        if (fill)
            return nullptr;
        fill = true;
    }
    complete4Sides(slices);
    // FIXME: For backwards compatibility, -webkit-border-image, -webkit-mask-box-image and -webkit-box-reflect have to do a fill by default.
    // FIXME: What do we do with -webkit-box-reflect and -webkit-mask-box-image? Probably just have to leave them filling...
    if (property == CSSPropertyWebkitBorderImage || property == CSSPropertyWebkitMaskBoxImage || property == CSSPropertyWebkitBoxReflect)
        fill = true;
    
    // Now build a rect value to hold all four of our primitive values.
    // FIXME-NEWPARSER: Should just have a CSSQuadValue.
    auto quad = Quad::create();
    quad->setTop(slices[0].releaseNonNull());
    quad->setRight(slices[1].releaseNonNull());
    quad->setBottom(slices[2].releaseNonNull());
    quad->setLeft(slices[3].releaseNonNull());
    
    // Make our new border image value now.
    return CSSBorderImageSliceValue::create(WTFMove(quad), fill);
}

RefPtr<CSSValue> consumeBorderImageOutset(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> outsets[4] = { nullptr };

    RefPtr<CSSPrimitiveValue> value;
    for (size_t index = 0; index < 4; ++index) {
        value = consumeNumber(range, ValueRange::NonNegative);
        if (!value)
            value = consumeLength(range, HTMLStandardMode, ValueRange::NonNegative);
        if (!value)
            break;
        outsets[index] = value;
    }
    if (!outsets[0])
        return nullptr;
    complete4Sides(outsets);
    
    // FIXME-NEWPARSER: Should just have a CSSQuadValue.
    auto quad = Quad::create();
    quad->setTop(outsets[0].releaseNonNull());
    quad->setRight(outsets[1].releaseNonNull());
    quad->setBottom(outsets[2].releaseNonNull());
    quad->setLeft(outsets[3].releaseNonNull());
    
    return CSSPrimitiveValue::create(WTFMove(quad));
}

RefPtr<CSSValue> consumeBorderImageWidth(CSSPropertyID property, CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> widths[4];

    RefPtr<CSSPrimitiveValue> value;
    for (size_t index = 0; index < 4; ++index) {
        value = consumeNumber(range, ValueRange::NonNegative);
        if (!value)
            value = consumeLengthOrPercent(range, HTMLStandardMode, ValueRange::NonNegative, UnitlessQuirk::Forbid);
        if (!value)
            value = consumeIdent<CSSValueAuto>(range);
        if (!value)
            break;
        widths[index] = value;
    }
    if (!widths[0])
        return nullptr;
    complete4Sides(widths);

    // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
    bool overridesBorderWidths = property == CSSPropertyWebkitBorderImage && (widths[0]->isLength() || widths[1]->isLength() || widths[2]->isLength() || widths[3]->isLength());

    // FIXME-NEWPARSER: Should just have a CSSQuadValue.
    auto quad = Quad::create();
    quad->setTop(widths[0].releaseNonNull());
    quad->setRight(widths[1].releaseNonNull());
    quad->setBottom(widths[2].releaseNonNull());
    quad->setLeft(widths[3].releaseNonNull());

    return CSSBorderImageWidthValue::create(WTFMove(quad), overridesBorderWidths);
}

bool consumeBorderImageComponents(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context, RefPtr<CSSValue>& source,
    RefPtr<CSSValue>& slice, RefPtr<CSSValue>& width, RefPtr<CSSValue>& outset, RefPtr<CSSValue>& repeat)
{
    do {
        if (!source) {
            source = consumeImageOrNone(range, context);
            if (source)
                continue;
        }
        if (!repeat) {
            repeat = consumeBorderImageRepeat(range);
            if (repeat)
                continue;
        }
        if (!slice) {
            slice = consumeBorderImageSlice(property, range);
            if (slice) {
                ASSERT(!width && !outset);
                if (consumeSlashIncludingWhitespace(range)) {
                    width = consumeBorderImageWidth(property, range);
                    if (consumeSlashIncludingWhitespace(range)) {
                        outset = consumeBorderImageOutset(range);
                        if (!outset)
                            return false;
                    } else if (!width)
                        return false;
                }
            } else
                return false;
        } else
            return false;
    } while (!range.atEnd());
    return true;
}

RefPtr<CSSValue> consumeReflect(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    
    RefPtr<CSSPrimitiveValue> direction = consumeIdent<CSSValueAbove, CSSValueBelow, CSSValueLeft, CSSValueRight>(range);
    if (!direction)
        return nullptr;

    RefPtr<CSSPrimitiveValue> offset;
    if (range.atEnd())
        offset = CSSPrimitiveValue::create(0, CSSUnitType::CSS_PX);
    else {
        offset = consumeLengthOrPercent(range, context.mode, ValueRange::All, UnitlessQuirk::Forbid);
        if (!offset)
            return nullptr;
    }

    RefPtr<CSSValue> mask;
    if (!range.atEnd()) {
        RefPtr<CSSValue> source;
        RefPtr<CSSValue> slice;
        RefPtr<CSSValue> width;
        RefPtr<CSSValue> outset;
        RefPtr<CSSValue> repeat;
        if (!consumeBorderImageComponents(CSSPropertyWebkitBoxReflect, range, context, source, slice, width, outset, repeat))
            return nullptr;
        mask = createBorderImageValue(WTFMove(source), WTFMove(slice), WTFMove(width), WTFMove(outset), WTFMove(repeat));
    }
    return CSSReflectValue::create(direction.releaseNonNull(), offset.releaseNonNull(), WTFMove(mask));
}

template<CSSPropertyID property> RefPtr<CSSValue> consumeBackgroundSize(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    // https://www.w3.org/TR/css-backgrounds-3/#typedef-bg-size
    //
    //    <bg-size> = [ <length-percentage [0,∞]> | auto ]{1,2} | cover | contain
    //

    if (identMatches<CSSValueContain, CSSValueCover>(range.peek().id()))
        return consumeIdent(range);

    auto identicalValueEncoding = Pair::IdenticalValueEncoding::DoNotCoalesce;

    // FIXME: We're allowing the unitless quirk on this property because our
    // tests assume that. Other browser engines don't allow it though.
    RefPtr<CSSPrimitiveValue> horizontal = consumeIdent<CSSValueAuto>(range);
    if (horizontal)
        identicalValueEncoding = Pair::IdenticalValueEncoding::Coalesce;
    else
        horizontal = consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative, UnitlessQuirk::Allow);

    if (!horizontal)
        return nullptr;

    RefPtr<CSSPrimitiveValue> vertical;
    if (!range.atEnd()) {
        vertical = consumeIdent<CSSValueAuto>(range);
        if (!vertical)
            vertical = consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative, UnitlessQuirk::Allow);
        }

    if (!vertical) {
        if constexpr (property == CSSPropertyWebkitBackgroundSize) {
            // Legacy syntax: "-webkit-background-size: 10px" is equivalent to "background-size: 10px 10px".
            vertical = horizontal;
        } else if constexpr (property == CSSPropertyBackgroundSize) {
            vertical = CSSPrimitiveValue::create(CSSValueAuto);
        } else if constexpr (property == CSSPropertyMaskSize) {
            return horizontal;
        }
    }

    return createPrimitiveValuePair(horizontal.releaseNonNull(), vertical.releaseNonNull(), identicalValueEncoding);
}

RefPtr<CSSValueList> consumeAlignTracks(CSSParserTokenRange& range)
{    
    RefPtr<CSSValueList> parsedValues = CSSValueList::createCommaSeparated();

    do {
        if (range.atEnd())
            break;

        auto value = consumeContentDistributionOverflowPosition(range, isContentPositionKeyword);
        if (value)
            parsedValues->append(value.releaseNonNull());
        else
            return nullptr;
    } while (consumeCommaIncludingWhitespace(range));

    return parsedValues;
}

RefPtr<CSSValueList> consumeJustifyTracks(CSSParserTokenRange& range)
{    
    RefPtr<CSSValueList> parsedValues = CSSValueList::createCommaSeparated();

    do {
        if (range.atEnd())
            break;

        // justify-tracks property does not allow the <baseline-position> values.
        if (isBaselineKeyword(range.peek().id()))
            return nullptr;

        auto value = consumeContentDistributionOverflowPosition(range, isContentPositionOrLeftOrRightKeyword);
        if (value)
            parsedValues->append(value.releaseNonNull());
        else
            return nullptr;
    } while (consumeCommaIncludingWhitespace(range));

    return parsedValues;
}

RefPtr<CSSValue> consumeGridAutoFlow(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> rowOrColumnValue = consumeIdent<CSSValueRow, CSSValueColumn>(range);
    RefPtr<CSSPrimitiveValue> denseAlgorithm = consumeIdent<CSSValueDense>(range);
    if (!rowOrColumnValue) {
        rowOrColumnValue = consumeIdent<CSSValueRow, CSSValueColumn>(range);
        if (!rowOrColumnValue && !denseAlgorithm)
            return nullptr;
    }
    RefPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();
    if (rowOrColumnValue) {
        CSSValueID value = rowOrColumnValue->valueID();
        if (value == CSSValueID::CSSValueColumn || (value == CSSValueID::CSSValueRow && !denseAlgorithm))
            parsedValues->append(rowOrColumnValue.releaseNonNull());
    }
    if (denseAlgorithm)
        parsedValues->append(denseAlgorithm.releaseNonNull());
    return parsedValues;
}

RefPtr<CSSValueList> consumeMasonryAutoFlow(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> packOrNextValue = consumeIdent<CSSValuePack, CSSValueNext>(range);
    RefPtr<CSSPrimitiveValue> definiteFirstOrOrderedValue = consumeIdent<CSSValueDefiniteFirst, CSSValueOrdered>(range);

    if (!packOrNextValue) {
        packOrNextValue = consumeIdent<CSSValuePack, CSSValueNext>(range);
        if (!packOrNextValue)
            packOrNextValue = CSSPrimitiveValue::create(CSSValuePack);
    }

    RefPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();
    if (packOrNextValue) {
        CSSValueID packOrNextValueID = packOrNextValue->valueID();
        if (!definiteFirstOrOrderedValue || (definiteFirstOrOrderedValue && definiteFirstOrOrderedValue->valueID() == CSSValueID::CSSValueDefiniteFirst) || packOrNextValueID == CSSValueID::CSSValueNext)
            parsedValues->append(packOrNextValue.releaseNonNull());
    }
    if (definiteFirstOrOrderedValue && definiteFirstOrOrderedValue->valueID() == CSSValueID::CSSValueOrdered)
        parsedValues->append(definiteFirstOrOrderedValue.releaseNonNull());

    return parsedValues;
}

RefPtr<CSSValue> consumeRepeatStyle(CSSParserTokenRange& range, const CSSParserContext&)
{
    // https://www.w3.org/TR/css-backgrounds-3/#typedef-repeat-style
    if (consumeIdent<CSSValueRepeatX>(range))
        return CSSBackgroundRepeatValue::create(CSSValueRepeat, CSSValueNoRepeat);

    if (consumeIdent<CSSValueRepeatY>(range))
        return CSSBackgroundRepeatValue::create(CSSValueNoRepeat, CSSValueRepeat);

    auto value1 = consumeIdentRaw<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound, CSSValueSpace>(range);
    if (!value1)
        return nullptr;

    auto value2 = consumeIdentRaw<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound, CSSValueSpace>(range);
    if (!value2)
        value2 = value1;

    return CSSBackgroundRepeatValue::create(*value1, *value2);
}

RefPtr<CSSValue> consumeSingleBackgroundSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://www.w3.org/TR/css-backgrounds-3/#background-size
    return consumeBackgroundSize<CSSPropertyBackgroundSize>(range, context.mode);
}

RefPtr<CSSValue> consumeSingleMaskSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://www.w3.org/TR/css-masking-1/#the-mask-size
    return consumeBackgroundSize<CSSPropertyMaskSize>(range, context.mode);
}

RefPtr<CSSValue> consumeSingleWebkitBackgroundSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeBackgroundSize<CSSPropertyWebkitBackgroundSize>(range, context.mode);
}

bool isSelfPositionKeyword(CSSValueID id)
{
    return identMatches<CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueSelfStart, CSSValueSelfEnd, CSSValueFlexStart, CSSValueFlexEnd>(id);
}

bool isSelfPositionOrLeftOrRightKeyword(CSSValueID id)
{
    return isSelfPositionKeyword(id) || isLeftOrRightKeyword(id);
}

RefPtr<CSSValue> consumeSelfPositionOverflowPosition(CSSParserTokenRange& range, IsPositionKeyword isPositionKeyword)
{
    ASSERT(isPositionKeyword);
    CSSValueID id = range.peek().id();
    if (isAuto(id) || isNormalOrStretch(id))
        return consumeIdent(range);

    if (isBaselineKeyword(id))
        return consumeBaselineKeyword(range);

    RefPtr<CSSPrimitiveValue> overflowPosition = consumeOverflowPositionKeyword(range);
    if (!isPositionKeyword(range.peek().id()))
        return nullptr;
    RefPtr<CSSPrimitiveValue> selfPosition = consumeIdent(range);
    if (overflowPosition)
        return createPrimitiveValuePair(overflowPosition.releaseNonNull(), selfPosition.releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce);
    return selfPosition;
}

RefPtr<CSSValue> consumeAlignItems(CSSParserTokenRange& range)
{
    // align-items property does not allow the 'auto' value.
    if (identMatches<CSSValueAuto>(range.peek().id()))
        return nullptr;
    return consumeSelfPositionOverflowPosition(range, isSelfPositionKeyword);
}

RefPtr<CSSValue> consumeJustifyItems(CSSParserTokenRange& range)
{
    // justify-items property does not allow the 'auto' value.
    if (identMatches<CSSValueAuto>(range.peek().id()))
        return nullptr;
    CSSParserTokenRange rangeCopy = range;
    RefPtr<CSSPrimitiveValue> legacy = consumeIdent<CSSValueLegacy>(rangeCopy);
    RefPtr<CSSPrimitiveValue> positionKeyword = consumeIdent<CSSValueCenter, CSSValueLeft, CSSValueRight>(rangeCopy);
    if (!legacy)
        legacy = consumeIdent<CSSValueLegacy>(rangeCopy);
    if (legacy) {
        range = rangeCopy;
        if (positionKeyword)
            return createPrimitiveValuePair(legacy.releaseNonNull(), positionKeyword.releaseNonNull(), Pair::IdenticalValueEncoding::Coalesce);
        return legacy;
    }
    return consumeSelfPositionOverflowPosition(range, isSelfPositionOrLeftOrRightKeyword);
}

static RefPtr<CSSValue> consumeFitContent(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    RefPtr<CSSPrimitiveValue> length = consumeLengthOrPercent(args, cssParserMode, ValueRange::NonNegative, UnitlessQuirk::Allow);
    if (!length || !args.atEnd())
        return nullptr;
    range = rangeCopy;
    RefPtr<CSSFunctionValue> result = CSSFunctionValue::create(CSSValueFitContent);
    result->append(length.releaseNonNull());
    return result;
}

static RefPtr<CSSPrimitiveValue> consumeCustomIdentForGridLine(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto || range.peek().id() == CSSValueSpan)
        return nullptr;
    return consumeCustomIdent(range);
}

RefPtr<CSSValue> consumeGridLine(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    RefPtr<CSSPrimitiveValue> spanValue;
    RefPtr<CSSPrimitiveValue> gridLineName;
    RefPtr<CSSPrimitiveValue> numericValue = consumeInteger(range);
    if (numericValue) {
        gridLineName = consumeCustomIdentForGridLine(range);
        spanValue = consumeIdent<CSSValueSpan>(range);
    } else {
        spanValue = consumeIdent<CSSValueSpan>(range);
        if (spanValue) {
            numericValue = consumeInteger(range);
            gridLineName = consumeCustomIdentForGridLine(range);
            if (!numericValue)
                numericValue = consumeInteger(range);
        } else {
            gridLineName = consumeCustomIdentForGridLine(range);
            if (gridLineName) {
                numericValue = consumeInteger(range);
                spanValue = consumeIdent<CSSValueSpan>(range);
                if (!spanValue && !numericValue)
                    return gridLineName;
            } else
                return nullptr;
        }
    }

    if (spanValue && !numericValue && !gridLineName)
        return nullptr; // "span" keyword alone is invalid.
    if (spanValue && numericValue && numericValue->intValue() < 0)
        return nullptr; // Negative numbers are not allowed for span.
    if (numericValue && numericValue->intValue() == 0)
        return nullptr; // An <integer> value of zero makes the declaration invalid.

    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    if (spanValue)
        values->append(spanValue.releaseNonNull());
    if (numericValue)
        values->append(numericValue.releaseNonNull());
    if (gridLineName)
        values->append(gridLineName.releaseNonNull());
    ASSERT(values->length());
    return values;
}

static bool isGridTrackFixedSized(const CSSPrimitiveValue& primitiveValue)
{
    switch (primitiveValue.valueID()) {
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
    case CSSValueAuto:
        return false;
    default:
        return !primitiveValue.isFlex();
    }
}

static bool isGridTrackFixedSized(const CSSValue& value)
{
    if (value.isPrimitiveValue())
        return isGridTrackFixedSized(downcast<CSSPrimitiveValue>(value));

    ASSERT(value.isFunctionValue());
    auto& function = downcast<CSSFunctionValue>(value);
    if (function.name() == CSSValueFitContent || function.length() < 2)
        return false;

    const CSSValue* minPrimitiveValue = downcast<CSSPrimitiveValue>(function.item(0));
    const CSSValue* maxPrimitiveValue = downcast<CSSPrimitiveValue>(function.item(1));
    return isGridTrackFixedSized(*minPrimitiveValue) || isGridTrackFixedSized(*maxPrimitiveValue);
}

static Vector<String> parseGridTemplateAreasColumnNames(StringView gridRowNames)
{
    ASSERT(!gridRowNames.isEmpty());
    Vector<String> columnNames;
    StringBuilder areaName;
    for (auto character : gridRowNames.codeUnits()) {
        if (isCSSSpace(character)) {
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
            continue;
        }
        if (character == '.') {
            if (areaName == "."_s)
                continue;
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        } else {
            if (!isNameCodePoint(character))
                return Vector<String>();
            if (areaName == "."_s) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        }

        areaName.append(character);
    }

    if (!areaName.isEmpty())
        columnNames.append(areaName.toString());

    return columnNames;
}

bool parseGridTemplateAreasRow(StringView gridRowNames, NamedGridAreaMap& gridAreaMap, const size_t rowCount, size_t& columnCount)
{
    if (gridRowNames.isAllSpecialCharacters<isCSSSpace>())
        return false;

    Vector<String> columnNames = parseGridTemplateAreasColumnNames(gridRowNames);
    if (rowCount == 0) {
        columnCount = columnNames.size();
        if (columnCount == 0)
            return false;
    } else if (columnCount != columnNames.size()) {
        // The declaration is invalid if all the rows don't have the number of columns.
        return false;
    }

    for (size_t currentColumn = 0; currentColumn < columnCount; ++currentColumn) {
        const String& gridAreaName = columnNames[currentColumn];

        // Unamed areas are always valid (we consider them to be 1x1).
        if (gridAreaName == "."_s)
            continue;

        size_t lookAheadColumn = currentColumn + 1;
        while (lookAheadColumn < columnCount && columnNames[lookAheadColumn] == gridAreaName)
            lookAheadColumn++;

        NamedGridAreaMap::iterator gridAreaIt = gridAreaMap.find(gridAreaName);
        if (gridAreaIt == gridAreaMap.end())
            gridAreaMap.add(gridAreaName, GridArea(GridSpan::translatedDefiniteGridSpan(rowCount, rowCount + 1), GridSpan::translatedDefiniteGridSpan(currentColumn, lookAheadColumn)));
        else {
            GridArea& gridArea = gridAreaIt->value;

            // The following checks test that the grid area is a single filled-in rectangle.
            // 1. The new row is adjacent to the previously parsed row.
            if (rowCount != gridArea.rows.endLine())
                return false;

            // 2. The new area starts at the same position as the previously parsed area.
            if (currentColumn != gridArea.columns.startLine())
                return false;

            // 3. The new area ends at the same position as the previously parsed area.
            if (lookAheadColumn != gridArea.columns.endLine())
                return false;

            gridArea.rows = GridSpan::translatedDefiniteGridSpan(gridArea.rows.startLine(), gridArea.rows.endLine() + 1);
        }
        currentColumn = lookAheadColumn - 1;
    }

    return true;
}

static RefPtr<CSSPrimitiveValue> consumeGridBreadth(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    const CSSParserToken& token = range.peek();
    if (identMatches<CSSValueMinContent, CSSValueWebkitMinContent, CSSValueMaxContent, CSSValueWebkitMaxContent, CSSValueAuto>(token.id()))
        return consumeIdent(range);
    if (token.type() == DimensionToken && token.unitType() == CSSUnitType::CSS_FR) {
        if (range.peek().numericValue() < 0)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_FR);
    }
    return consumeLengthOrPercent(range, cssParserMode, ValueRange::NonNegative);
}

RefPtr<CSSValue> consumeGridTrackSize(CSSParserTokenRange& range, CSSParserMode cssParserMode)
{
    const CSSParserToken& token = range.peek();
    if (identMatches<CSSValueAuto>(token.id()))
        return consumeIdent(range);

    if (token.functionId() == CSSValueMinmax) {
        CSSParserTokenRange rangeCopy = range;
        CSSParserTokenRange args = consumeFunction(rangeCopy);
        RefPtr<CSSPrimitiveValue> minTrackBreadth = consumeGridBreadth(args, cssParserMode);
        if (!minTrackBreadth || minTrackBreadth->isFlex() || !consumeCommaIncludingWhitespace(args))
            return nullptr;
        RefPtr<CSSPrimitiveValue> maxTrackBreadth = consumeGridBreadth(args, cssParserMode);
        if (!maxTrackBreadth || !args.atEnd())
            return nullptr;
        range = rangeCopy;
        RefPtr<CSSFunctionValue> result = CSSFunctionValue::create(CSSValueMinmax);
        result->append(minTrackBreadth.releaseNonNull());
        result->append(maxTrackBreadth.releaseNonNull());
        return result;
    }

    if (token.functionId() == CSSValueFitContent)
        return consumeFitContent(range, cssParserMode);

    return consumeGridBreadth(range, cssParserMode);
}

// Appends to the passed in CSSGridLineNamesValue if any, otherwise creates a new one. Returns nullptr if an empty list is consumed.
RefPtr<CSSGridLineNamesValue> consumeGridLineNames(CSSParserTokenRange& range, CSSGridLineNamesValue* lineNames, bool allowEmpty)
{
    CSSParserTokenRange rangeCopy = range;
    if (rangeCopy.consumeIncludingWhitespace().type() != LeftBracketToken)
        return nullptr;
    
    RefPtr<CSSGridLineNamesValue> result = lineNames;
    if (!result)
        result = CSSGridLineNamesValue::create();
    while (RefPtr<CSSPrimitiveValue> lineName = consumeCustomIdentForGridLine(rangeCopy))
        result->append(lineName.releaseNonNull());
    if (rangeCopy.consumeIncludingWhitespace().type() != RightBracketToken)
        return nullptr;
    range = rangeCopy;
    return (result->length() || allowEmpty) ? result : nullptr;
}

static bool consumeGridTrackRepeatFunction(CSSParserTokenRange& range, CSSParserMode cssParserMode, CSSValueList& list, bool& isAutoRepeat, bool& allTracksAreFixedSized)
{
    CSSParserTokenRange args = consumeFunction(range);
    // The number of repetitions for <auto-repeat> is not important at parsing level
    // because it will be computed later, let's set it to 1.
    size_t repetitions = 1;
    isAutoRepeat = identMatches<CSSValueAutoFill, CSSValueAutoFit>(args.peek().id());
    RefPtr<CSSValueList> repeatedValues;
    if (isAutoRepeat)
        repeatedValues = CSSGridAutoRepeatValue::create(args.consumeIncludingWhitespace().id());
    else {
        auto repetition = consumePositiveIntegerRaw(args);
        if (!repetition)
            return false;
        repetitions = clampTo<size_t>(static_cast<size_t>(*repetition), 0, GridPosition::max());
        repeatedValues = CSSValueList::createSpaceSeparated();
    }
    if (!consumeCommaIncludingWhitespace(args))
        return false;
    RefPtr<CSSGridLineNamesValue> lineNames = consumeGridLineNames(args);
    if (lineNames)
        repeatedValues->append(lineNames.releaseNonNull());

    size_t numberOfTracks = 0;
    while (!args.atEnd()) {
        RefPtr<CSSValue> trackSize = consumeGridTrackSize(args, cssParserMode);
        if (!trackSize)
            return false;
        if (allTracksAreFixedSized)
            allTracksAreFixedSized = isGridTrackFixedSized(*trackSize);
        repeatedValues->append(trackSize.releaseNonNull());
        ++numberOfTracks;
        lineNames = consumeGridLineNames(args);
        if (lineNames)
            repeatedValues->append(lineNames.releaseNonNull());
    }
    // We should have found at least one <track-size> or else it is not a valid <track-list>.
    if (!numberOfTracks)
        return false;

    if (isAutoRepeat)
        list.append(repeatedValues.releaseNonNull());
    else {
        // We clamp the repetitions to a multiple of the repeat() track list's size, while staying below the max grid size.
        repetitions = std::min(repetitions, GridPosition::max() / numberOfTracks);
        auto integerRepeatedValues = CSSGridIntegerRepeatValue::create(repetitions);
        for (auto& item : *repeatedValues)
            integerRepeatedValues->append(item.get());
        list.append(WTFMove(integerRepeatedValues));
    }
    return true;
}

static bool consumeSubgridNameRepeatFunction(CSSParserTokenRange& range, CSSValueList& list, bool& isAutoRepeat)
{
    CSSParserTokenRange args = consumeFunction(range);
    size_t repetitions = 1;
    isAutoRepeat = identMatches<CSSValueAutoFill>(args.peek().id());
    RefPtr<CSSValueList> repeatedValues;
    if (isAutoRepeat)
        repeatedValues = CSSGridAutoRepeatValue::create(args.consumeIncludingWhitespace().id());
    else {
        auto repetition = consumePositiveIntegerRaw(args);
        if (!repetition)
            return false;
        repetitions = clampTo<size_t>(static_cast<size_t>(*repetition), 0, GridPosition::max());
        repeatedValues = CSSGridIntegerRepeatValue::create(repetitions);
    }
    if (!consumeCommaIncludingWhitespace(args))
        return false;

    do {
        auto lineNames = consumeGridLineNames(args, nullptr, true);
        if (!lineNames)
            return false;
        repeatedValues->append(lineNames.releaseNonNull());
    } while (!args.atEnd());

    list.append(repeatedValues.releaseNonNull());
    return true;
}

RefPtr<CSSValue> consumeGridTrackList(CSSParserTokenRange& range, const CSSParserContext& context, TrackListType trackListType)
{
    if (range.peek().id() == CSSValueMasonry)
        return consumeIdent(range);
    bool seenAutoRepeat = false;
    if (trackListType == GridTemplate && context.subgridEnabled && range.peek().id() == CSSValueSubgrid) {
        consumeIdent(range);
        auto values = CSSSubgridValue::create();
        while (!range.atEnd() && range.peek().type() != DelimiterToken) {
            if (range.peek().functionId() == CSSValueRepeat) {
                bool isAutoRepeat;
                if (!consumeSubgridNameRepeatFunction(range, values, isAutoRepeat))
                    return nullptr;
                if (isAutoRepeat && seenAutoRepeat)
                    return nullptr;
                seenAutoRepeat = seenAutoRepeat || isAutoRepeat;
            } else if (auto value = consumeGridLineNames(range, nullptr, true))
                values->append(value.releaseNonNull());
            else
                return nullptr;
        }
        return values;
    }
    bool allowGridLineNames = trackListType != GridAuto;
    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    if (!allowGridLineNames && range.peek().type() == LeftBracketToken)
        return nullptr;
    RefPtr<CSSGridLineNamesValue> lineNames = consumeGridLineNames(range);
    if (lineNames)
        values->append(lineNames.releaseNonNull());
    
    bool allowRepeat = trackListType == GridTemplate;
    bool allTracksAreFixedSized = true;
    do {
        bool isAutoRepeat;
        if (range.peek().functionId() == CSSValueRepeat) {
            if (!allowRepeat)
                return nullptr;
            if (!consumeGridTrackRepeatFunction(range, context.mode, *values, isAutoRepeat, allTracksAreFixedSized))
                return nullptr;
            if (isAutoRepeat && seenAutoRepeat)
                return nullptr;
            seenAutoRepeat = seenAutoRepeat || isAutoRepeat;
        } else if (RefPtr<CSSValue> value = consumeGridTrackSize(range, context.mode)) {
            if (allTracksAreFixedSized)
                allTracksAreFixedSized = isGridTrackFixedSized(*value);
            values->append(value.releaseNonNull());
        } else
            return nullptr;
        if (seenAutoRepeat && !allTracksAreFixedSized)
            return nullptr;
        if (!allowGridLineNames && range.peek().type() == LeftBracketToken)
            return nullptr;
        lineNames = consumeGridLineNames(range);
        if (lineNames)
            values->append(lineNames.releaseNonNull());
    } while (!range.atEnd() && range.peek().type() != DelimiterToken);
    return values;
}

RefPtr<CSSValue> consumeGridTemplatesRowsOrColumns(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    if (context.masonryEnabled && range.peek().id() == CSSValueMasonry)
        return consumeIdent(range);
    return consumeGridTrackList(range, context, GridTemplate);
}

RefPtr<CSSValue> consumeGridTemplateAreas(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    NamedGridAreaMap gridAreaMap;
    size_t rowCount = 0;
    size_t columnCount = 0;

    while (range.peek().type() == StringToken) {
        if (!parseGridTemplateAreasRow(range.consumeIncludingWhitespace().value(), gridAreaMap, rowCount, columnCount))
            return nullptr;
        ++rowCount;
    }

    if (rowCount == 0)
        return nullptr;
    ASSERT(columnCount);
    return CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount);
}

RefPtr<CSSValue> consumeLineBoxContain(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    OptionSet<LineBoxContain> lineBoxContain;
    
    while (range.peek().type() == IdentToken) {
        auto id = range.peek().id();
        if (id == CSSValueBlock) {
            if (lineBoxContain.contains(LineBoxContain::Block))
                return nullptr;
            lineBoxContain.add(LineBoxContain::Block);
        } else if (id == CSSValueInline) {
            if (lineBoxContain.contains(LineBoxContain::Inline))
                return nullptr;
            lineBoxContain.add(LineBoxContain::Inline);
        } else if (id == CSSValueFont) {
            if (lineBoxContain.contains(LineBoxContain::Font))
                return nullptr;
            lineBoxContain.add(LineBoxContain::Font);
        } else if (id == CSSValueGlyphs) {
            if (lineBoxContain.contains(LineBoxContain::Glyphs))
                return nullptr;
            lineBoxContain.add(LineBoxContain::Glyphs);
        } else if (id == CSSValueReplaced) {
            if (lineBoxContain.contains(LineBoxContain::Replaced))
                return nullptr;
            lineBoxContain.add(LineBoxContain::Replaced);
        } else if (id == CSSValueInlineBox) {
            if (lineBoxContain.contains(LineBoxContain::InlineBox))
                return nullptr;
            lineBoxContain.add(LineBoxContain::InlineBox);
        } else if (id == CSSValueInitialLetter) {
            if (lineBoxContain.contains(LineBoxContain::InitialLetter))
                return nullptr;
            lineBoxContain.add(LineBoxContain::InitialLetter);
        } else
            return nullptr;
        range.consumeIncludingWhitespace();
    }
    
    if (!lineBoxContain)
        return nullptr;
    
    return CSSLineBoxContainValue::create(lineBoxContain);
}

RefPtr<CSSValue> consumeContainerName(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    auto list = CSSValueList::createSpaceSeparated();
    do {
        auto name = consumeSingleContainerName(range);
        if (!name)
            return list;
        list->append(name.releaseNonNull());
    } while (!range.atEnd());

    return list;
}

RefPtr<CSSValue> consumeWebkitInitialLetter(CSSParserTokenRange& range)
{
    RefPtr<CSSValue> ident = consumeIdent<CSSValueNormal>(range);
    if (ident)
        return ident;
    
    RefPtr<CSSPrimitiveValue> height = consumeNumber(range, ValueRange::NonNegative);
    if (!height)
        return nullptr;
    
    RefPtr<CSSPrimitiveValue> position;
    if (!range.atEnd()) {
        position = consumeNumber(range, ValueRange::NonNegative);
        if (!position || !range.atEnd())
            return nullptr;
    } else
        position = height.copyRef();
    
    return createPrimitiveValuePair(position.releaseNonNull(), WTFMove(height));
}

RefPtr<CSSValue> consumeSpeakAs(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    
    bool seenNormal = false;
    bool seenSpellOut = false;
    bool seenLiteralPunctuation = false;
    bool seenNoPunctuation = false;

    // normal | spell-out || digits || [ literal-punctuation | no-punctuation ]
    while (!range.atEnd()) {
        CSSValueID valueID = range.peek().id();
        if ((valueID == CSSValueNormal && seenSpellOut)
            || (valueID == CSSValueSpellOut && seenNormal)
            || (valueID == CSSValueLiteralPunctuation && seenNoPunctuation)
            || (valueID == CSSValueNoPunctuation && seenLiteralPunctuation))
            return nullptr;
        RefPtr<CSSValue> ident = consumeIdent<CSSValueNormal, CSSValueSpellOut, CSSValueDigits, CSSValueLiteralPunctuation, CSSValueNoPunctuation>(range);
        if (!ident)
            return nullptr;
        switch (valueID) {
        case CSSValueNormal:
            seenNormal = true;
            break;
        case CSSValueSpellOut:
            seenSpellOut = true;
            break;
        case CSSValueLiteralPunctuation:
            seenLiteralPunctuation = true;
            break;
        case CSSValueNoPunctuation:
            seenNoPunctuation = true;
            break;
        default:
            break;
        }
        list->append(ident.releaseNonNull());
    }
    
    return list->length() ? list : nullptr;
}
    
RefPtr<CSSValue> consumeHangingPunctuation(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    bool seenForceEnd = false;
    bool seenAllowEnd = false;
    bool seenFirst = false;
    bool seenLast = false;

    while (!range.atEnd()) {
        CSSValueID valueID = range.peek().id();
        if ((valueID == CSSValueFirst && seenFirst)
            || (valueID == CSSValueLast && seenLast)
            || (valueID == CSSValueAllowEnd && (seenAllowEnd || seenForceEnd))
            || (valueID == CSSValueForceEnd && (seenAllowEnd || seenForceEnd)))
            return nullptr;
        RefPtr<CSSValue> ident = consumeIdent<CSSValueAllowEnd, CSSValueForceEnd, CSSValueFirst, CSSValueLast>(range);
        if (!ident)
            return nullptr;
        switch (valueID) {
        case CSSValueAllowEnd:
            seenAllowEnd = true;
            break;
        case CSSValueForceEnd:
            seenForceEnd = true;
            break;
        case CSSValueFirst:
            seenFirst = true;
            break;
        case CSSValueLast:
            seenLast = true;
            break;
        default:
            break;
        }
        list->append(ident.releaseNonNull());
    }
    
    return list->length() ? list : nullptr;
}

RefPtr<CSSValue> consumeAlt(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().type() == StringToken)
        return consumeString(range);
    
    if (range.peek().functionId() != CSSValueAttr)
        return nullptr;
    
    return consumeAttr(consumeFunction(range), context);
}

RefPtr<CSSValue> consumeContain(CSSParserTokenRange& range)
{
    if (auto singleValue = consumeIdent<CSSValueNone, CSSValueStrict, CSSValueContent>(range))
        return singleValue;
    auto list = CSSValueList::createSpaceSeparated();
    RefPtr<CSSPrimitiveValue> size, inlineSize, layout, paint, style;
    while (!range.atEnd()) {
        switch (range.peek().id()) {
        case CSSValueSize:
            if (size)
                return nullptr;
            size = consumeIdent(range);
            break;
        case CSSValueInlineSize:
            if (inlineSize || size)
                return nullptr;
            inlineSize = consumeIdent(range);
            break;
        case CSSValueLayout:
            if (layout)
                return nullptr;
            layout = consumeIdent(range);
            break;
        case CSSValuePaint:
            if (paint)
                return nullptr;
            paint = consumeIdent(range);
            break;
        case CSSValueStyle:
            if (style)
                return nullptr;
            style = consumeIdent(range);
            break;
        default:
            return nullptr;
        }
    }
    if (size)
        list->append(size.releaseNonNull());
    if (inlineSize)
        list->append(inlineSize.releaseNonNull());
    if (layout)
        list->append(layout.releaseNonNull());
    if (style)
        list->append(style.releaseNonNull());
    if (paint)
        list->append(paint.releaseNonNull());
    if (!list->length())
        return nullptr;
    return RefPtr<CSSValue>(WTFMove(list));
}

RefPtr<CSSValue> consumeContainIntrinsicSize(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> autoValue;
    if (range.peek().type() == IdentToken) {
        switch (range.peek().id()) {
        case CSSValueNone:
            return consumeIdent<CSSValueNone>(range);
        case CSSValueAuto:
            autoValue = consumeIdent<CSSValueAuto>(range);
            break;
        default:
            return nullptr;
        }
    }

    if (range.atEnd())
        return nullptr;

    auto lengthValue = consumeLength(range, HTMLStandardMode, ValueRange::NonNegative);
    if (!lengthValue)
        return nullptr;

    if (!autoValue)
        return lengthValue;

    auto list = CSSValueList::createSpaceSeparated();
    list->append(autoValue.releaseNonNull());
    list->append(lengthValue.releaseNonNull());

    return list;
}

RefPtr<CSSValue> consumeTextEmphasisPosition(CSSParserTokenRange& range)
{
    bool foundOverOrUnder = false;
    CSSValueID overUnderValueID = CSSValueOver;
    bool foundLeftOrRight = false;
    CSSValueID leftRightValueID = CSSValueRight;
    while (!range.atEnd()) {
        switch (range.peek().id()) {
        case CSSValueOver:
            if (foundOverOrUnder)
                return nullptr;
            foundOverOrUnder = true;
            overUnderValueID = CSSValueOver;
            break;
        case CSSValueUnder:
            if (foundOverOrUnder)
                return nullptr;
            foundOverOrUnder = true;
            overUnderValueID = CSSValueUnder;
            break;
        case CSSValueLeft:
            if (foundLeftOrRight)
                return nullptr;
            foundLeftOrRight = true;
            leftRightValueID = CSSValueLeft;
            break;
        case CSSValueRight:
            if (foundLeftOrRight)
                return nullptr;
            foundLeftOrRight = true;
            leftRightValueID = CSSValueRight;
            break;
        default:
            return nullptr;
        }
        
        range.consumeIncludingWhitespace();
    }
    if (!foundOverOrUnder)
        return nullptr;
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(CSSPrimitiveValue::create(overUnderValueID));
    if (foundLeftOrRight)
        list->append(CSSPrimitiveValue::create(leftRightValueID));
    return list;
}

#if ENABLE(DARK_MODE_CSS)

RefPtr<CSSValue> consumeColorScheme(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    Vector<CSSValueID, 3> identifiers;

    while (!range.atEnd()) {
        if (range.peek().type() != IdentToken)
            return nullptr;

        CSSValueID id = range.peek().id();

        switch (id) {
        case CSSValueNormal:
            // `normal` is only allowed as a single value, and was handled earlier.
            // Don't allow it in the list.
            return nullptr;

        case CSSValueOnly:
        case CSSValueLight:
        case CSSValueDark:
            if (!identifiers.appendIfNotContains(id))
                return nullptr;
            break;

        default:
            // Unknown identifiers are allowed and ignored.
            break;
        }

        range.consumeIncludingWhitespace();
    }

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (auto id : identifiers)
        list->append(CSSPrimitiveValue::create(id));
    return list;
}

#endif

RefPtr<CSSValue> consumeOffsetRotate(CSSParserTokenRange& range, CSSParserMode mode)
{
    RefPtr<CSSPrimitiveValue> modifier;
    RefPtr<CSSPrimitiveValue> angle;

    auto rangeCopy = range;

    // Attempt to parse the first token as the modifier (auto / reverse keyword). If
    // successful, parse the second token as the angle. If not, try to parse the other
    // way around.
    if ((modifier = consumeIdent<CSSValueAuto, CSSValueReverse>(rangeCopy)))
        angle = consumeAngle(rangeCopy, mode);
    else {
        angle = consumeAngle(rangeCopy, mode);
        modifier = consumeIdent<CSSValueAuto, CSSValueReverse>(rangeCopy);
    }

    if (!angle && !modifier)
        return nullptr;

    range = rangeCopy;

    return CSSOffsetRotateValue::create(WTFMove(modifier), WTFMove(angle));
}

// MARK: - @-rule descriptor consumers:

// MARK: @font-face

RefPtr<CSSValue> consumeFontFaceFontFamily(CSSParserTokenRange& range)
{
    // FIXME-NEWPARSER: https://bugs.webkit.org/show_bug.cgi?id=196381 For compatibility with the old parser, we have to make
    // a list here, even though the list always contains only a single family name.
    // Once the old parser is gone, we can delete this function, make the caller
    // use consumeFamilyName instead, and then patch the @font-face code to
    // not expect a list with a single name in it.
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    RefPtr<CSSValue> parsedValue = consumeFamilyName(range);
    if (parsedValue)
        list->append(parsedValue.releaseNonNull());
    
    if (!range.atEnd() || !list->length())
        return nullptr;

    return list;
}

// MARK: @font-palette-values

RefPtr<CSSValue> consumeFontPaletteValuesOverrideColors(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto list = consumeCommaSeparatedListWithoutSingleValueOptimization(range, [](auto& range, auto& context) -> RefPtr<CSSValue> {
        auto key = consumeNonNegativeInteger(range);
        if (!key)
            return nullptr;

        auto color = consumeColor(range, context, false, { StyleColor::CSSColorType::Absolute });
        if (!color)
            return nullptr;

        return CSSFontPaletteValuesOverrideColorsValue::create(key.releaseNonNull(), color.releaseNonNull());
    }, context);

    if (!range.atEnd() || !list || !list->length())
        return nullptr;

    return list;
}

// MARK: @counter-style

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-system
RefPtr<CSSValue> consumeCounterStyleSystem(CSSParserTokenRange& range)
{
    // cyclic | numeric | alphabetic | symbolic | additive | [fixed <integer>?] | [ extends <counter-style-name> ]

    if (auto ident = consumeIdent<CSSValueCyclic, CSSValueNumeric, CSSValueAlphabetic, CSSValueSymbolic, CSSValueAdditive>(range))
        return ident;

    if (auto ident = consumeIdent<CSSValueFixed>(range)) {
        if (range.atEnd())
            return ident;
        // If we have the `fixed` keyword but the range is not at the end, the next token must be a integer.
        // If it's not, this value is invalid.
        auto firstSymbolValue = consumeInteger(range);
        if (!firstSymbolValue)
            return nullptr;
        return createPrimitiveValuePair(ident.releaseNonNull(), firstSymbolValue.releaseNonNull());
    }

    if (auto ident = consumeIdent<CSSValueExtends>(range)) {
        // FIXME: (rdar://103020193) "If a @counter-style uses the extends system, it must not contain a symbols or additive-symbols descriptor, or else the @counter-style rule is invalid." (https://www.w3.org/TR/css-counter-styles-3/#extends-system)

        // There must be a `<counter-style-name>` following the `extends` keyword. If there isn't, this value is invalid.
        auto parsedCounterStyleName = consumeCounterStyleName(range);
        if (!parsedCounterStyleName)
            return nullptr;
        return createPrimitiveValuePair(ident.releaseNonNull(), parsedCounterStyleName.releaseNonNull());
    }
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#typedef-symbol
RefPtr<CSSValue> consumeCounterStyleSymbol(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // [ <string> | <image> | <custom-ident> ]

    if (auto string = consumeString(range))
        return string;
    if (auto customIdent = consumeCustomIdent(range))
        return customIdent;
    // There are inherent difficulties in supporting <image> symbols in @counter-styles, so gate them behind a
    // flag for now. https://bugs.webkit.org/show_bug.cgi?id=167645
    if (context.counterStyleAtRuleImageSymbolsEnabled) {
        if (auto image = consumeImage(range, context, { AllowedImageType::URLFunction, AllowedImageType::GeneratedImage }))
            return image;
    }
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-negative
RefPtr<CSSValue> consumeCounterStyleNegative(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <symbol> <symbol>?

    auto prependValue = consumeCounterStyleSymbol(range, context);
    if (!prependValue)
        return nullptr;
    if (range.atEnd())
        return prependValue;

    auto appendValue = consumeCounterStyleSymbol(range, context);
    if (!appendValue || !range.atEnd())
        return nullptr;

    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    values->append(prependValue.releaseNonNull());
    values->append(appendValue.releaseNonNull());
    return values;
}

static RefPtr<CSSPrimitiveValue> consumeCounterStyleRangeBound(CSSParserTokenRange& range)
{
    if (auto infinite = consumeIdent<CSSValueInfinite>(range))
        return infinite;
    if (auto integer = consumeInteger(range))
        return integer;
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-range
RefPtr<CSSValue> consumeCounterStyleRange(CSSParserTokenRange& range)
{
    // [ [ <integer> | infinite ]{2} ]# | auto

    if (auto autoValue = consumeIdent<CSSValueAuto>(range))
        return autoValue;

    auto rangeList = consumeCommaSeparatedListWithoutSingleValueOptimization(range, [](auto& range) -> RefPtr<CSSValue> {
        auto lowerBound = consumeCounterStyleRangeBound(range);
        if (!lowerBound)
            return nullptr;
        auto upperBound = consumeCounterStyleRangeBound(range);
        if (!upperBound)
            return nullptr;

        // If the lower bound of any range is higher than the upper bound, the entire descriptor is invalid and must be
        // ignored.
        if (lowerBound->isInteger() && upperBound->isInteger() && lowerBound->intValue() > upperBound->intValue())
            return nullptr;
        
        return createPrimitiveValuePair(lowerBound.releaseNonNull(), upperBound.releaseNonNull(), Pair::IdenticalValueEncoding::DoNotCoalesce);
    });

    if (!range.atEnd() || !rangeList || !rangeList->length())
        return nullptr;
    return rangeList;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-pad
RefPtr<CSSValue> consumeCounterStylePad(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <integer [0,∞]> && <symbol>

    RefPtr<CSSValue> integer;
    RefPtr<CSSValue> symbol;
    while (!integer || !symbol) {
        if (!integer) {
            integer = consumeNonNegativeInteger(range);
            if (integer)
                continue;
        }
        if (!symbol) {
            symbol = consumeCounterStyleSymbol(range, context);
            if (symbol)
                continue;
        }
        return nullptr;
    }
    if (!range.atEnd())
        return nullptr;
    auto values = CSSValueList::createSpaceSeparated();
    values->append(integer.releaseNonNull());
    values->append(symbol.releaseNonNull());
    return values;
}

// https://www.w3.org/TR/css-counter-styles-3/#descdef-counter-style-symbols
RefPtr<CSSValue> consumeCounterStyleSymbols(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <symbol>+

    auto symbols = CSSValueList::createSpaceSeparated();
    while (!range.atEnd()) {
        auto symbol = consumeCounterStyleSymbol(range, context);
        if (!symbol)
            return nullptr;
        symbols->append(symbol.releaseNonNull());
    }
    if (!symbols->length())
        return nullptr;
    return symbols;
}

// https://www.w3.org/TR/css-counter-styles-3/#descdef-counter-style-additive-symbols
RefPtr<CSSValue> consumeCounterStyleAdditiveSymbols(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // [ <integer [0,∞]> && <symbol> ]#

    std::optional<int> lastWeight;
    auto values = consumeCommaSeparatedListWithoutSingleValueOptimization(range, [&lastWeight](auto& range, auto& context) -> RefPtr<CSSValue> {
        auto integer = consumeNonNegativeInteger(range);
        auto symbol = consumeCounterStyleSymbol(range, context);
        if (!integer) {
            if (!symbol)
                return nullptr;
            integer = consumeNonNegativeInteger(range);
            if (!integer)
                return nullptr;
        }

        // Additive tuples must be specified in order of strictly descending weight.
        auto weight = integer->intValue();
        if (lastWeight && !(weight < lastWeight))
            return nullptr;
        lastWeight = weight;

        auto pair = CSSValueList::createSpaceSeparated();
        pair->append(integer.releaseNonNull());
        pair->append(symbol.releaseNonNull());
        return pair;
    }, context);

    if (!range.atEnd() || !values || !values->length())
        return nullptr;
    return values;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-speak-as
RefPtr<CSSValue> consumeCounterStyleSpeakAs(CSSParserTokenRange& range)
{
    // auto | bullets | numbers | words | spell-out | <counter-style-name>

    if (auto speakAsIdent = consumeIdent<CSSValueAuto, CSSValueBullets, CSSValueNumbers, CSSValueWords, CSSValueSpellOut>(range))
        return speakAsIdent;
    return consumeCounterStyleName(range);
}

RefPtr<CSSValue> consumeDeclarationValue(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return CSSVariableParser::parseDeclarationValue(nullAtom(), range.consumeAll(), context);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
