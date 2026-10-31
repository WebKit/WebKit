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
#include "ImageDrawDescriptor.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

RasterDrawingOptions ImageDrawDescriptor::rasterDrawingOptions() const
{
    return {
        compositeOperator,
        blendMode,
        orientation,
        rendition.interpolationQuality,
#if ENABLE(AX_CUSTOM_COLOR_MODE)
        rendition.invertContent,
#endif
#if USE(SKIA)
        strictImageClamping,
#endif
        rendition.drawsHDRContent,
        rendition.headroom,
        rendition.dynamicRangeLimit
    };
}

TextStream& operator<<(TextStream& ts, const ImageDrawDescriptor& descriptor)
{
    ts.dumpProperty("composite-operator"_s, descriptor.compositeOperator);
    ts.dumpProperty("blend-mode"_s, descriptor.blendMode);
    ts.dumpProperty("decoding-mode"_s, descriptor.decodingMode);
    ts.dumpProperty("orientation"_s, descriptor.orientation.orientation());
    ts.dumpProperty("interpolation-quality"_s, descriptor.rendition.interpolationQuality);
    return ts;
}

} // namespace WebCore
