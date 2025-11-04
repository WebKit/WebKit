/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'item-tolerance'> = normal | <length-percentage [0,∞]> | infinite
// https://drafts.csswg.org/css-grid-3/#item-tolerance
struct ItemTolerance {
    ItemTolerance(CSS::Keyword::Normal keyword)
        : m_value { keyword }
    {
    }

    ItemTolerance(LengthPercentage<CSS::Nonnegative>&& lengthPercentage)
        : m_value { WTFMove(lengthPercentage) }
    {
    }

    ItemTolerance(CSS::Keyword::Infinite keyword)
        : m_value { keyword }
    {
    }

    ALWAYS_INLINE bool isNormal() const { return std::holds_alternative<CSS::Keyword::Normal>(m_value); }
    ALWAYS_INLINE bool isInfinite() const { return std::holds_alternative<CSS::Keyword::Infinite>(m_value); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const ItemTolerance&) const = default;

private:
    template<size_t I, class T> friend struct std::variant_alternative;
    template<class T> friend struct std::variant_size;

    Variant<CSS::Keyword::Normal, LengthPercentage<CSS::Nonnegative>, CSS::Keyword::Infinite> m_value { CSS::Keyword::Normal { } };
};

// MARK: - Conversion

template<> struct CSSValueConversion<ItemTolerance> {
    auto operator()(BuilderState&, const CSSValue&) -> ItemTolerance;
};

// MARK: - Blending

template<> struct Blending<ItemTolerance> {
    auto canBlend(const ItemTolerance&, const ItemTolerance&) -> bool;
    auto blend(const ItemTolerance&, const ItemTolerance&, const BlendingContext&) -> ItemTolerance;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ItemTolerance)
