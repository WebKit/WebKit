/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 *
 */

#import "config.h"
#import "GPUVideoDecoderVTBH265.h"

#if USE(LIBWEBRTC)

#import "CMUtilities.h"
#import "HEVCUtilitiesCocoa.h"
#import "Logging.h"
#import "TrackInfo.h"
#import <wtf/BlockPtr.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GPUVideoDecoderVTBH265);

GPUVideoDecoderVTBH265::GPUVideoDecoderVTBH265(WebRTCVideoDecoderCallback callback, std::optional<PlatformVideoColorSpace>&& colorSpaceOverride)
    : WebRTCVideoDecoderVTB(callback, WTF::move(colorSpaceOverride))
{
}

int32_t GPUVideoDecoderVTBH265::decodeFrame(int64_t timeStamp, std::span<const uint8_t> data)
{
    if (!m_isAnnexBFormat)
        return decodeFrameInternal(timeStamp, data);

    if (RefPtr videoInfo = createVideoInfoFromHEVCAnnexBStream(data))
        setVideoInfo(videoInfo.releaseNonNull(), findHEVCAnnexBMaxNumReorderPics(data).value_or(0));

    auto lengthPrefixedData = convertHEVCAnnexBToLengthPrefixed(data);
    return decodeFrameInternal(timeStamp, lengthPrefixedData.span());
}

void GPUVideoDecoderVTBH265::setFormat(std::span<const uint8_t> data, uint16_t width, uint16_t height)
{
    setFrameSize(width, height);

    if (data.empty())
        return;

    // Receiving an explicit format description means the incoming frames are hvcC-style
    // (length-prefixed), not Annex B; stop looking for in-band VPS/SPS/PPS per frame.
    m_isAnnexBFormat = false;

    RefPtr videoInfo = createVideoInfoFromHVCC(data);
    if (!videoInfo) {
        RELEASE_LOG_ERROR(WebRTC, "GPUVideoDecoderVTBH265::setFormat use default video info");
        videoInfo = VideoInfo::create({
            {
                .codecName = kCMVideoCodecType_HEVC
            }, {
                .size = { static_cast<float>(width), static_cast<float>(height) },
                .displaySize = { static_cast<float>(width), static_cast<float>(height) },
                .extensionAtoms = { FillWith { }, 1, { computeBoxType(kCMVideoCodecType_HEVC), SharedBuffer::create(data) } },
            }
        });
    }

    setVideoInfo(videoInfo.releaseNonNull(), findHVCCMaxNumReorderPics(data).value_or(0));
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
