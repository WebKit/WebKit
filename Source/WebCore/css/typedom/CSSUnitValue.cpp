/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSPrimitiveNumericCategory.h"
#include "CSSPrimitiveValue.h"
#include "CSSUnits.h"
#include "ExceptionOr.h"
#include <cmath>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CSSUnitValue);

CSSUnitType CSSUnitValue::parseUnit(const String& unit)
{
    if (unit == "number"_s)
        return CSSUnitType::Number;
    if (unit == "percent"_s)
        return CSSUnitType::Percentage;
    return CSSParserToken::stringToUnitType(unit);
}

ASCIILiteral CSSUnitValue::unit() const
{
    switch (m_unit) {
    case CSSUnitType::Number:
        return "number"_s;
    case CSSUnitType::Percentage:
        return "percent"_s;
    default:
        break;
    }
    return unitSerialization();
}

ASCIILiteral CSSUnitValue::unitSerialization() const
{
    return unitTypeString(m_unit);
}

void CSSUnitValue::serialize(StringBuilder& builder, OptionSet<SerializationArguments>) const
{
    builder.append(FormattedCSSNumber::create(m_value));
    builder.append(unitSerialization());
}

ExceptionOr<Ref<CSSUnitValue>> CSSUnitValue::create(double value, const String& unit)
{
    auto parsedUnit = parseUnit(unit);
    if (parsedUnit == CSSUnitType::Unknown)
        return Exception { ExceptionCode::TypeError };
    auto type = CSSNumericType::create(parsedUnit);
    if (!type)
        return Exception { ExceptionCode::TypeError };
    auto unitValue = adoptRef(*new CSSUnitValue(value, parsedUnit));
    unitValue->m_type = WTF::move(*type);
    return unitValue;
}

ExceptionOr<Ref<CSSUnitValue>> CSSUnitValue::reifyValue(Document&, const CSSValue& cssValue)
{
    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(cssValue);
    if (!primitiveValue)
        return Exception { ExceptionCode::TypeError };

    return WTF::switchOn(*primitiveValue,
        [&](const CSSPrimitiveValue::Calc&) -> ExceptionOr<Ref<CSSUnitValue>> {
            return Exception { ExceptionCode::TypeError };
        },
        [&](const CSSPrimitiveValue::Raw& raw) -> ExceptionOr<Ref<CSSUnitValue>> {
            if (raw.unit == CSSUnitType::Integer) {
                // Integer is special cased to resolved the same as <number>.
                return CSSUnitValue::create(raw.value, CSSUnitType::Number);
            } else {
                return CSSUnitValue::create(raw.value, raw.unit);
            }
        }
    );
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

    return create(m_value * conversionToCanonicalUnitsScaleFactor(unitEnum()).value_or(1) / conversionToCanonicalUnitsScaleFactor(unit).value_or(1), unit);
}

auto CSSUnitValue::toSumValue() const -> std::optional<SumValue>
{
    // https://drafts.css-houdini.org/css-typed-om/#create-a-sum-value
    auto canonicalUnit = canonicalUnitTypeForUnitType(m_unit);
    if (canonicalUnit == CSSUnitType::Unknown)
        canonicalUnit = m_unit;
    
    auto convertedValue = m_value * conversionToCanonicalUnitsScaleFactor(unitEnum()).value_or(1) / conversionToCanonicalUnitsScaleFactor(canonicalUnit).value_or(1);

    if (m_unit == CSSUnitType::Number)
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
    auto valueRange = CSSParserFastPaths::lengthValueRangeForPropertiesSupportingSimpleLengths(propertyID);
    if (valueRange && (value < valueRange->min || value > valueRange->max))
        return true;

    switch (propertyID) {
    case CSSPropertyOrder:
    case CSSPropertyZIndex:
        return round(value) != value;
    case CSSPropertyTabSize:
        return value < 0 || (unit == CSSUnitType::Number && round(value) != value);
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
    case CSSPropertyFontWidth:
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

static CSS::Range NODELETE rangeForProperty(CSSPropertyID propertyID, CSSUnitType)
{
    // FIXME: Merge with isValueOutOfRangeForProperty.

    if (auto valueRange = CSSParserFastPaths::lengthValueRangeForPropertiesSupportingSimpleLengths(propertyID))
        return *valueRange;

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
    case CSSPropertyFontWidth:
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

static CSS::Category NODELETE calculationCategoryForProperty(CSSPropertyID, CSSUnitType unit)
{
    // FIXME: This should be looking up the supported calculation categories for the CSSPropertyID and picking the one that best matches the unit.

    switch (unit) {
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
        return CSS::Category::Number;
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Inch:
    case CSSUnitType::Point:
    case CSSUnitType::Pica:
    case CSSUnitType::QuarterMillimeter:
    case CSSUnitType::LineHeight:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::RootCap:
    case CSSUnitType::RootCh:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIc:
    case CSSUnitType::RootLineHeight:
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportMin:
    case CSSUnitType::ViewportMax:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMin:
    case CSSUnitType::SmallViewportMax:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMin:
    case CSSUnitType::LargeViewportMax:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMin:
    case CSSUnitType::DynamicViewportMax:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMin:
    case CSSUnitType::ContainerQueryMax:
        return CSS::Category::Length;
    case CSSUnitType::Percentage:
        return CSS::Category::Percentage;
    case CSSUnitType::Degree:
    case CSSUnitType::Radian:
    case CSSUnitType::Gradian:
    case CSSUnitType::Turn:
        return CSS::Category::Angle;
    case CSSUnitType::Millisecond:
    case CSSUnitType::Second:
        return CSS::Category::Time;
    case CSSUnitType::Hertz:
    case CSSUnitType::Kilohertz:
        return CSS::Category::Frequency;
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::X:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerCentimeter:
        return CSS::Category::Resolution;
    case CSSUnitType::Fraction:
        return CSS::Category::Flex;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return CSS::Category::Number;
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

        Vector<CSSCalc::Child> sumChildren;
        sumChildren.append(WTF::move(*node));
        auto sum = CSSCalc::makeChild(CSSCalc::Sum { .children = WTF::move(sumChildren) }, type);

        return CSSPrimitiveValue::create(CSSCalc::Value::create(category, range, CSSCalc::Tree {
            .root = WTF::move(sum),
            .type = type,
            .stage = CSSCalc::Stage::Specified,
        }));
    }
    return toCSSValue();
}

std::optional<CSSCalc::Child> CSSUnitValue::toCalcTreeNode() const
{
    return CSSCalc::makeNumeric(m_value, m_unit);
}

} // namespace WebCore
