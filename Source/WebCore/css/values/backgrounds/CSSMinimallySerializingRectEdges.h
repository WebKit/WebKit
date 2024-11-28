/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSValueTypes.h"
#include "RectEdges.h"

namespace WebCore {
namespace CSS {

// A set of 4 values parsed and interpreted in the same manner as defined for the margin shorthand.
//
// <minimally-serializing-rect-edges> = <type>{1,4}
//
// - if only 1 value, `a`, is provided, set top, bottom, right & left to `a`.
// - if only 2 values, `a` and `b` are provided, set top & bottom to `a`, right & left to `b`.
// - if only 3 values, `a`, `b`, and `c` are provided, set top to `a`, right to `b`, bottom to `c`, & left to `b`.
//
// As the name implies, the benefit of using this over `RectEdges` directly is that this
// will serialize in its minimal form, checking for element equality and only serializing
// what is necessary.
template<typename T> struct MinimallySerializingRectEdges {
    MinimallySerializingRectEdges(RectEdges<T> edges)
        : value { WTFMove(edges) }
    {
    }

    MinimallySerializingRectEdges(T top, T right, T bottom, T left)
        : value { WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left) }
    {
    }

    const T& top() const    { return value.top(); }
    const T& right() const  { return value.right(); }
    const T& bottom() const { return value.bottom(); }
    const T& left() const   { return value.left(); }

    constexpr bool operator==(const MinimallySerializingRectEdges<T>&) const = default;

    RectEdges<T> value;
};

template<size_t I, typename T> decltype(auto) get(const MinimallySerializingRectEdges<T>& value)
{
    if constexpr (!I)
        return value.top();
    else if constexpr (I == 1)
        return value.right();
    else if constexpr (I == 2)
        return value.bottom();
    else if constexpr (I == 3)
        return value.left();
}

template<typename T> inline constexpr bool TreatAsTupleLike<MinimallySerializingRectEdges<T>> = true;

template<typename CSSType> struct Serialize<MinimallySerializingRectEdges<CSSType>> {
    void operator()(StringBuilder& builder, const MinimallySerializingRectEdges<CSSType>& value)
    {
        if (value.left() != value.right()) {
            serializationForCSS(builder, value.top());
            builder.append(' ');
            serializationForCSS(builder, value.right());
            builder.append(' ');
            serializationForCSS(builder, value.bottom());
            builder.append(' ');
            serializationForCSS(builder, value.left());
            return;
        }
        if (value.bottom() != value.top()) {
            serializationForCSS(builder, value.top());
            builder.append(' ');
            serializationForCSS(builder, value.right());
            builder.append(' ');
            serializationForCSS(builder, value.bottom());
            return;
        }
        if (value.right() != value.top()) {
            serializationForCSS(builder, value.top());
            builder.append(' ');
            serializationForCSS(builder, value.right());
            return;
        }
        serializationForCSS(builder, value.top());
    }
};

} // namespace CSS
} // namespace WebCore

namespace std {

template<typename T> class tuple_size<WebCore::CSS::MinimallySerializingRectEdges<T>> : public std::integral_constant<size_t, 4> { };
template<size_t I, typename T> class tuple_element<I, WebCore::CSS::MinimallySerializingRectEdges<T>> {
public:
    using type = T;
};

}
