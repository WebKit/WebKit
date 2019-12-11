/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_CONTRIBUTING_SOURCES_H_
#define MODULES_RTP_RTCP_SOURCE_CONTRIBUTING_SOURCES_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/rtpreceiverinterface.h"  // For RtpSource
#include "rtc_base/timeutils.h"        // For kNumMillisecsPerSec

namespace webrtc {

class ContributingSources {
 public:
  // Set by the spec, see
  // https://www.w3.org/TR/webrtc/#dom-rtcrtpreceiver-getcontributingsources
  static constexpr int64_t kHistoryMs = 10 * rtc::kNumMillisecsPerSec;

  ContributingSources();
  ~ContributingSources();

  void Update(int64_t now_ms, rtc::ArrayView<const uint32_t> csrcs,
              absl::optional<uint8_t> audio_level);

  // Returns contributing sources seen the last 10 s.
  std::vector<RtpSource> GetSources(int64_t now_ms) const;

 private:
  struct Entry {
    Entry();
    Entry(int64_t timestamp_ms, absl::optional<uint8_t> audio_level);

    int64_t last_seen_ms;
    absl::optional<uint8_t> audio_level;
  };

  void DeleteOldEntries(int64_t now_ms);

  // Indexed by csrc.
  std::map<uint32_t, Entry> active_csrcs_;
  absl::optional<int64_t> next_pruning_ms_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_CONTRIBUTING_SOURCES_H_
