/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "ColorComponents.h"
#include "ColorConversion.h"
#include "ColorInterpolation.h"
#include "ColorInterpolationMethod.h"
#include "GradientColorStops.h"
#include <wtf/Vector.h>

namespace WebCore {

struct SampledGradientStops {
    Vector<float> locations;
    Vector<float> colorComponents; // 4 components (RGBA) per location.
};

struct StopSample {
    float offset;
    ColorComponents<float, 4> color;
};

// Replace NaN ('none') components in `color` with the corresponding component
// from `neighbor`, per CSS "missing component" replacement rules.
static ColorComponents<float, 4> resolveNoneComponents(ColorComponents<float, 4> color, const ColorComponents<float, 4>& neighbor)
{
    return mapColorComponents([](float c, float n) {
        return std::isnan(c) ? (std::isnan(n) ? 0.0f : n) : c;
    }, color, neighbor);
}

struct SamplingData;

using EvaluateCallback = ColorComponents<float, 4> (*)(const SamplingData&, float offset);

struct SamplingData {
    ColorInterpolationMethod colorInterpolationMethod;
    bool firstStopIsSynthetic { false };
    bool lastStopIsSynthetic { false };
    Vector<StopSample> stopsIS; // stops in InterpolationSpace

    void coarseSampleStops(EvaluateCallback evaluateIS);
    template<typename OutputColorType>
    ColorComponents<float, 4> toColorTypeResolvingNone(size_t index, size_t neighborIndex) const;
};

void SamplingData::coarseSampleStops(EvaluateCallback evaluateIS)
{
    // Coarse sampling density: equivalent of every 19 pixels of a 2048-wide LUT.
    static constexpr float sampleStep = 19.0f / 2048.0f;

    Vector<StopSample> coarseSamples;

    // Iterate stop pairs and subdivide each segment. Hard stops (consecutive
    // stops at the same offset) naturally produce zero interior samples, so the
    // discontinuity is preserved as an exact boundary.
    for (size_t i = 0; i + 1 < stopsIS.size(); ++i) {
        float segStart = stopsIS[i].offset;
        float segEnd = stopsIS[i + 1].offset;

        coarseSamples.append({ segStart, resolveNoneComponents(stopsIS[i].color, stopsIS[i + 1].color) });

        // Subdivide the segment interior.
        float segLength = segEnd - segStart;
        if (segLength <= 0)
            continue;

        int subdivisions = std::max(1, static_cast<int>(std::ceil(segLength / sampleStep)));
        for (int j = 1; j < subdivisions; ++j) {
            float offset = segStart + segLength * j / subdivisions;
            coarseSamples.append({ offset, evaluateIS(*this, offset) });
        }
    }

    // Emit the final endpoint.
    if (stopsIS.size() >= 2)
        coarseSamples.append({ stopsIS.last().offset, resolveNoneComponents(stopsIS.last().color, stopsIS[stopsIS.size() - 2].color) });
    else if (!stopsIS.isEmpty())
        coarseSamples.append({ stopsIS.last().offset, stopsIS.last().color });

    stopsIS = WTF::move(coarseSamples);
}

template<typename OutputColorType>
ColorComponents<float, 4> SamplingData::toColorTypeResolvingNone(size_t index, size_t neighborIndex) const
{
    auto resolved = resolveNoneComponents(stopsIS[index].color, stopsIS[neighborIndex].color);
    return WTF::switchOn(colorInterpolationMethod.colorSpace,
        [&]<typename MethodColorSpace>(const MethodColorSpace&)
        {
            return asColorComponents(WebCore::convertColor<OutputColorType>(
            makeFromComponents<typename MethodColorSpace::ColorType>(resolved)).resolved());
        });
}

// Interpolate between stops in the interpolation color space and return the result
// still in that space (no conversion to the output color space).
template<typename InterpolationSpace, AlphaPremultiplication alphaPremultiplication>
static ColorComponents<float, 4> evaluateColorIS(const SamplingData& data, float offset)
{
    using InterpolationSpaceColorType = typename InterpolationSpace::ColorType;

    // 1. Find stops that bound the requested offset.
    auto [stop0, stop1] = [&] {
        for (size_t stop = 1; stop < data.stopsIS.size(); ++stop) {
            if (offset <= data.stopsIS[stop].offset)
                return std::tie(data.stopsIS[stop - 1], data.stopsIS[stop]);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    // 2. Compute percentage offset between the two stops.
    float t = (stop1.offset == stop0.offset) ? 0.0f : (offset - stop0.offset) / (stop1.offset - stop0.offset);

    // 3. Interpolate the two stops' colors by the computed offset.
    // Synthetic color stops are added to extend the author-provided gradient out to 0 and 1
    // with a solid color, if necessary. These need special handling because `longer hue` gradients
    // would otherwise rotate through 360° of hue in these segments.
    auto interpolatedColor = [&]() {
        if (stop0.offset == 0.0f && data.firstStopIsSynthetic)
            return makeFromComponents<InterpolationSpaceColorType>(stop0.color);

        if (stop1.offset == 1.0f && data.lastStopIsSynthetic)
            return makeFromComponents<InterpolationSpaceColorType>(stop1.color);

        return interpolateColorComponents<alphaPremultiplication>(
            std::get<InterpolationSpace>(data.colorInterpolationMethod.colorSpace),
            makeFromComponents<InterpolationSpaceColorType>(stop0.color), 1.0f - t,
            makeFromComponents<InterpolationSpaceColorType>(stop1.color), t);
    }();

    return asColorComponents(interpolatedColor.unresolved());
}

// Linearly interpolate between stops in the interpolation color space (plain
// component-wise lerp, no hue interpolation rules) and convert to the output
// color space. Used by bisection after coarse sampling, where the hue
// interpolation has already been baked into the coarse IS samples.
template<typename OutputColorType, typename InterpolationSpace>
static ColorComponents<float, 4> evaluateLinearOS(const SamplingData& data, float offset)
{
    using InterpolationSpaceColorType = typename InterpolationSpace::ColorType;

    // 1. Find stops that bound the requested offset.
    auto [stop0, stop1] = [&] {
        for (size_t stop = 1; stop < data.stopsIS.size(); ++stop) {
            if (offset <= data.stopsIS[stop].offset)
                return std::tie(data.stopsIS[stop - 1], data.stopsIS[stop]);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    // 2. Compute percentage offset between the two stops.
    float t = (stop1.offset == stop0.offset) ? 0.0f : (offset - stop0.offset) / (stop1.offset - stop0.offset);

    // 3. Plain linear interpolation of IS components (hue already resolved).
    // Handle NaN ('none') components: replace with the other stop's value per CSS spec.
    auto lerped = mapColorComponents([t](float c0, float c1) {
        if (std::isnan(c0))
            return c1;
        if (std::isnan(c1))
            return c0;
        return c0 + t * (c1 - c0);
    }, stop0.color, stop1.color);

    // 4. Convert to the output color space.
    return asColorComponents(convertColor<OutputColorType>(makeFromComponents<InterpolationSpaceColorType>(lerped)).resolved());
}

static void bisectAndCollectStops(
    EvaluateCallback evaluate, const SamplingData& data,
    float offset0, ColorComponents<float, 4> color0,
    float offset1, ColorComponents<float, 4> color1,
    Vector<float>& locations, Vector<float>& components)
{
    // Stop recursing when the segment is narrower than one step of a 2048-wide LUT.
    static constexpr float minimumSegmentWidth = 1.0f / 2048.0f;
    if (offset1 - offset0 < minimumSegmentWidth)
        return;

    float midOffset = (offset0 + offset1) * 0.5f;
    auto colorMid = evaluate(data, midOffset);

    // Compute the linearly-interpolated color at the midpoint and measure the error.
    auto colorLerp = mapColorComponents([](float c0, float c1) { return c0 + 0.5f * (c1 - c0); }, color0, color1); // NOLINT
    auto absDiff = mapColorComponents([](float a, float b) { return std::abs(a - b); }, colorMid, colorLerp); // NOLINT
    float maxDiff = std::max({ absDiff[0], absDiff[1], absDiff[2], absDiff[3] });

    static constexpr float tolerance = 8.0f / 255.0f;
    if (maxDiff <= tolerance)
        return;

    // The segment is not locally linear: recurse into both halves, then insert a stop
    // at the midpoint so the consumer's linear interpolation passes through the correct color.
    bisectAndCollectStops(evaluate, data, offset0, color0, midOffset, colorMid, locations, components);

    locations.append(midOffset);
    components.append(std::span(colorMid.components));

    bisectAndCollectStops(evaluate, data, midOffset, colorMid, offset1, color1, locations, components);
}

// Build sampling data with stops converted to the interpolation color space.
static SamplingData makeSamplingData(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops::StopVector& stops)
{
    auto convertColorToColorInterpolationSpace = [&](const Color& color) -> ColorComponents<float, 4> {
        return WTF::switchOn(colorInterpolationMethod.colorSpace,
            [&]<typename MethodColorSpace>(const MethodColorSpace&) -> ColorComponents<float, 4> {
                using ColorType = typename MethodColorSpace::ColorType;
                return asColorComponents(color.template toColorTypeLossyCarryingForwardMissing<ColorType>().unresolved());
            });
    };

    auto totalNumberOfStops = stops.size();
    bool hasZero = false;
    bool hasOne = false;

    for (const auto& stop : stops) {
        if (stop.offset == 0) // NOLINT
            hasZero = true;
        else if (stop.offset == 1)
            hasOne = true;
    }

    if (!hasZero)
        totalNumberOfStops++;
    if (!hasOne)
        totalNumberOfStops++;

    Vector<StopSample> stopsIS;
    stopsIS.reserveInitialCapacity(totalNumberOfStops);

    if (!hasZero)
        stopsIS.append({ 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } });

    // Clamp stop offsets to [0, 1], mapping NaN to 0. Stops with NaN or
    // infinite offsets can arrive from CSS calc() expressions like
    // calc(Infinity * -1%) and would otherwise cause infinite recursion
    // in the adaptive bisection.
    auto clampOffset = [](float offset) {
        return std::isnan(offset) ? 0.0f : std::clamp(offset, 0.0f, 1.0f);
    };

    stopsIS.appendContainerWithMapping(stops, [&](const auto& stop) {
        return StopSample { clampOffset(stop.offset), convertColorToColorInterpolationSpace(stop.color) };
    });

    if (!hasOne)
        stopsIS.append({ 1.0f, stopsIS.last().color });

    if (!hasZero)
        stopsIS[0].color = stopsIS[1].color;

    return SamplingData { colorInterpolationMethod, !hasZero, !hasOne, WTF::move(stopsIS) };
}

template<typename OutputColorType>
SampledGradientStops sampleGradientStops(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops::StopVector& stops)
{
    SamplingData data = makeSamplingData(colorInterpolationMethod, stops);

    bool needsCoarseSampling = WTF::switchOn(colorInterpolationMethod.colorSpace,
        []<typename MethodColorSpace>(const MethodColorSpace&)
        {
            return hasHueInterpolationMethod<MethodColorSpace>;
        });
    if (needsCoarseSampling) {
        auto evaluateIS = WTF::switchOn(colorInterpolationMethod.colorSpace,
            [&]<typename MethodColorSpace>(const MethodColorSpace&)->EvaluateCallback
            {
                switch (colorInterpolationMethod.alphaPremultiplication) {
                case AlphaPremultiplication::Unpremultiplied:
                    return &evaluateColorIS<MethodColorSpace, AlphaPremultiplication::Unpremultiplied>;
                case AlphaPremultiplication::Premultiplied:
                    return &evaluateColorIS<MethodColorSpace, AlphaPremultiplication::Premultiplied>;
                }
            });
        data.coarseSampleStops(evaluateIS);
    }

    auto evaluateOS = WTF::switchOn(colorInterpolationMethod.colorSpace,
        [&]<typename MethodColorSpace>(const MethodColorSpace&) -> EvaluateCallback {
            return &evaluateLinearOS<OutputColorType, MethodColorSpace>;
        });

    Vector<float> locations;
    Vector<float> components;

    auto stopCount = data.stopsIS.size();

    // Append the first coarse sample, resolving 'none' against the next stop.
    locations.append(data.stopsIS[0].offset);
    size_t firstNeighbor = stopCount > 1 ? 1 : 0;
    auto firstColor = data.toColorTypeResolvingNone<OutputColorType>(0, firstNeighbor);
    components.append(std::span(firstColor.components));

    // For each coarse interval, adaptively bisect to find non-linear sub-segments,
    // then append the right endpoint of the interval. Each emitted endpoint resolves
    // 'none' (NaN) components against its segment neighbor per CSS spec.
    for (size_t i = 0; i + 1 < stopCount; ++i) {
        auto leftColor = data.toColorTypeResolvingNone<OutputColorType>(i, i + 1);
        auto rightColor = data.toColorTypeResolvingNone<OutputColorType>(i + 1, i);

        bisectAndCollectStops(evaluateOS, data,
            data.stopsIS[i].offset, leftColor,
            data.stopsIS[i + 1].offset, rightColor,
            locations, components);

        locations.append(data.stopsIS[i + 1].offset);
        components.append(std::span(rightColor.components));
    }

    ASSERT(locations.size() * 4 == components.size());

    return { WTF::move(locations), WTF::move(components) };
}

}
