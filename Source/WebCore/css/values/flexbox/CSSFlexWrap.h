/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/CSSValueTypes.h>

namespace WebCore {
namespace CSS {

// <'flex-wrap'> = nowrap | [ wrap | wrap-reverse ] || balance
// NOTE: `balance` on its own means `wrap balance`, so it has no non-wrapping form.
// https://drafts.csswg.org/css-flexbox-2/#propdef-flex-wrap
enum class FlexWrapType : uint8_t {
    NoWrap,
    Wrap,
    WrapReverse,
    WrapBalance, // Shortens to `balance`
    WrapReverseBalance
};

struct FlexWrap {
    FlexWrapType value;

    constexpr FlexWrap(FlexWrapType value) : value { value } { }

    constexpr FlexWrap(CSS::Keyword::Nowrap) : value { FlexWrapType::NoWrap } { }
    constexpr FlexWrap(CSS::Keyword::Wrap) : value { FlexWrapType::Wrap } { }
    constexpr FlexWrap(CSS::Keyword::WrapReverse) : value { FlexWrapType::WrapReverse } { }
    constexpr FlexWrap(CSS::Keyword::Balance) : value { FlexWrapType::WrapBalance } { }
    constexpr FlexWrap(CSS::Keyword::Wrap, CSS::Keyword::Balance) : value { FlexWrapType::WrapBalance } { }
    constexpr FlexWrap(CSS::Keyword::WrapReverse, CSS::Keyword::Balance) : value { FlexWrapType::WrapReverseBalance } { }

    static constexpr FlexWrap fromRaw(unsigned rawValue) { return static_cast<FlexWrapType>(rawValue); }
    constexpr unsigned toRaw() const { return static_cast<unsigned>(value); }

    constexpr bool isMultiline() const
    {
        return value != FlexWrapType::NoWrap;
    }

    constexpr bool isReverse() const
    {
        return value == FlexWrapType::WrapReverse
            || value == FlexWrapType::WrapReverseBalance;
    }

    constexpr bool isBalance() const
    {
        return value == FlexWrapType::WrapBalance
            || value == FlexWrapType::WrapReverseBalance;
    }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        switch (value) {
        case FlexWrapType::NoWrap:
            return visitor(CSS::Keyword::Nowrap { });
        case FlexWrapType::Wrap:
            return visitor(CSS::Keyword::Wrap { });
        case FlexWrapType::WrapReverse:
            return visitor(CSS::Keyword::WrapReverse { });
        case FlexWrapType::WrapBalance:
            return visitor(CSS::Keyword::Balance { });
        case FlexWrapType::WrapReverseBalance:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::WrapReverse { }, CSS::Keyword::Balance { } });
        }

        RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    }

    constexpr bool operator==(const FlexWrap&) const = default;
};

} // namespace CSS
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::FlexWrap)
