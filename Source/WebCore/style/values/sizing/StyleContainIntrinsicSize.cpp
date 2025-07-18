/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleContainIntrinsicSize.h"

#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<ContainIntrinsicSize>::operator()(BuilderState& state, const CSSValue& value) -> ContainIntrinsicSize
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueNone)
            return CSS::Keyword::None { };

        if (primitiveValue->isLength()) {
            return ContainIntrinsicSize::Length {
                primitiveValue->resolveAsLength<float>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f))
            };
        }

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::None { };
    }

    auto pair = requiredPairDowncast<CSSPrimitiveValue>(state, value);
    if (!pair)
        return CSS::Keyword::None { };

    if (pair->first->valueID() != CSSValueAuto) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::None { };
    }

    if (pair->second->valueID() == CSSValueNone)
        return { CSS::Keyword::Auto { }, CSS::Keyword::None { } };

    if (Ref second = pair->second; second->isLength()) {
        return {
            CSS::Keyword::Auto { },
            ContainIntrinsicSize::Length {
                second->resolveAsLength<float>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f))
            }
        };
    }

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::None { };
}

// MARK: - Blending

auto Blending<ContainIntrinsicSize>::canBlend(const ContainIntrinsicSize& a, const ContainIntrinsicSize& b) -> bool
{
    return a.type == b.type && a.hasLength();
}

auto Blending<ContainIntrinsicSize>::blend(const ContainIntrinsicSize& a, const ContainIntrinsicSize& b, const BlendingContext& context) -> ContainIntrinsicSize
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    ASSERT(a.type == b.type);
    ASSERT(a.hasLength());

    return ContainIntrinsicSize { a.type, Style::blend(a.length, b.length, context) };
}

} // namespace Style
} // namespace WebCore
