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

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// <'contain-intrinsic-*'> = auto? [ none | <length [0,inf]> ]
// https://drafts.csswg.org/css-sizing-4/#intrinsic-size-override
struct ContainIntrinsicSize {
    using Length = Style::Length<CSS::Nonnegative, float>;

    ContainIntrinsicSize(CSS::Keyword::None)
        : type { ContainIntrinsicSizeType::None }
        , length { 0_css_px }
    {
    }

    ContainIntrinsicSize(Length length)
        : type { ContainIntrinsicSizeType::Length }
        , length { length }
    {
    }

    ContainIntrinsicSize(CSS::ValueLiteral<CSS::LengthUnit::Px> literal)
        : type { ContainIntrinsicSizeType::Length }
        , length { literal }
    {
    }

    ContainIntrinsicSize(CSS::Keyword::Auto, Length length)
        : type { ContainIntrinsicSizeType::AutoAndLength }
        , length { length }
    {
    }

    ContainIntrinsicSize(CSS::Keyword::Auto, CSS::Keyword::None)
        : type { ContainIntrinsicSizeType::AutoAndNone }
        , length { 0_css_px }
    {
    }

    bool isNone() const
    {
        return type == ContainIntrinsicSizeType::None;
    }

    bool isAutoAndNone() const
    {
        return type == ContainIntrinsicSizeType::AutoAndNone;
    }

    bool hasAuto() const
    {
        return type == ContainIntrinsicSizeType::AutoAndLength
            || type == ContainIntrinsicSizeType::AutoAndNone;
    }

    bool hasLength() const
    {
        return type == ContainIntrinsicSizeType::Length
            || type == ContainIntrinsicSizeType::AutoAndLength;
    }

    std::optional<Length> tryLength() const
    {
        switch (type) {
        case ContainIntrinsicSizeType::None:
        case ContainIntrinsicSizeType::AutoAndNone:
            return { };
        case ContainIntrinsicSizeType::Length:
        case ContainIntrinsicSizeType::AutoAndLength:
            return length;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    ContainIntrinsicSize addingAuto() const
    {
        switch (type) {
        case ContainIntrinsicSizeType::None:
            return ContainIntrinsicSize { CSS::Keyword::Auto { }, CSS::Keyword::None { } };
        case ContainIntrinsicSizeType::Length:
            return ContainIntrinsicSize { CSS::Keyword::Auto { }, length };
        case ContainIntrinsicSizeType::AutoAndNone:
        case ContainIntrinsicSizeType::AutoAndLength:
            return *this;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (type) {
        case ContainIntrinsicSizeType::None:
            return visitor(CSS::Keyword::None { });
        case ContainIntrinsicSizeType::Length:
            return visitor(length);
        case ContainIntrinsicSizeType::AutoAndLength:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Auto { }, length });
        case ContainIntrinsicSizeType::AutoAndNone:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Auto { }, CSS::Keyword::None { } });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const ContainIntrinsicSize&) const = default;

private:
    friend struct Blending<ContainIntrinsicSize>;

    ContainIntrinsicSize(ContainIntrinsicSizeType type, Length length)
        : type { type }
        , length { length }
    {
    }

    ContainIntrinsicSizeType type;
    Length length;
};

// MARK: - Conversion

template<> struct CSSValueConversion<ContainIntrinsicSize> { auto operator()(BuilderState&, const CSSValue&) -> ContainIntrinsicSize; };

// MARK: - Blending

template<> struct Blending<ContainIntrinsicSize> {
    auto canBlend(const ContainIntrinsicSize&, const ContainIntrinsicSize&) -> bool;
    auto blend(const ContainIntrinsicSize&, const ContainIntrinsicSize&, const BlendingContext&) -> ContainIntrinsicSize;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ContainIntrinsicSize);
