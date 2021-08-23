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

#pragma once

#include "ColorSpace.h"
#include <optional>

typedef struct CGColorSpace *CGColorSpaceRef;

namespace WebCore {

WEBCORE_EXPORT CGColorSpaceRef sRGBColorSpaceRef();

#if HAVE(CORE_GRAPHICS_ADOBE_RGB_1998_COLOR_SPACE)
WEBCORE_EXPORT CGColorSpaceRef adobeRGB1998ColorSpaceRef();
#endif

#if HAVE(CORE_GRAPHICS_DISPLAY_P3_COLOR_SPACE)
WEBCORE_EXPORT CGColorSpaceRef displayP3ColorSpaceRef();
#endif

#if HAVE(CORE_GRAPHICS_EXTENDED_SRGB_COLOR_SPACE)
WEBCORE_EXPORT CGColorSpaceRef extendedSRGBColorSpaceRef();
#endif

#if HAVE(CORE_GRAPHICS_ITUR_2020_COLOR_SPACE)
WEBCORE_EXPORT CGColorSpaceRef ITUR_2020ColorSpaceRef();
#endif

#if HAVE(CORE_GRAPHICS_LAB_COLOR_SPACE)
WEBCORE_EXPORT CGColorSpaceRef labColorSpaceRef();
#endif

#if HAVE(CORE_GRAPHICS_LINEAR_SRGB_COLOR_SPACE)
WEBCORE_EXPORT CGColorSpaceRef linearSRGBColorSpaceRef();
#endif

#if HAVE(CORE_GRAPHICS_ROMMRGB_COLOR_SPACE)
WEBCORE_EXPORT CGColorSpaceRef ROMMRGBColorSpaceRef();
#endif

#if HAVE(CORE_GRAPHICS_XYZ_COLOR_SPACE)
WEBCORE_EXPORT CGColorSpaceRef xyzColorSpaceRef();
#endif

std::optional<ColorSpace> colorSpaceForCGColorSpace(CGColorSpaceRef);

static inline CGColorSpaceRef cachedNullableCGColorSpace(ColorSpace colorSpace)
{
    switch (colorSpace) {
    case ColorSpace::A98RGB:
#if HAVE(CORE_GRAPHICS_ADOBE_RGB_1998_COLOR_SPACE)
        return adobeRGB1998ColorSpaceRef();
#else
        return nullptr;
#endif
    case ColorSpace::DisplayP3:
#if HAVE(CORE_GRAPHICS_DISPLAY_P3_COLOR_SPACE)
        return displayP3ColorSpaceRef();
#else
        return nullptr;
#endif
    case ColorSpace::LCH:
        return nullptr;
    case ColorSpace::Lab:
#if HAVE(CORE_GRAPHICS_LAB_COLOR_SPACE)
        return labColorSpaceRef();
#else
        return nullptr;
#endif
    case ColorSpace::LinearSRGB:
#if HAVE(CORE_GRAPHICS_LINEAR_SRGB_COLOR_SPACE)
        return linearSRGBColorSpaceRef();
#else
        return nullptr;
#endif
    case ColorSpace::ProPhotoRGB:
#if HAVE(CORE_GRAPHICS_ROMMRGB_COLOR_SPACE)
        return ROMMRGBColorSpaceRef();
#else
        return nullptr;
#endif
    case ColorSpace::Rec2020:
#if HAVE(CORE_GRAPHICS_ITUR_2020_COLOR_SPACE)
        return ITUR_2020ColorSpaceRef();
#else
        return nullptr;
#endif
    case ColorSpace::SRGB:
        return sRGBColorSpaceRef();
    case ColorSpace::XYZ_D50:
#if HAVE(CORE_GRAPHICS_XYZ_COLOR_SPACE)
        return xyzColorSpaceRef();
#else
        return nullptr;
#endif
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

}
