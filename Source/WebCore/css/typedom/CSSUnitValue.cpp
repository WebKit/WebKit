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

#include "CSSCalcOperationNode.h"
#include "CSSCalcPrimitiveValueNode.h"
#include "CSSCalcValue.h"
#include "CSSParserFastPaths.h"
#include "CSSParserToken.h"
#include "CSSPrimitiveValue.h"
#include <cmath>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSUnitValue);

CSSUnitType CSSUnitValue::parseUnit(const String& unit)
{
    if (unit == "number"_s)
        return CSSUnitType::CSS_NUMBER;
    if (unit == "percent"_s)
        return CSSUnitType::CSS_PERCENTAGE;

    // FIXME: Remove these when LineHeightUnitsEnabled is changed back to true or removed
    // https://bugs.webkit.org/show_bug.cgi?id=211351
    if (unit == "lh"_s)
        return CSSUnitType::CSS_LHS;
    if (unit == "rlh"_s)
        return CSSUnitType::CSS_RLHS;

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
        return Exception { TypeError };
    auto type = CSSNumericType::create(parsedUnit);
    if (!type)
        return Exception { TypeError };
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

static double conversionToCanonicalUnitsScaleFactor(CSSUnitType unit)
{
    constexpr double pixelsPerInch = 96;
    constexpr double pixelsPerCentimeter = pixelsPerInch / 2.54;
    constexpr double pointsPerInch = 72;
    constexpr double picasPerInch = 6;

    switch (unit) {
    case CSSUnitType::CSS_MS:
        return 0.001;
    case CSSUnitType::CSS_CM:
        return pixelsPerCentimeter;
    case CSSUnitType::CSS_DPCM:
        return 1.0 / pixelsPerCentimeter;
    case CSSUnitType::CSS_MM:
        return pixelsPerCentimeter / 10;
    case CSSUnitType::CSS_Q:
        return pixelsPerCentimeter / 40;
    case CSSUnitType::CSS_IN:
        return pixelsPerInch;
    case CSSUnitType::CSS_DPI:
        return 1.0 / pixelsPerInch;
    case CSSUnitType::CSS_PT:
        return pixelsPerInch / pointsPerInch;
    case CSSUnitType::CSS_PC:
        return pixelsPerInch / picasPerInch;
    case CSSUnitType::CSS_RAD:
        return 180 / piDouble;
    case CSSUnitType::CSS_GRAD:
        return 0.9;
    case CSSUnitType::CSS_TURN:
        return 360;
    case CSSUnitType::CSS_KHZ:
        return 1000;
    default:
        return 1.0;
    }
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
    bool acceptsNegativeNumbers = true;
    if (CSSParserFastPaths::isSimpleLengthPropertyID(propertyID, acceptsNegativeNumbers) && !acceptsNegativeNumbers && value < 0)
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

RefPtr<CSSValue> CSSUnitValue::toCSSValueWithProperty(CSSPropertyID propertyID) const
{
    if (isValueOutOfRangeForProperty(propertyID, m_value, m_unit)) {
        // Wrap out of range values with a calc.
        auto node = toCalcExpressionNode();
        ASSERT(node);
        auto sumNode = CSSCalcOperationNode::createSum(Vector { node.releaseNonNull() });
        if (!sumNode)
            return nullptr;
        return CSSPrimitiveValue::create(CSSCalcValue::create(sumNode.releaseNonNull()));
    }
    return toCSSValue();
}

RefPtr<CSSCalcExpressionNode> CSSUnitValue::toCalcExpressionNode() const
{
    return CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(m_value, m_unit));
}

} // namespace WebCore
