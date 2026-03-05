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

#include <WebCore/StyleValueTypes.h>
#include <WebCore/TouchAction.h>

namespace WebCore {
namespace Style {

// <'touch-action'> = auto | none | [ [ pan-x | pan-left | pan-right ] || [ pan-y | pan-up | pan-down ] ] | manipulation
// FIXME: Currently implemented grammar is: auto | none | [ pan-x || pan-y || pinch-zoom ] | manipulation
// https://w3c.github.io/pointerevents/#the-touch-action-css-property

enum class TouchActionValue : uint8_t {
    PanX,
    PanY,
    PinchZoom,
};

using TouchActionValueEnumSet = SpaceSeparatedEnumSet<TouchActionValue>;

struct TouchAction {
    constexpr TouchAction(CSS::Keyword::Auto keyword) : m_value { keyword } { }
    constexpr TouchAction(CSS::Keyword::None keyword) : m_value { keyword } { }
    constexpr TouchAction(CSS::Keyword::Manipulation keyword) : m_value { keyword } { }
    constexpr TouchAction(TouchActionValueEnumSet&& set) : m_value { WTF::move(set) } { }
    constexpr TouchAction(TouchActionValue value) : m_value { TouchActionValueEnumSet { value } } { }

    constexpr bool isNone() const { return WTF::holdsAlternative<CSS::Keyword::None>(m_value); }
    constexpr bool isAuto() const { return WTF::holdsAlternative<CSS::Keyword::Auto>(m_value); }
    constexpr bool isManipulation() const { return WTF::holdsAlternative<CSS::Keyword::Manipulation>(m_value); }
    constexpr bool isEnumSet() const { return WTF::holdsAlternative<TouchActionValueEnumSet>(m_value); }
    constexpr std::optional<TouchActionValueEnumSet> tryEnumSet() const { return isEnumSet() ? std::make_optional(std::get<TouchActionValueEnumSet>(m_value)) : std::nullopt; }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    constexpr bool operator==(const TouchAction&) const = default;

private:
    Variant<CSS::Keyword::Auto, CSS::Keyword::None, TouchActionValueEnumSet, CSS::Keyword::Manipulation> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<TouchAction> { auto operator()(BuilderState&, const CSSValue&) -> TouchAction; };

// MARK: - Platform

template<> struct ToPlatform<TouchAction> { auto operator()(const TouchAction&) -> OptionSet<WebCore::TouchAction>; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TouchAction)
