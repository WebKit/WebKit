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

#include "CSSTransformList.h"

namespace WebCore {
namespace CSS {

// https://drafts.csswg.org/css-transforms-1/#transform-property
// <'transform'> = none | <transform-list>
struct TransformProperty {
    // NOTE: `none` is represented by an empty `TransformList`.
    TransformList list;

    // Special value used by `TransformProperty` to indicate `None`.
    TransformProperty(None none)
        : list { none }
    {
    }

    TransformProperty(TransformList list)
        : list { WTFMove(list) }
    {
    }

    bool isNone() const { return list.isEmpty(); }

    bool operator==(const TransformProperty&) const = default;
};

template<size_t I> const auto& get(const TransformProperty& value)
{
    if constexpr (!I)
        return value.list;
}

template<> struct Serialize<TransformProperty> { void operator()(StringBuilder&, const TransformProperty&); };

} // namespace CSS
} // namespace WebCore

CSS_TUPLE_LIKE_CONFORMANCE(TransformProperty, 1)
