/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSParserTokenRange.h"
#include "GenericMediaQueryEvaluator.h"

namespace WebCore {

struct CSSParserContext;

namespace Style {

class Builder;

// Evaluates the condition of an if() branch (CSS Values 5 §8.6).
// https://drafts.csswg.org/css-values-5/#funcdef-if
class IfConditionEvaluator : public MQ::GenericMediaQueryEvaluator<IfConditionEvaluator> {
public:
    IfConditionEvaluator(Builder& styleBuilder, const CSSParserContext& context)
        : m_styleBuilder(styleBuilder)
        , m_context(context)
    {
    }

    enum class Result : uint8_t { True, False, Invalid };
    Result evaluate(CSSParserTokenRange branchCondition);

    // True if evaluating the condition read an attr()-tainted custom property. Per CSS Values 5 §8.7.2
    // that taints the whole if() substitution value, since the tainted value was involved in producing it.
    bool referencedAttrTaintedValue() const { return m_referencedAttrTaintedValue; }

private:
    friend class MQ::GenericMediaQueryEvaluator<IfConditionEvaluator>;

    MQ::EvaluationResult evaluateQueryInParens(const MQ::QueryInParens&, const MQ::FeatureEvaluationContext&) const;
    MQ::EvaluationResult evaluateFeature(const MQ::Feature&, const MQ::FeatureEvaluationContext&) const;

    Builder& m_styleBuilder;
    const CSSParserContext& m_context;
    mutable bool m_referencedAttrTaintedValue { false };
};

} // namespace Style
} // namespace WebCore
