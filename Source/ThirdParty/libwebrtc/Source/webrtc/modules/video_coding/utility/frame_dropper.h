/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_FRAME_DROPPER_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_FRAME_DROPPER_H_

#include <cstddef>

#include "webrtc/base/exp_filter.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// The Frame Dropper implements a variant of the leaky bucket algorithm
// for keeping track of when to drop frames to avoid bit rate
// over use when the encoder can't keep its bit rate.
class FrameDropper {
 public:
  FrameDropper();
  explicit FrameDropper(float max_time_drops);
  virtual ~FrameDropper() {}

  // Resets the FrameDropper to its initial state.
  // This means that the frameRateWeight is set to its
  // default value as well.
  virtual void Reset();

  virtual void Enable(bool enable);
  // Answers the question if it's time to drop a frame
  // if we want to reach a given frame rate. Must be
  // called for every frame.
  //
  // Return value     : True if we should drop the current frame
  virtual bool DropFrame();
  // Updates the FrameDropper with the size of the latest encoded
  // frame. The FrameDropper calculates a new drop ratio (can be
  // seen as the probability to drop a frame) and updates its
  // internal statistics.
  //
  // Input:
  //          - frameSizeBytes    : The size of the latest frame
  //                                returned from the encoder.
  //          - deltaFrame        : True if the encoder returned
  //                                a key frame.
  virtual void Fill(size_t frameSizeBytes, bool deltaFrame);

  virtual void Leak(uint32_t inputFrameRate);

  // Sets the target bit rate and the frame rate produced by
  // the camera.
  //
  // Input:
  //          - bitRate       : The target bit rate
  virtual void SetRates(float bitRate, float incoming_frame_rate);

 private:
  void UpdateRatio();
  void CapAccumulator();

  rtc::ExpFilter key_frame_ratio_;
  rtc::ExpFilter delta_frame_size_avg_kbits_;

  // Key frames and large delta frames are not immediately accumulated in the
  // bucket since they can immediately overflow the bucket leading to large
  // drops on the following packets that may be much smaller. Instead these
  // large frames are accumulated over several frames when the bucket leaks.

  // |large_frame_accumulation_spread_| represents the number of frames over
  // which a large frame is accumulated.
  float large_frame_accumulation_spread_;
  // |large_frame_accumulation_count_| represents the number of frames left
  // to finish accumulating a large frame.
  int large_frame_accumulation_count_;
  // |large_frame_accumulation_chunk_size_| represents the size of a single
  // chunk for large frame accumulation.
  float large_frame_accumulation_chunk_size_;

  float accumulator_;
  float accumulator_max_;
  float target_bitrate_;
  bool drop_next_;
  rtc::ExpFilter drop_ratio_;
  int drop_count_;
  float incoming_frame_rate_;
  bool was_below_max_;
  bool enabled_;
  const float max_drop_duration_secs_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_FRAME_DROPPER_H_
