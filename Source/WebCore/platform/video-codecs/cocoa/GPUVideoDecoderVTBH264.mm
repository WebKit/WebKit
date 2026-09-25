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
#import "GPUVideoDecoderVTBH264.h"

#if USE(LIBWEBRTC)

#import "CMUtilities.h"
#import "H264Utilities.h"
#import "H264UtilitiesCocoa.h"
#import "Logging.h"
#import "TrackInfo.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GPUVideoDecoderVTBH264);

GPUVideoDecoderVTBH264::GPUVideoDecoderVTBH264(GPUVideoDecoderCallback callback, Ref<WorkQueue>&& queue, VideoDecoder::Config&& config)
    : GPUVideoDecoderVTB(callback, WTF::move(queue), WTF::move(config.colorSpace))
    , m_isAnnexB(config.useAnnexB)
{
    setFrameSize(config.width, config.height);

    if (config.description.isEmpty())
        return;

    RefPtr<VideoInfo> videoInfo = createVideoInfoFromAVCC(config.description.span());
    if (!videoInfo) {
        RELEASE_LOG_ERROR(WebRTC, "GPUVideoDecoderVTBH264: use default video info");
        // FIXME: We likely want to error this code path.
        videoInfo = VideoInfo::create({
            {
                .codecName = kCMVideoCodecType_H264
            }, {
                .size = { static_cast<float>(config.width), static_cast<float>(config.height) },
                .displaySize = { static_cast<float>(config.width), static_cast<float>(config.height) },
                .extensionAtoms = { FillWith { }, 1, { computeBoxType(kCMVideoCodecType_H264), SharedBuffer::create(config.description.span()) } },
            }
        });
    }

    setVideoInfo(videoInfo.releaseNonNull(), findAVCCMaxNumReorderFrames(config.description.span()).value_or(0));
}

int32_t GPUVideoDecoderVTBH264::decodeFrame(int64_t timeStamp, std::span<const uint8_t> data)
{
    if (!m_isAnnexB)
        return decodeFrameInternal(timeStamp, data);

    auto naluIndices = findNaluIndices(data);

    // FIXME: Skip rebuilding the VideoInfo when the SPS/PPS pair is unchanged from the previous one.
    if (RefPtr videoInfo = createVideoInfoFromAVCAnnexBStream(data, naluIndices))
        setVideoInfo(videoInfo.releaseNonNull(), findH264AnnexBMaxNumReorderFrames(data, naluIndices).value_or(0));

    auto lengthPrefixedData = convertAVCAnnexBToLengthPrefixed(data, naluIndices);
    return decodeFrameInternal(timeStamp, lengthPrefixedData.span());
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
