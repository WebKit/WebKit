/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSKeyword.h"
#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

// <'font-style'> auto | normal | italic | oblique [ <angle [-90deg,90deg]>{1,2} ]?
// https://drafts.csswg.org/css-fonts-4/#descdef-font-face-font-style
// FIXME: Support `auto`
struct FontStyleRange {
    struct Oblique {
        using Angle = CSS::Angle<Range{-90, 90}>;

        std::optional<Angle> first;
        std::optional<Angle> second;

        bool operator==(const Oblique&) const = default;
    };

    Variant<Keyword::Normal, Keyword::Italic, Oblique> value;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const FontStyleRange&) const = default;
};

template<size_t I> const auto& get(const FontStyleRange::Oblique& value)
{
    if constexpr (!I)
        return value.first;
    else if constexpr (I == 1)
        return value.second;
}

// MARK: - Conversion

template<> struct CSSValueCreation<FontStyleRange> { Ref<CSSValue> operator()(CSSValuePool&, const FontStyleRange&); };

// MARK: - Serialization

template<> struct Serialize<FontStyleRange::Oblique> { void operator()(StringBuilder&, const SerializationContext&, const FontStyleRange::Oblique&); };

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::FontStyleRange::Oblique, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::FontStyleRange)
