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
#include "MediaQueryEvaluator.h"

#include "MediaQuery.h"

namespace WebCore {
namespace MQ {

MediaQueryEvaluator::MediaQueryEvaluator(const AtomString& mediaType, const Document& document, const RenderStyle* rootElementStyle)
    : GenericMediaQueryEvaluator()
    , m_mediaType(mediaType)
    , m_featureContext({ document, rootElementStyle })
{
}

bool MediaQueryEvaluator::evaluate(const MediaQueryList& list) const
{
    for (auto& query : list) {
        if (evaluate(query))
            return true;
    }
    return false;
}

bool MediaQueryEvaluator::evaluate(const MediaQuery& query) const
{
    bool isNegated = query.prefix && *query.prefix == Prefix::Not;

    auto mediaTypeMatches = [&] {
        if (query.mediaType.isEmpty())
            return true;
        if (query.mediaType == "all"_s)
            return true;
        return query.mediaType == m_mediaType;
    }();

    if (!mediaTypeMatches)
        return isNegated;

    auto conditionMatches = [&] {
        if (!query.condition)
            return false;
        return evaluateCondition(*query.condition, m_featureContext) == EvaluationResult::True;
    }();

    return conditionMatches != isNegated;
}

EvaluationResult MediaQueryEvaluator::evaluateFeature(const Feature&, const FeatureContext&) const
{
    return EvaluationResult::Unknown;
}


}
}
