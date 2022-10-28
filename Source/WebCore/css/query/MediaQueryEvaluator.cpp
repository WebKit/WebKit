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

#include "CSSToLengthConversionData.h"
#include "Document.h"
#include "MediaQuery.h"
#include "RenderView.h"
#include "StyleFontSizeFunctions.h"

namespace WebCore {
namespace MQ {

MediaQueryEvaluator::MediaQueryEvaluator(const AtomString& mediaType, const Document& document, const RenderStyle* rootElementStyle)
    : GenericMediaQueryEvaluator()
    , m_mediaType(mediaType)
    , m_document(document)
    , m_rootElementStyle(rootElementStyle)
{
}

bool MediaQueryEvaluator::evaluate(const MediaQueryList& list) const
{
    if (list.isEmpty())
        return true;

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
        if (equalLettersIgnoringASCIICase(query.mediaType, "all"_s))
            return true;
        return equalIgnoringASCIICase(query.mediaType, m_mediaType);
    }();

    if (!mediaTypeMatches)
        return isNegated;

    auto conditionMatches = [&] {
        if (!query.condition)
            return true;

        if (!m_document.view())
            return false;

        if (!m_document.documentElement())
            return false;

        auto defaultStyle = RenderStyle::create();
        auto fontDescription = defaultStyle.fontDescription();
        auto size = Style::fontSizeForKeyword(CSSValueMedium, false, m_document);
        fontDescription.setComputedSize(size);
        fontDescription.setSpecifiedSize(size);
        defaultStyle.setFontDescription(WTFMove(fontDescription));
        defaultStyle.fontCascade().update();

        FeatureEvaluationContext context { m_document, { *m_rootElementStyle, &defaultStyle, nullptr, m_document.renderView() }, nullptr };
        return evaluateCondition(*query.condition, context) == EvaluationResult::True;
    }();

    return conditionMatches != isNegated;
}

}
}
