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

#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

// <'perspective'> = none | <length [0,∞]>
// https://drafts.csswg.org/css-transforms-2/#propdef-perspective
struct Perspective {
    using Length = Style::Length<CSS::Nonnegative, float>;

    Perspective(CSS::Keyword::None)
        : m_value { }
    {
    }

    Perspective(Length value)
        : m_value { value }
    {
    }

    float usedPerspective() const { return m_value ? std::max(1.0f, m_value->value) : 1.0f; }

    bool isNone() const { return !m_value; }

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        if (!m_value)
            return functor(CSS::Keyword::None { });
        return functor(*m_value);
    }

    bool operator==(const Perspective&) const = default;

private:
    template<typename> friend struct Blending;

    Markable<Length> m_value;
};
static_assert(sizeof(Perspective) == sizeof(float));

template<> struct Blending<Perspective> {
    auto canBlend(const Perspective&, const Perspective&) -> bool;
    auto blend(const Perspective&, const Perspective&, const BlendingContext&) -> Perspective;
};

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::Perspective> = true;
