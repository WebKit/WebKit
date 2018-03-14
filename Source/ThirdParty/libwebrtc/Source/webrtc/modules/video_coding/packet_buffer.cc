/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/packet_buffer.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

#include "common_video/h264/h264_common.h"
#include "modules/video_coding/frame_object.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"

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
      received_frame_callback_(received_frame_callback),
      sps_pps_idr_is_h264_keyframe_(
          field_trial::IsEnabled("WebRTC-SpsPpsIdrIsH264Keyframe")) {
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

    UpdateMissingPackets(packet->seqNum);

    int64_t now_ms = clock_->TimeInMilliseconds();
    last_received_packet_ms_ = now_ms;
    if (packet->frameType == kVideoFrameKey)
      last_received_keyframe_packet_ms_ = now_ms;

    found_frames = FindFrames(seq_num);
  }

  for (std::unique_ptr<RtpFrameObject>& frame : found_frames)
    received_frame_callback_->OnReceivedFrame(std::move(frame));

  return true;
}

void PacketBuffer::ClearTo(uint16_t seq_num) {
  rtc::CritScope lock(&crit_);
  // We have already cleared past this sequence number, no need to do anything.
  if (is_cleared_to_first_seq_num_ &&
      AheadOf<uint16_t>(first_seq_num_, seq_num)) {
    return;
  }

  // If the packet buffer was cleared between a frame was created and returned.
  if (!first_packet_received_)
    return;

  // Avoid iterating over the buffer more than once by capping the number of
  // iterations to the |size_| of the buffer.
  ++seq_num;
  size_t diff = ForwardDiff<uint16_t>(first_seq_num_, seq_num);
  size_t iterations = std::min(diff, size_);
  for (size_t i = 0; i < iterations; ++i) {
    size_t index = first_seq_num_ % size_;
    RTC_DCHECK_EQ(data_buffer_[index].seqNum, sequence_buffer_[index].seq_num);
    if (AheadOf<uint16_t>(seq_num, sequence_buffer_[index].seq_num)) {
      delete[] data_buffer_[index].dataPtr;
      data_buffer_[index].dataPtr = nullptr;
      sequence_buffer_[index].used = false;
    }
    ++first_seq_num_;
  }

  // If |diff| is larger than |iterations| it means that we don't increment
  // |first_seq_num_| until we reach |seq_num|, so we set it here.
  first_seq_num_ = seq_num;

  is_cleared_to_first_seq_num_ = true;
  auto clear_to_it = missing_packets_.upper_bound(seq_num);
  if (clear_to_it != missing_packets_.begin()) {
    --clear_to_it;
    missing_packets_.erase(missing_packets_.begin(), clear_to_it);
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
  last_received_packet_ms_.reset();
  last_received_keyframe_packet_ms_.reset();
  newest_inserted_seq_num_.reset();
  missing_packets_.clear();
}

void PacketBuffer::PaddingReceived(uint16_t seq_num) {
  std::vector<std::unique_ptr<RtpFrameObject>> found_frames;
  {
    rtc::CritScope lock(&crit_);
    UpdateMissingPackets(seq_num);
    found_frames = FindFrames(static_cast<uint16_t>(seq_num + 1));
  }

  for (std::unique_ptr<RtpFrameObject>& frame : found_frames)
    received_frame_callback_->OnReceivedFrame(std::move(frame));
}

rtc::Optional<int64_t> PacketBuffer::LastReceivedPacketMs() const {
  rtc::CritScope lock(&crit_);
  return last_received_packet_ms_;
}

rtc::Optional<int64_t> PacketBuffer::LastReceivedKeyframePacketMs() const {
  rtc::CritScope lock(&crit_);
  return last_received_keyframe_packet_ms_;
}

bool PacketBuffer::ExpandBufferSize() {
  if (size_ == max_size_) {
    RTC_LOG(LS_WARNING) << "PacketBuffer is already at max size (" << max_size_
                        << "), failed to increase size. Clearing PacketBuffer.";
    Clear();
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
  RTC_LOG(LS_INFO) << "PacketBuffer size expanded to " << new_size;
  return true;
}

bool PacketBuffer::PotentialNewFrame(uint16_t seq_num) const {
  size_t index = seq_num % size_;
  int prev_index = index > 0 ? index - 1 : size_ - 1;

  if (!sequence_buffer_[index].used)
    return false;
  if (sequence_buffer_[index].seq_num != seq_num)
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
  for (size_t i = 0; i < size_ && PotentialNewFrame(seq_num); ++i) {
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
      size_t tested_packets = 0;
      int64_t frame_timestamp = data_buffer_[start_index].timestamp;

      // Identify H.264 keyframes by means of SPS, PPS, and IDR.
      bool is_h264 = data_buffer_[start_index].codec == kVideoCodecH264;
      bool has_h264_sps = false;
      bool has_h264_pps = false;
      bool has_h264_idr = false;
      bool is_h264_keyframe = false;

      while (true) {
        ++tested_packets;
        frame_size += data_buffer_[start_index].sizeBytes;
        max_nack_count =
            std::max(max_nack_count, data_buffer_[start_index].timesNacked);
        sequence_buffer_[start_index].frame_created = true;

        if (!is_h264 && sequence_buffer_[start_index].frame_begin)
          break;

        if (is_h264 && !is_h264_keyframe) {
          const RTPVideoHeaderH264& header =
              data_buffer_[start_index].video_header.codecHeader.H264;
          for (size_t j = 0; j < header.nalus_length; ++j) {
            if (header.nalus[j].type == H264::NaluType::kSps) {
              has_h264_sps = true;
            } else if (header.nalus[j].type == H264::NaluType::kPps) {
              has_h264_pps = true;
            } else if (header.nalus[j].type == H264::NaluType::kIdr) {
              has_h264_idr = true;
            }
          }
          if ((sps_pps_idr_is_h264_keyframe_ && has_h264_idr && has_h264_sps &&
               has_h264_pps) ||
              (!sps_pps_idr_is_h264_keyframe_ && has_h264_idr)) {
            is_h264_keyframe = true;
          }
        }

        if (tested_packets == size_)
          break;

        start_index = start_index > 0 ? start_index - 1 : size_ - 1;

        // In the case of H264 we don't have a frame_begin bit (yes,
        // |frame_begin| might be set to true but that is a lie). So instead
        // we traverese backwards as long as we have a previous packet and
        // the timestamp of that packet is the same as this one. This may cause
        // the PacketBuffer to hand out incomplete frames.
        // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=7106
        if (is_h264 &&
            (!sequence_buffer_[start_index].used ||
             data_buffer_[start_index].timestamp != frame_timestamp)) {
          break;
        }

        --start_seq_num;
      }

      if (is_h264) {
        // Warn if this is an unsafe frame.
        if (has_h264_idr && (!has_h264_sps || !has_h264_pps)) {
          std::stringstream ss;
          ss << "Received H.264-IDR frame "
             << "(SPS: " << has_h264_sps << ", PPS: " << has_h264_pps << "). ";
          if (sps_pps_idr_is_h264_keyframe_) {
            ss << "Treating as delta frame since "
                  "WebRTC-SpsPpsIdrIsH264Keyframe is enabled.";
          } else {
            ss << "Treating as key frame since "
                  "WebRTC-SpsPpsIdrIsH264Keyframe is disabled.";
          }
          RTC_LOG(LS_WARNING) << ss.str();
        }

        // Now that we have decided whether to treat this frame as a key frame
        // or delta frame in the frame buffer, we update the field that
        // determines if the RtpFrameObject is a key frame or delta frame.
        const size_t first_packet_index = start_seq_num % size_;
        RTC_CHECK_LT(first_packet_index, size_);
        if (is_h264_keyframe) {
          data_buffer_[first_packet_index].frameType = kVideoFrameKey;
        } else {
          data_buffer_[first_packet_index].frameType = kVideoFrameDelta;
        }

        // If this is not a keyframe, make sure there are no gaps in the
        // packet sequence numbers up until this point.
        if (!is_h264_keyframe && missing_packets_.upper_bound(start_seq_num) !=
                                     missing_packets_.begin()) {
          uint16_t stop_index = (index + 1) % size_;
          while (start_index != stop_index) {
            sequence_buffer_[start_index].frame_created = false;
            start_index = (start_index + 1) % size_;
          }

          return found_frames;
        }
      }

      missing_packets_.erase(missing_packets_.begin(),
                             missing_packets_.upper_bound(seq_num));

      found_frames.emplace_back(
          new RtpFrameObject(this, start_seq_num, seq_num, frame_size,
                             max_nack_count, clock_->TimeInMilliseconds()));
    }
    ++seq_num;
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
  uint8_t* destination_end = destination + frame.size();

  do {
    if (!sequence_buffer_[index].used ||
        sequence_buffer_[index].seq_num != seq_num) {
      return false;
    }

    RTC_DCHECK_EQ(data_buffer_[index].seqNum, sequence_buffer_[index].seq_num);
    size_t length = data_buffer_[index].sizeBytes;
    if (destination + length > destination_end) {
      RTC_LOG(LS_WARNING) << "Frame (" << frame.picture_id << ":"
                          << static_cast<int>(frame.spatial_layer) << ")"
                          << " bitstream buffer is not large enough.";
      return false;
    }

    const uint8_t* source = data_buffer_[index].dataPtr;
    memcpy(destination, source, length);
    destination += length;
    index = (index + 1) % size_;
    ++seq_num;
  } while (index != end);

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

void PacketBuffer::UpdateMissingPackets(uint16_t seq_num) {
  if (!newest_inserted_seq_num_)
    newest_inserted_seq_num_ = seq_num;

  const int kMaxPaddingAge = 1000;
  if (AheadOf(seq_num, *newest_inserted_seq_num_)) {
    uint16_t old_seq_num = seq_num - kMaxPaddingAge;
    auto erase_to = missing_packets_.lower_bound(old_seq_num);
    missing_packets_.erase(missing_packets_.begin(), erase_to);

    // Guard against inserting a large amount of missing packets if there is a
    // jump in the sequence number.
    if (AheadOf(old_seq_num, *newest_inserted_seq_num_))
      *newest_inserted_seq_num_ = old_seq_num;

    ++*newest_inserted_seq_num_;
    while (AheadOf(seq_num, *newest_inserted_seq_num_)) {
      missing_packets_.insert(*newest_inserted_seq_num_);
      ++*newest_inserted_seq_num_;
    }
  } else {
    missing_packets_.erase(seq_num);
  }
}

}  // namespace video_coding
}  // namespace webrtc
