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

#include <webrtc/modules/video_coding/codecs/vp8/include/vp8.h>
#include <webrtc/modules/video_coding/codecs/vp9/include/vp9.h>
#include <webrtc/sdk/WebKit/WebKitEncoder.h>
#include <webrtc/system_wrappers/include/cpu_info.h>

ALLOW_COMMA_END
ALLOW_UNUSED_PARAMETERS_END

namespace WebCore {

static WorkQueue& vpxQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("VPX VideoEncoder Queue"));
    return queue.get();
}

class LibWebRTCVPXInternalVideoEncoder : public ThreadSafeRefCounted<LibWebRTCVPXInternalVideoEncoder> , public webrtc::EncodedImageCallback {
public:
    static Ref<LibWebRTCVPXInternalVideoEncoder> create(LibWebRTCVPXVideoEncoder::Type type, const VideoEncoder::Config& config, VideoEncoder::OutputCallback&& outputCallback, VideoEncoder::PostTaskCallback&& postTaskCallback) { return adoptRef(*new LibWebRTCVPXInternalVideoEncoder(type, config, WTFMove(outputCallback), WTFMove(postTaskCallback))); }
    ~LibWebRTCVPXInternalVideoEncoder() = default;

    void postTask(Function<void()>&& task) { m_postTaskCallback(WTFMove(task)); }
    void encode(VideoEncoder::RawFrame&&, bool shouldGenerateKeyFrame, VideoEncoder::EncodeCallback&&);
    void close() { m_isClosed = true; }
private:
    LibWebRTCVPXInternalVideoEncoder(LibWebRTCVPXVideoEncoder::Type, const VideoEncoder::Config&, VideoEncoder::OutputCallback&&, VideoEncoder::PostTaskCallback&&);
    webrtc::EncodedImageCallback::Result OnEncodedImage(const webrtc::EncodedImage&, const webrtc::CodecSpecificInfo*) final;
    void OnDroppedFrame(DropReason) final;

    VideoEncoder::OutputCallback m_outputCallback;
    VideoEncoder::PostTaskCallback m_postTaskCallback;
    UniqueRef<webrtc::VideoEncoder> m_internalEncoder;
    int64_t m_timestamp { 0 };
    std::optional<uint64_t> m_duration;
    bool m_isClosed { false };
};

LibWebRTCVPXVideoEncoder::LibWebRTCVPXVideoEncoder(Type type, const VideoEncoder::Config& config, OutputCallback&& outputCallback, PostTaskCallback&& postTaskCallback)
    : m_internalEncoder(LibWebRTCVPXInternalVideoEncoder::create(type, config, WTFMove(outputCallback), WTFMove(postTaskCallback)))
{
}

LibWebRTCVPXVideoEncoder::~LibWebRTCVPXVideoEncoder()
{
}

void LibWebRTCVPXVideoEncoder::encode(RawFrame&& frame, bool shouldGenerateKeyFrame, EncodeCallback&& callback)
{
    vpxQueue().dispatch([frame = WTFMove(frame), shouldGenerateKeyFrame, encoder = m_internalEncoder, callback = WTFMove(callback)]() mutable {
        encoder->encode(WTFMove(frame), shouldGenerateKeyFrame, WTFMove(callback));
    });
}

void LibWebRTCVPXVideoEncoder::flush(Function<void()>&& callback)
{
    vpxQueue().dispatch([encoder = m_internalEncoder, callback = WTFMove(callback)]() mutable {
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


void LibWebRTCVPXInternalVideoEncoder::encode(VideoEncoder::RawFrame&& rawFrame, bool shouldGenerateKeyFrame, VideoEncoder::EncodeCallback&& callback)
{
    m_timestamp = rawFrame.timestamp;
    m_duration = rawFrame.duration;

    auto frameType = shouldGenerateKeyFrame ? webrtc::VideoFrameType::kVideoFrameKey : webrtc::VideoFrameType::kVideoFrameDelta;
    std::vector<webrtc::VideoFrameType> frameTypes { frameType };

    auto frameBuffer = webrtc::pixelBufferToFrame(rawFrame.frame->pixelBuffer());

    webrtc::VideoFrame frame { frameBuffer, webrtc::kVideoRotation_0, rawFrame.timestamp };
    auto error = m_internalEncoder->Encode(frame, &frameTypes);

    m_postTaskCallback([protectedThis = Ref { *this }, error, callback = WTFMove(callback)]() mutable {
        if (protectedThis->m_isClosed)
            return;

        String result;
        if (error)
            result = makeString("VPx encoding failed with error ", error);
        callback(WTFMove(result));
    });
}

static UniqueRef<webrtc::VideoEncoder> createInternalEncoder(LibWebRTCVPXVideoEncoder::Type type)
{
    if (type == LibWebRTCVPXVideoEncoder::Type::VP8)
        return makeUniqueRefFromNonNullUniquePtr(webrtc::VP8Encoder::Create());
    return makeUniqueRefFromNonNullUniquePtr(webrtc::VP9Encoder::Create());
}

LibWebRTCVPXInternalVideoEncoder::LibWebRTCVPXInternalVideoEncoder(LibWebRTCVPXVideoEncoder::Type type, const VideoEncoder::Config& config, VideoEncoder::OutputCallback&& outputCallback, VideoEncoder::PostTaskCallback&& postTaskCallback)
    : m_outputCallback(WTFMove(outputCallback))
    , m_postTaskCallback(WTFMove(postTaskCallback))
    , m_internalEncoder(createInternalEncoder(type))
{
    m_internalEncoder->RegisterEncodeCompleteCallback(this);

    // FIXME: Check InitEncode result.
    const int defaultPayloadSize = 1440;
    webrtc::VideoCodec videoCodec;
    videoCodec.width = config.width;
    videoCodec.height = config.height;
    m_internalEncoder->InitEncode(&videoCodec, webrtc::VideoEncoder::Settings { webrtc::VideoEncoder::Capabilities { true }, static_cast<int>(webrtc::CpuInfo::DetectNumberOfCores()), defaultPayloadSize });
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
