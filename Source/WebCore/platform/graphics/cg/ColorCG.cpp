/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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
#include <mutex>
#include <wtf/Assertions.h>
#include <wtf/Lock.h>
#include <wtf/RetainPtr.h>
#include <wtf/TinyLRUCache.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/StdLibExtras.h>

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

static Optional<SRGBA<uint8_t>> roundAndClampToSRGBALossy(CGColorRef color)
{
    // FIXME: ExtendedColor - needs to handle color spaces.

    if (!color)
        return WTF::nullopt;

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

    return convertColor<SRGBA<uint8_t>>(SRGBA<float> { r, g, b, a });
}

Color::Color(CGColorRef color, OptionSet<Flags> flags)
    : Color(roundAndClampToSRGBALossy(color), flags)
{
}

static RetainPtr<CGColorRef> createCGColor(const Color& color)
{
    auto [colorSpace, components] = color.colorSpaceAndComponents();

    auto cgColorSpace = cachedNullableCGColorSpace(colorSpace);

    // Some CG ports don't support all the color spaces required and return
    // nullptr for unsupported color spaces. In those cases, we eagerly convert
    // the color into either extended sRGB or normal sRGB, if extended sRGB is
    // not supported, before creating the CGColorRef.
    if (!cgColorSpace) {
#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
        auto colorConvertedToExtendedSRGBA = callWithColorType(components, colorSpace, [] (const auto& color) {
            return convertColor<ExtendedSRGBA<float>>(color);
        });
        components = asColorComponents(colorConvertedToExtendedSRGBA);
        cgColorSpace = extendedSRGBColorSpaceRef();
#else
        auto colorConvertedToSRGBA = callWithColorType(components, colorSpace, [] (const auto& color) {
            return convertColor<SRGBA<float>>(color);
        });
        components = asColorComponents(colorConvertedToSRGBA);
        cgColorSpace = sRGBColorSpaceRef();
#endif
    }

    auto [r, g, b, a] = components;
    CGFloat cgFloatComponents[4] { r, g, b, a };

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
    auto holder = holdLock(cachedColorLock);

    static NeverDestroyed<TinyLRUCache<Color, RetainPtr<CGColorRef>, 32>> cache;
    return cache.get().get(color).get();
}

}

#endif // USE(CG)
