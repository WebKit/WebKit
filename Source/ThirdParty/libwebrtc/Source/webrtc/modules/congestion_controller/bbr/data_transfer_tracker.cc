/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/bbr/data_transfer_tracker.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace bbr {

DataTransferTracker::DataTransferTracker() {}

DataTransferTracker::~DataTransferTracker() {}

void DataTransferTracker::AddSample(DataSize size_delta,
                                    Timestamp send_time,
                                    Timestamp ack_time) {
  size_sum_ += size_delta;

  RTC_DCHECK(samples_.empty() || ack_time >= samples_.back().ack_time);

  if (!samples_.empty() && ack_time == samples_.back().ack_time) {
    samples_.back().send_time = send_time;
    samples_.back().size_sum = size_sum_;
  } else {
    Sample new_sample;
    new_sample.ack_time = ack_time;
    new_sample.send_time = send_time;
    new_sample.size_delta = size_delta;
    new_sample.size_sum = size_sum_;
    samples_.push_back(new_sample);
  }
}

void DataTransferTracker::ClearOldSamples(Timestamp excluding_end) {
  while (!samples_.empty() && samples_.front().ack_time < excluding_end) {
    samples_.pop_front();
  }
}

DataTransferTracker::Result DataTransferTracker::GetRatesByAckTime(
    Timestamp covered_start,
    Timestamp including_end) {
  Result res;
  // Last sample before covered_start.
  const Sample* window_begin = nullptr;
  // Sample at end time or first sample after end time-
  const Sample* window_end = nullptr;
  // To handle the case when the first sample is after covered_start.
  if (samples_.front().ack_time < including_end)
    window_begin = &samples_.front();
  // To handle the case when the last sample is before including_end.
  if (samples_.back().ack_time > covered_start)
    window_end = &samples_.back();
  for (const auto& sample : samples_) {
    if (sample.ack_time < covered_start) {
      window_begin = &sample;
    } else if (sample.ack_time >= including_end) {
      window_end = &sample;
      break;
    }
  }
  if (window_begin != nullptr && window_end != nullptr) {
    res.acked_data = window_end->size_sum - window_begin->size_sum;
    res.send_timespan = window_end->send_time - window_begin->send_time;
    res.ack_timespan = window_end->ack_time - window_begin->ack_time;
  } else {
    res.acked_data = DataSize::Zero();
    res.ack_timespan = including_end - covered_start;
    res.send_timespan = TimeDelta::Zero();
  }
  return res;
}

}  // namespace bbr
}  // namespace webrtc
