/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSCalcPrimitiveValueNode.h"

#include "CSSCalcCategoryMapping.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CalcExpressionLength.h"
#include "CalcExpressionNumber.h"
#include "CalculationValue.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSCalcPrimitiveValueNode);

Ref<CSSCalcPrimitiveValueNode> CSSCalcPrimitiveValueNode::create(Ref<CSSPrimitiveValue>&& value)
{
    return adoptRef(*new CSSCalcPrimitiveValueNode(WTFMove(value)));
}

RefPtr<CSSCalcPrimitiveValueNode> CSSCalcPrimitiveValueNode::create(double value, CSSUnitType type)
{
    if (!std::isfinite(value))
        return nullptr;
    return adoptRef(new CSSCalcPrimitiveValueNode(CSSPrimitiveValue::create(value, type)));
}

String CSSCalcPrimitiveValueNode::customCSSText() const
{
    return protectedValue()->cssText();
}

CSSUnitType CSSCalcPrimitiveValueNode::primitiveType() const
{
    return protectedValue()->primitiveType();
}

CSSCalcPrimitiveValueNode::CSSCalcPrimitiveValueNode(Ref<CSSPrimitiveValue>&& value)
    : CSSCalcExpressionNode(calcUnitCategory(value->primitiveType()))
    , m_value(WTFMove(value))
{
}

// FIXME: Use calcUnitCategory?
bool CSSCalcPrimitiveValueNode::isNumericValue() const
{
    return m_value->isLength() || m_value->isNumber() || m_value->isInteger() || m_value->isPercentage() || m_value->isAngle()
        || m_value->isTime() || m_value->isResolution() || m_value->isFlex() || m_value->isFrequency();
}

bool CSSCalcPrimitiveValueNode::isNegative() const
{
    return isNumericValue() && protectedValue()->doubleValue() < 0.0;
}

void CSSCalcPrimitiveValueNode::negate()
{
    ASSERT(isNumericValue());
    Ref value = m_value;
    m_value = CSSPrimitiveValue::create(0.0 - value->doubleValue(), value->primitiveType());
}

void CSSCalcPrimitiveValueNode::invert()
{
    ASSERT(isNumericValue());
    Ref value = m_value;
    m_value = CSSPrimitiveValue::create(1.0 / value->doubleValue(), value->primitiveType());
}

void CSSCalcPrimitiveValueNode::add(const CSSCalcPrimitiveValueNode& node, UnitConversion unitConversion)
{
    Ref value = m_value;
    auto valueType = value->primitiveType();

    switch (unitConversion) {
    case UnitConversion::Invalid:
        ASSERT_NOT_REACHED();
        break;
    case UnitConversion::Preserve:
        ASSERT(node.primitiveType() == valueType);
        m_value = CSSPrimitiveValue::create(value->doubleValue() + node.doubleValue(valueType), valueType);
        break;
    case UnitConversion::Canonicalize: {
        auto canonicalType = canonicalUnitTypeForUnitType(valueType);
        ASSERT(canonicalType != CSSUnitType::CSS_UNKNOWN);
        double leftValue = value->doubleValue(canonicalType);
        double rightValue = node.doubleValue(canonicalType);
        m_value = CSSPrimitiveValue::create(leftValue + rightValue, canonicalType);
        break;
    }
    }
}

void CSSCalcPrimitiveValueNode::multiply(double multiplier)
{
    Ref value = m_value;
    auto valueType = value->primitiveType();
    ASSERT(hasDoubleValue(valueType));
    m_value = CSSPrimitiveValue::create(value->doubleValue(valueType) * multiplier, valueType);
}

void CSSCalcPrimitiveValueNode::convertToUnitType(CSSUnitType unitType)
{
    Ref value = m_value;
    ASSERT(unitCategory(unitType) == unitCategory(value->primitiveType()));
    double newValue = value->doubleValue(unitType);
    m_value = CSSPrimitiveValue::create(newValue, unitType);
}

void CSSCalcPrimitiveValueNode::canonicalizeUnit()
{
    Ref value = m_value;
    auto category = calculationCategoryForCombination(value->primitiveType());
    if (category == CalculationCategory::Other)
        return;

    auto canonicalType = canonicalUnitTypeForCalculationCategory(category);
    if (canonicalType == value->primitiveType())
        return;

    double newValue = value->doubleValue(canonicalType);
    m_value = CSSPrimitiveValue::create(newValue, canonicalType);
}

std::unique_ptr<CalcExpressionNode> CSSCalcPrimitiveValueNode::createCalcExpression(const CSSToLengthConversionData& conversionData) const
{
    switch (category()) {
    case CalculationCategory::Number:
        return makeUnique<CalcExpressionNumber>(protectedValue()->floatValue());
    case CalculationCategory::Length:
        return makeUnique<CalcExpressionLength>(Length(protectedValue()->computeLength<float>(conversionData), LengthType::Fixed));
    case CalculationCategory::Percent:
    case CalculationCategory::PercentLength: {
        return makeUnique<CalcExpressionLength>(protectedValue()->convertToLength<FixedFloatConversion | PercentConversion>(conversionData));
    }
    // Only types that could be part of a Length expression can be converted
    // to a CalcExpressionNode. CalculationCategory::PercentNumber makes no sense as a Length.
    case CalculationCategory::PercentNumber:
    case CalculationCategory::Angle:
    case CalculationCategory::Time:
    case CalculationCategory::Frequency:
    case CalculationCategory::Resolution:
    case CalculationCategory::Other:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

double CSSCalcPrimitiveValueNode::doubleValue(CSSUnitType unitType) const
{
    if (hasDoubleValue(unitType)) {
        Ref value = m_value;
        // FIXME: This should ASSERT(unitCategory(m_value->primitiveType()) == unitCategory(unitType)), but only when all callers are fixed (e.g. webkit.org/b/204826).
        if (unitCategory(value->primitiveType()) != unitCategory(unitType)) {
            LOG_WITH_STREAM(Calc, stream << "Calling doubleValue() with unit " << unitType << " on a node of unit type " << m_value->primitiveType() << " which is incompatible");
            return 0;
        }

        return value->doubleValue(unitType);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

double CSSCalcPrimitiveValueNode::computeLengthPx(const CSSToLengthConversionData& conversionData) const
{
    switch (category()) {
    case CalculationCategory::Length:
        return protectedValue()->computeLength<double>(conversionData);
    case CalculationCategory::Percent:
    case CalculationCategory::Number:
        return protectedValue()->doubleValue();
    case CalculationCategory::PercentLength:
    case CalculationCategory::PercentNumber:
    case CalculationCategory::Angle:
    case CalculationCategory::Time:
    case CalculationCategory::Frequency:
    case CalculationCategory::Resolution:
    case CalculationCategory::Other:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void CSSCalcPrimitiveValueNode::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    protectedValue()->collectComputedStyleDependencies(dependencies);
}

bool CSSCalcPrimitiveValueNode::isZero() const
{
    return !protectedValue()->doubleValue();
}

bool CSSCalcPrimitiveValueNode::equals(const CSSCalcExpressionNode& other) const
{
    if (type() != other.type())
        return false;

    return compareCSSValue(m_value, static_cast<const CSSCalcPrimitiveValueNode&>(other).m_value);
}

void CSSCalcPrimitiveValueNode::dump(TextStream& ts) const
{
    ts << "value " << protectedValue()->customCSSText() << " (category: " << category() << ", type: " << primitiveType() << ")";
}

}
