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

#include <WebCore/CSSPrimitiveNumericConcepts.h>
#include <WebCore/CSSPrimitiveNumericRaw.h>
#include <limits>
#include <type_traits>
#include <wtf/Brigand.h>
#include <wtf/EnumTraits.h>

namespace WebCore {
namespace CSS {

// MARK: - Concepts

// Concept for use in generic contexts to filter on Constant keyword CSS types.
template<typename Keyword> concept PrimitiveKeyword
    = std::same_as<Keyword, Constant<Keyword::value>>;

// Concept for use in generic contexts to filter on keywords that are valid for the provided `Keywords` list.
template<typename Keyword, typename KeywordsList> concept ValidKeywordForList
#if COMPILER(GCC) && (__GNUC__ < 13)
    = KeywordsList::isValidKeyword(Keyword()) && PrimitiveKeyword<Keyword>;
#else
    = PrimitiveKeyword<Keyword> && KeywordsList::isValidKeyword(Keyword());
#endif

// MARK: - Primitive Keywords List

template<PrimitiveKeyword... Ks> struct PrimitiveKeywordList {
    static constexpr auto count = sizeof...(Ks);
    static constexpr auto identifiers = std::array { Ks::value... };
    static constexpr auto tuple = std::tuple { Ks { }... };

    static consteval bool isValidKeyword(PrimitiveKeyword auto keyword)
    {
        return std::ranges::find(identifiers, keyword.value) != identifiers.end();
    }

    static consteval size_t offsetForKeyword(PrimitiveKeyword auto keyword)
    {
         return std::distance(identifiers.begin(), std::ranges::find(identifiers, keyword.value));
    }

    template<typename F> static constexpr decltype(auto) visitKeywordAtOffset(size_t offset, F&& f)
    {
        return WTF::visitTupleElementAtIndex(std::forward<F>(f), offset, tuple);
    }
};

// Specialization for an empty keyword list.
template<> struct PrimitiveKeywordList<> {
    static constexpr auto count = 0;

    static consteval bool isValidKeyword(PrimitiveKeyword auto)
    {
        return false;
    }

    static consteval size_t offsetForKeyword(PrimitiveKeyword auto)
    {
        return 0;
    }
};

} // namespace CSS
} // namespace WebCore
