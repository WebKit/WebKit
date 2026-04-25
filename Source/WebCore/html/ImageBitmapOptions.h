/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "ImageOrientation.h"
#include <optional>

namespace WebCore {

struct ImageBitmapOptions {
    enum class Orientation { FromImage, FlipY,  None };
    enum class PremultiplyAlpha { None, Premultiply, Default };
    enum class ColorSpaceConversion { None, Default };
    enum class ResizeQuality { Pixelated, Low, Medium, High };

    Orientation orientation { Orientation::FromImage };
    PremultiplyAlpha premultiplyAlpha { PremultiplyAlpha::Default };
    ColorSpaceConversion colorSpaceConversion { ColorSpaceConversion::Default };
    std::optional<unsigned> resizeWidth;
    std::optional<unsigned> resizeHeight;
    ResizeQuality resizeQuality { ResizeQuality::Low };

    ImageOrientation resolvedImageOrientation(ImageOrientation imageOrientation) const
    {
        return orientation == Orientation::FlipY ? imageOrientation.withFlippedY() : imageOrientation;
    }
};

}
