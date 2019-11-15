/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/pacing_controller.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "modules/pacing/bitrate_prober.h"
#include "modules/pacing/interval_budget.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {
// Time limit in milliseconds between packet bursts.
constexpr TimeDelta kDefaultMinPacketLimit = TimeDelta::Millis<5>();
constexpr TimeDelta kCongestedPacketInterval = TimeDelta::Millis<500>();
constexpr TimeDelta kMaxElapsedTime = TimeDelta::Seconds<2>();

// Upper cap on process interval, in case process has not been called in a long
// time.
constexpr TimeDelta kMaxProcessingInterval = TimeDelta::Millis<30>();

bool IsDisabled(const WebRtcKeyValueConfig& field_trials,
                absl::string_view key) {
  return field_trials.Lookup(key).find("Disabled") == 0;
}

bool IsEnabled(const WebRtcKeyValueConfig& field_trials,
               absl::string_view key) {
  return field_trials.Lookup(key).find("Enabled") == 0;
}

int GetPriorityForType(RtpPacketToSend::Type type) {
  switch (type) {
    case RtpPacketToSend::Type::kAudio:
      // Audio is always prioritized over other packet types.
      return 0;
    case RtpPacketToSend::Type::kRetransmission:
      // Send retransmissions before new media.
      return 1;
    case RtpPacketToSend::Type::kVideo:
      // Video has "normal" priority, in the old speak.
      return 2;
    case RtpPacketToSend::Type::kForwardErrorCorrection:
      // Send redundancy concurrently to video. If it is delayed it might have a
      // lower chance of being useful.
      return 2;
    case RtpPacketToSend::Type::kPadding:
      // Packets that are in themselves likely useless, only sent to keep the
      // BWE high.
      return 3;
  }
}

}  // namespace

const TimeDelta PacingController::kMaxExpectedQueueLength =
    TimeDelta::Millis<2000>();
const float PacingController::kDefaultPaceMultiplier = 2.5f;
const TimeDelta PacingController::kPausedProcessInterval =
    kCongestedPacketInterval;

PacingController::PacingController(Clock* clock,
                                   PacketSender* packet_sender,
                                   RtcEventLog* event_log,
                                   const WebRtcKeyValueConfig* field_trials)
    : clock_(clock),
      packet_sender_(packet_sender),
      fallback_field_trials_(
          !field_trials ? absl::make_unique<FieldTrialBasedConfig>() : nullptr),
      field_trials_(field_trials ? field_trials : fallback_field_trials_.get()),
      drain_large_queues_(
          !IsDisabled(*field_trials_, "WebRTC-Pacer-DrainQueue")),
      send_padding_if_silent_(
          IsEnabled(*field_trials_, "WebRTC-Pacer-PadInSilence")),
      pace_audio_(!IsDisabled(*field_trials_, "WebRTC-Pacer-BlockAudio")),
      min_packet_limit_(kDefaultMinPacketLimit),
      last_timestamp_(clock_->CurrentTime()),
      paused_(false),
      media_budget_(0),
      padding_budget_(0),
      prober_(*field_trials_),
      probing_send_failure_(false),
      padding_failure_state_(false),
      pacing_bitrate_(DataRate::Zero()),
      time_last_process_(clock->CurrentTime()),
      last_send_time_(time_last_process_),
      packet_queue_(time_last_process_, field_trials),
      packet_counter_(0),
      congestion_window_size_(DataSize::PlusInfinity()),
      outstanding_data_(DataSize::Zero()),
      queue_time_limit(kMaxExpectedQueueLength),
      account_for_audio_(false) {
  if (!drain_large_queues_) {
    RTC_LOG(LS_WARNING) << "Pacer queues will not be drained,"
                           "pushback experiment must be enabled.";
  }
  FieldTrialParameter<int> min_packet_limit_ms("", min_packet_limit_.ms());
  ParseFieldTrial({&min_packet_limit_ms},
                  field_trials_->Lookup("WebRTC-Pacer-MinPacketLimitMs"));
  min_packet_limit_ = TimeDelta::ms(min_packet_limit_ms.Get());
  UpdateBudgetWithElapsedTime(min_packet_limit_);
}

PacingController::~PacingController() = default;

void PacingController::CreateProbeCluster(DataRate bitrate, int cluster_id) {
  prober_.CreateProbeCluster(bitrate.bps(), CurrentTime().ms(), cluster_id);
}

void PacingController::Pause() {
  if (!paused_)
    RTC_LOG(LS_INFO) << "PacedSender paused.";
  paused_ = true;
  packet_queue_.SetPauseState(true, CurrentTime());
}

void PacingController::Resume() {
  if (paused_)
    RTC_LOG(LS_INFO) << "PacedSender resumed.";
  paused_ = false;
  packet_queue_.SetPauseState(false, CurrentTime());
}

bool PacingController::IsPaused() const {
  return paused_;
}

void PacingController::SetCongestionWindow(DataSize congestion_window_size) {
  congestion_window_size_ = congestion_window_size;
}

void PacingController::UpdateOutstandingData(DataSize outstanding_data) {
  outstanding_data_ = outstanding_data;
}

bool PacingController::Congested() const {
  if (congestion_window_size_.IsFinite()) {
    return outstanding_data_ >= congestion_window_size_;
  }
  return false;
}

Timestamp PacingController::CurrentTime() const {
  Timestamp time = clock_->CurrentTime();
  if (time < last_timestamp_) {
    RTC_LOG(LS_WARNING)
        << "Non-monotonic clock behavior observed. Previous timestamp: "
        << last_timestamp_.ms() << ", new timestamp: " << time.ms();
    RTC_DCHECK_GE(time, last_timestamp_);
    time = last_timestamp_;
  }
  last_timestamp_ = time;
  return time;
}

void PacingController::SetProbingEnabled(bool enabled) {
  RTC_CHECK_EQ(0, packet_counter_);
  prober_.SetEnabled(enabled);
}

void PacingController::SetPacingRates(DataRate pacing_rate,
                                      DataRate padding_rate) {
  RTC_DCHECK_GT(pacing_rate, DataRate::Zero());
  pacing_bitrate_ = pacing_rate;
  padding_budget_.set_target_rate_kbps(padding_rate.kbps());

  RTC_LOG(LS_VERBOSE) << "bwe:pacer_updated pacing_kbps="
                      << pacing_bitrate_.kbps()
                      << " padding_budget_kbps=" << padding_rate.kbps();
}

void PacingController::EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) {
  RTC_DCHECK(pacing_bitrate_ > DataRate::Zero())
      << "SetPacingRate must be called before InsertPacket.";

  Timestamp now = CurrentTime();
  prober_.OnIncomingPacket(packet->payload_size());

  if (packet->capture_time_ms() < 0) {
    packet->set_capture_time_ms(now.ms());
  }

  RTC_CHECK(packet->packet_type());
  int priority = GetPriorityForType(*packet->packet_type());
  packet_queue_.Push(priority, now, packet_counter_++, std::move(packet));
}

void PacingController::SetAccountForAudioPackets(bool account_for_audio) {
  account_for_audio_ = account_for_audio;
}

TimeDelta PacingController::ExpectedQueueTime() const {
  RTC_DCHECK_GT(pacing_bitrate_, DataRate::Zero());
  return TimeDelta::ms(
      (QueueSizeData().bytes() * 8 * rtc::kNumMillisecsPerSec) /
      pacing_bitrate_.bps());
}

size_t PacingController::QueueSizePackets() const {
  return packet_queue_.SizeInPackets();
}

DataSize PacingController::QueueSizeData() const {
  return packet_queue_.Size();
}

absl::optional<Timestamp> PacingController::FirstSentPacketTime() const {
  return first_sent_packet_time_;
}

TimeDelta PacingController::OldestPacketWaitTime() const {
  Timestamp oldest_packet = packet_queue_.OldestEnqueueTime();
  if (oldest_packet.IsInfinite()) {
    return TimeDelta::Zero();
  }

  return CurrentTime() - oldest_packet;
}

TimeDelta PacingController::UpdateTimeAndGetElapsed(Timestamp now) {
  TimeDelta elapsed_time = now - time_last_process_;
  time_last_process_ = now;
  if (elapsed_time > kMaxElapsedTime) {
    RTC_LOG(LS_WARNING) << "Elapsed time (" << elapsed_time.ms()
                        << " ms) longer than expected, limiting to "
                        << kMaxElapsedTime.ms();
    elapsed_time = kMaxElapsedTime;
  }
  return elapsed_time;
}

bool PacingController::ShouldSendKeepalive(Timestamp now) const {
  if (send_padding_if_silent_ || paused_ || Congested()) {
    // We send a padding packet every 500 ms to ensure we won't get stuck in
    // congested state due to no feedback being received.
    TimeDelta elapsed_since_last_send = now - last_send_time_;
    if (elapsed_since_last_send >= kCongestedPacketInterval) {
      // We can not send padding unless a normal packet has first been sent. If
      // we do, timestamps get messed up.
      if (packet_counter_ > 0) {
        return true;
      }
    }
  }
  return false;
}

absl::optional<TimeDelta> PacingController::TimeUntilNextProbe() {
  if (!prober_.IsProbing()) {
    return absl::nullopt;
  }

  TimeDelta time_delta =
      TimeDelta::ms(prober_.TimeUntilNextProbe(CurrentTime().ms()));
  if (time_delta > TimeDelta::Zero() ||
      (time_delta == TimeDelta::Zero() && !probing_send_failure_)) {
    return time_delta;
  }

  return absl::nullopt;
}

TimeDelta PacingController::TimeElapsedSinceLastProcess() const {
  return CurrentTime() - time_last_process_;
}

void PacingController::ProcessPackets() {
  Timestamp now = CurrentTime();
  TimeDelta elapsed_time = UpdateTimeAndGetElapsed(now);
  if (ShouldSendKeepalive(now)) {
    DataSize keepalive_data_sent = DataSize::Zero();
    std::vector<std::unique_ptr<RtpPacketToSend>> keepalive_packets =
        packet_sender_->GeneratePadding(DataSize::bytes(1));
    for (auto& packet : keepalive_packets) {
      keepalive_data_sent +=
          DataSize::bytes(packet->payload_size() + packet->padding_size());
      packet_sender_->SendRtpPacket(std::move(packet), PacedPacketInfo());
    }
    OnPaddingSent(keepalive_data_sent);
  }

  if (paused_)
    return;

  if (elapsed_time > TimeDelta::Zero()) {
    DataRate target_rate = pacing_bitrate_;
    DataSize queue_size_data = packet_queue_.Size();
    if (queue_size_data > DataSize::Zero()) {
      // Assuming equal size packets and input/output rate, the average packet
      // has avg_time_left_ms left to get queue_size_bytes out of the queue, if
      // time constraint shall be met. Determine bitrate needed for that.
      packet_queue_.UpdateQueueTime(CurrentTime());
      if (drain_large_queues_) {
        TimeDelta avg_time_left =
            std::max(TimeDelta::ms(1),
                     queue_time_limit - packet_queue_.AverageQueueTime());
        DataRate min_rate_needed = queue_size_data / avg_time_left;
        if (min_rate_needed > target_rate) {
          target_rate = min_rate_needed;
          RTC_LOG(LS_VERBOSE) << "bwe:large_pacing_queue pacing_rate_kbps="
                              << target_rate.kbps();
        }
      }
    }

    media_budget_.set_target_rate_kbps(target_rate.kbps());
    UpdateBudgetWithElapsedTime(elapsed_time);
  }

  bool is_probing = prober_.IsProbing();
  PacedPacketInfo pacing_info;
  absl::optional<DataSize> recommended_probe_size;
  if (is_probing) {
    pacing_info = prober_.CurrentCluster();
    recommended_probe_size = DataSize::bytes(prober_.RecommendedMinProbeSize());
  }

  DataSize data_sent = DataSize::Zero();
  // The paused state is checked in the loop since it leaves the critical
  // section allowing the paused state to be changed from other code.
  while (!paused_) {
    auto* packet = GetPendingPacket(pacing_info);
    if (packet == nullptr) {
      // No packet available to send, check if we should send padding.
      DataSize padding_to_add = PaddingToAdd(recommended_probe_size, data_sent);
      if (padding_to_add > DataSize::Zero()) {
        std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets =
            packet_sender_->GeneratePadding(padding_to_add);
        if (padding_packets.empty()) {
          // No padding packets were generated, quite send loop.
          break;
        }
        for (auto& packet : padding_packets) {
          EnqueuePacket(std::move(packet));
        }
        // Continue loop to send the padding that was just added.
        continue;
      }

      // Can't fetch new packet and no padding to send, exit send loop.
      break;
    }

    std::unique_ptr<RtpPacketToSend> rtp_packet = packet->ReleasePacket();
    RTC_DCHECK(rtp_packet);
    packet_sender_->SendRtpPacket(std::move(rtp_packet), pacing_info);

    data_sent += packet->size();
    // Send succeeded, remove it from the queue.
    OnPacketSent(packet);
    if (recommended_probe_size && data_sent > *recommended_probe_size)
      break;
  }

  if (is_probing) {
    probing_send_failure_ = data_sent == DataSize::Zero();
    if (!probing_send_failure_) {
      prober_.ProbeSent(CurrentTime().ms(), data_sent.bytes());
    }
  }
}

DataSize PacingController::PaddingToAdd(
    absl::optional<DataSize> recommended_probe_size,
    DataSize data_sent) {
  if (!packet_queue_.Empty()) {
    // Actual payload available, no need to add padding.
    return DataSize::Zero();
  }

  if (Congested()) {
    // Don't add padding if congested, even if requested for probing.
    return DataSize::Zero();
  }

  if (packet_counter_ == 0) {
    // We can not send padding unless a normal packet has first been sent. If we
    // do, timestamps get messed up.
    return DataSize::Zero();
  }

  if (recommended_probe_size) {
    if (*recommended_probe_size > data_sent) {
      return *recommended_probe_size - data_sent;
    }
    return DataSize::Zero();
  }

  return DataSize::bytes(padding_budget_.bytes_remaining());
}

RoundRobinPacketQueue::QueuedPacket* PacingController::GetPendingPacket(
    const PacedPacketInfo& pacing_info) {
  if (packet_queue_.Empty()) {
    return nullptr;
  }

  // Since we need to release the lock in order to send, we first pop the
  // element from the priority queue but keep it in storage, so that we can
  // reinsert it if send fails.
  RoundRobinPacketQueue::QueuedPacket* packet = packet_queue_.BeginPop();
  bool audio_packet = packet->type() == RtpPacketToSend::Type::kAudio;
  bool apply_pacing = !audio_packet || pace_audio_;
  if (apply_pacing && (Congested() || (media_budget_.bytes_remaining() == 0 &&
                                       pacing_info.probe_cluster_id ==
                                           PacedPacketInfo::kNotAProbe))) {
    packet_queue_.CancelPop();
    return nullptr;
  }
  return packet;
}

void PacingController::OnPacketSent(
    RoundRobinPacketQueue::QueuedPacket* packet) {
  Timestamp now = CurrentTime();
  if (!first_sent_packet_time_) {
    first_sent_packet_time_ = now;
  }
  bool audio_packet = packet->type() == RtpPacketToSend::Type::kAudio;
  if (!audio_packet || account_for_audio_) {
    // Update media bytes sent.
    UpdateBudgetWithSentData(packet->size());
    last_send_time_ = now;
  }
  // Send succeeded, remove it from the queue.
  packet_queue_.FinalizePop();
  padding_failure_state_ = false;
}

void PacingController::OnPaddingSent(DataSize data_sent) {
  if (data_sent > DataSize::Zero()) {
    UpdateBudgetWithSentData(data_sent);
  } else {
    padding_failure_state_ = true;
  }
  last_send_time_ = CurrentTime();
}

void PacingController::UpdateBudgetWithElapsedTime(TimeDelta delta) {
  delta = std::min(kMaxProcessingInterval, delta);
  media_budget_.IncreaseBudget(delta.ms());
  padding_budget_.IncreaseBudget(delta.ms());
}

void PacingController::UpdateBudgetWithSentData(DataSize size) {
  outstanding_data_ += size;
  media_budget_.UseBudget(size.bytes());
  padding_budget_.UseBudget(size.bytes());
}

void PacingController::SetQueueTimeLimit(TimeDelta limit) {
  queue_time_limit = limit;
}

}  // namespace webrtc
