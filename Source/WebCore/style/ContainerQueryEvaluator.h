/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ContainerQuery.h"
#include "SelectorMatchingState.h"
#include <wtf/Ref.h>

namespace WebCore {

class Element;

namespace Style {

enum class EvaluationResult : uint8_t { False, True, Unknown };

class ContainerQueryEvaluator {
public:
    ContainerQueryEvaluator(const Element&, PseudoId, SelectorMatchingState*);

    bool evaluate(const FilteredContainerQuery&) const;

private:
    struct ResolvedContainer;
    std::optional<ResolvedContainer> resolveContainer(const FilteredContainerQuery&) const;

    EvaluationResult evaluateQuery(const CQ::ContainerQuery&, const ResolvedContainer&) const;
    EvaluationResult evaluateQuery(const CQ::SizeQuery&, const ResolvedContainer&) const;
    template<typename ConditionType> EvaluationResult evaluateCondition(const ConditionType&, const ResolvedContainer&) const;
    EvaluationResult evaluateSizeFeature(const CQ::SizeFeature&, const ResolvedContainer&) const;

    const Ref<const Element> m_element;
    const PseudoId m_pseudoId;
    SelectorMatchingState* m_selectorMatchingState;
};

inline EvaluationResult toEvaluationResult(bool boolean)
{
    return boolean ? EvaluationResult::True : EvaluationResult::False;
};

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

}
}
