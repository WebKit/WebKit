/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSUnitValue.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSParserFastPaths.h"
#include "CSSParserToken.h"
#include "CSSPrimitiveValue.h"
#include "CSSUnits.h"
#include "CalculationCategory.h"
#include <cmath>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSUnitValue);

CSSUnitType CSSUnitValue::parseUnit(const String& unit)
{
    if (unit == "number"_s)
        return CSSUnitType::CSS_NUMBER;
    if (unit == "percent"_s)
        return CSSUnitType::CSS_PERCENTAGE;
    return CSSParserToken::stringToUnitType(unit);
}

ASCIILiteral CSSUnitValue::unit() const
{
    switch (m_unit) {
    case CSSUnitType::CSS_NUMBER:
        return "number"_s;
    case CSSUnitType::CSS_PERCENTAGE:
        return "percent"_s;
    default:
        break;
    }
    return unitSerialization();
}

ASCIILiteral CSSUnitValue::unitSerialization() const
{
    return CSSPrimitiveValue::unitTypeString(m_unit);
}

void CSSUnitValue::serialize(StringBuilder& builder, OptionSet<SerializationArguments>) const
{
    builder.append(FormattedCSSNumber::create(m_value));
    builder.append(unitSerialization());
}

ExceptionOr<Ref<CSSUnitValue>> CSSUnitValue::create(double value, const String& unit)
{
    auto parsedUnit = parseUnit(unit);
    if (parsedUnit == CSSUnitType::CSS_UNKNOWN)
        return Exception { ExceptionCode::TypeError };
    auto type = CSSNumericType::create(parsedUnit);
    if (!type)
        return Exception { ExceptionCode::TypeError };
    auto unitValue = adoptRef(*new CSSUnitValue(value, parsedUnit));
    unitValue->m_type = WTFMove(*type);
    return unitValue;
}

CSSUnitValue::CSSUnitValue(double value, CSSUnitType unit)
    : CSSNumericValue(CSSNumericType::create(unit).value_or(CSSNumericType()))
    , m_value(value)
    , m_unit(unit)
{
}

RefPtr<CSSUnitValue> CSSUnitValue::convertTo(CSSUnitType unit) const
{
    // https://drafts.css-houdini.org/css-typed-om/#convert-a-cssunitvalue
    if (unitCategory(unitEnum()) != unitCategory(unit))
        return nullptr;

    return create(m_value * conversionToCanonicalUnitsScaleFactor(unitEnum()) / conversionToCanonicalUnitsScaleFactor(unit), unit);
}

auto CSSUnitValue::toSumValue() const -> std::optional<SumValue>
{
    // https://drafts.css-houdini.org/css-typed-om/#create-a-sum-value
    auto canonicalUnit = canonicalUnitTypeForUnitType(m_unit);
    if (canonicalUnit == CSSUnitType::CSS_UNKNOWN)
        canonicalUnit = m_unit;
    
    auto convertedValue = m_value * conversionToCanonicalUnitsScaleFactor(unitEnum()) / conversionToCanonicalUnitsScaleFactor(canonicalUnit);

    if (m_unit == CSSUnitType::CSS_NUMBER)
        return { { { convertedValue, { } } } };
    return { { { convertedValue, { { canonicalUnit, 1 } } } } };
}

bool CSSUnitValue::equals(const CSSNumericValue& other) const
{
    // https://drafts.css-houdini.org/css-typed-om/#equal-numeric-value
    auto* otherUnitValue = dynamicDowncast<CSSUnitValue>(other);
    if (!otherUnitValue)
        return false;
    return m_value == otherUnitValue->m_value && m_unit == otherUnitValue->m_unit;
}

RefPtr<CSSValue> CSSUnitValue::toCSSValue() const
{
    return CSSPrimitiveValue::create(m_value, m_unit);
}

// FIXME: This function could be mostly generated from CSSProperties.json.
static bool isValueOutOfRangeForProperty(CSSPropertyID propertyID, double value, CSSUnitType unit)
{
    CSS::Range valueRange = CSS::All;
    if (CSSParserFastPaths::isSimpleLengthPropertyID(propertyID, valueRange) && (value < valueRange.min || value > valueRange.max))
        return true;

    switch (propertyID) {
    case CSSPropertyOrder:
    case CSSPropertyZIndex:
        return round(value) != value;
    case CSSPropertyTabSize:
        return value < 0 || (unit == CSSUnitType::CSS_NUMBER && round(value) != value);
    case CSSPropertyOrphans:
    case CSSPropertyWidows:
    case CSSPropertyColumnCount:
        return round(value) != value || value < 1;
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBlockSize:
    case CSSPropertyBorderBlockEndWidth:
    case CSSPropertyBorderBlockStartWidth:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderInlineEndWidth:
    case CSSPropertyBorderInlineStartWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyColumnGap:
    case CSSPropertyColumnRuleWidth:
    case CSSPropertyColumnWidth:
    case CSSPropertyFlexBasis:
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
    case CSSPropertyFontSize:
    case CSSPropertyFontSizeAdjust:
    case CSSPropertyFontStretch:
    case CSSPropertyGridAutoColumns:
    case CSSPropertyGridAutoRows:
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
    case CSSPropertyInlineSize:
    case CSSPropertyLineHeight:
    case CSSPropertyMaxBlockSize:
    case CSSPropertyMaxInlineSize:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinBlockSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyOutlineWidth:
    case CSSPropertyPerspective:
    case CSSPropertyR:
    case CSSPropertyRowGap:
    case CSSPropertyRx:
    case CSSPropertyRy:
    case CSSPropertyScrollPaddingBlockEnd:
    case CSSPropertyScrollPaddingBlockStart:
    case CSSPropertyScrollPaddingBottom:
    case CSSPropertyScrollPaddingInlineEnd:
    case CSSPropertyScrollPaddingInlineStart:
    case CSSPropertyScrollPaddingLeft:
    case CSSPropertyScrollPaddingRight:
    case CSSPropertyScrollPaddingTop:
    case CSSPropertyStrokeDasharray:
    case CSSPropertyStrokeMiterlimit:
    case CSSPropertyStrokeWidth:
    case CSSPropertyTransitionDuration:
        return value < 0;
    case CSSPropertyFontWeight:
        return value < 1 || value > 1000;
    default:
        return false;
    }
}

static CSS::Range rangeForProperty(CSSPropertyID propertyID, CSSUnitType)
{
    // FIXME: Merge with isValueOutOfRangeForProperty.
    auto valueRange = CSS::All;
    if (CSSParserFastPaths::isSimpleLengthPropertyID(propertyID, valueRange))
        return valueRange;

    switch (propertyID) {
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBlockSize:
    case CSSPropertyBorderBlockEndWidth:
    case CSSPropertyBorderBlockStartWidth:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderInlineEndWidth:
    case CSSPropertyBorderInlineStartWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyColumnGap:
    case CSSPropertyColumnRuleWidth:
    case CSSPropertyColumnWidth:
    case CSSPropertyFlexBasis:
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
    case CSSPropertyFontSize:
    case CSSPropertyFontSizeAdjust:
    case CSSPropertyFontStretch:
    case CSSPropertyGridAutoColumns:
    case CSSPropertyGridAutoRows:
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
    case CSSPropertyInlineSize:
    case CSSPropertyLineHeight:
    case CSSPropertyMaxBlockSize:
    case CSSPropertyMaxInlineSize:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinBlockSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyOutlineWidth:
    case CSSPropertyPerspective:
    case CSSPropertyR:
    case CSSPropertyRowGap:
    case CSSPropertyRx:
    case CSSPropertyRy:
    case CSSPropertyScrollPaddingBlockEnd:
    case CSSPropertyScrollPaddingBlockStart:
    case CSSPropertyScrollPaddingBottom:
    case CSSPropertyScrollPaddingInlineEnd:
    case CSSPropertyScrollPaddingInlineStart:
    case CSSPropertyScrollPaddingLeft:
    case CSSPropertyScrollPaddingRight:
    case CSSPropertyScrollPaddingTop:
    case CSSPropertyStrokeDasharray:
    case CSSPropertyStrokeMiterlimit:
    case CSSPropertyStrokeWidth:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTabSize:
    case CSSPropertyFontWeight:     // FIXME: Support more fine-grain ranges: `<number [1,1000]>`
    case CSSPropertyOrphans:        // FIXME: Support more fine-grain ranges: `<integer [1,∞]>`
    case CSSPropertyWidows:         // FIXME: Support more fine-grain ranges: `<integer [1,∞]>`
    case CSSPropertyColumnCount:    // FIXME: Support more fine-grain ranges: `<integer [1,∞]>`
        return CSS::Nonnegative;

    case CSSPropertyOrder:          // FIXME: Support more fine-grain ranges: `<integer>`
    case CSSPropertyZIndex:         // FIXME: Support more fine-grain ranges: `<integer>`
    default:
        return CSS::All;
    }
}

static Calculation::Category calculationCategoryForProperty(CSSPropertyID, CSSUnitType unit)
{
    // FIXME: This should be looking up the supported calculation categories for the CSSPropertyID and picking the one that best matches the unit.

    switch (unit) {
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
        return Calculation::Category::Number;
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_RLH:
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
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        return Calculation::Category::Length;
    case CSSUnitType::CSS_PERCENTAGE:
        return Calculation::Category::Percentage;
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
        return Calculation::Category::Angle;
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
        return Calculation::Category::Time;
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
        return Calculation::Category::Frequency;
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
        return Calculation::Category::Resolution;
    case CSSUnitType::CSS_FR:
        return Calculation::Category::Flex;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return Calculation::Category::Number;
}

RefPtr<CSSValue> CSSUnitValue::toCSSValueWithProperty(CSSPropertyID propertyID) const
{
    if (isValueOutOfRangeForProperty(propertyID, m_value, m_unit)) {
        // Wrap out of range values with a calc.

        auto node = toCalcTreeNode();
        ASSERT(node);
        auto type = CSSCalc::getType(*node);

        auto range = rangeForProperty(propertyID, m_unit);
        auto category = calculationCategoryForProperty(propertyID, m_unit);

        if (!type.matches(category)) {
            ALWAYS_LOG_WITH_STREAM(stream << "calc() type '" << type << "' is not valid for category '" << category << "'");
            return nullptr;
        }

        CSSCalc::Children sumChildren;
        sumChildren.append(WTFMove(*node));
        auto sum = CSSCalc::makeChild(CSSCalc::Sum { .children = WTFMove(sumChildren) }, type);

        return CSSPrimitiveValue::create(CSSCalcValue::create(CSSCalc::Tree {
            .root = WTFMove(sum),
            .type = type,
            .category = category,
            .stage = CSSCalc::Stage::Specified,
            .range = range
        }));
    }
    return toCSSValue();
}

std::optional<CSSCalc::Child> CSSUnitValue::toCalcTreeNode() const
{
    return CSSCalc::makeNumeric(m_value, m_unit);
}

} // namespace WebCore
