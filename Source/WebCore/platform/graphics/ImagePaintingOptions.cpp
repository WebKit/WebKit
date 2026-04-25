/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImagePaintingOptions.h"

#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, ImagePaintingOptions options)
{
    ts.dumpProperty("composite-operator"_s, options.compositeOperator());
    ts.dumpProperty("blend-mode"_s, options.blendMode());
    ts.dumpProperty("decoding-mode"_s, options.decodingMode());
    ts.dumpProperty("orientation"_s, options.orientation().orientation());
    ts.dumpProperty("interpolation-quality"_s, options.interpolationQuality());
    return ts;
}

TextStream& operator<<(TextStream& ts, DecodingMode mode)
{
    switch (mode) {
    case DecodingMode::Auto:
        ts << "auto"_s;
        break;
    case DecodingMode::Synchronous:
        ts << "synchronous"_s;
        break;
    case DecodingMode::Asynchronous:
        ts << "asynchronous"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ImageOrientation::Orientation orientation)
{
    using Orientation = ImageOrientation::Orientation;
    switch (orientation) {
    case Orientation::FromImage:
        ts << "from-image"_s;
        break;
    case Orientation::OriginTopLeft:
        ts << "origin-top-left"_s;
        break;
    case Orientation::OriginTopRight:
        ts << "origin-bottom-right"_s;
        break;
    case Orientation::OriginBottomRight:
        ts << "origin-top-right"_s;
        break;
    case Orientation::OriginBottomLeft:
        ts << "origin-top-left"_s;
        break;
    case Orientation::OriginLeftTop:
        ts << "origin-left-bottom"_s;
        break;
    case Orientation::OriginRightTop:
        ts << "origin-right-bottom"_s;
        break;
    case Orientation::OriginRightBottom:
        ts << "origin-right-top"_s;
        break;
    case Orientation::OriginLeftBottom:
        ts << "origin-left-top"_s;
        break;
    }
    return ts;
}

}
