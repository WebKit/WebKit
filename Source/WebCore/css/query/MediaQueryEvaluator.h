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

#include "GenericMediaQueryEvaluator.h"
#include "MediaQuery.h"

namespace WebCore {

class RenderStyle;

namespace MQ {

class MediaQueryEvaluator : public GenericMediaQueryEvaluator<MediaQueryEvaluator> {
public:
    MediaQueryEvaluator(const AtomString& mediaType, const Document&, const RenderStyle* rootElementStyle);
    MediaQueryEvaluator(const AtomString& mediaType = nullAtom(), EvaluationResult mediaConditionResult = EvaluationResult::False);

    bool evaluate(const MediaQueryList&) const;
    bool evaluate(const MediaQuery&) const;

    bool evaluateMediaType(const MediaQuery&) const;

    OptionSet<MediaQueryDynamicDependency> collectDynamicDependencies(const MediaQueryList&) const;
    OptionSet<MediaQueryDynamicDependency> collectDynamicDependencies(const MediaQuery&) const;

    bool isPrintMedia() const;

private:
    AtomString m_mediaType;
    const Document* m_document { nullptr };
    const RenderStyle* m_rootElementStyle { nullptr };
    EvaluationResult m_staticMediaConditionResult { EvaluationResult::Unknown };
};

}
}
