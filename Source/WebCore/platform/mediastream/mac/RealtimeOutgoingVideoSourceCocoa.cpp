/*
 * Copyright (C) 2017 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RealtimeOutgoingVideoSourceCocoa.h"

#if USE(LIBWEBRTC)

#include "Logging.h"
#include <webrtc/api/video/i420_buffer.h>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <webrtc/sdk/objc/Framework/Classes/Video/corevideo_frame_buffer.h>

#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

namespace WebCore {
using namespace PAL;

Ref<RealtimeOutgoingVideoSource> RealtimeOutgoingVideoSource::create(Ref<MediaStreamTrackPrivate>&& videoSource)
{
    return RealtimeOutgoingVideoSourceCocoa::create(WTFMove(videoSource));
}

Ref<RealtimeOutgoingVideoSourceCocoa> RealtimeOutgoingVideoSourceCocoa::create(Ref<MediaStreamTrackPrivate>&& videoSource)
{
    return adoptRef(*new RealtimeOutgoingVideoSourceCocoa(WTFMove(videoSource)));
}

RealtimeOutgoingVideoSourceCocoa::RealtimeOutgoingVideoSourceCocoa(Ref<MediaStreamTrackPrivate>&& videoSource)
    : RealtimeOutgoingVideoSource(WTFMove(videoSource))
{
}

void RealtimeOutgoingVideoSourceCocoa::sampleBufferUpdated(MediaStreamTrackPrivate&, MediaSample& sample)
{
    if (!m_sinks.size())
        return;

    if (m_muted || !m_enabled)
        return;

#if !RELEASE_LOG_DISABLED
    if (!(++m_numberOfFrames % 30))
        RELEASE_LOG(MediaStream, "RealtimeOutgoingVideoSourceCocoa::sendFrame %zu frame", m_numberOfFrames);
#endif

    switch (sample.videoRotation()) {
    case MediaSample::VideoRotation::None:
        m_currentRotation = webrtc::kVideoRotation_0;
        break;
    case MediaSample::VideoRotation::UpsideDown:
        m_currentRotation = webrtc::kVideoRotation_180;
        break;
    case MediaSample::VideoRotation::Right:
        m_currentRotation = webrtc::kVideoRotation_90;
        break;
    case MediaSample::VideoRotation::Left:
        m_currentRotation = webrtc::kVideoRotation_270;
        break;
    }

    ASSERT(sample.platformSample().type == PlatformSample::CMSampleBufferType);
    auto pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(sample.platformSample().sample.cmSampleBuffer));
    auto pixelFormatType = CVPixelBufferGetPixelFormatType(pixelBuffer);

    if (pixelFormatType == kCVPixelFormatType_420YpCbCr8Planar || pixelFormatType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
        rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer = new rtc::RefCountedObject<webrtc::CoreVideoFrameBuffer>(pixelBuffer);
        if (m_shouldApplyRotation && m_currentRotation != webrtc::kVideoRotation_0) {
            // FIXME: We should make AVVideoCaptureSource handle the rotation whenever possible.
            // This implementation is inefficient, we should rotate on the CMSampleBuffer directly instead of doing this double allocation.
            auto rotatedBuffer = buffer->ToI420();
            ASSERT(rotatedBuffer);
            buffer = webrtc::I420Buffer::Rotate(*rotatedBuffer, m_currentRotation);
        }
        sendFrame(WTFMove(buffer));
        return;
    }

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    auto* source = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));

    ASSERT(m_width);
    ASSERT(m_height);

    auto newBuffer = m_bufferPool.CreateBuffer(m_width, m_height);
    ASSERT(newBuffer);
    if (!newBuffer) {
        RELEASE_LOG(WebRTC, "RealtimeOutgoingVideoSourceCocoa::videoSampleAvailable unable to allocate buffer for conversion to YUV");
        return;
    }
    if (pixelFormatType == kCVPixelFormatType_32BGRA)
        webrtc::ConvertToI420(webrtc::VideoType::kARGB, source, 0, 0, m_width, m_height, 0, webrtc::kVideoRotation_0, newBuffer);
    else {
        ASSERT(pixelFormatType == kCVPixelFormatType_32ARGB);
        webrtc::ConvertToI420(webrtc::VideoType::kBGRA, source, 0, 0, m_width, m_height, 0, webrtc::kVideoRotation_0, newBuffer);
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    if (m_shouldApplyRotation && m_currentRotation != webrtc::kVideoRotation_0)
        newBuffer = webrtc::I420Buffer::Rotate(*newBuffer, m_currentRotation);
    sendFrame(WTFMove(newBuffer));
}


} // namespace WebCore

#endif // USE(LIBWEBRTC)
