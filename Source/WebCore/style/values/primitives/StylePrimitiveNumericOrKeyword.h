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

#pragma once

#include "CSSPrimitiveNumericOrKeyword.h"
#include "StylePrimitiveNumeric.h"
#include <algorithm>
#include <wtf/CompactVariant.h>
#include <wtf/FlatteningVariantAdaptor.h>

namespace WebCore {
namespace Style {

template<Numeric N, typename... Ks> struct PrimitiveNumericOrKeyword {
public:
    using CSS = CSS::PrimitiveNumericOrKeyword<typename N::CSS, Ks...>;

    using Specified = N;
    using KeywordList = typename CSS::KeywordList;
    using Keywords = typename CSS::Keywords;

    using Representation = FlatteningCompactVariant<N, Ks...>;

    template<typename U>
    PrimitiveNumericOrKeyword(U&& value) requires (requires { { N(value) }; })
        : m_value { std::forward<U>(value) }
    {
    }

    PrimitiveNumericOrKeyword(WebCore::CSS::ValidKeywordForList<KeywordList> auto keyword)
        : m_value { keyword }
    {
    }

    // "Variant-Like" operators.
    template<typename U> constexpr bool holdsAlternative() const
    {
        return WTF::holdsAlternative<U>(m_value);
    }
    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    size_t index() const { return m_value.index(); }

    bool operator==(const PrimitiveNumericOrKeyword&) const = default;

private:
    Representation m_value;
};

// Specialization of `PrimitiveNumeric` for composite dimension-percentage types.
template<DimensionPercentageNumeric N, typename... Ks> struct PrimitiveNumericOrKeyword<N, Ks...> {
public:
    using CSS = CSS::PrimitiveNumericOrKeyword<typename N::CSS, Ks...>;

    using Specified = N;
    using Dimension = typename N::Dimension;
    using Percentage = typename N::Percentage;
    using Calc = typename N::Calc;
    using KeywordList = typename CSS::KeywordList;
    using Keywords = typename CSS::Keywords;

    using Representation = FlatteningCompactVariant<N, Ks...>;

    PrimitiveNumericOrKeyword(N&& value)
        : m_value { WTF::switchOn(WTFMove(value), [](auto&& x) { return Representation(WTFMove(x)); })}
    {
    }

    // FIXME: Look into removing the switch for the case where the types are the same other than `OtherKs` being a prefix of `Ks`.
    template<typename OtherN, typename... OtherKs>
        requires (sizeof...(OtherKs) != sizeof...(Ks))
    PrimitiveNumericOrKeyword(PrimitiveNumericOrKeyword<OtherN, OtherKs...>&& value)
        : m_value { WTF::switchOn(WTFMove(value), [](auto&& x) { return Representation(WTFMove(x)); })}
    {
    }

    PrimitiveNumericOrKeyword(Dimension&& value)
        : m_value { WTFMove(value) }
    {
    }

    PrimitiveNumericOrKeyword(Percentage&& value)
        : m_value { WTFMove(value) }
    {
    }

    PrimitiveNumericOrKeyword(Calc&& value)
        : m_value { WTFMove(value) }
    {
    }

    PrimitiveNumericOrKeyword(WebCore::CSS::ValidKeywordForList<KeywordList> auto keyword)
        : m_value { keyword }
    {
    }

    PrimitiveNumericOrKeyword(WebCore::CSS::ValueLiteral<Dimension::UnitTraits::canonical> literal)
        : m_value { Dimension { literal } }
    {
    }

    PrimitiveNumericOrKeyword(WebCore::CSS::ValueLiteral<Percentage::UnitTraits::canonical> literal)
        : m_value { Percentage { literal } }
    {
    }

    PrimitiveNumericOrKeyword& operator=(N&& value)
    {
        WTF::switchOn(WTFMove(value), [&](auto&& x) { m_value = WTFMove(x); });
        return *this;
    }

    template<typename OtherN, typename... OtherKs>
        requires (sizeof...(OtherKs) != sizeof...(Ks))
    PrimitiveNumericOrKeyword& operator=(PrimitiveNumericOrKeyword<OtherN, OtherKs...>&& value)
    {
        WTF::switchOn(WTFMove(value), [&](auto&& x) { m_value = WTFMove(x); });
        return *this;
    }

    PrimitiveNumericOrKeyword& operator=(Dimension&& value)
    {
        m_value = WTFMove(value);
        return *this;
    }

    PrimitiveNumericOrKeyword& operator=(Percentage&& value)
    {
        m_value = WTFMove(value);
        return *this;
    }

    PrimitiveNumericOrKeyword& operator=(Calc&& value)
    {
        m_value = WTFMove(value);
        return *this;
    }

    PrimitiveNumericOrKeyword& operator=(WebCore::CSS::ValidKeywordForList<KeywordList> auto keyword)
    {
        m_value = keyword;
        return *this;
    }

    PrimitiveNumericOrKeyword& operator=(WebCore::CSS::ValueLiteral<Dimension::UnitTraits::canonical> literal)
    {
        m_value = Dimension { literal };
        return *this;
    }

    PrimitiveNumericOrKeyword& operator=(WebCore::CSS::ValueLiteral<Percentage::UnitTraits::canonical> literal)
    {
        m_value = Percentage { literal };
        return *this;
    }

    // "Variant-Like" operators.
    template<typename U> constexpr bool holdsAlternative() const
    {
        return WTF::holdsAlternative<U>(m_value);
    }
    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    size_t index() const { return m_value.index(); }

    auto specified() const -> std::optional<Specified>
    {
        return WTF::switchOn(m_value,
            [](const Dimension& value) -> std::optional<Specified> {
                return Specified { value };
            },
            [](const Percentage& value) -> std::optional<Specified> {
                return Specified { value };
            },
            [](const Calc& value) -> std::optional<Specified> {
                return Specified { value };
            },
            [](const auto&) -> std::optional<Specified> {
                return { };
            }
        );
    }

    auto keywords() const -> std::optional<Keywords>
    {
        return WTF::switchOn(m_value,
            [](const Dimension&) -> std::optional<Keywords> {
                return { };
            },
            [](const Percentage&) -> std::optional<Keywords> {
                return { };
            },
            [](const Calc&) -> std::optional<Specified> {
                return { };
            },
            []<CSSValueID Id>(const Constant<Id>& keyword) -> std::optional<Keywords> {
                return Keywords { keyword };
            }
        );
    }

    auto dimension() const -> std::optional<Dimension>
    {
        return WTF::switchOn(m_value,
            [](const Dimension& value) -> std::optional<Dimension> {
                return value;
            },
            [](const auto&) -> std::optional<Dimension> {
                return { };
            }
        );
    }

    auto percentage() const -> std::optional<Percentage>
    {
        return WTF::switchOn(m_value,
            [](const Percentage& value) -> std::optional<Percentage> {
                return value;
            },
            [](const auto&) -> std::optional<Percentage> {
                return { };
            }
        );
    }

    auto calc() const -> std::optional<Calc>
    {
        return WTF::switchOn(m_value,
            [](const Calc& value) -> std::optional<Calc> {
                return value;
            },
            [](const auto&) -> std::optional<Calc> {
                return { };
            }
        );
    }

    bool operator==(const PrimitiveNumericOrKeyword&) const = default;

private:
    Representation m_value;
};

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
