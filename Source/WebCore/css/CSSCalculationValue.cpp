/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2014, 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSCalculationValue.h"

#include "CSSParser.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValueMappings.h"
#include "Logging.h"
#include "StyleResolver.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

static const int maxExpressionDepth = 100;

namespace WebCore {
class CSSCalcPrimitiveValueNode;
class CSSCalcOperationNode;
class CSSCalcNegateNode;
class CSSCalcInvertNode;
}

SPECIALIZE_TYPE_TRAITS_CSSCALCEXPRESSION_NODE(CSSCalcPrimitiveValueNode, type() == WebCore::CSSCalcExpressionNode::Type::CssCalcPrimitiveValue)
SPECIALIZE_TYPE_TRAITS_CSSCALCEXPRESSION_NODE(CSSCalcOperationNode, type() == WebCore::CSSCalcExpressionNode::Type::CssCalcOperation)
SPECIALIZE_TYPE_TRAITS_CSSCALCEXPRESSION_NODE(CSSCalcNegateNode, type() == WebCore::CSSCalcExpressionNode::Type::CssCalcNegate)
SPECIALIZE_TYPE_TRAITS_CSSCALCEXPRESSION_NODE(CSSCalcInvertNode, type() == WebCore::CSSCalcExpressionNode::Type::CssCalcInvert)

namespace WebCore {

static RefPtr<CSSCalcExpressionNode> createCSS(const CalcExpressionNode&, const RenderStyle&);
static RefPtr<CSSCalcExpressionNode> createCSS(const Length&, const RenderStyle&);

static TextStream& operator<<(TextStream& ts, const CSSCalcExpressionNode& node)
{
    node.dump(ts);
    return ts;
}

static CalculationCategory calcUnitCategory(CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::CSS_NUMBER:
        return CalculationCategory::Number;
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_EXS:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_REMS:
    case CSSUnitType::CSS_CHS:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
        return CalculationCategory::Length;
    case CSSUnitType::CSS_PERCENTAGE:
        return CalculationCategory::Percent;
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
        return CalculationCategory::Angle;
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
        return CalculationCategory::Time;
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
        return CalculationCategory::Frequency;
    default:
        return CalculationCategory::Other;
    }
}

static CalculationCategory calculationCategoryForCombination(CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::CSS_NUMBER:
        return CalculationCategory::Number;
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_Q:
        return CalculationCategory::Length;
    case CSSUnitType::CSS_PERCENTAGE:
        return CalculationCategory::Percent;
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
        return CalculationCategory::Angle;
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
        return CalculationCategory::Time;
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
        return CalculationCategory::Frequency;
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_EXS:
    case CSSUnitType::CSS_REMS:
    case CSSUnitType::CSS_CHS:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    default:
        return CalculationCategory::Other;
    }
}

static CSSUnitType canonicalUnitTypeForCalculationCategory(CalculationCategory category)
{
    switch (category) {
    case CalculationCategory::Number: return CSSUnitType::CSS_NUMBER;
    case CalculationCategory::Length: return CSSUnitType::CSS_PX;
    case CalculationCategory::Percent: return CSSUnitType::CSS_PERCENTAGE;
    case CalculationCategory::Angle: return CSSUnitType::CSS_DEG;
    case CalculationCategory::Time: return CSSUnitType::CSS_MS;
    case CalculationCategory::Frequency: return CSSUnitType::CSS_HZ;
    case CalculationCategory::Other:
    case CalculationCategory::PercentNumber:
    case CalculationCategory::PercentLength:
        ASSERT_NOT_REACHED();
        break;
    }
    return CSSUnitType::CSS_UNKNOWN;
}

static bool hasDoubleValue(CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_EMS:
    case CSSUnitType::CSS_EXS:
    case CSSUnitType::CSS_CHS:
    case CSSUnitType::CSS_REMS:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_Q:
        return true;
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_COUNTER:
    case CSSUnitType::CSS_RECT:
    case CSSUnitType::CSS_RGBCOLOR:
    case CSSUnitType::CSS_PAIR:
    case CSSUnitType::CSS_UNICODE_RANGE:
    case CSSUnitType::CSS_COUNTER_NAME:
    case CSSUnitType::CSS_SHAPE:
    case CSSUnitType::CSS_QUAD:
    case CSSUnitType::CSS_QUIRKY_EMS:
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_VALUE_ID:
        return false;
    };
    ASSERT_NOT_REACHED();
    return false;
}

static CSSValueID functionFromOperator(CalcOperator op)
{
    switch (op) {
    case CalcOperator::Add:
    case CalcOperator::Subtract:
    case CalcOperator::Multiply:
    case CalcOperator::Divide:
        return CSSValueCalc;
    case CalcOperator::Min:
        return CSSValueMin;
    case CalcOperator::Max:
        return CSSValueMax;
    case CalcOperator::Clamp:
        return CSSValueClamp;
    }
    return CSSValueCalc;
}

#if !LOG_DISABLED
static String prettyPrintNode(const CSSCalcExpressionNode& node)
{
    TextStream multilineStream;
    multilineStream << node;
    return multilineStream.release();
}

static String prettyPrintNodes(const Vector<Ref<CSSCalcExpressionNode>>& nodes)
{
    TextStream multilineStream;
    multilineStream << nodes;
    return multilineStream.release();
}
#endif

class CSSCalcPrimitiveValueNode final : public CSSCalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<CSSCalcPrimitiveValueNode> create(Ref<CSSPrimitiveValue>&& value)
    {
        return adoptRef(*new CSSCalcPrimitiveValueNode(WTFMove(value)));
    }

    static RefPtr<CSSCalcPrimitiveValueNode> create(double value, CSSUnitType type)
    {
        if (!std::isfinite(value))
            return nullptr;
        return adoptRef(new CSSCalcPrimitiveValueNode(CSSPrimitiveValue::create(value, type)));
    }

    String customCSSText() const
    {
        return m_value->cssText();
    }

    CSSUnitType primitiveType() const final
    {
        return m_value->primitiveType();
    }

    bool isNumericValue() const;
    bool isNegative() const;

    void negate();
    void invert();

    enum class UnitConversion {
        Invalid,
        Preserve,
        Canonicalize
    };
    void add(const CSSCalcPrimitiveValueNode&, UnitConversion = UnitConversion::Preserve);
    void multiply(double);

    void convertToUnitType(CSSUnitType);
    void canonicalizeUnit();

    const CSSPrimitiveValue& value() const { return m_value.get(); }

private:
    bool isZero() const final
    {
        return !m_value->doubleValue();
    }

    bool equals(const CSSCalcExpressionNode& other) const final;
    Type type() const final { return CssCalcPrimitiveValue; }

    std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData&) const final;
    double doubleValue(CSSUnitType) const final;

    double computeLengthPx(const CSSToLengthConversionData&) const final;
    void collectDirectComputationalDependencies(HashSet<CSSPropertyID>&) const final;
    void collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>&) const final;

    void dump(TextStream&) const final;

private:
    explicit CSSCalcPrimitiveValueNode(Ref<CSSPrimitiveValue>&& value)
        : CSSCalcExpressionNode(calcUnitCategory(value->primitiveType()))
        , m_value(WTFMove(value))
    {
    }

    Ref<CSSPrimitiveValue> m_value;
};

// FIXME: Use calcUnitCategory?
bool CSSCalcPrimitiveValueNode::isNumericValue() const
{
    return m_value->isLength() || m_value->isNumber() || m_value->isPercentage() || m_value->isAngle()
        || m_value->isTime() || m_value->isResolution() || m_value->isFlex() || m_value->isFrequency();
}

bool CSSCalcPrimitiveValueNode::isNegative() const
{
    return isNumericValue() && m_value->doubleValue() < 0.0;
}

void CSSCalcPrimitiveValueNode::negate()
{
    ASSERT(isNumericValue());
    m_value = CSSPrimitiveValue::create(0.0 - m_value->doubleValue(), m_value->primitiveType());
}

void CSSCalcPrimitiveValueNode::invert()
{
    ASSERT(isNumericValue());
    if (!m_value->doubleValue())
        m_value = CSSPrimitiveValue::create(std::numeric_limits<double>::infinity(), m_value->primitiveType());

    m_value = CSSPrimitiveValue::create(1.0 / m_value->doubleValue(), m_value->primitiveType());
}

void CSSCalcPrimitiveValueNode::add(const CSSCalcPrimitiveValueNode& node, UnitConversion unitConversion)
{
    auto valueType = m_value->primitiveType();

    switch (unitConversion) {
    case UnitConversion::Invalid:
        ASSERT_NOT_REACHED();
        break;
    case UnitConversion::Preserve:
        ASSERT(node.primitiveType() == valueType);
        m_value = CSSPrimitiveValue::create(m_value->doubleValue() + node.doubleValue(valueType), valueType);
        break;
    case UnitConversion::Canonicalize: {
        auto valueCategory = unitCategory(valueType);
        // FIXME: It's awkward that canonicalUnitTypeForCategory() has special handling for CSSUnitCategory::Percent.
        auto canonicalType = valueCategory == CSSUnitCategory::Percent ? CSSUnitType::CSS_PERCENTAGE : canonicalUnitTypeForCategory(valueCategory);
        ASSERT(canonicalType != CSSUnitType::CSS_UNKNOWN);
        double leftValue = m_value->doubleValue(canonicalType);
        double rightValue = node.doubleValue(canonicalType);
        m_value = CSSPrimitiveValue::create(leftValue + rightValue, canonicalType);
        break;
    }
    }
}

void CSSCalcPrimitiveValueNode::multiply(double multiplier)
{
    auto valueType = m_value->primitiveType();
    ASSERT(hasDoubleValue(valueType));
    m_value = CSSPrimitiveValue::create(m_value->doubleValue(valueType) * multiplier, valueType);
}

void CSSCalcPrimitiveValueNode::convertToUnitType(CSSUnitType unitType)
{
    ASSERT(unitCategory(unitType) == unitCategory(m_value->primitiveType()));
    double newValue = m_value->doubleValue(unitType);
    m_value = CSSPrimitiveValue::create(newValue, unitType);
}

void CSSCalcPrimitiveValueNode::canonicalizeUnit()
{
    auto category = calculationCategoryForCombination(m_value->primitiveType());
    if (category == CalculationCategory::Other)
        return;

    auto canonicalType = canonicalUnitTypeForCalculationCategory(category);
    if (canonicalType == m_value->primitiveType())
        return;

    double newValue = m_value->doubleValue(canonicalType);
    m_value = CSSPrimitiveValue::create(newValue, canonicalType);
}

std::unique_ptr<CalcExpressionNode> CSSCalcPrimitiveValueNode::createCalcExpression(const CSSToLengthConversionData& conversionData) const
{
    switch (category()) {
    case CalculationCategory::Number:
        return makeUnique<CalcExpressionNumber>(m_value->floatValue());
    case CalculationCategory::Length:
        return makeUnique<CalcExpressionLength>(Length(m_value->computeLength<float>(conversionData), WebCore::Fixed));
    case CalculationCategory::Percent:
    case CalculationCategory::PercentLength: {
        return makeUnique<CalcExpressionLength>(m_value->convertToLength<FixedFloatConversion | PercentConversion>(conversionData));
    }
    // Only types that could be part of a Length expression can be converted
    // to a CalcExpressionNode. CalculationCategory::PercentNumber makes no sense as a Length.
    case CalculationCategory::PercentNumber:
    case CalculationCategory::Angle:
    case CalculationCategory::Time:
    case CalculationCategory::Frequency:
    case CalculationCategory::Other:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

double CSSCalcPrimitiveValueNode::doubleValue(CSSUnitType unitType) const
{
    if (hasDoubleValue(unitType)) {
        // FIXME: This should ASSERT(unitCategory(m_value->primitiveType()) == unitCategory(unitType)), but only when all callers are fixed (e.g. webkit.org/b/204826).
        if (unitCategory(m_value->primitiveType()) != unitCategory(unitType)) {
            LOG_WITH_STREAM(Calc, stream << "Calling doubleValue() with unit " << unitType << " on a node of unit type " << m_value->primitiveType() << " which is incompatible");
            return 0;
        }

        return m_value->doubleValue(unitType);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

double CSSCalcPrimitiveValueNode::computeLengthPx(const CSSToLengthConversionData& conversionData) const
{
    switch (category()) {
    case CalculationCategory::Length:
        return m_value->computeLength<double>(conversionData);
    case CalculationCategory::Percent:
    case CalculationCategory::Number:
        return m_value->doubleValue();
    case CalculationCategory::PercentLength:
    case CalculationCategory::PercentNumber:
    case CalculationCategory::Angle:
    case CalculationCategory::Time:
    case CalculationCategory::Frequency:
    case CalculationCategory::Other:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void CSSCalcPrimitiveValueNode::collectDirectComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    m_value->collectDirectComputationalDependencies(values);
}

void CSSCalcPrimitiveValueNode::collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    m_value->collectDirectRootComputationalDependencies(values);
}

bool CSSCalcPrimitiveValueNode::equals(const CSSCalcExpressionNode& other) const
{
    if (type() != other.type())
        return false;

    return compareCSSValue(m_value, static_cast<const CSSCalcPrimitiveValueNode&>(other).m_value);
}

void CSSCalcPrimitiveValueNode::dump(TextStream& ts) const
{
    ts << "value " << m_value->customCSSText() << " (category: " << category() << ", type: " << primitiveType() << ")";
}

class CSSCalcNegateNode final : public CSSCalcExpressionNode {
public:
    static Ref<CSSCalcNegateNode> create(Ref<CSSCalcExpressionNode>&& child)
    {
        return adoptRef(*new CSSCalcNegateNode(WTFMove(child)));
    }

    const CSSCalcExpressionNode& child() const { return m_child.get(); }
    CSSCalcExpressionNode& child() { return m_child.get(); }

    void setChild(Ref<CSSCalcExpressionNode>&& child) { m_child = WTFMove(child); }

private:
    CSSCalcNegateNode(Ref<CSSCalcExpressionNode>&& child)
        : CSSCalcExpressionNode(child->category())
        , m_child(WTFMove(child))
    {
    }

    std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData&) const final;

    bool isZero() const final { return m_child->isZero(); }
    double doubleValue(CSSUnitType unitType) const final { return -m_child->doubleValue(unitType); }
    double computeLengthPx(const CSSToLengthConversionData& conversionData) const final { return -m_child->computeLengthPx(conversionData); }
    Type type() const final { return Type::CssCalcNegate; }
    CSSUnitType primitiveType() const final { return m_child->primitiveType(); }

    void collectDirectComputationalDependencies(HashSet<CSSPropertyID>& properties) const final { m_child->collectDirectComputationalDependencies(properties); }
    void collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>& properties) const final { m_child->collectDirectRootComputationalDependencies(properties); }

    void dump(TextStream&) const final;

    Ref<CSSCalcExpressionNode> m_child;
};

std::unique_ptr<CalcExpressionNode> CSSCalcNegateNode::createCalcExpression(const CSSToLengthConversionData& conversionData) const
{
    auto childNode = m_child->createCalcExpression(conversionData);
    return makeUnique<CalcExpressionNegation>(WTFMove(childNode));
}

void CSSCalcNegateNode::dump(TextStream& ts) const
{
    ts << "-" << m_child.get();
}

class CSSCalcInvertNode final : public CSSCalcExpressionNode {
public:
    static Ref<CSSCalcInvertNode> create(Ref<CSSCalcExpressionNode>&& child)
    {
        return adoptRef(*new CSSCalcInvertNode(WTFMove(child)));
    }

    const CSSCalcExpressionNode& child() const { return m_child.get(); }
    CSSCalcExpressionNode& child() { return m_child.get(); }

    void setChild(Ref<CSSCalcExpressionNode>&& child) { m_child = WTFMove(child); }

private:
    CSSCalcInvertNode(Ref<CSSCalcExpressionNode>&& child)
        : CSSCalcExpressionNode(child->category())
        , m_child(WTFMove(child))
    {
    }

    std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData&) const final;

    bool isZero() const final { return m_child->isZero(); }
    double doubleValue(CSSUnitType) const final;
    double computeLengthPx(const CSSToLengthConversionData&) const final;
    Type type() const final { return Type::CssCalcInvert; }
    CSSUnitType primitiveType() const final { return m_child->primitiveType(); }

    void collectDirectComputationalDependencies(HashSet<CSSPropertyID>& properties) const final { m_child->collectDirectComputationalDependencies(properties); }
    void collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>& properties) const final { m_child->collectDirectRootComputationalDependencies(properties); }

    void dump(TextStream&) const final;

    Ref<CSSCalcExpressionNode> m_child;
};

std::unique_ptr<CalcExpressionNode> CSSCalcInvertNode::createCalcExpression(const CSSToLengthConversionData& conversionData) const
{
    auto childNode = m_child->createCalcExpression(conversionData);
    return makeUnique<CalcExpressionInversion>(WTFMove(childNode));
}

double CSSCalcInvertNode::doubleValue(CSSUnitType unitType) const
{
    auto childValue = m_child->doubleValue(unitType);
    if (!childValue)
        return std::numeric_limits<double>::infinity();
    return 1.0 / childValue;
}

double CSSCalcInvertNode::computeLengthPx(const CSSToLengthConversionData& conversionData) const
{
    auto childValue = m_child->computeLengthPx(conversionData);
    if (!childValue)
        return std::numeric_limits<double>::infinity();
    return 1.0 / childValue;
}

void CSSCalcInvertNode::dump(TextStream& ts) const
{
    ts << "1/" << m_child.get();
}

// This is the result of the "To add two types type1 and type2, perform the following steps:" rules.

static const CalculationCategory addSubtractResult[static_cast<unsigned>(CalculationCategory::Angle)][static_cast<unsigned>(CalculationCategory::Angle)] = {
//    CalculationCategory::Number         CalculationCategory::Length         CalculationCategory::Percent        CalculationCategory::PercentNumber  CalculationCategory::PercentLength
    { CalculationCategory::Number,        CalculationCategory::Other,         CalculationCategory::PercentNumber, CalculationCategory::PercentNumber, CalculationCategory::Other }, //         CalculationCategory::Number
    { CalculationCategory::Other,         CalculationCategory::Length,        CalculationCategory::PercentLength, CalculationCategory::Other,         CalculationCategory::PercentLength }, // CalculationCategory::Length
    { CalculationCategory::PercentNumber, CalculationCategory::PercentLength, CalculationCategory::Percent,       CalculationCategory::PercentNumber, CalculationCategory::PercentLength }, // CalculationCategory::Percent
    { CalculationCategory::PercentNumber, CalculationCategory::Other,         CalculationCategory::PercentNumber, CalculationCategory::PercentNumber, CalculationCategory::Other }, //         CalculationCategory::PercentNumber
    { CalculationCategory::Other,         CalculationCategory::PercentLength, CalculationCategory::PercentLength, CalculationCategory::Other,         CalculationCategory::PercentLength }, // CalculationCategory::PercentLength
};

static CalculationCategory determineCategory(const CSSCalcExpressionNode& leftSide, const CSSCalcExpressionNode& rightSide, CalcOperator op)
{
    CalculationCategory leftCategory = leftSide.category();
    CalculationCategory rightCategory = rightSide.category();
    ASSERT(leftCategory < CalculationCategory::Other);
    ASSERT(rightCategory < CalculationCategory::Other);

    switch (op) {
    case CalcOperator::Add:
    case CalcOperator::Subtract:
        if (leftCategory < CalculationCategory::Angle && rightCategory < CalculationCategory::Angle)
            return addSubtractResult[static_cast<unsigned>(leftCategory)][static_cast<unsigned>(rightCategory)];
        if (leftCategory == rightCategory)
            return leftCategory;
        return CalculationCategory::Other;
    case CalcOperator::Multiply:
        if (leftCategory != CalculationCategory::Number && rightCategory != CalculationCategory::Number)
            return CalculationCategory::Other;
        return leftCategory == CalculationCategory::Number ? rightCategory : leftCategory;
    case CalcOperator::Divide:
        if (rightCategory != CalculationCategory::Number || rightSide.isZero())
            return CalculationCategory::Other;
        return leftCategory;
    case CalcOperator::Min:
    case CalcOperator::Max:
    case CalcOperator::Clamp:
        ASSERT_NOT_REACHED();
        return CalculationCategory::Other;
    }

    ASSERT_NOT_REACHED();
    return CalculationCategory::Other;
}

// FIXME: Need to implement correct category computation per:
// <https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-invert-a-type>
// To invert a type type, perform the following steps:
// Let result be a new type with an initially empty ordered map and an initially null percent hint
// For each unit → exponent of type, set result[unit] to (-1 * exponent).
static CalculationCategory categoryForInvert(CalculationCategory category)
{
    return category;
}

static CalculationCategory determineCategory(const Vector<Ref<CSSCalcExpressionNode>>& nodes, CalcOperator op)
{
    if (nodes.isEmpty())
        return CalculationCategory::Other;

    auto currentCategory = nodes[0]->category();

    for (unsigned i = 1; i < nodes.size(); ++i) {
        const auto& node = nodes[i].get();
        
        auto usedOperator = op;
        if (node.type() == CSSCalcExpressionNode::Type::CssCalcInvert)
            usedOperator = CalcOperator::Divide;

        auto nextCategory = node.category();

        switch (usedOperator) {
        case CalcOperator::Add:
        case CalcOperator::Subtract:
            // <https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-add-two-types>
            // At a + or - sub-expression, attempt to add the types of the left and right arguments.
            // If this returns failure, the entire calculation’s type is failure. Otherwise, the sub-expression’s type is the returned type.
            if (currentCategory < CalculationCategory::Angle && nextCategory < CalculationCategory::Angle)
                currentCategory = addSubtractResult[static_cast<unsigned>(currentCategory)][static_cast<unsigned>(nextCategory)];
            else if (currentCategory != nextCategory)
                return CalculationCategory::Other;
            break;

        case CalcOperator::Multiply:
            // <https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-multiply-two-types>
            // At a * sub-expression, multiply the types of the left and right arguments. The sub-expression’s type is the returned result.
            if (currentCategory != CalculationCategory::Number && nextCategory != CalculationCategory::Number)
                return CalculationCategory::Other;

            currentCategory = currentCategory == CalculationCategory::Number ? nextCategory : currentCategory;
            break;

        case CalcOperator::Divide: {
            auto invertCategory = categoryForInvert(nextCategory);
            
            // At a / sub-expression, let left type be the result of finding the types of its left argument,
            // and right type be the result of finding the types of its right argument and then inverting it.
            // The sub-expression’s type is the result of multiplying the left type and right type.
            if (invertCategory != CalculationCategory::Number || node.isZero())
                return CalculationCategory::Other;
            break;
        }

        case CalcOperator::Min:
        case CalcOperator::Max:
        case CalcOperator::Clamp:
            // The type of a min(), max(), or clamp() expression is the result of adding the types of its comma-separated calculations
            return CalculationCategory::Other;
        }
    }

    return currentCategory;
}

static CalculationCategory resolvedTypeForMinOrMaxOrClamp(CalculationCategory category, CalculationCategory destinationCategory)
{
    switch (category) {
    case CalculationCategory::Number:
    case CalculationCategory::Length:
    case CalculationCategory::PercentNumber:
    case CalculationCategory::PercentLength:
    case CalculationCategory::Angle:
    case CalculationCategory::Time:
    case CalculationCategory::Frequency:
    case CalculationCategory::Other:
        return category;

    case CalculationCategory::Percent:
        if (destinationCategory == CalculationCategory::Length)
            return CalculationCategory::PercentLength;
        if (destinationCategory == CalculationCategory::Number)
            return CalculationCategory::PercentNumber;
        return category;
    }

    return CalculationCategory::Other;
}

static bool isSamePair(CalculationCategory a, CalculationCategory b, CalculationCategory x, CalculationCategory y)
{
    return (a == x && b == y) || (a == y && b == x);
}

class CSSCalcOperationNode final : public CSSCalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static RefPtr<CSSCalcOperationNode> create(CalcOperator, RefPtr<CSSCalcExpressionNode>&& leftSide, RefPtr<CSSCalcExpressionNode>&& rightSide);
    static RefPtr<CSSCalcOperationNode> createSum(Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createProduct(Vector<Ref<CSSCalcExpressionNode>>&& values);
    static RefPtr<CSSCalcOperationNode> createMinOrMaxOrClamp(CalcOperator, Vector<Ref<CSSCalcExpressionNode>>&& values, CalculationCategory destinationCategory);

    static Ref<CSSCalcExpressionNode> simplify(Ref<CSSCalcExpressionNode>&&);

    static void buildCSSText(const CSSCalcExpressionNode&, StringBuilder&);

    CalcOperator calcOperator() const { return m_operator; }
    bool isCalcSumNode() const { return m_operator == CalcOperator::Add; }
    bool isCalcProductNode() const { return m_operator == CalcOperator::Multiply; }
    bool isMinOrMaxNode() const { return m_operator == CalcOperator::Min || m_operator == CalcOperator::Max; }
    bool shouldSortChildren() const { return isCalcSumNode() || isCalcProductNode(); }

    void hoistChildrenWithOperator(CalcOperator);
    void combineChildren();
    
    bool canCombineAllChildren() const;

    const Vector<Ref<CSSCalcExpressionNode>>& children() const { return m_children; }
    Vector<Ref<CSSCalcExpressionNode>>& children() { return m_children; }

private:
    CSSCalcOperationNode(CalculationCategory category, CalcOperator op, Ref<CSSCalcExpressionNode>&& leftSide, Ref<CSSCalcExpressionNode>&& rightSide)
        : CSSCalcExpressionNode(category)
        , m_operator(op)
    {
        m_children.reserveInitialCapacity(2);
        m_children.uncheckedAppend(WTFMove(leftSide));
        m_children.uncheckedAppend(WTFMove(rightSide));
    }

    CSSCalcOperationNode(CalculationCategory category, CalcOperator op, Vector<Ref<CSSCalcExpressionNode>>&& children)
        : CSSCalcExpressionNode(category)
        , m_operator(op)
        , m_children(WTFMove(children))
    {
    }

    Type type() const final { return CssCalcOperation; }

    bool isZero() const final
    {
        return !doubleValue(primitiveType());
    }

    bool equals(const CSSCalcExpressionNode&) const final;

    std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData&) const final;

    CSSUnitType primitiveType() const final;
    double doubleValue(CSSUnitType) const final;
    double computeLengthPx(const CSSToLengthConversionData&) const final;

    void collectDirectComputationalDependencies(HashSet<CSSPropertyID>&) const final;
    void collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>&) const final;

    void dump(TextStream&) const final;

    static CSSCalcExpressionNode* getNumberSide(CSSCalcExpressionNode& leftSide, CSSCalcExpressionNode& rightSide)
    {
        if (leftSide.category() == CalculationCategory::Number)
            return &leftSide;
        if (rightSide.category() == CalculationCategory::Number)
            return &rightSide;
        return nullptr;
    }

    double evaluate(const Vector<double>& children) const
    {
        return evaluateOperator(m_operator, children);
    }

    static double evaluateOperator(CalcOperator, const Vector<double>&);
    static Ref<CSSCalcExpressionNode> simplifyNode(Ref<CSSCalcExpressionNode>&&, int depth);
    static Ref<CSSCalcExpressionNode> simplifyRecursive(Ref<CSSCalcExpressionNode>&&, int depth);
    
    enum class GroupingParens {
        Omit,
        Include
    };
    static void buildCSSTextRecursive(const CSSCalcExpressionNode&, StringBuilder&, GroupingParens = GroupingParens::Include);

    const CalcOperator m_operator;
    Vector<Ref<CSSCalcExpressionNode>> m_children;
};

RefPtr<CSSCalcOperationNode> CSSCalcOperationNode::create(CalcOperator op, RefPtr<CSSCalcExpressionNode>&& leftSide, RefPtr<CSSCalcExpressionNode>&& rightSide)
{
    if (!leftSide || !rightSide)
        return nullptr;

    ASSERT(op == CalcOperator::Add || op == CalcOperator::Multiply);

    ASSERT(leftSide->category() < CalculationCategory::Other);
    ASSERT(rightSide->category() < CalculationCategory::Other);

    auto newCategory = determineCategory(*leftSide, *rightSide, op);
    if (newCategory == CalculationCategory::Other) {
        LOG_WITH_STREAM(Calc, stream << "Failed to create CSSCalcOperationNode " << op << " node because unable to determine category from " << prettyPrintNode(*leftSide) << " and " << *rightSide);
        return nullptr;
    }

    return adoptRef(new CSSCalcOperationNode(newCategory, op, leftSide.releaseNonNull(), rightSide.releaseNonNull()));
}

RefPtr<CSSCalcOperationNode> CSSCalcOperationNode::createSum(Vector<Ref<CSSCalcExpressionNode>>&& values)
{
    if (values.isEmpty())
        return nullptr;

    auto newCategory = determineCategory(values, CalcOperator::Add);
    if (newCategory == CalculationCategory::Other) {
        LOG_WITH_STREAM(Calc, stream << "Failed to create sum node because unable to determine category from " << prettyPrintNodes(values));
        newCategory = determineCategory(values, CalcOperator::Add);
        return nullptr;
    }

    return adoptRef(new CSSCalcOperationNode(newCategory, CalcOperator::Add, WTFMove(values)));
}

RefPtr<CSSCalcOperationNode> CSSCalcOperationNode::createProduct(Vector<Ref<CSSCalcExpressionNode>>&& values)
{
    if (values.isEmpty())
        return nullptr;

    auto newCategory = determineCategory(values, CalcOperator::Multiply);
    if (newCategory == CalculationCategory::Other) {
        LOG_WITH_STREAM(Calc, stream << "Failed to create product node because unable to determine category from " << prettyPrintNodes(values));
        return nullptr;
    }

    return adoptRef(new CSSCalcOperationNode(newCategory, CalcOperator::Multiply, WTFMove(values)));
}

RefPtr<CSSCalcOperationNode> CSSCalcOperationNode::createMinOrMaxOrClamp(CalcOperator op, Vector<Ref<CSSCalcExpressionNode>>&& values, CalculationCategory destinationCategory)
{
    ASSERT(op == CalcOperator::Min || op == CalcOperator::Max || op == CalcOperator::Clamp);
    ASSERT_IMPLIES(op == CalcOperator::Clamp, values.size() == 3);

    Optional<CalculationCategory> category = WTF::nullopt;
    for (auto& value : values) {
        auto valueCategory = resolvedTypeForMinOrMaxOrClamp(value->category(), destinationCategory);

        ASSERT(valueCategory < CalculationCategory::Other);
        if (!category) {
            if (valueCategory == CalculationCategory::Other) {
                LOG_WITH_STREAM(Calc, stream << "Failed to create CSSCalcOperationNode " << op << " node because unable to determine category from " << prettyPrintNodes(values));
                return nullptr;
            }
            category = valueCategory;
        }

        if (category != valueCategory) {
            if (isSamePair(category.value(), valueCategory, CalculationCategory::Length, CalculationCategory::PercentLength)) {
                category = CalculationCategory::PercentLength;
                continue;
            }
            if (isSamePair(category.value(), valueCategory, CalculationCategory::Number, CalculationCategory::PercentNumber)) {
                category = CalculationCategory::PercentNumber;
                continue;
            }
            return nullptr;
        }
    }

    return adoptRef(new CSSCalcOperationNode(category.value(), op, WTFMove(values)));
}

void CSSCalcOperationNode::hoistChildrenWithOperator(CalcOperator op)
{
    ASSERT(op == CalcOperator::Add || op == CalcOperator::Multiply);

    auto hasChildWithOperator = [&] (CalcOperator op) {
        for (auto& child : m_children) {
            if (is<CSSCalcOperationNode>(child.get()) && downcast<CSSCalcOperationNode>(child.get()).calcOperator() == op)
                return true;
        }
        return false;
    };

    if (!hasChildWithOperator(op))
        return;

    Vector<Ref<CSSCalcExpressionNode>> newChildren;
    for (auto& child : m_children) {
        if (is<CSSCalcOperationNode>(child.get()) && downcast<CSSCalcOperationNode>(child.get()).calcOperator() == op) {
            auto& children = downcast<CSSCalcOperationNode>(child.get()).children();
            for (auto& childToMove : children)
                newChildren.append(WTFMove(childToMove));
        } else
            newChildren.append(WTFMove(child));
    }
    
    newChildren.shrinkToFit();
    m_children = WTFMove(newChildren);
}

enum class SortingCategory {
    Number,
    Percent,
    Dimension,
    Other
};

static SortingCategory sortingCategoryForType(CSSUnitType unitType)
{
    static constexpr SortingCategory sortOrder[] = {
        SortingCategory::Number,        // CalculationCategory::Number,
        SortingCategory::Dimension,     // CalculationCategory::Length,
        SortingCategory::Percent,       // CalculationCategory::Percent,
        SortingCategory::Number,        // CalculationCategory::PercentNumber,
        SortingCategory::Dimension,     // CalculationCategory::PercentLength,
        SortingCategory::Dimension,     // CalculationCategory::Angle,
        SortingCategory::Dimension,     // CalculationCategory::Time,
        SortingCategory::Dimension,     // CalculationCategory::Frequency,
        SortingCategory::Other,         // UOther
    };

    COMPILE_ASSERT(ARRAY_SIZE(sortOrder) == static_cast<unsigned>(CalculationCategory::Other) + 1, sortOrder_size_should_match_UnitCategory);
    return sortOrder[static_cast<unsigned>(calcUnitCategory(unitType))];
}

static SortingCategory sortingCategory(const CSSCalcExpressionNode& node)
{
    if (is<CSSCalcPrimitiveValueNode>(node))
        return sortingCategoryForType(node.primitiveType());

    return SortingCategory::Other;
}

static CSSUnitType primitiveTypeForCombination(const CSSCalcExpressionNode& node)
{
    if (is<CSSCalcPrimitiveValueNode>(node))
        return node.primitiveType();
    
    return CSSUnitType::CSS_UNKNOWN;
}

static CSSCalcPrimitiveValueNode::UnitConversion conversionToAddValuesWithTypes(CSSUnitType firstType, CSSUnitType secondType)
{
    if (firstType == CSSUnitType::CSS_UNKNOWN || secondType == CSSUnitType::CSS_UNKNOWN)
        return CSSCalcPrimitiveValueNode::UnitConversion::Invalid;

    auto firstCategory = calculationCategoryForCombination(firstType);

    // Compatible types.
    if (firstCategory != CalculationCategory::Other && firstCategory == calculationCategoryForCombination(secondType))
        return CSSCalcPrimitiveValueNode::UnitConversion::Canonicalize;

    // Matching types.
    if (firstType == secondType && hasDoubleValue(firstType))
        return CSSCalcPrimitiveValueNode::UnitConversion::Preserve;

    return CSSCalcPrimitiveValueNode::UnitConversion::Invalid;
}

bool CSSCalcOperationNode::canCombineAllChildren() const
{
    if (m_children.size() < 2)
        return false;

    if (!is<CSSCalcPrimitiveValueNode>(m_children[0]))
        return false;

    auto firstUnitType = m_children[0]->primitiveType();
    auto firstCategory = calculationCategoryForCombination(m_children[0]->primitiveType());

    for (unsigned i = 1; i < m_children.size(); ++i) {
        auto& node = m_children[i];

        if (!is<CSSCalcPrimitiveValueNode>(node))
            return false;

        auto nodeUnitType = node->primitiveType();
        auto nodeCategory = calculationCategoryForCombination(nodeUnitType);

        if (nodeCategory != firstCategory)
            return false;

        if (nodeCategory == CalculationCategory::Other && nodeUnitType != firstUnitType)
            return false;
        
        if (!hasDoubleValue(nodeUnitType))
            return false;
    }

    return true;
}

void CSSCalcOperationNode::combineChildren()
{
    if (m_children.size() < 2)
        return;

    if (shouldSortChildren()) {
        // <https://drafts.csswg.org/css-values-4/#sort-a-calculations-children>
        std::stable_sort(m_children.begin(), m_children.end(), [](const auto& first, const auto& second) {
            // Sort order: number, percentage, dimension, other.
            SortingCategory firstCategory = sortingCategory(first.get());
            SortingCategory secondCategory = sortingCategory(second.get());
            
            if (firstCategory == SortingCategory::Dimension && secondCategory == SortingCategory::Dimension) {
                // If nodes contains any dimensions, remove them from nodes, sort them by their units, and append them to ret.
                auto firstUnitString = CSSPrimitiveValue::unitTypeString(first->primitiveType());
                auto secondUnitString = CSSPrimitiveValue::unitTypeString(second->primitiveType());
                return codePointCompareLessThan(firstUnitString, secondUnitString);
            }

            return static_cast<unsigned>(firstCategory) < static_cast<unsigned>(secondCategory);
        });

        LOG_WITH_STREAM(Calc, stream << "post-sort: " << *this);
    }

    if (calcOperator() == CalcOperator::Add) {
        // For each set of root’s children that are numeric values with identical units,
        // remove those children and replace them with a single numeric value containing
        // the sum of the removed nodes, and with the same unit.
        Vector<Ref<CSSCalcExpressionNode>> newChildren;
        newChildren.reserveInitialCapacity(m_children.size());
        newChildren.uncheckedAppend(m_children[0].copyRef());

        CSSUnitType previousType = primitiveTypeForCombination(newChildren[0].get());

        for (unsigned i = 1; i < m_children.size(); ++i) {
            auto& currentNode = m_children[i];
            CSSUnitType currentType = primitiveTypeForCombination(currentNode.get());

            auto conversionType = conversionToAddValuesWithTypes(previousType, currentType);
            if (conversionType != CSSCalcPrimitiveValueNode::UnitConversion::Invalid) {
                downcast<CSSCalcPrimitiveValueNode>(newChildren.last().get()).add(downcast<CSSCalcPrimitiveValueNode>(currentNode.get()), conversionType);
                continue;
            }

            previousType = primitiveTypeForCombination(currentNode);
            newChildren.uncheckedAppend(currentNode.copyRef());
        }
        
        newChildren.shrinkToFit();
        m_children = WTFMove(newChildren);
        return;
    }

    if (calcOperator() == CalcOperator::Multiply) {
        // If root has multiple children that are numbers (not percentages or dimensions),
        // remove them and replace them with a single number containing the product of the removed nodes.
        double multiplier = 1;

        // Sorting will have put the number nodes first.
        unsigned leadingNumberNodeCount = 0;
        for (auto& node : m_children) {
            auto nodeType = primitiveTypeForCombination(node.get());
            if (nodeType != CSSUnitType::CSS_NUMBER)
                break;
            
            multiplier *= node->doubleValue(CSSUnitType::CSS_NUMBER);
            ++leadingNumberNodeCount;
        }
        
        Vector<Ref<CSSCalcExpressionNode>> newChildren;
        newChildren.reserveInitialCapacity(m_children.size());
        
        // If root contains only two children, one of which is a number (not a percentage or dimension) and the other of
        // which is a Sum whose children are all numeric values, multiply all of the Sum’s children by the number, then
        // return the Sum.
        // The Sum's children simplification will have happened already.
        bool didMultipy = false;
        if (leadingNumberNodeCount && m_children.size() - leadingNumberNodeCount == 1) {
            auto multiplicandCategory = calcUnitCategory(primitiveTypeForCombination(m_children.last().get()));
            if (multiplicandCategory != CalculationCategory::Other) {
                newChildren.uncheckedAppend(m_children.last().copyRef());
                downcast<CSSCalcPrimitiveValueNode>(newChildren[0].get()).multiply(multiplier);
                didMultipy = true;
            }
        }
        
        if (!didMultipy) {
            if (leadingNumberNodeCount) {
                auto multiplierNode = CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(multiplier, CSSUnitType::CSS_NUMBER));
                newChildren.uncheckedAppend(WTFMove(multiplierNode));
            }

            for (unsigned i = leadingNumberNodeCount; i < m_children.size(); ++i)
                newChildren.uncheckedAppend(m_children[i].copyRef());
        }

        newChildren.shrinkToFit();
        m_children = WTFMove(newChildren);
    }

    if (isMinOrMaxNode() && canCombineAllChildren()) {
        auto combinedUnitType = m_children[0]->primitiveType();
        auto category = calculationCategoryForCombination(combinedUnitType);
        if (category != CalculationCategory::Other)
            combinedUnitType = canonicalUnitTypeForCalculationCategory(category);

        double resolvedValue = doubleValue(combinedUnitType);
        auto newChild = CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(resolvedValue, combinedUnitType));

        m_children.clear();
        m_children.append(WTFMove(newChild));
    }
}

// https://drafts.csswg.org/css-values-4/#simplify-a-calculation-tree

Ref<CSSCalcExpressionNode> CSSCalcOperationNode::simplify(Ref<CSSCalcExpressionNode>&& rootNode)
{
    return simplifyRecursive(WTFMove(rootNode), 0);
}

Ref<CSSCalcExpressionNode> CSSCalcOperationNode::simplifyRecursive(Ref<CSSCalcExpressionNode>&& rootNode, int depth)
{
    if (is<CSSCalcOperationNode>(rootNode)) {
        auto& operationNode = downcast<CSSCalcOperationNode>(rootNode.get());
        
        auto& children = operationNode.children();
        for (unsigned i = 0; i < children.size(); ++i) {
            auto child = children[i].copyRef();
            auto newNode = simplifyRecursive(WTFMove(child), depth + 1);
            if (newNode.ptr() != children[i].ptr())
                children[i] = WTFMove(newNode);
        }
    } else if (is<CSSCalcNegateNode>(rootNode)) {
        auto& negateNode = downcast<CSSCalcNegateNode>(rootNode.get());
        Ref<CSSCalcExpressionNode> child = negateNode.child();
        auto newNode = simplifyRecursive(WTFMove(child), depth + 1);
        if (newNode.ptr() != &negateNode.child())
            negateNode.setChild(WTFMove(newNode));
    } else if (is<CSSCalcInvertNode>(rootNode)) {
        auto& invertNode = downcast<CSSCalcInvertNode>(rootNode.get());
        Ref<CSSCalcExpressionNode> child = invertNode.child();
        auto newNode = simplifyRecursive(WTFMove(child), depth + 1);
        if (newNode.ptr() != &invertNode.child())
            invertNode.setChild(WTFMove(newNode));
    }

    return simplifyNode(WTFMove(rootNode), depth);
}

Ref<CSSCalcExpressionNode> CSSCalcOperationNode::simplifyNode(Ref<CSSCalcExpressionNode>&& rootNode, int depth)
{
    if (is<CSSCalcPrimitiveValueNode>(rootNode)) {
        // If root is a percentage that will be resolved against another value, and there is enough information
        // available to resolve it, do so, and express the resulting numeric value in the appropriate canonical
        // unit. Return the value.

        // If root is a dimension that is not expressed in its canonical unit, and there is enough information
        // available to convert it to the canonical unit, do so, and return the value.
        auto& primitiveValueNode = downcast<CSSCalcPrimitiveValueNode>(rootNode.get());
        primitiveValueNode.canonicalizeUnit();
        return WTFMove(rootNode);
    }

    // If root is an operator node that’s not one of the calc-operator nodes, and all of its children are numeric values
    // with enough information to computed the operation root represents, return the result of running root’s operation
    // using its children, expressed in the result’s canonical unit.
    if (is<CSSCalcOperationNode>(rootNode)) {
        auto& calcOperationNode = downcast<CSSCalcOperationNode>(rootNode.get());
        // Don't simplify at the root, otherwise we lose track of the operation for serialization.
        if (calcOperationNode.children().size() == 1 && depth)
            return WTFMove(calcOperationNode.children()[0]);
        
        if (calcOperationNode.isCalcSumNode()) {
            calcOperationNode.hoistChildrenWithOperator(CalcOperator::Add);
            calcOperationNode.combineChildren();
        }

        if (calcOperationNode.isCalcProductNode()) {
            calcOperationNode.hoistChildrenWithOperator(CalcOperator::Multiply);
            calcOperationNode.combineChildren();
        }
        
        if (calcOperationNode.isMinOrMaxNode())
            calcOperationNode.combineChildren();

        // If only one child remains, return the child (except at the root).
        auto shouldCombineParentWithOnlyChild = [](const CSSCalcOperationNode& parent, int depth)
        {
            if (parent.children().size() != 1)
                return false;

            // Always simplify below the root.
            if (depth)
                return true;

            // At the root, preserve the root function by only merging nodes with the same function.
            auto& child = parent.children().first();
            if (!is<CSSCalcOperationNode>(child))
                return false;

            auto parentFunction = functionFromOperator(parent.calcOperator());
            auto childFunction = functionFromOperator(downcast<CSSCalcOperationNode>(child.get()).calcOperator());
            return childFunction == parentFunction;
        };

        if (shouldCombineParentWithOnlyChild(calcOperationNode, depth))
            return WTFMove(calcOperationNode.children().first());

        return WTFMove(rootNode);
    }

    if (is<CSSCalcNegateNode>(rootNode)) {
        auto& childNode = downcast<CSSCalcNegateNode>(rootNode.get()).child();
        // If root’s child is a numeric value, return an equivalent numeric value, but with the value negated (0 - value).
        if (is<CSSCalcPrimitiveValueNode>(childNode) && downcast<CSSCalcPrimitiveValueNode>(childNode).isNumericValue()) {
            downcast<CSSCalcPrimitiveValueNode>(childNode).negate();
            return childNode;
        }
        
        // If root’s child is a Negate node, return the child’s child.
        if (is<CSSCalcNegateNode>(childNode))
            return downcast<CSSCalcNegateNode>(childNode).child();
        
        return WTFMove(rootNode);
    }

    if (is<CSSCalcInvertNode>(rootNode)) {
        auto& childNode = downcast<CSSCalcInvertNode>(rootNode.get()).child();
        // If root’s child is a number (not a percentage or dimension) return the reciprocal of the child’s value.
        if (is<CSSCalcPrimitiveValueNode>(childNode) && downcast<CSSCalcPrimitiveValueNode>(childNode).isNumericValue()) {
            downcast<CSSCalcPrimitiveValueNode>(childNode).invert();
            return childNode;
        }

        // If root’s child is an Invert node, return the child’s child.
        if (is<CSSCalcInvertNode>(childNode))
            return downcast<CSSCalcInvertNode>(childNode).child();

        return WTFMove(rootNode);
    }

    return WTFMove(rootNode);
}

CSSUnitType CSSCalcOperationNode::primitiveType() const
{
    auto unitCategory = category();
    switch (unitCategory) {
    case CalculationCategory::Number:
#if ASSERT_ENABLED
        for (auto& child : m_children)
            ASSERT(child->category() == CalculationCategory::Number);
#endif
        return CSSUnitType::CSS_NUMBER;

    case CalculationCategory::Percent: {
        if (m_children.isEmpty())
            return CSSUnitType::CSS_UNKNOWN;

        if (m_children.size() == 2) {
            if (m_children[0]->category() == CalculationCategory::Number)
                return m_children[1]->primitiveType();
            if (m_children[1]->category() == CalculationCategory::Number)
                return m_children[0]->primitiveType();
        }
        CSSUnitType firstType = m_children[0]->primitiveType();
        for (auto& child : m_children) {
            if (firstType != child->primitiveType())
                return CSSUnitType::CSS_UNKNOWN;
        }
        return firstType;
    }

    case CalculationCategory::Length:
    case CalculationCategory::Angle:
    case CalculationCategory::Time:
    case CalculationCategory::Frequency:
        if (m_children.size() == 1)
            return m_children.first()->primitiveType();
        return canonicalUnitTypeForCalculationCategory(unitCategory);

    case CalculationCategory::PercentLength:
    case CalculationCategory::PercentNumber:
    case CalculationCategory::Other:
        return CSSUnitType::CSS_UNKNOWN;
    }
    ASSERT_NOT_REACHED();
    return CSSUnitType::CSS_UNKNOWN;
}

std::unique_ptr<CalcExpressionNode> CSSCalcOperationNode::createCalcExpression(const CSSToLengthConversionData& conversionData) const
{
    Vector<std::unique_ptr<CalcExpressionNode>> nodes;
    nodes.reserveInitialCapacity(m_children.size());

    for (auto& child : m_children) {
        auto node = child->createCalcExpression(conversionData);
        if (!node)
            return nullptr;
        nodes.uncheckedAppend(WTFMove(node));
    }
    return makeUnique<CalcExpressionOperation>(WTFMove(nodes), m_operator);
}

double CSSCalcOperationNode::doubleValue(CSSUnitType unitType) const
{
    bool allowNumbers = calcOperator() == CalcOperator::Multiply;

    return evaluate(m_children.map([&] (auto& child) {
        CSSUnitType childType = unitType;
        if (allowNumbers && unitType != CSSUnitType::CSS_NUMBER && child->primitiveType() == CSSUnitType::CSS_NUMBER)
            childType = CSSUnitType::CSS_NUMBER;
        return child->doubleValue(childType);
    }));
}

double CSSCalcOperationNode::computeLengthPx(const CSSToLengthConversionData& conversionData) const
{
    return evaluate(m_children.map([&] (auto& child) {
        return child->computeLengthPx(conversionData);
    }));
}

void CSSCalcOperationNode::collectDirectComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    for (auto& child : m_children)
        child->collectDirectComputationalDependencies(values);
}

void CSSCalcOperationNode::collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    for (auto& child : m_children)
        child->collectDirectRootComputationalDependencies(values);
}

void CSSCalcOperationNode::buildCSSText(const CSSCalcExpressionNode& node, StringBuilder& builder)
{
    auto shouldOutputEnclosingCalc = [](const CSSCalcExpressionNode& rootNode) {
        if (is<CSSCalcOperationNode>(rootNode)) {
            auto& operationNode = downcast<CSSCalcOperationNode>(rootNode);
            return operationNode.isCalcSumNode() || operationNode.isCalcProductNode();
        }
        return true;
    };
    
    bool outputCalc = shouldOutputEnclosingCalc(node);
    if (outputCalc)
        builder.append("calc(");

    buildCSSTextRecursive(node, builder, GroupingParens::Omit);

    if (outputCalc)
        builder.append(')');
}

static const char* functionPrefixForOperator(CalcOperator op)
{
    switch (op) {
    case CalcOperator::Add:
    case CalcOperator::Subtract:
    case CalcOperator::Multiply:
    case CalcOperator::Divide:
        ASSERT_NOT_REACHED();
        return "";
    case CalcOperator::Min: return "min(";
    case CalcOperator::Max: return "max(";
    case CalcOperator::Clamp: return "clamp(";
    }
    
    return "";
}

// <https://drafts.csswg.org/css-values-4/#serialize-a-calculation-tree>
void CSSCalcOperationNode::buildCSSTextRecursive(const CSSCalcExpressionNode& node, StringBuilder& builder, GroupingParens parens)
{
    // If root is a numeric value, or a non-math function, serialize root per the normal rules for it and return the result.
    if (is<CSSCalcPrimitiveValueNode>(node)) {
        auto& valueNode = downcast<CSSCalcPrimitiveValueNode>(node);
        builder.append(valueNode.customCSSText());
        return;
    }

    if (is<CSSCalcOperationNode>(node)) {
        auto& operationNode = downcast<CSSCalcOperationNode>(node);

        if (operationNode.isCalcSumNode()) {
            // If root is a Sum node, let s be a string initially containing "(".
            if (parens == GroupingParens::Include)
                builder.append('(');

            // Simplification already sorted children.
            auto& children = operationNode.children();
            ASSERT(children.size());
            // Serialize root’s first child, and append it to s.
            buildCSSTextRecursive(children.first(), builder);

            // For each child of root beyond the first:
            // If child is a Negate node, append " - " to s, then serialize the Negate’s child and append the result to s.
            // If child is a negative numeric value, append " - " to s, then serialize the negation of child as normal and append the result to s.
            // Otherwise, append " + " to s, then serialize child and append the result to s.
            for (unsigned i = 1; i < children.size(); ++i) {
                auto& child = children[i];
                if (is<CSSCalcNegateNode>(child)) {
                    builder.append(" - ");
                    buildCSSTextRecursive(downcast<CSSCalcNegateNode>(child.get()).child(), builder);
                    continue;
                }
                
                if (is<CSSCalcPrimitiveValueNode>(child)) {
                    auto& primitiveValueNode = downcast<CSSCalcPrimitiveValueNode>(child.get());
                    if (primitiveValueNode.isNegative()) {
                        builder.append(" - ");
                        // Serialize the negation of child.
                        auto unitType = primitiveValueNode.value().primitiveType();
                        builder.append(0 - primitiveValueNode.value().doubleValue(), CSSPrimitiveValue::unitTypeString(unitType));
                        continue;
                    }
                }
                
                builder.append(" + ");
                buildCSSTextRecursive(child, builder);
            }

            if (parens == GroupingParens::Include)
                builder.append(')');
            return;
        }
        
        if (operationNode.isCalcProductNode()) {
            // If root is a Product node, let s be a string initially containing "(".
            if (parens == GroupingParens::Include)
                builder.append('(');

            // Simplification already sorted children.
            auto& children = operationNode.children();
            ASSERT(children.size());
            // Serialize root’s first child, and append it to s.
            buildCSSTextRecursive(children.first(), builder);

            // For each child of root beyond the first:
            // If child is an Invert node, append " / " to s, then serialize the Invert’s child and append the result to s.
            // Otherwise, append " * " to s, then serialize child and append the result to s.
            for (unsigned i = 1; i < children.size(); ++i) {
                auto& child = children[i];
                if (is<CSSCalcInvertNode>(child)) {
                    builder.append(" / ");
                    buildCSSTextRecursive(downcast<CSSCalcInvertNode>(child.get()).child(), builder);
                    continue;
                }

                builder.append(" * ");
                buildCSSTextRecursive(child, builder);
            }

            if (parens == GroupingParens::Include)
                builder.append(')');
            return;
        }

        // If root is anything but a Sum, Negate, Product, or Invert node, serialize a math function for the
        // function corresponding to the node type, treating the node’s children as the function’s
        // comma-separated calculation arguments, and return the result.
        builder.append(functionPrefixForOperator(operationNode.calcOperator()));

        auto& children = operationNode.children();
        ASSERT(children.size());
        buildCSSTextRecursive(children.first(), builder, GroupingParens::Omit);

        for (unsigned i = 1; i < children.size(); ++i) {
            builder.append(", ");
            buildCSSTextRecursive(children[i], builder, GroupingParens::Omit);
        }
        
        builder.append(')');
        return;
    }
    
    if (is<CSSCalcNegateNode>(node)) {
        auto& negateNode = downcast<CSSCalcNegateNode>(node);
        // If root is a Negate node, let s be a string initially containing "(-1 * ".
        builder.append("-1 *");
        buildCSSTextRecursive(negateNode.child(), builder);
        return;
    }
    
    if (is<CSSCalcInvertNode>(node)) {
        auto& invertNode = downcast<CSSCalcInvertNode>(node);
        // If root is an Invert node, let s be a string initially containing "(1 / ".
        builder.append("1 / ");
        buildCSSTextRecursive(invertNode.child(), builder);
        return;
    }
}

void CSSCalcOperationNode::dump(TextStream& ts) const
{
    ts << "calc operation " << m_operator << " (category: " << category() << ", type " << primitiveType() << ")";

    TextStream::GroupScope scope(ts);
    ts << m_children.size() << " children";
    for (auto& child : m_children)
        ts.dumpProperty("node", child);
}

bool CSSCalcOperationNode::equals(const CSSCalcExpressionNode& exp) const
{
    if (type() != exp.type())
        return false;

    const CSSCalcOperationNode& other = static_cast<const CSSCalcOperationNode&>(exp);

    if (m_children.size() != other.m_children.size() || m_operator != other.m_operator)
        return false;

    for (size_t i = 0; i < m_children.size(); ++i) {
        if (!compareCSSValue(m_children[i], other.m_children[i]))
            return false;
    }
    return true;
}

double CSSCalcOperationNode::evaluateOperator(CalcOperator op, const Vector<double>& children)
{
    switch (op) {
    case CalcOperator::Add: {
        double sum = 0;
        for (auto& child : children)
            sum += child;
        return sum;
    }
    case CalcOperator::Subtract:
        ASSERT(children.size() == 2);
        return children[0] - children[1];
    case CalcOperator::Multiply: {
        double product = 1;
        for (auto& child : children)
            product *= child;
        return product;
    }
    case CalcOperator::Divide:
        ASSERT(children.size() == 1 || children.size() == 2);
        if (children.size() == 1)
            return std::numeric_limits<double>::quiet_NaN();
        return children[0] / children[1];
    case CalcOperator::Min: {
        if (children.isEmpty())
            return std::numeric_limits<double>::quiet_NaN();
        double minimum = children[0];
        for (auto child : children)
            minimum = std::min(minimum, child);
        return minimum;
    }
    case CalcOperator::Max: {
        if (children.isEmpty())
            return std::numeric_limits<double>::quiet_NaN();
        double maximum = children[0];
        for (auto child : children)
            maximum = std::max(maximum, child);
        return maximum;
    }
    case CalcOperator::Clamp: {
        if (children.size() != 3)
            return std::numeric_limits<double>::quiet_NaN();
        double min = children[0];
        double value = children[1];
        double max = children[2];
        return std::max(min, std::min(value, max));
    }
    }
    ASSERT_NOT_REACHED();
    return 0;
}


class CSSCalcExpressionNodeParser {
public:
    explicit CSSCalcExpressionNodeParser(CalculationCategory destinationCategory)
        : m_destinationCategory(destinationCategory)
    { }

    RefPtr<CSSCalcExpressionNode> parseCalc(CSSParserTokenRange, CSSValueID function);
    
private:
    char operatorValue(const CSSParserToken& token)
    {
        if (token.type() == DelimiterToken)
            return token.delimiter();
        return 0;
    }

    bool parseValue(CSSParserTokenRange&, RefPtr<CSSCalcExpressionNode>&);
    bool parseValueTerm(CSSParserTokenRange&, int depth, RefPtr<CSSCalcExpressionNode>&);
    bool parseCalcFunction(CSSParserTokenRange&, CSSValueID, int depth, RefPtr<CSSCalcExpressionNode>&);
    bool parseCalcSum(CSSParserTokenRange&, int depth, RefPtr<CSSCalcExpressionNode>&);
    bool parseCalcProduct(CSSParserTokenRange&, int depth, RefPtr<CSSCalcExpressionNode>&);
    bool parseCalcValue(CSSParserTokenRange&, int depth, RefPtr<CSSCalcExpressionNode>&);

    CalculationCategory m_destinationCategory;
};

// <https://drafts.csswg.org/css-values-4/#calc-syntax>:
// <calc()>  = calc( <calc-sum> )
// <min()>   = min( <calc-sum># )
// <max()>   = max( <calc-sum># )
// <clamp()> = clamp( <calc-sum>#{3} )
// <sin()>   = sin( <calc-sum> )
// <cos()>   = cos( <calc-sum> )
// <tan()>   = tan( <calc-sum> )
// <asin()>  = asin( <calc-sum> )
// <acos()>  = acos( <calc-sum> )
// <atan()>  = atan( <calc-sum> )
// <atan2()> = atan2( <calc-sum>, <calc-sum> )
// <pow()>   = pow( <calc-sum>, <calc-sum> )
// <sqrt()>  = sqrt( <calc-sum> )
// <hypot()> = hypot( <calc-sum># )
// <calc-sum> = <calc-product> [ [ '+' | '-' ] <calc-product> ]*
// <calc-product> = <calc-value> [ [ '*' | '/' ] <calc-value> ]*
// <calc-value> = <number> | <dimension> | <percentage> | ( <calc-sum> )
RefPtr<CSSCalcExpressionNode> CSSCalcExpressionNodeParser::parseCalc(CSSParserTokenRange tokens, CSSValueID function)
{
    tokens.consumeWhitespace();

    RefPtr<CSSCalcExpressionNode> result;
    bool ok = parseCalcFunction(tokens, function, 0, result);
    if (!ok || !tokens.atEnd())
        return nullptr;

    if (!result)
        return nullptr;

    LOG_WITH_STREAM(Calc, stream << "CSSCalcExpressionNodeParser::parseCalc " << prettyPrintNode(*result));

    result = CSSCalcOperationNode::simplify(result.releaseNonNull());

    LOG_WITH_STREAM(Calc, stream << "CSSCalcExpressionNodeParser::parseCalc - after simplification " << prettyPrintNode(*result));

    return result;
}

enum ParseState {
    OK,
    TooDeep,
    NoMoreTokens
};

static ParseState checkDepthAndIndex(int depth, CSSParserTokenRange tokens)
{
    if (tokens.atEnd())
        return NoMoreTokens;
    if (depth > maxExpressionDepth) {
        LOG_WITH_STREAM(Calc, stream << "Depth " << depth << " exceeded maxExpressionDepth " << maxExpressionDepth);
        return TooDeep;
    }
    return OK;
}

bool CSSCalcExpressionNodeParser::parseCalcFunction(CSSParserTokenRange& tokens, CSSValueID functionID, int depth, RefPtr<CSSCalcExpressionNode>& result)
{
    if (checkDepthAndIndex(depth, tokens) != OK)
        return false;

    // "arguments" refers to things between commas.
    unsigned minArgumentCount = 1;
    Optional<unsigned> maxArgumentCount;

    switch (functionID) {
    case CSSValueMin:
    case CSSValueMax:
        maxArgumentCount = WTF::nullopt;
        break;
    case CSSValueClamp:
        minArgumentCount = 3;
        maxArgumentCount = 3;
        break;
    case CSSValueCalc:
        maxArgumentCount = 1;
        break;
    // TODO: clamp, sin, cos, tan, asin, acos, atan, atan2, pow, sqrt, hypot.
    default:
        break;
    }

    Vector<Ref<CSSCalcExpressionNode>> nodes;

    bool requireComma = false;
    unsigned argumentCount = 0;
    while (!tokens.atEnd()) {
        tokens.consumeWhitespace();
        if (requireComma) {
            if (tokens.consume().type() != CommaToken)
                return false;
            tokens.consumeWhitespace();
        }

        RefPtr<CSSCalcExpressionNode> node;
        if (!parseCalcSum(tokens, depth, node))
            return false;

        ++argumentCount;
        if (maxArgumentCount && argumentCount > maxArgumentCount.value())
            return false;

        nodes.append(node.releaseNonNull());
        requireComma = true;
    }
    
    if (argumentCount < minArgumentCount)
        return false;

    switch (functionID) {
    case CSSValueMin:
        result = CSSCalcOperationNode::createMinOrMaxOrClamp(CalcOperator::Min, WTFMove(nodes), m_destinationCategory);
        break;
    case CSSValueMax:
        result = CSSCalcOperationNode::createMinOrMaxOrClamp(CalcOperator::Max, WTFMove(nodes), m_destinationCategory);
        break;
    case CSSValueClamp:
        result = CSSCalcOperationNode::createMinOrMaxOrClamp(CalcOperator::Clamp, WTFMove(nodes), m_destinationCategory);
        break;
    case CSSValueWebkitCalc:
    case CSSValueCalc:
        result = CSSCalcOperationNode::createSum(WTFMove(nodes));
        break;
    // TODO: clamp, sin, cos, tan, asin, acos, atan, atan2, pow, sqrt, hypot
    default:
        break;
    }

    return !!result;
}

bool CSSCalcExpressionNodeParser::parseValue(CSSParserTokenRange& tokens, RefPtr<CSSCalcExpressionNode>& result)
{
    CSSParserToken token = tokens.consumeIncludingWhitespace();
    if (!(token.type() == NumberToken || token.type() == PercentageToken || token.type() == DimensionToken))
        return false;
    
    auto type = token.unitType();
    if (calcUnitCategory(type) == CalculationCategory::Other)
        return false;
    
    result = CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(token.numericValue(), type));
    
    return true;
}

bool CSSCalcExpressionNodeParser::parseCalcValue(CSSParserTokenRange& tokens, int depth, RefPtr<CSSCalcExpressionNode>& result)
{
    if (checkDepthAndIndex(depth, tokens) != OK)
        return false;

    auto findFunctionId = [&](CSSValueID& functionId) {
        if (tokens.peek().type() == LeftParenthesisToken) {
            functionId = CSSValueCalc;
            return true;
        }

        functionId = tokens.peek().functionId();
        return CSSCalcValue::isCalcFunction(functionId);
    };

    CSSValueID functionId;
    if (findFunctionId(functionId)) {
        CSSParserTokenRange innerRange = tokens.consumeBlock();
        tokens.consumeWhitespace();
        innerRange.consumeWhitespace();
        return parseCalcFunction(innerRange, functionId, depth + 1, result);
    }

    return parseValue(tokens, result);
}

bool CSSCalcExpressionNodeParser::parseCalcProduct(CSSParserTokenRange& tokens, int depth, RefPtr<CSSCalcExpressionNode>& result)
{
    if (checkDepthAndIndex(depth, tokens) != OK)
        return false;

    RefPtr<CSSCalcExpressionNode> firstValue;
    if (!parseCalcValue(tokens, depth, firstValue))
        return false;

    Vector<Ref<CSSCalcExpressionNode>> nodes;

    while (!tokens.atEnd()) {
        char operatorCharacter = operatorValue(tokens.peek());
        if (operatorCharacter != static_cast<char>(CalcOperator::Multiply) && operatorCharacter != static_cast<char>(CalcOperator::Divide))
            break;
        tokens.consumeIncludingWhitespace();
        
        RefPtr<CSSCalcExpressionNode> nextValue;
        if (!parseCalcValue(tokens, depth, nextValue) || !nextValue)
            return false;

        if (operatorCharacter == static_cast<char>(CalcOperator::Divide))
            nextValue = CSSCalcInvertNode::create(nextValue.releaseNonNull());

        if (firstValue)
            nodes.append(firstValue.releaseNonNull());

        nodes.append(nextValue.releaseNonNull());
    }

    if (nodes.isEmpty()) {
        result = WTFMove(firstValue);
        return !!result;
    }

    result = CSSCalcOperationNode::createProduct(WTFMove(nodes));
    return !!result;
}

bool CSSCalcExpressionNodeParser::parseCalcSum(CSSParserTokenRange& tokens, int depth, RefPtr<CSSCalcExpressionNode>& result)
{
    if (checkDepthAndIndex(depth, tokens) != OK)
        return false;

    RefPtr<CSSCalcExpressionNode> firstValue;
    if (!parseCalcProduct(tokens, depth, firstValue))
        return false;

    Vector<Ref<CSSCalcExpressionNode>> nodes;

    while (!tokens.atEnd()) {
        char operatorCharacter = operatorValue(tokens.peek());
        if (operatorCharacter != static_cast<char>(CalcOperator::Add) && operatorCharacter != static_cast<char>(CalcOperator::Subtract))
            break;

        if ((&tokens.peek() - 1)->type() != WhitespaceToken)
            return false; // calc(1px+ 2px) is invalid

        tokens.consume();
        if (tokens.peek().type() != WhitespaceToken)
            return false; // calc(1px +2px) is invalid

        tokens.consumeIncludingWhitespace();

        RefPtr<CSSCalcExpressionNode> nextValue;
        if (!parseCalcProduct(tokens, depth, nextValue) || !nextValue)
            return false;

        if (operatorCharacter == static_cast<char>(CalcOperator::Subtract))
            nextValue = CSSCalcNegateNode::create(nextValue.releaseNonNull());

        if (firstValue)
            nodes.append(firstValue.releaseNonNull());

        nodes.append(nextValue.releaseNonNull());
    }

    if (nodes.isEmpty()) {
        result = WTFMove(firstValue);
        return !!result;
    }

    result = CSSCalcOperationNode::createSum(WTFMove(nodes));
    return !!result;
}

static inline RefPtr<CSSCalcOperationNode> createBlendHalf(const Length& length, const RenderStyle& style, float progress)
{
    return CSSCalcOperationNode::create(CalcOperator::Multiply, createCSS(length, style),
        CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(progress, CSSUnitType::CSS_NUMBER)));
}

static Vector<Ref<CSSCalcExpressionNode>> createCSS(const Vector<std::unique_ptr<CalcExpressionNode>>& nodes, const RenderStyle& style)
{
    Vector<Ref<CSSCalcExpressionNode>> values;
    values.reserveInitialCapacity(nodes.size());
    for (auto& node : nodes) {
        auto cssNode = createCSS(*node, style);
        if (!cssNode)
            return { };
        values.uncheckedAppend(cssNode.releaseNonNull());
    }
    return values;
}

static RefPtr<CSSCalcExpressionNode> createCSS(const CalcExpressionNode& node, const RenderStyle& style)
{
    switch (node.type()) {
    case CalcExpressionNodeType::Number: {
        float value = downcast<CalcExpressionNumber>(node).value(); // double?
        return CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(value, CSSUnitType::CSS_NUMBER));
    }
    case CalcExpressionNodeType::Length:
        return createCSS(downcast<CalcExpressionLength>(node).length(), style);

    case CalcExpressionNodeType::Negation: {
        auto childNode = createCSS(*downcast<CalcExpressionNegation>(node).child(), style);
        if (!childNode)
            return nullptr;
        return CSSCalcNegateNode::create(childNode.releaseNonNull());
    }
    case CalcExpressionNodeType::Inversion: {
        auto childNode = createCSS(*downcast<CalcExpressionInversion>(node).child(), style);
        if (!childNode)
            return nullptr;
        return CSSCalcInvertNode::create(childNode.releaseNonNull());
    }
    case CalcExpressionNodeType::Operation: {
        auto& operationNode = downcast<CalcExpressionOperation>(node);
        auto& operationChildren = operationNode.children();
        CalcOperator op = operationNode.getOperator();
        
        switch (op) {
        case CalcOperator::Add: {
            auto children = createCSS(operationChildren, style);
            if (children.isEmpty())
                return nullptr;
            return CSSCalcOperationNode::createSum(WTFMove(children));
        } case CalcOperator::Subtract: {
            ASSERT(operationChildren.size() == 2);

            Vector<Ref<CSSCalcExpressionNode>> values;
            values.reserveInitialCapacity(operationChildren.size());
            
            auto firstChild = createCSS(*operationChildren[0], style);
            if (!firstChild)
                return nullptr;

            auto secondChild = createCSS(*operationChildren[1], style);
            if (!secondChild)
                return nullptr;
            auto negateNode = CSSCalcNegateNode::create(secondChild.releaseNonNull());

            values.append(firstChild.releaseNonNull());
            values.append(WTFMove(negateNode));

            return CSSCalcOperationNode::createSum(WTFMove(values));
        }
        case CalcOperator::Multiply: {
            auto children = createCSS(operationChildren, style);
            if (children.isEmpty())
                return nullptr;
            return CSSCalcOperationNode::createProduct(WTFMove(children));
        }
        case CalcOperator::Divide: {
            ASSERT(operationChildren.size() == 2);

            Vector<Ref<CSSCalcExpressionNode>> values;
            values.reserveInitialCapacity(operationChildren.size());
            
            auto firstChild = createCSS(*operationChildren[0], style);
            if (!firstChild)
                return nullptr;

            auto secondChild = createCSS(*operationChildren[1], style);
            if (!secondChild)
                return nullptr;
            auto invertNode = CSSCalcInvertNode::create(secondChild.releaseNonNull());

            values.append(firstChild.releaseNonNull());
            values.append(WTFMove(invertNode));

            return CSSCalcOperationNode::createProduct(createCSS(operationChildren, style));
        }
        case CalcOperator::Min:
        case CalcOperator::Max:
        case CalcOperator::Clamp: {
            auto children = createCSS(operationChildren, style);
            if (children.isEmpty())
                return nullptr;
            return CSSCalcOperationNode::createMinOrMaxOrClamp(op, WTFMove(children), CalculationCategory::Other);
        }
        }
        return nullptr;
    }
    case CalcExpressionNodeType::BlendLength: {
        // FIXME: (http://webkit.org/b/122036) Create a CSSCalcExpressionNode equivalent of CalcExpressionBlendLength.
        auto& blend = downcast<CalcExpressionBlendLength>(node);
        float progress = blend.progress();
        return CSSCalcOperationNode::create(CalcOperator::Add, createBlendHalf(blend.from(), style, 1 - progress), createBlendHalf(blend.to(), style, progress));
    }
    case CalcExpressionNodeType::Undefined:
        ASSERT_NOT_REACHED();
    }
    return nullptr;
}

static RefPtr<CSSCalcExpressionNode> createCSS(const Length& length, const RenderStyle& style)
{
    switch (length.type()) {
    case Percent:
    case Fixed:
        return CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(length, style));
    case Calculated:
        return createCSS(length.calculationValue().expression(), style);
    case Auto:
    case Intrinsic:
    case MinIntrinsic:
    case MinContent:
    case MaxContent:
    case FillAvailable:
    case FitContent:
    case Relative:
    case Undefined:
        ASSERT_NOT_REACHED();
    }
    return nullptr;
}

String CSSCalcValue::customCSSText() const
{
    StringBuilder builder;
    CSSCalcOperationNode::buildCSSText(m_expression.get(), builder);
    return builder.toString();
}

bool CSSCalcValue::equals(const CSSCalcValue& other) const
{
    return compareCSSValue(m_expression, other.m_expression);
}

inline double CSSCalcValue::clampToPermittedRange(double value) const
{
    return m_shouldClampToNonNegative && value < 0 ? 0 : value;
}

double CSSCalcValue::doubleValue() const
{
    return clampToPermittedRange(m_expression->doubleValue(primitiveType()));
}

double CSSCalcValue::computeLengthPx(const CSSToLengthConversionData& conversionData) const
{
    return clampToPermittedRange(m_expression->computeLengthPx(conversionData));
}

bool CSSCalcValue::isCalcFunction(CSSValueID functionId)
{
    switch (functionId) {
    case CSSValueCalc:
    case CSSValueWebkitCalc:
    case CSSValueMin:
    case CSSValueMax:
    case CSSValueClamp:
        return true;
    default:
        return false;
    }
    return false;
}

void CSSCalcValue::dump(TextStream& ts) const
{
    ts << indent << "(" << "CSSCalcValue";

    TextStream multilineStream;
    multilineStream.setIndent(ts.indent() + 2);

    multilineStream.dumpProperty("should clamp non-negative", m_shouldClampToNonNegative);
    multilineStream.dumpProperty("expression", m_expression.get());

    ts << multilineStream.release();
    ts << ")\n";
}

RefPtr<CSSCalcValue> CSSCalcValue::create(CSSValueID function, const CSSParserTokenRange& tokens, CalculationCategory destinationCategory, ValueRange range)
{
    CSSCalcExpressionNodeParser parser(destinationCategory);
    auto expression = parser.parseCalc(tokens, function);
    if (!expression)
        return nullptr;
    auto result = adoptRef(new CSSCalcValue(expression.releaseNonNull(), range != ValueRangeAll));
    LOG_WITH_STREAM(Calc, stream << "CSSCalcValue::create " << *result);
    return result;
}
    
RefPtr<CSSCalcValue> CSSCalcValue::create(const CalculationValue& value, const RenderStyle& style)
{
    auto expression = createCSS(value.expression(), style);
    if (!expression)
        return nullptr;
    auto result = adoptRef(new CSSCalcValue(expression.releaseNonNull(), value.shouldClampToNonNegative()));
    LOG_WITH_STREAM(Calc, stream << "CSSCalcValue::create from CalculationValue: " << *result);
    return result;
}

TextStream& operator<<(TextStream& ts, CalculationCategory category)
{
    switch (category) {
    case CalculationCategory::Number: ts << "number"; break;
    case CalculationCategory::Length: ts << "length"; break;
    case CalculationCategory::Percent: ts << "percent"; break;
    case CalculationCategory::PercentNumber: ts << "percent-number"; break;
    case CalculationCategory::PercentLength: ts << "percent-length"; break;
    case CalculationCategory::Angle: ts << "angle"; break;
    case CalculationCategory::Time: ts << "time"; break;
    case CalculationCategory::Frequency: ts << "frequency"; break;
    case CalculationCategory::Other: ts << "other"; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, const CSSCalcValue& value)
{
    value.dump(ts);
    return ts;
}

} // namespace WebCore
