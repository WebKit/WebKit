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

#include "CSSPrimitiveDataIndex.h"

namespace WebCore {
namespace CSS {

// Extracted `keyword-only` subset of a `PrimitiveNumericOrKeyword` type.
template<Numeric N, PrimitiveKeyword... Ks> struct PrimitiveKeywords {
    using Index = PrimitiveDataIndex<N, Ks...>;
    using KeywordList = typename Index::KeywordList;

    PrimitiveKeywords(ValidKeywordForList<KeywordList> auto keyword)
        : m_index { keyword }
    {
    }

    explicit PrimitiveKeywords(Index index)
        : m_index { index }
    {
        ASSERT(m_index.isAnyKeyword());
    }

    // MARK: Variant-Like Conformance

    template<ValidKeywordForList<KeywordList> Keyword>
    bool holdsAlternative() const { return isKeyword<Keyword>(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return m_index.visitKeyword(WTF::makeVisitor(std::forward<F>(f)...));
    }

    // MARK: Predicates

    template<ValidKeywordForList<KeywordList> Keyword>
    bool isKeyword() const { return m_index.isKeyword(Keyword { }); }

private:
    Index m_index;
};

} // namespace CSS
} // namespace WebCore
