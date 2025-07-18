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
#include "StyleTextShadow.h"

#include "CSSTextShadowPropertyValue.h"
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

auto ToCSS<TextShadow>::operator()(const TextShadow& value, const RenderStyle& style) -> CSS::TextShadow
{
    return {
        .color = toCSS(value.color, style),
        .location = toCSS(value.location, style),
        .blur = toCSS(value.blur, style),
    };
}

auto ToStyle<CSS::TextShadow>::operator()(const CSS::TextShadow& value, const BuilderState& state) -> TextShadow
{
    return {
        .color = value.color ? toStyle(*value.color, state) : Color::currentColor(),
        .location = toStyle(value.location, state),
        .blur = value.blur ? toStyle(*value.blur, state) : Length<CSS::Nonnegative> { 0 },
    };
}

Ref<CSSValue> CSSValueCreation<TextShadowList>::operator()(CSSValuePool&, const RenderStyle& style, const TextShadowList& value)
{
    CSS::TextShadowProperty::List list;

    for (const auto& shadow : makeReversedRange(value))
        list.value.append(toCSS(shadow, style));

    return CSSTextShadowPropertyValue::create(CSS::TextShadowProperty { WTFMove(list) });
}

auto CSSValueConversion<TextShadows>::operator()(BuilderState& state, const CSSValue& value) -> TextShadows
{
    if (value.valueID() == CSSValueNone)
        return CSS::Keyword::None { };

    RefPtr shadow = requiredDowncast<CSSTextShadowPropertyValue>(state, value);
    if (!shadow)
        return CSS::Keyword::None { };

    return WTF::switchOn(shadow->shadow(),
        [&](const CSS::Keyword::None&) -> TextShadows {
            return CSS::Keyword::None { };
        },
        [&](const typename CSS::TextShadowProperty::List& list) -> TextShadows {
            return TextShadows::List::map(makeReversedRange(list), [&](const CSS::TextShadow& element) {
                return toStyle(element, state);
            });
        }
    );
}

// MARK: - Serialization

void Serialize<TextShadowList>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const TextShadowList& value)
{
    serializationForCSSOnRangeLike(builder, context, style, makeReversedRange(value), SerializationSeparatorString<TextShadowList>);
}

// MARK: - Blending

auto Blending<TextShadow>::blend(const TextShadow& a, const TextShadow& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> TextShadow
{
    return {
        .color = WebCore::blend(aStyle.colorResolvingCurrentColor(a.color), bStyle.colorResolvingCurrentColor(b.color), context),
        .location = WebCore::Style::blend(a.location, b.location, context),
        .blur = WebCore::Style::blend(a.blur, b.blur, context),
    };
}

} // namespace Style
} // namespace WebCore
