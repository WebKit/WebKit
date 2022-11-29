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

#pragma once

#include "CSSToLengthConversionData.h"
#include "CSSValueKeywords.h"
#include "GenericMediaQueryTypes.h"
#include "LayoutUnit.h"

namespace WebCore {

class RenderElement;

namespace MQ {

EvaluationResult evaluateLengthFeature(const Feature&, LayoutUnit, const CSSToLengthConversionData&);
EvaluationResult evaluateRatioFeature(const Feature&, FloatSize);
EvaluationResult evaluateBooleanFeature(const Feature&, bool);
EvaluationResult evaluateIntegerFeature(const Feature&, int);
EvaluationResult evaluateNumberFeature(const Feature&, double);
EvaluationResult evaluateResolutionFeature(const Feature&, double);
EvaluationResult evaluateIdentifierFeature(const Feature&, CSSValueID);

template<typename ConcreteEvaluator>
class GenericMediaQueryEvaluator {
public:
    EvaluationResult evaluateQueryInParens(const QueryInParens&, const FeatureEvaluationContext&) const;
    EvaluationResult evaluateCondition(const Condition&, const FeatureEvaluationContext&) const;
    EvaluationResult evaluateFeature(const Feature&, const FeatureEvaluationContext&) const;

private:
    const ConcreteEvaluator& concreteEvaluator() const { return static_cast<const ConcreteEvaluator&>(*this); }
};

template<typename ConcreteEvaluator>
EvaluationResult GenericMediaQueryEvaluator<ConcreteEvaluator>::evaluateQueryInParens(const QueryInParens& queryInParens, const FeatureEvaluationContext& context) const
{
    return WTF::switchOn(queryInParens, [&](const Condition& condition) {
        return evaluateCondition(condition, context);
    }, [&](const MQ::Feature& feature) {
        return concreteEvaluator().evaluateFeature(feature, context);
    }, [&](const MQ::GeneralEnclosed&) {
        return MQ::EvaluationResult::Unknown;
    });
}

template<typename ConcreteEvaluator>
EvaluationResult GenericMediaQueryEvaluator<ConcreteEvaluator>::evaluateCondition(const Condition& condition, const FeatureEvaluationContext& context) const
{
    if (condition.queries.isEmpty())
        return EvaluationResult::Unknown;

    switch (condition.logicalOperator) {
    case LogicalOperator::Not:
        return !concreteEvaluator().evaluateQueryInParens(condition.queries.first(), context);

    // Kleene 3-valued logic.
    case LogicalOperator::And: {
        auto result = EvaluationResult::True;
        for (auto& query : condition.queries) {
            auto queryResult = concreteEvaluator().evaluateQueryInParens(query, context);
            if (queryResult == EvaluationResult::False)
                return EvaluationResult::False;
            if (queryResult == EvaluationResult::Unknown)
                result = EvaluationResult::Unknown;
        }
        return result;
    }

    case LogicalOperator::Or: {
        auto result = EvaluationResult::False;
        for (auto& query : condition.queries) {
            auto queryResult = concreteEvaluator().evaluateQueryInParens(query, context);
            if (queryResult == EvaluationResult::True)
                return EvaluationResult::True;
            if (queryResult == EvaluationResult::Unknown)
                result = EvaluationResult::Unknown;
        }
        return result;
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename ConcreteEvaluator>
EvaluationResult GenericMediaQueryEvaluator<ConcreteEvaluator>::evaluateFeature(const Feature& feature, const FeatureEvaluationContext& context) const
{
    if (!feature.schema)
        return MQ::EvaluationResult::Unknown;

    return feature.schema->evaluate(feature, context);
}

inline EvaluationResult operator&(EvaluationResult left, EvaluationResult right)
{
    if (left == EvaluationResult::Unknown || right == EvaluationResult::Unknown)
        return EvaluationResult::Unknown;
    if (left == EvaluationResult::True && right == EvaluationResult::True)
        return EvaluationResult::True;
    return EvaluationResult::False;
}

inline EvaluationResult operator!(EvaluationResult result)
{
    switch (result) {
    case EvaluationResult::True:
        return EvaluationResult::False;
    case EvaluationResult::False:
        return EvaluationResult::True;
    case EvaluationResult::Unknown:
        return EvaluationResult::Unknown;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline EvaluationResult toEvaluationResult(bool boolean)
{
    return boolean ? EvaluationResult::True : EvaluationResult::False;
}

}
}
