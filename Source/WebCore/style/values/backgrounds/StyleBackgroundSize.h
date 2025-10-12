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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleLengthWrapper.h>

namespace WebCore {
namespace Style {

struct BackgroundSizeLength : LengthWrapperBase<LengthPercentage<CSS::Nonnegative>, CSS::Keyword::Auto> {
    using Base::Base;

    ALWAYS_INLINE bool isAuto() const { return holdsAlternative<CSS::Keyword::Auto>(); }

    bool isKnownZeroOrAuto() const
    {
        return switchOn(
            [](const Base::Fixed& fixed) {
                return fixed.isZero();
            },
            [](const Base::Percentage& percentage) {
                return percentage.isZero();
            },
            [](const Base::Calc&) {
                return false;
            },
            [](const CSS::Keyword::Auto&) {
                return true;
            }
        );
    }
};

// <bg-size> = [ <length-percentage [0,∞]> | auto ]{1,2}@(default=auto) | cover | contain
// https://www.w3.org/TR/css-backgrounds-3/#typedef-bg-size
struct BackgroundSize {
    using LengthSize = SpaceSeparatedSize<BackgroundSizeLength>;

    BackgroundSize(CSS::Keyword::Auto keyword)
        : m_value { LengthSize { keyword, keyword } }
    {
    }

    BackgroundSize(BackgroundSizeLength&& value)
        : m_value { LengthSize { value, CSS::Keyword::Auto { } } }
    {
    }

    BackgroundSize(LengthSize&& value)
        : m_value { WTFMove(value) }
    {
    }

    BackgroundSize(CSS::Keyword::Cover keyword)
        : m_value { keyword }
    {
    }

    BackgroundSize(CSS::Keyword::Contain contain)
        : m_value { contain }
    {
    }

    bool isCover() const { return WTF::holdsAlternative<CSS::Keyword::Cover>(m_value); }
    bool isContain() const { return WTF::holdsAlternative<CSS::Keyword::Contain>(m_value); }
    bool isLengthSize() const { return WTF::holdsAlternative<LengthSize>(m_value); }

    std::optional<LengthSize> tryLengthSize() const
    {
        if (auto* lengthSize = std::get_if<LengthSize>(&m_value))
            return std::make_optional(*lengthSize);
        return { };
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    // FIXME: This name is confusing, given it can't really guarantee empty.
    bool isEmpty() const
    {
        if (auto* lengthSize = std::get_if<LengthSize>(&m_value))
            return lengthSize->width().isKnownZeroOrAuto() || lengthSize->height().isKnownZeroOrAuto();
        return false;
    }

    bool specifiedHeight() const
    {
        if (auto* lengthSize = std::get_if<LengthSize>(&m_value))
            return !lengthSize->height().isAuto();
        return false;
    }

    bool specifiedWidth() const
    {
        if (auto* lengthSize = std::get_if<LengthSize>(&m_value))
            return !lengthSize->width().isAuto();
        return false;
    }

    bool operator==(const BackgroundSize&) const = default;

    bool hasSameType(const BackgroundSize& other) const { return m_value.index() == other.m_value.index(); }

private:
    Variant<LengthSize, CSS::Keyword::Cover, CSS::Keyword::Contain> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<BackgroundSize> { auto operator()(BuilderState&, const CSSValue&) -> BackgroundSize; };
template<> struct CSSValueCreation<BackgroundSize> { auto operator()(CSSValuePool&, const RenderStyle&, const BackgroundSize&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<BackgroundSize> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const BackgroundSize&); };

// MARK: - Blending

template<> struct Blending<BackgroundSize> {
    auto canBlend(const BackgroundSize&, const BackgroundSize&) -> bool;
    auto blend(const BackgroundSize&, const BackgroundSize&, const BlendingContext&) -> BackgroundSize;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::BackgroundSizeLength)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::BackgroundSize)
