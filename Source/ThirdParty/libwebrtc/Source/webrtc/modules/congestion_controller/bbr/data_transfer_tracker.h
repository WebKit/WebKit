/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_BBR_DATA_TRANSFER_TRACKER_H_
#define MODULES_CONGESTION_CONTROLLER_BBR_DATA_TRANSFER_TRACKER_H_

#include <deque>

#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {
namespace bbr {
class DataTransferTracker {
 public:
  struct Result {
    TimeDelta ack_timespan = TimeDelta::Zero();
    TimeDelta send_timespan = TimeDelta::Zero();
    DataSize acked_data = DataSize::Zero();
  };
  DataTransferTracker();
  ~DataTransferTracker();
  void AddSample(DataSize size_delta, Timestamp send_time, Timestamp ack_time);
  void ClearOldSamples(Timestamp excluding_end);

  // Get the average data rate in the window that starts with the last ack which
  // comes before covered_start and ends at the first ack that comes after or at
  // including_end.
  Result GetRatesByAckTime(Timestamp covered_start, Timestamp including_end);

 private:
  struct Sample {
    Timestamp ack_time = Timestamp::PlusInfinity();
    Timestamp send_time = Timestamp::PlusInfinity();
    DataSize size_delta = DataSize::Zero();
    DataSize size_sum = DataSize::Zero();
  };
  std::deque<Sample> samples_;
  DataSize size_sum_ = DataSize::Zero();
};
}  // namespace bbr
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_BBR_DATA_TRANSFER_TRACKER_H_
