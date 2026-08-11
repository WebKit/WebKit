/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_RTP_STREAM_RECEIVER_CONTROLLER_H_
#define CALL_RTP_STREAM_RECEIVER_CONTROLLER_H_

#include <cstdint>
#include <memory>

#include "absl/base/nullability.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "call/rtp_demuxer.h"
#include "call/rtp_stream_receiver_controller_interface.h"
#include "modules/rtp_rtcp/include/recovered_packet_receiver.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RtpPacketReceived;

// Interface used by RtpStreamReceiverController to validate the lifetime of
// registered sinks across thread boundaries. It represents the registration
// state of a sink as considered on the worker thread. Because demuxing occurs
// on the network thread while delivery happens on the worker thread, this
// validation prevents use-after-free bugs by verifying that a resolved sink
// remains valid on the worker thread prior to packet dispatch.
class RtpSinkValidator {
 public:
  virtual ~RtpSinkValidator() = default;
  // Called when a receiver sink is registered with the controller (e.g.,
  // synchronously during RtpStreamReceiverController::CreateReceiver on the
  // worker thread).
  virtual void OnSinkAdded(RtpPacketSinkInterface* sink) = 0;
  // Called when a receiver sink is unregistered from the controller (e.g.,
  // synchronously upon destruction of the returned RtpStreamReceiverInterface
  // on the worker thread).
  virtual void OnSinkRemoved(RtpPacketSinkInterface* sink) = 0;
  // Verifies whether the specified sink remains active and safe for delivery.
  virtual bool IsValidSink(RtpPacketSinkInterface* sink) const = 0;
};

// This class represents the RTP receive parsing and demuxing, for a
// single RTP session.
// TODO(bugs.webrtc.org/7135): Add RTCP processing, we should aim to terminate
// RTCP and not leave any RTCP processing to individual receive streams.
class RtpStreamReceiverController : public RtpStreamReceiverControllerInterface,
                                    public RecoveredPacketReceiver {
 public:
  RtpStreamReceiverController(TaskQueueBase* absl_nonnull network_thread,
                              TaskQueueBase* absl_nonnull worker_thread,
                              RtpSinkValidator* absl_nonnull sink_validator);
  ~RtpStreamReceiverController() override;

  // Implements RtpStreamReceiverControllerInterface.
  std::unique_ptr<RtpStreamReceiverInterface> CreateReceiver(
      uint32_t ssrc,
      RtpPacketSinkInterface* sink) override;

  RtpPacketSinkInterface* ResolveSink(const RtpPacketReceived& packet);

  // TODO(bugs.webrtc.org/7135): Not yet responsible for parsing.
  bool OnRtpPacket(const RtpPacketReceived& packet);

  // Implements RecoveredPacketReceiver.
  // Responsible for demuxing recovered FLEXFEC packets.
  void OnRecoveredPacket(const RtpPacketReceived& packet) override;

  void DisconnectFromNetworkThread();

  bool IsEmpty() const;

 private:
  class Receiver : public RtpStreamReceiverInterface {
   public:
    Receiver(RtpStreamReceiverController* controller,
             uint32_t ssrc,
             RtpPacketSinkInterface* sink);

    ~Receiver() override;

   private:
    RtpStreamReceiverController* const controller_;
    RtpPacketSinkInterface* const sink_;
  };

  bool AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink);
  bool RemoveSink(const RtpPacketSinkInterface* sink);

  TaskQueueBase* const network_thread_;
  TaskQueueBase* const worker_thread_;
  RtpSinkValidator* const sink_validator_;
  scoped_refptr<PendingTaskSafetyFlag> network_safety_ =
      PendingTaskSafetyFlag::CreateAttachedToTaskQueue(true, network_thread_);
  scoped_refptr<PendingTaskSafetyFlag> worker_safety_ =
      PendingTaskSafetyFlag::CreateAttachedToTaskQueue(true, worker_thread_);
  // TODO(bugs.webrtc.org/11993): Demuxing and sink resolution (OnRtpPacket,
  // ResolveSink) now run on the network thread. However, construction and
  // destruction still occur on the worker thread. Once all receive streams and
  // associated state are fully migrated to the network thread, cross-thread
  // posting and dual safety flags can be eliminated.
  RtpDemuxer demuxer_ RTC_GUARDED_BY(network_thread_){false /*use_mid*/};
};

}  // namespace webrtc

#endif  // CALL_RTP_STREAM_RECEIVER_CONTROLLER_H_
