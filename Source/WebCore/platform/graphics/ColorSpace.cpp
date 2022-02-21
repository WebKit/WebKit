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
        ts << "A98-RGB";
        break;
    case ColorSpace::DisplayP3:
        ts << "DisplayP3";
        break;
    case ColorSpace::ExtendedA98RGB:
        ts << "Extended A98-RGB";
        break;
    case ColorSpace::ExtendedDisplayP3:
        ts << "Extended DisplayP3";
        break;
    case ColorSpace::ExtendedLinearSRGB:
        ts << "Extended Linear sRGB";
        break;
    case ColorSpace::ExtendedProPhotoRGB:
        ts << "Extended ProPhotoRGB";
        break;
    case ColorSpace::ExtendedRec2020:
        ts << "Extended Rec2020";
        break;
    case ColorSpace::ExtendedSRGB:
        ts << "Extended sRGB";
        break;
    case ColorSpace::HSL:
        ts << "HSL";
        break;
    case ColorSpace::HWB:
        ts << "HWB";
        break;
    case ColorSpace::LCH:
        ts << "LCH";
        break;
    case ColorSpace::Lab:
        ts << "Lab";
        break;
    case ColorSpace::LinearSRGB:
        ts << "Linear sRGB";
        break;
    case ColorSpace::OKLCH:
        ts << "OKLCH";
        break;
    case ColorSpace::OKLab:
        ts << "OKLab";
        break;
    case ColorSpace::ProPhotoRGB:
        ts << "ProPhotoRGB";
        break;
    case ColorSpace::Rec2020:
        ts << "Rec2020";
        break;
    case ColorSpace::SRGB:
        ts << "sRGB";
        break;
    case ColorSpace::XYZ_D50:
        ts << "XYZ-D50";
        break;
    case ColorSpace::XYZ_D65:
        ts << "XYZ-D50";
        break;
    }
    return ts;
}

}
