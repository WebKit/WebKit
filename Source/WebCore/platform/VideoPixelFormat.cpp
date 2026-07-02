/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "VideoPixelFormat.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

#if USE(GSTREAMER)
#include <gst/video/video-format.h>
#endif

#if PLATFORM(COCOA)
#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"
#endif

namespace WebCore {

std::optional<VideoPixelFormat> convertVideoFramePixelFormat(uint32_t format, bool shouldDiscardAlpha)
{
#if PLATFORM(COCOA)
    if (format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange || format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange || format == kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarVideoRange)
        return VideoPixelFormat::NV12;
    if (format == kCVPixelFormatType_32BGRA)
        return shouldDiscardAlpha ? VideoPixelFormat::BGRX : VideoPixelFormat::BGRA;
    if (format == kCVPixelFormatType_32ARGB)
        return shouldDiscardAlpha ? VideoPixelFormat::RGBX : VideoPixelFormat::RGBA;
#elif USE(GSTREAMER)
    switch (format) {
    case GST_VIDEO_FORMAT_I420:
        return VideoPixelFormat::I420;
    case GST_VIDEO_FORMAT_A420:
        return VideoPixelFormat::I420A;
    case GST_VIDEO_FORMAT_Y42B:
        return VideoPixelFormat::I422;
    case GST_VIDEO_FORMAT_Y444:
        return VideoPixelFormat::I444;
    case GST_VIDEO_FORMAT_NV12:
        return VideoPixelFormat::NV12;
    case GST_VIDEO_FORMAT_RGBA:
        return shouldDiscardAlpha ? VideoPixelFormat::RGBX : VideoPixelFormat::RGBA;
    case GST_VIDEO_FORMAT_RGBx:
        return VideoPixelFormat::RGBX;
    case GST_VIDEO_FORMAT_BGRA:
        return shouldDiscardAlpha ? VideoPixelFormat::BGRX : VideoPixelFormat::BGRA;
    case GST_VIDEO_FORMAT_ARGB:
        return shouldDiscardAlpha ? VideoPixelFormat::RGBX : VideoPixelFormat::RGBA;
    case GST_VIDEO_FORMAT_BGRx:
        return VideoPixelFormat::BGRX;
    default:
        break;
    }
#else
    UNUSED_PARAM(format);
    UNUSED_PARAM(shouldDiscardAlpha);
#endif
    return { };
}

String convertVideoPixelFormatToString(VideoPixelFormat format)
{
    static const std::array<NeverDestroyed<String>, 9> values {
        MAKE_STATIC_STRING_IMPL("I420"),
        MAKE_STATIC_STRING_IMPL("I420A"),
        MAKE_STATIC_STRING_IMPL("I422"),
        MAKE_STATIC_STRING_IMPL("I444"),
        MAKE_STATIC_STRING_IMPL("NV12"),
        MAKE_STATIC_STRING_IMPL("RGBA"),
        MAKE_STATIC_STRING_IMPL("RGBX"),
        MAKE_STATIC_STRING_IMPL("BGRA"),
        MAKE_STATIC_STRING_IMPL("BGRX"),
    };
    static_assert(!static_cast<size_t>(VideoPixelFormat::I420), "VideoPixelFormat::I420 is not 0 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I420A) == 1, "VideoPixelFormat::I420A is not 1 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I422) == 2, "VideoPixelFormat::I422 is not 2 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I444) == 3, "VideoPixelFormat::I444 is not 3 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::NV12) == 4, "VideoPixelFormat::NV12 is not 4 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::RGBA) == 5, "VideoPixelFormat::RGBA is not 5 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::RGBX) == 6, "VideoPixelFormat::RGBX is not 6 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::BGRA) == 7, "VideoPixelFormat::BGRA is not 7 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::BGRX) == 8, "VideoPixelFormat::BGRX is not 8 as expected");
    ASSERT(static_cast<size_t>(format) < std::size(values));
    return values[static_cast<size_t>(format)];
}

} // namespace WebCore

