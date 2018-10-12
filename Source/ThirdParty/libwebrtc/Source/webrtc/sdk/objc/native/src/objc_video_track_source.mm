/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/native/src/objc_video_track_source.h"

#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"

#include "api/video/i420_buffer.h"
#include "sdk/objc/native/src/objc_frame_buffer.h"

@interface RTCObjCVideoSourceAdapter ()
@property(nonatomic) webrtc::ObjCVideoTrackSource *objCVideoTrackSource;
@end

@implementation RTCObjCVideoSourceAdapter

@synthesize objCVideoTrackSource = _objCVideoTrackSource;

- (void)capturer:(RTCVideoCapturer *)capturer didCaptureVideoFrame:(RTCVideoFrame *)frame {
  _objCVideoTrackSource->OnCapturedFrame(frame);
}

@end

namespace webrtc {

ObjCVideoTrackSource::ObjCVideoTrackSource() :
  AdaptedVideoTrackSource(/* required resolution alignment */ 4) {}

ObjCVideoTrackSource::ObjCVideoTrackSource(RTCObjCVideoSourceAdapter *adapter) : adapter_(adapter) {
  adapter_.objCVideoTrackSource = this;
}

bool ObjCVideoTrackSource::is_screencast() const {
  return false;
}

absl::optional<bool> ObjCVideoTrackSource::needs_denoising() const {
  return false;
}

MediaSourceInterface::SourceState ObjCVideoTrackSource::state() const {
  return SourceState::kLive;
}

bool ObjCVideoTrackSource::remote() const {
  return false;
}

void ObjCVideoTrackSource::OnOutputFormatRequest(int width, int height, int fps) {
  cricket::VideoFormat format(width, height, cricket::VideoFormat::FpsToInterval(fps), 0);
  video_adapter()->OnOutputFormatRequest(format);
}

void ObjCVideoTrackSource::OnCapturedFrame(RTCVideoFrame *frame) {
  const int64_t timestamp_us = frame.timeStampNs / rtc::kNumNanosecsPerMicrosec;
  const int64_t translated_timestamp_us =
      timestamp_aligner_.TranslateTimestamp(timestamp_us, rtc::TimeMicros());

  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;
  if (!AdaptFrame(frame.width,
                  frame.height,
                  timestamp_us,
                  &adapted_width,
                  &adapted_height,
                  &crop_width,
                  &crop_height,
                  &crop_x,
                  &crop_y)) {
    return;
  }

  rtc::scoped_refptr<VideoFrameBuffer> buffer;
  if (adapted_width == frame.width && adapted_height == frame.height) {
    // No adaption - optimized path.
    buffer = new rtc::RefCountedObject<ObjCFrameBuffer>(frame.buffer);
  } else if ([frame.buffer isKindOfClass:[RTCCVPixelBuffer class]]) {
    // Adapted CVPixelBuffer frame.
    RTCCVPixelBuffer *rtcPixelBuffer = (RTCCVPixelBuffer *)frame.buffer;
    buffer = new rtc::RefCountedObject<ObjCFrameBuffer>([[RTCCVPixelBuffer alloc]
        initWithPixelBuffer:rtcPixelBuffer.pixelBuffer
               adaptedWidth:adapted_width
              adaptedHeight:adapted_height
                  cropWidth:crop_width
                 cropHeight:crop_height
                      cropX:crop_x + rtcPixelBuffer.cropX
                      cropY:crop_y + rtcPixelBuffer.cropY]);
  } else {
    // Adapted I420 frame.
    // TODO(magjed): Optimize this I420 path.
    rtc::scoped_refptr<I420Buffer> i420_buffer = I420Buffer::Create(adapted_width, adapted_height);
    buffer = new rtc::RefCountedObject<ObjCFrameBuffer>(frame.buffer);
    i420_buffer->CropAndScaleFrom(*buffer->ToI420(), crop_x, crop_y, crop_width, crop_height);
    buffer = i420_buffer;
  }

  // Applying rotation is only supported for legacy reasons and performance is
  // not critical here.
  VideoRotation rotation = static_cast<VideoRotation>(frame.rotation);
  if (apply_rotation() && rotation != kVideoRotation_0) {
    buffer = I420Buffer::Rotate(*buffer->ToI420(), rotation);
    rotation = kVideoRotation_0;
  }

  OnFrame(VideoFrame(buffer, rotation, translated_timestamp_us));
}

}  // namespace webrtc
