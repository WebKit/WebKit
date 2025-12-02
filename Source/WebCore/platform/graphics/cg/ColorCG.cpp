/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Color.h"

#if USE(CG)

#include "ColorSpaceCG.h"
#include "DestinationColorSpace.h"
#include <mutex>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/Assertions.h>
#include <wtf/Lock.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TinyLRUCache.h>

namespace WebCore {
static RetainPtr<CGColorRef> createCGColor(const Color&);
static RetainPtr<CGColorRef> createSDRCGColorForColorspace(const Color&, const DestinationColorSpace&);
}

namespace WTF {

template<>
RetainPtr<CGColorRef> TinyLRUCachePolicy<WebCore::Color, RetainPtr<CGColorRef>>::createValueForKey(const WebCore::Color& color)
{
    return WebCore::createCGColor(color);
}

template<>
RetainPtr<CGColorRef> TinyLRUCachePolicy<std::pair<WebCore::Color, std::optional<WebCore::DestinationColorSpace>>, RetainPtr<CGColorRef>>::createValueForKey(const std::pair<WebCore::Color, std::optional<WebCore::DestinationColorSpace>>& colorAndColorSpace)
{
    return WebCore::createSDRCGColorForColorspace(colorAndColorSpace.first, *colorAndColorSpace.second);
}

} // namespace WTF

namespace WebCore {

static std::span<const CGFloat> componentsSpan(CGColorRef color)
{
    return unsafeMakeSpan(CGColorGetComponents(color), CGColorGetNumberOfComponents(color));
}

std::optional<SRGBA<uint8_t>> roundAndClampToSRGBALossy(CGColorRef color)
{
    // FIXME: Interpreting components of a color in an arbitrary color space
    // as sRGB could be wrong, not just lossy.

    if (!color)
        return std::nullopt;

    auto components = componentsSpan(color);

    float r = 0;
    float g = 0;
    float b = 0;
    float a = 0;

    switch (components.size()) {
    case 2:
        r = g = b = components[0];
        a = components[1];
        break;
    case 4:
        r = components[0];
        g = components[1];
        b = components[2];
        a = components[3];
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return convertColor<SRGBA<uint8_t>>(makeFromComponentsClamping<SRGBA<float>>(r, g, b, a ));
}

template<ColorSpace space>
static CGColorTransformRef cachedCGColorTransform()
{
    static NeverDestroyed<RetainPtr<CGColorTransformRef>> transform = adoptCF(CGColorTransformCreate(cachedCGColorSpaceSingleton<space>(), nullptr));
    return transform->get();
}

Color Color::createAndLosslesslyConvertToSupportedColorSpace(CGColorRef color, OptionSet<Flags> flags)
{
    // FIXME: This should probably use ExtendedSRGBA rather than XYZ_D50, as it is a more commonly used color space and just as expressive.
    constexpr auto destinationColorSpace = HasCGColorSpaceMapping<ColorSpace::XYZ_D50> ? ColorSpace::XYZ_D50 : ColorSpace::SRGB;
    ASSERT(CGColorSpaceGetNumberOfComponents(cachedCGColorSpaceSingleton<destinationColorSpace>()) == 3);

    RetainPtr sourceCGColorSpace = CGColorGetColorSpace(color);
    auto sourceComponents = CGColorGetComponents(color);
    std::array<CGFloat, 3> destinationComponents { };

    auto result = CGColorTransformConvertColorComponents(cachedCGColorTransform<destinationColorSpace>(), sourceCGColorSpace.get(), kCGRenderingIntentDefault, sourceComponents, destinationComponents.data());
    ASSERT_UNUSED(result, result);

    float a = destinationComponents[0];
    float b = destinationComponents[1];
    float c = destinationComponents[2];
    float alpha = CGColorGetAlpha(color);

    return Color(OutOfLineComponents::create({ a, b, c, alpha }), destinationColorSpace, flags);
}

Color Color::createAndPreserveColorSpace(CGColorRef color, OptionSet<Flags> flags)
{
    if (!color)
        return Color();

    auto components = componentsSpan(color);
    auto colorSpace = colorSpaceForCGColorSpace(RetainPtr { CGColorGetColorSpace(color) }.get());

    if (components.size() != 4 || !colorSpace)
        return createAndLosslesslyConvertToSupportedColorSpace(color, flags);

    float a = components[0];
    float b = components[1];
    float c = components[2];
    float alpha = components[3];

    return Color(OutOfLineComponents::create({ a, b, c, alpha }), *colorSpace, flags);
}

static std::pair<CGColorSpaceRef, ColorComponents<float, 4>> convertToCGCompatibleComponents(ColorSpace colorSpace, ColorComponents<float, 4> components)
{
    // Some CG ports don't support all the color spaces required and return
    // nullptr for unsupported color spaces. In those cases, we eagerly convert
    // the color into either extended sRGB or normal sRGB, if extended sRGB is
    // not supported.

    using FallbackColorType = std::conditional_t<HasCGColorSpaceMapping<ColorSpace::ExtendedSRGB>, ExtendedSRGBA<float>, SRGBA<float>>;

    if (RetainPtr cgColorSpace = cachedNullableCGColorSpaceSingleton(colorSpace))
        return { cgColorSpace.get(), components };

    auto componentsConvertedToFallbackColorSpace = callWithColorType(components, colorSpace, [] (const auto& color) {
        return asColorComponents(convertColor<FallbackColorType>(color).resolved());
    });
    return { cachedCGColorSpaceSingleton<ColorSpaceFor<FallbackColorType>>(), componentsConvertedToFallbackColorSpace };
}

static RetainPtr<CGColorRef> createCGColor(const Color& color)
{
    auto [colorSpace, components] = color.colorSpaceAndResolvedColorComponents();
    auto [cgColorSpace, cgCompatibleComponents] = convertToCGCompatibleComponents(colorSpace, components);
    
    auto [c1, c2, c3, c4] = cgCompatibleComponents;
    std::array<CGFloat, 4> cgFloatComponents { c1, c2, c3, c4 };

    return adoptCF(CGColorCreate(cgColorSpace, cgFloatComponents.data()));
}

RetainPtr<CGColorRef> cachedCGColor(const Color& color)
{
    if (auto srgb = color.tryGetAsSRGBABytes()) {
        switch (PackedColor::RGBA { *srgb }.value) {
        case PackedColor::RGBA { Color::transparentBlack }.value: {
            static NeverDestroyed<RetainPtr<CGColorRef>> transparentCGColor = createCGColor(Color::transparentBlack);
            return transparentCGColor.get();
        }
        case PackedColor::RGBA { Color::black }.value: {
            static NeverDestroyed<RetainPtr<CGColorRef>> blackCGColor = createCGColor(Color::black);
            return blackCGColor.get();
        }
        case PackedColor::RGBA { Color::white }.value: {
            static NeverDestroyed<RetainPtr<CGColorRef>> whiteCGColor = createCGColor(Color::white);
            return whiteCGColor.get();
        }
        }
    }

    static Lock cachedColorLock;
    Locker locker { cachedColorLock };

    static NeverDestroyed<TinyLRUCache<Color, RetainPtr<CGColorRef>, 32>> cache;
    return cache.get().get(color);
}

RetainPtr<CGColorRef> createSDRCGColorForColorspace(const Color& color, const DestinationColorSpace& colorSpace)
{
    RetainPtr result = cachedCGColor(color);
    RetainPtr standardRangeColorSpace = adoptCF(CGColorSpaceCreateCopyWithStandardRange(colorSpace.protectedPlatformColorSpace().get()));
    return adoptCF(CGColorCreateCopyByMatchingToColorSpace(standardRangeColorSpace.get(), kCGRenderingIntentDefault, result.get(), nullptr));
}

RetainPtr<CGColorRef> cachedSDRCGColorForColorspace(const Color& color, const DestinationColorSpace& colorSpace)
{
    if (!colorSpace.usesExtendedRange() || color.tryGetAsSRGBABytes())
        return cachedCGColor(color);

    static Lock cachedSDRColorLock;
    Locker locker { cachedSDRColorLock };

    static NeverDestroyed<TinyLRUCache<std::pair<Color, std::optional<DestinationColorSpace>>, RetainPtr<CGColorRef>, 32>> cache;
    return cache.get().get(std::make_pair(color, colorSpace));
}

ColorComponents<float, 4> platformConvertColorComponents(ColorSpace inputColorSpace, ColorComponents<float, 4> inputColorComponents, const DestinationColorSpace& outputColorSpace)
{
    // FIXME: Investigate optimizing this to use the builtin color conversion code for supported color spaces.

    auto [cgInputColorSpace, cgCompatibleComponents] = convertToCGCompatibleComponents(inputColorSpace, inputColorComponents);
    if (cgInputColorSpace == outputColorSpace.platformColorSpace())
        return cgCompatibleComponents;

    auto [c1, c2, c3, c4] = cgCompatibleComponents;
    std::array<CGFloat, 4> sourceComponents { c1, c2, c3, c4 };
    std::array<CGFloat, 4> destinationComponents { };

    auto transform = adoptCF(CGColorTransformCreate(outputColorSpace.protectedPlatformColorSpace().get(), nullptr));
    auto result = CGColorTransformConvertColorComponents(transform.get(), cgInputColorSpace, kCGRenderingIntentDefault, sourceComponents.data(), destinationComponents.data());
    ASSERT_UNUSED(result, result);
    // CGColorTransformConvertColorComponents doesn't copy over any alpha component.
    return { static_cast<float>(destinationComponents[0]), static_cast<float>(destinationComponents[1]), static_cast<float>(destinationComponents[2]), static_cast<float>(sourceComponents[3]) };
}

}

#endif // USE(CG)
