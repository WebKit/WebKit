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

#if ENABLE(WEB_CODECS) && USE(LIBWEBRTC)

#include "VideoFrameLibWebRTC.h"
#include <wtf/FastMalloc.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>

ALLOW_UNUSED_PARAMETERS_BEGIN
ALLOW_COMMA_BEGIN

#include <webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.h>
#include <webrtc/modules/video_coding/codecs/vp8/include/vp8.h>
#include <webrtc/modules/video_coding/codecs/vp9/include/vp9.h>
#include <webrtc/sdk/WebKit/WebKitEncoder.h>
#include <webrtc/system_wrappers/include/cpu_info.h>

ALLOW_COMMA_END
ALLOW_UNUSED_PARAMETERS_END

namespace WebCore {

static WorkQueue& vpxEncoderQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("VPX VideoEncoder Queue"));
    return queue.get();
}

class LibWebRTCVPXInternalVideoEncoder : public ThreadSafeRefCounted<LibWebRTCVPXInternalVideoEncoder> , public webrtc::EncodedImageCallback {
public:
    static Ref<LibWebRTCVPXInternalVideoEncoder> create(LibWebRTCVPXVideoEncoder::Type type, VideoEncoder::OutputCallback&& outputCallback, VideoEncoder::PostTaskCallback&& postTaskCallback) { return adoptRef(*new LibWebRTCVPXInternalVideoEncoder(type, WTFMove(outputCallback), WTFMove(postTaskCallback))); }
    ~LibWebRTCVPXInternalVideoEncoder() = default;

    int initialize(LibWebRTCVPXVideoEncoder::Type, const VideoEncoder::Config&);

    void postTask(Function<void()>&& task) { m_postTaskCallback(WTFMove(task)); }
    void encode(VideoEncoder::RawFrame&&, bool shouldGenerateKeyFrame, VideoEncoder::EncodeCallback&&);
    void close() { m_isClosed = true; }
private:
    LibWebRTCVPXInternalVideoEncoder(LibWebRTCVPXVideoEncoder::Type, VideoEncoder::OutputCallback&&, VideoEncoder::PostTaskCallback&&);
    webrtc::EncodedImageCallback::Result OnEncodedImage(const webrtc::EncodedImage&, const webrtc::CodecSpecificInfo*) final;
    void OnDroppedFrame(DropReason) final;

    VideoEncoder::OutputCallback m_outputCallback;
    VideoEncoder::PostTaskCallback m_postTaskCallback;
    UniqueRef<webrtc::VideoEncoder> m_internalEncoder;
    int64_t m_timestamp { 0 };
    std::optional<uint64_t> m_duration;
    bool m_isClosed { false };
    uint64_t m_width { 0 };
    uint64_t m_height { 0 };
    bool m_isInitialized { false };
    bool m_hasEncoded { false };
};

void LibWebRTCVPXVideoEncoder::create(Type type, const VideoEncoder::Config& config, CreateCallback&& callback, DescriptionCallback&& descriptionCallback, OutputCallback&& outputCallback, PostTaskCallback&& postTaskCallback)
{
    auto encoder = makeUniqueRef<LibWebRTCVPXVideoEncoder>(type, WTFMove(outputCallback), WTFMove(postTaskCallback));
    auto error = encoder->initialize(type, config);
    vpxEncoderQueue().dispatch([callback = WTFMove(callback), descriptionCallback = WTFMove(descriptionCallback), encoder = WTFMove(encoder), error]() mutable {
        auto internalEncoder = encoder->m_internalEncoder;
        internalEncoder->postTask([callback = WTFMove(callback), descriptionCallback = WTFMove(descriptionCallback), encoder = WTFMove(encoder), error]() mutable {
            if (error) {
                callback(makeUnexpected(makeString("VPx encoding initialization failed with error ", error)));
                return;
            }
            callback(UniqueRef<VideoEncoder> { WTFMove(encoder) });

            VideoEncoder::ActiveConfiguration configuration;
            configuration.colorSpace = PlatformVideoColorSpace { PlatformVideoColorPrimaries::Bt709, PlatformVideoTransferCharacteristics::Iec6196621, PlatformVideoMatrixCoefficients::Smpte170m, false };
            descriptionCallback(WTFMove(configuration));
        });
    });
}

LibWebRTCVPXVideoEncoder::LibWebRTCVPXVideoEncoder(Type type, OutputCallback&& outputCallback, PostTaskCallback&& postTaskCallback)
    : m_internalEncoder(LibWebRTCVPXInternalVideoEncoder::create(type, WTFMove(outputCallback), WTFMove(postTaskCallback)))
{
}

LibWebRTCVPXVideoEncoder::~LibWebRTCVPXVideoEncoder()
{
}

int LibWebRTCVPXVideoEncoder::initialize(LibWebRTCVPXVideoEncoder::Type type, const VideoEncoder::Config& config)
{
    return m_internalEncoder->initialize(type, config);
}

void LibWebRTCVPXVideoEncoder::encode(RawFrame&& frame, bool shouldGenerateKeyFrame, EncodeCallback&& callback)
{
    vpxEncoderQueue().dispatch([frame = WTFMove(frame), shouldGenerateKeyFrame, encoder = m_internalEncoder, callback = WTFMove(callback)]() mutable {
        encoder->encode(WTFMove(frame), shouldGenerateKeyFrame, WTFMove(callback));
    });
}

void LibWebRTCVPXVideoEncoder::flush(Function<void()>&& callback)
{
    vpxEncoderQueue().dispatch([encoder = m_internalEncoder, callback = WTFMove(callback)]() mutable {
        encoder->postTask(WTFMove(callback));
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

static UniqueRef<webrtc::VideoEncoder> createInternalEncoder(LibWebRTCVPXVideoEncoder::Type type)
{
    switch (type) {
    case LibWebRTCVPXVideoEncoder::Type::VP8:
        return makeUniqueRefFromNonNullUniquePtr(webrtc::VP8Encoder::Create());
    case LibWebRTCVPXVideoEncoder::Type::VP9:
        return makeUniqueRefFromNonNullUniquePtr(webrtc::VP9Encoder::Create());
    case LibWebRTCVPXVideoEncoder::Type::AV1:
        return makeUniqueRefFromNonNullUniquePtr(webrtc::CreateLibaomAv1Encoder());
    }
}

LibWebRTCVPXInternalVideoEncoder::LibWebRTCVPXInternalVideoEncoder(LibWebRTCVPXVideoEncoder::Type type, VideoEncoder::OutputCallback&& outputCallback, VideoEncoder::PostTaskCallback&& postTaskCallback)
    : m_outputCallback(WTFMove(outputCallback))
    , m_postTaskCallback(WTFMove(postTaskCallback))
    , m_internalEncoder(createInternalEncoder(type))
{
}

int LibWebRTCVPXInternalVideoEncoder::initialize(LibWebRTCVPXVideoEncoder::Type type, const VideoEncoder::Config& config)
{
    m_width = config.width;
    m_height = config.height;

    const int defaultPayloadSize = 1440;
    webrtc::VideoCodec videoCodec;
    videoCodec.width = config.width;
    videoCodec.height = config.height;
    videoCodec.maxFramerate = 100;

    if (type == LibWebRTCVPXVideoEncoder::Type::VP8)
        videoCodec.codecType = webrtc::kVideoCodecVP8;
    else {
        videoCodec.codecType = webrtc::kVideoCodecVP9;
        videoCodec.VP9()->numberOfSpatialLayers = 1;
        videoCodec.VP9()->numberOfTemporalLayers = 1;
    }

    if (auto error = m_internalEncoder->InitEncode(&videoCodec, webrtc::VideoEncoder::Settings { webrtc::VideoEncoder::Capabilities { true }, static_cast<int>(webrtc::CpuInfo::DetectNumberOfCores()), defaultPayloadSize }))
        return error;

    m_isInitialized = true;

    webrtc::VideoBitrateAllocation allocation;
    allocation.SetBitrate(0, 0, config.bitRate ? config.bitRate : 3 * config.width * config.height);
    m_internalEncoder->SetRates({ allocation, config.frameRate ? config.frameRate : 30.0 });

    m_internalEncoder->RegisterEncodeCompleteCallback(this);
    return 0;
}

void LibWebRTCVPXInternalVideoEncoder::encode(VideoEncoder::RawFrame&& rawFrame, bool shouldGenerateKeyFrame, VideoEncoder::EncodeCallback&& callback)
{
    if (!m_isInitialized)
        return;

    m_timestamp = rawFrame.timestamp;
    m_duration = rawFrame.duration;

    auto frameType = (shouldGenerateKeyFrame || !m_hasEncoded) ? webrtc::VideoFrameType::kVideoFrameKey : webrtc::VideoFrameType::kVideoFrameDelta;
    std::vector<webrtc::VideoFrameType> frameTypes { frameType };

    auto frameBuffer = webrtc::pixelBufferToFrame(rawFrame.frame->pixelBuffer());

    if (m_width != static_cast<size_t>(frameBuffer->width()) || m_height != static_cast<size_t>(frameBuffer->height()))
        frameBuffer = frameBuffer->Scale(m_width, m_height);

    webrtc::VideoFrame frame { frameBuffer, webrtc::kVideoRotation_0, rawFrame.timestamp };
    auto error = m_internalEncoder->Encode(frame, &frameTypes);

    if (!m_hasEncoded)
        m_hasEncoded = !error;

    m_postTaskCallback([protectedThis = Ref { *this }, error, callback = WTFMove(callback)]() mutable {
        if (protectedThis->m_isClosed)
            return;

        String result;
        if (error)
            result = makeString("VPx encoding failed with error ", error);
        callback(WTFMove(result));
    });
}

webrtc::EncodedImageCallback::Result LibWebRTCVPXInternalVideoEncoder::OnEncodedImage(const webrtc::EncodedImage& encodedImage, const webrtc::CodecSpecificInfo*)
{
    VideoEncoder::EncodedFrame encodedFrame {
        Vector<uint8_t> { Span<const uint8_t> { encodedImage.data(), encodedImage.size() } },
        encodedImage._frameType == webrtc::VideoFrameType::kVideoFrameKey,
        m_timestamp,
        m_duration
    };

    m_postTaskCallback([protectedThis = Ref { *this }, encodedFrame = WTFMove(encodedFrame)]() mutable {
        if (protectedThis->m_isClosed)
            return;
        protectedThis->m_outputCallback({ WTFMove(encodedFrame) });
    });
    return EncodedImageCallback::Result { EncodedImageCallback::Result::OK };
}

void LibWebRTCVPXInternalVideoEncoder::OnDroppedFrame(DropReason)
{
}

}

#endif // ENABLE(WEB_CODECS) && USE(LIBWEBRTC)
