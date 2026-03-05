/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/test/parsed_rtc_event_log_builder.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/rtc_event_log/rtc_event.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/rtc_event_log_output.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "test/create_test_environment.h"

namespace webrtc::video_timing_simulator {

// Implementation of `RtcEventLogOutput` that parses the serialized log
// into a `ParsedRtcEventLog` during destruction.
class ParsingRtcEventLogOutput : public RtcEventLogOutput {
 public:
  explicit ParsingRtcEventLogOutput(
      absl::AnyInvocable<void(std::unique_ptr<ParsedRtcEventLog>)> callback)
      : callback_(std::move(callback)) {}

  ~ParsingRtcEventLogOutput() override {
    auto parsed_log = std::make_unique<ParsedRtcEventLog>();
    ParsedRtcEventLog::ParseStatus status =
        parsed_log->ParseString(serialized_log_);
    if (status.ok()) {
      callback_(std::move(parsed_log));
    }
  }

  // Implements `RtcEventLogOutput`.
  bool IsActive() const override { return true; }
  bool Write(absl::string_view output) override {
    serialized_log_.append(output);
    return true;
  }
  void Flush() override {}

 private:
  absl::AnyInvocable<void(std::unique_ptr<ParsedRtcEventLog>)> callback_;
  std::string serialized_log_;
};

ParsedRtcEventLogBuilder::ParsedRtcEventLogBuilder()
    : log_clock_(Timestamp::Seconds(10000)),
      log_env_(CreateTestEnvironment(
          CreateTestEnvironmentOptions{.time = &log_clock_})),
      log_(RtcEventLogFactory().Create(log_env_)),
      parsed_log_(nullptr) {
  log_->StartLogging(std::make_unique<ParsingRtcEventLogOutput>(
                         [this](std::unique_ptr<ParsedRtcEventLog> parsed_log) {
                           parsed_log_ = std::move(parsed_log);
                         }),
                     /*output_period_ms=*/5000);
}

ParsedRtcEventLogBuilder::~ParsedRtcEventLogBuilder() = default;

Timestamp ParsedRtcEventLogBuilder::CurrentTime() {
  return log_clock_.CurrentTime();
}

void ParsedRtcEventLogBuilder::AdvanceTime(TimeDelta duration) {
  log_clock_.AdvanceTime(duration);
}

void ParsedRtcEventLogBuilder::LogVideoRecvConfig(uint32_t ssrc) {
  auto config = std::make_unique<rtclog::StreamConfig>();
  config->remote_ssrc = ssrc;
  Log(std::make_unique<RtcEventVideoReceiveStreamConfig>(std::move(config)));
}

void ParsedRtcEventLogBuilder::LogRtpPacketIncoming(uint32_t ssrc) {
  RtpPacketReceived rtp_packet(/*extensions=*/nullptr);
  rtp_packet.SetSsrc(ssrc);
  Log(std::make_unique<RtcEventRtpPacketIncoming>(rtp_packet));
}

void ParsedRtcEventLogBuilder::Log(std::unique_ptr<RtcEvent> event) {
  RTC_DCHECK(log_);
  log_->Log(std::move(event));
}

std::unique_ptr<ParsedRtcEventLog> ParsedRtcEventLogBuilder::Build() {
  RTC_DCHECK(log_);
  log_->StopLogging();
  log_.reset();  // This will implicitly destruct the output object, which will
                 // trigger the callback to be called.
  RTC_CHECK(parsed_log_);
  return std::move(parsed_log_);
}

}  // namespace webrtc::video_timing_simulator
