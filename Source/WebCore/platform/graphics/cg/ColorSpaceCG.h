/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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

#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/ColorSpaceName.h>
#include <optional>
#include <wtf/cf/TypeCastsCF.h>

WTF_DECLARE_CF_TYPE_TRAIT(CGColorSpace);

namespace WebCore {

template<ColorSpaceName> struct CGColorSpaceMapping;

WEBCORE_EXPORT CGColorSpaceRef sRGBColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::SRGB> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return sRGBColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef adobeRGB1998ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::A98RGB> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return adobeRGB1998ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef displayP3ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::DisplayP3> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return displayP3ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedAdobeRGB1998ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::ExtendedA98RGB> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return extendedAdobeRGB1998ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedDisplayP3ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::ExtendedDisplayP3> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return extendedDisplayP3ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedITUR_2020ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::ExtendedRec2020> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return extendedITUR_2020ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedLinearDisplayP3ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::ExtendedLinearDisplayP3> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return extendedLinearDisplayP3ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedLinearSRGBColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::ExtendedLinearSRGB> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return extendedLinearSRGBColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedROMMRGBColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::ExtendedProPhotoRGB> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return extendedROMMRGBColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef extendedSRGBColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::ExtendedSRGB> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return extendedSRGBColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef ITUR_2020ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::Rec2020> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return ITUR_2020ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef linearDisplayP3ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::LinearDisplayP3> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return linearDisplayP3ColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef linearSRGBColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::LinearSRGB> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return linearSRGBColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef ROMMRGBColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::ProPhotoRGB> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return ROMMRGBColorSpaceSingleton();
    }
};

WEBCORE_EXPORT CGColorSpaceRef xyzD50ColorSpaceSingleton();
template<> struct CGColorSpaceMapping<ColorSpaceName::XYZ_D50> {
    static CGColorSpaceRef colorSpaceSingleton()
    {
        return xyzD50ColorSpaceSingleton();
    }
};

// FIXME: Add support for these once/if CoreGraphics adds support for them.
template<> struct CGColorSpaceMapping<ColorSpaceName::HSL> { };
template<> struct CGColorSpaceMapping<ColorSpaceName::HWB> { };
template<> struct CGColorSpaceMapping<ColorSpaceName::LCH> { };
template<> struct CGColorSpaceMapping<ColorSpaceName::Lab> { };
template<> struct CGColorSpaceMapping<ColorSpaceName::OKLab> { };
template<> struct CGColorSpaceMapping<ColorSpaceName::OKLCH> { };
template<> struct CGColorSpaceMapping<ColorSpaceName::XYZ_D65> { };


WEBCORE_EXPORT std::optional<ColorSpaceName> colorSpaceForCGColorSpace(CGColorSpaceRef);


template<ColorSpaceName, typename = void> inline constexpr bool HasCGColorSpaceMapping = false;
template<ColorSpaceName space> inline constexpr bool HasCGColorSpaceMapping<space, std::void_t<decltype(CGColorSpaceMapping<space>::colorSpaceSingleton())>> = true;
static_assert(HasCGColorSpaceMapping<ColorSpaceName::SRGB>, "An SRGB color space mapping must be supported on all platforms.");

template<ColorSpaceName space, bool = HasCGColorSpaceMapping<space>> struct CGColorSpaceMappingOrNullGetter { static CGColorSpaceRef colorSpaceSingleton() { return nullptr; } };
template<ColorSpaceName space> struct CGColorSpaceMappingOrNullGetter<space, true> { static CGColorSpaceRef colorSpaceSingleton() { return CGColorSpaceMapping<space>::colorSpaceSingleton(); } };

template<ColorSpaceName space> CGColorSpaceRef cachedCGColorSpaceSingleton()
{
    return CGColorSpaceMapping<space>::colorSpaceSingleton();
}

template<ColorSpaceName space> CGColorSpaceRef cachedNullableCGColorSpaceSingleton()
{
    return CGColorSpaceMappingOrNullGetter<space>::colorSpaceSingleton();
}

inline CGColorSpaceRef cachedNullableCGColorSpaceSingleton(ColorSpaceName colorSpace)
{
    switch (colorSpace) {
    case ColorSpaceName::A98RGB:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::A98RGB>();
    case ColorSpaceName::DisplayP3:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::DisplayP3>();
    case ColorSpaceName::ExtendedA98RGB:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::ExtendedA98RGB>();
    case ColorSpaceName::ExtendedDisplayP3:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::ExtendedDisplayP3>();
    case ColorSpaceName::ExtendedLinearDisplayP3:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::ExtendedLinearDisplayP3>();
    case ColorSpaceName::ExtendedLinearSRGB:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::ExtendedLinearSRGB>();
    case ColorSpaceName::ExtendedProPhotoRGB:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::ExtendedProPhotoRGB>();
    case ColorSpaceName::ExtendedRec2020:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::ExtendedRec2020>();
    case ColorSpaceName::ExtendedSRGB:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::ExtendedSRGB>();
    case ColorSpaceName::HSL:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::HSL>();
    case ColorSpaceName::HWB:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::HWB>();
    case ColorSpaceName::LCH:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::LCH>();
    case ColorSpaceName::Lab:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::Lab>();
    case ColorSpaceName::LinearDisplayP3:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::LinearDisplayP3>();
    case ColorSpaceName::LinearSRGB:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::LinearSRGB>();
    case ColorSpaceName::OKLCH:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::OKLCH>();
    case ColorSpaceName::OKLab:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::OKLab>();
    case ColorSpaceName::ProPhotoRGB:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::ProPhotoRGB>();
    case ColorSpaceName::Rec2020:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::Rec2020>();
    case ColorSpaceName::SRGB:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::SRGB>();
    case ColorSpaceName::XYZ_D50:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::XYZ_D50>();
    case ColorSpaceName::XYZ_D65:
        return cachedNullableCGColorSpaceSingleton<ColorSpaceName::XYZ_D65>();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

}
