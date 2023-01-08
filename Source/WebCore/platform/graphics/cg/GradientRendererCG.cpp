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

#include "config.h"
#include "GradientRendererCG.h"

#include "ColorInterpolation.h"
#include "ColorSpaceCG.h"
#include "GradientColorStops.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>

namespace WebCore {

GradientRendererCG::GradientRendererCG(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops)
    : m_strategy { pickStrategy(colorInterpolationMethod, stops) }
{
}

// MARK: - Strategy selection.

#if !HAVE(CORE_GRAPHICS_PREMULTIPLIED_INTERPOLATION_GRADIENT)

enum class EmulatedAlphaPremultiplicationAnalysisResult {
    UseGradientAsIs,
    UseGradientWithAlphaTransforms,
    UseShading
};

enum class AlphaType {
    Opaque,
    PartiallyTransparent,
    FullyTransparent
};

static constexpr AlphaType classifyAlphaType(float alpha)
{
    if (alpha == 1.0f)
        return AlphaType::Opaque;
    if (alpha == 0.0f)
        return AlphaType::FullyTransparent;
    return AlphaType::PartiallyTransparent;
}

static EmulatedAlphaPremultiplicationAnalysisResult analyzeColorStopsForEmulatedAlphaPremultiplicationOpportunity(const GradientColorStops& stops)
{
    if (stops.size() < 2)
        return EmulatedAlphaPremultiplicationAnalysisResult::UseGradientAsIs;

    bool uniformAlpha = true;

    auto& initialStop = *stops.begin();
    auto previousStopAlpha = initialStop.color.alphaAsFloat();
    auto previousStopAlphaType = classifyAlphaType(previousStopAlpha);

    auto stopIterator = stops.begin();
    ++stopIterator;

    while (stopIterator != stops.end()) {
        auto& stop = *stopIterator;

        auto stopAlpha = stop.color.alphaAsFloat();
        auto stopAlphaType = classifyAlphaType(stopAlpha);

        switch (stopAlphaType) {
        case AlphaType::Opaque:
            switch (previousStopAlphaType) {
            case AlphaType::Opaque:
                break;
            case AlphaType::PartiallyTransparent:
                return EmulatedAlphaPremultiplicationAnalysisResult::UseShading;
            case AlphaType::FullyTransparent:
                uniformAlpha = false;
                break;
            }
            break;

        case AlphaType::PartiallyTransparent:
            switch (previousStopAlphaType) {
            case AlphaType::Opaque:
                return EmulatedAlphaPremultiplicationAnalysisResult::UseShading;
            case AlphaType::PartiallyTransparent:
                if (previousStopAlpha != stopAlpha)
                    return EmulatedAlphaPremultiplicationAnalysisResult::UseShading;
                break;
            case AlphaType::FullyTransparent:
                uniformAlpha = false;
                break;
            }
            break;

        case AlphaType::FullyTransparent:
            switch (previousStopAlphaType) {
            case AlphaType::Opaque:
            case AlphaType::PartiallyTransparent:
                uniformAlpha = false;
                break;

            case AlphaType::FullyTransparent:
                break;
            }
            break;
        }

        previousStopAlpha = stopAlpha;
        previousStopAlphaType = stopAlphaType;

        ++stopIterator;
    }

    return uniformAlpha ? EmulatedAlphaPremultiplicationAnalysisResult::UseGradientAsIs : EmulatedAlphaPremultiplicationAnalysisResult::UseGradientWithAlphaTransforms;
}

static GradientColorStops alphaTransformStopsToEmulateAlphaPremultiplication(const GradientColorStops& stops)
{
    // The following is the set of transforms that can be performed on a color stop list to transform it from one used with a premuliplied alpha gradient
    // implmentation to one used by with an un-premultiplied gradient implementation.

    // ... Opaque  -> Transparent -> Opaque  ...                                  ==>    ... Opaque  -> TRANSFORM{Transparent(PreviousChannels)} | ADDITION{Transparent(NextChannels)} -> Opaque  ...
    // ... Partial -> Transparent -> Partial ...                                  ==>    ... Partial -> TRANSFORM{Transparent(PreviousChannels)} | ADDITION{Transparent(NextChannels)} -> Partial ...
    // ... Opaque  -> Transparent -> Partial ...                                  ==>    ... Opaque  -> TRANSFORM{Transparent(PreviousChannels)} | ADDITION{Transparent(NextChannels)} -> Partial ...
    // ... Partial -> Transparent -> Opaque  ...                                  ==>    ... Partial -> TRANSFORM{Transparent(PreviousChannels)} | ADDITION{Transparent(NextChannels)} -> Opaque  ...
    //
    // ... Opaque  -> Transparent -> Transparent -> Opaque  ...                   ==>    ... Opaque  -> TRANSFORM{Transparent(PreviousChannels)} -> TRANSFORM{Transparent(NextChannels)} -> Opaque  ...
    // ... Partial -> Transparent -> Transparent -> Partial ...                   ==>    ... Partial -> TRANSFORM{Transparent(PreviousChannels)} -> TRANSFORM{Transparent(NextChannels)} -> Partial ...
    // ... Opaque  -> Transparent -> Transparent -> Partial ...                   ==>    ... Opaque  -> TRANSFORM{Transparent(PreviousChannels)} -> TRANSFORM{Transparent(NextChannels)} -> Partial ...
    // ... Partial -> Transparent -> Transparent -> Opaque  ...                   ==>    ... Partial -> TRANSFORM{Transparent(PreviousChannels)} -> TRANSFORM{Transparent(NextChannels)} -> Opaque  ...
    //
    // ... Opaque  -> Transparent -> Transparent -> Transparent -> Opaque  ...    ==>    ... Opaque  -> TRANSFORM{Transparent(PreviousChannels)} -> Transparent -> TRANSFORM{Transparent(NextChannels)} -> Opaque  ...
    // ... Partial -> Transparent -> Transparent -> Transparent -> Partial ...    ==>    ... Partial -> TRANSFORM{Transparent(PreviousChannels)} -> Transparent -> TRANSFORM{Transparent(NextChannels)} -> Partial ...
    // ... Opaque  -> Transparent -> Transparent -> Transparent -> Partial ...    ==>    ... Opaque  -> TRANSFORM{Transparent(PreviousChannels)} -> Transparent -> TRANSFORM{Transparent(NextChannels)} -> Partial ...
    // ... Partial -> Transparent -> Transparent -> Transparent -> Opaque  ...    ==>    ... Partial -> TRANSFORM{Transparent(PreviousChannels)} -> Transparent -> TRANSFORM{Transparent(NextChannels)} -> Opaque  ...
    //
    // [ Transparent -> Opaque      ...                                           ==>    [ TRANSFORM{Transparent(NextChannels)} -> Opaque      ...
    // [ Transparent -> Partial     ...                                           ==>    [ TRANSFORM{Transparent(NextChannels)} -> Partial     ...
    // [ Transparent -> Transparent ...                                           ==>    [ Transparent                          -> Transparent ...
    //
    // ... Opaque       -> Transparent ]                                          ==>    ... Opaque      -> TRANSFORM{Transparent(PreviousChannels)} ]
    // ... Partial      -> Transparent ]                                          ==>    ... Partial     -> TRANSFORM{Transparent(PreviousChannels)} ]
    // ... Transparent  -> Transparent ]                                          ==>    ... Transparent -> Transparent                              ]

    // For each stop the following actions are possible:
    //      - Default           Append self
    //      - Steal previous    Append previous.colorWithAlpha(0.0f)
    //      - Steal next        Append next.colorWithAlpha(0.0f)
    //      - Split             Append previous.colorWithAlpha(0.0f) + next.colorWithAlpha(0.0f)

    ASSERT(stops.size() > 1);

    // Special case when we only have two stops to avoid complicating the cases with more.
    if (stops.size() == 2) {
        // Illegal pairs (ruled out in analysis)
        //   Opaque  -> Partial
        //   Partial -> Opaque
        //
        // Possible pairs
        //   Opaque      -> Opaque       (default, default)
        //   Partial     -> Partial      (default, default)
        //   Transparent -> Transparent  (default, default)
        //   Opaque      -> Transparent  (default, steal previous)
        //   Partial     -> Transparent  (default, steal previous)
        //   Transparent -> Opaque       (steals next, default)
        //   Transparent -> Partial      (steals next, default)

        GradientColorStops::StopVector result;
        result.reserveInitialCapacity(2);

        auto& stop1 = stops.stops()[0];
        auto& stop2 = stops.stops()[1];

        auto stop1AlphaType = classifyAlphaType(stop1.color.alphaAsFloat());
        auto stop2AlphaType = classifyAlphaType(stop2.color.alphaAsFloat());

        switch (stop1AlphaType) {
        case AlphaType::Opaque:
        case AlphaType::PartiallyTransparent:
            // ACTION (stop1): Default.
            result.uncheckedAppend(stop1);

            switch (stop2AlphaType) {
            case AlphaType::Opaque:
            case AlphaType::PartiallyTransparent:
                // ACTION (stop2): Default.
                result.uncheckedAppend(stop2);
                break;
            case AlphaType::FullyTransparent:
                // ACTION (stop2): Steal previous.
                result.uncheckedAppend({ stop2.offset, stop1.color.colorWithAlpha(0.0f) });
                break;
            }

            break;

        case AlphaType::FullyTransparent:
            switch (stop2AlphaType) {
            case AlphaType::Opaque:
            case AlphaType::PartiallyTransparent:
                // ACTION (stop1): Steal next.
                result.uncheckedAppend({ stop1.offset, stop2.color.colorWithAlpha(0.0f) });
                break;
            case AlphaType::FullyTransparent:
                // ACTION (stop1): Default.
                result.uncheckedAppend(stop1);
                break;
            }
            // ACTION (stop2): Default.
            result.uncheckedAppend(stop2);
            break;
        }

        return GradientColorStops::Sorted { WTFMove(result) };
    }

    GradientColorStops::StopVector result;

    // 1. Handle first stop.
    //
    // [first stop matters]
    //
    // Illegal pairs (ruled out in analysis)
    //   Opaque  -> Partial
    //   Partial -> Opaque
    //
    // Possible pairs
    //   Opaque      -> Opaque       (default)
    //   Partial     -> Partial      (default)
    //   Transparent -> Transparent  (default)
    //   Opaque      -> Transparent  (default)
    //   Partial     -> Transparent  (default)
    //   Transparent -> Opaque       (steals next)
    //   Transparent -> Partial      (steals next)

    auto& firstStop = stops.stops()[0];
    auto& secondStop = stops.stops()[1];

    auto firstStopAlphaType = classifyAlphaType(firstStop.color.alphaAsFloat());
    auto secondStopAlphaType = classifyAlphaType(secondStop.color.alphaAsFloat());

    if (firstStopAlphaType == AlphaType::FullyTransparent && secondStopAlphaType != AlphaType::FullyTransparent) {
        // ACTION: Steal next.
        result.append({ firstStop.offset, secondStop.color.colorWithAlpha(0.0f) });
    } else {
        // ACTION: Default.
        result.append(firstStop);
    }

    // 2. Handle middle stops.
    //
    // [middle stop matters]
    //
    // Illegal triplets (ruled out in analysis)
    //   Opaque      -> Opaque      -> Partial
    //   Opaque      -> Partial     -> Partial
    //   Opaque      -> Partial     -> Opaque
    //   Partial     -> Opaque      -> Opaque
    //   Partial     -> Partial     -> Opaque
    //   Partial     -> Opaque      -> Partial
    //
    // Possible triplets
    //   Opaque      -> Opaque      -> Opaque       (default)
    //   Partial     -> Partial     -> Partial      (default)
    //   Opaque      -> Opaque      -> Transparent  (default)
    //   Opaque      -> Partial     -> Transparent  (default)
    //   Partial     -> Partial     -> Transparent  (default)
    //   Partial     -> Opaque      -> Transparent  (default)
    //   Transparent -> Opaque      -> Transparent  (default)
    //   Transparent -> Partial     -> Transparent  (default)
    //   Transparent -> Transparent -> Transparent  (default)
    //   Opaque      -> Transparent -> Opaque       (splits, steals previous + next)
    //   Opaque      -> Transparent -> Partial      (splits, steals previous + next)
    //   Partial     -> Transparent -> Partial      (splits, steals previous + next)
    //   Partial     -> Transparent -> Opaque       (splits, steals previous + next)
    //   Transparent -> Transparent -> Opaque       (steals next)
    //   Transparent -> Transparent -> Partial      (steals next)
    //   Opaque      -> Transparent -> Transparent  (steals previous)
    //   Partial     -> Transparent -> Transparent  (steals previous)

    size_t previousStopIndex = 0;
    size_t stopIndex = 1;
    size_t nextStopIndex = 2;

    for (; nextStopIndex < stops.size(); ++previousStopIndex, ++stopIndex, ++nextStopIndex) {
        auto& previousStop = stops.stops()[previousStopIndex];
        auto& stop = stops.stops()[stopIndex];
        auto& nextStop = stops.stops()[nextStopIndex];

        auto previousStopAlphaType = classifyAlphaType(previousStop.color.alphaAsFloat());
        auto stopAlphaType = classifyAlphaType(stop.color.alphaAsFloat());
        auto nextStopAlphaType = classifyAlphaType(nextStop.color.alphaAsFloat());

        if (stopAlphaType == AlphaType::FullyTransparent) {
            switch (previousStopAlphaType) {
            case AlphaType::Opaque:
            case AlphaType::PartiallyTransparent:
                switch (nextStopAlphaType) {
                case AlphaType::Opaque:
                case AlphaType::PartiallyTransparent:
                    // ACTION: Split.
                    result.append({ stop.offset, previousStop.color.colorWithAlpha(0.0f) });
                    result.append({ stop.offset, nextStop.color.colorWithAlpha(0.0f) });
                    break;

                case AlphaType::FullyTransparent:
                    // ACTION: Steal previous.
                    result.append({ stop.offset, previousStop.color.colorWithAlpha(0.0f) });
                    break;
                }
                break;

            case AlphaType::FullyTransparent:
                switch (nextStopAlphaType) {
                case AlphaType::Opaque:
                case AlphaType::PartiallyTransparent:
                    // ACTION: Steal next.
                    result.append({ stop.offset, nextStop.color.colorWithAlpha(0.0f) });
                    break;

                case AlphaType::FullyTransparent:
                    // ACTION: Default.
                    result.append(stop);
                    break;
                }
                break;
            }
        } else {
            // ACTION: Default.
            result.append(stop);
        }
    }

    ASSERT(nextStopIndex == stops.size());

    // 3. Handle last stop.
    //
    // [last stop matters]
    //
    // Illegal pairs (ruled out in analysis phase)
    //   Opaque  -> Partial
    //   Partial -> Opaque
    //
    // Possible pairs
    //   Opaque      -> Opaque       (default)
    //   Partial     -> Partial      (default)
    //   Transparent -> Transparent  (default)
    //   Opaque      -> Transparent  (steals previous)
    //   Partial     -> Transparent  (steals previous)
    //   Transparent -> Opaque       (default)
    //   Transparent -> Partial      (default)

    auto& secondToLastStop = stops.stops()[previousStopIndex];
    auto& lastStop = stops.stops()[stopIndex];

    auto secondToLastStopAlphaType = classifyAlphaType(secondToLastStop.color.alphaAsFloat());
    auto lastStopAlphaType = classifyAlphaType(lastStop.color.alphaAsFloat());

    if (lastStopAlphaType == AlphaType::FullyTransparent && secondToLastStopAlphaType != AlphaType::FullyTransparent) {
        // ACTION: Steal previous.
        result.append({ lastStop.offset, secondToLastStop.color.colorWithAlpha(0.0f) });
    } else {
        // ACTION: Default.
        result.append(lastStop);
    }

    return GradientColorStops::Sorted { WTFMove(result) };
}
#endif

static bool anyComponentIsNone(const GradientColorStops& stops)
{
    for (auto& stop : stops) {
        if (stop.color.anyComponentIsNone())
            return true;
    }
    
    return false;
}

GradientRendererCG::Strategy GradientRendererCG::pickStrategy(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops) const
{
    return WTF::switchOn(colorInterpolationMethod.colorSpace,
        [&] (const ColorInterpolationMethod::SRGB&) -> Strategy {
            // FIXME: As an optimization we can precompute 'none' replacements and create a transformed stop list rather than falling back on CGShadingRef.
            if (anyComponentIsNone(stops))
                return makeShading(colorInterpolationMethod, stops);

            switch (colorInterpolationMethod.alphaPremultiplication) {
            case AlphaPremultiplication::Unpremultiplied:
                return makeGradient(colorInterpolationMethod, stops);
            case AlphaPremultiplication::Premultiplied:
#if HAVE(CORE_GRAPHICS_PREMULTIPLIED_INTERPOLATION_GRADIENT)
                return makeGradient(colorInterpolationMethod, stops);
#else
                switch (analyzeColorStopsForEmulatedAlphaPremultiplicationOpportunity(stops)) {
                case EmulatedAlphaPremultiplicationAnalysisResult::UseGradientAsIs:
                    return makeGradient({ ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied }, stops);
                case EmulatedAlphaPremultiplicationAnalysisResult::UseGradientWithAlphaTransforms:
                    return makeGradient({ ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied }, alphaTransformStopsToEmulateAlphaPremultiplication(stops));
                case EmulatedAlphaPremultiplicationAnalysisResult::UseShading:
                    return makeShading(colorInterpolationMethod, stops);
                }
#endif
            }
        },
        [&] (const auto&) -> Strategy {
            return makeShading(colorInterpolationMethod, stops);
        }
    );
}

// MARK: - Gradient strategy.

GradientRendererCG::Strategy GradientRendererCG::makeGradient(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops) const
{
    ASSERT_UNUSED(colorInterpolationMethod, std::holds_alternative<ColorInterpolationMethod::SRGB>(colorInterpolationMethod.colorSpace));
#if !HAVE(CORE_GRAPHICS_PREMULTIPLIED_INTERPOLATION_GRADIENT)
    ASSERT_UNUSED(colorInterpolationMethod, colorInterpolationMethod.alphaPremultiplication == AlphaPremultiplication::Unpremultiplied);
#endif

#if HAVE(CORE_GRAPHICS_GRADIENT_CREATE_WITH_OPTIONS)
#if HAVE(CORE_GRAPHICS_PREMULTIPLIED_INTERPOLATION_GRADIENT)
    auto gradientInterpolatesPremultipliedOptionsDictionary = [] () -> CFDictionaryRef {
        static CFTypeRef keys[] = { kCGGradientInterpolatesPremultiplied };
        static CFTypeRef values[] = { kCFBooleanTrue };
        static CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        return options;
    };

   auto gradientOptionsDictionary = [&] (auto colorInterpolationMethod) -> CFDictionaryRef {
        switch (colorInterpolationMethod.alphaPremultiplication) {
        case AlphaPremultiplication::Unpremultiplied:
            return nullptr;
        case AlphaPremultiplication::Premultiplied:
            return gradientInterpolatesPremultipliedOptionsDictionary();
        }
   };
#else
   auto gradientOptionsDictionary = [] (auto) -> CFDictionaryRef {
        return nullptr;
   };
#endif
#endif

    auto hasOnlyBoundedSRGBColorStops = [] (const auto& stops) {
        for (const auto& stop : stops) {
            if (stop.color.colorSpace() != ColorSpace::SRGB)
                return false;
        }
        return true;
    };

    auto numberOfStops = stops.size();

    static constexpr auto reservedStops = 3;
    Vector<CGFloat, reservedStops> locations;
    locations.reserveInitialCapacity(numberOfStops);

    Vector<CGFloat, 4 * reservedStops> colorComponents;
    colorComponents.reserveInitialCapacity(numberOfStops * 4);

    auto cgColorSpace = [&] {
        // FIXME: Now that we only ever use CGGradientCreateWithColorComponents, we should investigate
        // if there is any real benefit to using sRGB when all the stops are bounded vs just using
        // extended sRGB for all gradients.
        if (hasOnlyBoundedSRGBColorStops(stops)) {
            for (const auto& stop : stops) {
                auto [r, g, b, a] = stop.color.toColorTypeLossy<SRGBA<float>>().resolved();
                colorComponents.uncheckedAppend(r);
                colorComponents.uncheckedAppend(g);
                colorComponents.uncheckedAppend(b);
                colorComponents.uncheckedAppend(a);

                locations.uncheckedAppend(stop.offset);
            }
            return cachedCGColorSpace<ColorSpaceFor<SRGBA<float>>>();
        }

        using OutputSpaceColorType = std::conditional_t<HasCGColorSpaceMapping<ColorSpace::ExtendedSRGB>, ExtendedSRGBA<float>, SRGBA<float>>;
        for (const auto& stop : stops) {
            auto [r, g, b, a] = stop.color.toColorTypeLossy<OutputSpaceColorType>().resolved();
            colorComponents.uncheckedAppend(r);
            colorComponents.uncheckedAppend(g);
            colorComponents.uncheckedAppend(b);
            colorComponents.uncheckedAppend(a);

            locations.uncheckedAppend(stop.offset);
        }
        return cachedCGColorSpace<ColorSpaceFor<OutputSpaceColorType>>();
    }();

#if HAVE(CORE_GRAPHICS_GRADIENT_CREATE_WITH_OPTIONS)
    return Gradient { adoptCF(CGGradientCreateWithColorComponentsAndOptions(cgColorSpace, colorComponents.data(), locations.data(), numberOfStops, gradientOptionsDictionary(colorInterpolationMethod))) };
#else
    return Gradient { adoptCF(CGGradientCreateWithColorComponents(cgColorSpace, colorComponents.data(), locations.data(), numberOfStops)) };
#endif
}

// MARK: - Shading strategy.

template<typename InterpolationSpace, AlphaPremultiplication alphaPremultiplication>
void GradientRendererCG::Shading::shadingFunction(void* info, const CGFloat* in, CGFloat* out)
{
    using InterpolationSpaceColorType = typename InterpolationSpace::ColorType;
    using OutputSpaceColorType = std::conditional_t<HasCGColorSpaceMapping<ColorSpace::ExtendedSRGB>, ExtendedSRGBA<float>, SRGBA<float>>;

    auto* data = static_cast<GradientRendererCG::Shading::Data*>(info);

    // Compute color at offset 'in[0]' and assign the components to out[0 -> 3].

    float requestedOffset = in[0];

    // 1. Find stops that bound the requested offset.
    auto [stop0, stop1] = [&] {
        for (size_t stop = 1; stop < data->stops().size(); ++stop) {
            if (requestedOffset <= data->stops()[stop].offset)
                return std::tie(data->stops()[stop - 1], data->stops()[stop]);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    // 2. Compute percentage offset between the two stops.
    float offset = (stop1.offset == stop0.offset) ? 0.0f : (requestedOffset - stop0.offset) / (stop1.offset - stop0.offset);

    // 3. Interpolate the two stops' colors by the computed offset.
    // FIXME: We don't want to due hue fixup for each call, so we should figure out how to precompute that.
    auto interpolatedColor = interpolateColorComponents<alphaPremultiplication>(
        std::get<InterpolationSpace>(data->colorInterpolationMethod().colorSpace),
        makeFromComponents<InterpolationSpaceColorType>(stop0.colorComponents), 1.0f - offset,
        makeFromComponents<InterpolationSpaceColorType>(stop1.colorComponents), offset);

    // 4. Convert to the output color space.
    auto interpolatedColorConvertedToOutputSpace = asColorComponents(convertColor<OutputSpaceColorType>(interpolatedColor).resolved());

    // 5. Write color components to 'out' pointer.
    for (size_t componentIndex = 0; componentIndex < interpolatedColorConvertedToOutputSpace.size(); ++componentIndex)
        out[componentIndex] = interpolatedColorConvertedToOutputSpace[componentIndex];
}

GradientRendererCG::Strategy GradientRendererCG::makeShading(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops) const
{
    using OutputSpaceColorType = std::conditional_t<HasCGColorSpaceMapping<ColorSpace::ExtendedSRGB>, ExtendedSRGBA<float>, SRGBA<float>>;

    auto makeData = [&] (auto colorInterpolationMethod, auto& stops) {
        auto convertColorToColorInterpolationSpace = [&] (const Color& color, auto colorInterpolationMethod) -> ColorComponents<float, 4> {
            return WTF::switchOn(colorInterpolationMethod.colorSpace,
                [&] (auto& colorSpace) -> ColorComponents<float, 4> {
                    using ColorType = typename std::remove_reference_t<decltype(colorSpace)>::ColorType;
                    return asColorComponents(color.template toColorTypeLossy<ColorType>().unresolved());
                }
            );
        };

        auto totalNumberOfStops = stops.size();
        bool hasZero = false;
        bool hasOne = false;

        for (const auto& stop : stops) {
            auto offset = stop.offset;
            ASSERT(offset >= 0);
            ASSERT(offset <= 1);
            
            if (offset == 0)
                hasZero = true;
            else if (offset == 1)
                hasOne = true;
        }

        if (!hasZero)
            totalNumberOfStops++;
        if (!hasOne)
            totalNumberOfStops++;

        // FIXME: To avoid duplicate work in the shader function, we could precompute a few things:
        //   - If we have a polar coordinate color space, we can pre-fixup the hues, inserting an extra stop at the same offset if both the fixup on the left and right require different results.
        //   - If we have 'none' components, we can precompute 'none' replacements, inserting an extra stop at the same offset if the replacements on the left and right are different.

        Vector<ColorConvertedToInterpolationColorSpaceStop> convertedStops;
        convertedStops.reserveInitialCapacity(totalNumberOfStops);

        if (!hasZero)
            convertedStops.uncheckedAppend({ 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } });

        for (const auto& stop : stops)
            convertedStops.uncheckedAppend({ stop.offset, convertColorToColorInterpolationSpace(stop.color, colorInterpolationMethod) });

        if (!hasOne)
            convertedStops.uncheckedAppend({ 1.0f, convertedStops.last().colorComponents });

        if (!hasZero)
            convertedStops[0].colorComponents = convertedStops[1].colorComponents;

        return Shading::Data::create(colorInterpolationMethod, WTFMove(convertedStops));
    };

    auto makeFunction = [&] (auto colorInterpolationMethod, auto& data) {
        auto makeEvaluateCallback = [&] (auto colorInterpolationMethod) -> CGFunctionEvaluateCallback {
            return WTF::switchOn(colorInterpolationMethod.colorSpace,
                [&] (auto& colorSpace) -> CGFunctionEvaluateCallback {
                    using InterpolationMethodColorSpace = typename std::remove_reference_t<decltype(colorSpace)>;
                    
                    switch (colorInterpolationMethod.alphaPremultiplication) {
                    case AlphaPremultiplication::Unpremultiplied:
                        return &Shading::shadingFunction<InterpolationMethodColorSpace, AlphaPremultiplication::Unpremultiplied>;
                    case AlphaPremultiplication::Premultiplied:
                        return &Shading::shadingFunction<InterpolationMethodColorSpace, AlphaPremultiplication::Premultiplied>;
                    }
                }
            );
        };

        const CGFunctionCallbacks callbacks = {
            0,
            makeEvaluateCallback(colorInterpolationMethod),
            [] (void* info) {
                static_cast<GradientRendererCG::Shading::Data*>(info)->deref();
            }
        };

        constexpr auto outputSpaceComponentInfo = OutputSpaceColorType::Model::componentInfo;

        static constexpr std::array<CGFloat, 2> domain = { 0, 1 };
        static constexpr std::array<CGFloat, 8> range = {
            outputSpaceComponentInfo[0].min, outputSpaceComponentInfo[0].max,
            outputSpaceComponentInfo[1].min, outputSpaceComponentInfo[1].max,
            outputSpaceComponentInfo[2].min, outputSpaceComponentInfo[2].max,
            0, 1
        };

        Ref dataRefCopy = data;
        return adoptCF(CGFunctionCreate(&dataRefCopy.leakRef(), domain.size() / 2, domain.data(), range.size() / 2, range.data(), &callbacks));
    };

    auto data = makeData(colorInterpolationMethod, stops);
    auto function = makeFunction(colorInterpolationMethod, data);

    // FIXME: Investigate using bounded sRGB when the input stops are all bounded sRGB.
    auto colorSpace = cachedCGColorSpace<ColorSpaceFor<OutputSpaceColorType>>();

    return Shading { WTFMove(data), WTFMove(function), colorSpace };
}

// MARK: - Drawing functions.

void GradientRendererCG::drawLinearGradient(CGContextRef platformContext, CGPoint startPoint, CGPoint endPoint, CGGradientDrawingOptions options)
{
    WTF::switchOn(m_strategy,
        [&] (Gradient& gradient) {
            CGContextDrawLinearGradient(platformContext, gradient.gradient.get(), startPoint, endPoint, options);
        },
        [&] (Shading& shading) {
            bool startExtend = (options & kCGGradientDrawsBeforeStartLocation) != 0;
            bool endExtend = (options & kCGGradientDrawsAfterEndLocation) != 0;

            CGContextDrawShading(platformContext, adoptCF(CGShadingCreateAxial(shading.colorSpace.get(), startPoint, endPoint, shading.function.get(), startExtend, endExtend)).get());
        }
    );
}

void GradientRendererCG::drawRadialGradient(CGContextRef platformContext, CGPoint startCenter, CGFloat startRadius, CGPoint endCenter, CGFloat endRadius, CGGradientDrawingOptions options)
{
    WTF::switchOn(m_strategy,
        [&] (Gradient& gradient) {
            CGContextDrawRadialGradient(platformContext, gradient.gradient.get(), startCenter, startRadius, endCenter, endRadius, options);
        },
        [&] (Shading& shading) {
            bool startExtend = (options & kCGGradientDrawsBeforeStartLocation) != 0;
            bool endExtend = (options & kCGGradientDrawsAfterEndLocation) != 0;

            CGContextDrawShading(platformContext, adoptCF(CGShadingCreateRadial(shading.colorSpace.get(), startCenter, startRadius, endCenter, endRadius, shading.function.get(), startExtend, endExtend)).get());
        }
    );
}

void GradientRendererCG::drawConicGradient(CGContextRef platformContext, CGPoint center, CGFloat angle)
{
#if HAVE(CORE_GRAPHICS_CONIC_GRADIENTS)
    WTF::switchOn(m_strategy,
        [&] (Gradient& gradient) {
            CGContextDrawConicGradient(platformContext, gradient.gradient.get(), center, angle);
        },
        [&] (Shading& shading) {
            CGContextDrawShading(platformContext, adoptCF(CGShadingCreateConic(shading.colorSpace.get(), center, angle, shading.function.get())).get());
        }
    );
#else
    UNUSED_PARAM(platformContext);
    UNUSED_PARAM(center);
    UNUSED_PARAM(angle);
#endif
}

}
