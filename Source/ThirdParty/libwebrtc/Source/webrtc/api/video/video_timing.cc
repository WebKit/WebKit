/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_timing.h"

#include <sstream>

namespace webrtc {

TimingFrameInfo::TimingFrameInfo()
    : rtp_timestamp(0),
      capture_time_ms(-1),
      encode_start_ms(-1),
      encode_finish_ms(-1),
      packetization_finish_ms(-1),
      pacer_exit_ms(-1),
      network_timestamp_ms(-1),
      network2_timestamp_ms(-1),
      receive_start_ms(-1),
      receive_finish_ms(-1),
      decode_start_ms(-1),
      decode_finish_ms(-1),
      render_time_ms(-1),
      flags(TimingFrameFlags::kDefault) {}

int64_t TimingFrameInfo::EndToEndDelay() const {
  return capture_time_ms >= 0 ? decode_finish_ms - capture_time_ms : -1;
}

bool TimingFrameInfo::IsLongerThan(const TimingFrameInfo& other) const {
  int64_t other_delay = other.EndToEndDelay();
  return other_delay == -1 || EndToEndDelay() > other_delay;
}

bool TimingFrameInfo::operator<(const TimingFrameInfo& other) const {
  return other.IsLongerThan(*this);
}

bool TimingFrameInfo::operator<=(const TimingFrameInfo& other) const {
  return !IsLongerThan(other);
}

bool TimingFrameInfo::IsOutlier() const {
  return !IsInvalid() && (flags & TimingFrameFlags::kTriggeredBySize);
}

bool TimingFrameInfo::IsTimerTriggered() const {
  return !IsInvalid() && (flags & TimingFrameFlags::kTriggeredByTimer);
}

bool TimingFrameInfo::IsInvalid() const {
  return flags == TimingFrameFlags::kInvalid;
}

std::string TimingFrameInfo::ToString() const {
  std::stringstream out;
  if (IsInvalid()) {
    out << "";
  } else {
    out << rtp_timestamp << ',' << capture_time_ms << ',' << encode_start_ms
        << ',' << encode_finish_ms << ',' << packetization_finish_ms << ','
        << pacer_exit_ms << ',' << network_timestamp_ms << ','
        << network2_timestamp_ms << ',' << receive_start_ms << ','
        << receive_finish_ms << ',' << decode_start_ms << ','
        << decode_finish_ms << ',' << render_time_ms << ','
        << IsOutlier() << ',' << IsTimerTriggered();
  }
  return out.str();
}

}  // namespace webrtc
