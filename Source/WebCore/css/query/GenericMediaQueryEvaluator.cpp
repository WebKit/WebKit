/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GenericMediaQueryEvaluator.h"

#include "CSSPrimitiveValue.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueList.h"

namespace WebCore {
namespace MQ {

static std::optional<LayoutUnit> computeLength(const CSSValue* value, const CSSToLengthConversionData& conversionData)
{
    if (!is<CSSPrimitiveValue>(value))
        return { };
    auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);

    if (primitiveValue.isNumberOrInteger()) {
        if (primitiveValue.doubleValue())
            return { };
        return 0_lu;
    }

    if (!primitiveValue.isLength())
        return { };
    return primitiveValue.computeLength<LayoutUnit>(conversionData);
}

template<typename T>
bool compare(ComparisonOperator op, T left, T right)
{
    switch (op) {
    case ComparisonOperator::LessThan:
        return left < right;
    case ComparisonOperator::GreaterThan:
        return left > right;
    case ComparisonOperator::LessThanOrEqual:
        return left <= right;
    case ComparisonOperator::GreaterThanOrEqual:
        return left >= right;
    case ComparisonOperator::Equal:
        return left == right;
    }
    RELEASE_ASSERT_NOT_REACHED();
};

enum class Side : uint8_t { Left, Right };
static EvaluationResult evaluateLengthComparison(LayoutUnit size, const std::optional<Comparison>& comparison, Side side, const CSSToLengthConversionData& conversionData)
{
    if (!comparison)
        return EvaluationResult::True;

    auto expressionSize = computeLength(comparison->value.get(), conversionData);
    if (!expressionSize)
        return EvaluationResult::Unknown;

    auto left = side == Side::Left ? *expressionSize : size;
    auto right = side == Side::Left ? size : *expressionSize;

    return toEvaluationResult(compare(comparison->op, left, right));
};

EvaluationResult GenericMediaQueryEvaluatorBase::evaluateLengthFeature(const Feature& feature, LayoutUnit length, const CSSToLengthConversionData& conversionData) const
{
    if (!feature.leftComparison && !feature.rightComparison)
        return toEvaluationResult(!!length);

    auto leftResult = evaluateLengthComparison(length, feature.leftComparison, Side::Left, conversionData);
    auto rightResult = evaluateLengthComparison(length, feature.rightComparison, Side::Right, conversionData);

    return leftResult & rightResult;
};

static EvaluationResult evaluateRatioComparison(double ratio, const std::optional<Comparison>& comparison, Side side)
{
    if (!comparison)
        return EvaluationResult::True;

    if (!is<CSSValueList>(comparison->value))
        return EvaluationResult::Unknown;

    auto& ratioList = downcast<CSSValueList>(*comparison->value);
    if (ratioList.length() != 2)
        return EvaluationResult::Unknown;

    auto first = dynamicDowncast<CSSPrimitiveValue>(ratioList.item(0));
    auto second = dynamicDowncast<CSSPrimitiveValue>(ratioList.item(1));

    if (!first || !second || !first->isNumberOrInteger() || !second->isNumberOrInteger())
        return EvaluationResult::Unknown;

    auto expressionRatio = first->doubleValue() / second->doubleValue();

    auto left = side == Side::Left ? expressionRatio : ratio;
    auto right = side == Side::Left ? ratio : expressionRatio;

    return toEvaluationResult(compare(comparison->op, left, right));
};

EvaluationResult GenericMediaQueryEvaluatorBase::evaluateRatioFeature(const Feature& feature, double ratio) const
{
    if (!feature.leftComparison && !feature.rightComparison)
        return toEvaluationResult(!!ratio);

    auto leftResult = evaluateRatioComparison(ratio, feature.leftComparison, Side::Left);
    auto rightResult = evaluateRatioComparison(ratio, feature.rightComparison, Side::Right);

    return leftResult & rightResult;
}

EvaluationResult GenericMediaQueryEvaluatorBase::evaluateDiscreteFeature(const Feature& feature, CSSValueID expectedValue) const
{
    if (!feature.rightComparison)
        return EvaluationResult::Unknown;

    auto& comparison = *feature.rightComparison;

    if (!is<CSSPrimitiveValue>(comparison.value) || comparison.op != ComparisonOperator::Equal)
        return EvaluationResult::Unknown;

    auto& value = downcast<CSSPrimitiveValue>(*feature.rightComparison->value);
    return toEvaluationResult(value.valueID() == expectedValue);
}

}
}
