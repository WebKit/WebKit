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
#include "LibWebRTCVPXVideoEncoder.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA)

#include "VideoFrameLibWebRTC.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/MakeString.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
ALLOW_UNUSED_PARAMETERS_BEGIN
ALLOW_COMMA_BEGIN

#include <webrtc/api/environment/environment_factory.h>
#include <webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.h>
#include <webrtc/modules/video_coding/codecs/vp8/include/vp8.h>
#include <webrtc/modules/video_coding/codecs/vp9/include/vp9.h>
#include <webrtc/system_wrappers/include/cpu_info.h>
#include <webrtc/webkit_sdk/WebKit/WebKitEncoder.h>

ALLOW_COMMA_END
ALLOW_UNUSED_PARAMETERS_END
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LibWebRTCVPXVideoEncoder);

static constexpr double defaultFrameRate = 30.0;

static WorkQueue& vpxEncoderQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("VPX VideoEncoder Queue"_s));
    return queue.get();
}

class LibWebRTCVPXInternalVideoEncoder : public ThreadSafeRefCounted<LibWebRTCVPXInternalVideoEncoder> , public webrtc::EncodedImageCallback {
public:
    static Ref<LibWebRTCVPXInternalVideoEncoder> create(LibWebRTCVPXVideoEncoder::Type type, VideoEncoder::OutputCallback&& outputCallback) { return adoptRef(*new LibWebRTCVPXInternalVideoEncoder(type, WTFMove(outputCallback))); }
    ~LibWebRTCVPXInternalVideoEncoder() = default;

    int initialize(LibWebRTCVPXVideoEncoder::Type, const VideoEncoder::Config&);

    Ref<VideoEncoder::EncodePromise> encode(VideoEncoder::RawFrame&&, bool shouldGenerateKeyFrame);
    void close() { m_isClosed = true; }
    void setRates(uint64_t bitRate, double frameRate);

private:
    LibWebRTCVPXInternalVideoEncoder(LibWebRTCVPXVideoEncoder::Type, VideoEncoder::OutputCallback&&);
    webrtc::EncodedImageCallback::Result OnEncodedImage(const webrtc::EncodedImage&, const webrtc::CodecSpecificInfo*) final;
    void OnDroppedFrame(DropReason) final;

    VideoEncoder::OutputCallback m_outputCallback;
    UniqueRef<webrtc::VideoEncoder> m_internalEncoder;
    int64_t m_timestamp { 0 };
    int64_t m_timestampOffset { 0 };
    std::optional<uint64_t> m_duration;
    bool m_isClosed { false };
    VideoEncoder::Config m_config;
    bool m_isInitialized { false };
    bool m_hasEncoded { false };
    bool m_hasMultipleTemporalLayers { false };
};

void LibWebRTCVPXVideoEncoder::create(Type type, const VideoEncoder::Config& config, CreateCallback&& callback, DescriptionCallback&& descriptionCallback, OutputCallback&& outputCallback)
{
    auto encoder = makeUniqueRef<LibWebRTCVPXVideoEncoder>(type, WTFMove(outputCallback));
    auto error = encoder->initialize(type, config);

    if (error) {
        callback(makeUnexpected(makeString("VPx encoding initialization failed with error "_s, error)));
        return;
    }
    callback(UniqueRef<VideoEncoder> { WTFMove(encoder) });

    VideoEncoder::ActiveConfiguration configuration;
    configuration.colorSpace = PlatformVideoColorSpace { PlatformVideoColorPrimaries::Bt709, PlatformVideoTransferCharacteristics::Bt709, PlatformVideoMatrixCoefficients::Bt709, false };
    descriptionCallback(WTFMove(configuration));
}

LibWebRTCVPXVideoEncoder::LibWebRTCVPXVideoEncoder(Type type, OutputCallback&& outputCallback)
    : m_internalEncoder(LibWebRTCVPXInternalVideoEncoder::create(type, WTFMove(outputCallback)))
{
}

LibWebRTCVPXVideoEncoder::~LibWebRTCVPXVideoEncoder()
{
}

int LibWebRTCVPXVideoEncoder::initialize(LibWebRTCVPXVideoEncoder::Type type, const VideoEncoder::Config& config)
{
    return m_internalEncoder->initialize(type, config);
}

Ref<VideoEncoder::EncodePromise> LibWebRTCVPXVideoEncoder::encode(RawFrame&& frame, bool shouldGenerateKeyFrame)
{
    return invokeAsync(vpxEncoderQueue(), [frame = WTFMove(frame), shouldGenerateKeyFrame, encoder = m_internalEncoder]() mutable {
        return encoder->encode(WTFMove(frame), shouldGenerateKeyFrame);
    });
}

Ref<GenericPromise> LibWebRTCVPXVideoEncoder::flush()
{
    return invokeAsync(vpxEncoderQueue(), [] {
        return GenericPromise::createAndResolve();
    });
}

void LibWebRTCVPXVideoEncoder::reset()
{
    m_internalEncoder->close();
}

void LibWebRTCVPXVideoEncoder::close()
{
    m_internalEncoder->close();
}

Ref<GenericPromise> LibWebRTCVPXVideoEncoder::setRates(uint64_t bitRate, double frameRate)
{
    return invokeAsync(vpxEncoderQueue(), [encoder = m_internalEncoder, bitRate, frameRate] {
        encoder->setRates(bitRate, frameRate);
        return GenericPromise::createAndResolve();
    });
}

static UniqueRef<webrtc::VideoEncoder> createInternalEncoder(LibWebRTCVPXVideoEncoder::Type type)
{
    switch (type) {
    case LibWebRTCVPXVideoEncoder::Type::VP8:
        return makeUniqueRefFromNonNullUniquePtr(webrtc::CreateVp8Encoder(webrtc::EnvironmentFactory().Create()));
    case LibWebRTCVPXVideoEncoder::Type::VP9:
        return makeUniqueRefFromNonNullUniquePtr(webrtc::CreateVp9Encoder(webrtc::EnvironmentFactory().Create()));
    case LibWebRTCVPXVideoEncoder::Type::VP9_P2:
        return makeUniqueRefFromNonNullUniquePtr(webrtc::CreateVp9Encoder(webrtc::EnvironmentFactory().Create(), { webrtc::VP9Profile::kProfile2 }));
#if ENABLE(AV1)
    case LibWebRTCVPXVideoEncoder::Type::AV1:
        return makeUniqueRefFromNonNullUniquePtr(webrtc::CreateLibaomAv1Encoder(webrtc::EnvironmentFactory().Create()));
#endif
    }
}

LibWebRTCVPXInternalVideoEncoder::LibWebRTCVPXInternalVideoEncoder(LibWebRTCVPXVideoEncoder::Type type, VideoEncoder::OutputCallback&& outputCallback)
    : m_outputCallback(WTFMove(outputCallback))
    , m_internalEncoder(createInternalEncoder(type))
{
}

static webrtc::VideoBitrateAllocation computeAllocation(const VideoEncoder::Config& config)
{
    auto totalBitRate = config.bitRate ? config.bitRate : 3 * config.width * config.height;

    webrtc::VideoBitrateAllocation allocation;
    switch (config.scalabilityMode) {
    case VideoEncoder::ScalabilityMode::L1T1:
        allocation.SetBitrate(0, 0, totalBitRate);
        break;
    case VideoEncoder::ScalabilityMode::L1T2:
        allocation.SetBitrate(0, 0, totalBitRate * 0.6);
        allocation.SetBitrate(0, 1, totalBitRate * 0.4);
        break;
    case VideoEncoder::ScalabilityMode::L1T3:
        allocation.SetBitrate(0, 0, totalBitRate * 0.5);
        allocation.SetBitrate(0, 1, totalBitRate * 0.2);
        allocation.SetBitrate(0, 2, totalBitRate * 0.3);
        break;
    }
    return allocation;
}

int LibWebRTCVPXInternalVideoEncoder::initialize(LibWebRTCVPXVideoEncoder::Type type, const VideoEncoder::Config& config)
{
    m_config = config;

    const int defaultPayloadSize = 1440;
    webrtc::VideoCodec videoCodec;
    videoCodec.width = config.width;
    videoCodec.height = config.height;
    videoCodec.maxFramerate = 100;

    switch (config.scalabilityMode) {
    case VideoEncoder::ScalabilityMode::L1T1:
        videoCodec.SetScalabilityMode(webrtc::ScalabilityMode::kL1T1);
        break;
    case VideoEncoder::ScalabilityMode::L1T2:
        m_hasMultipleTemporalLayers = true;
        videoCodec.SetScalabilityMode(webrtc::ScalabilityMode::kL1T2);
        break;
    case VideoEncoder::ScalabilityMode::L1T3:
        m_hasMultipleTemporalLayers = true;
        videoCodec.SetScalabilityMode(webrtc::ScalabilityMode::kL1T3);
        break;
    }

    switch (type) {
    case LibWebRTCVPXVideoEncoder::Type::VP8:
        videoCodec.codecType = webrtc::kVideoCodecVP8;
        switch (config.scalabilityMode) {
        case VideoEncoder::ScalabilityMode::L1T1:
            videoCodec.VP8()->numberOfTemporalLayers = 1;
            break;
        case VideoEncoder::ScalabilityMode::L1T2:
            videoCodec.VP8()->numberOfTemporalLayers = 2;
            break;
        case VideoEncoder::ScalabilityMode::L1T3:
            videoCodec.VP8()->numberOfTemporalLayers = 3;
            break;
        }
        break;
    case LibWebRTCVPXVideoEncoder::Type::VP9:
    case LibWebRTCVPXVideoEncoder::Type::VP9_P2:
        videoCodec.codecType = webrtc::kVideoCodecVP9;
        videoCodec.VP9()->numberOfSpatialLayers = 1;
        break;
#if ENABLE(AV1)
    case LibWebRTCVPXVideoEncoder::Type::AV1:
        videoCodec.codecType = webrtc::kVideoCodecAV1;
        videoCodec.qpMax = 56; // default qp max
        break;
#endif
    }

    if (auto error = m_internalEncoder->InitEncode(&videoCodec, webrtc::VideoEncoder::Settings { webrtc::VideoEncoder::Capabilities { true }, static_cast<int>(webrtc::CpuInfo::DetectNumberOfCores()), defaultPayloadSize }))
        return error;

    m_isInitialized = true;
    m_internalEncoder->SetRates({ computeAllocation(config), config.frameRate ? config.frameRate : defaultFrameRate });

    m_internalEncoder->RegisterEncodeCompleteCallback(this);
    return 0;
}

Ref<VideoEncoder::EncodePromise> LibWebRTCVPXInternalVideoEncoder::encode(VideoEncoder::RawFrame&& rawFrame, bool shouldGenerateKeyFrame)
{
    if (!m_isInitialized)
        return VideoEncoder::EncodePromise::createAndReject("Encoder is not initialized"_s);

    if (rawFrame.timestamp + m_timestampOffset <= 0)
        m_timestampOffset = 1 - rawFrame.timestamp;
    m_timestamp = rawFrame.timestamp;
    m_duration = rawFrame.duration;

    auto frameType = (shouldGenerateKeyFrame || !m_hasEncoded) ? webrtc::VideoFrameType::kVideoFrameKey : webrtc::VideoFrameType::kVideoFrameDelta;
    std::vector<webrtc::VideoFrameType> frameTypes { frameType };

    auto frameBuffer = webrtc::pixelBufferToFrame(rawFrame.frame->pixelBuffer());

    if (m_config.width != static_cast<size_t>(frameBuffer->width()) || m_config.height != static_cast<size_t>(frameBuffer->height()))
        frameBuffer = frameBuffer->Scale(m_config.width, m_config.height);

    webrtc::VideoFrame frame { frameBuffer, webrtc::kVideoRotation_0, rawFrame.timestamp + m_timestampOffset };
    auto error = m_internalEncoder->Encode(frame, &frameTypes);

    if (!m_hasEncoded)
        m_hasEncoded = !error;

    if (error)
        return VideoEncoder::EncodePromise::createAndReject("Encoder is not initialized"_s);

    return VideoEncoder::EncodePromise::createAndResolve();
}

void LibWebRTCVPXInternalVideoEncoder::setRates(uint64_t bitRate, double frameRate)
{
    if (bitRate)
        m_config.bitRate = bitRate;
    if (frameRate)
        m_config.frameRate = frameRate;
    m_internalEncoder->SetRates({ computeAllocation(m_config), m_config.frameRate ? m_config.frameRate : defaultFrameRate });
}

webrtc::EncodedImageCallback::Result LibWebRTCVPXInternalVideoEncoder::OnEncodedImage(const webrtc::EncodedImage& encodedImage, const webrtc::CodecSpecificInfo*)
{
    if (m_isClosed)
        return EncodedImageCallback::Result { EncodedImageCallback::Result::OK };

    std::optional<unsigned> frameTemporalIndex;
    if (m_hasMultipleTemporalLayers) {
        if (auto temporalIndex = encodedImage.TemporalIndex())
            frameTemporalIndex = *temporalIndex;
    }

    VideoEncoder::EncodedFrame encodedFrame {
        Vector<uint8_t> { std::span { encodedImage.data(), encodedImage.size() } },
        encodedImage._frameType == webrtc::VideoFrameType::kVideoFrameKey,
        m_timestamp,
        m_duration,
        frameTemporalIndex
    };

    m_outputCallback({ WTFMove(encodedFrame) });
    return EncodedImageCallback::Result { EncodedImageCallback::Result::OK };
}

void LibWebRTCVPXInternalVideoEncoder::OnDroppedFrame(DropReason)
{
}

}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // USE(LIBWEBRTC)
