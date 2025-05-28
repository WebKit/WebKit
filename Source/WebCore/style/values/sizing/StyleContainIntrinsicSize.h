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

#include "RenderStyleConstants.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

// <'contain-intrinsic-*'> = auto? [ none | <length [0,inf]> ]
// https://drafts.csswg.org/css-sizing-4/#intrinsic-size-override
struct ContainIntrinsicSize {
    using Length = Style::Length<CSS::Nonnegative, float>;

    ContainIntrinsicSize(ContainIntrinsicSizeType type, std::optional<Length> length)
        : type { type }
        , length { length }
    {
    }

    ContainIntrinsicSize(CSS::Keyword::None)
        : type { ContainIntrinsicSizeType::None }
        , length { }
    {
    }

    ContainIntrinsicSize(Length length)
        : type { ContainIntrinsicSizeType::Length }
        , length { length }
    {
    }

    ContainIntrinsicSize(CSS::Keyword::Auto, Length length)
        : type { ContainIntrinsicSizeType::AutoAndLength }
        , length { length }
    {
    }

    ContainIntrinsicSize(CSS::Keyword::Auto, CSS::Keyword::None)
        : type { ContainIntrinsicSizeType::AutoAndNone }
        , length { }
    {
    }

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        switch (type) {
        case ContainIntrinsicSizeType::None:
            return functor(CSS::Keyword::None { });
        case ContainIntrinsicSizeType::Length:
            return functor(*length);
        case ContainIntrinsicSizeType::AutoAndLength:
            return functor(SpaceSeparatedTuple { CSS::Keyword::Auto { }, *length });
        case ContainIntrinsicSizeType::AutoAndNone:
            return functor(SpaceSeparatedTuple { CSS::Keyword::Auto { }, CSS::Keyword::None { } });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const ContainIntrinsicSize&) const = default;

private:
    ContainIntrinsicSizeType type;
    Markable<Length> length;
};

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::ContainIntrinsicSize> = true;
