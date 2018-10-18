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
#include "RealtimeIncomingVideoSourceCocoa.h"

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/video/i420_buffer.h>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <webrtc/sdk/WebKit/WebKitUtilities.h>

ALLOW_UNUSED_PARAMETERS_END

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

    RetainPtr<CVPixelBufferRef> convertedBuffer = pixelBuffer;
    if (pixelFormatType != preferedPixelBufferFormat())
        convertedBuffer = convertToYUV(pixelBuffer);

    if (m_shouldApplyRotation && m_currentRotation != webrtc::kVideoRotation_0)
        convertedBuffer = rotatePixelBuffer(convertedBuffer.get(), m_currentRotation);

    sendFrame(webrtc::pixelBufferToFrame(convertedBuffer.get()));
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer> RealtimeOutgoingVideoSourceCocoa::createBlackFrame(size_t  width, size_t  height)
{
    return webrtc::pixelBufferToFrame(createBlackPixelBuffer(width, height).get());
}


} // namespace WebCore

#endif // USE(LIBWEBRTC)
