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

namespace WebCore {

CGColorSpaceRef sRGBColorSpaceRef()
{
    static CGColorSpaceRef sRGBColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if PLATFORM(WIN)
        // Out-of-date CG installations will not honor kCGColorSpaceSRGB. This logic avoids
        // causing a crash under those conditions. Since the default color space in Windows
        // is sRGB, this all works out nicely.
        // FIXME: Is this still needed? rdar://problem/15213515 was fixed.
        sRGBColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        if (!sRGBColorSpace)
            sRGBColorSpace = CGColorSpaceCreateDeviceRGB();
#else
        sRGBColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
#endif // PLATFORM(WIN)
    });
    return sRGBColorSpace;
}

#if HAVE(CORE_GRAPHICS_ADOBE_RGB_1998_COLOR_SPACE)
CGColorSpaceRef adobeRGB1998ColorSpaceRef()
{
    static CGColorSpaceRef adobeRGB1998ColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        adobeRGB1998ColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceAdobeRGB1998);
        ASSERT(adobeRGB1998ColorSpace);
    });
    return adobeRGB1998ColorSpace;
}
#endif

#if HAVE(CORE_GRAPHICS_DISPLAY_P3_COLOR_SPACE)
CGColorSpaceRef displayP3ColorSpaceRef()
{
    static CGColorSpaceRef displayP3ColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        displayP3ColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3);
        ASSERT(displayP3ColorSpace);
    });
    return displayP3ColorSpace;
}
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
CGColorSpaceRef extendedSRGBColorSpaceRef()
{
    static CGColorSpaceRef extendedSRGBColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        extendedSRGBColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceExtendedSRGB);
        ASSERT(extendedSRGBColorSpace);
    });
    return extendedSRGBColorSpace;
}
#endif

#if HAVE(CORE_GRAPHICS_ITUR_2020_COLOR_SPACE)
CGColorSpaceRef ITUR_2020ColorSpaceRef()
{
    static CGColorSpaceRef ITUR2020ColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        ITUR2020ColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020);
        ASSERT(ITUR2020ColorSpace);
    });
    return ITUR2020ColorSpace;
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
    static CGColorSpaceRef linearRGBColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        linearRGBColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);
        ASSERT(linearRGBColorSpace);
    });
    return linearRGBColorSpace;
}
#endif

#if HAVE(CORE_GRAPHICS_ROMMRGB_COLOR_SPACE)
CGColorSpaceRef ROMMRGBColorSpaceRef()
{
    static CGColorSpaceRef ROMMRGBColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        ROMMRGBColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceROMMRGB);
        ASSERT(ROMMRGBColorSpace);
    });
    return ROMMRGBColorSpace;
}
#endif

#if HAVE(CORE_GRAPHICS_XYZ_COLOR_SPACE)
CGColorSpaceRef xyzColorSpaceRef()
{
    static CGColorSpaceRef xyzColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        xyzColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericXYZ);
        ASSERT(xyzColorSpace);
    });
    return xyzColorSpace;
}
#endif

}

#endif // USE(CG)
