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
#include <webrtc/sdk/WebKit/WebKitUtilities.h>
#include <webrtc/sdk/objc/Framework/Classes/Video/corevideo_frame_buffer.h>
#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

namespace libyuv {
extern "C" {
typedef enum RotationMode {
  kRotate0 = 0,      // No rotation.
  kRotate90 = 90,    // Rotate 90 degrees clockwise.
  kRotate180 = 180,  // Rotate 180 degrees.
  kRotate270 = 270,  // Rotate 270 degrees clockwise.

  // Deprecated.
  kRotateNone = 0,
  kRotateClockwise = 90,
  kRotateCounterClockwise = 270,
} RotationModeEnum;

int ConvertToI420(const uint8_t* src_frame,
                  size_t src_size,
                  uint8_t* dst_y,
                  int dst_stride_y,
                  uint8_t* dst_u,
                  int dst_stride_u,
                  uint8_t* dst_v,
                  int dst_stride_v,
                  int crop_x,
                  int crop_y,
                  int src_width,
                  int src_height,
                  int crop_width,
                  int crop_height,
                  RotationMode rotation,
                  uint32_t format);
}
}

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

static inline int ConvertToI420(webrtc::VideoType src_video_type,
                  const uint8_t* src_frame,
                  int crop_x,
                  int crop_y,
                  int src_width,
                  int src_height,
                  size_t sample_size,
                  libyuv::RotationMode rotation,
                  webrtc::I420Buffer* dst_buffer) {
  int dst_width = dst_buffer->width();
  int dst_height = dst_buffer->height();
  // LibYuv expects pre-rotation values for dst.
  // Stride values should correspond to the destination values.
  if (rotation == libyuv::kRotate90 || rotation == libyuv::kRotate270) {
    std::swap(dst_width, dst_height);
  }
  return libyuv::ConvertToI420(
      src_frame, sample_size,
      dst_buffer->MutableDataY(), dst_buffer->StrideY(),
      dst_buffer->MutableDataU(), dst_buffer->StrideU(),
      dst_buffer->MutableDataV(), dst_buffer->StrideV(),
      crop_x, crop_y,
      src_width, src_height,
      dst_width, dst_height,
      rotation,
      webrtc::ConvertVideoType(src_video_type));
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
        rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer = webrtc::pixelBufferToFrame(pixelBuffer);
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
        ConvertToI420(webrtc::VideoType::kARGB, source, 0, 0, m_width, m_height, 0, libyuv::kRotate0, newBuffer);
    else {
        ASSERT(pixelFormatType == kCVPixelFormatType_32ARGB);
        ConvertToI420(webrtc::VideoType::kBGRA, source, 0, 0, m_width, m_height, 0, libyuv::kRotate0, newBuffer);
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    if (m_shouldApplyRotation && m_currentRotation != webrtc::kVideoRotation_0)
        newBuffer = webrtc::I420Buffer::Rotate(*newBuffer, m_currentRotation);
    sendFrame(WTFMove(newBuffer));
}


} // namespace WebCore

#endif // USE(LIBWEBRTC)
