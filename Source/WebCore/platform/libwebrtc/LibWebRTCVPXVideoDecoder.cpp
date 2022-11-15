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
#include "LibWebRTCVPXVideoDecoder.h"

#if ENABLE(WEB_CODECS) && USE(LIBWEBRTC)

#include "Logging.h"
#include "VideoFrameLibWebRTC.h"
#include <wtf/FastMalloc.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>

ALLOW_UNUSED_PARAMETERS_BEGIN
ALLOW_COMMA_BEGIN

#include <webrtc/modules/video_coding/codecs/vp8/include/vp8.h>
#include <webrtc/modules/video_coding/codecs/vp9/include/vp9.h>
#include <webrtc/sdk/WebKit/WebKitDecoder.h>
#include <webrtc/system_wrappers/include/cpu_info.h>

ALLOW_COMMA_END
ALLOW_UNUSED_PARAMETERS_END

#include "CoreVideoSoftLink.h"

namespace WebCore {

static WorkQueue& vpxDecoderQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("VPx VideoDecoder Queue"));
    return queue.get();
}

class LibWebRTCVPXInternalVideoDecoder : public ThreadSafeRefCounted<LibWebRTCVPXInternalVideoDecoder> , public webrtc::DecodedImageCallback {
public:
    static Ref<LibWebRTCVPXInternalVideoDecoder> create(LibWebRTCVPXVideoDecoder::Type type, VideoDecoder::OutputCallback&& outputCallback, VideoDecoder::PostTaskCallback&& postTaskCallback) { return adoptRef(*new LibWebRTCVPXInternalVideoDecoder(type, WTFMove(outputCallback), WTFMove(postTaskCallback))); }
    ~LibWebRTCVPXInternalVideoDecoder() = default;

    void postTask(Function<void()>&& task) { m_postTaskCallback(WTFMove(task)); }
    void decode(Span<const uint8_t>, bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration,  VideoDecoder::DecodeCallback&&);
    void close() { m_isClosed = true; }
private:
    LibWebRTCVPXInternalVideoDecoder(LibWebRTCVPXVideoDecoder::Type, VideoDecoder::OutputCallback&&, VideoDecoder::PostTaskCallback&&);
    int32_t Decoded(webrtc::VideoFrame&) final;

    VideoDecoder::OutputCallback m_outputCallback;
    VideoDecoder::PostTaskCallback m_postTaskCallback;
    UniqueRef<webrtc::VideoDecoder> m_internalDecoder;
    int64_t m_timestamp { 0 };
    std::optional<uint64_t> m_duration;
    bool m_isClosed { false };
};

void LibWebRTCVPXVideoDecoder::create(Type type, CreateCallback&& callback, OutputCallback&& outputCallback, PostTaskCallback&& postTaskCallback)
{
    auto decoder = makeUniqueRef<LibWebRTCVPXVideoDecoder>(type, WTFMove(outputCallback), WTFMove(postTaskCallback));
    vpxDecoderQueue().dispatch([callback = WTFMove(callback), decoder = WTFMove(decoder)]() mutable {
        auto internalDecoder = decoder->m_internalDecoder;
        internalDecoder->postTask([callback = WTFMove(callback), decoder = WTFMove(decoder)]() mutable {
            callback(UniqueRef<VideoDecoder> { WTFMove(decoder) });
        });
    });
}

LibWebRTCVPXVideoDecoder::LibWebRTCVPXVideoDecoder(Type type, OutputCallback&& outputCallback, PostTaskCallback&& postTaskCallback)
    : m_internalDecoder(LibWebRTCVPXInternalVideoDecoder::create(type, WTFMove(outputCallback), WTFMove(postTaskCallback)))
{
}

LibWebRTCVPXVideoDecoder::~LibWebRTCVPXVideoDecoder()
{
}

void LibWebRTCVPXVideoDecoder::decode(EncodedFrame&& frame, DecodeCallback&& callback)
{
    vpxDecoderQueue().dispatch([value = Vector<uint8_t> { frame.data }, isKeyFrame = frame.isKeyFrame, timestamp = frame.timestamp, duration = frame.duration, decoder = m_internalDecoder, callback = WTFMove(callback)]() mutable {
        decoder->decode({ value.data(), value.size() }, isKeyFrame, timestamp, duration, WTFMove(callback));
    });
}

void LibWebRTCVPXVideoDecoder::flush(Function<void()>&& callback)
{
    vpxDecoderQueue().dispatch([decoder = m_internalDecoder, callback = WTFMove(callback)]() mutable {
        decoder->postTask(WTFMove(callback));
    });
}

void LibWebRTCVPXVideoDecoder::reset()
{
    m_internalDecoder->close();
}

void LibWebRTCVPXVideoDecoder::close()
{
    m_internalDecoder->close();
}


void LibWebRTCVPXInternalVideoDecoder::decode(Span<const uint8_t> data, bool isKeyFrame, int64_t timestamp, std::optional<uint64_t> duration,  VideoDecoder::DecodeCallback&& callback)
{
    m_timestamp = timestamp;
    m_duration = duration;

    webrtc::EncodedImage image;
    image.SetEncodedData(webrtc::WebKitEncodedImageBufferWrapper::create(const_cast<uint8_t*>(data.data()), data.size()));
    image._frameType = isKeyFrame ? webrtc::VideoFrameType::kVideoFrameKey : webrtc::VideoFrameType::kVideoFrameDelta;

    auto error = m_internalDecoder->Decode(image, false, 0);

    m_postTaskCallback([protectedThis = Ref { *this }, error, callback = WTFMove(callback)]() mutable {
        if (protectedThis->m_isClosed)
            return;

        if (error)
            protectedThis->m_outputCallback(makeUnexpected(makeString("VPx decoding failed with error ", error)));

        callback({ });
    });
}

static UniqueRef<webrtc::VideoDecoder> createInternalDecoder(LibWebRTCVPXVideoDecoder::Type type)
{
    if (type == LibWebRTCVPXVideoDecoder::Type::VP8)
        return makeUniqueRefFromNonNullUniquePtr(webrtc::VP8Decoder::Create());
    return makeUniqueRefFromNonNullUniquePtr(webrtc::VP9Decoder::Create());
}

LibWebRTCVPXInternalVideoDecoder::LibWebRTCVPXInternalVideoDecoder(LibWebRTCVPXVideoDecoder::Type type, VideoDecoder::OutputCallback&& outputCallback, VideoDecoder::PostTaskCallback&& postTaskCallback)
    : m_outputCallback(WTFMove(outputCallback))
    , m_postTaskCallback(WTFMove(postTaskCallback))
    , m_internalDecoder(createInternalDecoder(type))
{
    m_internalDecoder->RegisterDecodeCompleteCallback(this);
    webrtc::VideoDecoder::Settings settings;
    settings.set_number_of_cores(webrtc::CpuInfo::DetectNumberOfCores());
    m_internalDecoder->Configure(settings);
}

int32_t LibWebRTCVPXInternalVideoDecoder::Decoded(webrtc::VideoFrame& frame)
{
    m_postTaskCallback([protectedThis = Ref { *this }, colorSpace = VideoFrameLibWebRTC::colorSpaceFromFrame(frame), buffer = frame.video_frame_buffer(), timestamp = m_timestamp, duration = m_duration]() mutable {
        if (protectedThis->m_isClosed)
            return;

        auto videoFrame = VideoFrameLibWebRTC::create({ }, false, VideoFrame::Rotation::None, WTFMove(colorSpace), WTFMove(buffer), [](auto& buffer) {
            return adoptCF(webrtc::createPixelBufferFromFrameBuffer(buffer, [](size_t width, size_t height, webrtc::BufferType bufferType) -> CVPixelBufferRef {
                OSType pixelBufferType;
                switch (bufferType) {
                case webrtc::BufferType::I420:
                    pixelBufferType = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
                    break;
                case webrtc::BufferType::I010:
                    pixelBufferType = kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
                    break;
                default:
                    return nullptr;
                }

                CVPixelBufferRef pixelBuffer = nullptr;
                auto status = CVPixelBufferCreate(kCFAllocatorDefault, width, height, pixelBufferType, nullptr, &pixelBuffer);
                if (status != kCVReturnSuccess) {
                    RELEASE_LOG_ERROR(Media, "Failed creating a pixel buffer for converting a VPX frame with error %d", status);
                    return nullptr;
                }
                return pixelBuffer;
            }));
        });

        protectedThis->m_outputCallback(VideoDecoder::DecodedFrame { WTFMove(videoFrame), timestamp, duration });
    });
    return 0;
}

}

#endif // ENABLE(WEB_CODECS) && USE(LIBWEBRTC)
