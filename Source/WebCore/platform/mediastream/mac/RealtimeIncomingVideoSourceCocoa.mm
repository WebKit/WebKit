/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RealtimeIncomingVideoSourceCocoa.h"

#if USE(LIBWEBRTC)

#import "CVUtilities.h"
#import "Logging.h"
#import "VideoFrameCV.h"
#import "VideoFrameLibWebRTC.h"
#import <wtf/cf/TypeCastsCF.h>

ALLOW_UNUSED_PARAMETERS_BEGIN
#import <webrtc/sdk/WebKit/WebKitUtilities.h>
ALLOW_UNUSED_PARAMETERS_END

#import "CoreVideoSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

Ref<RealtimeIncomingVideoSource> RealtimeIncomingVideoSource::create(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& trackId)
{
    auto source = RealtimeIncomingVideoSourceCocoa::create(WTFMove(videoTrack), WTFMove(trackId));
    source->start();
    return WTFMove(source);
}

Ref<RealtimeIncomingVideoSourceCocoa> RealtimeIncomingVideoSourceCocoa::create(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& trackId)
{
    return adoptRef(*new RealtimeIncomingVideoSourceCocoa(WTFMove(videoTrack), WTFMove(trackId)));
}

RealtimeIncomingVideoSourceCocoa::RealtimeIncomingVideoSourceCocoa(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& videoTrackId)
    : RealtimeIncomingVideoSource(WTFMove(videoTrack), WTFMove(videoTrackId))
{
}

CVPixelBufferPoolRef RealtimeIncomingVideoSourceCocoa::pixelBufferPool(size_t width, size_t height, webrtc::BufferType bufferType) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    if (!m_pixelBufferPool || m_pixelBufferPoolWidth != width || m_pixelBufferPoolHeight != height || m_pixelBufferPoolBufferType != bufferType) {
        OSType poolBufferType;
        switch (bufferType) {
        case webrtc::BufferType::I420:
            poolBufferType = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
            break;
        case webrtc::BufferType::I010:
            poolBufferType = kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
            break;
        default:
            return nullptr;
        }

        auto result = createInMemoryCVPixelBufferPool(width, height, poolBufferType);
        if (!result) {
            RELEASE_LOG_ERROR(WebRTC, "RealtimeIncomingVideoSourceCocoa failed creating buffer pool with error %d", result.error());
            return nullptr;
        }

        m_pixelBufferPool = WTFMove(*result);
        m_pixelBufferPoolWidth = width;
        m_pixelBufferPoolHeight = height;
        m_pixelBufferPoolBufferType = bufferType;
    }
    return m_pixelBufferPool.get();
}

Ref<VideoFrame> RealtimeIncomingVideoSourceCocoa::createVideoSampleFromCVPixelBuffer(CVPixelBufferRef pixelBuffer, VideoFrame::Rotation rotation, int64_t timeStamp)
{
    return VideoFrameCV::create(MediaTime(timeStamp, 1000000), false, rotation, pixelBuffer);
}

RefPtr<VideoFrame> RealtimeIncomingVideoSourceCocoa::toVideoFrame(const webrtc::VideoFrame& frame, VideoFrame::Rotation rotation)
{
    if (muted()) {
        if (!m_blackFrame || m_blackFrameWidth != frame.width() || m_blackFrameHeight != frame.height()) {
            m_blackFrameWidth = frame.width();
            m_blackFrameHeight = frame.height();
            m_blackFrame = createBlackPixelBuffer(m_blackFrameWidth, m_blackFrameHeight);
        }
        return createVideoSampleFromCVPixelBuffer(m_blackFrame.get(), rotation, frame.timestamp_us());
    }

    if (auto* provider = videoFrameBufferProvider(frame)) {
        // The only supported provider is VideoFrame.
        auto* videoFrame = static_cast<VideoFrame*>(provider);
        videoFrame->initializeCharacteristics(MediaTime { frame.timestamp_us(), 1000000 }, false, rotation);
        return videoFrame;
    }

    // If we already have a CVPixelBufferRef, use it directly.
    if (auto pixelBuffer = webrtc::pixelBufferFromFrame(frame))
        return createVideoSampleFromCVPixelBuffer(pixelBuffer, rotation, frame.timestamp_us());

    // In case of in memory libwebrtc samples, we have non interleaved YUV data, let's lazily create CVPixelBuffers if needed.
    return VideoFrameLibWebRTC::create(MediaTime(frame.timestamp_us(), 1000000), false, rotation, VideoFrameLibWebRTC::colorSpaceFromFrame(frame), frame.video_frame_buffer(), [protectedThis = Ref { *this }, this](auto& buffer) {
        return adoptCF(webrtc::createPixelBufferFromFrameBuffer(buffer, [this](size_t width, size_t height, webrtc::BufferType bufferType) -> CVPixelBufferRef {
            Locker lock(m_pixelBufferPoolLock);
            auto pixelBufferPool = this->pixelBufferPool(width, height, bufferType);
            if (!pixelBufferPool)
                return nullptr;
            CVPixelBufferRef pixelBuffer = nullptr;
            auto status = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, m_pixelBufferPool.get(), &pixelBuffer);
            if (status != kCVReturnSuccess) {
                ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "Failed creating a pixel buffer with error ", status);
                return nullptr;
            }
            return pixelBuffer;
        }));
    });
}

void RealtimeIncomingVideoSourceCocoa::OnFrame(const webrtc::VideoFrame& webrtcVideoFrame)
{
    if (!isProducingData())
        return;

    unsigned width = webrtcVideoFrame.width();
    unsigned height = webrtcVideoFrame.height();

    VideoFrame::Rotation rotation;
    switch (webrtcVideoFrame.rotation()) {
    case webrtc::kVideoRotation_0:
        rotation = VideoFrame::Rotation::None;
        break;
    case webrtc::kVideoRotation_180:
        rotation = VideoFrame::Rotation::UpsideDown;
        break;
    case webrtc::kVideoRotation_90:
        rotation = VideoFrame::Rotation::Right;
        std::swap(width, height);
        break;
    case webrtc::kVideoRotation_270:
        rotation = VideoFrame::Rotation::Left;
        std::swap(width, height);
        break;
    }

#if !RELEASE_LOG_DISABLED
    ALWAYS_LOG_IF(loggerPtr() && !(++m_numberOfFrames % 60), LOGIDENTIFIER, "frame ", m_numberOfFrames, ", rotation ", webrtcVideoFrame.rotation(), " size ", width, "x", height);
#endif

    auto videoFrame = toVideoFrame(webrtcVideoFrame, rotation);
    if (!videoFrame)
        return;

    setIntrinsicSize(IntSize(width, height));
    videoFrameAvailable(*videoFrame, metadataFromVideoFrame(webrtcVideoFrame));
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
