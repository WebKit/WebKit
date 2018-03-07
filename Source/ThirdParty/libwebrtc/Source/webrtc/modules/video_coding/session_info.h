/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_SESSION_INFO_H_
#define MODULES_VIDEO_CODING_SESSION_INFO_H_

#include <list>
#include <vector>

#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/packet.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {
// Used to pass data from jitter buffer to session info.
// This data is then used in determining whether a frame is decodable.
struct FrameData {
  int64_t rtt_ms;
  float rolling_average_packets_per_frame;
};

class VCMSessionInfo {
 public:
  VCMSessionInfo();

  void UpdateDataPointers(const uint8_t* old_base_ptr,
                          const uint8_t* new_base_ptr);
  // NACK - Building the NACK lists.
  // Build hard NACK list: Zero out all entries in list up to and including
  // _lowSeqNum.
  int BuildHardNackList(int* seq_num_list,
                        int seq_num_list_length,
                        int nack_seq_nums_index);

  // Build soft NACK list:  Zero out only a subset of the packets, discard
  // empty packets.
  int BuildSoftNackList(int* seq_num_list,
                        int seq_num_list_length,
                        int nack_seq_nums_index,
                        int rtt_ms);
  void Reset();
  int InsertPacket(const VCMPacket& packet,
                   uint8_t* frame_buffer,
                   VCMDecodeErrorMode enable_decodable_state,
                   const FrameData& frame_data);
  bool complete() const;
  bool decodable() const;

  // Makes the frame decodable. I.e., only contain decodable NALUs. All
  // non-decodable NALUs will be deleted and packets will be moved to in
  // memory to remove any empty space.
  // Returns the number of bytes deleted from the session.
  size_t MakeDecodable();

  // Sets decodable_ to false.
  // Used by the dual decoder. After the mode is changed to kNoErrors from
  // kWithErrors or kSelective errors, any states that have been marked
  // decodable and are not complete are marked as non-decodable.
  void SetNotDecodableIfIncomplete();

  size_t SessionLength() const;
  int NumPackets() const;
  bool HaveFirstPacket() const;
  bool HaveLastPacket() const;
  bool session_nack() const;
  webrtc::FrameType FrameType() const { return frame_type_; }
  int LowSequenceNumber() const;

  // Returns highest sequence number, media or empty.
  int HighSequenceNumber() const;
  int PictureId() const;
  int TemporalId() const;
  bool LayerSync() const;
  int Tl0PicId() const;
  bool NonReference() const;

  std::vector<NaluInfo> GetNaluInfos() const;

  void SetGofInfo(const GofInfoVP9& gof_info, size_t idx);

  // The number of packets discarded because the decoder can't make use of
  // them.
  int packets_not_decodable() const;

 private:
  enum { kMaxVP8Partitions = 9 };

  typedef std::list<VCMPacket> PacketList;
  typedef PacketList::iterator PacketIterator;
  typedef PacketList::const_iterator PacketIteratorConst;
  typedef PacketList::reverse_iterator ReversePacketIterator;

  void InformOfEmptyPacket(uint16_t seq_num);

  // Finds the packet of the beginning of the next VP8 partition. If
  // none is found the returned iterator points to |packets_.end()|.
  // |it| is expected to point to the last packet of the previous partition,
  // or to the first packet of the frame. |packets_skipped| is incremented
  // for each packet found which doesn't have the beginning bit set.
  PacketIterator FindNextPartitionBeginning(PacketIterator it) const;

  // Returns an iterator pointing to the last packet of the partition pointed to
  // by |it|.
  PacketIterator FindPartitionEnd(PacketIterator it) const;
  static bool InSequence(const PacketIterator& it,
                         const PacketIterator& prev_it);
  size_t InsertBuffer(uint8_t* frame_buffer, PacketIterator packetIterator);
  size_t Insert(const uint8_t* buffer,
                size_t length,
                bool insert_start_code,
                uint8_t* frame_buffer);
  void ShiftSubsequentPackets(PacketIterator it, int steps_to_shift);
  PacketIterator FindNaluEnd(PacketIterator packet_iter) const;
  // Deletes the data of all packets between |start| and |end|, inclusively.
  // Note that this function doesn't delete the actual packets.
  size_t DeletePacketData(PacketIterator start, PacketIterator end);
  void UpdateCompleteSession();

  // When enabled, determine if session is decodable, i.e. incomplete but
  // would be sent to the decoder.
  // Note: definition assumes random loss.
  // A frame is defined to be decodable when:
  //  Round trip time is higher than threshold
  //  It is not a key frame
  //  It has the first packet: In VP8 the first packet contains all or part of
  //    the first partition, which consists of the most relevant information for
  //    decoding.
  //  Either more than the upper threshold of the average number of packets per
  //        frame is present
  //      or less than the lower threshold of the average number of packets per
  //        frame is present: suggests a small frame. Such a frame is unlikely
  //        to contain many motion vectors, so having the first packet will
  //        likely suffice. Once we have more than the lower threshold of the
  //        frame, we know that the frame is medium or large-sized.
  void UpdateDecodableSession(const FrameData& frame_data);

  // If this session has been NACKed by the jitter buffer.
  bool session_nack_;
  bool complete_;
  bool decodable_;
  webrtc::FrameType frame_type_;
  // Packets in this frame.
  PacketList packets_;
  int empty_seq_num_low_;
  int empty_seq_num_high_;

  // The following two variables correspond to the first and last media packets
  // in a session defined by the first packet flag and the marker bit.
  // They are not necessarily equal to the front and back packets, as packets
  // may enter out of order.
  // TODO(mikhal): Refactor the list to use a map.
  int first_packet_seq_num_;
  int last_packet_seq_num_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_SESSION_INFO_H_
