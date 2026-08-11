/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_RESULTS_BASE_H_
#define VIDEO_TIMING_SIMULATOR_RESULTS_BASE_H_

#include "absl/algorithm/container.h"

namespace webrtc::video_timing_simulator {

// CRTP base struct for code reuse.
template <typename ResultsT>
struct ResultsBase {
  // Data members are defined in derived struct.

  // -- CRTP accessors --
  const ResultsT& self() const { return static_cast<const ResultsT&>(*this); }

  // -- Helpers --
  bool IsEmpty() const {
    if (self().streams.empty()) {
      return true;
    }
    return absl::c_all_of(self().streams,
                          [](const auto& stream) { return stream.IsEmpty(); });
  }
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_RESULTS_BASE_H_
