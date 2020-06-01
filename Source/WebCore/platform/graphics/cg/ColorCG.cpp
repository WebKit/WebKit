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

#include "GraphicsContextCG.h"
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

static SimpleColor makeSimpleColorFromCGColor(CGColorRef color)
{
    // FIXME: ExtendedColor - needs to handle color spaces.

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

    static const double scaleFactor = nextafter(256.0, 0.0);
    return makeSimpleColor(r * scaleFactor, g * scaleFactor, b * scaleFactor, a * scaleFactor);
}

Color::Color(CGColorRef color)
{
    // FIXME: ExtendedColor - needs to handle color spaces.

    if (!color) {
        m_colorData.simpleColorAndFlags = invalidSimpleColor;
        return;
    }

    setSimpleColor(makeSimpleColorFromCGColor(color));
}

Color::Color(CGColorRef color, SemanticTag)
{
    // FIXME: ExtendedColor - needs to handle color spaces.

    if (!color) {
        m_colorData.simpleColorAndFlags = invalidSimpleColor;
        return;
    }

    setSimpleColor(makeSimpleColorFromCGColor(color));
    tagAsSemantic();
}

static CGColorRef leakCGColor(const Color& color)
{
    auto [colorSpace, components] = color.colorSpaceAndComponents();
    auto [r, g, b, a] = components;
    CGFloat cgFloatComponents[4] { r, g, b, a };

    switch (colorSpace) {
    case ColorSpace::SRGB:
        return CGColorCreate(sRGBColorSpaceRef(), cgFloatComponents);
    case ColorSpace::DisplayP3:
        return CGColorCreate(displayP3ColorSpaceRef(), cgFloatComponents);
    case ColorSpace::LinearRGB:
        // FIXME: Do we ever create CGColorRefs in these spaces? It may only be ImageBuffers.
        return CGColorCreate(sRGBColorSpaceRef(), cgFloatComponents);
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

CGColorRef cachedCGColor(const Color& color)
{
    if (!color.isExtended()) {
        switch (color.asSimple().value()) {
        case Color::transparent.value(): {
            static CGColorRef transparentCGColor = leakCGColor(color);
            return transparentCGColor;
        }
        case Color::black.value(): {
            static CGColorRef blackCGColor = leakCGColor(color);
            return blackCGColor;
        }
        case Color::white.value(): {
            static CGColorRef whiteCGColor = leakCGColor(color);
            return whiteCGColor;
        }
        }
    }

    ASSERT(color.isExtended() || color.asSimple().value());

    static NeverDestroyed<TinyLRUCache<Color, RetainPtr<CGColorRef>, 32>> cache;
    return cache.get().get(color).get();
}

}

#endif // USE(CG)
