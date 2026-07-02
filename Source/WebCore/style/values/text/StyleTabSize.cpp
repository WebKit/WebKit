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
#include "StyleTabSize.h"

#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "TabSize.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

auto CSSValueConversion<TabSize>::operator()(BuilderState& state, const CSSValue& value) -> TabSize
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return 8_css_number;

    if (primitiveValue->isNumber())
        return toStyleFromCSSValue<TabSize::Spaces>(state, *primitiveValue);
    return toStyleFromCSSValue<TabSize::Length>(state, *primitiveValue);
}

// MARK: - Blending

auto Blending<TabSize>::canBlend(const TabSize& a, const TabSize& b) -> bool
{
    return a.hasSameType(b);
}

auto Blending<TabSize>::blend(const TabSize& a, const TabSize& b, const BlendingContext& context) -> TabSize
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    ASSERT(canBlend(a, b));
    if (a.isSpaces()) {
        ASSERT(b.isSpaces());
        return Style::blend(std::get<TabSize::Spaces>(a.m_value), std::get<TabSize::Spaces>(b.m_value), context);
    } else {
        ASSERT(a.isLength());
        ASSERT(b.isLength());
        return Style::blend(std::get<TabSize::Length>(a.m_value), std::get<TabSize::Length>(b.m_value), context);
    }
}

// MARK: - Platform

auto ToPlatform<TabSize>::operator()(const TabSize& value) -> WebCore::TabSize
{
    return WTF::switchOn(value,
        [](const TabSize::Spaces& spaces) {
            return WebCore::TabSize { spaces.value, SpaceValueType };
        },
        [](const TabSize::Length& length) {
            return WebCore::TabSize { evaluate<float>(length, ZoomNeeded { }), LengthValueType };
        }
    );
}

} // namespace Style
} // namespace WebCore
