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
#include "StyleAspectRatio.h"

#include "AnimationUtilities.h"
#include "CSSPrimitiveValue.h"
#include "CSSRatioValue.h"
#include "CSSValueList.h"
#include "RenderStyleInlines.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<AspectRatio>::operator()(BuilderState& state, const CSSValue& value) -> AspectRatio
{
    if (isValueID(value, CSSValueAuto))
        return CSS::Keyword::Auto { };

    if (RefPtr ratioValue = dynamicDowncast<CSSRatioValue>(value))
        return toStyle(ratioValue->ratio(), state);

    auto list = requiredListDowncast<CSSValueList, CSSValue, 2>(state, value);
    if (!list)
        return CSS::Keyword::Auto { };

    Ref value0 = list->item(0);
    if (!isValueID(value0.get(), CSSValueAuto))
        return CSS::Keyword::Auto { };

    Ref value1 = list->item(1);
    RefPtr ratioValue = requiredDowncast<CSSRatioValue>(state, value1);
    if (!ratioValue)
        return CSS::Keyword::Auto { };

    return { CSS::Keyword::Auto { }, toStyle(ratioValue->ratio(), state) };
}

// MARK: - Blending

auto Blending<AspectRatio>::canBlend(const AspectRatio& a, const AspectRatio& b) -> bool
{
    return (a.isRatio() && b.isRatio())
        || (a.isAutoAndRatio() && b.isAutoAndRatio());
}

auto Blending<AspectRatio>::blend(const AspectRatio& a, const AspectRatio& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> AspectRatio
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    ASSERT(a.m_type == b.m_type);
    auto blendedRatio = WebCore::blend(std::log(aStyle.logicalAspectRatio()), std::log(bStyle.logicalAspectRatio()), context);
    return AspectRatio { a.m_type, Ratio { .numerator = { std::exp(blendedRatio) }, .denominator = { 1 } } };
}

} // namespace Style
} // namespace WebCore
