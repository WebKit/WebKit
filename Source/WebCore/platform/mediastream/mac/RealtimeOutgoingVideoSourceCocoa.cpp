/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "CVUtilities.h"
#include "ImageRotationSessionVT.h"
#include "Logging.h"
#include "RealtimeIncomingVideoSourceCocoa.h"
#include "RealtimeVideoUtilities.h"
#include "VideoFrameLibWebRTC.h"

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/video/i420_buffer.h>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <webrtc/webkit_sdk/WebKit/WebKitUtilities.h>

ALLOW_UNUSED_PARAMETERS_END

#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

namespace WebCore {

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

RealtimeOutgoingVideoSourceCocoa::~RealtimeOutgoingVideoSourceCocoa() = default;

void RealtimeOutgoingVideoSourceCocoa::videoFrameAvailable(VideoFrame& videoFrame, VideoFrameTimeMetadata)
{
    ASSERT(!m_shouldApplyRotation);

#if !RELEASE_LOG_DISABLED
    if (!(++m_numberOfFrames % 60))
        ALWAYS_LOG(LOGIDENTIFIER, "frame ", m_numberOfFrames);
#endif

    switch (videoFrame.rotation()) {
    case VideoFrame::Rotation::None:
        m_currentRotation = webrtc::kVideoRotation_0;
        break;
    case VideoFrame::Rotation::UpsideDown:
        m_currentRotation = webrtc::kVideoRotation_180;
        break;
    case VideoFrame::Rotation::Right:
        m_currentRotation = webrtc::kVideoRotation_90;
        break;
    case VideoFrame::Rotation::Left:
        m_currentRotation = webrtc::kVideoRotation_270;
        break;
    }

    auto videoFrameScaling = this->videoFrameScaling();
    bool shouldApplyRotation = m_shouldApplyRotation && m_currentRotation != webrtc::kVideoRotation_0;
    if (!shouldApplyRotation) {
        if (videoFrame.isRemoteProxy()) {
            Ref remoteVideoFrame { videoFrame };
            auto size = videoFrame.presentationSize();
            sendFrame(webrtc::toWebRTCVideoFrameBuffer(&remoteVideoFrame.leakRef(),
                [](auto* pointer) { return static_cast<VideoFrame*>(pointer)->pixelBuffer(); },
                [](auto* pointer) { static_cast<VideoFrame*>(pointer)->deref(); },
                static_cast<int>(size.width() * videoFrameScaling), static_cast<int>(size.height() * videoFrameScaling)));
            return;
        }
        if (auto* webrtcVideoFrame = dynamicDowncast<VideoFrameLibWebRTC>(videoFrame)) {
            auto webrtcBuffer = webrtcVideoFrame->buffer();
            if (videoFrameScaling != 1)
                webrtcBuffer = webrtcBuffer->Scale(webrtcBuffer->width() * videoFrameScaling, webrtcBuffer->height() * videoFrameScaling);
            sendFrame(WTFMove(webrtcBuffer));
            return;
        }
    }

#if ASSERT_ENABLED
    auto pixelFormat = videoFrame.pixelFormat();
    // FIXME: We should use a pixel conformer for other pixel formats and kCVPixelFormatType_32BGRA.
    ASSERT(pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange || pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange || pixelFormat == kCVPixelFormatType_32BGRA);
#endif

    RefPtr<VideoFrame> rotatedVideoFrame;
    if (shouldApplyRotation) {
        if (!m_rotationSession)
            m_rotationSession = makeUnique<ImageRotationSessionVT>(ImageRotationSessionVT::ShouldUseIOSurface::No);
        rotatedVideoFrame = m_rotationSession->applyRotation(videoFrame);
    }
    RetainPtr<CVPixelBufferRef> convertedBuffer = rotatedVideoFrame ? rotatedVideoFrame->pixelBuffer() : videoFrame.pixelBuffer();

    auto webrtcBuffer = webrtc::pixelBufferToFrame(convertedBuffer.get());
    if (videoFrameScaling != 1)
        webrtcBuffer = webrtcBuffer->Scale(webrtcBuffer->width() * videoFrameScaling, webrtcBuffer->height() * videoFrameScaling);

    sendFrame(WTFMove(webrtcBuffer));
}

webrtc::scoped_refptr<webrtc::VideoFrameBuffer> RealtimeOutgoingVideoSourceCocoa::createBlackFrame(size_t  width, size_t  height)
{
    return webrtc::pixelBufferToFrame(createBlackPixelBuffer(width, height).get());
}


} // namespace WebCore

#endif // USE(LIBWEBRTC)
