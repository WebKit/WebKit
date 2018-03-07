/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/paced_sender.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <vector>
#include <utility>

#include "modules/include/module_common_types.h"
#include "modules/pacing/alr_detector.h"
#include "modules/pacing/bitrate_prober.h"
#include "modules/pacing/interval_budget.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"

namespace {
// Time limit in milliseconds between packet bursts.
const int64_t kMinPacketLimitMs = 5;
const int64_t kPausedPacketIntervalMs = 500;

// Upper cap on process interval, in case process has not been called in a long
// time.
const int64_t kMaxIntervalTimeMs = 30;

}  // namespace

namespace webrtc {

const int64_t PacedSender::kMaxQueueLengthMs = 2000;
const float PacedSender::kDefaultPaceMultiplier = 2.5f;

PacedSender::PacedSender(const Clock* clock,
                         PacketSender* packet_sender,
                         RtcEventLog* event_log) :
    PacedSender(clock, packet_sender, event_log,
                webrtc::field_trial::IsEnabled("WebRTC-RoundRobinPacing")
                    ? rtc::MakeUnique<PacketQueue2>(clock)
                    : rtc::MakeUnique<PacketQueue>(clock)) {}

PacedSender::PacedSender(const Clock* clock,
                         PacketSender* packet_sender,
                         RtcEventLog* event_log,
                         std::unique_ptr<PacketQueue> packets)
    : clock_(clock),
      packet_sender_(packet_sender),
      alr_detector_(rtc::MakeUnique<AlrDetector>()),
      paused_(false),
      media_budget_(rtc::MakeUnique<IntervalBudget>(0)),
      padding_budget_(rtc::MakeUnique<IntervalBudget>(0)),
      prober_(rtc::MakeUnique<BitrateProber>(event_log)),
      probing_send_failure_(false),
      estimated_bitrate_bps_(0),
      min_send_bitrate_kbps_(0u),
      max_padding_bitrate_kbps_(0u),
      pacing_bitrate_kbps_(0),
      time_last_update_us_(clock->TimeInMicroseconds()),
      first_sent_packet_ms_(-1),
      packets_(std::move(packets)),
      packet_counter_(0),
      pacing_factor_(kDefaultPaceMultiplier),
      queue_time_limit(kMaxQueueLengthMs),
      account_for_audio_(false) {
  UpdateBudgetWithElapsedTime(kMinPacketLimitMs);
}

PacedSender::~PacedSender() {}

void PacedSender::CreateProbeCluster(int bitrate_bps) {
  rtc::CritScope cs(&critsect_);
  prober_->CreateProbeCluster(bitrate_bps, clock_->TimeInMilliseconds());
}

void PacedSender::Pause() {
  {
    rtc::CritScope cs(&critsect_);
    if (!paused_)
      RTC_LOG(LS_INFO) << "PacedSender paused.";
    paused_ = true;
    packets_->SetPauseState(true, clock_->TimeInMilliseconds());
  }
  // Tell the process thread to call our TimeUntilNextProcess() method to get
  // a new (longer) estimate for when to call Process().
  if (process_thread_)
    process_thread_->WakeUp(this);
}

void PacedSender::Resume() {
  {
    rtc::CritScope cs(&critsect_);
    if (paused_)
      RTC_LOG(LS_INFO) << "PacedSender resumed.";
    paused_ = false;
    packets_->SetPauseState(false, clock_->TimeInMilliseconds());
  }
  // Tell the process thread to call our TimeUntilNextProcess() method to
  // refresh the estimate for when to call Process().
  if (process_thread_)
    process_thread_->WakeUp(this);
}

void PacedSender::SetProbingEnabled(bool enabled) {
  rtc::CritScope cs(&critsect_);
  RTC_CHECK_EQ(0, packet_counter_);
  prober_->SetEnabled(enabled);
}

void PacedSender::SetEstimatedBitrate(uint32_t bitrate_bps) {
  if (bitrate_bps == 0)
    RTC_LOG(LS_ERROR) << "PacedSender is not designed to handle 0 bitrate.";
  rtc::CritScope cs(&critsect_);
  estimated_bitrate_bps_ = bitrate_bps;
  padding_budget_->set_target_rate_kbps(
      std::min(estimated_bitrate_bps_ / 1000, max_padding_bitrate_kbps_));
  pacing_bitrate_kbps_ =
      std::max(min_send_bitrate_kbps_, estimated_bitrate_bps_ / 1000) *
      pacing_factor_;
  alr_detector_->SetEstimatedBitrate(bitrate_bps);
}

void PacedSender::SetSendBitrateLimits(int min_send_bitrate_bps,
                                       int padding_bitrate) {
  rtc::CritScope cs(&critsect_);
  min_send_bitrate_kbps_ = min_send_bitrate_bps / 1000;
  pacing_bitrate_kbps_ =
      std::max(min_send_bitrate_kbps_, estimated_bitrate_bps_ / 1000) *
      pacing_factor_;
  max_padding_bitrate_kbps_ = padding_bitrate / 1000;
  padding_budget_->set_target_rate_kbps(
      std::min(estimated_bitrate_bps_ / 1000, max_padding_bitrate_kbps_));
}

void PacedSender::InsertPacket(RtpPacketSender::Priority priority,
                               uint32_t ssrc,
                               uint16_t sequence_number,
                               int64_t capture_time_ms,
                               size_t bytes,
                               bool retransmission) {
  rtc::CritScope cs(&critsect_);
  RTC_DCHECK(estimated_bitrate_bps_ > 0)
        << "SetEstimatedBitrate must be called before InsertPacket.";

  int64_t now_ms = clock_->TimeInMilliseconds();
  prober_->OnIncomingPacket(bytes);

  if (capture_time_ms < 0)
    capture_time_ms = now_ms;

  packets_->Push(PacketQueue::Packet(priority, ssrc, sequence_number,
                                     capture_time_ms, now_ms, bytes,
                                     retransmission, packet_counter_++));
}

void PacedSender::SetAccountForAudioPackets(bool account_for_audio) {
  rtc::CritScope cs(&critsect_);
  account_for_audio_ = account_for_audio;
}

int64_t PacedSender::ExpectedQueueTimeMs() const {
  rtc::CritScope cs(&critsect_);
  RTC_DCHECK_GT(pacing_bitrate_kbps_, 0);
  return static_cast<int64_t>(packets_->SizeInBytes() * 8 /
                              pacing_bitrate_kbps_);
}

rtc::Optional<int64_t> PacedSender::GetApplicationLimitedRegionStartTime()
    const {
  rtc::CritScope cs(&critsect_);
  return alr_detector_->GetApplicationLimitedRegionStartTime();
}

size_t PacedSender::QueueSizePackets() const {
  rtc::CritScope cs(&critsect_);
  return packets_->SizeInPackets();
}

int64_t PacedSender::FirstSentPacketTimeMs() const {
  rtc::CritScope cs(&critsect_);
  return first_sent_packet_ms_;
}

int64_t PacedSender::QueueInMs() const {
  rtc::CritScope cs(&critsect_);

  int64_t oldest_packet = packets_->OldestEnqueueTimeMs();
  if (oldest_packet == 0)
    return 0;

  return clock_->TimeInMilliseconds() - oldest_packet;
}

int64_t PacedSender::TimeUntilNextProcess() {
  rtc::CritScope cs(&critsect_);
  int64_t elapsed_time_us = clock_->TimeInMicroseconds() - time_last_update_us_;
  int64_t elapsed_time_ms = (elapsed_time_us + 500) / 1000;
  // When paused we wake up every 500 ms to send a padding packet to ensure
  // we won't get stuck in the paused state due to no feedback being received.
  if (paused_)
    return std::max<int64_t>(kPausedPacketIntervalMs - elapsed_time_ms, 0);

  if (prober_->IsProbing()) {
    int64_t ret = prober_->TimeUntilNextProbe(clock_->TimeInMilliseconds());
    if (ret > 0 || (ret == 0 && !probing_send_failure_))
      return ret;
  }
  return std::max<int64_t>(kMinPacketLimitMs - elapsed_time_ms, 0);
}

void PacedSender::Process() {
  int64_t now_us = clock_->TimeInMicroseconds();
  rtc::CritScope cs(&critsect_);
  int64_t elapsed_time_ms = std::min(
      kMaxIntervalTimeMs, (now_us - time_last_update_us_ + 500) / 1000);
  int target_bitrate_kbps = pacing_bitrate_kbps_;

  if (paused_) {
    PacedPacketInfo pacing_info;
    time_last_update_us_ = now_us;
    // We can not send padding unless a normal packet has first been sent. If we
    // do, timestamps get messed up.
    if (packet_counter_ == 0)
      return;
    size_t bytes_sent = SendPadding(1, pacing_info);
    alr_detector_->OnBytesSent(bytes_sent, elapsed_time_ms);
    return;
  }

  if (elapsed_time_ms > 0) {
    size_t queue_size_bytes = packets_->SizeInBytes();
    if (queue_size_bytes > 0) {
      // Assuming equal size packets and input/output rate, the average packet
      // has avg_time_left_ms left to get queue_size_bytes out of the queue, if
      // time constraint shall be met. Determine bitrate needed for that.
      packets_->UpdateQueueTime(clock_->TimeInMilliseconds());
      int64_t avg_time_left_ms = std::max<int64_t>(
          1, queue_time_limit - packets_->AverageQueueTimeMs());
      int min_bitrate_needed_kbps =
          static_cast<int>(queue_size_bytes * 8 / avg_time_left_ms);
      if (min_bitrate_needed_kbps > target_bitrate_kbps)
        target_bitrate_kbps = min_bitrate_needed_kbps;
    }

    media_budget_->set_target_rate_kbps(target_bitrate_kbps);
    UpdateBudgetWithElapsedTime(elapsed_time_ms);
  }

  time_last_update_us_ = now_us;

  bool is_probing = prober_->IsProbing();
  PacedPacketInfo pacing_info;
  size_t bytes_sent = 0;
  size_t recommended_probe_size = 0;
  if (is_probing) {
    pacing_info = prober_->CurrentCluster();
    recommended_probe_size = prober_->RecommendedMinProbeSize();
  }
  while (!packets_->Empty()) {
    // Since we need to release the lock in order to send, we first pop the
    // element from the priority queue but keep it in storage, so that we can
    // reinsert it if send fails.
    const PacketQueue::Packet& packet = packets_->BeginPop();

    if (SendPacket(packet, pacing_info)) {
      // Send succeeded, remove it from the queue.
      if (first_sent_packet_ms_ == -1)
        first_sent_packet_ms_ = clock_->TimeInMilliseconds();
      bytes_sent += packet.bytes;
      packets_->FinalizePop(packet);
      if (is_probing && bytes_sent > recommended_probe_size)
        break;
    } else {
      // Send failed, put it back into the queue.
      packets_->CancelPop(packet);
      break;
    }
  }

  if (packets_->Empty()) {
    // We can not send padding unless a normal packet has first been sent. If we
    // do, timestamps get messed up.
    if (packet_counter_ > 0) {
      int padding_needed =
          static_cast<int>(is_probing ? (recommended_probe_size - bytes_sent)
                                      : padding_budget_->bytes_remaining());
      if (padding_needed > 0)
        bytes_sent += SendPadding(padding_needed, pacing_info);
    }
  }
  if (is_probing) {
    probing_send_failure_ = bytes_sent == 0;
    if (!probing_send_failure_)
      prober_->ProbeSent(clock_->TimeInMilliseconds(), bytes_sent);
  }
  alr_detector_->OnBytesSent(bytes_sent, elapsed_time_ms);
}

void PacedSender::ProcessThreadAttached(ProcessThread* process_thread) {
  RTC_LOG(LS_INFO) << "ProcessThreadAttached 0x" << std::hex << process_thread;
  process_thread_ = process_thread;
}

bool PacedSender::SendPacket(const PacketQueue::Packet& packet,
                             const PacedPacketInfo& pacing_info) {
  RTC_DCHECK(!paused_);
  if (media_budget_->bytes_remaining() == 0 &&
      pacing_info.probe_cluster_id == PacedPacketInfo::kNotAProbe) {
    return false;
  }

  critsect_.Leave();
  const bool success = packet_sender_->TimeToSendPacket(
      packet.ssrc, packet.sequence_number, packet.capture_time_ms,
      packet.retransmission, pacing_info);
  critsect_.Enter();

  if (success) {
    if (packet.priority != kHighPriority || account_for_audio_) {
      // Update media bytes sent.
      // TODO(eladalon): TimeToSendPacket() can also return |true| in some
      // situations where nothing actually ended up being sent to the network,
      // and we probably don't want to update the budget in such cases.
      // https://bugs.chromium.org/p/webrtc/issues/detail?id=8052
      UpdateBudgetWithBytesSent(packet.bytes);
    }
  }

  return success;
}

size_t PacedSender::SendPadding(size_t padding_needed,
                                const PacedPacketInfo& pacing_info) {
  RTC_DCHECK_GT(packet_counter_, 0);
  critsect_.Leave();
  size_t bytes_sent =
      packet_sender_->TimeToSendPadding(padding_needed, pacing_info);
  critsect_.Enter();

  if (bytes_sent > 0) {
    UpdateBudgetWithBytesSent(bytes_sent);
  }
  return bytes_sent;
}

void PacedSender::UpdateBudgetWithElapsedTime(int64_t delta_time_ms) {
  media_budget_->IncreaseBudget(delta_time_ms);
  padding_budget_->IncreaseBudget(delta_time_ms);
}

void PacedSender::UpdateBudgetWithBytesSent(size_t bytes_sent) {
  media_budget_->UseBudget(bytes_sent);
  padding_budget_->UseBudget(bytes_sent);
}

void PacedSender::SetPacingFactor(float pacing_factor) {
  rtc::CritScope cs(&critsect_);
  pacing_factor_ = pacing_factor;
  // Make sure new padding factor is applied immediately, otherwise we need to
  // wait for the send bitrate estimate to be updated before this takes effect.
  SetEstimatedBitrate(estimated_bitrate_bps_);
}

float PacedSender::GetPacingFactor() const {
  rtc::CritScope cs(&critsect_);
  return pacing_factor_;
}

void PacedSender::SetQueueTimeLimit(int limit_ms) {
  rtc::CritScope cs(&critsect_);
  queue_time_limit = limit_ms;
}

}  // namespace webrtc
