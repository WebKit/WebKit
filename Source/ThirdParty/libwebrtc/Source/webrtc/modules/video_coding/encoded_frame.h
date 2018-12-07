/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_ENCODED_FRAME_H_
#define MODULES_VIDEO_CODING_ENCODED_FRAME_H_

#include <vector>

#include "api/video/encoded_image.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_coding_defines.h"

namespace webrtc {

class VCMEncodedFrame : protected EncodedImage {
 public:
  VCMEncodedFrame();
  VCMEncodedFrame(const VCMEncodedFrame&) = delete;

  ~VCMEncodedFrame();
  /**
   *   Delete VideoFrame and resets members to zero
   */
  void Free();
  /**
   *   Set render time in milliseconds
   */
  void SetRenderTime(const int64_t renderTimeMs) {
    _renderTimeMs = renderTimeMs;
  }

  /**
   *   Set the encoded frame size
   */
  void SetEncodedSize(uint32_t width, uint32_t height) {
    _encodedWidth = width;
    _encodedHeight = height;
  }

  void SetPlayoutDelay(PlayoutDelay playout_delay) {
    playout_delay_ = playout_delay;
  }

  /**
   *   Get the encoded image
   */
  const webrtc::EncodedImage& EncodedImage() const {
    return static_cast<const webrtc::EncodedImage&>(*this);
  }
  /**
   *   Get pointer to frame buffer
   */
  const uint8_t* Buffer() const { return _buffer; }
  /**
   *   Get pointer to frame buffer that can be mutated.
   */
  uint8_t* MutableBuffer() { return _buffer; }
  /**
   *   Get frame length
   */
  size_t Length() const { return _length; }
  /**
   *   Set frame length
   */
  void SetLength(size_t length) {
    RTC_DCHECK(length <= _size);
    _length = length;
  }
  /**
   *   Frame RTP timestamp (90kHz)
   */
  using EncodedImage::Timestamp;
  using EncodedImage::SetTimestamp;
  /**
   *   Get render time in milliseconds
   */
  int64_t RenderTimeMs() const { return _renderTimeMs; }
  /**
   *   Get frame type
   */
  webrtc::FrameType FrameType() const { return _frameType; }
  /**
   *   Get frame rotation
   */
  VideoRotation rotation() const { return rotation_; }
  /**
   *  Get video content type
   */
  VideoContentType contentType() const { return content_type_; }
  /**
   * Get video timing
   */
  EncodedImage::Timing video_timing() const { return timing_; }
  /**
   *   True if this frame is complete, false otherwise
   */
  bool Complete() const { return _completeFrame; }
  /**
   *   True if there's a frame missing before this frame
   */
  bool MissingFrame() const { return _missingFrame; }
  /**
   *   Payload type of the encoded payload
   */
  uint8_t PayloadType() const { return _payloadType; }
  /**
   *   Get codec specific info.
   *   The returned pointer is only valid as long as the VCMEncodedFrame
   *   is valid. Also, VCMEncodedFrame owns the pointer and will delete
   *   the object.
   */
  const CodecSpecificInfo* CodecSpecific() const { return &_codecSpecificInfo; }

 protected:
  /**
   * Verifies that current allocated buffer size is larger than or equal to the
   * input size.
   * If the current buffer size is smaller, a new allocation is made and the old
   * buffer data
   * is copied to the new buffer.
   * Buffer size is updated to minimumSize.
   */
  void VerifyAndAllocate(size_t minimumSize);

  void Reset();

  void CopyCodecSpecific(const RTPVideoHeader* header);

  int64_t _renderTimeMs;
  uint8_t _payloadType;
  bool _missingFrame;
  CodecSpecificInfo _codecSpecificInfo;
  webrtc::VideoCodecType _codec;

  // Video rotation is only set along with the last packet for each frame
  // (same as marker bit). This |_rotation_set| is only for debugging purpose
  // to ensure we don't set it twice for a frame.
  bool _rotation_set;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_ENCODED_FRAME_H_
