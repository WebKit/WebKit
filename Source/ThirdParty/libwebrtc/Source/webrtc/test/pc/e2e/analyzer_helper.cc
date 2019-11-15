/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer_helper.h"

#include <utility>

namespace webrtc {
namespace webrtc_pc_e2e {

AnalyzerHelper::AnalyzerHelper() {
  signaling_sequence_checker_.Detach();
}

void AnalyzerHelper::AddTrackToStreamMapping(std::string track_id,
                                             std::string stream_label) {
  RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
  track_to_stream_map_.insert({std::move(track_id), std::move(stream_label)});
}

const std::string& AnalyzerHelper::GetStreamLabelFromTrackId(
    const std::string& track_id) const {
  RTC_DCHECK_RUN_ON(&signaling_sequence_checker_);
  auto track_to_stream_pair = track_to_stream_map_.find(track_id);
  RTC_CHECK(track_to_stream_pair != track_to_stream_map_.end());
  return track_to_stream_pair->second;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
