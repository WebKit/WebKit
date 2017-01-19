/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_FRAME_H_
#define WEBRTC_VIDEO_FRAME_H_

#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/common_video/rotation.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class VideoFrame {
 public:
  // TODO(nisse): Deprecated. Using the default constructor violates the
  // reasonable assumption that video_frame_buffer() returns a valid buffer.
  VideoFrame();

  // TODO(nisse): This constructor is consistent with
  // cricket::WebRtcVideoFrame. After the class
  // cricket::WebRtcVideoFrame and its baseclass cricket::VideoFrame
  // are deleted, we should consider whether or not we want to stick
  // to this style and deprecate the other constructors.
  VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
             webrtc::VideoRotation rotation,
             int64_t timestamp_us);

  // Preferred constructor.
  VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
             uint32_t timestamp,
             int64_t render_time_ms,
             VideoRotation rotation);

  // Support move and copy.
  VideoFrame(const VideoFrame&) = default;
  VideoFrame(VideoFrame&&) = default;
  VideoFrame& operator=(const VideoFrame&) = default;
  VideoFrame& operator=(VideoFrame&&) = default;

  // Get frame width.
  int width() const;

  // Get frame height.
  int height() const;

  // System monotonic clock, same timebase as rtc::TimeMicros().
  int64_t timestamp_us() const { return timestamp_us_; }
  void set_timestamp_us(int64_t timestamp_us) {
    timestamp_us_ = timestamp_us;
  }

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
  void set_ntp_time_ms(int64_t ntp_time_ms) {
    ntp_time_ms_ = ntp_time_ms;
  }

  // Get capture ntp time in milliseconds.
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
  void set_rotation(VideoRotation rotation) {
    rotation_ = rotation;
  }

  // Set render time in milliseconds.
  void set_render_time_ms(int64_t render_time_ms) {
    set_timestamp_us(render_time_ms * rtc::kNumMicrosecsPerMillisec);;
  }

  // Get render time in milliseconds.
  int64_t render_time_ms() const {
    return timestamp_us() / rtc::kNumMicrosecsPerMillisec;
  }

  // Return true if and only if video_frame_buffer() is null. Which is possible
  // only if the object was default-constructed.
  // TODO(nisse): Deprecated. Should be deleted in the cricket::VideoFrame and
  // webrtc::VideoFrame merge. The intention is that video_frame_buffer() never
  // should return nullptr. To handle potentially uninitialized or non-existent
  // frames, consider using rtc::Optional. Otherwise, IsZeroSize() can be
  // replaced by video_frame_buffer() == nullptr.
  bool IsZeroSize() const;

  // Return the underlying buffer. Never nullptr for a properly
  // initialized VideoFrame.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer() const;

  // Return true if the frame is stored in a texture.
  bool is_texture() const {
    return video_frame_buffer() &&
           video_frame_buffer()->native_handle() != nullptr;
  }

 private:
  // An opaque reference counted handle that stores the pixel data.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer_;
  uint32_t timestamp_rtp_;
  int64_t ntp_time_ms_;
  int64_t timestamp_us_;
  VideoRotation rotation_;
};


// TODO(pbos): Rename EncodedFrame and reformat this class' members.
class EncodedImage {
 public:
  static const size_t kBufferPaddingBytesH264;

  // Some decoders require encoded image buffers to be padded with a small
  // number of additional bytes (due to over-reading byte readers).
  static size_t GetBufferPaddingBytes(VideoCodecType codec_type);

  EncodedImage() : EncodedImage(nullptr, 0, 0) {}

  EncodedImage(uint8_t* buffer, size_t length, size_t size)
      : _buffer(buffer), _length(length), _size(size) {}

  struct AdaptReason {
    AdaptReason()
        : quality_resolution_downscales(-1),
          bw_resolutions_disabled(-1) {}

    int quality_resolution_downscales;  // Number of times this frame is down
                                        // scaled in resolution due to quality.
                                        // Or -1 if information is not provided.
    int bw_resolutions_disabled;  // Number of resolutions that are not sent
                                  // due to bandwidth for this frame.
                                  // Or -1 if information is not provided.
  };
  uint32_t _encodedWidth = 0;
  uint32_t _encodedHeight = 0;
  uint32_t _timeStamp = 0;
  // NTP time of the capture time in local timebase in milliseconds.
  int64_t ntp_time_ms_ = 0;
  int64_t capture_time_ms_ = 0;
  FrameType _frameType = kVideoFrameDelta;
  uint8_t* _buffer;
  size_t _length;
  size_t _size;
  VideoRotation rotation_ = kVideoRotation_0;
  bool _completeFrame = false;
  AdaptReason adapt_reason_;
  int qp_ = -1;  // Quantizer value.

  // When an application indicates non-zero values here, it is taken as an
  // indication that all future frames will be constrained with those limits
  // until the application indicates a change again.
  PlayoutDelay playout_delay_ = {-1, -1};
};

}  // namespace webrtc
#endif  // WEBRTC_VIDEO_FRAME_H_
