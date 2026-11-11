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
#include <concepts>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {

// The stops are always sorted by offset. Stops with equal offsets keep the order they were
// specified in, which defines hard color transitions.
class GradientColorStops {
public:
    using StopVector = Vector<GradientColorStop, 2>;

    GradientColorStops() = default;

    // For stops of unknown order. Sorts the stops by offset, unless they are sorted already.
    // The sort is stable: stops with equal offsets keep their order.
    GradientColorStops(StopVector stops)
        : m_stops { WTF::move(stops) }
    {
        if (!std::ranges::is_sorted(m_stops, { }, &GradientColorStop::offset))
            std::ranges::stable_sort(m_stops, { }, &GradientColorStop::offset);
    }

    // The stops must be listed in ascending offset order. Stops with equal offsets are kept in
    // the order they are listed in, which defines hard color transitions. The list is not
    // sorted; debug builds assert the order.
    GradientColorStops(std::initializer_list<GradientColorStop> stops)
        : m_stops { stops }
    {
        ASSERT(isSortedByOffset(m_stops));
    }

    // Stops for the colors, at offsets evenly distributed from 0 to 1: the offset of color i of
    // n colors is i / (n - 1). A single color gets offset 0.
    template<std::ranges::sized_range Colors> requires std::convertible_to<std::ranges::range_value_t<Colors>, Color>
    explicit GradientColorStops(const Colors& colors)
    {
        size_t count = std::ranges::size(colors);
        if (count == 1) {
            m_stops.append(GradientColorStop { 0, *std::ranges::begin(colors) });
            return;
        }
        m_stops.reserveInitialCapacity(count);
        size_t i = 0;
        for (auto& color : colors) {
            float offset = i * 1.0f / (count - 1);
            m_stops.append(GradientColorStop { offset, color });
            ++i;
        }
    }

    // Inserts the stop in sorted position, after the stops with offsets less than or equal to
    // its offset.
    void addColorStop(GradientColorStop stop)
    {
        auto position = std::ranges::upper_bound(m_stops, stop.offset, { }, &GradientColorStop::offset);
        m_stops.insert(position - m_stops.begin(), WTF::move(stop));
    }

    size_t size() const { return m_stops.size(); }
    bool isEmpty() const { return m_stops.isEmpty(); }

    StopVector::const_iterator begin() const LIFETIME_BOUND { return m_stops.begin(); }
    StopVector::const_iterator end() const LIFETIME_BOUND { return m_stops.end(); }

    template<typename MapFunction> GradientColorStops mapColors(NOESCAPE const MapFunction& mapFunction) const
    {
        return {
            m_stops.map<StopVector>([&] (const GradientColorStop& stop) -> GradientColorStop {
                return { stop.offset, mapFunction(stop.color) };
            })
        };
    }

    const StopVector& stops() const LIFETIME_BOUND { return m_stops; }

private:
#if ASSERT_ENABLED
    static bool isSortedByOffset(const StopVector& stops)
    {
        return std::ranges::is_sorted(stops, { }, &GradientColorStop::offset);
    }
#endif

    StopVector m_stops;
};

TextStream& operator<<(TextStream&, const GradientColorStops&);

} // namespace WebCore
