/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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
#include "CSSCalcValue.h"

#include "CSSCalcExpressionNodeParser.h"
#include "CSSCalcInvertNode.h"
#include "CSSCalcNegateNode.h"
#include "CSSCalcOperationNode.h"
#include "CSSCalcPrimitiveValueNode.h"
#include "CSSCalcSymbolTable.h"
#include "CSSParser.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValueMappings.h"
#include "CalcExpressionBlendLength.h"
#include "CalcExpressionInversion.h"
#include "CalcExpressionLength.h"
#include "CalcExpressionNegation.h"
#include "CalcExpressionNumber.h"
#include "CalcExpressionOperation.h"
#include "Logging.h"
#include "StyleResolver.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static RefPtr<CSSCalcExpressionNode> createCSS(const CalcExpressionNode&, const RenderStyle&);
static RefPtr<CSSCalcExpressionNode> createCSS(const Length&, const RenderStyle&);

static inline RefPtr<CSSCalcOperationNode> createBlendHalf(const Length& length, const RenderStyle& style, float progress)
{
    return CSSCalcOperationNode::create(CalcOperator::Multiply, createCSS(length, style),
        CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(progress, CSSUnitType::CSS_NUMBER)));
}

static Vector<Ref<CSSCalcExpressionNode>> createCSS(const Vector<std::unique_ptr<CalcExpressionNode>>& nodes, const RenderStyle& style)
{
    return WTF::compactMap(nodes, [&](auto& node) -> RefPtr<CSSCalcExpressionNode> {
        return createCSS(*node, style);
    });
}

static RefPtr<CSSCalcExpressionNode> createCSS(const CalcExpressionNode& node, const RenderStyle& style)
{
    switch (node.type()) {
    case CalcExpressionNodeType::Number: {
        float value = downcast<CalcExpressionNumber>(node).value(); // double?
        return CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(value, CSSUnitType::CSS_NUMBER));
    }
    case CalcExpressionNodeType::Length: {
        auto& length = downcast<CalcExpressionLength>(node).length();
        if (!length.isPercent() && length.isZero())
            return nullptr;
        return createCSS(length, style);
    }

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
            if (children.size() == 1)
                return WTFMove(children[0]);
            return CSSCalcOperationNode::createSum(WTFMove(children));
        }
        case CalcOperator::Subtract: {
            ASSERT(operationChildren.size() == 2);

            Vector<Ref<CSSCalcExpressionNode>> values;
            values.reserveInitialCapacity(operationChildren.size());
            
            auto firstChild = createCSS(*operationChildren[0], style);
            auto secondChild = createCSS(*operationChildren[1], style);

            if (!secondChild)
                return firstChild;

            auto negateNode = CSSCalcNegateNode::create(secondChild.releaseNonNull());
            if (!firstChild)
                return negateNode;

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
        case CalcOperator::Cos:
        case CalcOperator::Tan:
        case CalcOperator::Sin: {
            auto children = createCSS(operationChildren, style);
            if (children.size() != 1)
                return nullptr;
            return CSSCalcOperationNode::createTrig(op, WTFMove(children));
        }
        case CalcOperator::Min:
        case CalcOperator::Max:
        case CalcOperator::Clamp: {
            auto children = createCSS(operationChildren, style);
            if (children.isEmpty())
                return nullptr;
            return CSSCalcOperationNode::createMinOrMaxOrClamp(op, WTFMove(children), operationNode.destinationCategory());
        }
        case CalcOperator::Log: {
            auto children = createCSS(operationChildren, style);
            if (children.size() != 1 && children.size() != 2)
                return nullptr;
            return CSSCalcOperationNode::createLog(WTFMove(children));
        }
        case CalcOperator::Exp: {
            auto children = createCSS(operationChildren, style);
            if (children.size() != 1)
                return nullptr;
            return CSSCalcOperationNode::createExp(WTFMove(children));
        }
        case CalcOperator::Asin:
        case CalcOperator::Acos:
        case CalcOperator::Atan: {
            auto children = createCSS(operationChildren, style);
            if (children.size() != 1)
                return nullptr;
            return CSSCalcOperationNode::createInverseTrig(op, WTFMove(children));
        }
        case CalcOperator::Atan2: {
            auto children = createCSS(operationChildren, style);
            if (children.size() != 2)
                return nullptr;
            return CSSCalcOperationNode::createAtan2(WTFMove(children));
        }
        case CalcOperator::Sign:
        case CalcOperator::Abs: {
            auto children = createCSS(operationChildren, style);
            if (children.size() != 1)
                return nullptr;
            return CSSCalcOperationNode::createSign(op, WTFMove(children));
        }
        case CalcOperator::Sqrt:
        case CalcOperator::Pow: {
            auto children = createCSS(operationChildren, style);
            if (children.isEmpty())
                return nullptr;
            return CSSCalcOperationNode::createPowOrSqrt(op, WTFMove(children));
        }
        case CalcOperator::Hypot: {
            auto children = createCSS(operationChildren, style);
            if (children.isEmpty())
                return nullptr;
            return CSSCalcOperationNode::createHypot(WTFMove(children));
        }
        case CalcOperator::Mod:
        case CalcOperator::Rem:
        case CalcOperator::Round: {
            auto children = createCSS(operationChildren, style);
            if (children.size() != 2)
                return nullptr;
            return CSSCalcOperationNode::createStep(op, WTFMove(children));
        }
        case CalcOperator::Nearest:
        case CalcOperator::ToZero:
        case CalcOperator::Up:
        case CalcOperator::Down: {
            return CSSCalcOperationNode::createRoundConstant(op);
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
    case LengthType::Percent:
    case LengthType::Fixed:
        return CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(length, style));
    case LengthType::Calculated:
        return createCSS(length.calculationValue().expression(), style);
    case LengthType::Auto:
    case LengthType::Content:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
    case LengthType::Relative:
    case LengthType::Undefined:
        ASSERT_NOT_REACHED();
    }
    return nullptr;
}

CSSCalcValue::CSSCalcValue(Ref<CSSCalcExpressionNode>&& expression, bool shouldClampToNonNegative)
    : CSSValue(CalculationClass)
    , m_expression(WTFMove(expression))
    , m_shouldClampToNonNegative(shouldClampToNonNegative)
{
}

CSSCalcValue::~CSSCalcValue() = default;

CalculationCategory CSSCalcValue::category() const
{
    return m_expression->category();
}

CSSUnitType CSSCalcValue::primitiveType() const
{
    return m_expression->primitiveType();
}

Ref<CalculationValue> CSSCalcValue::createCalculationValue(const CSSToLengthConversionData& conversionData) const
{
    return CalculationValue::create(m_expression->createCalcExpression(conversionData), m_shouldClampToNonNegative ? ValueRange::NonNegative : ValueRange::All);
}

void CSSCalcValue::setPermittedValueRange(ValueRange range)
{
    m_shouldClampToNonNegative = range != ValueRange::All;
}

void CSSCalcValue::collectDirectComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    m_expression->collectDirectComputationalDependencies(values);
}

void CSSCalcValue::collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    m_expression->collectDirectRootComputationalDependencies(values);
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
    if (primitiveType() == CSSUnitType::CSS_DEG && (isnan(value) || isinf(value)))
        return 0;
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

bool CSSCalcValue::convertingToLengthRequiresNonNullStyle(int lengthConversion) const
{
    return m_expression->convertingToLengthRequiresNonNullStyle(lengthConversion);
}

bool CSSCalcValue::isCalcFunction(CSSValueID functionId)
{
    switch (functionId) {
    case CSSValueCalc:
    case CSSValueWebkitCalc:
    case CSSValueMin:
    case CSSValueMax:
    case CSSValueClamp:
    case CSSValuePow:
    case CSSValueSqrt:
    case CSSValueHypot:
    case CSSValueSin:
    case CSSValueCos:
    case CSSValueTan:
    case CSSValueExp:
    case CSSValueLog:
    case CSSValueAsin:
    case CSSValueAcos:
    case CSSValueAtan:
    case CSSValueAtan2:
    case CSSValueAbs:
    case CSSValueSign:
    case CSSValueRound:
    case CSSValueMod:
    case CSSValueRem:
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

RefPtr<CSSCalcValue> CSSCalcValue::create(CSSValueID function, const CSSParserTokenRange& tokens, CalculationCategory destinationCategory, ValueRange range, const CSSCalcSymbolTable& symbolTable, bool allowsNegativePercentage)
{
    CSSCalcExpressionNodeParser parser(destinationCategory, symbolTable);
    auto expression = parser.parseCalc(tokens, function, allowsNegativePercentage);
    if (!expression)
        return nullptr;
    auto result = adoptRef(new CSSCalcValue(expression.releaseNonNull(), range != ValueRange::All));
    LOG_WITH_STREAM(Calc, stream << "CSSCalcValue::create " << *result);
    return result;
}

RefPtr<CSSCalcValue> CSSCalcValue::create(CSSValueID function, const CSSParserTokenRange& tokens, CalculationCategory destinationCategory, ValueRange range)
{
    return create(function, tokens, destinationCategory, range, { });
}

RefPtr<CSSCalcValue> CSSCalcValue::create(const CalculationValue& value, const RenderStyle& style)
{
    auto expression = createCSS(value.expression(), style);
    if (!expression)
        return nullptr;

    auto simplifiedExpression = CSSCalcOperationNode::simplify(expression.releaseNonNull());

    auto result = adoptRef(new CSSCalcValue(WTFMove(simplifiedExpression), value.shouldClampToNonNegative()));
    LOG_WITH_STREAM(Calc, stream << "CSSCalcValue::create from CalculationValue: " << *result);
    return result;
}

RefPtr<CSSCalcValue> CSSCalcValue::create(Ref<CSSCalcExpressionNode>&& node, bool allowsNegativePercentage)
{
    return adoptRef(*new CSSCalcValue(WTFMove(node), allowsNegativePercentage));
}

TextStream& operator<<(TextStream& ts, const CSSCalcValue& value)
{
    value.dump(ts);
    return ts;
}

} // namespace WebCore
