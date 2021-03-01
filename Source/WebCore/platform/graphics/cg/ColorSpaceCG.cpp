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

CGColorSpaceRef a98RGBColorSpaceRef()
{
    static CGColorSpaceRef a98RGBColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if PLATFORM(COCOA)
        a98RGBColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceAdobeRGB1998);
#else
        a98RGBColorSpace = sRGBColorSpaceRef();
#endif
    });
    return a98RGBColorSpace;
}

CGColorSpaceRef displayP3ColorSpaceRef()
{
    static CGColorSpaceRef displayP3ColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if PLATFORM(COCOA)
        displayP3ColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3);
#else
        displayP3ColorSpace = sRGBColorSpaceRef();
#endif
    });
    return displayP3ColorSpace;
}

CGColorSpaceRef extendedSRGBColorSpaceRef()
{
    static CGColorSpaceRef extendedSRGBColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        CGColorSpaceRef colorSpace = nullptr;
#if PLATFORM(COCOA)
        colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceExtendedSRGB);
#endif
        // If there is no support for extended sRGB, fall back to sRGB.
        if (!colorSpace)
            colorSpace = sRGBColorSpaceRef();

        extendedSRGBColorSpace = colorSpace;
    });
    return extendedSRGBColorSpace;
}

CGColorSpaceRef labColorSpaceRef()
{
    // FIXME: Add support for conversion to Lab on supported platforms.
    return sRGBColorSpaceRef();
}

CGColorSpaceRef linearRGBColorSpaceRef()
{
    static CGColorSpaceRef linearRGBColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if PLATFORM(WIN)
        // FIXME: Windows should be able to use linear sRGB, this is tracked by http://webkit.org/b/80000.
        linearRGBColorSpace = sRGBColorSpaceRef();
#else
        linearRGBColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);
#endif
    });
    return linearRGBColorSpace;
}

CGColorSpaceRef proPhotoRGBColorSpaceRef()
{
    static CGColorSpaceRef proPhotoRGBColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if PLATFORM(COCOA)
        proPhotoRGBColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceROMMRGB);
#else
        proPhotoRGBColorSpace = sRGBColorSpaceRef();
#endif
    });
    return proPhotoRGBColorSpace;
}

CGColorSpaceRef rec2020ColorSpaceRef()
{
    static CGColorSpaceRef rec2020ColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if PLATFORM(COCOA)
        rec2020ColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020);
#else
        rec2020ColorSpace = sRGBColorSpaceRef();
#endif
    });
    return rec2020ColorSpace;
}

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

CGColorSpaceRef xyzD50ColorSpaceRef()
{
    static CGColorSpaceRef xyzD50ColorSpace;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if PLATFORM(COCOA)
        xyzD50ColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericXYZ);
#else
        xyzD50ColorSpace = sRGBColorSpaceRef();
#endif
    });
    return xyzD50ColorSpace;
}

}

#endif // USE(CG)
