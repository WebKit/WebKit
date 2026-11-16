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
#include "PredefinedColorSpace.h"

#include "ColorSpace.h"
#include "PixelFormat.h"

namespace WebCore {

ColorSpace toColorSpace(PredefinedColorSpace colorSpace)
{
    switch (colorSpace) {
    case PredefinedColorSpace::SRGB:
        return ColorSpace::SRGB();
    case PredefinedColorSpace::SRGBLinear:
        return ColorSpace::LinearSRGB();
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
    case PredefinedColorSpace::DisplayP3:
        return ColorSpace::DisplayP3();
    case PredefinedColorSpace::DisplayP3Linear:
        return ColorSpace::LinearDisplayP3();
#endif
    }

    ASSERT_NOT_REACHED();
    return ColorSpace::SRGB();
}

ColorSpace toExtendedColorSpace(PredefinedColorSpace colorSpace)
{
    switch (colorSpace) {
    case PredefinedColorSpace::SRGB:
#if ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB)
        return ColorSpace::ExtendedSRGB();
#else
        return ColorSpace::SRGB();
#endif
    case PredefinedColorSpace::SRGBLinear:
#if ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB)
        return ColorSpace::ExtendedLinearSRGB();
#else
        return ColorSpace::LinearSRGB();
#endif
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
    case PredefinedColorSpace::DisplayP3:
        return ColorSpace::ExtendedDisplayP3();
    case PredefinedColorSpace::DisplayP3Linear:
        return ColorSpace::ExtendedLinearDisplayP3();
#endif
    }

    ASSERT_NOT_REACHED();
#if ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB)
    return ColorSpace::ExtendedSRGB();
#else
    return ColorSpace::SRGB();
#endif
}

ColorSpace toColorSpace(PredefinedColorSpace colorSpace, AllowExtendedColorSpace allowExtendedColorSpace)
{
    return (allowExtendedColorSpace == AllowExtendedColorSpace::No) ? toColorSpace(colorSpace) : toExtendedColorSpace(colorSpace);
}

std::optional<PredefinedColorSpace> toPredefinedColorSpace(const ColorSpace& colorSpace)
{
    if (colorSpace == ColorSpace::SRGB())
        return PredefinedColorSpace::SRGB;
    if (colorSpace == ColorSpace::LinearSRGB())
        return PredefinedColorSpace::SRGBLinear;
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
    if (colorSpace == ColorSpace::DisplayP3())
        return PredefinedColorSpace::DisplayP3;
    if (colorSpace == ColorSpace::LinearDisplayP3())
        return PredefinedColorSpace::DisplayP3Linear;
#endif

#if ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB)
    if (colorSpace == ColorSpace::ExtendedSRGB())
        return PredefinedColorSpace::SRGB;
    if (colorSpace == ColorSpace::ExtendedLinearSRGB())
        return PredefinedColorSpace::SRGBLinear;
#endif
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
    if (colorSpace == ColorSpace::ExtendedDisplayP3())
        return PredefinedColorSpace::DisplayP3;
    if (colorSpace == ColorSpace::ExtendedLinearDisplayP3())
        return PredefinedColorSpace::DisplayP3Linear;
#endif

    return std::nullopt;
}

}
