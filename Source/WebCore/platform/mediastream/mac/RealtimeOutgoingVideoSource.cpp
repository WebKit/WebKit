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
 * 3.  Neither the name of Apple Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "RealtimeOutgoingVideoSource.h"

#if USE(LIBWEBRTC)

#include <webrtc/api/video/i420_buffer.h>
#include <webrtc/common_video/include/corevideo_frame_buffer.h>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <webrtc/media/base/videoframe.h>
#include <wtf/MainThread.h>

#include "CoreMediaSoftLink.h"
#include "CoreVideoSoftLink.h"

namespace WebCore {

RealtimeOutgoingVideoSource::RealtimeOutgoingVideoSource(Ref<RealtimeMediaSource>&& videoSource)
    : m_videoSource(WTFMove(videoSource))
    , m_blackFrameTimer(*this, &RealtimeOutgoingVideoSource::sendOneBlackFrame)
{
    m_videoSource->addObserver(*this);
    setSizeFromSource();
}

bool RealtimeOutgoingVideoSource::setSource(Ref<RealtimeMediaSource>&& newSource)
{
    if (!m_initialSettings)
        m_initialSettings = m_videoSource->settings();

    auto newSettings = newSource->settings();

    if (m_initialSettings->width() < newSettings.width() || m_initialSettings->height() < newSettings.height())
        return false;

    m_videoSource->removeObserver(*this);
    m_videoSource = WTFMove(newSource);
    m_videoSource->addObserver(*this);

    setSizeFromSource();
    m_muted = m_videoSource->muted();
    m_enabled = m_videoSource->enabled();

    return true;
}

void RealtimeOutgoingVideoSource::stop()
{
    m_videoSource->removeObserver(*this);
    m_blackFrameTimer.stop();
    m_isStopped = true;
}

void RealtimeOutgoingVideoSource::sourceMutedChanged()
{
    ASSERT(m_muted != m_videoSource->muted());

    m_muted = m_videoSource->muted();

    if (m_muted && m_sinks.size() && m_enabled) {
        sendBlackFrames();
        return;
    }
    if (m_blackFrameTimer.isActive())
        m_blackFrameTimer.stop();
}

void RealtimeOutgoingVideoSource::sourceEnabledChanged()
{
    ASSERT(m_enabled != m_videoSource->enabled());

    m_enabled = m_videoSource->enabled();

    if (!m_enabled && m_sinks.size() && !m_muted) {
        sendBlackFrames();
        return;
    }
    if (m_blackFrameTimer.isActive())
        m_blackFrameTimer.stop();
}

void RealtimeOutgoingVideoSource::setSizeFromSource()
{
    const auto& settings = m_videoSource->settings();
    m_width = settings.width();
    m_height = settings.height();
}

bool RealtimeOutgoingVideoSource::GetStats(Stats*)
{
    return false;
}

void RealtimeOutgoingVideoSource::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants& sinkWants)
{
    ASSERT(!sinkWants.black_frames);

    if (sinkWants.rotation_applied)
        m_shouldApplyRotation = true;

    if (!m_sinks.contains(sink))
        m_sinks.append(sink);
}

void RealtimeOutgoingVideoSource::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
    m_sinks.removeFirst(sink);
}

void RealtimeOutgoingVideoSource::sendBlackFrames()
{
    if (!m_blackFrame) {
        auto frame = m_bufferPool.CreateBuffer(m_width, m_height);
        frame->SetToBlack();
        m_blackFrame = WTFMove(frame);
    }
    sendOneBlackFrame();
    m_blackFrameTimer.startRepeating(1_s);
}

void RealtimeOutgoingVideoSource::sendOneBlackFrame()
{
    sendFrame(rtc::scoped_refptr<webrtc::VideoFrameBuffer>(m_blackFrame));
}

void RealtimeOutgoingVideoSource::sendFrame(rtc::scoped_refptr<webrtc::VideoFrameBuffer>&& buffer)
{
    // FIXME: We should make AVVideoCaptureSource handle the rotation whenever possible.
    if (m_shouldApplyRotation && m_currentRotation != webrtc::kVideoRotation_0) {
        // This implementation is inefficient, we should rotate on the CMSampleBuffer directly instead of doing this double allocation.
        buffer = buffer->NativeToI420Buffer();
        buffer = webrtc::I420Buffer::Rotate(*buffer, m_currentRotation);
    }
    webrtc::VideoFrame frame(buffer, 0, 0, m_currentRotation);
    for (auto* sink : m_sinks)
        sink->OnFrame(frame);
}

void RealtimeOutgoingVideoSource::videoSampleAvailable(MediaSample& sample)
{
    if (!m_sinks.size())
        return;

    if (m_muted || !m_enabled)
        return;

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
        sendFrame(new rtc::RefCountedObject<webrtc::CoreVideoFrameBuffer>(pixelBuffer));
        return;
    }

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    auto* source = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));

    auto newBuffer = m_bufferPool.CreateBuffer(m_width, m_height);
    if (pixelFormatType == kCVPixelFormatType_32BGRA)
        webrtc::ConvertToI420(webrtc::kARGB, source, 0, 0, m_width, m_height, 0, webrtc::kVideoRotation_0, newBuffer);
    else {
        ASSERT(pixelFormatType == kCVPixelFormatType_32ARGB);
        webrtc::ConvertToI420(webrtc::kBGRA, source, 0, 0, m_width, m_height, 0, webrtc::kVideoRotation_0, newBuffer);
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    sendFrame(WTFMove(newBuffer));
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
