/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "ColorSpaceCG.h"

#if USE(CG)

#include <mutex>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

template<const CFStringRef& colorSpaceNameGlobalConstant> static CGColorSpaceRef namedColorSpace()
{
    static NeverDestroyed<RetainPtr<CGColorSpaceRef>> colorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        colorSpace.get() = adoptCF(CGColorSpaceCreateWithName(colorSpaceNameGlobalConstant));
        ASSERT(colorSpace.get());
    });
    return colorSpace.get().get();
}

#if HAVE(CORE_GRAPHICS_CREATE_EXTENDED_COLOR_SPACE)
template<const CFStringRef& colorSpaceNameGlobalConstant> static CGColorSpaceRef extendedNamedColorSpace()
{
    static NeverDestroyed<RetainPtr<CGColorSpaceRef>> colorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        colorSpace.get() = adoptCF(CGColorSpaceCreateExtended(namedColorSpace<colorSpaceNameGlobalConstant>()));
        ASSERT(colorSpace.get());
    });
    return colorSpace.get().get();
}
#endif

CGColorSpaceRef sRGBColorSpaceRef()
{
    return namedColorSpace<kCGColorSpaceSRGB>();
}

#if HAVE(CORE_GRAPHICS_ADOBE_RGB_1998_COLOR_SPACE)
CGColorSpaceRef adobeRGB1998ColorSpaceRef()
{
    return namedColorSpace<kCGColorSpaceAdobeRGB1998>();
}
#endif

#if HAVE(CORE_GRAPHICS_DISPLAY_P3_COLOR_SPACE)
CGColorSpaceRef displayP3ColorSpaceRef()
{
    return namedColorSpace<kCGColorSpaceDisplayP3>();
}
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_ADOBE_RGB_1998_COLOR_SPACE)
CGColorSpaceRef extendedAdobeRGB1998ColorSpaceRef()
{
    return extendedNamedColorSpace<kCGColorSpaceAdobeRGB1998>();
}
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_DISPLAY_P3_COLOR_SPACE)
CGColorSpaceRef extendedDisplayP3ColorSpaceRef()
{
    return namedColorSpace<kCGColorSpaceExtendedDisplayP3>();
}
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_ITUR_2020_COLOR_SPACE)
CGColorSpaceRef extendedITUR_2020ColorSpaceRef()
{
    return namedColorSpace<kCGColorSpaceExtendedITUR_2020>();
}
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_LINEAR_SRGB_COLOR_SPACE)
CGColorSpaceRef extendedLinearSRGBColorSpaceRef()
{
    return namedColorSpace<kCGColorSpaceExtendedLinearSRGB>();
}
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_ROMMRGB_COLOR_SPACE)
CGColorSpaceRef extendedROMMRGBColorSpaceRef()
{
    return extendedNamedColorSpace<kCGColorSpaceROMMRGB>();
}
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
CGColorSpaceRef extendedSRGBColorSpaceRef()
{
    return namedColorSpace<kCGColorSpaceExtendedSRGB>();
}
#endif

#if HAVE(CORE_GRAPHICS_ITUR_2020_COLOR_SPACE)
CGColorSpaceRef ITUR_2020ColorSpaceRef()
{
    return namedColorSpace<kCGColorSpaceITUR_2020>();
}
#endif

#if HAVE(CORE_GRAPHICS_LINEAR_SRGB_COLOR_SPACE)
CGColorSpaceRef linearSRGBColorSpaceRef()
{
    return namedColorSpace<kCGColorSpaceLinearSRGB>();
}
#endif

#if HAVE(CORE_GRAPHICS_ROMMRGB_COLOR_SPACE)
CGColorSpaceRef ROMMRGBColorSpaceRef()
{
    return namedColorSpace<kCGColorSpaceROMMRGB>();
}
#endif

#if HAVE(CORE_GRAPHICS_XYZ_D50_COLOR_SPACE)
CGColorSpaceRef xyzD50ColorSpaceRef()
{
    return namedColorSpace<kCGColorSpaceGenericXYZ>();
}

// FIXME: Figure out how to create a CoreGraphics XYZ-D65 color space and add a xyzD65ColorSpaceRef(). Perhaps CGColorSpaceCreateCalibratedRGB() with identify black point, D65 white point, and identity matrix.

#endif

std::optional<ColorSpace> colorSpaceForCGColorSpace(CGColorSpaceRef colorSpace)
{
    // First test for the four most common spaces, sRGB, Extended sRGB, DisplayP3 and Linear sRGB, and then test
    // the reset in alphabetical order.
    // FIXME: Consider using a UncheckedKeyHashMap (with CFHash based keys) rather than the linear set of tests.

    if (CGColorSpaceEqualToColorSpace(colorSpace, sRGBColorSpaceRef()))
        return ColorSpace::SRGB;

#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, extendedSRGBColorSpaceRef()))
        return ColorSpace::ExtendedSRGB;
#endif

#if HAVE(CORE_GRAPHICS_DISPLAY_P3_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, displayP3ColorSpaceRef()))
        return ColorSpace::DisplayP3;
#endif

#if HAVE(CORE_GRAPHICS_LINEAR_SRGB_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, linearSRGBColorSpaceRef()))
        return ColorSpace::LinearSRGB;
#endif


#if HAVE(CORE_GRAPHICS_ADOBE_RGB_1998_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, adobeRGB1998ColorSpaceRef()))
        return ColorSpace::A98RGB;
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_ADOBE_RGB_1998_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, extendedAdobeRGB1998ColorSpaceRef()))
        return ColorSpace::ExtendedA98RGB;
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_DISPLAY_P3_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, extendedDisplayP3ColorSpaceRef()))
        return ColorSpace::ExtendedDisplayP3;
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_LINEAR_SRGB_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, extendedLinearSRGBColorSpaceRef()))
        return ColorSpace::ExtendedLinearSRGB;
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_ITUR_2020_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, extendedITUR_2020ColorSpaceRef()))
        return ColorSpace::ExtendedRec2020;
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_ROMMRGB_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, extendedROMMRGBColorSpaceRef()))
        return ColorSpace::ExtendedProPhotoRGB;
#endif

#if HAVE(CORE_GRAPHICS_ITUR_2020_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, ITUR_2020ColorSpaceRef()))
        return ColorSpace::Rec2020;
#endif

#if HAVE(CORE_GRAPHICS_ROMMRGB_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, ROMMRGBColorSpaceRef()))
        return ColorSpace::ProPhotoRGB;
#endif

#if HAVE(CORE_GRAPHICS_XYZ_D50_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, xyzD50ColorSpaceRef()))
        return ColorSpace::XYZ_D50;
#endif

    // FIXME: Add support for remaining color spaces to support more direct conversions.

    return std::nullopt;
}

}

#endif // USE(CG)
