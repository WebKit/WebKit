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

#include <WebCore/StyleScrollFunction.h>
#include <WebCore/StyleValueTypes.h>
#include <WebCore/StyleViewFunction.h>

namespace WebCore {
namespace Style {

// <single-animation-timeline> = auto | none | <custom-ident> | <scroll()> | <view()>
// https://www.w3.org/TR/css-animations-2/#typedef-single-animation-timeline
struct SingleAnimationTimeline {
    SingleAnimationTimeline(CSS::Keyword::Auto keyword)
        : m_value { keyword }
    {
    }

    SingleAnimationTimeline(CSS::Keyword::None keyword)
        : m_value { keyword }
    {
    }

    SingleAnimationTimeline(CustomIdentifier&& identifier)
        : m_value { identifier }
    {
    }

    SingleAnimationTimeline(ScrollFunction&& scrollFunction)
        : m_value { WTFMove(scrollFunction) }
    {
    }

    SingleAnimationTimeline(ViewFunction&& viewFunction)
        : m_value { WTFMove(viewFunction) }
    {
    }

    bool isAuto() const { return std::holds_alternative<CSS::Keyword::Auto>(m_value); }
    bool isNone() const { return std::holds_alternative<CSS::Keyword::None>(m_value); }
    bool isCustomIdentifier() const { return std::holds_alternative<CustomIdentifier>(m_value); }
    std::optional<CustomIdentifier> tryCustomIdentifier() const { return isCustomIdentifier() ? std::make_optional(std::get<CustomIdentifier>(m_value)) : std::nullopt; }
    bool isScrollFunction() const { return std::holds_alternative<ScrollFunction>(m_value); }
    std::optional<ScrollFunction> tryScrollFunction() const { return isScrollFunction() ? std::make_optional(std::get<ScrollFunction>(m_value)) : std::nullopt; }
    bool isViewFunction() const { return std::holds_alternative<ViewFunction>(m_value); }
    std::optional<ViewFunction> tryViewFunction() const { return isViewFunction() ? std::make_optional(std::get<ViewFunction>(m_value)) : std::nullopt; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const SingleAnimationTimeline&) const = default;

private:
    Variant<CSS::Keyword::Auto, CSS::Keyword::None, CustomIdentifier, ScrollFunction, ViewFunction> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<SingleAnimationTimeline> { auto operator()(BuilderState&, const CSSValue&) -> SingleAnimationTimeline; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SingleAnimationTimeline)
