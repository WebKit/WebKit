/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_RTC_EVENT_LOG_DRIVER_H_
#define VIDEO_TIMING_SIMULATOR_RTC_EVENT_LOG_DRIVER_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/task_queue/task_queue_base.h"
#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "logging/rtc_event_log/rtc_event_processor.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/thread_annotations.h"
#include "video/timing/simulator/rtp_packet_simulator.h"

namespace webrtc::video_timing_simulator {

// The `RtcEventLogDriver` is responsible for driving a simulation given an
// RtcEventLog. It walks through the relevant events in the log in
// `log_timestamp` order, and provides the events to an underlying `stream`
// abstraction. This abstraction allows the `RtcEventLogDriver` to be agnostic
// w.r.t. how the simulated packets are used. We will provide abstractions for
// decodability tracking and simulated rendering, but in the future we could
// also wrap all of `VideoReceiveStream2`, to have a "full stack simulation".
//
// The `RtcEventLogDriver` is responsible for the environment and the
// simulated time task queues. All worker objects should be single-threaded,
// running on the provided task queue(s).
//
// TODO: b/423646186 - Improvements:
//  * Handle `LogSegment`s.
//  * Handle stop events.
//  * Parse RTT updates from RTCPs.
//  * Handle RTX.
//  * Split `GlobalSimulatedTimeController` into global and non-global. Use the
//    later for driving the single-threaded time in this class.
class RtcEventLogDriver {
 public:
  // A stream that is driven by simulated RTP packets coming from the log.
  class StreamInterface {
   public:
    virtual ~StreamInterface() = default;
    // Insert `rtp_packet` into the stream.
    virtual void InsertPacket(const RtpPacketReceived& rtp_packet) = 0;
    // Notify the stream that no more packets will be inserted.
    virtual void Close() = 0;
  };

  // Factory that creates a stream given the environment and the stream SSRC.
  using StreamInterfaceFactory =
      absl::AnyInvocable<std::unique_ptr<StreamInterface>(Environment, uint32_t)
                             const>;

  // Slack added after final event, in order to catch any straggling frames.
  static constexpr TimeDelta kShutdownAdvanceTimeSlack = TimeDelta::Millis(100);

  RtcEventLogDriver(const ParsedRtcEventLog* absl_nonnull parsed_log,
                    absl::string_view field_trials_string,
                    StreamInterfaceFactory stream_factory);
  ~RtcEventLogDriver();

  RtcEventLogDriver(const RtcEventLogDriver&) = delete;
  RtcEventLogDriver& operator=(const RtcEventLogDriver&) = delete;

  // Perform the simulation. Should only be called once per instantiation.
  void Simulate();

  Timestamp GetCurrentTimeForTesting() const {
    return time_controller_->GetClock()->CurrentTime();
  }

 private:
  // Simulation.
  // Sets the `time_controller_` simulated time to `log_timestamp`, thus
  // executing all relevant tasks on the `simulator_queue_`.
  void AdvanceTime(Timestamp log_timestamp);

  // Advances time according to `log_timestamp`, and handles the event through
  // the `handler`.
  void HandleEvent(Timestamp log_timestamp,
                   absl::AnyInvocable<void() &&> handler);

  // RtcEventProcessor callbacks (running on main thread).
  void OnLoggedVideoRecvConfig(const LoggedVideoRecvConfig& config);
  void OnLoggedRtpPacketIncoming(const LoggedRtpPacketIncoming& packet);

  // Environment.
  std::unique_ptr<TimeController> absl_nonnull time_controller_;
  const Environment env_;

  // Input.
  const ParsedRtcEventLog& parsed_log_;
  StreamInterfaceFactory stream_factory_;

  // Simulator.
  RtcEventProcessor processor_;
  std::optional<Timestamp> prev_log_timestamp_;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> simulator_queue_;
  RtpPacketSimulator packet_simulator_ RTC_GUARDED_BY(simulator_queue_);
  absl::flat_hash_map<uint32_t, std::unique_ptr<StreamInterface>> streams_
      RTC_GUARDED_BY(simulator_queue_);
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_RTC_EVENT_LOG_DRIVER_H_
