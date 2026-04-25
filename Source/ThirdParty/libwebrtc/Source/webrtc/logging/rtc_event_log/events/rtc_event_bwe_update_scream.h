/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_BWE_UPDATE_SCREAM_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_BWE_UPDATE_SCREAM_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/rtc_event_log/rtc_event.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_log_parse_status.h"

namespace webrtc {

struct LoggedBweScreamUpdate {
  LoggedBweScreamUpdate() = default;
  LoggedBweScreamUpdate(Timestamp timestamp,
                        uint32_t ref_window_bytes,
                        uint32_t data_in_flight_bytes,
                        uint32_t target_rate_kbps,
                        uint32_t smoothed_rtt_ms,
                        uint32_t avg_queue_delay_ms,
                        uint32_t l4s_marked_permille)
      : timestamp(timestamp),
        ref_window(DataSize::Bytes(ref_window_bytes)),
        data_in_flight(DataSize::Bytes(data_in_flight_bytes)),
        target_rate(DataRate::KilobitsPerSec(target_rate_kbps)),
        smoothed_rtt(TimeDelta::Millis(smoothed_rtt_ms)),
        avg_queue_delay(TimeDelta::Millis(avg_queue_delay_ms)),
        l4s_marked_permille(l4s_marked_permille) {}

  int64_t log_time_us() const { return timestamp.us(); }
  int64_t log_time_ms() const { return timestamp.ms(); }
  Timestamp log_time() const { return timestamp; }

  Timestamp timestamp = Timestamp::MinusInfinity();
  DataSize ref_window;
  DataSize data_in_flight;
  DataRate target_rate;
  TimeDelta smoothed_rtt;
  TimeDelta avg_queue_delay;
  uint32_t l4s_marked_permille;
};

class RtcEventBweUpdateScream final : public RtcEvent {
 public:
  static constexpr Type kType = Type::BweUpdateScream;

  RtcEventBweUpdateScream(DataSize ref_window,
                          DataSize data_in_flight,
                          DataRate target_rate,
                          TimeDelta smoothed_rtt,
                          TimeDelta avg_queue_delay,
                          uint32_t l4s_marked_permille);
  ~RtcEventBweUpdateScream() override;

  Type GetType() const override { return kType; }
  bool IsConfigEvent() const override { return false; }

  std::unique_ptr<RtcEventBweUpdateScream> Copy() const;

  uint32_t ref_window_bytes() const { return ref_window_bytes_; }
  uint32_t data_in_flight_bytes() const { return data_in_flight_bytes_; }
  uint32_t target_rate_kbps() const { return target_rate_kbps_; }
  uint32_t smoothed_rtt_ms() const { return smoothed_rtt_ms_; }
  uint32_t avg_queue_delay_ms() const { return avg_queue_delay_ms_; }
  uint32_t l4s_marked_permille() const { return l4s_marked_permille_; }

  static std::string Encode(ArrayView<const RtcEvent*> batch) {
    // TODO(terelius): Implement
    return "";
  }

  static RtcEventLogParseStatus Parse(
      absl::string_view encoded_bytes,
      bool batched,
      std::vector<LoggedBweScreamUpdate>& output) {
    // TODO(terelius): Implement
    return RtcEventLogParseStatus::Error("Not Implemented", __FILE__, __LINE__);
  }

 private:
  RtcEventBweUpdateScream(const RtcEventBweUpdateScream&) = default;

  const uint32_t ref_window_bytes_;
  const uint32_t data_in_flight_bytes_;
  const uint32_t target_rate_kbps_;
  const uint32_t smoothed_rtt_ms_;
  const uint32_t avg_queue_delay_ms_;
  const uint32_t l4s_marked_permille_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_BWE_UPDATE_SCREAM_H_
