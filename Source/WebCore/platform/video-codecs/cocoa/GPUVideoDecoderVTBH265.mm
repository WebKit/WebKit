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
#import "HEVCUtilities.h"
#import "HEVCUtilitiesCocoa.h"
#import "Logging.h"
#import "TrackInfo.h"
#import <wtf/BlockPtr.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GPUVideoDecoderVTBH265);

GPUVideoDecoderVTBH265::GPUVideoDecoderVTBH265(GPUVideoDecoderCallback callback, Ref<WorkQueue>&& queue, std::optional<PlatformVideoColorSpace>&& colorSpaceOverride)
    : GPUVideoDecoderVTB(callback, WTF::move(queue), WTF::move(colorSpaceOverride))
{
}

int32_t GPUVideoDecoderVTBH265::decodeFrame(int64_t timeStamp, std::span<const uint8_t> data)
{
    if (!m_isAnnexB)
        return decodeFrameInternal(timeStamp, data);

    auto naluIndices = findNaluIndices(data);

    // FIXME: Skip rebuilding the VideoInfo when the VPS/SPS/PPS triplet is unchanged from the previous one.
    if (RefPtr videoInfo = createVideoInfoFromHEVCAnnexBStream(data, naluIndices))
        setVideoInfo(videoInfo.releaseNonNull(), findHEVCAnnexBMaxNumReorderPics(data, naluIndices).value_or(0));

    auto lengthPrefixedData = convertHEVCAnnexBToLengthPrefixed(data, naluIndices);
    return decodeFrameInternal(timeStamp, lengthPrefixedData.span());
}

void GPUVideoDecoderVTBH265::setFormat(std::span<const uint8_t> data, uint16_t width, uint16_t height)
{
    // FIXME: We should provide this info at decoder construction time.
    setFrameSize(width, height);

    if (data.empty())
        return;

    // Receiving an explicit format description means the incoming frames are hvcC-style.
    m_isAnnexB = false;

    auto parameterSets = parseHVCCParameterSets(data);
    RELEASE_LOG_ERROR_IF(parameterSets, WebRTC, "Unable to correctly parse the hvcC data");

    RefPtr<VideoInfo> videoInfo;
    if (parameterSets)
        videoInfo = createVideoInfoFromHVCC(*parameterSets);
    if (!videoInfo) {
        RELEASE_LOG_ERROR_IF(parameterSets, WebRTC, "Unable to create video info from hvcC data");
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

    setVideoInfo(videoInfo.releaseNonNull(), parameterSets ? findHVCCMaxNumReorderPics(*parameterSets).value_or(0) : 0);
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
