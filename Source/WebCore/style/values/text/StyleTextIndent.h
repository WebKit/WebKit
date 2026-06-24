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

#include <WebCore/StylePrimitiveNumeric.h>

namespace WebCore {
namespace Style {

// <'text-indent'> = <length-percentage> && hanging? && each-line?
// https://drafts.csswg.org/css-text-3/#propdef-text-indent
struct TextIndent {
    using Amount = LengthPercentage<CSS::AllLayoutUnitClampedUnzoomed>;

    TextIndent(CSS::ValueLiteral<CSS::LengthUnit::Px> literal)
        : amount { literal }
    {
    }

    TextIndent(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal)
        : amount { literal }
    {
    }

    TextIndent(Amount&& amount)
        : amount { WTF::move(amount) }
    {
    }

    TextIndent(Amount&& amount, CSS::Keyword::Hanging hanging)
        : amount { WTF::move(amount) }
        , hanging { hanging }
    {
    }

    TextIndent(Amount&& amount, CSS::Keyword::EachLine eachLine)
        : amount { WTF::move(amount) }
        , eachLine { eachLine }
    {
    }

    TextIndent(Amount&& amount, std::optional<CSS::Keyword::Hanging> hanging, std::optional<CSS::Keyword::EachLine> eachLine)
        : amount { WTF::move(amount) }
        , hanging { hanging }
        , eachLine { eachLine }
    {
    }

    Amount amount;
    std::optional<CSS::Keyword::Hanging> hanging;
    std::optional<CSS::Keyword::EachLine> eachLine;

    bool operator==(const TextIndent&) const = default;
};

template<size_t I> const auto& get(const TextIndent& value)
{
    if constexpr (!I)
        return value.amount;
    else if constexpr (I == 1)
        return value.hanging;
    else if constexpr (I == 2)
        return value.eachLine;
}

// MARK: - Conversion

template<> struct CSSValueConversion<TextIndent> { auto operator()(BuilderState&, const CSSValue&) -> TextIndent; };

// MARK: - Blending

template<> struct Blending<TextIndent> {
    bool NODELETE canBlend(const TextIndent&, const TextIndent&);
    auto blend(const TextIndent&, const TextIndent&, const BlendingContext&) -> TextIndent;
};

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::TextIndent, 3);
