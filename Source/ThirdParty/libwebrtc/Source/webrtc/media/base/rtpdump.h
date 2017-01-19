/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_RTPDUMP_H_
#define WEBRTC_MEDIA_BASE_RTPDUMP_H_

#include <string.h>

#include <string>
#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/stream.h"

namespace cricket {

// We use the RTP dump file format compatible to the format used by rtptools
// (http://www.cs.columbia.edu/irt/software/rtptools/) and Wireshark
// (http://wiki.wireshark.org/rtpdump). In particular, the file starts with the
// first line "#!rtpplay1.0 address/port\n", followed by a 16 byte file header.
// For each packet, the file contains a 8 byte dump packet header, followed by
// the actual RTP or RTCP packet.

enum RtpDumpPacketFilter {
  PF_NONE = 0x0,
  PF_RTPHEADER = 0x1,
  PF_RTPPACKET = 0x3,  // includes header
  // PF_RTCPHEADER = 0x4,  // TODO(juberti)
  PF_RTCPPACKET = 0xC,  // includes header
  PF_ALL = 0xF
};

struct RtpDumpFileHeader {
  RtpDumpFileHeader(int64_t start_ms, uint32_t s, uint16_t p);
  void WriteToByteBuffer(rtc::ByteBufferWriter* buf);

  static const char kFirstLine[];
  static const size_t kHeaderLength = 16;
  uint32_t start_sec;   // start of recording, the seconds part.
  uint32_t start_usec;  // start of recording, the microseconds part.
  uint32_t source;      // network source (multicast address).
  uint16_t port;        // UDP port.
  uint16_t padding;     // 2 bytes padding.
};

struct RtpDumpPacket {
  RtpDumpPacket() {}

  RtpDumpPacket(const void* d, size_t s, uint32_t elapsed, bool rtcp)
      : elapsed_time(elapsed), original_data_len((rtcp) ? 0 : s) {
    data.resize(s);
    memcpy(&data[0], d, s);
  }

  // In the rtpdump file format, RTCP packets have their data len set to zero,
  // since RTCP has an internal length field.
  bool is_rtcp() const { return original_data_len == 0; }
  bool IsValidRtpPacket() const;
  bool IsValidRtcpPacket() const;
  // Get the payload type, sequence number, timestampe, and SSRC of the RTP
  // packet. Return true and set the output parameter if successful.
  bool GetRtpPayloadType(int* pt) const;
  bool GetRtpSeqNum(int* seq_num) const;
  bool GetRtpTimestamp(uint32_t* ts) const;
  bool GetRtpSsrc(uint32_t* ssrc) const;
  bool GetRtpHeaderLen(size_t* len) const;
  // Get the type of the RTCP packet. Return true and set the output parameter
  // if successful.
  bool GetRtcpType(int* type) const;

  static const size_t kHeaderLength = 8;
  uint32_t elapsed_time;      // Milliseconds since the start of recording.
  std::vector<uint8_t> data;  // The actual RTP or RTCP packet.
  size_t original_data_len;  // The original length of the packet; may be
                             // greater than data.size() if only part of the
                             // packet was recorded.
};

class RtpDumpReader {
 public:
  explicit RtpDumpReader(rtc::StreamInterface* stream)
      : stream_(stream),
        file_header_read_(false),
        first_line_and_file_header_len_(0),
        start_time_ms_(0),
        ssrc_override_(0) {
  }
  virtual ~RtpDumpReader() {}

  // Use the specified ssrc, rather than the ssrc from dump, for RTP packets.
  void SetSsrc(uint32_t ssrc);
  virtual rtc::StreamResult ReadPacket(RtpDumpPacket* packet);

 protected:
  rtc::StreamResult ReadFileHeader();
  bool RewindToFirstDumpPacket() {
    return stream_->SetPosition(first_line_and_file_header_len_);
  }

 private:
  // Check if its matches "#!rtpplay1.0 address/port\n".
  bool CheckFirstLine(const std::string& first_line);

  rtc::StreamInterface* stream_;
  bool file_header_read_;
  size_t first_line_and_file_header_len_;
  int64_t start_time_ms_;
  uint32_t ssrc_override_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpDumpReader);
};

// RtpDumpLoopReader reads RTP dump packets from the input stream and rewinds
// the stream when it ends. RtpDumpLoopReader maintains the elapsed time, the
// RTP sequence number and the RTP timestamp properly. RtpDumpLoopReader can
// handle both RTP dump and RTCP dump. We assume that the dump does not mix
// RTP packets and RTCP packets.
class RtpDumpLoopReader : public RtpDumpReader {
 public:
  explicit RtpDumpLoopReader(rtc::StreamInterface* stream);
  virtual rtc::StreamResult ReadPacket(RtpDumpPacket* packet);

 private:
  // During the first loop, update the statistics, including packet count, frame
  // count, timestamps, and sequence number, of the input stream.
  void UpdateStreamStatistics(const RtpDumpPacket& packet);

  // At the end of first loop, calculate elapsed_time_increases_,
  // rtp_seq_num_increase_, and rtp_timestamp_increase_.
  void CalculateIncreases();

  // During the second and later loops, update the elapsed time of the dump
  // packet. If the dumped packet is a RTP packet, update its RTP sequence
  // number and timestamp as well.
  void UpdateDumpPacket(RtpDumpPacket* packet);

  int loop_count_;
  // How much to increase the elapsed time, RTP sequence number, RTP timestampe
  // for each loop. They are calcualted with the variables below during the
  // first loop.
  uint32_t elapsed_time_increases_;
  int rtp_seq_num_increase_;
  uint32_t rtp_timestamp_increase_;
  // How many RTP packets and how many payload frames in the input stream. RTP
  // packets belong to the same frame have the same RTP timestamp, different
  // dump timestamp, and different RTP sequence number.
  uint32_t packet_count_;
  uint32_t frame_count_;
  // The elapsed time, RTP sequence number, and RTP timestamp of the first and
  // the previous dump packets in the input stream.
  uint32_t first_elapsed_time_;
  int first_rtp_seq_num_;
  int64_t first_rtp_timestamp_;
  uint32_t prev_elapsed_time_;
  int prev_rtp_seq_num_;
  int64_t prev_rtp_timestamp_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpDumpLoopReader);
};

class RtpDumpWriter {
 public:
  explicit RtpDumpWriter(rtc::StreamInterface* stream);

  // Filter to control what packets we actually record.
  void set_packet_filter(int filter);
  // Write a RTP or RTCP packet. The parameters data points to the packet and
  // data_len is its length.
  rtc::StreamResult WriteRtpPacket(const void* data, size_t data_len) {
    return WritePacket(data, data_len, GetElapsedTime(), false);
  }
  rtc::StreamResult WriteRtcpPacket(const void* data, size_t data_len) {
    return WritePacket(data, data_len, GetElapsedTime(), true);
  }
  rtc::StreamResult WritePacket(const RtpDumpPacket& packet) {
    return WritePacket(&packet.data[0], packet.data.size(), packet.elapsed_time,
                       packet.is_rtcp());
  }
  uint32_t GetElapsedTime() const;

  bool GetDumpSize(size_t* size) {
    // Note that we use GetPosition(), rather than GetSize(), to avoid flush the
    // stream per write.
    return stream_ && size && stream_->GetPosition(size);
  }

 protected:
  rtc::StreamResult WriteFileHeader();

 private:
  rtc::StreamResult WritePacket(const void* data,
                                size_t data_len,
                                uint32_t elapsed,
                                bool rtcp);
  size_t FilterPacket(const void* data, size_t data_len, bool rtcp);
  rtc::StreamResult WriteToStream(const void* data, size_t data_len);

  rtc::StreamInterface* stream_;
  int packet_filter_;
  bool file_header_written_;
  int64_t start_time_ms_;  // Time when the record starts.
  // If writing to the stream takes longer than this many ms, log a warning.
  int64_t warn_slow_writes_delay_;
  RTC_DISALLOW_COPY_AND_ASSIGN(RtpDumpWriter);
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_RTPDUMP_H_
