/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_FAKEVIDEORENDERER_H_
#define MEDIA_BASE_FAKEVIDEORENDERER_H_

#include "api/video/video_frame.h"
#include "media/base/videosinkinterface.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/logging.h"

namespace cricket {

// Faked video renderer that has a callback for actions on rendering.
class FakeVideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  FakeVideoRenderer()
      : errors_(0),
        width_(0),
        height_(0),
        rotation_(webrtc::kVideoRotation_0),
        timestamp_us_(0),
        num_rendered_frames_(0),
        black_frame_(false) {}

  virtual void OnFrame(const webrtc::VideoFrame& frame) {
    rtc::CritScope cs(&crit_);
    // TODO(zhurunz) Check with VP8 team to see if we can remove this
    // tolerance on Y values. Some unit tests produce Y values close
    // to 16 rather than close to zero, for supposedly black frames.
    // Largest value observed is 34, e.g., running
    // PeerConnectionIntegrationTest.SendAndReceive16To9AspectRatio.
    black_frame_ = CheckFrameColorYuv(0, 48, 128, 128, 128, 128, &frame);
    // Treat unexpected frame size as error.
    ++num_rendered_frames_;
    width_ = frame.width();
    height_ = frame.height();
    rotation_ = frame.rotation();
    timestamp_us_ = frame.timestamp_us();
  }

  int errors() const { return errors_; }
  int width() const {
    rtc::CritScope cs(&crit_);
    return width_;
  }
  int height() const {
    rtc::CritScope cs(&crit_);
    return height_;
  }
  webrtc::VideoRotation rotation() const {
    rtc::CritScope cs(&crit_);
    return rotation_;
  }

  int64_t timestamp_us() const {
    rtc::CritScope cs(&crit_);
    return timestamp_us_;
  }
  int num_rendered_frames() const {
    rtc::CritScope cs(&crit_);
    return num_rendered_frames_;
  }
  bool black_frame() const {
    rtc::CritScope cs(&crit_);
    return black_frame_;
  }

 private:
  static bool CheckFrameColorYuv(uint8_t y_min,
                                 uint8_t y_max,
                                 uint8_t u_min,
                                 uint8_t u_max,
                                 uint8_t v_min,
                                 uint8_t v_max,
                                 const webrtc::VideoFrame* frame) {
    if (!frame || !frame->video_frame_buffer()) {
      return false;
    }
    rtc::scoped_refptr<const webrtc::I420BufferInterface> i420_buffer =
        frame->video_frame_buffer()->ToI420();
    // Y
    int y_width = frame->width();
    int y_height = frame->height();
    const uint8_t* y_plane = i420_buffer->DataY();
    const uint8_t* y_pos = y_plane;
    int32_t y_pitch = i420_buffer->StrideY();
    for (int i = 0; i < y_height; ++i) {
      for (int j = 0; j < y_width; ++j) {
        uint8_t y_value = *(y_pos + j);
        if (y_value < y_min || y_value > y_max) {
          return false;
        }
      }
      y_pos += y_pitch;
    }
    // U and V
    int chroma_width = i420_buffer->ChromaWidth();
    int chroma_height = i420_buffer->ChromaHeight();
    const uint8_t* u_plane = i420_buffer->DataU();
    const uint8_t* v_plane = i420_buffer->DataV();
    const uint8_t* u_pos = u_plane;
    const uint8_t* v_pos = v_plane;
    int32_t u_pitch = i420_buffer->StrideU();
    int32_t v_pitch = i420_buffer->StrideV();
    for (int i = 0; i < chroma_height; ++i) {
      for (int j = 0; j < chroma_width; ++j) {
        uint8_t u_value = *(u_pos + j);
        if (u_value < u_min || u_value > u_max) {
          return false;
        }
        uint8_t v_value = *(v_pos + j);
        if (v_value < v_min || v_value > v_max) {
          return false;
        }
      }
      u_pos += u_pitch;
      v_pos += v_pitch;
    }
    return true;
  }

  int errors_;
  int width_;
  int height_;
  webrtc::VideoRotation rotation_;
  int64_t timestamp_us_;
  int num_rendered_frames_;
  bool black_frame_;
  rtc::CriticalSection crit_;
};

}  // namespace cricket

#endif  // MEDIA_BASE_FAKEVIDEORENDERER_H_
