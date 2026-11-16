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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StyleValueTypes.h"
#include "StyleZoomPrimitives.h"

namespace WebCore {
namespace Style {

// Utility used to store a numeric type along with the zoom to apply during
// evaluation. Useful when storing numeric values outside of `Style::ComputedStyle`.
template<typename T>
struct ZoomResolvable {
    T value;
    ZoomFactor zoom;

    constexpr bool operator==(const ZoomResolvable&) const = default;
};

template<typename T, typename Result> struct Evaluation<ZoomResolvable<T>, Result> {
    template<typename... Rest>
    constexpr auto operator()(const ZoomResolvable<T>& value, Rest&&... rest) -> Result
    {
        return evaluate<Result>(value.value, value.zoom, std::forward<Rest>(rest)...);
    }
};

} // namespace Style
} // namespace WebCore
