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

#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

// <border-image-outset-value> = <length [0,∞]> | <number [0,∞]>
struct BorderImageOutsetValue {
    using Length = CSS::Length<CSS::Nonnegative, float>;
    using Number = CSS::Number<CSS::Nonnegative, float>;

    BorderImageOutsetValue(Length length) : m_value { length } { }
    BorderImageOutsetValue(Number number) : m_value { number } { }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const BorderImageOutsetValue&) const = default;
    bool operator==(ValueLiteral<LengthUnit::Px> literal) const
    {
        if (auto* length = std::get_if<Length>(&m_value))
            return *length == literal;
        return false;
    }
    bool operator==(ValueLiteral<NumberUnit::Number> literal) const
    {
        if (auto* number = std::get_if<Number>(&m_value))
            return *number == literal;
        return false;
    }

private:
    Variant<Length, Number> m_value;
};

// <'border-image-outset'> = [ <length [0,∞]> | <number [0,∞]> ]{1,4}
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-outset
struct BorderImageOutset {
    using Value = BorderImageOutsetValue;
    using Edges = MinimallySerializingSpaceSeparatedRectEdges<Value>;

    Edges values;

    bool operator==(const BorderImageOutset&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(BorderImageOutset, values);

// MARK: - Conversion

template<> struct CSSValueCreation<BorderImageOutset> { auto operator()(CSSValuePool&, const BorderImageOutset&) -> Ref<CSSValue>; };

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::BorderImageOutset)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::BorderImageOutsetValue)
