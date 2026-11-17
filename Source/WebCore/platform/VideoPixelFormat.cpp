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
#include "PlanarImageConverterCV.h"
#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"
#endif

namespace WebCore {

#if PLATFORM(COCOA)
static std::optional<VideoPixelFormat> planarYUVVideoPixelFormat(PlanarImageConverter::ChromaSubsampling subsampling, uint8_t bitDepth, bool hasAlpha)
{
    if (subsampling == PlanarImageConverter::ChromaSubsampling::HorizontalAndVertical && bitDepth == 8 && hasAlpha)
        return VideoPixelFormat::I420A;

    static constexpr std::array formats {
        VideoPixelFormat::I420, VideoPixelFormat::I420P10, VideoPixelFormat::I420P12, VideoPixelFormat::I420AP10, VideoPixelFormat::I420AP12,
        VideoPixelFormat::I422, VideoPixelFormat::I422P10, VideoPixelFormat::I422P12, VideoPixelFormat::I422A, VideoPixelFormat::I422AP10, VideoPixelFormat::I422AP12,
        VideoPixelFormat::I444, VideoPixelFormat::I444P10, VideoPixelFormat::I444P12, VideoPixelFormat::I444A, VideoPixelFormat::I444AP10, VideoPixelFormat::I444AP12,
    };
    for (auto format : formats) {
        auto layout = planarYUVFormatLayout(format);
        if (layout->subsampling == subsampling && layout->format.bitDepth == bitDepth && layout->hasAlpha == hasAlpha)
            return format;
    }
    return { };
}
#endif

std::optional<VideoPixelFormat> convertVideoFramePixelFormat(uint32_t format, bool shouldDiscardAlpha)
{
#if PLATFORM(COCOA)
    if (format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange || format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange || format == kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarVideoRange)
        return VideoPixelFormat::NV12;
    if (format == kCVPixelFormatType_32BGRA)
        return shouldDiscardAlpha ? VideoPixelFormat::BGRX : VideoPixelFormat::BGRA;
    if (format == kCVPixelFormatType_32ARGB)
        return shouldDiscardAlpha ? VideoPixelFormat::RGBX : VideoPixelFormat::RGBA;
    if (auto description = PlanarImageConverter::pixelFormatDescription(format))
        return planarYUVVideoPixelFormat(description->subsampling, description->color.bitDepth, description->alpha && !shouldDiscardAlpha);
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
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    case GST_VIDEO_FORMAT_I420_10LE:
        return VideoPixelFormat::I420P10;
    case GST_VIDEO_FORMAT_I420_12LE:
        return VideoPixelFormat::I420P12;
    case GST_VIDEO_FORMAT_I422_10LE:
        return VideoPixelFormat::I422P10;
    case GST_VIDEO_FORMAT_I422_12LE:
        return VideoPixelFormat::I422P12;
    case GST_VIDEO_FORMAT_Y444_10LE:
        return VideoPixelFormat::I444P10;
    case GST_VIDEO_FORMAT_Y444_12LE:
        return VideoPixelFormat::I444P12;
#else
    case GST_VIDEO_FORMAT_I420_10BE:
        return VideoPixelFormat::I420P10;
    case GST_VIDEO_FORMAT_I420_12BE:
        return VideoPixelFormat::I420P12;
    case GST_VIDEO_FORMAT_I422_10BE:
        return VideoPixelFormat::I422P10;
    case GST_VIDEO_FORMAT_I422_12BE:
        return VideoPixelFormat::I422P12;
    case GST_VIDEO_FORMAT_Y444_10BE:
        return VideoPixelFormat::I444P10;
    case GST_VIDEO_FORMAT_Y444_12BE:
        return VideoPixelFormat::I444P12;
#endif
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
    static const std::array<NeverDestroyed<String>, 23> values {
        MAKE_STATIC_STRING_IMPL("I420"),
        MAKE_STATIC_STRING_IMPL("I420P10"),
        MAKE_STATIC_STRING_IMPL("I420P12"),
        MAKE_STATIC_STRING_IMPL("I420A"),
        MAKE_STATIC_STRING_IMPL("I420AP10"),
        MAKE_STATIC_STRING_IMPL("I420AP12"),
        MAKE_STATIC_STRING_IMPL("I422"),
        MAKE_STATIC_STRING_IMPL("I422P10"),
        MAKE_STATIC_STRING_IMPL("I422P12"),
        MAKE_STATIC_STRING_IMPL("I422A"),
        MAKE_STATIC_STRING_IMPL("I422AP10"),
        MAKE_STATIC_STRING_IMPL("I422AP12"),
        MAKE_STATIC_STRING_IMPL("I444"),
        MAKE_STATIC_STRING_IMPL("I444P10"),
        MAKE_STATIC_STRING_IMPL("I444P12"),
        MAKE_STATIC_STRING_IMPL("I444A"),
        MAKE_STATIC_STRING_IMPL("I444AP10"),
        MAKE_STATIC_STRING_IMPL("I444AP12"),
        MAKE_STATIC_STRING_IMPL("NV12"),
        MAKE_STATIC_STRING_IMPL("RGBA"),
        MAKE_STATIC_STRING_IMPL("RGBX"),
        MAKE_STATIC_STRING_IMPL("BGRA"),
        MAKE_STATIC_STRING_IMPL("BGRX"),
    };
    static_assert(!static_cast<size_t>(VideoPixelFormat::I420), "VideoPixelFormat::I420 is not 0 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I420P10) == 1, "VideoPixelFormat::I420P10 is not 1 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I420P12) == 2, "VideoPixelFormat::I420P12 is not 2 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I420A) == 3, "VideoPixelFormat::I420A is not 3 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I420AP10) == 4, "VideoPixelFormat::I420AP10 is not 4 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I420AP12) == 5, "VideoPixelFormat::I420AP12 is not 5 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I422) == 6, "VideoPixelFormat::I422 is not 6 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I422P10) == 7, "VideoPixelFormat::I422P10 is not 7 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I422P12) == 8, "VideoPixelFormat::I422P12 is not 8 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I422A) == 9, "VideoPixelFormat::I422A is not 9 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I422AP10) == 10, "VideoPixelFormat::I422AP10 is not 10 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I422AP12) == 11, "VideoPixelFormat::I422AP12 is not 11 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I444) == 12, "VideoPixelFormat::I444 is not 12 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I444P10) == 13, "VideoPixelFormat::I444P10 is not 13 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I444P12) == 14, "VideoPixelFormat::I444P12 is not 14 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I444A) == 15, "VideoPixelFormat::I444A is not 15 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I444AP10) == 16, "VideoPixelFormat::I444AP10 is not 16 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::I444AP12) == 17, "VideoPixelFormat::I444AP12 is not 17 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::NV12) == 18, "VideoPixelFormat::NV12 is not 18 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::RGBA) == 19, "VideoPixelFormat::RGBA is not 19 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::RGBX) == 20, "VideoPixelFormat::RGBX is not 20 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::BGRA) == 21, "VideoPixelFormat::BGRA is not 21 as expected");
    static_assert(static_cast<size_t>(VideoPixelFormat::BGRX) == 22, "VideoPixelFormat::BGRX is not 22 as expected");
    ASSERT(static_cast<size_t>(format) < std::size(values));
    return values[static_cast<size_t>(format)];
}

} // namespace WebCore

