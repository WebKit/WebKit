/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Helpers/Test.h"
#include <WebCore/GradientColorStops.h>

namespace TestWebKitAPI {

using namespace WebCore;

// Unsorted, with several stops at the same offsets.
static GradientColorStops::StopVector unsortedStops()
{
    return {
        { 0.5, Color::red },
        { 0, Color::green },
        { 0.5, Color::blue },
        { 1, Color::black },
        { 0.5, Color::white },
        { 0, Color::yellow },
    };
}

// The stops above, sorted by offset. Stops with equal offsets keep their order.
static GradientColorStops::StopVector stableSortedStops()
{
    return {
        { 0, Color::green },
        { 0, Color::yellow },
        { 0.5, Color::red },
        { 0.5, Color::blue },
        { 0.5, Color::white },
        { 1, Color::black },
    };
}

static void expectStops(const GradientColorStops& stops, const GradientColorStops::StopVector& expected)
{
    ASSERT_EQ(stops.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(stops.stops()[i].offset, expected[i].offset) << "stop " << i;
        EXPECT_TRUE(stops.stops()[i].color == expected[i].color) << "stop " << i;
    }
}

TEST(GradientColorStops, AddColorStopKeepsStopsSortedStably)
{
    GradientColorStops stops;
    for (auto& stop : unsortedStops())
        stops.addColorStop(stop);
    expectStops(stops, stableSortedStops());
}

TEST(GradientColorStops, StopVectorIsSortedStably)
{
    GradientColorStops stops { unsortedStops() };
    expectStops(stops, stableSortedStops());
}

TEST(GradientColorStops, SortedStopVectorKeepsOrder)
{
    GradientColorStops stops { stableSortedStops() };
    expectStops(stops, stableSortedStops());
}

TEST(GradientColorStops, InitializerListKeepsOrder)
{
    GradientColorStops stops {
        { 0, Color::green },
        { 0, Color::yellow },
        { 0.5, Color::red },
        { 0.5, Color::blue },
        { 0.5, Color::white },
        { 1, Color::black },
    };
    expectStops(stops, stableSortedStops());
}

TEST(GradientColorStops, AddColorStopToSortedStops)
{
    GradientColorStops stops { stableSortedStops() };
    stops.addColorStop({ 0.5, Color::cyan });
    stops.addColorStop({ 0, Color::magenta });
    stops.addColorStop({ 0.25, Color::orange });

    expectStops(stops, {
        { 0, Color::green },
        { 0, Color::yellow },
        { 0, Color::magenta },
        { 0.25, Color::orange },
        { 0.5, Color::red },
        { 0.5, Color::blue },
        { 0.5, Color::white },
        { 0.5, Color::cyan },
        { 1, Color::black },
    });
}

TEST(GradientColorStops, MapColorsKeepsOrder)
{
    GradientColorStops stops { unsortedStops() };
    auto mapped = stops.mapColors([](const Color& color) {
        return color.colorWithAlpha(0.5);
    });

    auto expected = stableSortedStops();
    for (auto& stop : expected)
        stop.color = stop.color.colorWithAlpha(0.5);
    expectStops(mapped, expected);
}

TEST(GradientColorStops, ColorsAreEvenlyDistributed)
{
    expectStops(GradientColorStops { std::array<Color, 0> { } }, { });

    expectStops(GradientColorStops { std::array { Color::red } }, {
        { 0, Color::red },
    });

    constexpr std::array displayP3Colors {
        DisplayP3<float> { 1, 0, 0, 1 },
        DisplayP3<float> { 0, 1, 0, 1 },
    };
    expectStops(GradientColorStops { displayP3Colors }, {
        { 0, displayP3Colors[0] },
        { 1, displayP3Colors[1] },
    });

    expectStops(GradientColorStops { std::array { Color::red, Color::green, Color::blue } }, {
        { 0, Color::red },
        { 0.5f, Color::green },
        { 1, Color::blue },
    });

    expectStops(GradientColorStops { Vector<Color> { Color::red, Color::green, Color::blue, Color::black, Color::white, Color::yellow } }, {
        { 0, Color::red },
        { 0.2f, Color::green },
        { 0.4f, Color::blue },
        { 0.6f, Color::black },
        { 0.8f, Color::white },
        { 1, Color::yellow },
    });
}

} // namespace TestWebKitAPI
