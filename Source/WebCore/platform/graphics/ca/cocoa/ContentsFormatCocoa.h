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

#if PLATFORM(COCOA)

#import "ContentsFormat.h"
#if HAVE(IOSURFACE)
#import "IOSurface.h"
#endif
#import <pal/spi/cocoa/QuartzCoreSPI.h>

namespace WebCore {

#if HAVE(IOSURFACE)
constexpr IOSurface::Format convertToIOSurfaceFormat(ContentsFormat contentsFormat)
{
    switch (contentsFormat) {
    case ContentsFormat::RGBA8:
        return IOSurface::Format::BGRA;
#if HAVE(IOSURFACE_RGB10)
    case ContentsFormat::RGBA10:
        return IOSurface::Format::RGB10;
#endif
#if HAVE(HDR_SUPPORT)
    case ContentsFormat::RGBA16F:
        return IOSurface::Format::RGBA16F;
#endif
    }
}
#endif

constexpr NSString *contentsFormatString(ContentsFormat contentsFormat)
{
    switch (contentsFormat) {
    case ContentsFormat::RGBA8:
        return nil;
#if HAVE(IOSURFACE_RGB10)
    case ContentsFormat::RGBA10:
        return kCAContentsFormatRGBA10XR;
#endif
#if HAVE(HDR_SUPPORT)
    case ContentsFormat::RGBA16F:
        return kCAContentsFormatRGBA16Float;
#endif
    }
}

constexpr bool contentsFormatWantsExtendedDynamicRangeContent(ContentsFormat contentsFormat)
{
    switch (contentsFormat) {
    case ContentsFormat::RGBA8:
        return false;
#if HAVE(IOSURFACE_RGB10)
    case ContentsFormat::RGBA10:
        return true;
#endif
#if HAVE(HDR_SUPPORT)
    case ContentsFormat::RGBA16F:
        return true;
#endif
    }
}

constexpr bool contentsFormatWantsToneMapMode(ContentsFormat contentsFormat)
{
    switch (contentsFormat) {
    case ContentsFormat::RGBA8:
        return false;
#if HAVE(IOSURFACE_RGB10)
    case ContentsFormat::RGBA10:
        return false;
#endif
#if HAVE(HDR_SUPPORT)
    case ContentsFormat::RGBA16F:
        return true;
#endif
    }
}

} // namespace WebCore

#endif // PLATFORM(COCOA)
