/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/rtp_file_reader.h"

#include <stdio.h>

#include <map>
#include <string>
#include <vector>

#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "rtc_base/system/arch.h"

namespace webrtc {
namespace test {

static const size_t kFirstLineLength = 80;
static uint16_t kPacketHeaderSize = 8;

#define TRY(expr)                           \
  do {                                      \
    if (!(expr)) {                          \
      RTC_LOG(LS_INFO) << "Failed to read"; \
      return false;                         \
    }                                       \
  } while (0)

bool ReadUint32(uint32_t* out, FILE* file) {
  *out = 0;
  for (size_t i = 0; i < 4; ++i) {
    *out <<= 8;
    uint8_t tmp;
    if (fread(&tmp, 1, sizeof(uint8_t), file) != sizeof(uint8_t))
      return false;
    *out |= tmp;
  }
  return true;
}

bool ReadUint16(uint16_t* out, FILE* file) {
  *out = 0;
  for (size_t i = 0; i < 2; ++i) {
    *out <<= 8;
    uint8_t tmp;
    if (fread(&tmp, 1, sizeof(uint8_t), file) != sizeof(uint8_t))
      return false;
    *out |= tmp;
  }
  return true;
}

class RtpFileReaderImpl : public RtpFileReader {
 public:
  virtual bool Init(const std::string& filename,
                    const std::set<uint32_t>& ssrc_filter) = 0;
};

class InterleavedRtpFileReader : public RtpFileReaderImpl {
 public:
  virtual ~InterleavedRtpFileReader() {
    if (file_ != NULL) {
      fclose(file_);
      file_ = NULL;
    }
  }

  virtual bool Init(const std::string& filename,
                    const std::set<uint32_t>& ssrc_filter) {
    file_ = fopen(filename.c_str(), "rb");
    if (file_ == NULL) {
      printf("ERROR: Can't open file: %s\n", filename.c_str());
      return false;
    }
    return true;
  }
  virtual bool NextPacket(RtpPacket* packet) {
    assert(file_ != NULL);
    packet->length = RtpPacket::kMaxPacketBufferSize;
    uint32_t len = 0;
    TRY(ReadUint32(&len, file_));
    if (packet->length < len) {
      FATAL() << "Packet is too large to fit: " << len << " bytes vs "
              << packet->length
              << " bytes allocated. Consider increasing the buffer "
                 "size";
    }
    if (fread(packet->data, 1, len, file_) != len)
      return false;

    packet->length = len;
    packet->original_length = len;
    packet->time_ms = time_ms_;
    time_ms_ += 5;
    return true;
  }

 private:
  FILE* file_ = NULL;
  int64_t time_ms_ = 0;
};

// Read RTP packets from file in rtpdump format, as documented at:
// http://www.cs.columbia.edu/irt/software/rtptools/
class RtpDumpReader : public RtpFileReaderImpl {
 public:
  RtpDumpReader() : file_(NULL) {}
  virtual ~RtpDumpReader() {
    if (file_ != NULL) {
      fclose(file_);
      file_ = NULL;
    }
  }

  bool Init(const std::string& filename,
            const std::set<uint32_t>& ssrc_filter) override {
    file_ = fopen(filename.c_str(), "rb");
    if (file_ == NULL) {
      printf("ERROR: Can't open file: %s\n", filename.c_str());
      return false;
    }

    char firstline[kFirstLineLength + 1] = {0};
    if (fgets(firstline, kFirstLineLength, file_) == NULL) {
      RTC_LOG(LS_INFO) << "Can't read from file";
      return false;
    }
    if (strncmp(firstline, "#!rtpplay", 9) == 0) {
      if (strncmp(firstline, "#!rtpplay1.0", 12) != 0) {
        RTC_LOG(LS_INFO) << "Wrong rtpplay version, must be 1.0";
        return false;
      }
    } else if (strncmp(firstline, "#!RTPencode", 11) == 0) {
      if (strncmp(firstline, "#!RTPencode1.0", 14) != 0) {
        RTC_LOG(LS_INFO) << "Wrong RTPencode version, must be 1.0";
        return false;
      }
    } else {
      RTC_LOG(LS_INFO) << "Wrong file format of input file";
      return false;
    }

    uint32_t start_sec;
    uint32_t start_usec;
    uint32_t source;
    uint16_t port;
    uint16_t padding;
    TRY(ReadUint32(&start_sec, file_));
    TRY(ReadUint32(&start_usec, file_));
    TRY(ReadUint32(&source, file_));
    TRY(ReadUint16(&port, file_));
    TRY(ReadUint16(&padding, file_));

    return true;
  }

  bool NextPacket(RtpPacket* packet) override {
    uint8_t* rtp_data = packet->data;
    packet->length = RtpPacket::kMaxPacketBufferSize;

    uint16_t len;
    uint16_t plen;
    uint32_t offset;
    TRY(ReadUint16(&len, file_));
    TRY(ReadUint16(&plen, file_));
    TRY(ReadUint32(&offset, file_));

    // Use 'len' here because a 'plen' of 0 specifies rtcp.
    len -= kPacketHeaderSize;
    if (packet->length < len) {
      FATAL() << "Packet is too large to fit: " << len << " bytes vs "
              << packet->length
              << " bytes allocated. Consider increasing the buffer "
                 "size";
    }
    if (fread(rtp_data, 1, len, file_) != len) {
      return false;
    }

    packet->length = len;
    packet->original_length = plen;
    packet->time_ms = offset;
    return true;
  }

 private:
  FILE* file_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpDumpReader);
};

enum {
  kResultFail = -1,
  kResultSuccess = 0,
  kResultSkip = 1,

  kPcapVersionMajor = 2,
  kPcapVersionMinor = 4,
  kLinktypeNull = 0,
  kLinktypeEthernet = 1,
  kBsdNullLoopback1 = 0x00000002,
  kBsdNullLoopback2 = 0x02000000,
  kEthernetIIHeaderMacSkip = 12,
  kEthertypeIp = 0x0800,
  kIpVersion4 = 4,
  kMinIpHeaderLength = 20,
  kFragmentOffsetClear = 0x0000,
  kFragmentOffsetDoNotFragment = 0x4000,
  kProtocolTcp = 0x06,
  kProtocolUdp = 0x11,
  kUdpHeaderLength = 8,
  kMaxReadBufferSize = 4096
};

const uint32_t kPcapBOMSwapOrder = 0xd4c3b2a1UL;
const uint32_t kPcapBOMNoSwapOrder = 0xa1b2c3d4UL;

#define TRY_PCAP(expr)                                               \
  do {                                                               \
    int r = (expr);                                                  \
    if (r == kResultFail) {                                          \
      RTC_LOG(LS_INFO) << "FAIL at " << __FILE__ << ":" << __LINE__; \
      return kResultFail;                                            \
    } else if (r == kResultSkip) {                                   \
      return kResultSkip;                                            \
    }                                                                \
  } while (0)

// Read RTP packets from file in tcpdump/libpcap format, as documented at:
// http://wiki.wireshark.org/Development/LibpcapFileFormat
class PcapReader : public RtpFileReaderImpl {
 public:
  PcapReader()
      : file_(NULL),
        swap_pcap_byte_order_(false),
#ifdef WEBRTC_ARCH_BIG_ENDIAN
        swap_network_byte_order_(false),
#else
        swap_network_byte_order_(true),
#endif
        read_buffer_(),
        packets_by_ssrc_(),
        packets_(),
        next_packet_it_() {
  }

  virtual ~PcapReader() {
    if (file_ != NULL) {
      fclose(file_);
      file_ = NULL;
    }
  }

  bool Init(const std::string& filename,
            const std::set<uint32_t>& ssrc_filter) override {
    return Initialize(filename, ssrc_filter) == kResultSuccess;
  }

  int Initialize(const std::string& filename,
                 const std::set<uint32_t>& ssrc_filter) {
    file_ = fopen(filename.c_str(), "rb");
    if (file_ == NULL) {
      printf("ERROR: Can't open file: %s\n", filename.c_str());
      return kResultFail;
    }

    if (ReadGlobalHeader() < 0) {
      return kResultFail;
    }

    int total_packet_count = 0;
    uint32_t stream_start_ms = 0;
    int32_t next_packet_pos = ftell(file_);
    for (;;) {
      TRY_PCAP(fseek(file_, next_packet_pos, SEEK_SET));
      int result = ReadPacket(&next_packet_pos, stream_start_ms,
                              ++total_packet_count, ssrc_filter);
      if (result == kResultFail) {
        break;
      } else if (result == kResultSuccess && packets_.size() == 1) {
        assert(stream_start_ms == 0);
        PacketIterator it = packets_.begin();
        stream_start_ms = it->time_offset_ms;
        it->time_offset_ms = 0;
      }
    }

    if (feof(file_) == 0) {
      printf("Failed reading file!\n");
      return kResultFail;
    }

    printf("Total packets in file: %d\n", total_packet_count);
    printf("Total RTP/RTCP packets: %" PRIuS "\n", packets_.size());

    for (SsrcMapIterator mit = packets_by_ssrc_.begin();
         mit != packets_by_ssrc_.end(); ++mit) {
      uint32_t ssrc = mit->first;
      const std::vector<uint32_t>& packet_indices = mit->second;
      uint8_t pt = packets_[packet_indices[0]].rtp_header.payloadType;
      printf("SSRC: %08x, %" PRIuS " packets, pt=%d\n", ssrc,
             packet_indices.size(), pt);
    }

    // TODO(solenberg): Better validation of identified SSRC streams.
    //
    // Since we're dealing with raw network data here, we will wrongly identify
    // some packets as RTP. When these packets are consumed by RtpPlayer, they
    // are unlikely to cause issues as they will ultimately be filtered out by
    // the RtpRtcp module. However, we should really do better filtering here,
    // which we can accomplish in a number of ways, e.g.:
    //
    // - Verify that the time stamps and sequence numbers for RTP packets are
    //   both increasing/decreasing. If they move in different directions, the
    //   SSRC is likely bogus and can be dropped. (Normally they should be inc-
    //   reasing but we must allow packet reordering).
    // - If RTP sequence number is not changing, drop the stream.
    // - Can also use srcip:port->dstip:port pairs, assuming few SSRC collisions
    //   for up/down streams.

    next_packet_it_ = packets_.begin();
    return kResultSuccess;
  }

  bool NextPacket(RtpPacket* packet) override {
    uint32_t length = RtpPacket::kMaxPacketBufferSize;
    if (NextPcap(packet->data, &length, &packet->time_ms) != kResultSuccess)
      return false;
    packet->length = static_cast<size_t>(length);
    packet->original_length = packet->length;
    return true;
  }

  virtual int NextPcap(uint8_t* data, uint32_t* length, uint32_t* time_ms) {
    assert(data);
    assert(length);
    assert(time_ms);

    if (next_packet_it_ == packets_.end()) {
      return -1;
    }
    if (*length < next_packet_it_->payload_length) {
      return -1;
    }
    TRY_PCAP(fseek(file_, next_packet_it_->pos_in_file, SEEK_SET));
    TRY_PCAP(Read(data, next_packet_it_->payload_length));
    *length = next_packet_it_->payload_length;
    *time_ms = next_packet_it_->time_offset_ms;
    next_packet_it_++;

    return 0;
  }

 private:
  // A marker of an RTP packet within the file.
  struct RtpPacketMarker {
    uint32_t packet_number;  // One-based index (like in WireShark)
    uint32_t time_offset_ms;
    uint32_t source_ip;
    uint32_t dest_ip;
    uint16_t source_port;
    uint16_t dest_port;
    RTPHeader rtp_header;
    int32_t pos_in_file;  // Byte offset of payload from start of file.
    uint32_t payload_length;
  };

  typedef std::vector<RtpPacketMarker>::iterator PacketIterator;
  typedef std::map<uint32_t, std::vector<uint32_t> > SsrcMap;
  typedef std::map<uint32_t, std::vector<uint32_t> >::iterator SsrcMapIterator;

  int ReadGlobalHeader() {
    uint32_t magic;
    TRY_PCAP(Read(&magic, false));
    if (magic == kPcapBOMSwapOrder) {
      swap_pcap_byte_order_ = true;
    } else if (magic == kPcapBOMNoSwapOrder) {
      swap_pcap_byte_order_ = false;
    } else {
      return kResultFail;
    }

    uint16_t version_major;
    uint16_t version_minor;
    TRY_PCAP(Read(&version_major, false));
    TRY_PCAP(Read(&version_minor, false));
    if (version_major != kPcapVersionMajor ||
        version_minor != kPcapVersionMinor) {
      return kResultFail;
    }

    int32_t this_zone;  // GMT to local correction.
    uint32_t sigfigs;   // Accuracy of timestamps.
    uint32_t snaplen;   // Max length of captured packets, in octets.
    uint32_t network;   // Data link type.
    TRY_PCAP(Read(&this_zone, false));
    TRY_PCAP(Read(&sigfigs, false));
    TRY_PCAP(Read(&snaplen, false));
    TRY_PCAP(Read(&network, false));

    // Accept only LINKTYPE_NULL and LINKTYPE_ETHERNET.
    // See: http://www.tcpdump.org/linktypes.html
    if (network != kLinktypeNull && network != kLinktypeEthernet) {
      return kResultFail;
    }

    return kResultSuccess;
  }

  int ReadPacket(int32_t* next_packet_pos,
                 uint32_t stream_start_ms,
                 uint32_t number,
                 const std::set<uint32_t>& ssrc_filter) {
    assert(next_packet_pos);

    uint32_t ts_sec;    // Timestamp seconds.
    uint32_t ts_usec;   // Timestamp microseconds.
    uint32_t incl_len;  // Number of octets of packet saved in file.
    uint32_t orig_len;  // Actual length of packet.
    TRY_PCAP(Read(&ts_sec, false));
    TRY_PCAP(Read(&ts_usec, false));
    TRY_PCAP(Read(&incl_len, false));
    TRY_PCAP(Read(&orig_len, false));

    *next_packet_pos = ftell(file_) + incl_len;

    RtpPacketMarker marker = {0};
    marker.packet_number = number;
    marker.time_offset_ms = CalcTimeDelta(ts_sec, ts_usec, stream_start_ms);
    TRY_PCAP(ReadPacketHeader(&marker));
    marker.pos_in_file = ftell(file_);

    if (marker.payload_length > sizeof(read_buffer_)) {
      printf("Packet too large!\n");
      return kResultFail;
    }
    TRY_PCAP(Read(read_buffer_, marker.payload_length));

    RtpUtility::RtpHeaderParser rtp_parser(read_buffer_, marker.payload_length);
    if (rtp_parser.RTCP()) {
      rtp_parser.ParseRtcp(&marker.rtp_header);
      packets_.push_back(marker);
    } else {
      if (!rtp_parser.Parse(&marker.rtp_header, nullptr)) {
        RTC_LOG(LS_INFO) << "Not recognized as RTP/RTCP";
        return kResultSkip;
      }

      uint32_t ssrc = marker.rtp_header.ssrc;
      if (ssrc_filter.empty() || ssrc_filter.find(ssrc) != ssrc_filter.end()) {
        packets_by_ssrc_[ssrc].push_back(
            static_cast<uint32_t>(packets_.size()));
        packets_.push_back(marker);
      } else {
        return kResultSkip;
      }
    }

    return kResultSuccess;
  }

  int ReadPacketHeader(RtpPacketMarker* marker) {
    int32_t file_pos = ftell(file_);

    // Check for BSD null/loopback frame header. The header is just 4 bytes in
    // native byte order, so we check for both versions as we don't care about
    // the header as such and will likely fail reading the IP header if this is
    // something else than null/loopback.
    uint32_t protocol;
    TRY_PCAP(Read(&protocol, true));
    if (protocol == kBsdNullLoopback1 || protocol == kBsdNullLoopback2) {
      int result = ReadXxpIpHeader(marker);
      RTC_LOG(LS_INFO) << "Recognized loopback frame";
      if (result != kResultSkip) {
        return result;
      }
    }

    TRY_PCAP(fseek(file_, file_pos, SEEK_SET));

    // Check for Ethernet II, IP frame header.
    uint16_t type;
    TRY_PCAP(Skip(kEthernetIIHeaderMacSkip));  // Source+destination MAC.
    TRY_PCAP(Read(&type, true));
    if (type == kEthertypeIp) {
      int result = ReadXxpIpHeader(marker);
      RTC_LOG(LS_INFO) << "Recognized ethernet 2 frame";
      if (result != kResultSkip) {
        return result;
      }
    }

    return kResultSkip;
  }

  uint32_t CalcTimeDelta(uint32_t ts_sec, uint32_t ts_usec, uint32_t start_ms) {
    // Round to nearest ms.
    uint64_t t2_ms =
        ((static_cast<uint64_t>(ts_sec) * 1000000) + ts_usec + 500) / 1000;
    uint64_t t1_ms = static_cast<uint64_t>(start_ms);
    if (t2_ms < t1_ms) {
      return 0;
    } else {
      return t2_ms - t1_ms;
    }
  }

  int ReadXxpIpHeader(RtpPacketMarker* marker) {
    assert(marker);

    uint16_t version;
    uint16_t length;
    uint16_t id;
    uint16_t fragment;
    uint16_t protocol;
    uint16_t checksum;
    TRY_PCAP(Read(&version, true));
    TRY_PCAP(Read(&length, true));
    TRY_PCAP(Read(&id, true));
    TRY_PCAP(Read(&fragment, true));
    TRY_PCAP(Read(&protocol, true));
    TRY_PCAP(Read(&checksum, true));
    TRY_PCAP(Read(&marker->source_ip, true));
    TRY_PCAP(Read(&marker->dest_ip, true));

    if (((version >> 12) & 0x000f) != kIpVersion4) {
      RTC_LOG(LS_INFO) << "IP header is not IPv4";
      return kResultSkip;
    }

    if (fragment != kFragmentOffsetClear &&
        fragment != kFragmentOffsetDoNotFragment) {
      RTC_LOG(LS_INFO) << "IP fragments cannot be handled";
      return kResultSkip;
    }

    // Skip remaining fields of IP header.
    uint16_t header_length = (version & 0x0f00) >> (8 - 2);
    assert(header_length >= kMinIpHeaderLength);
    TRY_PCAP(Skip(header_length - kMinIpHeaderLength));

    protocol = protocol & 0x00ff;
    if (protocol == kProtocolTcp) {
      RTC_LOG(LS_INFO) << "TCP packets are not handled";
      return kResultSkip;
    } else if (protocol == kProtocolUdp) {
      uint16_t length;
      uint16_t checksum;
      TRY_PCAP(Read(&marker->source_port, true));
      TRY_PCAP(Read(&marker->dest_port, true));
      TRY_PCAP(Read(&length, true));
      TRY_PCAP(Read(&checksum, true));
      marker->payload_length = length - kUdpHeaderLength;
    } else {
      RTC_LOG(LS_INFO) << "Unknown transport (expected UDP or TCP)";
      return kResultSkip;
    }

    return kResultSuccess;
  }

  int Read(uint32_t* out, bool expect_network_order) {
    uint32_t tmp = 0;
    if (fread(&tmp, 1, sizeof(uint32_t), file_) != sizeof(uint32_t)) {
      return kResultFail;
    }
    if ((!expect_network_order && swap_pcap_byte_order_) ||
        (expect_network_order && swap_network_byte_order_)) {
      tmp = ((tmp >> 24) & 0x000000ff) | (tmp << 24) |
            ((tmp >> 8) & 0x0000ff00) | ((tmp << 8) & 0x00ff0000);
    }
    *out = tmp;
    return kResultSuccess;
  }

  int Read(uint16_t* out, bool expect_network_order) {
    uint16_t tmp = 0;
    if (fread(&tmp, 1, sizeof(uint16_t), file_) != sizeof(uint16_t)) {
      return kResultFail;
    }
    if ((!expect_network_order && swap_pcap_byte_order_) ||
        (expect_network_order && swap_network_byte_order_)) {
      tmp = ((tmp >> 8) & 0x00ff) | (tmp << 8);
    }
    *out = tmp;
    return kResultSuccess;
  }

  int Read(uint8_t* out, uint32_t count) {
    if (fread(out, 1, count, file_) != count) {
      return kResultFail;
    }
    return kResultSuccess;
  }

  int Read(int32_t* out, bool expect_network_order) {
    int32_t tmp = 0;
    if (fread(&tmp, 1, sizeof(uint32_t), file_) != sizeof(uint32_t)) {
      return kResultFail;
    }
    if ((!expect_network_order && swap_pcap_byte_order_) ||
        (expect_network_order && swap_network_byte_order_)) {
      tmp = ((tmp >> 24) & 0x000000ff) | (tmp << 24) |
            ((tmp >> 8) & 0x0000ff00) | ((tmp << 8) & 0x00ff0000);
    }
    *out = tmp;
    return kResultSuccess;
  }

  int Skip(uint32_t length) {
    if (fseek(file_, length, SEEK_CUR) != 0) {
      return kResultFail;
    }
    return kResultSuccess;
  }

  FILE* file_;
  bool swap_pcap_byte_order_;
  const bool swap_network_byte_order_;
  uint8_t read_buffer_[kMaxReadBufferSize];

  SsrcMap packets_by_ssrc_;
  std::vector<RtpPacketMarker> packets_;
  PacketIterator next_packet_it_;

  RTC_DISALLOW_COPY_AND_ASSIGN(PcapReader);
};

RtpFileReader* RtpFileReader::Create(FileFormat format,
                                     const std::string& filename,
                                     const std::set<uint32_t>& ssrc_filter) {
  RtpFileReaderImpl* reader = NULL;
  switch (format) {
    case kPcap:
      reader = new PcapReader();
      break;
    case kRtpDump:
      reader = new RtpDumpReader();
      break;
    case kLengthPacketInterleaved:
      reader = new InterleavedRtpFileReader();
      break;
  }
  if (!reader->Init(filename, ssrc_filter)) {
    delete reader;
    return NULL;
  }
  return reader;
}

RtpFileReader* RtpFileReader::Create(FileFormat format,
                                     const std::string& filename) {
  return RtpFileReader::Create(format, filename, std::set<uint32_t>());
}

}  // namespace test
}  // namespace webrtc
