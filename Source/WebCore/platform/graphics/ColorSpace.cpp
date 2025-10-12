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
#include "ColorSpace.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, ColorSpace colorSpace)
{
    switch (colorSpace) {
    case ColorSpace::A98RGB:
        ts << "A98-RGB"_s;
        break;
    case ColorSpace::DisplayP3:
        ts << "DisplayP3"_s;
        break;
    case ColorSpace::ExtendedA98RGB:
        ts << "Extended A98-RGB"_s;
        break;
    case ColorSpace::ExtendedDisplayP3:
        ts << "Extended DisplayP3"_s;
        break;
    case ColorSpace::ExtendedLinearDisplayP3:
        ts << "Extended Linear DisplayP3"_s;
        break;
    case ColorSpace::ExtendedLinearSRGB:
        ts << "Extended Linear sRGB"_s;
        break;
    case ColorSpace::ExtendedProPhotoRGB:
        ts << "Extended ProPhotoRGB"_s;
        break;
    case ColorSpace::ExtendedRec2020:
        ts << "Extended Rec2020"_s;
        break;
    case ColorSpace::ExtendedSRGB:
        ts << "Extended sRGB"_s;
        break;
    case ColorSpace::HSL:
        ts << "HSL"_s;
        break;
    case ColorSpace::HWB:
        ts << "HWB"_s;
        break;
    case ColorSpace::LCH:
        ts << "LCH"_s;
        break;
    case ColorSpace::Lab:
        ts << "Lab"_s;
        break;
    case ColorSpace::LinearDisplayP3:
        ts << "Linear DisplayP3"_s;
        break;
    case ColorSpace::LinearSRGB:
        ts << "Linear sRGB"_s;
        break;
    case ColorSpace::OKLCH:
        ts << "OKLCH"_s;
        break;
    case ColorSpace::OKLab:
        ts << "OKLab"_s;
        break;
    case ColorSpace::ProPhotoRGB:
        ts << "ProPhotoRGB"_s;
        break;
    case ColorSpace::Rec2020:
        ts << "Rec2020"_s;
        break;
    case ColorSpace::SRGB:
        ts << "sRGB"_s;
        break;
    case ColorSpace::XYZ_D50:
        ts << "XYZ-D50"_s;
        break;
    case ColorSpace::XYZ_D65:
        ts << "XYZ-D50"_s;
        break;
    }
    return ts;
}

}
