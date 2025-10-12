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

#include <WebCore/ScopedName.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <single-animation-name> = none | <keyframes-name>
// https://www.w3.org/TR/css-animations-1/#propdef-animation-name
struct SingleAnimationName {
    SingleAnimationName(CSS::Keyword::None)
        : m_value { ScopedName { nullAtom() } }
    {
    }

    SingleAnimationName(ScopedName&& keyframesName)
        : m_value { WTFMove(keyframesName) }
    {
    }

    bool isNone() const { return m_value.name.isNull(); }
    bool isKeyframesName() const { return !m_value.name.isNull(); }
    std::optional<ScopedName> tryKeyframesName() const { return isKeyframesName() ? std::make_optional(m_value) : std::nullopt; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });
        return visitor(m_value);
    }

    bool operator==(const SingleAnimationName&) const = default;

private:
    ScopedName m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<SingleAnimationName> { auto operator()(BuilderState&, const CSSValue&) -> SingleAnimationName; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SingleAnimationName)
