/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This is the implementation of the PacketBuffer class. It is mostly based on
// an STL list. The list is kept sorted at all times so that the next packet to
// decode is at the beginning of the list.

#include "modules/audio_coding/neteq/packet_buffer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "api/audio_codecs/audio_decoder.h"
#include "api/field_trials_view.h"
#include "api/neteq/tick_timer.h"
#include "modules/audio_coding/neteq/decoder_database.h"
#include "modules/audio_coding/neteq/packet.h"
#include "modules/audio_coding/neteq/statistics_calculator.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/struct_parameters_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace {
// Predicate used when inserting packets in the buffer list.
// Operator() returns true when `packet` goes before `new_packet`.
class NewTimestampIsLarger {
 public:
  explicit NewTimestampIsLarger(const Packet& new_packet)
      : new_packet_(new_packet) {}
  bool operator()(const Packet& packet) { return (new_packet_ >= packet); }

 private:
  const Packet& new_packet_;
};

std::optional<SmartFlushingConfig> GetSmartflushingConfig(
    const FieldTrialsView& field_trials) {
  std::string field_trial_string =
      field_trials.Lookup("WebRTC-Audio-NetEqSmartFlushing");
  SmartFlushingConfig config;
  bool enabled = false;
  auto parser = StructParametersParser::Create(
      "enabled", &enabled, "target_level_threshold_ms",
      &config.target_level_threshold_ms, "target_level_multiplier",
      &config.target_level_multiplier);
  parser->Parse(field_trial_string);
  if (!enabled) {
    return std::nullopt;
  }
  RTC_LOG(LS_INFO) << "Using smart flushing, target_level_threshold_ms: "
                   << config.target_level_threshold_ms
                   << ", target_level_multiplier: "
                   << config.target_level_multiplier;
  return config;
}

}  // namespace

PacketBuffer::PacketBuffer(const FieldTrialsView& field_trials,
                           size_t max_number_of_packets,
                           const TickTimer* tick_timer,
                           StatisticsCalculator* stats)
    : smart_flushing_config_(GetSmartflushingConfig(field_trials)),
      max_number_of_packets_(max_number_of_packets),
      tick_timer_(tick_timer),
      stats_(stats) {}

// Destructor. All packets in the buffer will be destroyed.
PacketBuffer::~PacketBuffer() {
  buffer_.clear();
}

void PacketBuffer::SetMaxNumberOfPackets(size_t max_number_of_packets) {
  max_number_of_packets_ = max_number_of_packets;
  while (buffer_.size() > max_number_of_packets_) {
    DiscardNextPacket();
  }
}

// Flush the buffer. All packets in the buffer will be destroyed.
void PacketBuffer::Flush() {
  for (auto& p : buffer_) {
    LogPacketDiscarded(p.priority.codec_level);
  }
  buffer_.clear();
  stats_->FlushedPacketBuffer();
}

void PacketBuffer::PartialFlush(int target_level_ms,
                                size_t sample_rate,
                                size_t last_decoded_length) {
  // Make sure that at least half the packet buffer capacity will be available
  // after the flush. This is done to avoid getting stuck if the target level is
  // very high.
  int target_level_samples = std::min(
      target_level_ms * static_cast<int>(sample_rate) / 1000,
      static_cast<int>(max_number_of_packets_ * last_decoded_length / 2));
  // We should avoid flushing to very low levels.
  if (smart_flushing_config_.has_value()) {
    target_level_samples =
        std::max(target_level_samples,
                 smart_flushing_config_->target_level_threshold_ms *
                     static_cast<int>(sample_rate) / 1000);
  }
  while (!buffer_.empty() &&
         (GetSpanSamples(last_decoded_length, sample_rate, false) >
              static_cast<size_t>(target_level_samples) ||
          buffer_.size() > max_number_of_packets_ / 2)) {
    LogPacketDiscarded(PeekNextPacket()->priority.codec_level);
    buffer_.pop_front();
  }
}

bool PacketBuffer::Empty() const {
  return buffer_.empty();
}

int PacketBuffer::InsertPacket(Packet&& packet,
                               size_t last_decoded_length,
                               size_t sample_rate,
                               int target_level_ms) {
  if (packet.empty()) {
    RTC_LOG(LS_WARNING) << "InsertPacket invalid packet";
    return kInvalidPacket;
  }

  RTC_DCHECK_GE(packet.priority.codec_level, 0);
  RTC_DCHECK_GE(packet.priority.red_level, 0);

  int return_val = kOK;

  packet.waiting_time = tick_timer_->GetNewStopwatch();

  // Perform a smart flush if the buffer size exceeds a multiple of the target
  // level.
  const size_t span_threshold =
      smart_flushing_config_
          ? static_cast<size_t>(
                smart_flushing_config_->target_level_multiplier *
                std::max(smart_flushing_config_->target_level_threshold_ms,
                         target_level_ms) *
                static_cast<int>(sample_rate) / 1000)
          : 0;
  const bool smart_flush =
      smart_flushing_config_.has_value() &&
      GetSpanSamples(last_decoded_length, sample_rate, false) >= span_threshold;
  if (buffer_.size() >= max_number_of_packets_ || smart_flush) {
    size_t buffer_size_before_flush = buffer_.size();
    if (smart_flushing_config_.has_value()) {
      // Flush down to the target level.
      PartialFlush(target_level_ms, sample_rate, last_decoded_length);
      return_val = kPartialFlush;
      size_t discarded = buffer_size_before_flush - buffer_.size();
      if (discarded > 0) {
        RTC_LOG(LS_VERBOSE) << "Partial flushed " << discarded
                            << " packets to reach target level.";
      }
    } else {
      // Buffer is full.
      Flush();
      return_val = kFlushed;
      RTC_LOG(LS_WARNING) << "Packet buffer fully flushed.";
    }
  }

  // Get an iterator pointing to the place in the buffer where the new packet
  // should be inserted. The list is searched from the back, since the most
  // likely case is that the new packet should be near the end of the list.
  PacketList::reverse_iterator rit = std::find_if(
      buffer_.rbegin(), buffer_.rend(), NewTimestampIsLarger(packet));

  // The new packet is to be inserted to the right of `rit`. If it has the same
  // timestamp as `rit`, which has a higher priority, do not insert the new
  // packet to list.
  if (rit != buffer_.rend() && packet.timestamp == rit->timestamp) {
    LogPacketDiscarded(packet.priority.codec_level);
    return return_val;
  }

  // The new packet is to be inserted to the left of `it`. If it has the same
  // timestamp as `it`, which has a lower priority, replace `it` with the new
  // packet.
  PacketList::iterator it = rit.base();
  if (it != buffer_.end() && packet.timestamp == it->timestamp) {
    LogPacketDiscarded(it->priority.codec_level);
    it = buffer_.erase(it);
  }
  buffer_.insert(it, std::move(packet));  // Insert the packet at that position.

  return return_val;
}

int PacketBuffer::NextTimestamp(uint32_t* next_timestamp) const {
  if (Empty()) {
    return kBufferEmpty;
  }
  if (!next_timestamp) {
    return kInvalidPointer;
  }
  *next_timestamp = buffer_.front().timestamp;
  return kOK;
}

int PacketBuffer::NextHigherTimestamp(uint32_t timestamp,
                                      uint32_t* next_timestamp) const {
  if (Empty()) {
    return kBufferEmpty;
  }
  if (!next_timestamp) {
    return kInvalidPointer;
  }
  PacketList::const_iterator it;
  for (it = buffer_.begin(); it != buffer_.end(); ++it) {
    if (it->timestamp >= timestamp) {
      // Found a packet matching the search.
      *next_timestamp = it->timestamp;
      return kOK;
    }
  }
  return kNotFound;
}

const Packet* PacketBuffer::PeekNextPacket() const {
  return buffer_.empty() ? nullptr : &buffer_.front();
}

std::optional<Packet> PacketBuffer::GetNextPacket() {
  if (Empty()) {
    // Buffer is empty.
    return std::nullopt;
  }

  std::optional<Packet> packet(std::move(buffer_.front()));
  // Assert that the packet sanity checks in InsertPacket method works.
  RTC_DCHECK(!packet->empty());
  buffer_.pop_front();

  return packet;
}

int PacketBuffer::DiscardNextPacket() {
  if (Empty()) {
    return kBufferEmpty;
  }
  // Assert that the packet sanity checks in InsertPacket method works.
  const Packet& packet = buffer_.front();
  RTC_DCHECK(!packet.empty());
  LogPacketDiscarded(packet.priority.codec_level);
  buffer_.pop_front();
  return kOK;
}

void PacketBuffer::DiscardOldPackets(uint32_t timestamp_limit,
                                     uint32_t horizon_samples) {
  buffer_.remove_if([this, timestamp_limit, horizon_samples](const Packet& p) {
    if (timestamp_limit == p.timestamp ||
        !IsObsoleteTimestamp(p.timestamp, timestamp_limit, horizon_samples)) {
      return false;
    }
    LogPacketDiscarded(p.priority.codec_level);
    return true;
  });
}

void PacketBuffer::DiscardAllOldPackets(uint32_t timestamp_limit) {
  DiscardOldPackets(timestamp_limit, 0);
}

void PacketBuffer::DiscardPacketsWithPayloadType(uint8_t payload_type) {
  buffer_.remove_if([this, payload_type](const Packet& p) {
    if (p.payload_type != payload_type) {
      return false;
    }
    LogPacketDiscarded(p.priority.codec_level);
    return true;
  });
}

size_t PacketBuffer::NumPacketsInBuffer() const {
  return buffer_.size();
}

size_t PacketBuffer::NumSamplesInBuffer(size_t last_decoded_length) const {
  size_t num_samples = 0;
  size_t last_duration = last_decoded_length;
  for (const Packet& packet : buffer_) {
    if (packet.frame) {
      // TODO(hlundin): Verify that it's fine to count all packets and remove
      // this check.
      if (packet.priority != Packet::Priority(0, 0)) {
        continue;
      }
      size_t duration = packet.frame->Duration();
      if (duration > 0) {
        last_duration = duration;  // Save the most up-to-date (valid) duration.
      }
    }
    num_samples += last_duration;
  }
  return num_samples;
}

size_t PacketBuffer::GetSpanSamples(size_t last_decoded_length,
                                    size_t sample_rate,
                                    bool count_waiting_time) const {
  if (buffer_.empty()) {
    return 0;
  }

  size_t span = buffer_.back().timestamp - buffer_.front().timestamp;
  size_t waiting_time_samples = dchecked_cast<size_t>(
      buffer_.back().waiting_time->ElapsedMs() * (sample_rate / 1000));
  if (count_waiting_time) {
    span += waiting_time_samples;
  } else if (buffer_.back().frame && buffer_.back().frame->Duration() > 0) {
    size_t duration = buffer_.back().frame->Duration();
    if (buffer_.back().frame->IsDtxPacket()) {
      duration = std::max(duration, waiting_time_samples);
    }
    span += duration;
  } else {
    span += last_decoded_length;
  }
  return span;
}

bool PacketBuffer::ContainsDtxOrCngPacket(
    const DecoderDatabase* decoder_database) const {
  RTC_DCHECK(decoder_database);
  for (const Packet& packet : buffer_) {
    if ((packet.frame && packet.frame->IsDtxPacket()) ||
        decoder_database->IsComfortNoise(packet.payload_type)) {
      return true;
    }
  }
  return false;
}

void PacketBuffer::LogPacketDiscarded(int codec_level) {
  if (codec_level > 0) {
    stats_->SecondaryPacketsDiscarded(1);
  } else {
    stats_->PacketsDiscarded(1);
  }
}

}  // namespace webrtc
