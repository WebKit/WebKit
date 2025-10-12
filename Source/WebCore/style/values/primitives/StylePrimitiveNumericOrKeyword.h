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

#pragma once

#include <WebCore/CSSPrimitiveNumericOrKeyword.h>
#include <WebCore/StylePrimitiveNumeric.h>
#include <algorithm>
#include <wtf/CompactVariant.h>
#include <wtf/FlatteningVariantAdaptor.h>

namespace WebCore {
namespace Style {

template<Numeric N, CSS::PrimitiveKeyword... Ks> class PrimitiveNumericOrKeyword {
public:
    using CSS = typename N::CSS;
    using Raw = typename N::Raw;
    using Calc = typename N::Calc;
    using Dimension = typename N::Dimension;
    using Keywords = WebCore::CSS::PrimitiveKeywordList<Ks...>;
    using Representation = FlatteningCompactVariant<N, Ks...>;

    template<typename U>
        requires std::same_as<std::remove_cvref_t<U>, N>
    PrimitiveNumericOrKeyword(U&& value) : m_value { std::forward<U>(value) } { }

    PrimitiveNumericOrKeyword(WebCore::CSS::ValidKeywordForList<Keywords> auto keyword) : m_value { keyword } { }

    // "Variant-Like" operators.
    template<typename> constexpr bool holdsAlternative() const;
    template<typename... F> constexpr decltype(auto) switchOn(F&&...) const;

    constexpr bool isZero() const;

    bool operator==(const PrimitiveNumericOrKeyword&) const = default;

private:
    Representation m_value;
};

template<Numeric N, CSS::PrimitiveKeyword... Ks>
template<typename U>
constexpr bool PrimitiveNumericOrKeyword<N, Ks...>::holdsAlternative() const
{
    return WTF::holdsAlternative<U>(m_value);
}

template<Numeric N, CSS::PrimitiveKeyword... Ks>
template<typename... F>
constexpr decltype(auto) PrimitiveNumericOrKeyword<N, Ks...>::switchOn(F&&... functors) const
{
    return WTF::switchOn(m_value, std::forward<F>(functors)...);
}

template<Numeric N, CSS::PrimitiveKeyword... Ks>
constexpr bool PrimitiveNumericOrKeyword<N, Ks...>::isZero() const
{
    return switchOn(
        []<HasIsZero U>(const U& value) { return value.isZero(); },
        [](const auto&) { return false; }
    );
}

// MARK: CSS -> Style

template<typename N, typename... Ks>
struct ToStyleMapping<CSS::PrimitiveNumericOrKeyword<N, Ks...>> {
    using type = PrimitiveNumericOrKeyword<StyleType<N>, Ks...>;
};

// MARK: Style -> CSS

template<typename N, typename... Ks>
struct ToCSSMapping<PrimitiveNumericOrKeyword<N, Ks...>> {
    using type = CSS::PrimitiveNumericOrKeyword<CSSType<N>, Ks...>;
};

} // namespace Style
} // namespace WebCore

template<typename N, typename... Ks>
struct WTF::FlatteningVariantTraits<WebCore::Style::PrimitiveNumericOrKeyword<N, Ks...>> {
    using TypeList = typename WTF::FlatteningVariantTraits<typename WebCore::Style::PrimitiveNumericOrKeyword<N, Ks...>::Representation>::TypeList;
};

template<typename N, typename... Ks> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::PrimitiveNumericOrKeyword<N, Ks...>> = true;
