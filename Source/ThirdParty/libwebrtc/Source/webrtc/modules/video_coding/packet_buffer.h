/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_PACKET_BUFFER_H_
#define MODULES_VIDEO_CODING_PACKET_BUFFER_H_

#include <memory>
#include <queue>
#include <set>
#include <vector>

#include "absl/base/attributes.h"
#include "api/rtp_packet_info.h"
#include "api/video/encoded_image.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/frame_object.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace video_coding {

class PacketBuffer {
 public:
  struct Packet {
    Packet() = default;
    Packet(const RtpPacketReceived& rtp_packet,
           const RTPVideoHeader& video_header,
           int64_t ntp_time_ms,
           int64_t receive_time_ms);
    Packet(const Packet&) = delete;
    Packet(Packet&&) = delete;
    Packet& operator=(const Packet&) = delete;
    Packet& operator=(Packet&&) = delete;
    ~Packet() = default;

    VideoCodecType codec() const { return video_header.codec; }
    int width() const { return video_header.width; }
    int height() const { return video_header.height; }

    bool is_first_packet_in_frame() const {
      return video_header.is_first_packet_in_frame;
    }
    bool is_last_packet_in_frame() const {
      return video_header.is_last_packet_in_frame;
    }

    bool marker_bit = false;
    uint8_t payload_type = 0;
    uint16_t seq_num = 0;
    uint32_t timestamp = 0;
    // NTP time of the capture time in local timebase in milliseconds.
    int64_t ntp_time_ms = -1;
    int times_nacked = -1;

    rtc::CopyOnWriteBuffer video_payload;
    RTPVideoHeader video_header;

    RtpPacketInfo packet_info;
  };
  struct InsertResult {
    std::vector<std::unique_ptr<RtpFrameObject>> frames;
    // Indicates if the packet buffer was cleared, which means that a key
    // frame request should be sent.
    bool buffer_cleared = false;
  };

  // Both |start_buffer_size| and |max_buffer_size| must be a power of 2.
  PacketBuffer(Clock* clock, size_t start_buffer_size, size_t max_buffer_size);
  ~PacketBuffer();

  InsertResult InsertPacket(std::unique_ptr<Packet> packet)
      ABSL_MUST_USE_RESULT;
  InsertResult InsertPadding(uint16_t seq_num) ABSL_MUST_USE_RESULT;
  void ClearTo(uint16_t seq_num);
  void Clear();

  // Timestamp (not RTP timestamp) of the last received packet/keyframe packet.
  absl::optional<int64_t> LastReceivedPacketMs() const;
  absl::optional<int64_t> LastReceivedKeyframePacketMs() const;

 private:
  struct StoredPacket {
    uint16_t seq_num() const { return packet->seq_num; }

    // If this is the first packet of the frame.
    bool frame_begin() const { return packet->is_first_packet_in_frame(); }

    // If this is the last packet of the frame.
    bool frame_end() const { return packet->is_last_packet_in_frame(); }

    // If this slot is currently used.
    bool used() const { return packet != nullptr; }

    // If all its previous packets have been inserted into the packet buffer.
    bool continuous = false;

    std::unique_ptr<Packet> packet;
  };

  Clock* const clock_;

  // Tries to expand the buffer.
  bool ExpandBufferSize() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Test if all previous packets has arrived for the given sequence number.
  bool PotentialNewFrame(uint16_t seq_num) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Test if all packets of a frame has arrived, and if so, creates a frame.
  // Returns a vector of received frames.
  std::vector<std::unique_ptr<RtpFrameObject>> FindFrames(uint16_t seq_num)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  std::unique_ptr<RtpFrameObject> AssembleFrame(uint16_t first_seq_num,
                                                uint16_t last_seq_num)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Get the packet with sequence number |seq_num|.
  const Packet& GetPacket(uint16_t seq_num) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Clears the packet buffer from |start_seq_num| to |stop_seq_num| where the
  // endpoints are inclusive.
  void ClearInterval(uint16_t start_seq_num, uint16_t stop_seq_num)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  void UpdateMissingPackets(uint16_t seq_num)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  rtc::CriticalSection crit_;

  // buffer_.size() and max_size_ must always be a power of two.
  const size_t max_size_;

  // The fist sequence number currently in the buffer.
  uint16_t first_seq_num_ RTC_GUARDED_BY(crit_);

  // If the packet buffer has received its first packet.
  bool first_packet_received_ RTC_GUARDED_BY(crit_);

  // If the buffer is cleared to |first_seq_num_|.
  bool is_cleared_to_first_seq_num_ RTC_GUARDED_BY(crit_);

  // Buffer that holds the the inserted packets and information needed to
  // determine continuity between them.
  std::vector<StoredPacket> buffer_ RTC_GUARDED_BY(crit_);

  // Timestamp of the last received packet/keyframe packet.
  absl::optional<int64_t> last_received_packet_ms_ RTC_GUARDED_BY(crit_);
  absl::optional<int64_t> last_received_keyframe_packet_ms_
      RTC_GUARDED_BY(crit_);
  absl::optional<uint32_t> last_received_keyframe_rtp_timestamp_
      RTC_GUARDED_BY(crit_);

  absl::optional<uint16_t> newest_inserted_seq_num_ RTC_GUARDED_BY(crit_);
  std::set<uint16_t, DescendingSeqNumComp<uint16_t>> missing_packets_
      RTC_GUARDED_BY(crit_);

  // Indicates if we should require SPS, PPS, and IDR for a particular
  // RTP timestamp to treat the corresponding frame as a keyframe.
  const bool sps_pps_idr_is_h264_keyframe_;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_PACKET_BUFFER_H_
