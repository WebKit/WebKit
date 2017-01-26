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

#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <webrtc/media/base/videoframe.h>

#include "CoreMediaSoftLink.h"

namespace WebCore {

RealtimeOutgoingVideoSource::RealtimeOutgoingVideoSource(Ref<RealtimeMediaSource>&& videoSource)
    : m_videoSource(WTFMove(videoSource))
{
    m_videoSource->addObserver(this);
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

void RealtimeOutgoingVideoSource::sourceHasMoreMediaData(MediaSample& sample)
{
    if (!m_sinks.size())
        return;

    ASSERT(sample.platformSample().type == PlatformSample::CMSampleBufferType);
    auto pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(sample.platformSample().sample.cmSampleBuffer));
    auto pixelFormatType = CVPixelBufferGetPixelFormatType(pixelBuffer);

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    uint8_t* src = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));

    // FIXME: Shouldn't we use RealtimeMediaSource::size()
    const auto& settings = m_videoSource->settings();

    // FIXME: We should not need to allocate one buffer per frame.
    auto dest = webrtc::I420Buffer::Create(settings.width(), settings.height());

    if (pixelFormatType == kCVPixelFormatType_420YpCbCr8Planar) {
        // We probably can memcpy the data directly
        webrtc::ConvertToI420(webrtc::kI420, src, 0, 0, settings.width(), settings.height(), 0, webrtc::kVideoRotation_0, dest);
    } else if (pixelFormatType == kCVPixelFormatType_32BGRA)
        webrtc::ConvertToI420(webrtc::kARGB, src, 0, 0, settings.width(), settings.height(), 0, webrtc::kVideoRotation_0, dest);
    else {
        // FIXME: Mock source conversion works with kBGRA while regular camera works with kARGB
        ASSERT(pixelFormatType == kCVPixelFormatType_32ARGB);
        webrtc::ConvertToI420(webrtc::kBGRA, src, 0, 0, settings.width(), settings.height(), 0, webrtc::kVideoRotation_0, dest);
    }

    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

    webrtc::VideoFrame frame(dest, 0, 0,  webrtc::kVideoRotation_0);
    for (auto* sink : m_sinks)
        sink->OnFrame(frame);
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
