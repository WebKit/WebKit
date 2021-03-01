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

typedef struct CGColorSpace *CGColorSpaceRef;

namespace WebCore {

WEBCORE_EXPORT CGColorSpaceRef a98RGBColorSpaceRef();
WEBCORE_EXPORT CGColorSpaceRef displayP3ColorSpaceRef();
WEBCORE_EXPORT CGColorSpaceRef extendedSRGBColorSpaceRef();
WEBCORE_EXPORT CGColorSpaceRef labColorSpaceRef();
WEBCORE_EXPORT CGColorSpaceRef linearRGBColorSpaceRef();
WEBCORE_EXPORT CGColorSpaceRef proPhotoRGBColorSpaceRef();
WEBCORE_EXPORT CGColorSpaceRef rec2020ColorSpaceRef();
WEBCORE_EXPORT CGColorSpaceRef sRGBColorSpaceRef();
WEBCORE_EXPORT CGColorSpaceRef xyzD50ColorSpaceRef();

static inline CGColorSpaceRef cachedCGColorSpace(ColorSpace colorSpace)
{
    switch (colorSpace) {
    case ColorSpace::A98RGB:
        return a98RGBColorSpaceRef();
    case ColorSpace::DisplayP3:
        return displayP3ColorSpaceRef();
    case ColorSpace::Lab:
        return labColorSpaceRef();
    case ColorSpace::LinearSRGB:
        return linearRGBColorSpaceRef();
    case ColorSpace::ProPhotoRGB:
        return proPhotoRGBColorSpaceRef();
    case ColorSpace::Rec2020:
        return rec2020ColorSpaceRef();
    case ColorSpace::SRGB:
        return sRGBColorSpaceRef();
    case ColorSpace::XYZ_D50:
        return xyzD50ColorSpaceRef();
    }
    ASSERT_NOT_REACHED();
    return sRGBColorSpaceRef();
}

static inline CGColorSpaceRef cachedCGColorSpace(DestinationColorSpace colorSpace)
{
    switch (colorSpace) {
    case DestinationColorSpace::LinearSRGB:
        return linearRGBColorSpaceRef();
    case DestinationColorSpace::SRGB:
        return sRGBColorSpaceRef();
    }
    ASSERT_NOT_REACHED();
    return sRGBColorSpaceRef();
}

}
