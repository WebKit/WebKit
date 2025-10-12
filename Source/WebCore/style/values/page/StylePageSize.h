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

#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {
namespace Style {

// <'size'> (for @page) = <length [0,∞]>{1,2} | auto | [ <page-size> || [ portrait | landscape ] ]
// https://drafts.csswg.org/css-page-3/#descdef-page-size
struct PageSize {
    using Lengths = MinimallySerializingSpaceSeparatedSize<Length<CSS::Nonnegative>>;

    PageSize(Lengths&& lengths) : m_value { WTFMove(lengths) } { }
    PageSize(CSS::Keyword::Auto keyword) : m_value { keyword } { }
    PageSize(CSS::Keyword::Portrait keyword) : m_value { keyword } { }
    PageSize(CSS::Keyword::Landscape keyword) : m_value { keyword } { }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const PageSize&) const = default;

private:
    Variant<Lengths, CSS::Keyword::Auto, CSS::Keyword::Portrait, CSS::Keyword::Landscape> m_value { CSS::Keyword::Auto { } };
};

// MARK: - Conversion

template<> struct CSSValueConversion<PageSize> { auto operator()(BuilderState&, const CSSValue&) -> PageSize; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::PageSize)
