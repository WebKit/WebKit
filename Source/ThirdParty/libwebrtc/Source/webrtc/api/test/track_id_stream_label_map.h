/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_TRACK_ID_STREAM_LABEL_MAP_H_
#define API_TEST_TRACK_ID_STREAM_LABEL_MAP_H_

#include <string>

namespace webrtc {
namespace webrtc_pc_e2e {

// Instances of |TrackIdStreamLabelMap| provide bookkeeping capabilities that
// are useful to associate stats reports track_ids to the remote stream_id.
class TrackIdStreamLabelMap {
 public:
  virtual ~TrackIdStreamLabelMap() = default;

  // This method must be called on the same thread where
  // StatsObserverInterface::OnStatsReports is invoked.
  // Returns a reference to a stream label owned by the TrackIdStreamLabelMap.
  // Precondition: |track_id| must be already mapped to a stream_label.
  virtual const std::string& GetStreamLabelFromTrackId(
      const std::string& track_id) const = 0;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // API_TEST_TRACK_ID_STREAM_LABEL_MAP_H_
