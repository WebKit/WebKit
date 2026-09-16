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
#include "WebRTCVideoEncoder.h"

#import "WebRTCVideoEncoderVTBH265.h"
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

static inline WebRTCVideoEncoderFrameInfo toWebRTCVideoEncoderFrameInfo(const webrtc::WebKitEncodedFrameInfo& info)
{
    return {
        info.width,
        info.height,
        info.timeStamp,
        info.duration,
        info.captureTimeMS,
        info.frameType == webrtc::VideoFrameType::kVideoFrameKey,
        fromWebRTCEncodedVideoRotation(info.rotation),
        info.contentType == webrtc::VideoContentType::SCREENSHARE,
        info.qp,
        info.temporalIndex >= 0 ? std::make_optional<uint8_t>(info.temporalIndex) : std::nullopt
    };
}

class WebRTCLocalVideoEncoder final : public WebRTCVideoEncoder {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WebRTCLocalVideoEncoder);
public:
    WebRTCLocalVideoEncoder(webrtc::LocalEncoder encoder)
        : m_encoder(encoder)
    {
    }

    ~WebRTCLocalVideoEncoder()
    {
        webrtc::releaseLocalEncoder(m_encoder);
    }

private:
    void setLowLatency(bool enabled) final { webrtc::setLocalEncoderLowLatency(m_encoder, enabled); }
    void initialize(uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate) final { webrtc::initializeLocalEncoder(m_encoder, width, height, startBitrate, maxBitrate, minBitrate, maxFramerate); }
    void encodeFrame(CVPixelBufferRef pixelBuffer, int64_t timeStampNs, int64_t timeStamp, std::optional<uint64_t> duration, VideoFrame::Rotation rotation, bool isKeyframeRequired) final { webrtc::encodeLocalEncoderFrame(m_encoder, pixelBuffer, timeStampNs, timeStamp, duration, toWebRTCVideoRotation(rotation), isKeyframeRequired); }
    void setRates(uint32_t bitRate, uint32_t frameRate) final { webrtc::setLocalEncoderRates(m_encoder, bitRate, frameRate); }
    void flush() final { webrtc::flushLocalEncoder(m_encoder); }

    webrtc::LocalEncoder m_encoder;
};
#endif

std::unique_ptr<WebRTCVideoEncoder> WebRTCVideoEncoder::create(VideoCodecType codecType, bool useWebCoreEncoder, const Vector<std::pair<String, String>>& parameters, bool useAnnexB, VideoEncoderScalabilityMode scalabilityMode, WebRTCVideoEncoderCallback&& callback, WebRTCVideoEncoderDescriptionCallback&& descriptionCallback, WebRTCVideoEncoderErrorCallback&& errorCallback)
{
    if (useWebCoreEncoder) {
#if USE(AVFOUNDATION)
        if (codecType == VideoCodecType::H265)
            return makeUnique<WebRTCVideoEncoderVTBH265>(useAnnexB, WTF::move(callback), WTF::move(descriptionCallback), WTF::move(errorCallback));
#endif
    }

#if USE(LIBWEBRTC)
    ASSERT(codecType == VideoCodecType::H264 || codecType == VideoCodecType::H265);

    std::map<std::string, std::string> rtcParameters;
    for (auto& parameter : parameters)
        rtcParameters.emplace(parameter.first.utf8().legacyCStringPointer(), parameter.second.utf8().legacyCStringPointer());

    webrtc::LocalEncoderScalabilityMode rtcScalabilityMode;
    switch (scalabilityMode) {
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
        callback(unsafeMakeSpan(buffer, size), toWebRTCVideoEncoderFrameInfo(info));
    });
    auto newConfigurationBlock = makeBlockPtr([descriptionCallback = WTF::move(descriptionCallback)](const uint8_t* buffer, size_t size) {
        descriptionCallback(unsafeMakeSpan(buffer, size));
    });
    auto errorBlock = makeBlockPtr([errorCallback = WTF::move(errorCallback)](bool isFrameDropped) {
        errorCallback(isFrameDropped);
    });

    auto* encoder = webrtc::createLocalEncoder(webrtc::SdpVideoFormat { codecType == VideoCodecType::H264 ? "H264" : "H265", rtcParameters }, useAnnexB, rtcScalabilityMode, newFrameBlock.get(), newConfigurationBlock.get(), errorBlock.get());
    if (!encoder)
        return nullptr;

    return makeUnique<WebRTCLocalVideoEncoder>(encoder);
#else
    UNUSED_PARAM(codecType);
    UNUSED_PARAM(parameters);
    UNUSED_PARAM(useAnnexB);
    UNUSED_PARAM(scalabilityMode);
    UNUSED_PARAM(callback);
    UNUSED_PARAM(descriptionCallback);
    UNUSED_PARAM(errorCallback);
    return nullptr;
#endif
}

}
