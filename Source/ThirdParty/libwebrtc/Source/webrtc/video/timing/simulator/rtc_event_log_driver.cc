/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/rtc_event_log_driver.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc::video_timing_simulator {

RtcEventLogDriver::RtcEventLogDriver(
    const ParsedRtcEventLog* absl_nonnull parsed_log,
    absl::string_view field_trials_string,
    RtcEventLogDriver::StreamInterfaceFactory stream_factory)
    : time_controller_(
          std::make_unique<GlobalSimulatedTimeController>(Timestamp::Zero())),
      env_(CreateEnvironment(
          std::make_unique<webrtc::FieldTrials>(field_trials_string),
          time_controller_->GetClock(),
          time_controller_->GetTaskQueueFactory())),
      parsed_log_(*parsed_log),
      stream_factory_(std::move(stream_factory)),
      prev_log_timestamp_(std::nullopt),
      simulator_queue_(time_controller_->GetTaskQueueFactory()->CreateTaskQueue(
          "simulator_queue",
          TaskQueueFactory::Priority::NORMAL)),
      packet_simulator_(env_) {
  RTC_DCHECK(stream_factory_) << "stream_factory must be provided";

  // Config events.
  processor_.AddEvents(
      parsed_log_.video_recv_configs(),
      [&](const auto& config) { OnLoggedVideoRecvConfig(config); });

  // Video packet events.
  for (const auto& stream : parsed_log_.incoming_rtp_packets_by_ssrc()) {
    bool is_video = parsed_log_.GetMediaType(
                        stream.ssrc, PacketDirection::kIncomingPacket) ==
                    ParsedRtcEventLog::MediaType::VIDEO;
    bool is_rtx = parsed_log_.incoming_rtx_ssrcs().contains(stream.ssrc);
    // TODO: b/423646186 - Handle RTX.
    if (!is_video || is_rtx) {
      continue;
    }
    processor_.AddEvents(stream.incoming_packets, [&](const auto& packet) {
      OnLoggedRtpPacketIncoming(packet);
    });
  }
}

RtcEventLogDriver::~RtcEventLogDriver() = default;

void RtcEventLogDriver::Simulate() {
  // Walk through events in timestamp order and call the registered handlers.
  processor_.ProcessEventsInOrder();

  // Attempt to get straggling frames out by advancing time a little bit after
  // the last logged event.
  time_controller_->AdvanceTime(kShutdownAdvanceTimeSlack);

  // Tear down on the queue.
  bool done = false;
  simulator_queue_->PostTask([this, &done]() {
    RTC_DCHECK_RUN_ON(simulator_queue_.get());
    for (auto& stream : streams_) {
      stream.second->Close();
    }
    streams_.clear();
    done = true;
  });
  time_controller_->Wait([&done]() { return done; });
}

void RtcEventLogDriver::AdvanceTime(Timestamp log_timestamp) {
  if (!prev_log_timestamp_) {
    // For the first event, set the clock in absolute terms.
    prev_log_timestamp_ = log_timestamp;
    time_controller_->AdvanceTime(log_timestamp - env_.clock().CurrentTime());
    RTC_DCHECK_EQ(env_.clock().CurrentTime(), log_timestamp);
    return;
  }
  TimeDelta duration = log_timestamp - *prev_log_timestamp_;
  prev_log_timestamp_ = log_timestamp;
  if (duration < TimeDelta::Zero()) {
    RTC_LOG(LS_WARNING) << "Non-monotonic sequence of timestamps";
    return;
  }
  time_controller_->AdvanceTime(duration);
}

void RtcEventLogDriver::HandleEvent(Timestamp log_timestamp,
                                    absl::AnyInvocable<void() &&> handler) {
  // Execute all tasks scheduled before the new logged event.
  AdvanceTime(log_timestamp);

  bool done = false;
  simulator_queue_->PostTask([handler = std::move(handler), &done]() mutable {
    std::move(handler)();
    done = true;
  });

  // Execute the logged event itself.
  AdvanceTime(log_timestamp);
  RTC_DCHECK(done) << "Handler was not executed";
}

void RtcEventLogDriver::OnLoggedVideoRecvConfig(
    const webrtc::LoggedVideoRecvConfig& config) {
  uint32_t ssrc = config.config.remote_ssrc;
  HandleEvent(config.log_time(), [this, ssrc]() {
    RTC_DCHECK_RUN_ON(simulator_queue_.get());
    RTC_LOG(LS_INFO) << "OnLoggedVideoRecvConfig for ssrc=" << ssrc
                     << "at clock=" << env_.clock().CurrentTime();
    if (auto it = streams_.find(ssrc); it != streams_.end()) {
      RTC_LOG(LS_WARNING) << "SSRC=" << ssrc
                          << "already existed. Overwriting it.";
      it->second->Close();
    }
    std::unique_ptr<StreamInterface> stream = stream_factory_(env_, ssrc);
    RTC_DCHECK(stream);
    streams_[ssrc] = std::move(stream);
  });
}

void RtcEventLogDriver::OnLoggedRtpPacketIncoming(
    const webrtc::LoggedRtpPacketIncoming& packet) {
  HandleEvent(packet.log_time(), [this, packet]() {
    RTC_DCHECK_RUN_ON(simulator_queue_.get());
    if (auto it = streams_.find(packet.rtp.header.ssrc); it != streams_.end()) {
      RtpPacketReceived rtp_packet =
          packet_simulator_.SimulateRtpPacketReceived(packet.rtp);
      RTC_DCHECK_EQ(rtp_packet.arrival_time(), packet.log_time());
      RTC_DCHECK_EQ(env_.clock().CurrentTime(), packet.log_time());
      it->second->InsertPacket(rtp_packet);
    } else {
      RTC_LOG(LS_WARNING) << "Received packet for unknown ssrc="
                          << packet.rtp.header.ssrc;
    }
  });
}

}  // namespace webrtc::video_timing_simulator
