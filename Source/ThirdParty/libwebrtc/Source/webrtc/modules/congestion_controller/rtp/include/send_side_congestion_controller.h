/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_RTP_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_RTP_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_H_

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/congestion_controller/include/send_side_congestion_controller_interface.h"
#include "modules/congestion_controller/rtp/pacer_controller.h"
#include "modules/congestion_controller/rtp/transport_feedback_adapter.h"
#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "modules/pacing/paced_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/task_queue.h"

namespace rtc {
struct SentPacket;
}

namespace webrtc {

class Clock;
class RateLimiter;
class RtcEventLog;

namespace webrtc_cc {

namespace send_side_cc_internal {
// This is used to observe the network controller state and route calls to
// the proper handler. It also keeps cached values for safe asynchronous use.
// This makes sure that things running on the worker queue can't access state
// in SendSideCongestionController, which would risk causing data race on
// destruction unless members are properly ordered.
class ControlHandler;

// TODO(srte): Make sure the PeriodicTask implementation is reusable and move it
// to task_queue.h.
class PeriodicTask : public rtc::QueuedTask {
 public:
  virtual void Stop() = 0;
};
}  // namespace send_side_cc_internal

class SendSideCongestionController
    : public SendSideCongestionControllerInterface,
      public RtcpBandwidthObserver {
 public:
  SendSideCongestionController(
      const Clock* clock,
      rtc::TaskQueue* task_queue,
      RtcEventLog* event_log,
      PacedSender* pacer,
      int start_bitrate_bps,
      int min_bitrate_bps,
      int max_bitrate_bps,
      NetworkControllerFactoryInterface* controller_factory);

  ~SendSideCongestionController() override;

  void RegisterPacketFeedbackObserver(
      PacketFeedbackObserver* observer) override;
  void DeRegisterPacketFeedbackObserver(
      PacketFeedbackObserver* observer) override;

  // Currently, there can be at most one observer.
  // TODO(nisse): The RegisterNetworkObserver method is needed because we first
  // construct this object (as part of RtpTransportControllerSend), then pass a
  // reference to Call, which then registers itself as the observer. We should
  // try to break this circular chain of references, and make the observer a
  // construction time constant.
  void RegisterNetworkObserver(NetworkChangedObserver* observer) override;

  void SetBweBitrates(int min_bitrate_bps,
                      int start_bitrate_bps,
                      int max_bitrate_bps) override;

  void SetAllocatedSendBitrateLimits(int64_t min_send_bitrate_bps,
                                     int64_t max_padding_bitrate_bps,
                                     int64_t max_total_bitrate_bps) override;

  // Resets the BWE state. Note the first argument is the bitrate_bps.
  void OnNetworkRouteChanged(const rtc::NetworkRoute& network_route,
                             int bitrate_bps,
                             int min_bitrate_bps,
                             int max_bitrate_bps) override;
  void SignalNetworkState(NetworkState state) override;

  RtcpBandwidthObserver* GetBandwidthObserver() override;

  bool AvailableBandwidth(uint32_t* bandwidth) const override;

  TransportFeedbackObserver* GetTransportFeedbackObserver() override;

  void SetPerPacketFeedbackAvailable(bool available) override;
  void EnablePeriodicAlrProbing(bool enable) override;

  void OnSentPacket(const rtc::SentPacket& sent_packet) override;

  // Implements RtcpBandwidthObserver
  void OnReceivedEstimatedBitrate(uint32_t bitrate) override;
  void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                    int64_t rtt,
                                    int64_t now_ms) override;
  // Implements CallStatsObserver.
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

  // Implements Module.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  // Implements TransportFeedbackObserver.
  void AddPacket(uint32_t ssrc,
                 uint16_t sequence_number,
                 size_t length,
                 const PacedPacketInfo& pacing_info) override;
  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override;

  std::vector<PacketFeedback> GetTransportFeedbackVector() const;

  void SetPacingFactor(float pacing_factor) override;

  void SetAllocatedBitrateWithoutFeedback(uint32_t bitrate_bps) override;

 protected:
  // TODO(srte): The tests should be rewritten to not depend on internals and
  // these functions should be removed.
  // Since we can't control the timing of the internal task queue, this method
  // is used in unit tests to stop the periodic tasks from running unless
  // PostPeriodicTasksForTest is called.
  void DisablePeriodicTasks();
  // Post periodic tasks just once. This allows unit tests to trigger process
  // updates immediately.
  void PostPeriodicTasksForTest();
  // Waits for outstanding tasks to be finished. This allos unit tests to ensure
  // that expected callbacks has been called.
  void WaitOnTasksForTest();

 private:
  void MaybeCreateControllers() RTC_RUN_ON(task_queue_);
  void MaybeRecreateControllers() RTC_RUN_ON(task_queue_);
  void UpdateInitialConstraints(TargetRateConstraints new_contraints)
      RTC_RUN_ON(task_queue_);

  void StartProcessPeriodicTasks() RTC_RUN_ON(task_queue_);
  void UpdateControllerWithTimeInterval() RTC_RUN_ON(task_queue_);
  void UpdatePacerQueue() RTC_RUN_ON(task_queue_);

  void UpdateStreamsConfig() RTC_RUN_ON(task_queue_);
  void MaybeUpdateOutstandingData();
  void OnReceivedRtcpReceiverReportBlocks(const ReportBlockList& report_blocks,
                                          int64_t now_ms)
      RTC_RUN_ON(task_queue_);

  const Clock* const clock_;
  // PacedSender is thread safe and doesn't need protection here.
  PacedSender* const pacer_;
  // TODO(srte): Move all access to feedback adapter to task queue.
  TransportFeedbackAdapter transport_feedback_adapter_;

  NetworkControllerFactoryInterface* const controller_factory_with_feedback_
      RTC_GUARDED_BY(task_queue_);
  const std::unique_ptr<NetworkControllerFactoryInterface>
      controller_factory_fallback_ RTC_GUARDED_BY(task_queue_);

  const std::unique_ptr<PacerController> pacer_controller_
      RTC_GUARDED_BY(task_queue_);

  std::unique_ptr<send_side_cc_internal::ControlHandler> control_handler_
      RTC_GUARDED_BY(task_queue_);

  std::unique_ptr<NetworkControllerInterface> controller_
      RTC_GUARDED_BY(task_queue_);

  TimeDelta process_interval_ RTC_GUARDED_BY(task_queue_);

  std::map<uint32_t, RTCPReportBlock> last_report_blocks_
      RTC_GUARDED_BY(task_queue_);
  Timestamp last_report_block_time_ RTC_GUARDED_BY(task_queue_);

  NetworkChangedObserver* observer_ RTC_GUARDED_BY(task_queue_);
  NetworkControllerConfig initial_config_ RTC_GUARDED_BY(task_queue_);
  StreamsConfig streams_config_ RTC_GUARDED_BY(task_queue_);

  const bool send_side_bwe_with_overhead_;
  // Transport overhead is written by OnNetworkRouteChanged and read by
  // AddPacket.
  // TODO(srte): Remove atomic when feedback adapter runs on task queue.
  std::atomic<size_t> transport_overhead_bytes_per_packet_;
  bool network_available_ RTC_GUARDED_BY(task_queue_);
  bool periodic_tasks_enabled_ RTC_GUARDED_BY(task_queue_);
  bool packet_feedback_available_ RTC_GUARDED_BY(task_queue_);
  send_side_cc_internal::PeriodicTask* pacer_queue_update_task_
      RTC_GUARDED_BY(task_queue_);
  send_side_cc_internal::PeriodicTask* controller_task_
      RTC_GUARDED_BY(task_queue_);

  // Protects access to last_packet_feedback_vector_ in feedback adapter.
  // TODO(srte): Remove this checker when feedback adapter runs on task queue.
  rtc::RaceChecker worker_race_;

  rtc::TaskQueue* task_queue_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(SendSideCongestionController);
};
}  // namespace webrtc_cc
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_RTP_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_H_
