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

#include "ColorConversion.h"
#include "ColorHash.h"
#include "ColorSpaceCG.h"
#include "DestinationColorSpace.h"
#include "GradientColorStops.h"
#include "SampledGradientBuilder.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/HashMap.h>
#include <wtf/TinyLRUCache.h>

namespace WTF {
using namespace WebCore;

struct SampledGradientCacheKey {
    ColorInterpolationMethod interpolationMethod;
    GradientColorStops::StopVector colorStops;
    std::optional<DestinationColorSpace> destinationColorSpace;

    friend bool operator==(const SampledGradientCacheKey&, const SampledGradientCacheKey&) = default;
};

template<>
bool TinyLRUCachePolicy<SampledGradientCacheKey, RetainPtr<CGGradientRef>>::isKeyNull(const SampledGradientCacheKey& key)
{
    return key.colorStops.isEmpty();
}

template<>
RetainPtr<CGGradientRef> TinyLRUCachePolicy<SampledGradientCacheKey, RetainPtr<CGGradientRef>>::createValueForKey(const SampledGradientCacheKey& params)
{
    return WebCore::GradientRendererCG::createGradientBySampling(params.interpolationMethod, params.colorStops, params.destinationColorSpace);
}

} // namespace WTF

namespace WebCore {

// MARK: - Constructor.

GradientRendererCG::GradientRendererCG(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops, std::optional<DestinationColorSpace> colorSpace)
    : m_colorSpace { WTF::move(colorSpace) }
    , m_gradient { makeGradient(colorInterpolationMethod, stops) }
{
}

// MARK: - Gradient options.

static CFDictionaryRef gradientInterpolatesPremultipliedOptionsDictionary()
{
    static CFTypeRef keys[] = { kCGGradientInterpolatesPremultiplied };
    static CFTypeRef values[] = { kCFBooleanTrue };
    static CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    return options;
}

static CFDictionaryRef gradientOptionsDictionary(ColorInterpolationMethod colorInterpolationMethod)
{
    switch (colorInterpolationMethod.alphaPremultiplication) {
    case AlphaPremultiplication::Unpremultiplied:
        return nullptr;
    case AlphaPremultiplication::Premultiplied:
        return gradientInterpolatesPremultipliedOptionsDictionary();
    }
}

// MARK: - Direct CGGradient strategy (sRGB only).

static bool anyComponentIsNone(const GradientColorStops& stops)
{
    for (auto& stop : stops) {
        if (stop.color.anyComponentIsNone())
            return true;
    }

    return false;
}

GradientRendererCG::Gradient GradientRendererCG::makeGradient(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops) const
{
    // For non-sRGB color spaces, or sRGB with 'none' components, fall back to sampling.
    bool needsSampling = WTF::switchOn(colorInterpolationMethod.colorSpace,
        [&] (const ColorInterpolationMethod::SRGB&) {
            // FIXME: As an optimization we can precompute 'none' replacements and create a transformed stop list rather than falling back on gradient sampling.
            return anyComponentIsNone(stops);
        },
        [&] (const auto&) {
            return true;
        }
    );

    if (needsSampling)
        return makeGradientBySampling(colorInterpolationMethod, stops);

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

    RetainPtr cgColorSpace = [&] () -> CGColorSpaceRef {
        if (m_colorSpace) {
            for (const auto& stop : stops) {
                auto components = stop.color.toResolvedColorComponentsInColorSpace(*m_colorSpace);
                colorComponents.appendList({ components[0], components[1], components[2], components[3] });
                locations.append(stop.offset);
            }
            return m_colorSpace->platformColorSpace();
        }

        // FIXME: Now that we only ever use CGGradientCreateWithColorComponents, we should investigate
        // if there is any real benefit to using sRGB when all the stops are bounded vs just using
        // extended sRGB for all gradients.
        if (hasOnlyBoundedSRGBColorStops(stops)) {
            for (const auto& stop : stops) {
                auto [r, g, b, a] = stop.color.toColorTypeLossy<SRGBA<float>>().resolved();
                colorComponents.appendList({ r, g, b, a });

                locations.append(stop.offset);
            }

            return cachedCGColorSpaceSingleton<ColorSpaceFor<SRGBA<float>>>();
        }

        using OutputSpaceColorType = std::conditional_t<HasCGColorSpaceMapping<ColorSpace::ExtendedSRGB>, ExtendedSRGBA<float>, SRGBA<float>>;
        for (const auto& stop : stops) {
            auto [r, g, b, a] = stop.color.toColorTypeLossy<OutputSpaceColorType>().resolved();
            colorComponents.appendList({ r, g, b, a });

            locations.append(stop.offset);
        }
        return cachedCGColorSpaceSingleton<ColorSpaceFor<OutputSpaceColorType>>();
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

    return Gradient { adoptCF(CGGradientCreateWithColorComponentsAndOptions(cgColorSpace.get(), colorComponents.span().data(), locations.span().data(), numberOfStops, gradientOptionsDictionary(colorInterpolationMethod))) };
}

// MARK: - Gradient-by-sampling strategy.

GradientRendererCG::Gradient GradientRendererCG::makeGradientBySampling(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops& stops) const
{
    auto colorStops = stops.sorted().stops();
    static NeverDestroyed<TinyLRUCache<WTF::SampledGradientCacheKey, RetainPtr<CGGradientRef>, 8>> cache;
    RetainPtr gradient = cache.get().get({ colorInterpolationMethod, colorStops, m_colorSpace });
    return Gradient { WTF::move(gradient) };
}

RetainPtr<CGGradientRef> GradientRendererCG::createGradientBySampling(ColorInterpolationMethod colorInterpolationMethod, const GradientColorStops::StopVector& stops, const std::optional<DestinationColorSpace>& destinationColorSpace)
{
    using OutputSpaceColorType = std::conditional_t<HasCGColorSpaceMapping<ColorSpace::ExtendedSRGB>, ExtendedSRGBA<float>, SRGBA<float>>;

    auto sampled = sampleGradientStops<OutputSpaceColorType>(colorInterpolationMethod, stops);

    Vector<CGFloat> locations(sampled.locations.size());
    Vector<CGFloat> components(sampled.colorComponents.size());
    for (size_t i = 0; i < sampled.locations.size(); ++i)
        locations[i] = sampled.locations[i];

    CGColorSpaceRef cgColorSpace;
    if (destinationColorSpace) {
        constexpr auto inputColorSpace = ColorSpaceFor<OutputSpaceColorType>;
        auto numberOfStops = sampled.locations.size();
        for (size_t i = 0; i < numberOfStops; ++i) {
            ColorComponents<float, 4> input {
                sampled.colorComponents[i * 4],
                sampled.colorComponents[i * 4 + 1],
                sampled.colorComponents[i * 4 + 2],
                sampled.colorComponents[i * 4 + 3]
            };
            auto converted = convertAndResolveColorComponents(inputColorSpace, input, *destinationColorSpace);
            components[i * 4] = converted[0];
            components[i * 4 + 1] = converted[1];
            components[i * 4 + 2] = converted[2];
            components[i * 4 + 3] = converted[3];
        }
        cgColorSpace = destinationColorSpace->platformColorSpace();
    } else {
        for (size_t i = 0; i < sampled.colorComponents.size(); ++i)
            components[i] = sampled.colorComponents[i];
        cgColorSpace = cachedCGColorSpaceSingleton<ColorSpaceFor<OutputSpaceColorType>>();
    }

    return adoptCF(CGGradientCreateWithColorComponentsAndOptions(cgColorSpace,
        components.span().data(), locations.span().data(), locations.size(), gradientOptionsDictionary(colorInterpolationMethod)));
}

// MARK: - Drawing functions.

void GradientRendererCG::drawLinearGradient(CGContextRef platformContext, CGPoint startPoint, CGPoint endPoint, CGGradientDrawingOptions options)
{
    CGContextDrawLinearGradient(platformContext, m_gradient.get(), startPoint, endPoint, options);
}

void GradientRendererCG::drawRadialGradient(CGContextRef platformContext, CGPoint startCenter, CGFloat startRadius, CGPoint endCenter, CGFloat endRadius, CGGradientDrawingOptions options)
{
    CGContextDrawRadialGradient(platformContext, m_gradient.get(), startCenter, startRadius, endCenter, endRadius, options);
}

void GradientRendererCG::drawConicGradient(CGContextRef platformContext, CGPoint center, CGFloat angle)
{
    CGContextDrawConicGradient(platformContext, m_gradient.get(), center, angle);
}

}
