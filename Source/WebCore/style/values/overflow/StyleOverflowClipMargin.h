/*
 * Copyright (C) 2026 Suraj Thanugundla <contact@surajt.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSValueKeywords.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'overflow-clip-margin'> = <visual-box> || <length [0,∞]>
// https://drafts.csswg.org/css-overflow/#overflow-clip-margin
struct OverflowClipMargin {
    using Length = Style::Length<CSS::Nonnegative, float>;

    OverflowClipMargin(CSS::ValueLiteral<CSS::LengthUnit::Px> length)
        : m_value { length }
    {
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const OverflowClipMargin&) const = default;

private:
    Variant<VisualBox, Length, SpaceSeparatedTuple<VisualBox, Length>> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<OverflowClipMargin> {
    auto operator()(BuilderState&, const CSSValue&) -> OverflowClipMargin;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::OverflowClipMargin)
