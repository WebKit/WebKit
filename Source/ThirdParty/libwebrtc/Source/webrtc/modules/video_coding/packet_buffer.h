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

#include "modules/include/module_common_types.h"
#include "modules/video_coding/packet.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class Clock;

namespace video_coding {

class RtpFrameObject;

// A received frame is a frame which has received all its packets.
class OnReceivedFrameCallback {
 public:
  virtual ~OnReceivedFrameCallback() {}
  virtual void OnReceivedFrame(std::unique_ptr<RtpFrameObject> frame) = 0;
};

class PacketBuffer {
 public:
  static rtc::scoped_refptr<PacketBuffer> Create(
      Clock* clock,
      size_t start_buffer_size,
      size_t max_buffer_size,
      OnReceivedFrameCallback* frame_callback);

  virtual ~PacketBuffer();

  // Returns true if |packet| is inserted into the packet buffer, false
  // otherwise. The PacketBuffer will always take ownership of the
  // |packet.dataPtr| when this function is called. Made virtual for testing.
  virtual bool InsertPacket(VCMPacket* packet);
  void ClearTo(uint16_t seq_num);
  void Clear();
  void PaddingReceived(uint16_t seq_num);

  // Timestamp (not RTP timestamp) of the last received packet/keyframe packet.
  absl::optional<int64_t> LastReceivedPacketMs() const;
  absl::optional<int64_t> LastReceivedKeyframePacketMs() const;

  // Returns number of different frames seen in the packet buffer
  int GetUniqueFramesSeen() const;

  int AddRef() const;
  int Release() const;

 protected:
  // Both |start_buffer_size| and |max_buffer_size| must be a power of 2.
  PacketBuffer(Clock* clock,
               size_t start_buffer_size,
               size_t max_buffer_size,
               OnReceivedFrameCallback* frame_callback);

 private:
  friend RtpFrameObject;
  // Since we want the packet buffer to be as packet type agnostic
  // as possible we extract only the information needed in order
  // to determine whether a sequence of packets is continuous or not.
  struct ContinuityInfo {
    // The sequence number of the packet.
    uint16_t seq_num = 0;

    // If this is the first packet of the frame.
    bool frame_begin = false;

    // If this is the last packet of the frame.
    bool frame_end = false;

    // If this slot is currently used.
    bool used = false;

    // If all its previous packets have been inserted into the packet buffer.
    bool continuous = false;

    // If this packet has been used to create a frame already.
    bool frame_created = false;
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

  // Copy the bitstream for |frame| to |destination|.
  // Virtual for testing.
  virtual bool GetBitstream(const RtpFrameObject& frame, uint8_t* destination);

  // Get the packet with sequence number |seq_num|.
  // Virtual for testing.
  virtual VCMPacket* GetPacket(uint16_t seq_num)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Mark all slots used by |frame| as not used.
  // Virtual for testing.
  virtual void ReturnFrame(RtpFrameObject* frame);

  void UpdateMissingPackets(uint16_t seq_num)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Counts unique received timestamps and updates |unique_frames_seen_|.
  void OnTimestampReceived(uint32_t rtp_timestamp)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  rtc::CriticalSection crit_;

  // Buffer size_ and max_size_ must always be a power of two.
  size_t size_ RTC_GUARDED_BY(crit_);
  const size_t max_size_;

  // The fist sequence number currently in the buffer.
  uint16_t first_seq_num_ RTC_GUARDED_BY(crit_);

  // If the packet buffer has received its first packet.
  bool first_packet_received_ RTC_GUARDED_BY(crit_);

  // If the buffer is cleared to |first_seq_num_|.
  bool is_cleared_to_first_seq_num_ RTC_GUARDED_BY(crit_);

  // Buffer that holds the inserted packets.
  std::vector<VCMPacket> data_buffer_ RTC_GUARDED_BY(crit_);

  // Buffer that holds the information about which slot that is currently in use
  // and information needed to determine the continuity between packets.
  std::vector<ContinuityInfo> sequence_buffer_ RTC_GUARDED_BY(crit_);

  // Called when a received frame is found.
  OnReceivedFrameCallback* const received_frame_callback_;

  // Timestamp (not RTP timestamp) of the last received packet/keyframe packet.
  absl::optional<int64_t> last_received_packet_ms_ RTC_GUARDED_BY(crit_);
  absl::optional<int64_t> last_received_keyframe_packet_ms_
      RTC_GUARDED_BY(crit_);

  int unique_frames_seen_ RTC_GUARDED_BY(crit_);

  absl::optional<uint16_t> newest_inserted_seq_num_ RTC_GUARDED_BY(crit_);
  std::set<uint16_t, DescendingSeqNumComp<uint16_t>> missing_packets_
      RTC_GUARDED_BY(crit_);

  // Indicates if we should require SPS, PPS, and IDR for a particular
  // RTP timestamp to treat the corresponding frame as a keyframe.
  const bool sps_pps_idr_is_h264_keyframe_;

  // Stores several last seen unique timestamps for quick search.
  std::set<uint32_t> rtp_timestamps_history_set_ RTC_GUARDED_BY(crit_);
  // Stores the same unique timestamps in the order of insertion.
  std::queue<uint32_t> rtp_timestamps_history_queue_ RTC_GUARDED_BY(crit_);

  mutable volatile int ref_count_ = 0;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_PACKET_BUFFER_H_
