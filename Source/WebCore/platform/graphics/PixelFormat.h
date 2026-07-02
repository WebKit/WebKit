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

#pragma once

#include <WebCore/ContentsFormat.h>

#include <wtf/Forward.h>

namespace WebCore {

enum class PixelFormat : uint8_t {
    RGBA8,
    BGRX8,
    BGRA8,
#if ENABLE(PIXEL_FORMAT_RGB10)
    RGB10,
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    RGB10A8,
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    RGBA16F,
#endif
};

enum class UseLosslessCompression : bool { No, Yes };

constexpr ContentsFormat convertToContentsFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::RGBA8:
    case PixelFormat::BGRX8:
    case PixelFormat::BGRA8:
        return ContentsFormat::RGBA8;
#if ENABLE(PIXEL_FORMAT_RGB10)
    case PixelFormat::RGB10:
        return ContentsFormat::RGBA10;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case PixelFormat::RGB10A8:
        return ContentsFormat::RGBA10;
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
        return ContentsFormat::RGBA16F;
#endif
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return ContentsFormat::RGBA8;
    }
}

constexpr bool pixelFormatIsOpaque(PixelFormat format)
{
    switch (format) {
    case PixelFormat::BGRX8:
#if ENABLE(PIXEL_FORMAT_RGB10)
    case PixelFormat::RGB10:
#endif
        return true;
    case PixelFormat::RGBA8:
    case PixelFormat::BGRA8:
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case PixelFormat::RGB10A8:
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
#endif
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

WEBCORE_EXPORT TextStream& operator<<(TextStream&, PixelFormat);

}
