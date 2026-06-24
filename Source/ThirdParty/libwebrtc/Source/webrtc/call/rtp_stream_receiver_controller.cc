/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_stream_receiver_controller.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "call/rtp_packet_sink_interface.h"
#include "call/rtp_stream_receiver_controller_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

RtpStreamReceiverController::Receiver::Receiver(
    RtpStreamReceiverController* controller,
    uint32_t ssrc,
    RtpPacketSinkInterface* sink)
    : controller_(controller), sink_(sink) {
  controller_->sink_validator_->OnSinkAdded(sink_);
  auto add_sink =
      [controller = controller_, ssrc, sink = sink_] {
        RTC_DCHECK_RUN_ON(controller->network_thread_);
        if (!controller->AddSink(ssrc, sink)) {
          RTC_LOG(LS_ERROR)
              << "RtpStreamReceiverController::Receiver::Receiver: Sink "
                 "could not be added for SSRC="
              << ssrc << ".";
        }
      };
  if (controller_->network_thread_->IsCurrent()) {
    add_sink();
  } else {
    controller_->network_thread_->PostTask(
        SafeTask(controller_->network_safety_, std::move(add_sink)));
  }
}

RtpStreamReceiverController::Receiver::~Receiver() {
  controller_->sink_validator_->OnSinkRemoved(sink_);
  auto remove_sink = [controller = controller_, sink = sink_] {
    RTC_DCHECK_RUN_ON(controller->network_thread_);
    controller->RemoveSink(sink);
  };
  if (controller_->network_thread_->IsCurrent()) {
    remove_sink();
  } else {
    controller_->network_thread_->PostTask(
        SafeTask(controller_->network_safety_, std::move(remove_sink)));
  }
}

RtpStreamReceiverController::RtpStreamReceiverController(
    TaskQueueBase* absl_nonnull network_thread,
    TaskQueueBase* absl_nonnull worker_thread,
    RtpSinkValidator* absl_nonnull sink_validator)
    : network_thread_(network_thread),
      worker_thread_(worker_thread),
      sink_validator_(sink_validator) {
  RTC_DCHECK(network_thread_);
  RTC_DCHECK(worker_thread_);
  RTC_DCHECK(sink_validator_);
}

RtpStreamReceiverController::~RtpStreamReceiverController() {
  worker_safety_->SetNotAlive();
}

std::unique_ptr<RtpStreamReceiverInterface>
RtpStreamReceiverController::CreateReceiver(uint32_t ssrc,
                                            RtpPacketSinkInterface* sink) {
  return std::make_unique<Receiver>(this, ssrc, sink);
}

void RtpStreamReceiverController::DisconnectFromNetworkThread() {
  RTC_DCHECK_RUN_ON(network_thread_);
  demuxer_.RemoveAllSinks();
  network_safety_->SetNotAlive();
}

bool RtpStreamReceiverController::IsEmpty() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return demuxer_.IsEmpty();
}

RtpPacketSinkInterface* RtpStreamReceiverController::ResolveSink(
    const RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(network_thread_);
  return demuxer_.ResolveSink(packet);
}

bool RtpStreamReceiverController::OnRtpPacket(const RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(network_thread_);
  return demuxer_.OnRtpPacket(packet);
}

void RtpStreamReceiverController::OnRecoveredPacket(
    const RtpPacketReceived& packet) {
  auto demux_and_post = [this, packet]() mutable {
    RTC_DCHECK_RUN_ON(network_thread_);
    if (RtpPacketSinkInterface* sink = demuxer_.ResolveSink(packet)) {
      auto deliver_task = [sink, validator = sink_validator_,
                           packet = std::move(packet)]() mutable {
        if (validator->IsValidSink(sink)) {
          sink->OnRtpPacket(packet);
        }
      };
      if (worker_thread_->IsCurrent()) {
        deliver_task();
      } else {
        worker_thread_->PostTask(
            SafeTask(worker_safety_, std::move(deliver_task)));
      }
    }
  };

  if (network_thread_->IsCurrent()) {
    demux_and_post();
  } else {
    network_thread_->PostTask(
        SafeTask(network_safety_, std::move(demux_and_post)));
  }
}

bool RtpStreamReceiverController::AddSink(uint32_t ssrc,
                                          RtpPacketSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(network_thread_);
  return demuxer_.AddSink(ssrc, sink);
}

bool RtpStreamReceiverController::RemoveSink(
    const RtpPacketSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(network_thread_);
  return demuxer_.RemoveSink(sink);
}

}  // namespace webrtc
