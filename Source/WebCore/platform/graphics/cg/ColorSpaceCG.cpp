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

#if HAVE(CORE_GRAPHICS_LAB_COLOR_SPACE)
CGColorSpaceRef labColorSpaceRef()
{
    // FIXME: Add support for conversion to Lab on supported platforms.
    return nullptr;
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

#if HAVE(CORE_GRAPHICS_XYZ_COLOR_SPACE)
CGColorSpaceRef xyzColorSpaceRef()
{
    return namedColorSpace<kCGColorSpaceGenericXYZ>();
}
#endif

std::optional<ColorSpace> colorSpaceForCGColorSpace(CGColorSpaceRef colorSpace)
{
    if (CGColorSpaceEqualToColorSpace(colorSpace, sRGBColorSpaceRef()))
        return ColorSpace::SRGB;

#if HAVE(CORE_GRAPHICS_ADOBE_RGB_1998_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, adobeRGB1998ColorSpaceRef()))
        return ColorSpace::A98RGB;
#endif

#if HAVE(CORE_GRAPHICS_DISPLAY_P3_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, displayP3ColorSpaceRef()))
        return ColorSpace::DisplayP3;
#endif

    // FIXME: labColorSpaceRef() not implemented.

#if HAVE(CORE_GRAPHICS_LINEAR_SRGB_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, linearSRGBColorSpaceRef()))
        return ColorSpace::LinearSRGB;
#endif

#if HAVE(CORE_GRAPHICS_ROMMRGB_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, ROMMRGBColorSpaceRef()))
        return ColorSpace::ProPhotoRGB;
#endif

#if HAVE(CORE_GRAPHICS_ITUR_2020_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, ITUR_2020ColorSpaceRef()))
        return ColorSpace::Rec2020;
#endif

#if HAVE(CORE_GRAPHICS_XYZ_COLOR_SPACE)
    if (CGColorSpaceEqualToColorSpace(colorSpace, xyzColorSpaceRef()))
        return ColorSpace::XYZ_D50;
#endif

    return std::nullopt;
}

}

#endif // USE(CG)
