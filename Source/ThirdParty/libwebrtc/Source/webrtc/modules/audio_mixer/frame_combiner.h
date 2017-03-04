/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_MIXER_FRAME_COMBINER_H_
#define WEBRTC_MODULES_AUDIO_MIXER_FRAME_COMBINER_H_

#include <memory>
#include <vector>

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/include/module_common_types.h"

namespace webrtc {

class FrameCombiner {
 public:
  explicit FrameCombiner(bool use_apm_limiter);
  ~FrameCombiner();

  // Combine several frames into one. Assumes sample_rate,
  // samples_per_channel of the input frames match the parameters. The
  // extra parameters are needed because 'mix_list' can be empty.
  void Combine(const std::vector<AudioFrame*>& mix_list,
               size_t number_of_channels,
               int sample_rate,
               AudioFrame* audio_frame_for_mixing) const;

 private:
  // Lower-level helper function called from Combine(...) when there
  // are several input frames.
  //
  // TODO(aleloi): change interface to ArrayView<int16_t> output_frame
  // once we have gotten rid of the APM limiter.
  //
  // Only the 'data' field of output_frame should be modified. The
  // rest are used for potentially sending the output to the APM
  // limiter.
  void CombineMultipleFrames(
      const std::vector<rtc::ArrayView<const int16_t>>& input_frames,
      AudioFrame* audio_frame_for_mixing) const;

  const bool use_apm_limiter_;
  std::unique_ptr<AudioProcessing> limiter_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_MIXER_FRAME_COMBINER_H_
