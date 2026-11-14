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

#pragma once

#include <optional>
#include <wtf/Forward.h>

namespace WebCore {

enum class VideoPixelFormat {
    I420,
    I420P10,
    I420P12,
    I420A,
    I422,
    I422P10,
    I422P12,
    I444,
    I444P10,
    I444P12,
    NV12,
    RGBA,
    RGBX,
    BGRA,
    BGRX
};

NODELETE std::optional<VideoPixelFormat> convertVideoFramePixelFormat(uint32_t, bool shouldDiscardAlpha = false);

inline bool isRGBVideoPixelFormat(VideoPixelFormat format)
{
    return format == VideoPixelFormat::RGBA || format == VideoPixelFormat::RGBX || format == VideoPixelFormat::BGRA || format == VideoPixelFormat::BGRX;
}

inline bool isHighBitDepthVideoPixelFormat(VideoPixelFormat format)
{
    return format == VideoPixelFormat::I420P10 || format == VideoPixelFormat::I420P12
        || format == VideoPixelFormat::I422P10 || format == VideoPixelFormat::I422P12
        || format == VideoPixelFormat::I444P10 || format == VideoPixelFormat::I444P12;
}

struct PlanarYUVFormatLayout {
    unsigned bitDepth { 8 };
    bool isChromaHorizontallySubsampled { false };
    bool isChromaVerticallySubsampled { false };
};

// The three plane Y'CbCr formats without alpha. Samples above 8 bits are stored in 16 bits, LSB aligned.
inline std::optional<PlanarYUVFormatLayout> planarYUVFormatLayout(VideoPixelFormat format)
{
    switch (format) {
    case VideoPixelFormat::I420:
        return PlanarYUVFormatLayout { 8, true, true };
    case VideoPixelFormat::I420P10:
        return PlanarYUVFormatLayout { 10, true, true };
    case VideoPixelFormat::I420P12:
        return PlanarYUVFormatLayout { 12, true, true };
    case VideoPixelFormat::I422:
        return PlanarYUVFormatLayout { 8, true, false };
    case VideoPixelFormat::I422P10:
        return PlanarYUVFormatLayout { 10, true, false };
    case VideoPixelFormat::I422P12:
        return PlanarYUVFormatLayout { 12, true, false };
    case VideoPixelFormat::I444:
        return PlanarYUVFormatLayout { 8, false, false };
    case VideoPixelFormat::I444P10:
        return PlanarYUVFormatLayout { 10, false, false };
    case VideoPixelFormat::I444P12:
        return PlanarYUVFormatLayout { 12, false, false };
    case VideoPixelFormat::I420A:
    case VideoPixelFormat::NV12:
    case VideoPixelFormat::RGBA:
    case VideoPixelFormat::RGBX:
    case VideoPixelFormat::BGRA:
    case VideoPixelFormat::BGRX:
        break;
    }
    return { };
}

WEBCORE_EXPORT String convertVideoPixelFormatToString(VideoPixelFormat);

} // namespace WebCore
