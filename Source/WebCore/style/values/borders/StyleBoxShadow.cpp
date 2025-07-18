/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "StyleBoxShadow.h"

#include "CSSBoxShadowPropertyValue.h"
#include "ColorBlending.h"
#include "RenderStyle.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto ToCSS<BoxShadow>::operator()(const BoxShadow& value, const RenderStyle& style) -> CSS::BoxShadow
{
    return {
        .color = toCSS(value.color, style),
        .location = toCSS(value.location, style),
        .blur = toCSS(value.blur, style),
        .spread = toCSS(value.spread, style),
        .inset = toCSS(value.inset, style),
        .isWebkitBoxShadow = value.isWebkitBoxShadow,
    };
}

auto ToStyle<CSS::BoxShadow>::operator()(const CSS::BoxShadow& value, const BuilderState& state) -> BoxShadow
{
    return {
        .color = value.color ? toStyle(*value.color, state) : Color::currentColor(),
        .location = toStyle(value.location, state),
        .blur = value.blur ? toStyle(*value.blur, state) : 0_css_px,
        .spread = value.spread ? toStyle(*value.spread, state) : 0_css_px,
        .inset = toStyle(value.inset, state),
        .isWebkitBoxShadow = value.isWebkitBoxShadow,
    };
}

Ref<CSSValue> CSSValueCreation<BoxShadowList>::operator()(CSSValuePool&, const RenderStyle& style, const BoxShadowList& value)
{
    CSS::BoxShadowProperty::List list;

    for (const auto& shadow : makeReversedRange(value))
        list.value.append(toCSS(shadow, style));

    return CSSBoxShadowPropertyValue::create(CSS::BoxShadowProperty { WTFMove(list) });
}

auto CSSValueConversion<BoxShadows>::operator()(BuilderState& state, const CSSValue& value) -> BoxShadows
{
    if (value.valueID() == CSSValueNone)
        return CSS::Keyword::None { };

    RefPtr shadow = requiredDowncast<CSSBoxShadowPropertyValue>(state, value);
    if (!shadow)
        return CSS::Keyword::None { };

    return WTF::switchOn(shadow->shadow(),
        [&](const CSS::Keyword::None&) -> BoxShadows {
            return CSS::Keyword::None { };
        },
        [&](const typename CSS::BoxShadowProperty::List& list) -> BoxShadows {
            return BoxShadows::List::map(makeReversedRange(list), [&](const CSS::BoxShadow& element) {
                return toStyle(element, state);
            });
        }
    );
}

// MARK: - Serialization

void Serialize<BoxShadowList>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const BoxShadowList& value)
{
    serializationForCSSOnRangeLike(builder, context, style, makeReversedRange(value), SerializationSeparatorString<BoxShadowList>);
}

// MARK: - Blending

static inline std::optional<CSS::Keyword::Inset> blendInset(std::optional<CSS::Keyword::Inset> a, std::optional<CSS::Keyword::Inset> b, const BlendingContext& context)
{
    if (a == b)
        return b;

    auto aVal = !a ? 1.0 : 0.0;
    auto bVal = !b ? 1.0 : 0.0;

    auto result = WebCore::blend(aVal, bVal, context);
    return result > 0 ? std::nullopt : std::make_optional(CSS::Keyword::Inset { });
}

auto Blending<BoxShadow>::blend(const BoxShadow& a, const BoxShadow& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> BoxShadow
{
    return {
        .color = WebCore::blend(aStyle.colorResolvingCurrentColor(a.color), bStyle.colorResolvingCurrentColor(b.color), context),
        .location = WebCore::Style::blend(a.location, b.location, context),
        .blur = WebCore::Style::blend(a.blur, b.blur, context),
        .spread = WebCore::Style::blend(a.spread, b.spread, context),
        .inset = blendInset(a.inset, b.inset, context),
        .isWebkitBoxShadow = b.isWebkitBoxShadow
    };
}

} // namespace Style
} // namespace WebCore
