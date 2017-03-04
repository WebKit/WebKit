/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/packet_buffer.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
namespace video_coding {

rtc::scoped_refptr<PacketBuffer> PacketBuffer::Create(
    Clock* clock,
    size_t start_buffer_size,
    size_t max_buffer_size,
    OnReceivedFrameCallback* received_frame_callback) {
  return rtc::scoped_refptr<PacketBuffer>(new PacketBuffer(
      clock, start_buffer_size, max_buffer_size, received_frame_callback));
}

PacketBuffer::PacketBuffer(Clock* clock,
                           size_t start_buffer_size,
                           size_t max_buffer_size,
                           OnReceivedFrameCallback* received_frame_callback)
    : clock_(clock),
      size_(start_buffer_size),
      max_size_(max_buffer_size),
      first_seq_num_(0),
      first_packet_received_(false),
      is_cleared_to_first_seq_num_(false),
      data_buffer_(start_buffer_size),
      sequence_buffer_(start_buffer_size),
      received_frame_callback_(received_frame_callback) {
  RTC_DCHECK_LE(start_buffer_size, max_buffer_size);
  // Buffer size must always be a power of 2.
  RTC_DCHECK((start_buffer_size & (start_buffer_size - 1)) == 0);
  RTC_DCHECK((max_buffer_size & (max_buffer_size - 1)) == 0);
}

PacketBuffer::~PacketBuffer() {
  Clear();
}

bool PacketBuffer::InsertPacket(VCMPacket* packet) {
  std::vector<std::unique_ptr<RtpFrameObject>> found_frames;
  {
    rtc::CritScope lock(&crit_);
    uint16_t seq_num = packet->seqNum;
    size_t index = seq_num % size_;

    if (!first_packet_received_) {
      first_seq_num_ = seq_num;
      first_packet_received_ = true;
    } else if (AheadOf(first_seq_num_, seq_num)) {
      // If we have explicitly cleared past this packet then it's old,
      // don't insert it.
      if (is_cleared_to_first_seq_num_) {
        delete[] packet->dataPtr;
        packet->dataPtr = nullptr;
        return false;
      }

      first_seq_num_ = seq_num;
    }

    if (sequence_buffer_[index].used) {
      // Duplicate packet, just delete the payload.
      if (data_buffer_[index].seqNum == packet->seqNum) {
        delete[] packet->dataPtr;
        packet->dataPtr = nullptr;
        return true;
      }

      // The packet buffer is full, try to expand the buffer.
      while (ExpandBufferSize() && sequence_buffer_[seq_num % size_].used) {
      }
      index = seq_num % size_;

      // Packet buffer is still full.
      if (sequence_buffer_[index].used) {
        delete[] packet->dataPtr;
        packet->dataPtr = nullptr;
        return false;
      }
    }

    sequence_buffer_[index].frame_begin = packet->is_first_packet_in_frame;
    sequence_buffer_[index].frame_end = packet->markerBit;
    sequence_buffer_[index].seq_num = packet->seqNum;
    sequence_buffer_[index].continuous = false;
    sequence_buffer_[index].frame_created = false;
    sequence_buffer_[index].used = true;
    data_buffer_[index] = *packet;
    packet->dataPtr = nullptr;

    found_frames = FindFrames(seq_num);
  }

  for (std::unique_ptr<RtpFrameObject>& frame : found_frames)
    received_frame_callback_->OnReceivedFrame(std::move(frame));

  return true;
}

void PacketBuffer::ClearTo(uint16_t seq_num) {
  rtc::CritScope lock(&crit_);

  // If the packet buffer was cleared between a frame was created and returned.
  if (!first_packet_received_)
    return;

  is_cleared_to_first_seq_num_ = true;
  while (AheadOrAt<uint16_t>(seq_num, first_seq_num_)) {
    size_t index = first_seq_num_ % size_;
    delete[] data_buffer_[index].dataPtr;
    data_buffer_[index].dataPtr = nullptr;
    sequence_buffer_[index].used = false;
    ++first_seq_num_;
  }
}

void PacketBuffer::Clear() {
  rtc::CritScope lock(&crit_);
  for (size_t i = 0; i < size_; ++i) {
    delete[] data_buffer_[i].dataPtr;
    data_buffer_[i].dataPtr = nullptr;
    sequence_buffer_[i].used = false;
  }

  first_packet_received_ = false;
  is_cleared_to_first_seq_num_ = false;
}

bool PacketBuffer::ExpandBufferSize() {
  if (size_ == max_size_) {
    LOG(LS_WARNING) << "PacketBuffer is already at max size (" << max_size_
                    << "), failed to increase size.";
    return false;
  }

  size_t new_size = std::min(max_size_, 2 * size_);
  std::vector<VCMPacket> new_data_buffer(new_size);
  std::vector<ContinuityInfo> new_sequence_buffer(new_size);
  for (size_t i = 0; i < size_; ++i) {
    if (sequence_buffer_[i].used) {
      size_t index = sequence_buffer_[i].seq_num % new_size;
      new_sequence_buffer[index] = sequence_buffer_[i];
      new_data_buffer[index] = data_buffer_[i];
    }
  }
  size_ = new_size;
  sequence_buffer_ = std::move(new_sequence_buffer);
  data_buffer_ = std::move(new_data_buffer);
  LOG(LS_INFO) << "PacketBuffer size expanded to " << new_size;
  return true;
}

bool PacketBuffer::PotentialNewFrame(uint16_t seq_num) const {
  size_t index = seq_num % size_;
  int prev_index = index > 0 ? index - 1 : size_ - 1;

  if (!sequence_buffer_[index].used)
    return false;
  if (sequence_buffer_[index].frame_created)
    return false;
  if (sequence_buffer_[index].frame_begin)
    return true;
  if (!sequence_buffer_[prev_index].used)
    return false;
  if (sequence_buffer_[prev_index].frame_created)
    return false;
  if (sequence_buffer_[prev_index].seq_num !=
      static_cast<uint16_t>(sequence_buffer_[index].seq_num - 1)) {
    return false;
  }
  if (sequence_buffer_[prev_index].continuous)
    return true;

  return false;
}

std::vector<std::unique_ptr<RtpFrameObject>> PacketBuffer::FindFrames(
    uint16_t seq_num) {
  std::vector<std::unique_ptr<RtpFrameObject>> found_frames;
  size_t packets_tested = 0;
  while (packets_tested < size_ && PotentialNewFrame(seq_num)) {
    size_t index = seq_num % size_;
    sequence_buffer_[index].continuous = true;

    // If all packets of the frame is continuous, find the first packet of the
    // frame and create an RtpFrameObject.
    if (sequence_buffer_[index].frame_end) {
      size_t frame_size = 0;
      int max_nack_count = -1;
      uint16_t start_seq_num = seq_num;

      // Find the start index by searching backward until the packet with
      // the |frame_begin| flag is set.
      int start_index = index;

      bool is_h264 = data_buffer_[start_index].codec == kVideoCodecH264;
      int64_t frame_timestamp = data_buffer_[start_index].timestamp;
      while (true) {
        frame_size += data_buffer_[start_index].sizeBytes;
        max_nack_count =
            std::max(max_nack_count, data_buffer_[start_index].timesNacked);
        sequence_buffer_[start_index].frame_created = true;

        if (!is_h264 && sequence_buffer_[start_index].frame_begin)
          break;

        start_index = start_index > 0 ? start_index - 1 : size_ - 1;

        // In the case of H264 we don't have a frame_begin bit (yes,
        // |frame_begin| might be set to true but that is a lie). So instead
        // we traverese backwards as long as we have a previous packet and
        // the timestamp of that packet is the same as this one. This may cause
        // the PacketBuffer to hand out incomplete frames.
        // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=7106
        //
        // Since we ignore the |frame_begin| flag of the inserted packets
        // we check that |start_index != static_cast<int>(index)| to make sure
        // that we don't get stuck in a loop if the packet buffer is filled
        // with packets of the same timestamp.
        if (is_h264 && start_index != static_cast<int>(index) &&
            (!sequence_buffer_[start_index].used ||
             data_buffer_[start_index].timestamp != frame_timestamp)) {
          break;
        }

        --start_seq_num;
      }

      found_frames.emplace_back(
          new RtpFrameObject(this, start_seq_num, seq_num, frame_size,
                             max_nack_count, clock_->TimeInMilliseconds()));
    }
    ++seq_num;
    ++packets_tested;
  }
  return found_frames;
}

void PacketBuffer::ReturnFrame(RtpFrameObject* frame) {
  rtc::CritScope lock(&crit_);
  size_t index = frame->first_seq_num() % size_;
  size_t end = (frame->last_seq_num() + 1) % size_;
  uint16_t seq_num = frame->first_seq_num();
  while (index != end) {
    if (sequence_buffer_[index].seq_num == seq_num) {
      delete[] data_buffer_[index].dataPtr;
      data_buffer_[index].dataPtr = nullptr;
      sequence_buffer_[index].used = false;
    }

    index = (index + 1) % size_;
    ++seq_num;
  }
}

bool PacketBuffer::GetBitstream(const RtpFrameObject& frame,
                                uint8_t* destination) {
  rtc::CritScope lock(&crit_);

  size_t index = frame.first_seq_num() % size_;
  size_t end = (frame.last_seq_num() + 1) % size_;
  uint16_t seq_num = frame.first_seq_num();
  while (index != end) {
    if (!sequence_buffer_[index].used ||
        sequence_buffer_[index].seq_num != seq_num) {
      return false;
    }

    const uint8_t* source = data_buffer_[index].dataPtr;
    size_t length = data_buffer_[index].sizeBytes;
    memcpy(destination, source, length);
    destination += length;
    index = (index + 1) % size_;
    ++seq_num;
  }
  return true;
}

VCMPacket* PacketBuffer::GetPacket(uint16_t seq_num) {
  size_t index = seq_num % size_;
  if (!sequence_buffer_[index].used ||
      seq_num != sequence_buffer_[index].seq_num) {
    return nullptr;
  }
  return &data_buffer_[index];
}

int PacketBuffer::AddRef() const {
  return rtc::AtomicOps::Increment(&ref_count_);
}

int PacketBuffer::Release() const {
  int count = rtc::AtomicOps::Decrement(&ref_count_);
  if (!count) {
    delete this;
  }
  return count;
}

}  // namespace video_coding
}  // namespace webrtc
