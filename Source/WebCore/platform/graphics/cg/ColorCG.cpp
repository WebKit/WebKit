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
#include <wtf/Assertions.h>
#include <wtf/RetainPtr.h>
#include <wtf/TinyLRUCache.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {
static CGColorRef leakCGColor(const Color&) CF_RETURNS_RETAINED;
}

namespace WTF {

template<>
RetainPtr<CGColorRef> TinyLRUCachePolicy<WebCore::Color, RetainPtr<CGColorRef>>::createValueForKey(const WebCore::Color& color)
{
    return adoptCF(WebCore::leakCGColor(color));
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

    return convertColor<SRGBA<uint8_t>>(SRGBA { r, g, b, a });
}

Color::Color(CGColorRef color)
    : Color(roundAndClampToSRGBALossy(color))
{
}

Color::Color(CGColorRef color, SemanticTag tag)
    : Color(roundAndClampToSRGBALossy(color), tag)
{
}

static CGColorRef leakCGColor(const Color& color)
{
    auto [colorSpace, components] = color.colorSpaceAndComponents();

    auto cgColorSpace = cachedCGColorSpace(colorSpace);

    // Some CG ports don't support all the color spaces required and return
    // sRGBColorSpaceRef() for unsupported color spaces. In those cases, we
    // need to eagerly convert the color into extended sRGB ourselves before
    // creating the CGColorRef.
    // FIXME: This is not a good way to indicate lack of support. Make this
    // more explicit.
    if (colorSpace != ColorSpace::SRGB && cgColorSpace == sRGBColorSpaceRef()) {
        auto colorConvertedToExtendedSRGBA = callWithColorType(components, colorSpace, [] (const auto& color) {
            return convertColor<ExtendedSRGBA<float>>(color);
        });
        components = asColorComponents(colorConvertedToExtendedSRGBA);
        cgColorSpace = extendedSRGBColorSpaceRef();
    }

    auto [r, g, b, a] = components;
    CGFloat cgFloatComponents[4] { r, g, b, a };

    return CGColorCreate(cgColorSpace, cgFloatComponents);
}

CGColorRef cachedCGColor(const Color& color)
{
    if (color.isInline()) {
        switch (PackedColor::RGBA { color.asInline() }.value) {
        case PackedColor::RGBA { Color::transparentBlack }.value: {
            static CGColorRef transparentCGColor = leakCGColor(color);
            return transparentCGColor;
        }
        case PackedColor::RGBA { Color::black }.value: {
            static CGColorRef blackCGColor = leakCGColor(color);
            return blackCGColor;
        }
        case PackedColor::RGBA { Color::white }.value: {
            static CGColorRef whiteCGColor = leakCGColor(color);
            return whiteCGColor;
        }
        }
    }

    static NeverDestroyed<TinyLRUCache<Color, RetainPtr<CGColorRef>, 32>> cache;
    return cache.get().get(color).get();
}

}

#endif // USE(CG)
