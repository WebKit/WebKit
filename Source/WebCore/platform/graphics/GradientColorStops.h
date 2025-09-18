/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/GradientColorStop.h>
#include <algorithm>
#include <optional>
#include <ranges>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {

using GradientColorStops = Vector<GradientColorStop, 2>;

class SortedGradientColorStops {
public:
    explicit SortedGradientColorStops(GradientColorStops&& stops)
        : m_stops(stops)
    {
        ASSERT(std::ranges::is_sorted(m_stops, { }, &GradientColorStop::offset));
    }
    SortedGradientColorStops(SortedGradientColorStops&&) = default;
    SortedGradientColorStops& operator=(SortedGradientColorStops&&) = default;
    operator GradientColorStops() && { return WTFMove(m_stops); }
private:
    GradientColorStops m_stops;
};

template<typename MapFunction>
GradientColorStops mapGradientColors(const GradientColorStops& stops, NOESCAPE MapFunction&& mapFunction)
{
    return stops.map<GradientColorStops>([&] (const GradientColorStop& stop) -> GradientColorStop {
        return { stop.offset, mapFunction(stop.color) };
    });
}

} // namespace WebCore
