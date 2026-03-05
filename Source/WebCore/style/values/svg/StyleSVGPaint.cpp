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
#include "StyleSVGPaint.h"

#include "AnimationUtilities.h"
#include "CSSURLValue.h"
#include "RenderStyle+SettersInlines.h"
#include "StyleBuilderChecking.h"
#include "StyleForVisitedLink.h"

namespace WebCore {
namespace Style {

bool containsCurrentColor(const Style::SVGPaint& paint)
{
    if (auto color = paint.tryAnyColor())
        return color->containsCurrentColor();
    return false;
}

// MARK: - Conversion

auto CSSValueConversion<SVGPaint>::operator()(BuilderState& state, const CSSValue& value, ForVisitedLink forVisitedLink) -> SVGPaint
{
    if (RefPtr list = dynamicDowncast<CSSValueList>(value)) {
        RefPtr firstValue = list->item(0);
        RefPtr urlValue = requiredDowncast<CSSURLValue>(state, *firstValue);
        if (!urlValue)
            return CSS::Keyword::None { };

        auto url = toStyle(urlValue->url(), state);

        if (list->size() == 1)
            return url;

        RefPtr secondItem = list->item(1);
        if (RefPtr primitiveValue = dynamicDowncast<const CSSPrimitiveValue>(secondItem)) {
            switch (primitiveValue->valueID()) {
            case CSSValueNone:
                return SVGPaint::URLNone { url, CSS::Keyword::None { } };

            case CSSValueCurrentcolor:
                state.style().setDisallowsFastPathInheritance();
                return SVGPaint::URLColor { url, Color::currentColor() };

            default:
                break;
            }
        }

        return SVGPaint::URLColor { url, toStyleFromCSSValue<Color>(state, *protect(list->item(1)), forVisitedLink) };
    }

    if (RefPtr urlValue = dynamicDowncast<CSSURLValue>(value))
        return toStyle(urlValue->url(), state);

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNone:
            return CSS::Keyword::None { };

        case CSSValueCurrentcolor:
            state.style().setDisallowsFastPathInheritance();
            return Color { Color::currentColor() };

        default:
            break;
        }
    }

    return toStyleFromCSSValue<Color>(state, value, forVisitedLink);
}

// MARK: - Blending

auto Blending<SVGPaint>::equals(const SVGPaint& a, const SVGPaint& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    if (!a.hasSameType(b))
        return false;

    // We only support animations between SVGPaints that are pure Color values.
    // For everything else we must return true for this method, otherwise
    // we will try to animate between values forever.

    if (a.isColor())
        return equalsForBlending(a.colorDisregardingType(), b.colorDisregardingType(), aStyle, bStyle);

    return true;
}

auto Blending<SVGPaint>::canBlend(const SVGPaint& a, const SVGPaint& b) -> bool
{
    if (!a.isColor() || !b.isColor())
        return false;
    return Style::canBlend(a.colorDisregardingType(), b.colorDisregardingType());
}

auto Blending<SVGPaint>::blend(const SVGPaint& a, const SVGPaint& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> SVGPaint
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }
    return Style::blend(a.colorDisregardingType(), b.colorDisregardingType(), aStyle, bStyle, context);
}

} // namespace Style
} // namespace WebCore
