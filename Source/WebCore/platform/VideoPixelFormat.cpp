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

#if USE(GSTREAMER)
#include <gst/video/video-format.h>
#endif

#if PLATFORM(COCOA)
#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"
#endif

namespace WebCore {

VideoPixelFormat convertVideoFramePixelFormat(uint32_t format, bool shouldDiscardAlpha)
{
#if PLATFORM(COCOA)
    if (format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
        return VideoPixelFormat::NV12;
    if (format == kCVPixelFormatType_32BGRA)
        return shouldDiscardAlpha ? VideoPixelFormat::BGRX : VideoPixelFormat::BGRA;
    if (format == kCVPixelFormatType_32ARGB)
        return shouldDiscardAlpha ? VideoPixelFormat::RGBX : VideoPixelFormat::RGBA;
    ASSERT_NOT_REACHED();
#elif USE(GSTREAMER)
    switch (format) {
    case GST_VIDEO_FORMAT_I420:
        return VideoPixelFormat::I420;
    case GST_VIDEO_FORMAT_A420:
        return VideoPixelFormat::I420A;
    case GST_VIDEO_FORMAT_I422_10BE:
    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_I422_12BE:
    case GST_VIDEO_FORMAT_I422_12LE:
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
    return VideoPixelFormat::I420;
}

} // namespace WebCore

