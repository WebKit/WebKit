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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleSingleAnimationTrigger.h"

#include "CSSKeywordValue.h"
#include "StyleBuilderChecking.h"
#include "StyleValueTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<NamedAnimationTrigger>::operator()(BuilderState& state, const CSSValue& value) -> NamedAnimationTrigger
{
    auto list = requiredListDowncast<CSSValueList, CSSValue, 2>(state, value);
    if (!list)
        return { };

    return {
        toStyleFromCSSValue<CustomIdent>(state, list->item(0)),
        toStyleFromCSSValue<SpaceSeparatedFixedVector<AnimationAction>>(state, list->item(1))
    };
}

auto CSSValueConversion<AnimationAction>::operator()(BuilderState& state, const CSSValue& value) -> AnimationAction
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueNone:
            return CSS::Keyword::None { };
        case CSSValuePlay:
            return CSS::Keyword::Play { };
        case CSSValuePlayOnce:
            return CSS::Keyword::PlayOnce { };
        case CSSValuePlayForwards:
            return CSS::Keyword::PlayForwards { };
        case CSSValuePlayBackwards:
            return CSS::Keyword::PlayBackwards { };
        case CSSValuePause:
            return CSS::Keyword::Pause { };
        case CSSValueReset:
            return CSS::Keyword::Reset { };
        case CSSValueReplay:
            return CSS::Keyword::Replay { };
        default:
            break;
        }
    }

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::None { };
}

} // namespace Style
} // namespace WebCore
