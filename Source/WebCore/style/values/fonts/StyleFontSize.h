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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {
namespace Style {

// <'font-size'> = <absolute-size> | <relative-size> | <length-percentage [0,∞]> | math
// FIXME: Support `math` value.
// NOTE: Resolved down to a <length> at style building time.
// https://drafts.csswg.org/css-fonts-4/#propdef-font-size
struct FontSize {
    using Length = Style::Length<CSS::Nonnegative, float>;

    constexpr FontSize(Length size) : value { size } { }
    constexpr FontSize(typename Length::ResolvedValueType size) : value { size } { }
    constexpr FontSize(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : value { literal } { }

    constexpr float platform() const { return value.unresolvedValue(); }

    bool operator==(const FontSize&) const = default;

    Length value;
};
DEFINE_TYPE_WRAPPER_GET(FontSize, value)

// MARK: - Blending

template<> struct Blending<FontSize> {
    auto equals(const FontSize&, const FontSize&, const RenderStyle&, const RenderStyle&) -> bool;
    auto blend(const FontSize&, const FontSize&, const BlendingContext&) -> FontSize;
};

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::FontSize)
