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
}

namespace WTF {

template<>
RetainPtr<CGColorRef> TinyLRUCachePolicy<WebCore::Color, RetainPtr<CGColorRef>>::createValueForKey(const WebCore::Color& color)
{
    return WebCore::createCGColor(color);
}

} // namespace WTF

namespace WebCore {

std::optional<SRGBA<uint8_t>> roundAndClampToSRGBALossy(CGColorRef color)
{
    // FIXME: Interpreting components of a color in an arbitrary color space
    // as sRGB could be wrong, not just lossy.

    if (!color)
        return std::nullopt;

    size_t numComponents = CGColorGetNumberOfComponents(color);
    const CGFloat* components = CGColorGetComponents(color);

    float r = 0;
    float g = 0;
    float b = 0;
    float a = 0;

    switch (numComponents) {
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

Color Color::createAndLosslesslyConvertToSupportedColorSpace(CGColorRef color, OptionSet<Flags> flags)
{
    auto sourceCGColorSpace = CGColorGetColorSpace(color);
#if HAVE(CORE_GRAPHICS_XYZ_COLOR_SPACE)
    auto destinationCGColorSpace = xyzColorSpaceRef();
    auto destinationColorSpace = ColorSpace::XYZ_D50;
#else
    auto destinationCGColorSpace = sRGBColorSpaceRef();
    auto destinationColorSpace = ColorSpace::SRGB;
#endif
    ASSERT(CGColorSpaceGetNumberOfComponents(destinationCGColorSpace) == 3);

    auto sourceComponents = CGColorGetComponents(color);
    CGFloat destinationComponents[3] { };

    auto transform = adoptCF(CGColorTransformCreate(destinationCGColorSpace, nullptr));
    auto result = CGColorTransformConvertColorComponents(transform.get(), sourceCGColorSpace, kCGRenderingIntentDefault, sourceComponents, destinationComponents);
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

    size_t numComponents = CGColorGetNumberOfComponents(color);
    auto colorSpace = colorSpaceForCGColorSpace(CGColorGetColorSpace(color));

    if (numComponents != 4 || !colorSpace)
        return createAndLosslesslyConvertToSupportedColorSpace(color, flags);

    const CGFloat* components = CGColorGetComponents(color);

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

    auto cgColorSpace = cachedNullableCGColorSpace(colorSpace);
    if (!cgColorSpace) {
#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
        auto componentsConvertedToExtendedSRGBA = callWithColorType(components, colorSpace, [] (const auto& color) {
            return asColorComponents(convertColor<ExtendedSRGBA<float>>(color));
        });
        return { extendedSRGBColorSpaceRef(), componentsConvertedToExtendedSRGBA };
#else
        auto componentsConvertedToSRGBA = callWithColorType(components, colorSpace, [] (const auto& color) {
            return asColorComponents(convertColor<SRGBA<float>>(color));
        });
        return { sRGBColorSpaceRef(), componentsConvertedToSRGBA };
#endif
    }

    return { cgColorSpace, components };
}

static RetainPtr<CGColorRef> createCGColor(const Color& color)
{
    auto [colorSpace, components] = color.colorSpaceAndComponents();
    auto [cgColorSpace, cgCompatibleComponents] = convertToCGCompatibleComponents(colorSpace, components);
    
    auto [c1, c2, c3, c4] = cgCompatibleComponents;
    CGFloat cgFloatComponents[4] { c1, c2, c3, c4 };

    return adoptCF(CGColorCreate(cgColorSpace, cgFloatComponents));
}

CGColorRef cachedCGColor(const Color& color)
{
    if (auto srgb = color.tryGetAsSRGBABytes()) {
        switch (PackedColor::RGBA { *srgb }.value) {
        case PackedColor::RGBA { Color::transparentBlack }.value: {
            static LazyNeverDestroyed<RetainPtr<CGColorRef>> transparentCGColor;
            static std::once_flag onceFlag;
            std::call_once(onceFlag, [] {
                transparentCGColor.construct(createCGColor(Color::transparentBlack));
            });
            return transparentCGColor.get().get();
        }
        case PackedColor::RGBA { Color::black }.value: {
            static LazyNeverDestroyed<RetainPtr<CGColorRef>> blackCGColor;
            static std::once_flag onceFlag;
            std::call_once(onceFlag, [] {
                blackCGColor.construct(createCGColor(Color::black));
            });
            return blackCGColor.get().get();
        }
        case PackedColor::RGBA { Color::white }.value: {
            static LazyNeverDestroyed<RetainPtr<CGColorRef>> whiteCGColor;
            static std::once_flag onceFlag;
            std::call_once(onceFlag, [] {
                whiteCGColor.construct(createCGColor(Color::white));
            });
            return whiteCGColor.get().get();
        }
        }
    }

    static Lock cachedColorLock;
    Locker locker { cachedColorLock };

    static NeverDestroyed<TinyLRUCache<Color, RetainPtr<CGColorRef>, 32>> cache;
    return cache.get().get(color).get();
}

ColorComponents<float, 4> platformConvertColorComponents(ColorSpace inputColorSpace, ColorComponents<float, 4> inputColorComponents, const DestinationColorSpace& outputColorSpace)
{
    // FIXME: Investigate optimizing this to use the builtin color conversion code for supported color spaces.

    auto [cgInputColorSpace, cgCompatibleComponents] = convertToCGCompatibleComponents(inputColorSpace, inputColorComponents);
    if (cgInputColorSpace == outputColorSpace.platformColorSpace())
        return cgCompatibleComponents;

    auto [c1, c2, c3, c4] = cgCompatibleComponents;
    CGFloat sourceComponents[4] { c1, c2, c3, c4 };
    CGFloat destinationComponents[4] { };

    auto transform = adoptCF(CGColorTransformCreate(outputColorSpace.platformColorSpace(), nullptr));
    auto result = CGColorTransformConvertColorComponents(transform.get(), cgInputColorSpace, kCGRenderingIntentDefault, sourceComponents, destinationComponents);
    ASSERT_UNUSED(result, result);
    // FIXME: CGColorTransformConvertColorComponents doesn't copy over any alpha component.
    return { static_cast<float>(destinationComponents[0]), static_cast<float>(destinationComponents[1]), static_cast<float>(destinationComponents[2]), static_cast<float>(destinationComponents[3]) };
}

}

#endif // USE(CG)
