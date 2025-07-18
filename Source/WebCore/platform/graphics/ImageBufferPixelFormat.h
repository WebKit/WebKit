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

#include "ContentsFormat.h"
#if HAVE(IOSURFACE)
#include "IOSurface.h"
#endif
#include "PixelFormat.h"

#include <wtf/Forward.h>

namespace WebCore {
enum class ImageBufferPixelFormat : uint8_t {
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

constexpr PixelFormat convertToPixelFormat(ImageBufferPixelFormat format)
{
    switch (format) {
    case ImageBufferPixelFormat::BGRX8:
        return PixelFormat::BGRX8;
    case ImageBufferPixelFormat::BGRA8:
        return PixelFormat::BGRA8;
#if ENABLE(PIXEL_FORMAT_RGB10)
    case ImageBufferPixelFormat::RGB10:
        return PixelFormat::RGB10;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case ImageBufferPixelFormat::RGB10A8:
        return PixelFormat::RGB10A8;
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case ImageBufferPixelFormat::RGBA16F:
        return PixelFormat::RGBA16F;
#endif
    }

    ASSERT_NOT_REACHED();
    return PixelFormat::BGRX8;
}

constexpr ContentsFormat convertToContentsFormat(ImageBufferPixelFormat format)
{
    switch (format) {
    case ImageBufferPixelFormat::BGRX8:
    case ImageBufferPixelFormat::BGRA8:
        return ContentsFormat::RGBA8;
#if ENABLE(PIXEL_FORMAT_RGB10)
    case ImageBufferPixelFormat::RGB10:
        return ContentsFormat::RGBA10;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case ImageBufferPixelFormat::RGB10A8:
        return ContentsFormat::RGBA10;
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case ImageBufferPixelFormat::RGBA16F:
        return ContentsFormat::RGBA16F;
#endif
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return ContentsFormat::RGBA8;
    }
}

#if HAVE(IOSURFACE)
constexpr IOSurface::Format convertToIOSurfaceFormat(ImageBufferPixelFormat format)
{
    switch (format) {
    case ImageBufferPixelFormat::BGRX8:
        return IOSurface::Format::BGRX;
    case ImageBufferPixelFormat::BGRA8:
        return IOSurface::Format::BGRA;
#if ENABLE(PIXEL_FORMAT_RGB10)
    case ImageBufferPixelFormat::RGB10:
        return IOSurface::Format::RGB10;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case ImageBufferPixelFormat::RGB10A8:
        return IOSurface::Format::RGB10A8;
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case ImageBufferPixelFormat::RGBA16F:
        return IOSurface::Format::RGBA16F;
#endif
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return IOSurface::Format::BGRA;
    }
}
#endif

constexpr bool imageBufferPixelFormatIsOpaque(ImageBufferPixelFormat format)
{
    switch (format) {
    case ImageBufferPixelFormat::BGRX8:
#if ENABLE(PIXEL_FORMAT_RGB10)
    case ImageBufferPixelFormat::RGB10:
#endif
        return true;
    case ImageBufferPixelFormat::BGRA8:
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case ImageBufferPixelFormat::RGB10A8:
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case ImageBufferPixelFormat::RGBA16F:
#endif
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore
