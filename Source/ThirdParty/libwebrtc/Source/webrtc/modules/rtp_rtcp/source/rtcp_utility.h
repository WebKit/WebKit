/*
*  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_UTILITY_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_UTILITY_H_

#include <stddef.h> // size_t, ptrdiff_t

#include <memory>

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace rtcp {
class RtcpPacket;
}
namespace RTCPUtility {

class NackStats {
 public:
  NackStats();
  ~NackStats();

  // Updates stats with requested sequence number.
  // This function should be called for each NACK request to calculate the
  // number of unique NACKed RTP packets.
  void ReportRequest(uint16_t sequence_number);

  // Gets the number of NACKed RTP packets.
  uint32_t requests() const { return requests_; }

  // Gets the number of unique NACKed RTP packets.
  uint32_t unique_requests() const { return unique_requests_; }

 private:
  uint16_t max_sequence_number_;
  uint32_t requests_;
  uint32_t unique_requests_;
};

uint32_t MidNtp(uint32_t ntp_sec, uint32_t ntp_frac);

struct RTCPPacketRR {
  uint32_t SenderSSRC;
  uint8_t NumberOfReportBlocks;
};
struct RTCPPacketSR {
  uint32_t SenderSSRC;
  uint8_t NumberOfReportBlocks;

  // sender info
  uint32_t NTPMostSignificant;
  uint32_t NTPLeastSignificant;
  uint32_t RTPTimestamp;
  uint32_t SenderPacketCount;
  uint32_t SenderOctetCount;
};
struct RTCPPacketReportBlockItem {
  // report block
  uint32_t SSRC;
  uint8_t FractionLost;
  uint32_t CumulativeNumOfPacketsLost;
  uint32_t ExtendedHighestSequenceNumber;
  uint32_t Jitter;
  uint32_t LastSR;
  uint32_t DelayLastSR;
};
struct RTCPPacketSDESCName {
  // RFC3550
  uint32_t SenderSSRC;
  char CName[RTCP_CNAME_SIZE];
};

struct RTCPPacketExtendedJitterReportItem {
  // RFC 5450
  uint32_t Jitter;
};

struct RTCPPacketBYE {
  uint32_t SenderSSRC;
};
struct RTCPPacketXR {
  // RFC 3611
  uint32_t OriginatorSSRC;
};
struct RTCPPacketXRReceiverReferenceTimeItem {
  // RFC 3611 4.4
  uint32_t NTPMostSignificant;
  uint32_t NTPLeastSignificant;
};
struct RTCPPacketXRDLRRReportBlockItem {
  // RFC 3611 4.5
  uint32_t SSRC;
  uint32_t LastRR;
  uint32_t DelayLastRR;
};
struct RTCPPacketXRVOIPMetricItem {
  // RFC 3611 4.7
  uint32_t SSRC;
  uint8_t lossRate;
  uint8_t discardRate;
  uint8_t burstDensity;
  uint8_t gapDensity;
  uint16_t burstDuration;
  uint16_t gapDuration;
  uint16_t roundTripDelay;
  uint16_t endSystemDelay;
  uint8_t signalLevel;
  uint8_t noiseLevel;
  uint8_t RERL;
  uint8_t Gmin;
  uint8_t Rfactor;
  uint8_t extRfactor;
  uint8_t MOSLQ;
  uint8_t MOSCQ;
  uint8_t RXconfig;
  uint16_t JBnominal;
  uint16_t JBmax;
  uint16_t JBabsMax;
};

struct RTCPPacketRTPFBNACK {
  uint32_t SenderSSRC;
  uint32_t MediaSSRC;
};
struct RTCPPacketRTPFBNACKItem {
  // RFC4585
  uint16_t PacketID;
  uint16_t BitMask;
};

struct RTCPPacketRTPFBTMMBR {
  uint32_t SenderSSRC;
  uint32_t MediaSSRC;  // zero!
};
struct RTCPPacketRTPFBTMMBRItem {
  // RFC5104
  uint32_t SSRC;
  uint32_t MaxTotalMediaBitRate;  // In Kbit/s
  uint32_t MeasuredOverhead;
};

struct RTCPPacketRTPFBTMMBN {
  uint32_t SenderSSRC;
  uint32_t MediaSSRC;  // zero!
};
struct RTCPPacketRTPFBTMMBNItem {
  // RFC5104
  uint32_t SSRC;  // "Owner"
  uint32_t MaxTotalMediaBitRate;
  uint32_t MeasuredOverhead;
};

struct RTCPPacketPSFBFIR {
  uint32_t SenderSSRC;
  uint32_t MediaSSRC;  // zero!
};
struct RTCPPacketPSFBFIRItem {
  // RFC5104
  uint32_t SSRC;
  uint8_t CommandSequenceNumber;
};

struct RTCPPacketPSFBPLI {
  // RFC4585
  uint32_t SenderSSRC;
  uint32_t MediaSSRC;
};

struct RTCPPacketPSFBSLI {
  // RFC4585
  uint32_t SenderSSRC;
  uint32_t MediaSSRC;
};
struct RTCPPacketPSFBSLIItem {
  // RFC4585
  uint16_t FirstMB;
  uint16_t NumberOfMB;
  uint8_t PictureId;
};
struct RTCPPacketPSFBRPSI {
  // RFC4585
  uint32_t SenderSSRC;
  uint32_t MediaSSRC;
  uint8_t PayloadType;
  uint16_t NumberOfValidBits;
  uint8_t NativeBitString[RTCP_RPSI_DATA_SIZE];
};
struct RTCPPacketPSFBAPP {
  uint32_t SenderSSRC;
  uint32_t MediaSSRC;
};
struct RTCPPacketPSFBREMBItem {
  uint32_t BitRate;
  uint8_t NumberOfSSRCs;
  uint32_t SSRCs[MAX_NUMBER_OF_REMB_FEEDBACK_SSRCS];
};
// generic name APP
struct RTCPPacketAPP {
  uint8_t SubType;
  uint32_t Name;
  uint8_t Data[kRtcpAppCode_DATA_SIZE];
  uint16_t Size;
};

union RTCPPacket {
  RTCPPacketRR RR;
  RTCPPacketSR SR;
  RTCPPacketReportBlockItem ReportBlockItem;

  RTCPPacketSDESCName CName;
  RTCPPacketBYE BYE;

  RTCPPacketExtendedJitterReportItem ExtendedJitterReportItem;

  RTCPPacketRTPFBNACK NACK;
  RTCPPacketRTPFBNACKItem NACKItem;

  RTCPPacketPSFBPLI PLI;
  RTCPPacketPSFBSLI SLI;
  RTCPPacketPSFBSLIItem SLIItem;
  RTCPPacketPSFBRPSI RPSI;
  RTCPPacketPSFBAPP PSFBAPP;
  RTCPPacketPSFBREMBItem REMBItem;

  RTCPPacketRTPFBTMMBR TMMBR;
  RTCPPacketRTPFBTMMBRItem TMMBRItem;
  RTCPPacketRTPFBTMMBN TMMBN;
  RTCPPacketRTPFBTMMBNItem TMMBNItem;
  RTCPPacketPSFBFIR FIR;
  RTCPPacketPSFBFIRItem FIRItem;

  RTCPPacketXR XR;
  RTCPPacketXRReceiverReferenceTimeItem XRReceiverReferenceTimeItem;
  RTCPPacketXRDLRRReportBlockItem XRDLRRReportBlockItem;
  RTCPPacketXRVOIPMetricItem XRVOIPMetricItem;

  RTCPPacketAPP APP;
};

enum class RTCPPacketTypes {
  kInvalid,

  // RFC3550
  kRr,
  kSr,
  kReportBlockItem,

  kSdes,
  kSdesChunk,
  kBye,

  // RFC5450
  kExtendedIj,
  kExtendedIjItem,

  // RFC4585
  kRtpfbNack,
  kRtpfbNackItem,

  kPsfbPli,
  kPsfbRpsi,
  kPsfbRpsiItem,
  kPsfbSli,
  kPsfbSliItem,
  kPsfbApp,
  kPsfbRemb,
  kPsfbRembItem,

  // RFC5104
  kRtpfbTmmbr,
  kRtpfbTmmbrItem,
  kRtpfbTmmbn,
  kRtpfbTmmbnItem,
  kPsfbFir,
  kPsfbFirItem,

  // draft-perkins-avt-rapid-rtp-sync
  kRtpfbSrReq,

  // RFC 3611
  kXrHeader,
  kXrReceiverReferenceTime,
  kXrDlrrReportBlock,
  kXrDlrrReportBlockItem,
  kXrVoipMetric,

  kApp,
  kAppItem,

  // draft-holmer-rmcat-transport-wide-cc-extensions
  kTransportFeedback,
};

struct RTCPRawPacket {
  const uint8_t* _ptrPacketBegin;
  const uint8_t* _ptrPacketEnd;
};

struct RTCPModRawPacket {
  uint8_t* _ptrPacketBegin;
  uint8_t* _ptrPacketEnd;
};

struct RtcpCommonHeader {
  static const uint8_t kHeaderSizeBytes = 4;
  RtcpCommonHeader()
      : version(2),
        count_or_format(0),
        packet_type(0),
        payload_size_bytes(0),
        padding_bytes(0) {}

  uint32_t BlockSize() const {
    return kHeaderSizeBytes + payload_size_bytes + padding_bytes;
  }

  uint8_t version;
  uint8_t count_or_format;
  uint8_t packet_type;
  uint32_t payload_size_bytes;
  uint8_t padding_bytes;
};

enum RTCPPT : uint8_t {
  PT_IJ = 195,
  PT_SR = 200,
  PT_RR = 201,
  PT_SDES = 202,
  PT_BYE = 203,
  PT_APP = 204,
  PT_RTPFB = 205,
  PT_PSFB = 206,
  PT_XR = 207
};

// Extended report blocks, RFC 3611.
enum RtcpXrBlockType : uint8_t {
  kBtReceiverReferenceTime = 4,
  kBtDlrr = 5,
  kBtVoipMetric = 7
};

bool RtcpParseCommonHeader(const uint8_t* buffer,
                           size_t size_bytes,
                           RtcpCommonHeader* parsed_header);

class RTCPParserV2 {
 public:
  RTCPParserV2(
      const uint8_t* rtcpData,
      size_t rtcpDataLength,
      bool rtcpReducedSizeEnable);  // Set to true, to allow non-compound RTCP!
  ~RTCPParserV2();

  RTCPPacketTypes PacketType() const;
  const RTCPPacket& Packet() const;
  rtcp::RtcpPacket* ReleaseRtcpPacket();
  const RTCPRawPacket& RawPacket() const;
  ptrdiff_t LengthLeft() const;

  bool IsValid() const;
  size_t NumSkippedBlocks() const;

  RTCPPacketTypes Begin();
  RTCPPacketTypes Iterate();

 private:
  enum class ParseState {
    State_TopLevel,            // Top level packet
    State_ReportBlockItem,     // SR/RR report block
    State_SDESChunk,           // SDES chunk
    State_BYEItem,             // BYE item
    State_ExtendedJitterItem,  // Extended jitter report item
    State_RTPFB_NACKItem,      // NACK FCI item
    State_RTPFB_TMMBRItem,     // TMMBR FCI item
    State_RTPFB_TMMBNItem,     // TMMBN FCI item
    State_PSFB_SLIItem,        // SLI FCI item
    State_PSFB_RPSIItem,       // RPSI FCI item
    State_PSFB_FIRItem,        // FIR FCI item
    State_PSFB_AppItem,        // Application specific FCI item
    State_PSFB_REMBItem,       // Application specific REMB item
    State_XRItem,
    State_XR_DLLRItem,
    State_AppItem
  };

 private:
  void IterateTopLevel();
  void IterateReportBlockItem();
  void IterateSDESChunk();
  void IterateBYEItem();
  void IterateExtendedJitterItem();
  void IterateNACKItem();
  void IterateTMMBRItem();
  void IterateTMMBNItem();
  void IterateSLIItem();
  void IterateRPSIItem();
  void IterateFIRItem();
  void IteratePsfbAppItem();
  void IteratePsfbREMBItem();
  void IterateAppItem();
  void IterateXrItem();
  void IterateXrDlrrItem();

  void Validate();
  void EndCurrentBlock();

  bool ParseRR();
  bool ParseSR();
  bool ParseReportBlockItem();

  bool ParseSDES();
  bool ParseSDESChunk();
  bool ParseSDESItem();

  bool ParseBYE();
  bool ParseBYEItem();

  bool ParseIJ();
  bool ParseIJItem();

  bool ParseXr();
  bool ParseXrItem();
  bool ParseXrReceiverReferenceTimeItem(int block_length_4bytes);
  bool ParseXrDlrr(int block_length_4bytes);
  bool ParseXrDlrrItem();
  bool ParseXrVoipMetricItem(int block_length_4bytes);
  bool ParseXrUnsupportedBlockType(int block_length_4bytes);

  bool ParseFBCommon(const RtcpCommonHeader& header);
  bool ParseNACKItem();
  bool ParseTMMBRItem();
  bool ParseTMMBNItem();
  bool ParseSLIItem();
  bool ParseRPSIItem();
  bool ParseFIRItem();
  bool ParsePsfbAppItem();
  bool ParsePsfbREMBItem();

  bool ParseAPP(const RtcpCommonHeader& header);
  bool ParseAPPItem();

 private:
  const uint8_t* const _ptrRTCPDataBegin;
  const bool _RTCPReducedSizeEnable;
  const uint8_t* const _ptrRTCPDataEnd;

  bool _validPacket;
  const uint8_t* _ptrRTCPData;
  const uint8_t* _ptrRTCPBlockEnd;

  ParseState _state;
  uint8_t _numberOfBlocks;
  size_t num_skipped_blocks_;

  RTCPPacketTypes _packetType;
  RTCPPacket _packet;
  std::unique_ptr<webrtc::rtcp::RtcpPacket> rtcp_packet_;
};

class RTCPPacketIterator {
 public:
  RTCPPacketIterator(uint8_t* rtcpData, size_t rtcpDataLength);
  ~RTCPPacketIterator();

  const RtcpCommonHeader* Begin();
  const RtcpCommonHeader* Iterate();
  const RtcpCommonHeader* Current();

 private:
  uint8_t* const _ptrBegin;
  uint8_t* const _ptrEnd;

  uint8_t* _ptrBlock;

  RtcpCommonHeader _header;
};
}  // namespace RTCPUtility
}  // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_UTILITY_H_
