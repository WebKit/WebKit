/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <algorithm>
#include <wtf/Vector.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

struct FloatSegment {
    float begin { 0.f };
    float end { 0.f };
    bool operator==(const FloatSegment&) const = default;
    float length() const { return end - begin; }
    FloatSegment dilate(float length) { return { begin - length, end + length }; }
};


// Computes difference between a line segment `a` and list of line segments `bs` that are dilated by
// dilationAmount.
// The difference list is further filtered to only results > dilationAmount.
//
// Vector<FloatSegment> intersections = ...;
// Vector<FloatSegment> solidLine { { 0, totalWidth } };
// Vector<FloatSegment> diff = difference(solidLine, dilate(intersections, dilationAmount));
// return filter(diff, [](auto&& s) { return s.length() > dilationAmount });
// <=>
// return differenceWithDilation({ 0, totalWidth }, intersections);
inline Vector<FloatSegment> differenceWithDilation(FloatSegment a, Vector<FloatSegment>&& bs, float dilationAmount)
{
    std::sort(bs.begin(), bs.end(), [](auto&& a, auto&& b) {
        return a.begin < b.begin;
    });

    auto result = bs.begin();
    for (auto b : bs) {
        if (a.begin >= a.end)
            break;
        b = b.dilate(dilationAmount);
        if (b.begin >= a.end)
            break;
        FloatSegment candidate { a.begin, b.begin };
        if (candidate.length() > dilationAmount) {
            *result = candidate;
            result = std::next(result);
        }
        a.begin = std::max(a.begin, b.end);
    }
    bs.shrink(result - bs.begin());
    if (a.length() > dilationAmount)
        bs.append(a);
    return WTFMove(bs);
}

WEBCORE_EXPORT TextStream& operator<<(TextStream&, FloatSegment);

} // namespace WebCore
