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
#include "StyleClip.h"

#include "AnimationUtilities.h"
#include "CSSPrimitiveValue.h"
#include "CSSRectValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<Clip>::operator()(BuilderState& state, const CSSValue& value) -> Clip
{
    if (isValueID(value, CSSValueAuto))
        return CSS::Keyword::Auto { };

    RefPtr rectValue = requiredDowncast<CSSRectValue>(state, value);
    if (!rectValue)
        return CSS::Keyword::Auto { };

    RefPtr primitiveValueTop = requiredDowncast<CSSPrimitiveValue>(state, rectValue->rect().top());
    if (!primitiveValueTop)
        return CSS::Keyword::Auto { };
    RefPtr primitiveValueRight = requiredDowncast<CSSPrimitiveValue>(state, rectValue->rect().right());
    if (!primitiveValueRight)
        return CSS::Keyword::Auto { };
    RefPtr primitiveValueBottom = requiredDowncast<CSSPrimitiveValue>(state, rectValue->rect().bottom());
    if (!primitiveValueBottom)
        return CSS::Keyword::Auto { };
    RefPtr primitiveValueLeft = requiredDowncast<CSSPrimitiveValue>(state, rectValue->rect().left());
    if (!primitiveValueLeft)
        return CSS::Keyword::Auto { };

    auto convertEdge = [&](Ref<const CSSPrimitiveValue>&& primitiveValue) -> ClipEdge {
        if (isValueID(primitiveValue.get(), CSSValueAuto))
            return CSS::Keyword::Auto { };
        return toStyleFromCSSValue<Length<>>(state, primitiveValue);
    };

    return ClipRect {
        convertEdge(primitiveValueTop.releaseNonNull()),
        convertEdge(primitiveValueRight.releaseNonNull()),
        convertEdge(primitiveValueBottom.releaseNonNull()),
        convertEdge(primitiveValueLeft.releaseNonNull()),
    };
}

Ref<CSSValue> CSSValueCreation<ClipRect>::operator()(CSSValuePool& pool, const RenderStyle& style, const ClipRect& clipRect)
{
    return CSSRectValue::create({
        createCSSValue(pool, style, clipRect.value->top()),
        createCSSValue(pool, style, clipRect.value->right()),
        createCSSValue(pool, style, clipRect.value->bottom()),
        createCSSValue(pool, style, clipRect.value->left()),
    });
}

// MARK: - Blending

template<> struct Blending<ClipEdge> {
    auto canBlend(const ClipEdge&, const ClipEdge&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const ClipEdge&, const ClipEdge&) -> bool;
    auto blend(const ClipEdge&, const ClipEdge&, const BlendingContext&) -> ClipEdge;
};

template<> struct Blending<ClipRect> {
    auto canBlend(const ClipRect&, const ClipRect&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const ClipRect&, const ClipRect&) -> bool;
    auto blend(const ClipRect&, const ClipRect&, const BlendingContext&) -> ClipRect;
};

auto Blending<ClipEdge>::canBlend(const ClipEdge& a, const ClipEdge& b) -> bool
{
    return a.isAuto() == b.isAuto();
}

auto Blending<ClipEdge>::requiresInterpolationForAccumulativeIteration(const ClipEdge& a, const ClipEdge& b) -> bool
{
    return a.isAuto() != b.isAuto();
}

auto Blending<ClipEdge>::blend(const ClipEdge& a, const ClipEdge& b, const BlendingContext& context) -> ClipEdge
{
    if (a.isAuto() || b.isAuto())
        return context.progress < 0.5 ? a : b;

    ASSERT(canBlend(a, b));
    return Style::blend(*a.value, *b.value, context);
}

auto Blending<ClipRect>::canBlend(const ClipRect& a, const ClipRect& b) -> bool
{
    return Style::canBlend(a.value->top(), b.value->top())
        && Style::canBlend(a.value->right(), b.value->right())
        && Style::canBlend(a.value->bottom(), b.value->bottom())
        && Style::canBlend(a.value->left(), b.value->left());
}

auto Blending<ClipRect>::requiresInterpolationForAccumulativeIteration(const ClipRect& a, const ClipRect& b) -> bool
{
    return Style::requiresInterpolationForAccumulativeIteration(a.value->top(), b.value->top())
        && Style::requiresInterpolationForAccumulativeIteration(a.value->right(), b.value->right())
        && Style::requiresInterpolationForAccumulativeIteration(a.value->bottom(), b.value->bottom())
        && Style::requiresInterpolationForAccumulativeIteration(a.value->left(), b.value->left());
}

auto Blending<ClipRect>::blend(const ClipRect& a, const ClipRect& b, const BlendingContext& context) -> ClipRect
{
    return ClipRect {
        Style::blend(a.value->top(), b.value->top(), context),
        Style::blend(a.value->right(), b.value->right(), context),
        Style::blend(a.value->bottom(), b.value->bottom(), context),
        Style::blend(a.value->left(), b.value->left(), context),
    };
}

auto Blending<Clip>::canBlend(const Clip& a, const Clip& b) -> bool
{
    return !a.isAuto() && !b.isAuto() && Style::canBlend(*a.value, *b.value);
}

auto Blending<Clip>::requiresInterpolationForAccumulativeIteration(const Clip& a, const Clip& b) -> bool
{
    return Style::requiresInterpolationForAccumulativeIteration(
        a.value.value_or(ClipRect { CSS::Keyword::Auto { } }),
        b.value.value_or(ClipRect { CSS::Keyword::Auto { } })
    );
}

auto Blending<Clip>::blend(const Clip& a, const Clip& b, const BlendingContext& context) -> Clip
{
    return Style::blend(
        a.value.value_or(ClipRect { CSS::Keyword::Auto { } }),
        b.value.value_or(ClipRect { CSS::Keyword::Auto { } }),
        context
    );
}

} // namespace Style
} // namespace WebCore
