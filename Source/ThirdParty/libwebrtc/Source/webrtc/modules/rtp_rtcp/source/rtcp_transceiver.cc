/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_transceiver.h"

#include <utility>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/timeutils.h"

namespace webrtc {
namespace {
struct Destructor {
  void operator()() { rtcp_transceiver = nullptr; }
  std::unique_ptr<RtcpTransceiverImpl> rtcp_transceiver;
};
}  // namespace

RtcpTransceiver::RtcpTransceiver(const RtcpTransceiverConfig& config)
    : task_queue_(config.task_queue),
      rtcp_transceiver_(absl::make_unique<RtcpTransceiverImpl>(config)) {
  RTC_DCHECK(task_queue_);
}

RtcpTransceiver::~RtcpTransceiver() {
  if (!rtcp_transceiver_)
    return;
  task_queue_->PostTask(Destructor{std::move(rtcp_transceiver_)});
  RTC_DCHECK(!rtcp_transceiver_);
}

void RtcpTransceiver::Stop(std::function<void()> on_destroyed) {
  RTC_DCHECK(rtcp_transceiver_);
  task_queue_->PostTask(rtc::NewClosure(
      Destructor{std::move(rtcp_transceiver_)}, std::move(on_destroyed)));
  RTC_DCHECK(!rtcp_transceiver_);
}

void RtcpTransceiver::AddMediaReceiverRtcpObserver(
    uint32_t remote_ssrc,
    MediaReceiverRtcpObserver* observer) {
  RTC_CHECK(rtcp_transceiver_);
  RtcpTransceiverImpl* ptr = rtcp_transceiver_.get();
  task_queue_->PostTask([ptr, remote_ssrc, observer] {
    ptr->AddMediaReceiverRtcpObserver(remote_ssrc, observer);
  });
}

void RtcpTransceiver::RemoveMediaReceiverRtcpObserver(
    uint32_t remote_ssrc,
    MediaReceiverRtcpObserver* observer,
    std::function<void()> on_removed) {
  RTC_CHECK(rtcp_transceiver_);
  RtcpTransceiverImpl* ptr = rtcp_transceiver_.get();
  auto remove = [ptr, remote_ssrc, observer] {
    ptr->RemoveMediaReceiverRtcpObserver(remote_ssrc, observer);
  };
  task_queue_->PostTask(
      rtc::NewClosure(std::move(remove), std::move(on_removed)));
}

void RtcpTransceiver::SetReadyToSend(bool ready) {
  RTC_CHECK(rtcp_transceiver_);
  RtcpTransceiverImpl* ptr = rtcp_transceiver_.get();
  task_queue_->PostTask([ptr, ready] {
      ptr->SetReadyToSend(ready);
  });
}

void RtcpTransceiver::ReceivePacket(rtc::CopyOnWriteBuffer packet) {
  RTC_CHECK(rtcp_transceiver_);
  RtcpTransceiverImpl* ptr = rtcp_transceiver_.get();
  int64_t now_us = rtc::TimeMicros();
  task_queue_->PostTask([ptr, packet, now_us] {
      ptr->ReceivePacket(packet, now_us);
  });
}

void RtcpTransceiver::SendCompoundPacket() {
  RTC_CHECK(rtcp_transceiver_);
  RtcpTransceiverImpl* ptr = rtcp_transceiver_.get();
  task_queue_->PostTask([ptr] {
      ptr->SendCompoundPacket();
  });
}

void RtcpTransceiver::SetRemb(int64_t bitrate_bps,
                              std::vector<uint32_t> ssrcs) {
  RTC_CHECK(rtcp_transceiver_);
  // TODO(danilchap): Replace with lambda with move capture when available.
  struct SetRembClosure {
    void operator()() {
        ptr->SetRemb(bitrate_bps, std::move(ssrcs));
    }

    RtcpTransceiverImpl* ptr;
    int64_t bitrate_bps;
    std::vector<uint32_t> ssrcs;
  };
  task_queue_->PostTask(
      SetRembClosure{rtcp_transceiver_.get(), bitrate_bps, std::move(ssrcs)});
}

void RtcpTransceiver::UnsetRemb() {
  RTC_CHECK(rtcp_transceiver_);
  RtcpTransceiverImpl* ptr = rtcp_transceiver_.get();
  task_queue_->PostTask([ptr] {
      ptr->UnsetRemb();
  });
}

uint32_t RtcpTransceiver::SSRC() const {
  return rtcp_transceiver_->sender_ssrc();
}

bool RtcpTransceiver::SendFeedbackPacket(
    const rtcp::TransportFeedback& packet) {
  RTC_CHECK(rtcp_transceiver_);
  struct Closure {
    void operator()() {
        ptr->SendRawPacket(raw_packet);
    }
    RtcpTransceiverImpl* ptr;
    rtc::Buffer raw_packet;
  };
  task_queue_->PostTask(Closure{rtcp_transceiver_.get(), packet.Build()});
  return true;
}

void RtcpTransceiver::SendNack(uint32_t ssrc,
                               std::vector<uint16_t> sequence_numbers) {
  RTC_CHECK(rtcp_transceiver_);
  // TODO(danilchap): Replace with lambda with move capture when available.
  struct Closure {
    void operator()() {
        ptr->SendNack(ssrc, std::move(sequence_numbers));
    }

    RtcpTransceiverImpl* ptr;
    uint32_t ssrc;
    std::vector<uint16_t> sequence_numbers;
  };
  task_queue_->PostTask(
      Closure{rtcp_transceiver_.get(), ssrc, std::move(sequence_numbers)});
}

void RtcpTransceiver::SendPictureLossIndication(uint32_t ssrc) {
  RTC_CHECK(rtcp_transceiver_);
  RtcpTransceiverImpl* ptr = rtcp_transceiver_.get();
  task_queue_->PostTask([ptr, ssrc] { ptr->SendPictureLossIndication(ssrc); });
}

void RtcpTransceiver::SendFullIntraRequest(std::vector<uint32_t> ssrcs) {
  RTC_CHECK(rtcp_transceiver_);
  // TODO(danilchap): Replace with lambda with move capture when available.
  struct Closure {
    void operator()() {
        ptr->SendFullIntraRequest(ssrcs);
    }

    RtcpTransceiverImpl* ptr;
    std::vector<uint32_t> ssrcs;
  };
  task_queue_->PostTask(Closure{rtcp_transceiver_.get(), std::move(ssrcs)});
}

}  // namespace webrtc
