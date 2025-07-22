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

#include "CSSPrimitiveKeywordSet.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// <'margin-trim'> = none | [ block || inline ] | [ block-start || inline-start || block-end || inline-end ]
//  - `block` is an alias for `block-start block-end`.
//  - `inline` is an alias for `inline-start inline-end`.
// https://drafts.csswg.org/css-box-4/#propdef-margin-trim

// A two level structure (`MarginTrim` and `MarginTrimBase`) is used to allow `MarginTrim` to provide customization
// of the representation while still utilizing the default `CSSValueCreation` and `Serialization` for `MarginTrimBase`
// by explicit cast to `MarginTrimBase` in `MarginTrim::switchOn`.

struct MarginTrimBase : CSS::PrimitiveKeywordSet<MarginTrimBase, CSS::Keyword::None, CSS::Keyword::BlockStart, CSS::Keyword::InlineStart, CSS::Keyword::BlockEnd, CSS::Keyword::InlineEnd> {
    using Base::Base;
};

struct MarginTrim : MarginTrimBase {
    using MarginTrimBase::MarginTrimBase;
    MarginTrim(MarginTrimBase base) : MarginTrimBase { base } { }

    static constexpr MarginTrim blockTrims() { return { CSS::Keyword::BlockStart { }, CSS::Keyword::BlockEnd { } }; }
    static constexpr MarginTrim inlineTrims() { return { CSS::Keyword::InlineStart { }, CSS::Keyword::InlineEnd { } }; }
    static constexpr MarginTrim startTrims() { return { CSS::Keyword::BlockStart { }, CSS::Keyword::InlineStart { } }; }
    static constexpr MarginTrim endTrims() { return { CSS::Keyword::BlockEnd { }, CSS::Keyword::InlineEnd { } }; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (containsAll(blockTrims()) && !containsAny(inlineTrims()))
            return visitor(CSS::Keyword::Block { });

        if (containsAll(inlineTrims()) && !containsAny(blockTrims()))
            return visitor(CSS::Keyword::Inline { });

        if (containsAll(all()))
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Block { }, CSS::Keyword::Inline { } });

        return visitor(static_cast<const MarginTrimBase&>(*this));
    }
};
static_assert(sizeof(MarginTrim) == 1);

// MARK: - Conversion

template<> struct CSSValueConversion<MarginTrim> { auto operator()(BuilderState&, const CSSValue&) -> MarginTrim; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::MarginTrimBase)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::MarginTrim)
