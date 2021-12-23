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

GradientRendererCG::Strategy GradientRendererCG::pickStrategy(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops) const
{
    return WTF::switchOn(colorInterpolationMethod.colorSpace,
        [&] (const ColorInterpolationMethod::SRGB&) -> Strategy {
            switch (colorInterpolationMethod.alphaPremultiplication) {
            case AlphaPremultiplication::Unpremultiplied:
                return makeGradient(colorInterpolationMethod, stops);
            case AlphaPremultiplication::Premultiplied:
#if HAVE(CORE_GRAPHICS_PREMULTIPLIED_INTERPOLATION_GRADIENT)
                return makeGradient(colorInterpolationMethod, stops);
#else
                // FIXME: Use gradient strategy if none of the stops have alpha.
                return makeShading(colorInterpolationMethod, stops);
#endif
            }
        },
        [&] (const auto&) -> Strategy {
            // FIXME: Add support for the other interpolation color spaces.
            RELEASE_ASSERT_NOT_REACHED();
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
        static CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

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

    auto gradientColorSpace = sRGBColorSpaceRef();

    // FIXME: Now that we only ever use CGGradientCreateWithColorComponents, we should investigate
    // if there is any real benefit to using sRGB when all the stops are bounded vs just using
    // extended sRGB for all gradients.
    if (hasOnlyBoundedSRGBColorStops(stops)) {
        for (const auto& stop : stops) {
            auto [colorSpace, components] = stop.color.colorSpaceAndComponents();
            auto [r, g, b, a] = components;
            colorComponents.uncheckedAppend(r);
            colorComponents.uncheckedAppend(g);
            colorComponents.uncheckedAppend(b);
            colorComponents.uncheckedAppend(a);

            locations.uncheckedAppend(stop.offset);
        }
    } else {
#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
        gradientColorSpace = extendedSRGBColorSpaceRef();
#endif

        for (const auto& stop : stops) {
#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
            auto [r, g, b, a] = stop.color.toColorTypeLossy<ExtendedSRGBA<float>>();
#else
            auto [r, g, b, a] = stop.color.toColorTypeLossy<SRGBA<float>>();
#endif
            colorComponents.uncheckedAppend(r);
            colorComponents.uncheckedAppend(g);
            colorComponents.uncheckedAppend(b);
            colorComponents.uncheckedAppend(a);

            locations.uncheckedAppend(stop.offset);
        }
    }

#if HAVE(CORE_GRAPHICS_GRADIENT_CREATE_WITH_OPTIONS)
    return Gradient { adoptCF(CGGradientCreateWithColorComponentsAndOptions(gradientColorSpace, colorComponents.data(), locations.data(), numberOfStops, gradientOptionsDictionary(colorInterpolationMethod))) };
#else
    return Gradient { adoptCF(CGGradientCreateWithColorComponents(gradientColorSpace, colorComponents.data(), locations.data(), numberOfStops)) };
#endif
}

// MARK: - Shading strategy.

template<typename InterpolationSpace, AlphaPremultiplication alphaPremultiplication>
void GradientRendererCG::Shading::shadingFunction(void* info, const CGFloat* in, CGFloat* out)
{
    using InterpolationSpaceColorType = typename InterpolationSpace::ColorType;
#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
    using OutputSpaceColorType = ExtendedSRGBA<float>;
#else
    using OutputSpaceColorType = SRGBA<float>;
#endif

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
    auto interpolatedColorConvertedToOutputSpace = asColorComponents(convertColor<OutputSpaceColorType>(interpolatedColor));

    // 5. Write color components to 'out' pointer.
    for (size_t componentIndex = 0; componentIndex < interpolatedColorConvertedToOutputSpace.size(); ++componentIndex)
        out[componentIndex] = interpolatedColorConvertedToOutputSpace[componentIndex];
}

GradientRendererCG::Strategy GradientRendererCG::makeShading(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops) const
{
    auto makeData = [&] (auto colorInterpolationMethod, auto& stops) {
        auto convertColorToColorInterpolationSpace = [&] (const Color& color, auto colorInterpolationMethod) -> ColorComponents<float, 4> {
            return WTF::switchOn(colorInterpolationMethod.colorSpace,
                [&] (auto& colorSpace) -> ColorComponents<float, 4> {
                    using ColorType = typename std::remove_reference_t<decltype(colorSpace)>::ColorType;
                    return asColorComponents(color.template toColorTypeLossy<ColorType>());
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

        static const CGFloat domain[2] = { 0, 1 };
#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
        static const CGFloat range[8] = { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), 0, 1 };
#else
        static const CGFloat range[8] = { 0, 1, 0, 1, 0, 1, 0, 1 };
#endif

        Ref dataRefCopy = data;
        return adoptCF(CGFunctionCreate(&dataRefCopy.leakRef(), 1, domain, 4, range, &callbacks));
    };

    auto data = makeData(colorInterpolationMethod, stops);
    auto function = makeFunction(colorInterpolationMethod, data);

    // FIXME: Investigate using bounded sRGB when the input stops are all bounded sRGB.
#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
    auto colorSpace = extendedSRGBColorSpaceRef();
#else
    auto colorSpace = sRGBColorSpaceRef();
#endif

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
// FIXME: Seems like this should be HAVE(CG_CONTEXT_DRAW_CONIC_GRADIENT).
// FIXME: Can we change tvOS to be like the other Cocoa platforms?
#if PLATFORM(COCOA) && !PLATFORM(APPLETV)
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
