/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <string.h>

#include <algorithm>
#include <bitset>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/residual_echo_detector.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  // Number of times to update the echo detector.
  constexpr size_t kNrOfUpdates = 7;
  // Each round of updates requires a call to both AnalyzeRender and
  // AnalyzeCapture, so the amount of needed input bytes doubles. Also, two
  // bytes are used to set the call order.
  constexpr size_t kNrOfNeededInputBytes = 2 * kNrOfUpdates * sizeof(float) + 2;
  // The maximum audio energy that an audio frame can have is equal to the
  // number of samples in the frame multiplied by 2^30. We use a single sample
  // to represent an audio frame in this test, so it should have a maximum value
  // equal to the square root of that value.
  const float maxFuzzedValue = sqrtf(20 * 48) * 32768;
  if (size < kNrOfNeededInputBytes) {
    return;
  }
  size_t read_idx = 0;
  // Use the first two bytes to choose the call order.
  uint16_t call_order_int;
  memcpy(&call_order_int, &data[read_idx], 2);
  read_idx += 2;
  std::bitset<16> call_order(call_order_int);

  ResidualEchoDetector echo_detector;
  std::vector<float> input(1);
  // Call AnalyzeCaptureAudio once to prevent the flushing of the buffer.
  echo_detector.AnalyzeCaptureAudio(input);
  for (size_t i = 0; i < 2 * kNrOfUpdates; ++i) {
    // Convert 4 input bytes to a float.
    RTC_DCHECK_LE(read_idx + sizeof(float), size);
    memcpy(input.data(), &data[read_idx], sizeof(float));
    read_idx += sizeof(float);
    if (!isfinite(input[0]) || fabs(input[0]) > maxFuzzedValue) {
      // Ignore infinity, nan values and values that are unrealistically large.
      continue;
    }
    if (call_order[i]) {
      echo_detector.AnalyzeRenderAudio(input);
    } else {
      echo_detector.AnalyzeCaptureAudio(input);
    }
  }
}

}  // namespace webrtc
