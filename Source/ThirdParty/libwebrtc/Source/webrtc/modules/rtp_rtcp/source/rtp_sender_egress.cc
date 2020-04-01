/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_egress.h"

#include <limits>
#include <memory>
#include <utility>

#include "absl/strings/match.h"
#include "api/transport/field_trial_based_config.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
constexpr uint32_t kTimestampTicksPerMs = 90;
constexpr int kSendSideDelayWindowMs = 1000;
constexpr int kBitrateStatisticsWindowMs = 1000;
constexpr size_t kRtpSequenceNumberMapMaxEntries = 1 << 13;

bool IsEnabled(absl::string_view name,
               const WebRtcKeyValueConfig* field_trials) {
  FieldTrialBasedConfig default_trials;
  auto& trials = field_trials ? *field_trials : default_trials;
  return absl::StartsWith(trials.Lookup(name), "Enabled");
}
}  // namespace

RtpSenderEgress::NonPacedPacketSender::NonPacedPacketSender(
    RtpSenderEgress* sender)
    : transport_sequence_number_(0), sender_(sender) {}
RtpSenderEgress::NonPacedPacketSender::~NonPacedPacketSender() = default;

void RtpSenderEgress::NonPacedPacketSender::EnqueuePackets(
    std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
  for (auto& packet : packets) {
    if (!packet->SetExtension<TransportSequenceNumber>(
            ++transport_sequence_number_)) {
      --transport_sequence_number_;
    }
    packet->ReserveExtension<TransmissionOffset>();
    packet->ReserveExtension<AbsoluteSendTime>();
    sender_->SendPacket(packet.get(), PacedPacketInfo());
  }
}

RtpSenderEgress::RtpSenderEgress(const RtpRtcp::Configuration& config,
                                 RtpPacketHistory* packet_history)
    : ssrc_(config.local_media_ssrc),
      rtx_ssrc_(config.rtx_send_ssrc),
      flexfec_ssrc_(config.fec_generator ? config.fec_generator->FecSsrc()
                                         : absl::nullopt),
      populate_network2_timestamp_(config.populate_network2_timestamp),
      send_side_bwe_with_overhead_(
          IsEnabled("WebRTC-SendSideBwe-WithOverhead", config.field_trials)),
      clock_(config.clock),
      packet_history_(packet_history),
      transport_(config.outgoing_transport),
      event_log_(config.event_log),
      is_audio_(config.audio),
      need_rtp_packet_infos_(config.need_rtp_packet_infos),
      transport_feedback_observer_(config.transport_feedback_callback),
      send_side_delay_observer_(config.send_side_delay_observer),
      send_packet_observer_(config.send_packet_observer),
      overhead_observer_(config.overhead_observer),
      rtp_stats_callback_(config.rtp_stats_callback),
      bitrate_callback_(config.send_bitrate_observer),
      media_has_been_sent_(false),
      force_part_of_allocation_(false),
      timestamp_offset_(0),
      max_delay_it_(send_delays_.end()),
      sum_delays_ms_(0),
      total_packet_send_delay_ms_(0),
      rtp_overhead_bytes_per_packet_(0),
      total_bitrate_sent_(kBitrateStatisticsWindowMs,
                          RateStatistics::kBpsScale),
      nack_bitrate_sent_(kBitrateStatisticsWindowMs, RateStatistics::kBpsScale),
      rtp_sequence_number_map_(need_rtp_packet_infos_
                                   ? std::make_unique<RtpSequenceNumberMap>(
                                         kRtpSequenceNumberMapMaxEntries)
                                   : nullptr) {}

void RtpSenderEgress::SendPacket(RtpPacketToSend* packet,
                                 const PacedPacketInfo& pacing_info) {
  RTC_DCHECK(packet);

  const uint32_t packet_ssrc = packet->Ssrc();
  RTC_DCHECK(packet->packet_type().has_value());
  RTC_DCHECK(HasCorrectSsrc(*packet));
  int64_t now_ms = clock_->TimeInMilliseconds();

  if (is_audio_) {
#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
    BWE_TEST_LOGGING_PLOT_WITH_SSRC(1, "AudioTotBitrate_kbps", now_ms,
                                    SendBitrate().kbps(), packet_ssrc);
    BWE_TEST_LOGGING_PLOT_WITH_SSRC(1, "AudioNackBitrate_kbps", now_ms,
                                    NackOverheadRate().kbps(), packet_ssrc);
#endif
  } else {
#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
    BWE_TEST_LOGGING_PLOT_WITH_SSRC(1, "VideoTotBitrate_kbps", now_ms,
                                    SendBitrate().kbps(), packet_ssrc);
    BWE_TEST_LOGGING_PLOT_WITH_SSRC(1, "VideoNackBitrate_kbps", now_ms,
                                    NackOverheadRate().kbps(), packet_ssrc);
#endif
  }

  PacketOptions options;
  {
    rtc::CritScope lock(&lock_);
    options.included_in_allocation = force_part_of_allocation_;

    if (need_rtp_packet_infos_ &&
        packet->packet_type() == RtpPacketToSend::Type::kVideo) {
      RTC_DCHECK(rtp_sequence_number_map_);
      // Last packet of a frame, add it to sequence number info map.
      const uint32_t timestamp = packet->Timestamp() - timestamp_offset_;
      bool is_first_packet_of_frame = packet->is_first_packet_of_frame();
      bool is_last_packet_of_frame = packet->Marker();

      rtp_sequence_number_map_->InsertPacket(
          packet->SequenceNumber(),
          RtpSequenceNumberMap::Info(timestamp, is_first_packet_of_frame,
                                     is_last_packet_of_frame));
    }
  }

  // Bug webrtc:7859. While FEC is invoked from rtp_sender_video, and not after
  // the pacer, these modifications of the header below are happening after the
  // FEC protection packets are calculated. This will corrupt recovered packets
  // at the same place. It's not an issue for extensions, which are present in
  // all the packets (their content just may be incorrect on recovered packets).
  // In case of VideoTimingExtension, since it's present not in every packet,
  // data after rtp header may be corrupted if these packets are protected by
  // the FEC.
  int64_t diff_ms = now_ms - packet->capture_time_ms();
  if (packet->HasExtension<TransmissionOffset>()) {
    packet->SetExtension<TransmissionOffset>(kTimestampTicksPerMs * diff_ms);
  }
  if (packet->HasExtension<AbsoluteSendTime>()) {
    packet->SetExtension<AbsoluteSendTime>(
        AbsoluteSendTime::MsTo24Bits(now_ms));
  }

  if (packet->HasExtension<VideoTimingExtension>()) {
    if (populate_network2_timestamp_) {
      packet->set_network2_time_ms(now_ms);
    } else {
      packet->set_pacer_exit_time_ms(now_ms);
    }
  }

  const bool is_media = packet->packet_type() == RtpPacketMediaType::kAudio ||
                        packet->packet_type() == RtpPacketMediaType::kVideo;

  // Downstream code actually uses this flag to distinguish between media and
  // everything else.
  options.is_retransmit = !is_media;
  if (auto packet_id = packet->GetExtension<TransportSequenceNumber>()) {
    options.packet_id = *packet_id;
    options.included_in_feedback = true;
    options.included_in_allocation = true;
    AddPacketToTransportFeedback(*packet_id, *packet, pacing_info);
  }

  options.application_data.assign(packet->application_data().begin(),
                                  packet->application_data().end());

  if (packet->packet_type() != RtpPacketMediaType::kPadding &&
      packet->packet_type() != RtpPacketMediaType::kRetransmission) {
    UpdateDelayStatistics(packet->capture_time_ms(), now_ms, packet_ssrc);
    UpdateOnSendPacket(options.packet_id, packet->capture_time_ms(),
                       packet_ssrc);
  }

  const bool send_success = SendPacketToNetwork(*packet, options, pacing_info);

  // Put packet in retransmission history or update pending status even if
  // actual sending fails.
  if (is_media && packet->allow_retransmission()) {
    packet_history_->PutRtpPacket(std::make_unique<RtpPacketToSend>(*packet),
                                  now_ms);
  } else if (packet->retransmitted_sequence_number()) {
    packet_history_->MarkPacketAsSent(*packet->retransmitted_sequence_number());
  }

  if (send_success) {
    rtc::CritScope lock(&lock_);
    UpdateRtpStats(*packet);
    media_has_been_sent_ = true;
  }
}

void RtpSenderEgress::ProcessBitrateAndNotifyObservers() {
  if (!bitrate_callback_)
    return;

  rtc::CritScope lock(&lock_);
  int64_t now_ms = clock_->TimeInMilliseconds();
  bitrate_callback_->Notify(total_bitrate_sent_.Rate(now_ms).value_or(0),
                            nack_bitrate_sent_.Rate(now_ms).value_or(0), ssrc_);
}

DataRate RtpSenderEgress::SendBitrate() const {
  rtc::CritScope cs(&lock_);
  return DataRate::BitsPerSec(
      total_bitrate_sent_.Rate(clock_->TimeInMilliseconds()).value_or(0));
}

DataRate RtpSenderEgress::NackOverheadRate() const {
  rtc::CritScope cs(&lock_);
  return DataRate::BitsPerSec(
      nack_bitrate_sent_.Rate(clock_->TimeInMilliseconds()).value_or(0));
}

void RtpSenderEgress::GetDataCounters(StreamDataCounters* rtp_stats,
                                      StreamDataCounters* rtx_stats) const {
  rtc::CritScope lock(&lock_);
  *rtp_stats = rtp_stats_;
  *rtx_stats = rtx_rtp_stats_;
}

void RtpSenderEgress::ForceIncludeSendPacketsInAllocation(
    bool part_of_allocation) {
  rtc::CritScope lock(&lock_);
  force_part_of_allocation_ = part_of_allocation;
}

bool RtpSenderEgress::MediaHasBeenSent() const {
  rtc::CritScope lock(&lock_);
  return media_has_been_sent_;
}

void RtpSenderEgress::SetMediaHasBeenSent(bool media_sent) {
  rtc::CritScope lock(&lock_);
  media_has_been_sent_ = media_sent;
}

void RtpSenderEgress::SetTimestampOffset(uint32_t timestamp) {
  rtc::CritScope lock(&lock_);
  timestamp_offset_ = timestamp;
}

std::vector<RtpSequenceNumberMap::Info> RtpSenderEgress::GetSentRtpPacketInfos(
    rtc::ArrayView<const uint16_t> sequence_numbers) const {
  RTC_DCHECK(!sequence_numbers.empty());
  if (!need_rtp_packet_infos_) {
    return std::vector<RtpSequenceNumberMap::Info>();
  }

  std::vector<RtpSequenceNumberMap::Info> results;
  results.reserve(sequence_numbers.size());

  rtc::CritScope cs(&lock_);
  for (uint16_t sequence_number : sequence_numbers) {
    const auto& info = rtp_sequence_number_map_->Get(sequence_number);
    if (!info) {
      // The empty vector will be returned. We can delay the clearing
      // of the vector until after we exit the critical section.
      return std::vector<RtpSequenceNumberMap::Info>();
    }
    results.push_back(*info);
  }

  return results;
}

bool RtpSenderEgress::HasCorrectSsrc(const RtpPacketToSend& packet) const {
  switch (*packet.packet_type()) {
    case RtpPacketMediaType::kAudio:
    case RtpPacketMediaType::kVideo:
      return packet.Ssrc() == ssrc_;
    case RtpPacketMediaType::kRetransmission:
    case RtpPacketMediaType::kPadding:
      // Both padding and retransmission must be on either the media or the
      // RTX stream.
      return packet.Ssrc() == rtx_ssrc_ || packet.Ssrc() == ssrc_;
    case RtpPacketMediaType::kForwardErrorCorrection:
      // FlexFEC is on separate SSRC, ULPFEC uses media SSRC.
      return packet.Ssrc() == ssrc_ || packet.Ssrc() == flexfec_ssrc_;
  }
  return false;
}

void RtpSenderEgress::AddPacketToTransportFeedback(
    uint16_t packet_id,
    const RtpPacketToSend& packet,
    const PacedPacketInfo& pacing_info) {
  if (transport_feedback_observer_) {
    size_t packet_size = packet.payload_size() + packet.padding_size();
    if (send_side_bwe_with_overhead_) {
      packet_size = packet.size();
    }

    RtpPacketSendInfo packet_info;
    packet_info.ssrc = ssrc_;
    packet_info.transport_sequence_number = packet_id;
    packet_info.has_rtp_sequence_number = true;
    packet_info.rtp_sequence_number = packet.SequenceNumber();
    packet_info.length = packet_size;
    packet_info.pacing_info = pacing_info;
    packet_info.packet_type = packet.packet_type();
    transport_feedback_observer_->OnAddPacket(packet_info);
  }
}

void RtpSenderEgress::UpdateDelayStatistics(int64_t capture_time_ms,
                                            int64_t now_ms,
                                            uint32_t ssrc) {
  if (!send_side_delay_observer_ || capture_time_ms <= 0)
    return;

  int avg_delay_ms = 0;
  int max_delay_ms = 0;
  uint64_t total_packet_send_delay_ms = 0;
  {
    rtc::CritScope cs(&lock_);
    // Compute the max and average of the recent capture-to-send delays.
    // The time complexity of the current approach depends on the distribution
    // of the delay values. This could be done more efficiently.

    // Remove elements older than kSendSideDelayWindowMs.
    auto lower_bound =
        send_delays_.lower_bound(now_ms - kSendSideDelayWindowMs);
    for (auto it = send_delays_.begin(); it != lower_bound; ++it) {
      if (max_delay_it_ == it) {
        max_delay_it_ = send_delays_.end();
      }
      sum_delays_ms_ -= it->second;
    }
    send_delays_.erase(send_delays_.begin(), lower_bound);
    if (max_delay_it_ == send_delays_.end()) {
      // Removed the previous max. Need to recompute.
      RecomputeMaxSendDelay();
    }

    // Add the new element.
    RTC_DCHECK_GE(now_ms, 0);
    RTC_DCHECK_LE(now_ms, std::numeric_limits<int64_t>::max() / 2);
    RTC_DCHECK_GE(capture_time_ms, 0);
    RTC_DCHECK_LE(capture_time_ms, std::numeric_limits<int64_t>::max() / 2);
    int64_t diff_ms = now_ms - capture_time_ms;
    RTC_DCHECK_GE(diff_ms, static_cast<int64_t>(0));
    RTC_DCHECK_LE(diff_ms, std::numeric_limits<int>::max());
    int new_send_delay = rtc::dchecked_cast<int>(now_ms - capture_time_ms);
    SendDelayMap::iterator it;
    bool inserted;
    std::tie(it, inserted) =
        send_delays_.insert(std::make_pair(now_ms, new_send_delay));
    if (!inserted) {
      // TODO(terelius): If we have multiple delay measurements during the same
      // millisecond then we keep the most recent one. It is not clear that this
      // is the right decision, but it preserves an earlier behavior.
      int previous_send_delay = it->second;
      sum_delays_ms_ -= previous_send_delay;
      it->second = new_send_delay;
      if (max_delay_it_ == it && new_send_delay < previous_send_delay) {
        RecomputeMaxSendDelay();
      }
    }
    if (max_delay_it_ == send_delays_.end() ||
        it->second >= max_delay_it_->second) {
      max_delay_it_ = it;
    }
    sum_delays_ms_ += new_send_delay;
    total_packet_send_delay_ms_ += new_send_delay;
    total_packet_send_delay_ms = total_packet_send_delay_ms_;

    size_t num_delays = send_delays_.size();
    RTC_DCHECK(max_delay_it_ != send_delays_.end());
    max_delay_ms = rtc::dchecked_cast<int>(max_delay_it_->second);
    int64_t avg_ms = (sum_delays_ms_ + num_delays / 2) / num_delays;
    RTC_DCHECK_GE(avg_ms, static_cast<int64_t>(0));
    RTC_DCHECK_LE(avg_ms,
                  static_cast<int64_t>(std::numeric_limits<int>::max()));
    avg_delay_ms =
        rtc::dchecked_cast<int>((sum_delays_ms_ + num_delays / 2) / num_delays);
  }
  send_side_delay_observer_->SendSideDelayUpdated(
      avg_delay_ms, max_delay_ms, total_packet_send_delay_ms, ssrc);
}

void RtpSenderEgress::RecomputeMaxSendDelay() {
  max_delay_it_ = send_delays_.begin();
  for (auto it = send_delays_.begin(); it != send_delays_.end(); ++it) {
    if (it->second >= max_delay_it_->second) {
      max_delay_it_ = it;
    }
  }
}

void RtpSenderEgress::UpdateOnSendPacket(int packet_id,
                                         int64_t capture_time_ms,
                                         uint32_t ssrc) {
  if (!send_packet_observer_ || capture_time_ms <= 0 || packet_id == -1) {
    return;
  }

  send_packet_observer_->OnSendPacket(packet_id, capture_time_ms, ssrc);
}

bool RtpSenderEgress::SendPacketToNetwork(const RtpPacketToSend& packet,
                                          const PacketOptions& options,
                                          const PacedPacketInfo& pacing_info) {
  int bytes_sent = -1;
  if (transport_) {
    UpdateRtpOverhead(packet);
    bytes_sent = transport_->SendRtp(packet.data(), packet.size(), options)
                     ? static_cast<int>(packet.size())
                     : -1;
    if (event_log_ && bytes_sent > 0) {
      event_log_->Log(std::make_unique<RtcEventRtpPacketOutgoing>(
          packet, pacing_info.probe_cluster_id));
    }
  }

  if (bytes_sent <= 0) {
    RTC_LOG(LS_WARNING) << "Transport failed to send packet.";
    return false;
  }
  return true;
}

void RtpSenderEgress::UpdateRtpOverhead(const RtpPacketToSend& packet) {
  if (!overhead_observer_)
    return;
  size_t overhead_bytes_per_packet;
  {
    rtc::CritScope lock(&lock_);
    if (rtp_overhead_bytes_per_packet_ == packet.headers_size()) {
      return;
    }
    rtp_overhead_bytes_per_packet_ = packet.headers_size();
    overhead_bytes_per_packet = rtp_overhead_bytes_per_packet_;
  }
  overhead_observer_->OnOverheadChanged(overhead_bytes_per_packet);
}

void RtpSenderEgress::UpdateRtpStats(const RtpPacketToSend& packet) {
  int64_t now_ms = clock_->TimeInMilliseconds();

  StreamDataCounters* counters =
      packet.Ssrc() == rtx_ssrc_ ? &rtx_rtp_stats_ : &rtp_stats_;

  total_bitrate_sent_.Update(packet.size(), now_ms);

  if (counters->first_packet_time_ms == -1) {
    counters->first_packet_time_ms = now_ms;
  }

  if (packet.packet_type() == RtpPacketMediaType::kForwardErrorCorrection) {
    counters->fec.AddPacket(packet);
  }

  if (packet.packet_type() == RtpPacketMediaType::kRetransmission) {
    counters->retransmitted.AddPacket(packet);
    nack_bitrate_sent_.Update(packet.size(), now_ms);
  }
  counters->transmitted.AddPacket(packet);

  if (rtp_stats_callback_) {
    rtp_stats_callback_->DataCountersUpdated(*counters, packet.Ssrc());
  }
}

}  // namespace webrtc
