/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_TEST_PARSED_RTC_EVENT_LOG_BUILDER_H_
#define VIDEO_TIMING_SIMULATOR_TEST_PARSED_RTC_EVENT_LOG_BUILDER_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "api/environment/environment.h"
#include "api/rtc_event_log/rtc_event.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "system_wrappers/include/clock.h"

namespace webrtc::video_timing_simulator {

// Helper class for building a `ParsedRtcEventLog` from a sequence of events.
class ParsedRtcEventLogBuilder {
 public:
  ParsedRtcEventLogBuilder();
  ~ParsedRtcEventLogBuilder();

  ParsedRtcEventLogBuilder(const ParsedRtcEventLogBuilder&) = delete;
  ParsedRtcEventLogBuilder& operator=(const ParsedRtcEventLogBuilder&) = delete;

  // Interactions with the `log_clock_`.
  // Note that this clock is different from the simulation clock!
  Timestamp CurrentTime();
  void AdvanceTime(TimeDelta duration);

  // Log specific events to the log.
  // Should not be called after a call to `Build`.
  void LogVideoRecvConfig(uint32_t ssrc, uint32_t rtx_ssrc);
  void LogRtpPacketIncoming(
      uint32_t ssrc,
      std::optional<uint16_t> rtx_original_sequence_number = std::nullopt);

  // Returns the parsed log. Should only be called once.
  std::unique_ptr<ParsedRtcEventLog> Build();

 private:
  // Logs a generic event to the log.
  void Log(std::unique_ptr<RtcEvent> event);

  // The `log_clock_` and `log_env_` are different from the _simulation_
  // clock and environment! This is because the `ParsedRtcEventLogBuilder` acts
  // as the logger, which in production would happen in a different context than
  // the simulation.
  SimulatedClock log_clock_;
  const Environment log_env_;
  std::unique_ptr<RtcEventLog> log_;
  std::unique_ptr<ParsedRtcEventLog> parsed_log_;
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_TEST_PARSED_RTC_EVENT_LOG_BUILDER_H_
