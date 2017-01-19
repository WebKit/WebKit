/*
 *  Copyright (c) 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_TIMESTAMPALIGNER_H_
#define WEBRTC_BASE_TIMESTAMPALIGNER_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/base/constructormagic.h"

namespace rtc {

// The TimestampAligner class helps translating camera timestamps into
// the same timescale as is used by rtc::TimeMicros(). Some cameras
// have built in timestamping which is more accurate than reading the
// system clock, but using a different epoch and unknown clock drift.
// Frame timestamps in webrtc should use rtc::TimeMicros (system monotonic
// time), and this class provides a filter which lets us use the
// rtc::TimeMicros timescale, and at the same time take advantage of
// higher accuracy of the camera clock.

// This class is not thread safe, so all calls to it must be synchronized
// externally.
class TimestampAligner {
 public:
  TimestampAligner();
  ~TimestampAligner();

 public:
  // Translates camera timestamps to the same timescale as is used by
  // rtc::TimeMicros(). |camera_time_us| is assumed to be accurate, but
  // with an unknown epoch and clock drift. |system_time_us| is
  // time according to rtc::TimeMicros(), preferably read as soon as
  // possible when the frame is captured. It may have poor accuracy
  // due to poor resolution or scheduling delays. Returns the
  // translated timestamp.
  int64_t TranslateTimestamp(int64_t camera_time_us, int64_t system_time_us);

 protected:
  // Update the estimated offset between camera time and system monotonic time.
  int64_t UpdateOffset(int64_t camera_time_us, int64_t system_time_us);

  // Clip timestamp, return value is always
  //    <= |system_time_us|, and
  //    >= min(|prev_translated_time_us_| + |kMinFrameIntervalUs|,
  //           |system_time_us|).
  int64_t ClipTimestamp(int64_t filtered_time_us, int64_t system_time_us);

 private:
  // State for the timestamp translation.
  int frames_seen_;
  // Estimated offset between camera time and system monotonic time.
  int64_t offset_us_;

  // State for the ClipTimestamp method, applied after the filter.
  // A large negative camera clock drift tends to push translated
  // timestamps into the future. |clip_bias_us_| is subtracted from the
  // translated timestamps, to get them back from the future.
  int64_t clip_bias_us_;
  // Used to ensure that translated timestamps are monotonous.
  int64_t prev_translated_time_us_;
  RTC_DISALLOW_COPY_AND_ASSIGN(TimestampAligner);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_TIMESTAMPALIGNER_H_
