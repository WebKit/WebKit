/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <limits>

#include "webrtc/modules/video_coding/nack_module.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/utility/include/process_thread.h"

namespace webrtc {

namespace {
const int kMaxPacketAge = 10000;
const int kMaxNackPackets = 1000;
const int kDefaultRttMs = 100;
const int kMaxNackRetries = 10;
const int kProcessFrequency = 50;
const int kProcessIntervalMs = 1000 / kProcessFrequency;
const int kMaxReorderedPackets = 128;
const int kNumReorderingBuckets = 10;
}  // namespace

NackModule::NackInfo::NackInfo()
    : seq_num(0), send_at_seq_num(0), sent_at_time(-1), retries(0) {}

NackModule::NackInfo::NackInfo(uint16_t seq_num, uint16_t send_at_seq_num)
    : seq_num(seq_num),
      send_at_seq_num(send_at_seq_num),
      sent_at_time(-1),
      retries(0) {}

NackModule::NackModule(Clock* clock,
                       NackSender* nack_sender,
                       KeyFrameRequestSender* keyframe_request_sender)
    : clock_(clock),
      nack_sender_(nack_sender),
      keyframe_request_sender_(keyframe_request_sender),
      reordering_histogram_(kNumReorderingBuckets, kMaxReorderedPackets),
      running_(true),
      initialized_(false),
      rtt_ms_(kDefaultRttMs),
      newest_seq_num_(0),
      next_process_time_ms_(-1) {
  RTC_DCHECK(clock_);
  RTC_DCHECK(nack_sender_);
  RTC_DCHECK(keyframe_request_sender_);
}

int NackModule::OnReceivedPacket(const VCMPacket& packet) {
  rtc::CritScope lock(&crit_);
  if (!running_)
    return -1;
  uint16_t seq_num = packet.seqNum;
  // TODO(philipel): When the packet includes information whether it is
  //                 retransmitted or not, use that value instead. For
  //                 now set it to true, which will cause the reordering
  //                 statistics to never be updated.
  bool is_retransmitted = true;
  bool is_keyframe = packet.isFirstPacket && packet.frameType == kVideoFrameKey;

  if (!initialized_) {
    newest_seq_num_ = seq_num;
    if (is_keyframe)
      keyframe_list_.insert(seq_num);
    initialized_ = true;
    return 0;
  }

  // Since the |newest_seq_num_| is a packet we have actually received we know
  // that packet has never been Nacked.
  if (seq_num == newest_seq_num_)
    return 0;

  if (AheadOf(newest_seq_num_, seq_num)) {
    // An out of order packet has been received.
    auto nack_list_it = nack_list_.find(seq_num);
    int nacks_sent_for_packet = 0;
    if (nack_list_it != nack_list_.end()) {
      nacks_sent_for_packet = nack_list_it->second.retries;
      nack_list_.erase(nack_list_it);
    }
    if (!is_retransmitted)
      UpdateReorderingStatistics(seq_num);
    return nacks_sent_for_packet;
  }
  AddPacketsToNack(newest_seq_num_ + 1, seq_num);
  newest_seq_num_ = seq_num;

  // Keep track of new keyframes.
  if (is_keyframe)
    keyframe_list_.insert(seq_num);

  // And remove old ones so we don't accumulate keyframes.
  auto it = keyframe_list_.lower_bound(seq_num - kMaxPacketAge);
  if (it != keyframe_list_.begin())
    keyframe_list_.erase(keyframe_list_.begin(), it);

  // Are there any nacks that are waiting for this seq_num.
  std::vector<uint16_t> nack_batch = GetNackBatch(kSeqNumOnly);
  if (!nack_batch.empty())
    nack_sender_->SendNack(nack_batch);

  return 0;
}

void NackModule::ClearUpTo(uint16_t seq_num) {
  rtc::CritScope lock(&crit_);
  nack_list_.erase(nack_list_.begin(), nack_list_.lower_bound(seq_num));
  keyframe_list_.erase(keyframe_list_.begin(),
                       keyframe_list_.lower_bound(seq_num));
}

void NackModule::UpdateRtt(int64_t rtt_ms) {
  rtc::CritScope lock(&crit_);
  rtt_ms_ = rtt_ms;
}

void NackModule::Clear() {
  rtc::CritScope lock(&crit_);
  nack_list_.clear();
  keyframe_list_.clear();
}

void NackModule::Stop() {
  rtc::CritScope lock(&crit_);
  running_ = false;
}

int64_t NackModule::TimeUntilNextProcess() {
  rtc::CritScope lock(&crit_);
  return std::max<int64_t>(next_process_time_ms_ - clock_->TimeInMilliseconds(),
                           0);
}

void NackModule::Process() {
  rtc::CritScope lock(&crit_);
  if (!running_)
    return;

  // Update the next_process_time_ms_ in intervals to achieve
  // the targeted frequency over time. Also add multiple intervals
  // in case of a skip in time as to not make uneccessary
  // calls to Process in order to catch up.
  int64_t now_ms = clock_->TimeInMilliseconds();
  if (next_process_time_ms_ == -1) {
    next_process_time_ms_ = now_ms + kProcessIntervalMs;
  } else {
    next_process_time_ms_ = next_process_time_ms_ + kProcessIntervalMs +
                            (now_ms - next_process_time_ms_) /
                                kProcessIntervalMs * kProcessIntervalMs;
  }

  std::vector<uint16_t> nack_batch = GetNackBatch(kTimeOnly);
  if (!nack_batch.empty() && nack_sender_ != nullptr)
    nack_sender_->SendNack(nack_batch);
}

bool NackModule::RemovePacketsUntilKeyFrame() {
  while (!keyframe_list_.empty()) {
    auto it = nack_list_.lower_bound(*keyframe_list_.begin());

    if (it != nack_list_.begin()) {
      // We have found a keyframe that actually is newer than at least one
      // packet in the nack list.
      RTC_DCHECK(it != nack_list_.end());
      nack_list_.erase(nack_list_.begin(), it);
      return true;
    }

    // If this keyframe is so old it does not remove any packets from the list,
    // remove it from the list of keyframes and try the next keyframe.
    keyframe_list_.erase(keyframe_list_.begin());
  }
  return false;
}

void NackModule::AddPacketsToNack(uint16_t seq_num_start,
                                  uint16_t seq_num_end) {
  // Remove old packets.
  auto it = nack_list_.lower_bound(seq_num_end - kMaxPacketAge);
  nack_list_.erase(nack_list_.begin(), it);

  // If the nack list is too large, remove packets from the nack list until
  // the latest first packet of a keyframe. If the list is still too large,
  // clear it and request a keyframe.
  uint16_t num_new_nacks = ForwardDiff(seq_num_start, seq_num_end);
  if (nack_list_.size() + num_new_nacks > kMaxNackPackets) {
    while (RemovePacketsUntilKeyFrame() &&
           nack_list_.size() + num_new_nacks > kMaxNackPackets) {
    }

    if (nack_list_.size() + num_new_nacks > kMaxNackPackets) {
      nack_list_.clear();
      LOG(LS_WARNING) << "NACK list full, clearing NACK"
                         " list and requesting keyframe.";
      keyframe_request_sender_->RequestKeyFrame();
      return;
    }
  }

  for (uint16_t seq_num = seq_num_start; seq_num != seq_num_end; ++seq_num) {
    NackInfo nack_info(seq_num, seq_num + WaitNumberOfPackets(0.5));
    RTC_DCHECK(nack_list_.find(seq_num) == nack_list_.end());
    nack_list_[seq_num] = nack_info;
  }
}

std::vector<uint16_t> NackModule::GetNackBatch(NackFilterOptions options) {
  bool consider_seq_num = options != kTimeOnly;
  bool consider_timestamp = options != kSeqNumOnly;
  int64_t now_ms = clock_->TimeInMilliseconds();
  std::vector<uint16_t> nack_batch;
  auto it = nack_list_.begin();
  while (it != nack_list_.end()) {
    if (consider_seq_num && it->second.sent_at_time == -1 &&
        AheadOrAt(newest_seq_num_, it->second.send_at_seq_num)) {
      nack_batch.emplace_back(it->second.seq_num);
      ++it->second.retries;
      it->second.sent_at_time = now_ms;
      if (it->second.retries >= kMaxNackRetries) {
        LOG(LS_WARNING) << "Sequence number " << it->second.seq_num
                        << " removed from NACK list due to max retries.";
        it = nack_list_.erase(it);
      } else {
        ++it;
      }
      continue;
    }

    if (consider_timestamp && it->second.sent_at_time + rtt_ms_ <= now_ms) {
      nack_batch.emplace_back(it->second.seq_num);
      ++it->second.retries;
      it->second.sent_at_time = now_ms;
      if (it->second.retries >= kMaxNackRetries) {
        LOG(LS_WARNING) << "Sequence number " << it->second.seq_num
                        << " removed from NACK list due to max retries.";
        it = nack_list_.erase(it);
      } else {
        ++it;
      }
      continue;
    }
    ++it;
  }
  return nack_batch;
}

void NackModule::UpdateReorderingStatistics(uint16_t seq_num) {
  RTC_DCHECK(AheadOf(newest_seq_num_, seq_num));
  uint16_t diff = ReverseDiff(newest_seq_num_, seq_num);
  reordering_histogram_.Add(diff);
}

int NackModule::WaitNumberOfPackets(float probability) const {
  if (reordering_histogram_.NumValues() == 0)
    return 0;
  return reordering_histogram_.InverseCdf(probability);
}

}  // namespace webrtc
