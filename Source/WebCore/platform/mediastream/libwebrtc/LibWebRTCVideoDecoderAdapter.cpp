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
#include "LibWebRTCVideoDecoderAdapter.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA)

#include "LibWebRTCColorSpaceUtilities.h"
#include "LibWebRTCVideoFrameUtilities.h"
#include "Logging.h"
#include "SharedBuffer.h"
#include "VideoDecoder.h"
#include "VideoFrameLibWebRTC.h"
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN

#include <webrtc/api/video/encoded_image.h>
#include <webrtc/modules/video_coding/include/video_error_codes.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

static constexpr size_t maxPendingFramesWhileCreatingDecoder = 30;

static webrtc::scoped_refptr<webrtc::VideoFrameBuffer> createWebRTCVideoFrameBuffer(Ref<VideoFrame>&& videoFrame)
{
    if (RefPtr webrtcFrame = dynamicDowncast<VideoFrameLibWebRTC>(videoFrame))
        return webrtcFrame->buffer();

    auto size = videoFrame->presentationSize();
    return webrtc::toWebRTCVideoFrameBuffer(&videoFrame.leakRef(),
        [](auto* pointer) { return static_cast<VideoFrame*>(pointer)->pixelBuffer(); },
        [](auto* pointer) { static_cast<VideoFrame*>(pointer)->deref(); },
        size.width(), size.height());
}

class LibWebRTCVideoDecoderAdapterBackend final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<LibWebRTCVideoDecoderAdapterBackend> {
public:
    static Ref<LibWebRTCVideoDecoderAdapterBackend> create(LibWebRTCVideoDecoderAdapter::Description&& description, Ref<WorkQueue>&& queue) { return adoptRef(*new LibWebRTCVideoDecoderAdapterBackend(WTF::move(description), WTF::move(queue))); }

    // Called on the libwebrtc decoding thread.
    bool shouldDecode(bool isKeyFrame);
    void decode(VideoEncodedData&&, std::optional<PlatformVideoColorSpace>&&);
    void setDecodeCompleteCallback(webrtc::DecodedImageCallback*);
    void reset();

private:
    LibWebRTCVideoDecoderAdapterBackend(LibWebRTCVideoDecoderAdapter::Description&&, Ref<WorkQueue>&&);

    void decodeOnQueue(VideoEncodedData&&, std::optional<PlatformVideoColorSpace>&&);
    void createDecoder(std::optional<PlatformVideoColorSpace>&&);
    void decoderFlushed();
    void startDecoderCreation();
    void decoderCreated(VideoDecoder::CreateResult&&, uint64_t decoderGeneration);
    void decodeFrame(VideoEncodedData&&);
    void decodingFailed(uint64_t decoderGeneration);
    void closeDecoder();

    // Called on any thread.
    void decoderOutput(std::expected<VideoDecoder::DecodedFrame, String>&&, uint64_t decoderGeneration);

    const Ref<WorkQueue> m_queue;
    const LibWebRTCVideoDecoderAdapter::Description m_description;

    enum class State : uint8_t {
        None,
        Flushing,
        Creating,
        Ready,
    };

    RefPtr<VideoDecoder> m_decoder WTF_GUARDED_BY_CAPABILITY(m_queue.get());
    RefPtr<VideoDecoder> m_flushingDecoder WTF_GUARDED_BY_CAPABILITY(m_queue.get());
    State m_state WTF_GUARDED_BY_CAPABILITY(m_queue.get()) { State::None };
    uint64_t m_decoderGeneration WTF_GUARDED_BY_CAPABILITY(m_queue.get()) { 0 };
    bool m_shouldDropDeltaFrames WTF_GUARDED_BY_CAPABILITY(m_queue.get()) { true };
    std::optional<PlatformVideoColorSpace> m_decoderColorSpace WTF_GUARDED_BY_CAPABILITY(m_queue.get());
    Deque<VideoEncodedData> m_pendingFrames WTF_GUARDED_BY_CAPABILITY(m_queue.get());

    std::atomic<bool> m_shouldRequestKeyFrame { true };

    Lock m_callbackLock;
    webrtc::DecodedImageCallback* m_callback WTF_GUARDED_BY_LOCK(m_callbackLock) { nullptr };
};

LibWebRTCVideoDecoderAdapterBackend::LibWebRTCVideoDecoderAdapterBackend(LibWebRTCVideoDecoderAdapter::Description&& description, Ref<WorkQueue>&& queue)
    : m_queue(WTF::move(queue))
    , m_description(WTF::move(description).isolatedCopy())
{
}

bool LibWebRTCVideoDecoderAdapterBackend::shouldDecode(bool isKeyFrame)
{
    if (!m_shouldRequestKeyFrame)
        return true;

    if (!isKeyFrame)
        return false;

    m_shouldRequestKeyFrame = false;
    return true;
}

void LibWebRTCVideoDecoderAdapterBackend::decode(VideoEncodedData&& frame, std::optional<PlatformVideoColorSpace>&& colorSpace)
{
    m_queue->dispatch([protectedThis = Ref { *this }, frame = WTF::move(frame), colorSpace = WTF::move(colorSpace)] mutable {
        protectedThis->decodeOnQueue(WTF::move(frame), WTF::move(colorSpace));
    });
}

void LibWebRTCVideoDecoderAdapterBackend::setDecodeCompleteCallback(webrtc::DecodedImageCallback* callback)
{
    Locker locker { m_callbackLock };
    m_callback = callback;
}

void LibWebRTCVideoDecoderAdapterBackend::reset()
{
    setDecodeCompleteCallback(nullptr);
    m_shouldRequestKeyFrame = true;

    m_queue->dispatch([protectedThis = Ref { *this }] {
        assertIsCurrent(protectedThis->m_queue.get());
        protectedThis->closeDecoder();
        protectedThis->m_shouldDropDeltaFrames = true;
    });
}

void LibWebRTCVideoDecoderAdapterBackend::decodeOnQueue(VideoEncodedData&& frame, std::optional<PlatformVideoColorSpace>&& colorSpace)
{
    assertIsCurrent(m_queue.get());

    if (m_shouldDropDeltaFrames) {
        if (!frame.isKeyFrame) {
            m_shouldRequestKeyFrame = true;
            return;
        }
        m_shouldRequestKeyFrame = false;
        m_shouldDropDeltaFrames = false;
    }

    if (frame.isKeyFrame && (m_state == State::None || colorSpace != m_decoderColorSpace))
        createDecoder(WTF::move(colorSpace));

    if (m_state == State::Flushing || m_state == State::Creating) {
        if (m_pendingFrames.size() >= maxPendingFramesWhileCreatingDecoder) {
            RELEASE_LOG_ERROR(WebRTC, "LibWebRTCVideoDecoderAdapter dropping pending frames while creating decoder");
            m_pendingFrames.clear();
            if (!frame.isKeyFrame) {
                m_shouldDropDeltaFrames = true;
                m_shouldRequestKeyFrame = true;
                return;
            }
        }
        m_pendingFrames.append(WTF::move(frame));
        return;
    }

    if (m_state != State::Ready)
        return;

    decodeFrame(WTF::move(frame));
}

void LibWebRTCVideoDecoderAdapterBackend::createDecoder(std::optional<PlatformVideoColorSpace>&& colorSpace)
{
    assertIsCurrent(m_queue.get());

    m_pendingFrames.clear();
    m_decoderColorSpace = WTF::move(colorSpace);

    switch (m_state) {
    case State::Flushing:
        return;
    case State::Ready: {
        m_state = State::Flushing;
        m_flushingDecoder = std::exchange(m_decoder, nullptr);
        protect(m_flushingDecoder)->flush()->whenSettled(m_queue, [weakThis = ThreadSafeWeakPtr { *this }](auto&&) {
            if (RefPtr protectedThis = weakThis)
                protectedThis->decoderFlushed();
        });
        return;
    }
    case State::None:
    case State::Creating:
        startDecoderCreation();
        return;
    }
}

void LibWebRTCVideoDecoderAdapterBackend::decoderFlushed()
{
    assertIsCurrent(m_queue.get());

    if (m_state != State::Flushing) {
        ASSERT(m_state == State::None);
        return;
    }

    if (RefPtr decoder = std::exchange(m_flushingDecoder, nullptr))
        decoder->close();

    startDecoderCreation();
}

void LibWebRTCVideoDecoderAdapterBackend::startDecoderCreation()
{
    assertIsCurrent(m_queue.get());
    ASSERT(!m_decoder && !m_flushingDecoder);

    m_state = State::Creating;
    auto decoderGeneration = ++m_decoderGeneration;

    VideoDecoder::Config config {
        .description = { },
        .colorSpace = m_decoderColorSpace,
        .decoding = m_description.hardwareAcceleration,
        .pixelBuffer = VideoDecoder::HardwareBuffer::Yes
    };

    VideoDecoder::create(m_description.codec, config, [weakThis = ThreadSafeWeakPtr { *this }, decoderGeneration](auto&& result) {
        if (RefPtr protectedThis = weakThis)
            protectedThis->decoderOutput(WTF::move(result), decoderGeneration);
    })->whenSettled(m_queue, [weakThis = ThreadSafeWeakPtr { *this }, decoderGeneration](auto&& result) mutable {
        if (RefPtr protectedThis = weakThis)
            protectedThis->decoderCreated(WTF::move(result), decoderGeneration);
    });
}

void LibWebRTCVideoDecoderAdapterBackend::decoderCreated(VideoDecoder::CreateResult&& result, uint64_t decoderGeneration)
{
    assertIsCurrent(m_queue.get());

    if (m_state != State::Creating || decoderGeneration != m_decoderGeneration)
        return;

    if (!result) {
        RELEASE_LOG_ERROR(WebRTC, "LibWebRTCVideoDecoderAdapter failed creating decoder");
        m_state = State::None;
        m_pendingFrames.clear();
        m_shouldDropDeltaFrames = true;
        m_shouldRequestKeyFrame = true;
        return;
    }

    m_state = State::Ready;
    m_decoder = WTF::move(*result);

    auto pendingFrames = std::exchange(m_pendingFrames, { });
    while (!pendingFrames.isEmpty())
        decodeFrame(pendingFrames.takeFirst());
}

void LibWebRTCVideoDecoderAdapterBackend::decodeFrame(VideoEncodedData&& frame)
{
    assertIsCurrent(m_queue.get());
    ASSERT(m_state == State::Ready);

    RefPtr decoder = m_decoder;
    decoder->decode(WTF::move(frame))->whenSettled(m_queue, [weakThis = ThreadSafeWeakPtr { *this }, decoderGeneration = m_decoderGeneration](auto&& result) {
        RefPtr protectedThis = weakThis;
        if (protectedThis && !result)
            protectedThis->decodingFailed(decoderGeneration);
    });
}

void LibWebRTCVideoDecoderAdapterBackend::decodingFailed(uint64_t decoderGeneration)
{
    assertIsCurrent(m_queue.get());

    RELEASE_LOG_ERROR(WebRTC, "LibWebRTCVideoDecoderAdapter decoding failed");

    if (m_state != State::Ready || decoderGeneration != m_decoderGeneration)
        return;

    closeDecoder();
    m_shouldDropDeltaFrames = true;
    m_shouldRequestKeyFrame = true;
}

void LibWebRTCVideoDecoderAdapterBackend::closeDecoder()
{
    assertIsCurrent(m_queue.get());

    m_state = State::None;
    m_decoderColorSpace = { };
    m_pendingFrames.clear();

    if (RefPtr decoder = std::exchange(m_flushingDecoder, nullptr))
        decoder->close();
    if (RefPtr decoder = std::exchange(m_decoder, nullptr))
        decoder->close();
}

void LibWebRTCVideoDecoderAdapterBackend::decoderOutput(std::expected<VideoDecoder::DecodedFrame, String>&& result, uint64_t decoderGeneration)
{
    if (!result) {
        m_queue->dispatch([protectedThis = Ref { *this }, decoderGeneration] {
            protectedThis->decodingFailed(decoderGeneration);
        });
        return;
    }

    auto colorSpace = toWebRTCColorSpace(result->frame->colorSpace());
    auto rtpTimestamp = static_cast<uint32_t>(result->timestamp);

    webrtc::VideoFrame::Builder builder;
    builder.set_video_frame_buffer(createWebRTCVideoFrameBuffer(WTF::move(result->frame)))
        .set_timestamp_rtp(rtpTimestamp);
    if (colorSpace)
        builder.set_color_space(&*colorSpace);
    auto webrtcVideoFrame = builder.build();

    Locker locker { m_callbackLock };
    if (m_callback)
        m_callback->Decoded(webrtcVideoFrame);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(LibWebRTCVideoDecoderAdapter);

LibWebRTCVideoDecoderAdapter::LibWebRTCVideoDecoderAdapter(Description&& description, Ref<WorkQueue>&& queue)
    : m_description(description.isolatedCopy())
    , m_queue(WTF::move(queue))
    , m_backend(LibWebRTCVideoDecoderAdapterBackend::create(WTF::move(description), m_queue.copyRef()))
{
}

LibWebRTCVideoDecoderAdapter::~LibWebRTCVideoDecoderAdapter()
{
    protect(m_backend)->reset();
}

bool LibWebRTCVideoDecoderAdapter::Configure(const Settings&)
{
    if (m_shouldCreateBackend) {
        m_backend = LibWebRTCVideoDecoderAdapterBackend::create(Description { m_description }, m_queue.copyRef());
        m_shouldCreateBackend = false;
    }
    return true;
}

int32_t LibWebRTCVideoDecoderAdapter::RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback)
{
    protect(m_backend)->setDecodeCompleteCallback(callback);
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t LibWebRTCVideoDecoderAdapter::Release()
{
    protect(m_backend)->reset();
    // LibWebRTC may reuse this decoder after a configure call, we need to recreate a backend in that case.
    m_shouldCreateBackend = true;
    return WEBRTC_VIDEO_CODEC_OK;
}

LibWebRTCVideoDecoderAdapter::DecoderInfo LibWebRTCVideoDecoderAdapter::GetDecoderInfo() const
{
    // FIXME: We could provide the info on whether the VideoDecoder is HW or SW.
    DecoderInfo info;
    info.implementation_name = ImplementationName();
    return info;
}

const char* LibWebRTCVideoDecoderAdapter::ImplementationName() const
{
    return "WebCoreVideoDecoder";
}

int32_t LibWebRTCVideoDecoderAdapter::Decode(const webrtc::EncodedImage& image, int64_t)
{
    if (!image.data() || !image.size())
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;

    bool isKeyFrame = image.IsKey();

    // FIXME: We fallback to SW for VP9 SVC via libwebrtc while we should handle it directly via VideoDecoder.
    if (m_description.isVP9 && (m_description.hardwareAcceleration == WebCore::VideoDecoder::HardwareAcceleration::No || (isKeyFrame && image.SpatialIndex().value_or(0) > 0)))
        return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;

    Ref backend = m_backend;
    if (!backend->shouldDecode(isKeyFrame))
        return WEBRTC_VIDEO_CODEC_ERROR;

    VideoEncodedData frame {
        .data = SharedBuffer::create(DataSegment::Provider {[data = image.GetEncodedData(), size = image.size()] {
            return unsafeMakeSpan(data->data(), size);
        } }),
        .isKeyFrame = isKeyFrame,
        .timestamp = image.RtpTimestamp(),
        .duration = { }
    };
    backend->decode(WTF::move(frame), colorSpaceFromLibWebRTCColorSpace(image.ColorSpace()));

    return WEBRTC_VIDEO_CODEC_OK;
}

} // namespace WebCore

#endif // USE(LIBWEBRTC) && PLATFORM(COCOA)
