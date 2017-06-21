/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MIN_RTT_FILTER_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MIN_RTT_FILTER_H_

#include <climits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "webrtc/logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "webrtc/modules/remote_bitrate_estimator/include/send_time_history.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"

namespace webrtc {
namespace testing {
namespace bwe {
class MinRttFilter {
 public:
  MinRttFilter();
  ~MinRttFilter();
  int64_t min_rtt();
  void UpdateMinRtt(int64_t min_rtt);

  // Checks whether or last discovered min_rtt value
  // is older than x seconds.
  bool MinRttExpired(int64_t now);
  void set_min_rtt_discovery_time(int64_t discovery_time);
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MIN_RTT_FILTER_H_
