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
 */

#include "config.h"
#include "GPUVideoEncoder.h"

#import "GPUVideoEncoderVTBH264.h"
#import "GPUVideoEncoderVTBH265.h"
#import <wtf/BlockPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>

#if USE(LIBWEBRTC)
#include "LibWebRTCMacros.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/webkit_sdk/WebKit/WebKitEncoder.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {

#if USE(LIBWEBRTC)
static inline webrtc::VideoRotation toWebRTCVideoRotation(VideoFrame::Rotation rotation)
{
    switch (rotation) {
    case VideoFrame::Rotation::None:
        return webrtc::kVideoRotation_0;
    case VideoFrame::Rotation::UpsideDown:
        return webrtc::kVideoRotation_180;
    case VideoFrame::Rotation::Right:
        return webrtc::kVideoRotation_90;
    case VideoFrame::Rotation::Left:
        return webrtc::kVideoRotation_270;
    }
    ASSERT_NOT_REACHED();
    return webrtc::kVideoRotation_0;
}

static inline VideoFrame::Rotation fromWebRTCEncodedVideoRotation(webrtc::WebKitEncodedVideoRotation rotation)
{
    switch (rotation) {
    case webrtc::WebKitEncodedVideoRotation::kVideoRotation_0:
        return VideoFrame::Rotation::None;
    case webrtc::WebKitEncodedVideoRotation::kVideoRotation_180:
        return VideoFrame::Rotation::UpsideDown;
    case webrtc::WebKitEncodedVideoRotation::kVideoRotation_90:
        return VideoFrame::Rotation::Right;
    case webrtc::WebKitEncodedVideoRotation::kVideoRotation_270:
        return VideoFrame::Rotation::Left;
    }
    ASSERT_NOT_REACHED();
    return VideoFrame::Rotation::None;
}

static inline GPUVideoEncoderFrameInfo toGPUVideoEncoderFrameInfo(const webrtc::WebKitEncodedFrameInfo& info)
{
    return {
        .width = info.width,
        .height = info.height,
        .timeStamp = info.timeStamp,
        .duration = info.duration,
        .captureTimeMS = info.captureTimeMS,
        .isKeyFrame = info.frameType == webrtc::VideoFrameType::kVideoFrameKey,
        .rotation = fromWebRTCEncodedVideoRotation(info.rotation),
        .isScreenshare = info.contentType == webrtc::VideoContentType::SCREENSHARE,
        .qp = info.qp,
        .temporalIndex = info.temporalIndex >= 0 ? std::make_optional<uint8_t>(info.temporalIndex) : std::nullopt
    };
}

class GPULocalVideoEncoder final : public GPUVideoEncoder {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(GPULocalVideoEncoder);
public:
    static Ref<GPULocalVideoEncoder> create(webrtc::LocalEncoder encoder) { return adoptRef(*new GPULocalVideoEncoder(encoder)); }

    ~GPULocalVideoEncoder()
    {
        webrtc::releaseLocalEncoder(m_encoder);
    }

private:
    explicit GPULocalVideoEncoder(webrtc::LocalEncoder encoder)
        : m_encoder(encoder)
    {
    }

    void initialize(uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate) final { webrtc::initializeLocalEncoder(m_encoder, width, height, startBitrate, maxBitrate, minBitrate, maxFramerate); }
    void encodeFrame(CVPixelBufferRef pixelBuffer, int64_t timeStampNs, int64_t timeStamp, std::optional<uint64_t> duration, VideoFrame::Rotation rotation, bool isKeyframeRequired) final { webrtc::encodeLocalEncoderFrame(m_encoder, pixelBuffer, timeStampNs, timeStamp, duration, toWebRTCVideoRotation(rotation), isKeyframeRequired); }
    void setRates(uint32_t bitRate, uint32_t frameRate) final { webrtc::setLocalEncoderRates(m_encoder, bitRate, frameRate); }
    void flush() final { webrtc::flushLocalEncoder(m_encoder); }

    webrtc::LocalEncoder m_encoder;
};
#endif

RefPtr<GPUVideoEncoder> GPUVideoEncoder::create(CreationInfo&& creationInfo, const Vector<std::pair<String, String>>& parameters, GPUVideoEncoderCallback&& callback, GPUVideoEncoderDescriptionCallback&& descriptionCallback, GPUVideoEncoderErrorCallback&& errorCallback)
{
    if (creationInfo.useWebCoreEncoder) {
#if USE(AVFOUNDATION)
        if (creationInfo.codecType == VideoCodecType::H265) {
            // We only support L1T1 for H265.
            creationInfo.scalabilityMode = VideoEncoderScalabilityMode::L1T1;
            return GPUVideoEncoderVTBH265::create(WTF::move(creationInfo), WTF::move(callback), WTF::move(descriptionCallback), WTF::move(errorCallback));
        }

        if (creationInfo.codecType == VideoCodecType::H264) {
            if (creationInfo.scalabilityMode == VideoEncoderScalabilityMode::L1T3)
                return nullptr;
            return GPUVideoEncoderVTBH264::create(WTF::move(creationInfo), parameters, WTF::move(callback), WTF::move(descriptionCallback), WTF::move(errorCallback));
        }
        return nullptr;
#endif
    }

#if USE(LIBWEBRTC)
    ASSERT(creationInfo.codecType == VideoCodecType::H264 || creationInfo.codecType == VideoCodecType::H265);

    std::map<std::string, std::string> rtcParameters;
    for (auto& parameter : parameters)
        rtcParameters.emplace(parameter.first.utf8().legacyCStringPointer(), parameter.second.utf8().legacyCStringPointer());

    webrtc::LocalEncoderScalabilityMode rtcScalabilityMode;
    switch (creationInfo.scalabilityMode) {
    case VideoEncoderScalabilityMode::L1T1:
        rtcScalabilityMode = webrtc::LocalEncoderScalabilityMode::L1T1;
        break;
    case VideoEncoderScalabilityMode::L1T2:
        rtcScalabilityMode = webrtc::LocalEncoderScalabilityMode::L1T2;
        break;
    case VideoEncoderScalabilityMode::L1T3:
        return nullptr;
    }

    auto newFrameBlock = makeBlockPtr([callback = WTF::move(callback)](const uint8_t* buffer, size_t size, const webrtc::WebKitEncodedFrameInfo& info) {
        callback(unsafeMakeSpan(buffer, size), toGPUVideoEncoderFrameInfo(info));
    });
    auto newConfigurationBlock = makeBlockPtr([descriptionCallback = WTF::move(descriptionCallback)](const uint8_t* buffer, size_t size) {
        // This backend has no way to report the color space it actually encoded with, so we report a fixed default.
        PlatformVideoColorSpace colorSpace {
            .primaries = PlatformVideoColorPrimaries::Bt709,
            .transfer = PlatformVideoTransferCharacteristics::Iec6196621,
            .matrix = PlatformVideoMatrixCoefficients::Bt709,
            .fullRange = true
        };
        descriptionCallback(unsafeMakeSpan(buffer, size), colorSpace);
    });
    auto errorBlock = makeBlockPtr([errorCallback = WTF::move(errorCallback)](bool isFrameDropped) {
        errorCallback(isFrameDropped);
    });

    auto* encoder = webrtc::createLocalEncoder(webrtc::SdpVideoFormat { creationInfo.codecType == VideoCodecType::H264 ? "H264" : "H265", rtcParameters }, creationInfo.useAnnexB, rtcScalabilityMode, newFrameBlock.get(), newConfigurationBlock.get(), errorBlock.get());
    if (!encoder)
        return nullptr;

    webrtc::setLocalEncoderLowLatency(encoder, creationInfo.isLowLatencyEnabled);

    return GPULocalVideoEncoder::create(encoder);
#else
    UNUSED_PARAM(creationInfo);
    UNUSED_PARAM(parameters);
    UNUSED_PARAM(callback);
    UNUSED_PARAM(descriptionCallback);
    UNUSED_PARAM(errorCallback);
    return nullptr;
#endif
}

}
