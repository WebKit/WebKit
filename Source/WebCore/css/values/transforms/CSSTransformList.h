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

#include "CSSTransformFunctions.h"

namespace WebCore {
namespace CSS {

// https://drafts.csswg.org/css-transforms-1/#typedef-transform-list
// <transform-list> = <transform-function>+
struct TransformList {
    using List = SpaceSeparatedVariantList<TransformFunction, 64>;

    List value;

    // Special value used by `TransformProperty` to indicate `None`.
    explicit TransformList(None)
        : value { }
    {
    }

    TransformList(List list)
        : value { WTFMove(list) }
    {
    }

    // NOTE: This should only be true when `TransformList` is being used to
    // by `Transform`, and `Transform` is trying to represent the value `none`.
    //
    // FIXME: Find a typed method for implementing this optimization. Perhaps
    // creating a new "non-empty vector" type and using a custom Markable trait
    // to utilize the unused "empty" state.
    bool isEmpty() const { return value.isEmpty(); }

    auto begin() const { return value.begin(); }
    auto end() const { return value.end(); }

    bool operator==(const TransformList&) const = default;
};
DEFINE_CSS_TYPE_WRAPPER(TransformList)

} // namespace CSS
} // namespace WebCore
