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

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "absl/strings/match.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
constexpr uint32_t kTimestampTicksPerMs = 90;
constexpr TimeDelta kSendSideDelayWindow = TimeDelta::Seconds(1);
constexpr int kBitrateStatisticsWindowMs = 1000;
constexpr size_t kRtpSequenceNumberMapMaxEntries = 1 << 13;
constexpr TimeDelta kUpdateInterval =
    TimeDelta::Millis(kBitrateStatisticsWindowMs);
}  // namespace

RtpSenderEgress::NonPacedPacketSender::NonPacedPacketSender(
    RtpSenderEgress* sender,
    PacketSequencer* sequencer)
    : transport_sequence_number_(0), sender_(sender), sequencer_(sequencer) {
  RTC_DCHECK(sequencer);
}
RtpSenderEgress::NonPacedPacketSender::~NonPacedPacketSender() = default;

void RtpSenderEgress::NonPacedPacketSender::EnqueuePackets(
    std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
  for (auto& packet : packets) {
    PrepareForSend(packet.get());
    sender_->SendPacket(std::move(packet), PacedPacketInfo());
  }
  auto fec_packets = sender_->FetchFecPackets();
  if (!fec_packets.empty()) {
    EnqueuePackets(std::move(fec_packets));
  }
}

void RtpSenderEgress::NonPacedPacketSender::PrepareForSend(
    RtpPacketToSend* packet) {
  // Assign sequence numbers, but not for flexfec which is already running on
  // an internally maintained sequence number series.
  if (packet->Ssrc() != sender_->FlexFecSsrc()) {
    sequencer_->Sequence(*packet);
  }
  if (!packet->SetExtension<TransportSequenceNumber>(
          ++transport_sequence_number_)) {
    --transport_sequence_number_;
  }
  packet->ReserveExtension<TransmissionOffset>();
  packet->ReserveExtension<AbsoluteSendTime>();
}

RtpSenderEgress::RtpSenderEgress(const RtpRtcpInterface::Configuration& config,
                                 RtpPacketHistory* packet_history)
    : enable_send_packet_batching_(config.enable_send_packet_batching),
      worker_queue_(TaskQueueBase::Current()),
      ssrc_(config.local_media_ssrc),
      rtx_ssrc_(config.rtx_send_ssrc),
      flexfec_ssrc_(config.fec_generator ? config.fec_generator->FecSsrc()
                                         : absl::nullopt),
      populate_network2_timestamp_(config.populate_network2_timestamp),
      clock_(config.clock),
      packet_history_(packet_history),
      transport_(config.outgoing_transport),
      event_log_(config.event_log),
      is_audio_(config.audio),
      need_rtp_packet_infos_(config.need_rtp_packet_infos),
      fec_generator_(config.fec_generator),
      transport_feedback_observer_(config.transport_feedback_callback),
      send_side_delay_observer_(config.send_side_delay_observer),
      send_packet_observer_(config.send_packet_observer),
      rtp_stats_callback_(config.rtp_stats_callback),
      bitrate_callback_(config.send_bitrate_observer),
      media_has_been_sent_(false),
      force_part_of_allocation_(false),
      timestamp_offset_(0),
      max_delay_it_(send_delays_.end()),
      sum_delays_(TimeDelta::Zero()),
      send_rates_(kNumMediaTypes,
                  {kBitrateStatisticsWindowMs, RateStatistics::kBpsScale}),
      rtp_sequence_number_map_(need_rtp_packet_infos_
                                   ? std::make_unique<RtpSequenceNumberMap>(
                                         kRtpSequenceNumberMapMaxEntries)
                                   : nullptr) {
  RTC_DCHECK(worker_queue_);
  pacer_checker_.Detach();
  if (bitrate_callback_) {
    update_task_ = RepeatingTaskHandle::DelayedStart(worker_queue_,
                                                     kUpdateInterval, [this]() {
                                                       PeriodicUpdate();
                                                       return kUpdateInterval;
                                                     });
  }
}

RtpSenderEgress::~RtpSenderEgress() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  update_task_.Stop();
}

void RtpSenderEgress::SendPacket(std::unique_ptr<RtpPacketToSend> packet,
                                 const PacedPacketInfo& pacing_info) {
  RTC_DCHECK_RUN_ON(&pacer_checker_);
  RTC_DCHECK(packet);

  if (packet->Ssrc() == ssrc_ &&
      packet->packet_type() != RtpPacketMediaType::kRetransmission) {
    if (last_sent_seq_.has_value()) {
      RTC_DCHECK_EQ(static_cast<uint16_t>(*last_sent_seq_ + 1),
                    packet->SequenceNumber());
    }
    last_sent_seq_ = packet->SequenceNumber();
  } else if (packet->Ssrc() == rtx_ssrc_) {
    if (last_sent_rtx_seq_.has_value()) {
      RTC_DCHECK_EQ(static_cast<uint16_t>(*last_sent_rtx_seq_ + 1),
                    packet->SequenceNumber());
    }
    last_sent_rtx_seq_ = packet->SequenceNumber();
  }

  RTC_DCHECK(packet->packet_type().has_value());
  RTC_DCHECK(HasCorrectSsrc(*packet));
  if (packet->packet_type() == RtpPacketMediaType::kRetransmission) {
    RTC_DCHECK(packet->retransmitted_sequence_number().has_value());
  }

  const Timestamp now = clock_->CurrentTime();

#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
  worker_queue_->PostTask(SafeTask(task_safety_.flag(),
                                   [this, now, packet_ssrc = packet->Ssrc()]() {
                                     BweTestLoggingPlot(now, packet_ssrc);
                                   }));
#endif

  if (need_rtp_packet_infos_ &&
      packet->packet_type() == RtpPacketToSend::Type::kVideo) {
    worker_queue_->PostTask(SafeTask(
        task_safety_.flag(),
        [this, packet_timestamp = packet->Timestamp(),
         is_first_packet_of_frame = packet->is_first_packet_of_frame(),
         is_last_packet_of_frame = packet->Marker(),
         sequence_number = packet->SequenceNumber()]() {
          RTC_DCHECK_RUN_ON(worker_queue_);
          // Last packet of a frame, add it to sequence number info map.
          const uint32_t timestamp = packet_timestamp - timestamp_offset_;
          rtp_sequence_number_map_->InsertPacket(
              sequence_number,
              RtpSequenceNumberMap::Info(timestamp, is_first_packet_of_frame,
                                         is_last_packet_of_frame));
        }));
  }

  if (fec_generator_ && packet->fec_protect_packet()) {
    // This packet should be protected by FEC, add it to packet generator.
    RTC_DCHECK(fec_generator_);
    RTC_DCHECK(packet->packet_type() == RtpPacketMediaType::kVideo);
    absl::optional<std::pair<FecProtectionParams, FecProtectionParams>>
        new_fec_params;
    {
      MutexLock lock(&lock_);
      new_fec_params.swap(pending_fec_params_);
    }
    if (new_fec_params) {
      fec_generator_->SetProtectionParameters(new_fec_params->first,
                                              new_fec_params->second);
    }
    if (packet->is_red()) {
      RtpPacketToSend unpacked_packet(*packet);

      const rtc::CopyOnWriteBuffer buffer = packet->Buffer();
      // Grab media payload type from RED header.
      const size_t headers_size = packet->headers_size();
      unpacked_packet.SetPayloadType(buffer[headers_size]);

      // Copy the media payload into the unpacked buffer.
      uint8_t* payload_buffer =
          unpacked_packet.SetPayloadSize(packet->payload_size() - 1);
      std::copy(&packet->payload()[0] + 1,
                &packet->payload()[0] + packet->payload_size(), payload_buffer);

      fec_generator_->AddPacketAndGenerateFec(unpacked_packet);
    } else {
      // If not RED encapsulated - we can just insert packet directly.
      fec_generator_->AddPacketAndGenerateFec(*packet);
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
  if (packet->HasExtension<TransmissionOffset>() &&
      packet->capture_time() > Timestamp::Zero()) {
    TimeDelta diff = now - packet->capture_time();
    packet->SetExtension<TransmissionOffset>(kTimestampTicksPerMs * diff.ms());
  }
  if (packet->HasExtension<AbsoluteSendTime>()) {
    packet->SetExtension<AbsoluteSendTime>(AbsoluteSendTime::To24Bits(now));
  }

  if (packet->HasExtension<VideoTimingExtension>()) {
    if (populate_network2_timestamp_) {
      packet->set_network2_time(now);
    } else {
      packet->set_pacer_exit_time(now);
    }
  }

  auto compound_packet = Packet{std::move(packet), pacing_info, now};
  if (enable_send_packet_batching_ && !is_audio_) {
    packets_to_send_.push_back(std::move(compound_packet));
  } else {
    CompleteSendPacket(compound_packet, false);
  }
}

void RtpSenderEgress::OnBatchComplete() {
  RTC_DCHECK_RUN_ON(&pacer_checker_);
  for (auto& packet : packets_to_send_) {
    CompleteSendPacket(packet, &packet == &packets_to_send_.back());
  }
  packets_to_send_.clear();
}

void RtpSenderEgress::CompleteSendPacket(const Packet& compound_packet,
                                         bool last_in_batch) {
  auto& [packet, pacing_info, now] = compound_packet;

  const bool is_media = packet->packet_type() == RtpPacketMediaType::kAudio ||
                        packet->packet_type() == RtpPacketMediaType::kVideo;

  PacketOptions options;
  {
    MutexLock lock(&lock_);
    options.included_in_allocation = force_part_of_allocation_;
  }

  // Downstream code actually uses this flag to distinguish between media and
  // everything else.
  options.is_retransmit = !is_media;
  if (auto packet_id = packet->GetExtension<TransportSequenceNumber>()) {
    options.packet_id = *packet_id;
    options.included_in_feedback = true;
    options.included_in_allocation = true;
    AddPacketToTransportFeedback(*packet_id, *packet, pacing_info);
  }

  options.additional_data = packet->additional_data();

  const uint32_t packet_ssrc = packet->Ssrc();
  if (packet->packet_type() != RtpPacketMediaType::kPadding &&
      packet->packet_type() != RtpPacketMediaType::kRetransmission) {
    UpdateDelayStatistics(packet->capture_time(), now, packet_ssrc);
    UpdateOnSendPacket(options.packet_id, packet->capture_time(), packet_ssrc);
  }
  options.batchable = enable_send_packet_batching_ && !is_audio_;
  options.last_packet_in_batch = last_in_batch;
  const bool send_success = SendPacketToNetwork(*packet, options, pacing_info);

  // Put packet in retransmission history or update pending status even if
  // actual sending fails.
  if (is_media && packet->allow_retransmission()) {
    packet_history_->PutRtpPacket(std::make_unique<RtpPacketToSend>(*packet),
                                  now);
  } else if (packet->retransmitted_sequence_number()) {
    packet_history_->MarkPacketAsSent(*packet->retransmitted_sequence_number());
  }

  if (send_success) {
    // `media_has_been_sent_` is used by RTPSender to figure out if it can send
    // padding in the absence of transport-cc or abs-send-time.
    // In those cases media must be sent first to set a reference timestamp.
    media_has_been_sent_ = true;

    // TODO(sprang): Add support for FEC protecting all header extensions, add
    // media packet to generator here instead.

    RTC_DCHECK(packet->packet_type().has_value());
    RtpPacketMediaType packet_type = *packet->packet_type();
    RtpPacketCounter counter(*packet);
    size_t size = packet->size();
    // TODO(crbug.com/1373439): clean up task posting when the combined
    // network/worker project launches.
    if (TaskQueueBase::Current() != worker_queue_) {
      worker_queue_->PostTask(SafeTask(
          task_safety_.flag(), [this, now = now, packet_ssrc, packet_type,
                                counter = std::move(counter), size]() {
            RTC_DCHECK_RUN_ON(worker_queue_);
            UpdateRtpStats(now, packet_ssrc, packet_type, std::move(counter),
                           size);
          }));
    } else {
      RTC_DCHECK_RUN_ON(worker_queue_);
      UpdateRtpStats(now, packet_ssrc, packet_type, std::move(counter), size);
    }
  }
}

RtpSendRates RtpSenderEgress::GetSendRates() const {
  MutexLock lock(&lock_);
  return GetSendRatesLocked(clock_->CurrentTime());
}

RtpSendRates RtpSenderEgress::GetSendRatesLocked(Timestamp now) const {
  RtpSendRates current_rates;
  for (size_t i = 0; i < kNumMediaTypes; ++i) {
    RtpPacketMediaType type = static_cast<RtpPacketMediaType>(i);
    current_rates[type] =
        DataRate::BitsPerSec(send_rates_[i].Rate(now.ms()).value_or(0));
  }
  return current_rates;
}

void RtpSenderEgress::GetDataCounters(StreamDataCounters* rtp_stats,
                                      StreamDataCounters* rtx_stats) const {
  // TODO(bugs.webrtc.org/11581): make sure rtx_rtp_stats_ and rtp_stats_ are
  // only touched on the worker thread.
  MutexLock lock(&lock_);
  *rtp_stats = rtp_stats_;
  *rtx_stats = rtx_rtp_stats_;
}

void RtpSenderEgress::ForceIncludeSendPacketsInAllocation(
    bool part_of_allocation) {
  MutexLock lock(&lock_);
  force_part_of_allocation_ = part_of_allocation;
}

bool RtpSenderEgress::MediaHasBeenSent() const {
  RTC_DCHECK_RUN_ON(&pacer_checker_);
  return media_has_been_sent_;
}

void RtpSenderEgress::SetMediaHasBeenSent(bool media_sent) {
  RTC_DCHECK_RUN_ON(&pacer_checker_);
  media_has_been_sent_ = media_sent;
}

void RtpSenderEgress::SetTimestampOffset(uint32_t timestamp) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  timestamp_offset_ = timestamp;
}

std::vector<RtpSequenceNumberMap::Info> RtpSenderEgress::GetSentRtpPacketInfos(
    rtc::ArrayView<const uint16_t> sequence_numbers) const {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_DCHECK(!sequence_numbers.empty());
  if (!need_rtp_packet_infos_) {
    return std::vector<RtpSequenceNumberMap::Info>();
  }

  std::vector<RtpSequenceNumberMap::Info> results;
  results.reserve(sequence_numbers.size());

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

void RtpSenderEgress::SetFecProtectionParameters(
    const FecProtectionParams& delta_params,
    const FecProtectionParams& key_params) {
  // TODO(sprang): Post task to pacer queue instead, one pacer is fully
  // migrated to a task queue.
  MutexLock lock(&lock_);
  pending_fec_params_.emplace(delta_params, key_params);
}

std::vector<std::unique_ptr<RtpPacketToSend>>
RtpSenderEgress::FetchFecPackets() {
  RTC_DCHECK_RUN_ON(&pacer_checker_);
  if (fec_generator_) {
    return fec_generator_->GetFecPackets();
  }
  return {};
}

void RtpSenderEgress::OnAbortedRetransmissions(
    rtc::ArrayView<const uint16_t> sequence_numbers) {
  RTC_DCHECK_RUN_ON(&pacer_checker_);
  // Mark aborted retransmissions as sent, rather than leaving them in
  // a 'pending' state - otherwise they can not be requested again and
  // will not be cleared until the history has reached its max size.
  for (uint16_t seq_no : sequence_numbers) {
    packet_history_->MarkPacketAsSent(seq_no);
  }
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
    RtpPacketSendInfo packet_info;
    packet_info.transport_sequence_number = packet_id;
    packet_info.rtp_timestamp = packet.Timestamp();
    packet_info.length = packet.size();
    packet_info.pacing_info = pacing_info;
    packet_info.packet_type = packet.packet_type();

    switch (*packet_info.packet_type) {
      case RtpPacketMediaType::kAudio:
      case RtpPacketMediaType::kVideo:
        packet_info.media_ssrc = ssrc_;
        packet_info.rtp_sequence_number = packet.SequenceNumber();
        break;
      case RtpPacketMediaType::kRetransmission:
        // For retransmissions, we're want to remove the original media packet
        // if the retransmit arrives - so populate that in the packet info.
        packet_info.media_ssrc = ssrc_;
        packet_info.rtp_sequence_number =
            *packet.retransmitted_sequence_number();
        break;
      case RtpPacketMediaType::kPadding:
      case RtpPacketMediaType::kForwardErrorCorrection:
        // We're not interested in feedback about these packets being received
        // or lost.
        break;
    }

    transport_feedback_observer_->OnAddPacket(packet_info);
  }
}

void RtpSenderEgress::UpdateDelayStatistics(Timestamp capture_time,
                                            Timestamp now,
                                            uint32_t ssrc) {
  if (!send_side_delay_observer_ || capture_time.IsInfinite())
    return;

  TimeDelta avg_delay = TimeDelta::Zero();
  TimeDelta max_delay = TimeDelta::Zero();
  {
    MutexLock lock(&lock_);
    // Compute the max and average of the recent capture-to-send delays.
    // The time complexity of the current approach depends on the distribution
    // of the delay values. This could be done more efficiently.

    // Remove elements older than kSendSideDelayWindowMs.
    auto lower_bound = send_delays_.lower_bound(now - kSendSideDelayWindow);
    for (auto it = send_delays_.begin(); it != lower_bound; ++it) {
      if (max_delay_it_ == it) {
        max_delay_it_ = send_delays_.end();
      }
      sum_delays_ -= it->second;
    }
    send_delays_.erase(send_delays_.begin(), lower_bound);
    if (max_delay_it_ == send_delays_.end()) {
      // Removed the previous max. Need to recompute.
      RecomputeMaxSendDelay();
    }

    // Add the new element.
    TimeDelta new_send_delay = now - capture_time;
    auto [it, inserted] = send_delays_.emplace(now, new_send_delay);
    if (!inserted) {
      // TODO(terelius): If we have multiple delay measurements during the same
      // millisecond then we keep the most recent one. It is not clear that this
      // is the right decision, but it preserves an earlier behavior.
      TimeDelta previous_send_delay = it->second;
      sum_delays_ -= previous_send_delay;
      it->second = new_send_delay;
      if (max_delay_it_ == it && new_send_delay < previous_send_delay) {
        RecomputeMaxSendDelay();
      }
    }
    if (max_delay_it_ == send_delays_.end() ||
        it->second >= max_delay_it_->second) {
      max_delay_it_ = it;
    }
    sum_delays_ += new_send_delay;

    size_t num_delays = send_delays_.size();
    RTC_DCHECK(max_delay_it_ != send_delays_.end());
    max_delay = max_delay_it_->second;
    avg_delay = sum_delays_ / num_delays;
  }
  send_side_delay_observer_->SendSideDelayUpdated(avg_delay.ms(),
                                                  max_delay.ms(), ssrc);
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
                                         Timestamp capture_time,
                                         uint32_t ssrc) {
  if (!send_packet_observer_ || capture_time.IsInfinite() || packet_id == -1) {
    return;
  }

  send_packet_observer_->OnSendPacket(packet_id, capture_time.ms(), ssrc);
}

bool RtpSenderEgress::SendPacketToNetwork(const RtpPacketToSend& packet,
                                          const PacketOptions& options,
                                          const PacedPacketInfo& pacing_info) {
  int bytes_sent = -1;
  if (transport_) {
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

void RtpSenderEgress::UpdateRtpStats(Timestamp now,
                                     uint32_t packet_ssrc,
                                     RtpPacketMediaType packet_type,
                                     RtpPacketCounter counter,
                                     size_t packet_size) {
  RTC_DCHECK_RUN_ON(worker_queue_);

  // TODO(bugs.webrtc.org/11581): send_rates_ should be touched only on the
  // worker thread.
  RtpSendRates send_rates;
  {
    MutexLock lock(&lock_);

    // TODO(bugs.webrtc.org/11581): make sure rtx_rtp_stats_ and rtp_stats_ are
    // only touched on the worker thread.
    StreamDataCounters* counters =
        packet_ssrc == rtx_ssrc_ ? &rtx_rtp_stats_ : &rtp_stats_;

    if (counters->first_packet_time_ms == -1) {
      counters->first_packet_time_ms = now.ms();
    }

    if (packet_type == RtpPacketMediaType::kForwardErrorCorrection) {
      counters->fec.Add(counter);
    } else if (packet_type == RtpPacketMediaType::kRetransmission) {
      counters->retransmitted.Add(counter);
    }
    counters->transmitted.Add(counter);

    send_rates_[static_cast<size_t>(packet_type)].Update(packet_size, now.ms());
    if (bitrate_callback_) {
      send_rates = GetSendRatesLocked(now);
    }

    if (rtp_stats_callback_) {
      rtp_stats_callback_->DataCountersUpdated(*counters, packet_ssrc);
    }
  }

  // The bitrate_callback_ and rtp_stats_callback_ pointers in practice point
  // to the same object, so these callbacks could be consolidated into one.
  if (bitrate_callback_) {
    bitrate_callback_->Notify(
        send_rates.Sum().bps(),
        send_rates[RtpPacketMediaType::kRetransmission].bps(), ssrc_);
  }
}

void RtpSenderEgress::PeriodicUpdate() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_DCHECK(bitrate_callback_);
  RtpSendRates send_rates = GetSendRates();
  bitrate_callback_->Notify(
      send_rates.Sum().bps(),
      send_rates[RtpPacketMediaType::kRetransmission].bps(), ssrc_);
}

#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
void RtpSenderEgress::BweTestLoggingPlot(Timestamp now, uint32_t packet_ssrc) {
  RTC_DCHECK_RUN_ON(worker_queue_);

  const auto rates = GetSendRates();
  if (is_audio_) {
    BWE_TEST_LOGGING_PLOT_WITH_SSRC(1, "AudioTotBitrate_kbps", now.ms(),
                                    rates.Sum().kbps(), packet_ssrc);
    BWE_TEST_LOGGING_PLOT_WITH_SSRC(
        1, "AudioNackBitrate_kbps", now.ms(),
        rates[RtpPacketMediaType::kRetransmission].kbps(), packet_ssrc);
  } else {
    BWE_TEST_LOGGING_PLOT_WITH_SSRC(1, "VideoTotBitrate_kbps", now.ms(),
                                    rates.Sum().kbps(), packet_ssrc);
    BWE_TEST_LOGGING_PLOT_WITH_SSRC(
        1, "VideoNackBitrate_kbps", now.ms(),
        rates[RtpPacketMediaType::kRetransmission].kbps(), packet_ssrc);
  }
}
#endif  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE

}  // namespace webrtc
