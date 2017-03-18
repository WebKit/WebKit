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

#include <webrtc/common_video/include/corevideo_frame_buffer.h>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <webrtc/media/base/videoframe.h>

#include "CoreMediaSoftLink.h"
#include "CoreVideoSoftLink.h"

namespace WebCore {

RealtimeOutgoingVideoSource::RealtimeOutgoingVideoSource(Ref<RealtimeMediaSource>&& videoSource)
    : m_videoSource(WTFMove(videoSource))
{
    m_videoSource->addObserver(*this);
}

void RealtimeOutgoingVideoSource::stop()
{
    m_videoSource->removeObserver(*this);
}

void RealtimeOutgoingVideoSource::sourceMutedChanged()
{
    m_muted = m_videoSource->muted();
}

void RealtimeOutgoingVideoSource::sourceEnabledChanged()
{
    m_enabled = m_videoSource->enabled();
}

bool RealtimeOutgoingVideoSource::GetStats(Stats*)
{
    return false;
}

void RealtimeOutgoingVideoSource::AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink, const rtc::VideoSinkWants&)
{
    // FIXME: support sinkWants
    if (!m_sinks.contains(sink))
        m_sinks.append(sink);
}

void RealtimeOutgoingVideoSource::RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink)
{
    m_sinks.removeFirst(sink);
}

void RealtimeOutgoingVideoSource::sendFrame(rtc::scoped_refptr<webrtc::VideoFrameBuffer>&& buffer, webrtc::VideoRotation rotation)
{
    webrtc::VideoFrame frame(buffer, 0, 0, rotation);
    for (auto* sink : m_sinks)
        sink->OnFrame(frame);
}

void RealtimeOutgoingVideoSource::videoSampleAvailable(MediaSample& sample)
{
    if (!m_sinks.size())
        return;

    // FIXME: Shouldn't we use RealtimeMediaSource::size()
    const auto& settings = m_videoSource->settings();

    if (m_muted || !m_enabled) {
        auto blackBuffer = m_bufferPool.CreateBuffer(settings.width(), settings.height());
        blackBuffer->SetToBlack();
        sendFrame(WTFMove(blackBuffer), webrtc::kVideoRotation_0);
        return;
    }

    ASSERT(sample.platformSample().type == PlatformSample::CMSampleBufferType);
    auto pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(sample.platformSample().sample.cmSampleBuffer));
    auto pixelFormatType = CVPixelBufferGetPixelFormatType(pixelBuffer);

    webrtc::VideoRotation rotation;
    switch (sample.videoOrientation()) {
    case MediaSample::VideoOrientation::Unknown:
    case MediaSample::VideoOrientation::Portrait:
        rotation = webrtc::kVideoRotation_0;
        break;
    case MediaSample::VideoOrientation::PortraitUpsideDown:
        rotation = webrtc::kVideoRotation_180;
        break;
    case MediaSample::VideoOrientation::LandscapeRight:
        rotation = webrtc::kVideoRotation_90;
        break;
    case MediaSample::VideoOrientation::LandscapeLeft:
        rotation = webrtc::kVideoRotation_270;
        break;
    }

    if (pixelFormatType == kCVPixelFormatType_420YpCbCr8Planar || pixelFormatType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
        sendFrame(new rtc::RefCountedObject<webrtc::CoreVideoFrameBuffer>(pixelBuffer), rotation);
        return;
    }

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    auto* source = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));

    auto newBuffer = m_bufferPool.CreateBuffer(settings.width(), settings.height());
    if (pixelFormatType == kCVPixelFormatType_32BGRA)
        webrtc::ConvertToI420(webrtc::kARGB, source, 0, 0, settings.width(), settings.height(), 0, webrtc::kVideoRotation_0, newBuffer);
    else {
        // FIXME: Mock source conversion works with kBGRA while regular camera works with kARGB
        ASSERT(pixelFormatType == kCVPixelFormatType_32ARGB);
        webrtc::ConvertToI420(webrtc::kBGRA, source, 0, 0, settings.width(), settings.height(), 0, webrtc::kVideoRotation_0, newBuffer);
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    sendFrame(WTFMove(newBuffer), rotation);
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
