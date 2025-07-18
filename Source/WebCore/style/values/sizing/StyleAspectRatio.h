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
#include "StyleRatio.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// <'aspect-ratio'> = auto || <ratio>
// https://drafts.csswg.org/css-sizing-4/#propdef-aspect-ratio
struct AspectRatio {
    AspectRatio(CSS::Keyword::Auto)
        : m_type { AspectRatioType::Auto }
        , m_ratio { 0_css_number, 0_css_number }
    {
    }

    AspectRatio(CSS::Keyword::Auto, Style::Ratio ratio)
        : m_type { AspectRatioType::AutoAndRatio }
        , m_ratio { ratio }
    {
    }

    AspectRatio(Style::Ratio ratio)
        : m_type { (!ratio.numerator.value || !ratio.denominator.value) ? AspectRatioType::AutoZero : AspectRatioType::Ratio }
        , m_ratio { ratio }
    {
    }

    bool isAuto() const             { return m_type == AspectRatioType::Auto; }
    bool isAutoAndRatio() const     { return m_type == AspectRatioType::AutoAndRatio; }
    bool isAutoZero() const         { return m_type == AspectRatioType::AutoZero; }
    bool isRatio() const            { return m_type == AspectRatioType::Ratio; }

    bool hasRatio() const           { return isRatio() || isAutoAndRatio(); }

    std::optional<Style::Ratio> tryRatio() const
    {
        if (isAuto())
            return { };
        return m_ratio;
    }

    Number<CSS::Nonnegative> width() const { return m_ratio.numerator; }
    Number<CSS::Nonnegative> height() const { return m_ratio.denominator; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_type) {
        case AspectRatioType::Auto:
            return visitor(CSS::Keyword::Auto { });
        case AspectRatioType::AutoZero:
        case AspectRatioType::Ratio:
            return visitor(m_ratio);
        case AspectRatioType::AutoAndRatio:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Auto { }, m_ratio });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const AspectRatio&) const = default;

private:
    friend struct Blending<AspectRatio>;

    AspectRatio(AspectRatioType type, Style::Ratio ratio)
        : m_type { type }
        , m_ratio { ratio }
    {
    }

    AspectRatioType m_type;
    Style::Ratio m_ratio;
};

// MARK: - Conversion

template<> struct CSSValueConversion<AspectRatio> { auto operator()(BuilderState&, const CSSValue&) -> AspectRatio; };

// MARK: - Blending

template<> struct Blending<AspectRatio> {
    auto canBlend(const AspectRatio&, const AspectRatio&) -> bool;
    auto blend(const AspectRatio&, const AspectRatio&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> AspectRatio;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::AspectRatio);
