/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_FRAME_H_
#define API_VIDEO_VIDEO_FRAME_H_

#include <stdint.h>

#include "api/video/video_rotation.h"
#include "api/video/video_frame_buffer.h"

namespace webrtc {

class VideoFrame {
 public:
  // TODO(nisse): This constructor is consistent with the now deleted
  // cricket::WebRtcVideoFrame. We should consider whether or not we
  // want to stick to this style and deprecate the other constructor.
  VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
             webrtc::VideoRotation rotation,
             int64_t timestamp_us);

  // Preferred constructor.
  VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
             uint32_t timestamp,
             int64_t render_time_ms,
             VideoRotation rotation);

  ~VideoFrame();

  // Support move and copy.
  VideoFrame(const VideoFrame&);
  VideoFrame(VideoFrame&&);
  VideoFrame& operator=(const VideoFrame&);
  VideoFrame& operator=(VideoFrame&&);

  // Get frame width.
  int width() const;
  // Get frame height.
  int height() const;
  // Get frame size in pixels.
  uint32_t size() const;

  // System monotonic clock, same timebase as rtc::TimeMicros().
  int64_t timestamp_us() const { return timestamp_us_; }
  void set_timestamp_us(int64_t timestamp_us) { timestamp_us_ = timestamp_us; }

  // TODO(nisse): After the cricket::VideoFrame and webrtc::VideoFrame
  // merge, timestamps other than timestamp_us will likely be
  // deprecated.

  // Set frame timestamp (90kHz).
  void set_timestamp(uint32_t timestamp) { timestamp_rtp_ = timestamp; }

  // Get frame timestamp (90kHz).
  uint32_t timestamp() const { return timestamp_rtp_; }

  // For now, transport_frame_id and rtp timestamp are the same.
  // TODO(nisse): Must be handled differently for QUIC.
  uint32_t transport_frame_id() const { return timestamp(); }

  // Set capture ntp time in milliseconds.
  // TODO(nisse): Deprecated. Migrate all users to timestamp_us().
  void set_ntp_time_ms(int64_t ntp_time_ms) { ntp_time_ms_ = ntp_time_ms; }

  // Get capture ntp time in milliseconds.
  // TODO(nisse): Deprecated. Migrate all users to timestamp_us().
  int64_t ntp_time_ms() const { return ntp_time_ms_; }

  // Naming convention for Coordination of Video Orientation. Please see
  // http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/ts_126114v120700p.pdf
  //
  // "pending rotation" or "pending" = a frame that has a VideoRotation > 0.
  //
  // "not pending" = a frame that has a VideoRotation == 0.
  //
  // "apply rotation" = modify a frame from being "pending" to being "not
  //                    pending" rotation (a no-op for "unrotated").
  //
  VideoRotation rotation() const { return rotation_; }
  void set_rotation(VideoRotation rotation) { rotation_ = rotation; }

  // Get render time in milliseconds.
  // TODO(nisse): Deprecated. Migrate all users to timestamp_us().
  int64_t render_time_ms() const;

  // Return the underlying buffer. Never nullptr for a properly
  // initialized VideoFrame.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer() const;

  // TODO(nisse): Deprecated.
  // Return true if the frame is stored in a texture.
  bool is_texture() const {
    return video_frame_buffer()->type() == VideoFrameBuffer::Type::kNative;
  }

 private:
  // An opaque reference counted handle that stores the pixel data.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer_;
  uint32_t timestamp_rtp_;
  int64_t ntp_time_ms_;
  int64_t timestamp_us_;
  VideoRotation rotation_;
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_FRAME_H_
