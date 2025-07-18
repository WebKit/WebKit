/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/rtp/transport_feedback_demuxer.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/sequence_checker.h"
#include "api/transport/network_types.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/checks.h"

namespace webrtc {

TransportFeedbackDemuxer::TransportFeedbackDemuxer() {
  // In case the construction thread is different from where the registration
  // and callbacks occur, detach from the construction thread.
  observer_checker_.Detach();
}

void TransportFeedbackDemuxer::RegisterStreamFeedbackObserver(
    std::vector<uint32_t> ssrcs,
    StreamFeedbackObserver* observer) {
  RTC_DCHECK_RUN_ON(&observer_checker_);
  RTC_DCHECK(observer);
  RTC_DCHECK(absl::c_find_if(observers_, [=](const auto& pair) {
               return pair.second == observer;
             }) == observers_.end());
  observers_.push_back({ssrcs, observer});
}

void TransportFeedbackDemuxer::DeRegisterStreamFeedbackObserver(
    StreamFeedbackObserver* observer) {
  RTC_DCHECK_RUN_ON(&observer_checker_);
  RTC_DCHECK(observer);
  const auto it = absl::c_find_if(
      observers_, [=](const auto& pair) { return pair.second == observer; });
  RTC_DCHECK(it != observers_.end());
  observers_.erase(it);
}

void TransportFeedbackDemuxer::OnTransportFeedback(
    const TransportPacketsFeedback& feedback) {
  RTC_DCHECK_RUN_ON(&observer_checker_);

  std::vector<StreamFeedbackObserver::StreamPacketInfo> stream_feedbacks;
  for (const PacketResult& packet : feedback.packet_feedbacks) {
    if (packet.rtp_packet_info.has_value()) {
      stream_feedbacks.push_back(
          {.received = packet.receive_time.IsFinite(),
           .ssrc = packet.rtp_packet_info->ssrc,
           .rtp_sequence_number = packet.rtp_packet_info->rtp_sequence_number,
           .is_retransmission = packet.rtp_packet_info->is_retransmission});
    }
  }

  for (auto& observer : observers_) {
    std::vector<StreamFeedbackObserver::StreamPacketInfo> selected_feedback;
    for (const auto& packet_info : stream_feedbacks) {
      if (absl::c_count(observer.first, packet_info.ssrc) > 0) {
        selected_feedback.push_back(packet_info);
      }
    }
    if (!selected_feedback.empty()) {
      observer.second->OnPacketFeedbackVector(std::move(selected_feedback));
    }
  }
}

}  // namespace webrtc
