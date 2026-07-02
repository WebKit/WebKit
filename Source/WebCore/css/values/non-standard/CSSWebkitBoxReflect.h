/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSMaskBorder.h"
#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

struct WebkitBoxReflection {
    using Direction = Variant<Keyword::Above, Keyword::Below, Keyword::Left, Keyword::Right>;
    using Offset = LengthPercentage<CSS::All, float>;
    using Mask = MaskBorder;

    Direction direction;
    Offset offset;
    Mask mask;

    bool operator==(const WebkitBoxReflection&) const = default;
};
template<size_t I> const auto& get(const WebkitBoxReflection& value)
{
    if constexpr (!I)
        return value.direction;
    else if constexpr (I == 1)
        return value.offset;
    else if constexpr (I == 2)
        return value.mask;
}

// <'-webkit-box-reflect'> = none | [ [ above | below | left | right ] <length-percentage>? <mask-border>? ]
// NOTE: There is no standard associated with this property.
struct WebkitBoxReflect {
    WebkitBoxReflect(CSS::Keyword::None) { }
    WebkitBoxReflect(WebkitBoxReflection&& reflection) : m_reflection { WTF::move(reflection) } { }

    bool isNone() const { return !m_reflection; }
    bool isReflection() const { return !!m_reflection; }
    std::optional<WebkitBoxReflection> tryReflection() const { return m_reflection; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });
        return visitor(*m_reflection);
    }

    bool operator==(const WebkitBoxReflect&) const = default;

private:
    std::optional<WebkitBoxReflection> m_reflection { };
};

// MARK: - Serialization

template<> struct Serialize<WebkitBoxReflection> { void operator()(StringBuilder&, const CSS::SerializationContext&, const WebkitBoxReflection&); };

} // namespace CSS
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::WebkitBoxReflection, 3)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::WebkitBoxReflect);
