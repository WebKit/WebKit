/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleAppleColorFilter.h"

#include "CSSAppleColorFilterValue.h"
#include "ColorConversion.h"
#include "StyleBuilderChecking.h"
#include "StyleFilterInterpolation.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace Style {

const AppleColorFilter& AppleColorFilter::none()
{
    static NeverDestroyed<AppleColorFilter> value { CSS::Keyword::None { } };
    return value.get();
}

bool AppleColorFilter::transformColor(WebCore::Color& color) const
{
    if (isNone() || !color.isValid())
        return false;
    // Color filter does not apply to semantic CSS colors (like "Windowframe").
    if (color.isSemantic())
        return false;

    auto sRGBAColor = color.toColorTypeLossy<SRGBA<float>>();

    for (auto& value : *this) {
        auto didTransform = WTF::switchOn(value,
            [&](const auto& function) { return function->transformColor(sRGBAColor); }
        );
        if (!didTransform)
            return false;
    }

    color = convertColor<SRGBA<uint8_t>>(sRGBAColor);
    return true;
}

bool AppleColorFilter::inverseTransformColor(WebCore::Color& color) const
{
    if (isNone() || !color.isValid())
        return false;
    // Color filter does not apply to semantic CSS colors (like "Windowframe").
    if (color.isSemantic())
        return false;

    auto sRGBAColor = color.toColorTypeLossy<SRGBA<float>>();

    for (auto& value : *this) {
        auto didInverseTransform = WTF::switchOn(value,
            [&](const auto& function) { return function->inverseTransformColor(sRGBAColor); }
        );
        if (!didInverseTransform)
            return false;
    }

    color = convertColor<SRGBA<uint8_t>>(sRGBAColor);
    return true;
}

// MARK: - Conversions

// MARK: (AppleColorFilterValueList)

auto ToCSS<AppleColorFilterValueList>::operator()(const AppleColorFilterValueList& value, const RenderStyle& style) -> CSS::AppleColorFilterValueList
{
    return CSS::AppleColorFilterValueList::map(value, [&](const auto& x) -> CSS::AppleColorFilterValue { return toCSS(x, style); });
}

auto ToStyle<CSS::AppleColorFilterValueList>::operator()(const CSS::AppleColorFilterValueList& value, const BuilderState& state) -> AppleColorFilterValueList
{
    return AppleColorFilterValueList::map(value, [&](const auto& x) -> AppleColorFilterValue { return toStyle(x, state); });
}

// MARK: (AppleColorFilter)

auto CSSValueConversion<AppleColorFilter>::operator()(BuilderState& state, const CSSValue& value) -> AppleColorFilter
{
    if (value.valueID() == CSSValueNone)
        return CSS::Keyword::None { };

    RefPtr filter = requiredDowncast<CSSAppleColorFilterValue>(state, value);
    if (!filter)
        return CSS::Keyword::None { };

    return toStyle(filter->filter(), state);
}

Ref<CSSValue> CSSValueCreation<AppleColorFilter>::operator()(CSSValuePool&, const RenderStyle& style, const AppleColorFilter& value)
{
    return CSSAppleColorFilterValue::create(toCSS(value, style));
}

// MARK: - Blending

auto Blending<AppleColorFilter>::canBlend(const AppleColorFilter& from, const AppleColorFilter& to, CompositeOperation compositeOperation) -> bool
{
    return canBlendFilterLists(from.m_value, to.m_value, compositeOperation);
}

auto Blending<AppleColorFilter>::blend(const AppleColorFilter& from, const AppleColorFilter& to, const RenderStyle& fromStyle, const RenderStyle& toStyle, const BlendingContext& context) -> AppleColorFilter
{
    auto blendedFilterList = blendFilterLists(from.m_value, to.m_value, fromStyle, toStyle, context);

    if (blendedFilterList.isEmpty())
        return CSS::Keyword::None { };

    return AppleColorFilter { WTF::move(blendedFilterList) };
}

} // namespace Style
} // namespace WebCore
