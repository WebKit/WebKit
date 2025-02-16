/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "ContentsFormat.h"

#if USE(CG)
#include "ColorSpaceCG.h"
#endif
#include "DestinationColorSpace.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

std::optional<DestinationColorSpace> contentsFormatExtendedColorSpace(ContentsFormat contentsFormat)
{
    switch (contentsFormat) {
    case ContentsFormat::RGBA8:
        return std::nullopt;
#if ENABLE(PIXEL_FORMAT_RGB10) && ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_SRGB)
    case ContentsFormat::RGBA10:
        return DestinationColorSpace::ExtendedSRGB();
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F) && ENABLE(DESTINATION_COLOR_SPACE_EXTENDED_REC_2020)
    case ContentsFormat::RGBA16F:
        return DestinationColorSpace::ExtendedRec2020();
#endif
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

TextStream& operator<<(TextStream& ts, ContentsFormat contentsFormat)
{
    switch (contentsFormat) {
    case ContentsFormat::RGBA8:
        ts << "RGBA8";
        break;
#if ENABLE(PIXEL_FORMAT_RGB10)
    case ContentsFormat::RGBA10:
        ts << "RGBA10";
        break;
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case ContentsFormat::RGBA16F:
        ts << "RGBA16F";
        break;
#endif
    }
    return ts;
}

} // namespace WebCore
