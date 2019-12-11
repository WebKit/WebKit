/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_HIGH_PASS_FILTER_H_
#define MODULES_AUDIO_PROCESSING_HIGH_PASS_FILTER_H_

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/utility/cascaded_biquad_filter.h"

namespace webrtc {

class AudioBuffer;

// Filters that high
class HighPassFilter {
 public:
  explicit HighPassFilter(size_t num_channels);
  ~HighPassFilter();
  HighPassFilter(const HighPassFilter&) = delete;
  HighPassFilter& operator=(const HighPassFilter&) = delete;

  void Process(AudioBuffer* audio);
  // Only to be used when the number of channels are 1.
  // TODO(peah): Add support for more channels.
  void Process(rtc::ArrayView<float> audio);
  void Reset();
  void Reset(size_t num_channels);

 private:
  std::vector<std::unique_ptr<CascadedBiQuadFilter>> filters_;
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_HIGH_PASS_FILTER_H_
