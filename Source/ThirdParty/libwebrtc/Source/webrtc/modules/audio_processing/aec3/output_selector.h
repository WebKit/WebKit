/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_OUTPUT_SELECTOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_OUTPUT_SELECTOR_H_

#include "webrtc/base/array_view.h"
#include "webrtc/base/constructormagic.h"

namespace webrtc {

// Performs the selection between which of the linear aec output and the
// microphone signal should be used as the echo suppressor output.
class OutputSelector {
 public:
  OutputSelector();
  ~OutputSelector();

  // Forms the most appropriate output signal.
  void FormLinearOutput(bool use_subtractor_output,
                        rtc::ArrayView<const float> subtractor_output,
                        rtc::ArrayView<float> capture);

  // Returns true if the linear aec output is the one used.
  bool UseSubtractorOutput() const { return use_subtractor_output_; }

 private:
  bool use_subtractor_output_ = false;
  RTC_DISALLOW_COPY_AND_ASSIGN(OutputSelector);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_OUTPUT_SELECTOR_H_
