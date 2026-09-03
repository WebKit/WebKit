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
#include "ColorSpaceName.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, ColorSpaceName colorSpace)
{
    switch (colorSpace) {
    case ColorSpaceName::A98RGB:
        ts << "A98-RGB"_s;
        break;
    case ColorSpaceName::DisplayP3:
        ts << "DisplayP3"_s;
        break;
    case ColorSpaceName::ExtendedA98RGB:
        ts << "Extended A98-RGB"_s;
        break;
    case ColorSpaceName::ExtendedDisplayP3:
        ts << "Extended DisplayP3"_s;
        break;
    case ColorSpaceName::ExtendedLinearDisplayP3:
        ts << "Extended Linear DisplayP3"_s;
        break;
    case ColorSpaceName::ExtendedLinearSRGB:
        ts << "Extended Linear sRGB"_s;
        break;
    case ColorSpaceName::ExtendedProPhotoRGB:
        ts << "Extended ProPhotoRGB"_s;
        break;
    case ColorSpaceName::ExtendedRec2020:
        ts << "Extended Rec2020"_s;
        break;
    case ColorSpaceName::ExtendedSRGB:
        ts << "Extended sRGB"_s;
        break;
    case ColorSpaceName::HSL:
        ts << "HSL"_s;
        break;
    case ColorSpaceName::HWB:
        ts << "HWB"_s;
        break;
    case ColorSpaceName::LCH:
        ts << "LCH"_s;
        break;
    case ColorSpaceName::Lab:
        ts << "Lab"_s;
        break;
    case ColorSpaceName::LinearDisplayP3:
        ts << "Linear DisplayP3"_s;
        break;
    case ColorSpaceName::LinearSRGB:
        ts << "Linear sRGB"_s;
        break;
    case ColorSpaceName::OKLCH:
        ts << "OKLCH"_s;
        break;
    case ColorSpaceName::OKLab:
        ts << "OKLab"_s;
        break;
    case ColorSpaceName::ProPhotoRGB:
        ts << "ProPhotoRGB"_s;
        break;
    case ColorSpaceName::Rec2020:
        ts << "Rec2020"_s;
        break;
    case ColorSpaceName::SRGB:
        ts << "sRGB"_s;
        break;
    case ColorSpaceName::XYZ_D50:
        ts << "XYZ-D50"_s;
        break;
    case ColorSpaceName::XYZ_D65:
        ts << "XYZ-D65"_s;
        break;
    }
    return ts;
}

}
