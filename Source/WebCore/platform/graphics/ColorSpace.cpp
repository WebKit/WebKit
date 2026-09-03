/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ColorSpace.h"

#include "NotImplemented.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextStream.h>

#if USE(CG)
#include "ColorSpaceCG.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#elif USE(SKIA)
#include "ColorSpaceSkia.h"
#endif

namespace WebCore {

#if USE(CG) || USE(SKIA)
#if USE(CG)
using KnownColorSpaceAccessor = CGColorSpaceRef();
#elif USE(SKIA)
using KnownColorSpaceAccessor = sk_sp<SkColorSpace>();
#endif
template<KnownColorSpaceAccessor accessor> static const ColorSpace& knownColorSpace()
{
    static NeverDestroyed<ColorSpace> colorSpace { accessor() };
    return colorSpace.get();
}
#else
template<PlatformColorSpace::Name name> static const ColorSpace& knownColorSpace()
{
    static NeverDestroyed<ColorSpace> colorSpace { name };
    return colorSpace.get();
}
#endif

const ColorSpace& ColorSpace::SRGB()
{
#if USE(CG) || USE(SKIA)
    return knownColorSpace<sRGBColorSpaceSingleton>();
#else
    return knownColorSpace<PlatformColorSpace::Name::SRGB>();
#endif
}

const ColorSpace& ColorSpace::LinearSRGB()
{
#if USE(CG) || USE(SKIA)
    return knownColorSpace<linearSRGBColorSpaceSingleton>();
#else
    return knownColorSpace<PlatformColorSpace::Name::LinearSRGB>();
#endif
}

#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
const ColorSpace& ColorSpace::DisplayP3()
{
#if USE(CG) || USE(SKIA)
    return knownColorSpace<displayP3ColorSpaceSingleton>();
#else
    return knownColorSpace<PlatformColorSpace::Name::DisplayP3>();
#endif
}

const ColorSpace& ColorSpace::ExtendedDisplayP3()
{
#if USE(CG) || USE(SKIA)
    return knownColorSpace<extendedDisplayP3ColorSpaceSingleton>();
#else
    return knownColorSpace<PlatformColorSpace::Name::ExtendedDisplayP3>();
#endif
}

const ColorSpace& ColorSpace::LinearDisplayP3()
{
#if USE(CG) || USE(SKIA)
    return knownColorSpace<linearDisplayP3ColorSpaceSingleton>();
#else
    return knownColorSpace<PlatformColorSpace::Name::LinearDisplayP3>();
#endif
}

const ColorSpace& ColorSpace::ExtendedLinearDisplayP3()
{
#if USE(CG) || USE(SKIA)
    return knownColorSpace<extendedLinearDisplayP3ColorSpaceSingleton>();
#else
    return knownColorSpace<PlatformColorSpace::Name::ExtendedLinearDisplayP3>();
#endif
}
#endif

#if ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB)
const ColorSpace& ColorSpace::ExtendedSRGB()
{
#if USE(CG) || USE(SKIA)
    return knownColorSpace<extendedSRGBColorSpaceSingleton>();
#else
    return knownColorSpace<PlatformColorSpace::Name::ExtendedSRGB>();
#endif
}

const ColorSpace& ColorSpace::ExtendedLinearSRGB()
{
#if USE(CG) || USE(SKIA)
    return knownColorSpace<extendedLinearSRGBColorSpaceSingleton>();
#else
    return knownColorSpace<PlatformColorSpace::Name::ExtendedLinearSRGB>();
#endif
}
#endif

#if ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_REC_2020)
const ColorSpace& ColorSpace::ExtendedRec2020()
{
#if USE(CG)
    return knownColorSpace<ITUR_2020ColorSpaceSingleton>();
#else
    return knownColorSpace<PlatformColorSpace::Name::ExtendedRec2020>();
#endif
}
#endif

bool operator==(const ColorSpace& a, const ColorSpace& b)
{
#if USE(CG)
    // Do not protect the platformColorSpace here as it is not strictly required for safety and
    // this code is performance sensitive.
    SUPPRESS_UNRETAINED_ARG return CGColorSpaceEqualToColorSpace(a.platformColorSpace(), b.platformColorSpace());
#elif USE(SKIA)
    return SkColorSpace::Equals(a.platformColorSpace().get(), b.platformColorSpace().get());
#else
    return a.platformColorSpace() == b.platformColorSpace();
#endif
}

std::optional<ColorSpace> ColorSpace::asRGB() const
{
#if USE(CG)
    // Avoid refing colorSpace here as this is performance-sensitive code.
    SUPPRESS_UNRETAINED_LOCAL CGColorSpaceRef colorSpace = platformColorSpace();
    if (CGColorSpaceGetModel(colorSpace) == kCGColorSpaceModelIndexed)
        colorSpace = CGColorSpaceGetBaseColorSpace(colorSpace);

    if (CGColorSpaceGetModel(colorSpace) != kCGColorSpaceModelRGB)
        return std::nullopt;

    if (usesExtendedRange())
        return std::nullopt;

    return ColorSpace(colorSpace);

#elif USE(SKIA)
    // When using skia, we're not using color spaces consisting of custom lookup tables, so we either yield SRGB or nothing.
    if (platformColorSpace()->isSRGB())
        return SRGB();
    return std::nullopt;

#else
    return *this;
#endif
}

std::optional<ColorSpace> ColorSpace::asExtended() const
{
    if (usesExtendedRange())
        return *this;
#if USE(CG)
    // Avoid refing color space here as this is performance-sensitive.
    SUPPRESS_UNRETAINED_ARG if (RetainPtr colorSpace = adoptCF(CGColorSpaceCreateExtended(platformColorSpace())))
        return ColorSpace(WTF::move(colorSpace));
#endif
    return std::nullopt;
}

bool ColorSpace::supportsOutput() const
{
#if USE(CG)
    // Avoid refing color space here as this is performance-sensitive.
    SUPPRESS_UNRETAINED_ARG return CGColorSpaceSupportsOutput(platformColorSpace());
#else
    notImplemented();
    return true;
#endif
}

bool ColorSpace::usesRGBColorModel() const
{
#if USE(CG)
    // Avoid refing color space here as this is performance-sensitive.
    SUPPRESS_UNRETAINED_ARG return CGColorSpaceGetModel(platformColorSpace()) == kCGColorSpaceModelRGB;
#else
    return true;
#endif
}

bool ColorSpace::usesExtendedRange() const
{
#if USE(CG)
    // Avoid refing color space here as this is performance-sensitive.
    SUPPRESS_UNRETAINED_ARG return CGColorSpaceUsesExtendedRange(platformColorSpace());
#else
    notImplemented();
    return false;
#endif
}

bool ColorSpace::usesITUR_2100TF() const
{
#if USE(CG)
    // Avoid refing color space here as this is performance-sensitive.
    SUPPRESS_UNRETAINED_ARG return CGColorSpaceUsesITUR_2100TF(platformColorSpace());
#else
    notImplemented();
    return false;
#endif
}

TextStream& operator<<(TextStream& ts, const ColorSpace& colorSpace)
{
    if (colorSpace == ColorSpace::SRGB())
        ts << "sRGB"_s;
    else if (colorSpace == ColorSpace::LinearSRGB())
        ts << "LinearSRGB"_s;
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
    else if (colorSpace == ColorSpace::DisplayP3())
        ts << "DisplayP3"_s;
#endif
#if ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB)
    else if (colorSpace == ColorSpace::ExtendedSRGB())
        ts << "ExtendedSRGB"_s;
#endif
#if ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_REC_2020)
    else if (colorSpace == ColorSpace::ExtendedRec2020())
        ts << "ExtendedRec2020"_s;
#endif
#if USE(CG)
    else if (RetainPtr description = adoptCF(CGColorSpaceCopyICCProfileDescription(protect(colorSpace.platformColorSpace()).get())))
        ts << String(description.get());
#endif

    return ts;
}

}
