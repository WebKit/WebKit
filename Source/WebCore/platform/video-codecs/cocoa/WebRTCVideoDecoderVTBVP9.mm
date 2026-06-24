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
#import "WebRTCVideoDecoderVTBVP9.h"

#if USE(LIBWEBRTC)

#import "CMUtilities.h"
#import "VP9UtilitiesCocoa.h"
#import <wtf/BlockPtr.h>

#import "CoreVideoSoftLink.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebRTCVideoDecoderVTBVP9);

static RefPtr<VideoInfo> createVP9VideoInfoFromData(std::span<const uint8_t> data, int32_t width, int32_t height)
{
    auto parsedRecord = vpCodecConfigurationRecordFromVPXByteStream(VPXCodec::Vp9, data);
    if (!parsedRecord)
        return { };

    if (width)
        parsedRecord->frameWidth = width;
    if (height)
        parsedRecord->frameHeight = height;

    if (parsedRecord->colorPrimaries == VPConfigurationColorPrimaries::Unspecified && parsedRecord->transferCharacteristics == VPConfigurationTransferCharacteristics::Unspecified && parsedRecord->matrixCoefficients == VPConfigurationMatrixCoefficients::Unspecified) {
        parsedRecord->colorPrimaries = VPConfigurationColorPrimaries::BT_709_6;
        parsedRecord->transferCharacteristics = VPConfigurationTransferCharacteristics::BT_709_6;
        parsedRecord->matrixCoefficients = VPConfigurationMatrixCoefficients::BT_709_6;
    }

    return createVideoInfoFromVPCodecConfigurationRecord(*parsedRecord);
}

WebRTCVideoDecoderVTBVP9::WebRTCVideoDecoderVTBVP9(WebRTCVideoDecoderCallback callback, std::optional<PlatformVideoColorSpace>&& colorSpaceOverride)
    : WebRTCVideoDecoderVTB(callback, WTF::move(colorSpaceOverride))
{
}

int32_t WebRTCVideoDecoderVTBVP9::decodeFrame(int64_t timeStamp, std::span<const uint8_t> data)
{
    if (RefPtr videoInfo = createVP9VideoInfoFromData(data, width(), height()))
        setVideoInfo(videoInfo.releaseNonNull());

    return decodeFrameInternal(timeStamp, data);
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
