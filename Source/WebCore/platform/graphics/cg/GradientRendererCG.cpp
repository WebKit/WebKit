/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include "ColorHash.h"
#include "ColorInterpolation.h"
#include "ColorSpaceCG.h"
#include "GradientColorStops.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/HashMap.h>

namespace WebCore {

GradientRendererCG::GradientRendererCG(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops, std::optional<DestinationColorSpace> destinationColorSpace)
    : m_strategy { pickStrategy(colorInterpolationMethod, stops, destinationColorSpace) }
{
}

std::optional<DestinationColorSpace> GradientRendererCG::colorSpace() const
{
    if (auto* gradient = std::get_if<Gradient>(&m_strategy))
        return gradient->colorSpace;

    return { };
}

// MARK: - Strategy selection.

static bool anyComponentIsNone(const GradientColorStops& stops)
{
    for (auto& stop : stops) {
        if (stop.color.anyComponentIsNone())
            return true;
    }
    
    return false;
}

GradientRendererCG::Strategy GradientRendererCG::pickStrategy(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops, std::optional<DestinationColorSpace> destinationColorSpace) const
{
    return WTF::switchOn(colorInterpolationMethod.colorSpace,
        [&] (const ColorInterpolationMethod::SRGB&) -> Strategy {
            // FIXME: As an optimization we can precompute 'none' replacements and create a transformed stop list rather than falling back on CGShadingRef.
            if (anyComponentIsNone(stops))
                return makeShading(colorInterpolationMethod, stops);

            return makeGradient(colorInterpolationMethod, stops, destinationColorSpace);
        },
        [&] (const auto&) -> Strategy {
            return makeShading(colorInterpolationMethod, stops);
        }
    );
}

// MARK: - Gradient strategy.

static ColorComponents<float, 4> getResolvedColorComponentsInColorSpace(const Color& color, const DestinationColorSpace& destinationColorSpace)
{
    static LazyNeverDestroyed<HashMap<Color, ColorComponents<float, 4>>> colorCache;
    static LazyNeverDestroyed<DestinationColorSpace> cacheColorSpace;
    static Lock colorCacheLock;

    auto locker = Locker(colorCacheLock);

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        colorCache.construct();
        cacheColorSpace.construct(destinationColorSpace);
    });

    if (cacheColorSpace != destinationColorSpace) {
        colorCache->clear();
        cacheColorSpace.get() = destinationColorSpace;
    }

    auto result = colorCache->ensure(color, [&] {
        return color.toResolvedColorComponentsInColorSpace(destinationColorSpace);
    }).iterator->value;

    const size_t maxCacheCount = 32;
    if (colorCache->size() > maxCacheCount) {
        auto iteratorToRemove = colorCache->random();
        colorCache->remove(iteratorToRemove);
    }

    return result;
}

GradientRendererCG::Strategy GradientRendererCG::makeGradient(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops, std::optional<DestinationColorSpace> destinationColorSpace) const
{
    ASSERT_UNUSED(colorInterpolationMethod, std::holds_alternative<ColorInterpolationMethod::SRGB>(colorInterpolationMethod.colorSpace));

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
                if (destinationColorSpace) {
                    auto [r, g, b, a ] = getResolvedColorComponentsInColorSpace(stop.color, *destinationColorSpace);
                    colorComponents.appendList({ r, g, b, a });
                } else {
                    auto [r, g, b, a] = stop.color.toColorTypeLossy<SRGBA<float>>().resolved();
                    colorComponents.appendList({ r, g, b, a });
                }

                locations.append(stop.offset);
            }

            if (destinationColorSpace)
                return destinationColorSpace->platformColorSpace();

            return cachedCGColorSpace<ColorSpaceFor<SRGBA<float>>>();
        }

        using OutputSpaceColorType = std::conditional_t<HasCGColorSpaceMapping<ColorSpace::ExtendedSRGB>, ExtendedSRGBA<float>, SRGBA<float>>;
        for (const auto& stop : stops) {
            auto [r, g, b, a] = stop.color.toColorTypeLossy<OutputSpaceColorType>().resolved();
            colorComponents.appendList({ r, g, b, a });

            locations.append(stop.offset);
        }
        return cachedCGColorSpace<ColorSpaceFor<OutputSpaceColorType>>();
    }();

    // CoreGraphics has a bug where if the last two stops are at 1, it fails to extend the last stop's color. This can be visible in radial gradients.
    auto apply139572277Workaround = [&]() {
        if (numberOfStops < 2)
            return;

        if (locations[numberOfStops - 2] == 1.0 && locations[numberOfStops - 1] == 1.0) {
            // Replicate the last color stop.
            locations.append(1.0);

            auto lastColorComponentIndex = 4 * (numberOfStops - 1);

            colorComponents.reserveCapacity((numberOfStops + 1) * 4);
            colorComponents.append(colorComponents[lastColorComponentIndex]);
            colorComponents.append(colorComponents[lastColorComponentIndex + 1]);
            colorComponents.append(colorComponents[lastColorComponentIndex + 2]);
            colorComponents.append(colorComponents[lastColorComponentIndex + 3]);

            ++numberOfStops;
        }
    };

    apply139572277Workaround();

    return Gradient { adoptCF(CGGradientCreateWithColorComponentsAndOptions(cgColorSpace, colorComponents.span().data(), locations.span().data(), numberOfStops, gradientOptionsDictionary(colorInterpolationMethod))), destinationColorSpace };
}

// MARK: - Shading strategy.

template<typename InterpolationSpace, AlphaPremultiplication alphaPremultiplication>
void GradientRendererCG::Shading::shadingFunction(void* info, const CGFloat* rawIn, CGFloat* rawOut)
{
    using InterpolationSpaceColorType = typename InterpolationSpace::ColorType;
    using OutputSpaceColorType = std::conditional_t<HasCGColorSpaceMapping<ColorSpace::ExtendedSRGB>, ExtendedSRGBA<float>, SRGBA<float>>;

    auto* data = static_cast<GradientRendererCG::Shading::Data*>(info);

    // Compute color at offset 'in[0]' and assign the components to out[0 -> 3].
    auto in = unsafeMakeSpan(rawIn, 1);
    auto out = unsafeMakeSpan(rawOut, 4);

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
    // Synthetic color stops are added to extend the author-provided gradient out to 0 and 1
    // with a solid color, if necessary. These need special handling because `longer hue` gradients
    // would otherwise rotate through 360° of hue in these segments.
    auto interpolatedColor = [&]() {
        if (stop0.offset == 0.0f && data->firstStopIsSynthetic())
            return makeFromComponents<InterpolationSpaceColorType>(stop0.colorComponents);

        if (stop1.offset == 1.0f && data->lastStopIsSynthetic())
            return makeFromComponents<InterpolationSpaceColorType>(stop1.colorComponents);

        return interpolateColorComponents<alphaPremultiplication>(
            std::get<InterpolationSpace>(data->colorInterpolationMethod().colorSpace),
            makeFromComponents<InterpolationSpaceColorType>(stop0.colorComponents), 1.0f - offset,
            makeFromComponents<InterpolationSpaceColorType>(stop1.colorComponents), offset);
    }();

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
                [&]<typename MethodColorSpace>(const MethodColorSpace&) -> ColorComponents<float, 4> {
                    using ColorType = typename MethodColorSpace::ColorType;
                    return asColorComponents(color.template toColorTypeLossyCarryingForwardMissing<ColorType>().unresolved());
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
            convertedStops.append({ 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } });

        convertedStops.appendContainerWithMapping(stops, [&](auto& stop) {
            return ColorConvertedToInterpolationColorSpaceStop { stop.offset, convertColorToColorInterpolationSpace(stop.color, colorInterpolationMethod) };
        });

        if (!hasOne)
            convertedStops.append({ 1.0f, convertedStops.last().colorComponents });

        if (!hasZero)
            convertedStops[0].colorComponents = convertedStops[1].colorComponents;

        return Shading::Data::create(colorInterpolationMethod, WTFMove(convertedStops), !hasZero, !hasOne);
    };

    auto makeFunction = [&] (auto colorInterpolationMethod, auto& data) {
        auto makeEvaluateCallback = [&] (auto colorInterpolationMethod) -> CGFunctionEvaluateCallback {
            return WTF::switchOn(colorInterpolationMethod.colorSpace,
                [&]<typename MethodColorSpace> (const MethodColorSpace&) -> CGFunctionEvaluateCallback {
                    switch (colorInterpolationMethod.alphaPremultiplication) {
                    case AlphaPremultiplication::Unpremultiplied:
                        return &Shading::shadingFunction<MethodColorSpace, AlphaPremultiplication::Unpremultiplied>;
                    case AlphaPremultiplication::Premultiplied:
                        return &Shading::shadingFunction<MethodColorSpace, AlphaPremultiplication::Premultiplied>;
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
    WTF::switchOn(m_strategy,
        [&] (Gradient& gradient) {
            CGContextDrawConicGradient(platformContext, gradient.gradient.get(), center, angle);
        },
        [&] (Shading& shading) {
            CGContextDrawShading(platformContext, adoptCF(CGShadingCreateConic(shading.colorSpace.get(), center, angle, shading.function.get())).get());
        }
    );
}

}
