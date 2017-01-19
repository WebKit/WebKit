/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

#include <assert.h>
#include <math.h>   // ceil
#include <string.h> // memcpy

#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"

namespace webrtc {
namespace {
constexpr uint64_t kMaxBitrateBps = std::numeric_limits<uint32_t>::max();
}  // namespace

namespace RTCPUtility {

NackStats::NackStats()
    : max_sequence_number_(0),
      requests_(0),
      unique_requests_(0) {}

NackStats::~NackStats() {}

void NackStats::ReportRequest(uint16_t sequence_number) {
  if (requests_ == 0 ||
      webrtc::IsNewerSequenceNumber(sequence_number, max_sequence_number_)) {
    max_sequence_number_ =  sequence_number;
    ++unique_requests_;
  }
  ++requests_;
}

uint32_t MidNtp(uint32_t ntp_sec, uint32_t ntp_frac) {
  return (ntp_sec << 16) + (ntp_frac >> 16);
}
}  // namespace RTCPUtility

// RTCPParserV2 : currently read only
RTCPUtility::RTCPParserV2::RTCPParserV2(const uint8_t* rtcpData,
                                        size_t rtcpDataLength,
                                        bool rtcpReducedSizeEnable)
    : _ptrRTCPDataBegin(rtcpData),
      _RTCPReducedSizeEnable(rtcpReducedSizeEnable),
      _ptrRTCPDataEnd(rtcpData + rtcpDataLength),
      _validPacket(false),
      _ptrRTCPData(rtcpData),
      _ptrRTCPBlockEnd(NULL),
      _state(ParseState::State_TopLevel),
      _numberOfBlocks(0),
      num_skipped_blocks_(0),
      _packetType(RTCPPacketTypes::kInvalid) {
  Validate();
}

RTCPUtility::RTCPParserV2::~RTCPParserV2() {
}

ptrdiff_t
RTCPUtility::RTCPParserV2::LengthLeft() const
{
    return (_ptrRTCPDataEnd- _ptrRTCPData);
}

RTCPUtility::RTCPPacketTypes
RTCPUtility::RTCPParserV2::PacketType() const
{
    return _packetType;
}

const RTCPUtility::RTCPPacket&
RTCPUtility::RTCPParserV2::Packet() const
{
    return _packet;
}

rtcp::RtcpPacket* RTCPUtility::RTCPParserV2::ReleaseRtcpPacket() {
  return rtcp_packet_.release();
}
RTCPUtility::RTCPPacketTypes
RTCPUtility::RTCPParserV2::Begin()
{
    _ptrRTCPData = _ptrRTCPDataBegin;

    return Iterate();
}

RTCPUtility::RTCPPacketTypes
RTCPUtility::RTCPParserV2::Iterate()
{
    // Reset packet type
  _packetType = RTCPPacketTypes::kInvalid;

    if (IsValid())
    {
        switch (_state)
        {
          case ParseState::State_TopLevel:
            IterateTopLevel();
            break;
          case ParseState::State_ReportBlockItem:
            IterateReportBlockItem();
            break;
          case ParseState::State_SDESChunk:
            IterateSDESChunk();
            break;
          case ParseState::State_BYEItem:
            IterateBYEItem();
            break;
          case ParseState::State_ExtendedJitterItem:
            IterateExtendedJitterItem();
            break;
          case ParseState::State_RTPFB_NACKItem:
            IterateNACKItem();
            break;
          case ParseState::State_RTPFB_TMMBRItem:
            IterateTMMBRItem();
            break;
          case ParseState::State_RTPFB_TMMBNItem:
            IterateTMMBNItem();
            break;
          case ParseState::State_PSFB_SLIItem:
            IterateSLIItem();
            break;
          case ParseState::State_PSFB_RPSIItem:
            IterateRPSIItem();
            break;
          case ParseState::State_PSFB_FIRItem:
            IterateFIRItem();
            break;
          case ParseState::State_PSFB_AppItem:
            IteratePsfbAppItem();
            break;
          case ParseState::State_PSFB_REMBItem:
            IteratePsfbREMBItem();
            break;
          case ParseState::State_XRItem:
            IterateXrItem();
            break;
          case ParseState::State_XR_DLLRItem:
            IterateXrDlrrItem();
            break;
          case ParseState::State_AppItem:
            IterateAppItem();
            break;
        default:
          RTC_NOTREACHED() << "Invalid state!";
            break;
        }
    }
    return _packetType;
}

void
RTCPUtility::RTCPParserV2::IterateTopLevel()
{
    for (;;)
    {
      RtcpCommonHeader header;
      if (_ptrRTCPDataEnd <= _ptrRTCPData)
        return;

      if (!RtcpParseCommonHeader(_ptrRTCPData, _ptrRTCPDataEnd - _ptrRTCPData,
                                 &header)) {
            return;
        }
        _ptrRTCPBlockEnd = _ptrRTCPData + header.BlockSize();
        if (_ptrRTCPBlockEnd > _ptrRTCPDataEnd)
        {
          ++num_skipped_blocks_;
            return;
        }

        switch (header.packet_type) {
        case PT_SR:
        {
            // number of Report blocks
            _numberOfBlocks = header.count_or_format;
            ParseSR();
            return;
        }
        case PT_RR:
        {
            // number of Report blocks
            _numberOfBlocks = header.count_or_format;
            ParseRR();
            return;
        }
        case PT_SDES:
        {
            // number of SDES blocks
            _numberOfBlocks = header.count_or_format;
            const bool ok = ParseSDES();
            if (!ok)
            {
                // Nothing supported found, continue to next block!
                break;
            }
            return;
        }
        case PT_BYE:
        {
          _numberOfBlocks = header.count_or_format;
            const bool ok = ParseBYE();
            if (!ok)
            {
                // Nothing supported found, continue to next block!
                break;
            }
            return;
        }
        case PT_IJ:
        {
            // number of Report blocks
            _numberOfBlocks = header.count_or_format;
            ParseIJ();
            return;
        }
        case PT_RTPFB:
          FALLTHROUGH();
        case PT_PSFB:
        {
          if (!ParseFBCommon(header)) {
            // Nothing supported found, continue to next block!
            EndCurrentBlock();
            break;
          }
          return;
        }
        case PT_APP:
        {
            const bool ok = ParseAPP(header);
            if (!ok)
            {
                // Nothing supported found, continue to next block!
                break;
            }
            return;
        }
        case PT_XR:
        {
            const bool ok = ParseXr();
            if (!ok)
            {
                // Nothing supported found, continue to next block!
                break;
            }
            return;
        }
        default:
            // Not supported! Skip!
            ++num_skipped_blocks_;
            EndCurrentBlock();
            break;
        }
    }
}

void
RTCPUtility::RTCPParserV2::IterateXrItem()
{
    const bool success = ParseXrItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateXrDlrrItem()
{
    const bool success = ParseXrDlrrItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateReportBlockItem()
{
    const bool success = ParseReportBlockItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateSDESChunk()
{
    const bool success = ParseSDESChunk();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateBYEItem()
{
    const bool success = ParseBYEItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateExtendedJitterItem()
{
    const bool success = ParseIJItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateNACKItem()
{
    const bool success = ParseNACKItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateTMMBRItem()
{
    const bool success = ParseTMMBRItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateTMMBNItem()
{
    const bool success = ParseTMMBNItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateSLIItem()
{
    const bool success = ParseSLIItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateRPSIItem()
{
    const bool success = ParseRPSIItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateFIRItem()
{
    const bool success = ParseFIRItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IteratePsfbAppItem()
{
    const bool success = ParsePsfbAppItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IteratePsfbREMBItem()
{
    const bool success = ParsePsfbREMBItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateAppItem()
{
    const bool success = ParseAPPItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::Validate()
{
  if (_ptrRTCPData == nullptr)
    return;  // NOT VALID

  RtcpCommonHeader header;
  if (_ptrRTCPDataEnd <= _ptrRTCPDataBegin)
    return;  // NOT VALID

  if (!RtcpParseCommonHeader(_ptrRTCPDataBegin,
                             _ptrRTCPDataEnd - _ptrRTCPDataBegin, &header))
    return;  // NOT VALID!

  // * if (!reducedSize) : first packet must be RR or SR.
  //
  // * The padding bit (P) should be zero for the first packet of a
  //   compound RTCP packet because padding should only be applied,
  //   if it is needed, to the last packet. (NOT CHECKED!)
  //
  // * The length fields of the individual RTCP packets must add up
  //   to the overall length of the compound RTCP packet as
  //   received.  This is a fairly strong check. (NOT CHECKED!)

  if (!_RTCPReducedSizeEnable) {
    if ((header.packet_type != PT_SR) && (header.packet_type != PT_RR))
      return;  // NOT VALID
  }

  _validPacket = true;
}

bool
RTCPUtility::RTCPParserV2::IsValid() const
{
    return _validPacket;
}

void
RTCPUtility::RTCPParserV2::EndCurrentBlock()
{
    _ptrRTCPData = _ptrRTCPBlockEnd;
}

//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|    IC   |      PT       |             length            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// Common header for all RTCP packets, 4 octets.

bool RTCPUtility::RtcpParseCommonHeader(const uint8_t* packet,
                                        size_t size_bytes,
                                        RtcpCommonHeader* parsed_header) {
  RTC_DCHECK(parsed_header != nullptr);
  if (size_bytes < RtcpCommonHeader::kHeaderSizeBytes) {
    LOG(LS_WARNING) << "Too little data (" << size_bytes << " byte"
                    << (size_bytes != 1 ? "s" : "")
                    << ") remaining in buffer to parse RTCP header (4 bytes).";
    return false;
  }

  const uint8_t kRtcpVersion = 2;
  uint8_t version = packet[0] >> 6;
  if (version != kRtcpVersion) {
    LOG(LS_WARNING) << "Invalid RTCP header: Version must be "
                    << static_cast<int>(kRtcpVersion) << " but was "
                    << static_cast<int>(version);
    return false;
  }

  bool has_padding = (packet[0] & 0x20) != 0;
  uint8_t format = packet[0] & 0x1F;
  uint8_t packet_type = packet[1];
  size_t packet_size_words =
      ByteReader<uint16_t>::ReadBigEndian(&packet[2]) + 1;

  if (size_bytes < packet_size_words * 4) {
    LOG(LS_WARNING) << "Buffer too small (" << size_bytes
                    << " bytes) to fit an RtcpPacket of " << packet_size_words
                    << " 32bit words.";
    return false;
  }

  size_t payload_size = packet_size_words * 4;
  size_t padding_bytes = 0;
  if (has_padding) {
    if (payload_size <= RtcpCommonHeader::kHeaderSizeBytes) {
      LOG(LS_WARNING) << "Invalid RTCP header: Padding bit set but 0 payload "
                         "size specified.";
      return false;
    }

    padding_bytes = packet[payload_size - 1];
    if (RtcpCommonHeader::kHeaderSizeBytes + padding_bytes > payload_size) {
      LOG(LS_WARNING) << "Invalid RTCP header: Too many padding bytes ("
                      << padding_bytes << ") for a packet size of "
                      << payload_size << "bytes.";
      return false;
    }
    payload_size -= padding_bytes;
  }
  payload_size -= RtcpCommonHeader::kHeaderSizeBytes;

  parsed_header->version = kRtcpVersion;
  parsed_header->count_or_format = format;
  parsed_header->packet_type = packet_type;
  parsed_header->payload_size_bytes = payload_size;
  parsed_header->padding_bytes = padding_bytes;

  return true;
}

bool
RTCPUtility::RTCPParserV2::ParseRR()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
        return false;
    }


    _ptrRTCPData += 4; // Skip header

    _packetType = RTCPPacketTypes::kRr;

    _packet.RR.SenderSSRC = *_ptrRTCPData++ << 24;
    _packet.RR.SenderSSRC += *_ptrRTCPData++ << 16;
    _packet.RR.SenderSSRC += *_ptrRTCPData++ << 8;
    _packet.RR.SenderSSRC += *_ptrRTCPData++;

    _packet.RR.NumberOfReportBlocks = _numberOfBlocks;

    // State transition
    _state = ParseState::State_ReportBlockItem;

    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseSR()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 28)
    {
        EndCurrentBlock();
        return false;
    }

    _ptrRTCPData += 4; // Skip header

    _packetType = RTCPPacketTypes::kSr;

    _packet.SR.SenderSSRC = *_ptrRTCPData++ << 24;
    _packet.SR.SenderSSRC += *_ptrRTCPData++ << 16;
    _packet.SR.SenderSSRC += *_ptrRTCPData++ << 8;
    _packet.SR.SenderSSRC += *_ptrRTCPData++;

    _packet.SR.NTPMostSignificant = *_ptrRTCPData++ << 24;
    _packet.SR.NTPMostSignificant += *_ptrRTCPData++ << 16;
    _packet.SR.NTPMostSignificant += *_ptrRTCPData++ << 8;
    _packet.SR.NTPMostSignificant += *_ptrRTCPData++;

    _packet.SR.NTPLeastSignificant = *_ptrRTCPData++ << 24;
    _packet.SR.NTPLeastSignificant += *_ptrRTCPData++ << 16;
    _packet.SR.NTPLeastSignificant += *_ptrRTCPData++ << 8;
    _packet.SR.NTPLeastSignificant += *_ptrRTCPData++;

    _packet.SR.RTPTimestamp = *_ptrRTCPData++ << 24;
    _packet.SR.RTPTimestamp += *_ptrRTCPData++ << 16;
    _packet.SR.RTPTimestamp += *_ptrRTCPData++ << 8;
    _packet.SR.RTPTimestamp += *_ptrRTCPData++;

    _packet.SR.SenderPacketCount = *_ptrRTCPData++ << 24;
    _packet.SR.SenderPacketCount += *_ptrRTCPData++ << 16;
    _packet.SR.SenderPacketCount += *_ptrRTCPData++ << 8;
    _packet.SR.SenderPacketCount += *_ptrRTCPData++;

    _packet.SR.SenderOctetCount = *_ptrRTCPData++ << 24;
    _packet.SR.SenderOctetCount += *_ptrRTCPData++ << 16;
    _packet.SR.SenderOctetCount += *_ptrRTCPData++ << 8;
    _packet.SR.SenderOctetCount += *_ptrRTCPData++;

    _packet.SR.NumberOfReportBlocks = _numberOfBlocks;

    // State transition
    if(_numberOfBlocks != 0)
    {
      _state = ParseState::State_ReportBlockItem;
    }else
    {
        // don't go to state report block item if 0 report blocks
      _state = ParseState::State_TopLevel;
        EndCurrentBlock();
    }
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseReportBlockItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 24 || _numberOfBlocks <= 0)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _packet.ReportBlockItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.SSRC += *_ptrRTCPData++;

    _packet.ReportBlockItem.FractionLost = *_ptrRTCPData++;

    _packet.ReportBlockItem.CumulativeNumOfPacketsLost = *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.CumulativeNumOfPacketsLost += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.CumulativeNumOfPacketsLost += *_ptrRTCPData++;

    _packet.ReportBlockItem.ExtendedHighestSequenceNumber = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.ExtendedHighestSequenceNumber += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.ExtendedHighestSequenceNumber += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.ExtendedHighestSequenceNumber += *_ptrRTCPData++;

    _packet.ReportBlockItem.Jitter = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.Jitter += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.Jitter += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.Jitter += *_ptrRTCPData++;

    _packet.ReportBlockItem.LastSR = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.LastSR += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.LastSR += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.LastSR += *_ptrRTCPData++;

    _packet.ReportBlockItem.DelayLastSR = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.DelayLastSR += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.DelayLastSR += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.DelayLastSR += *_ptrRTCPData++;

    _numberOfBlocks--;
    _packetType = RTCPPacketTypes::kReportBlockItem;
    return true;
}

/* From RFC 5450: Transmission Time Offsets in RTP Streams.
      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 hdr |V=2|P|    RC   |   PT=IJ=195   |             length            |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                      inter-arrival jitter                     |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     .                                                               .
     .                                                               .
     .                                                               .
     |                      inter-arrival jitter                     |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

bool
RTCPUtility::RTCPParserV2::ParseIJ()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
        return false;
    }

    _ptrRTCPData += 4; // Skip header

    _packetType = RTCPPacketTypes::kExtendedIj;

    // State transition
    _state = ParseState::State_ExtendedJitterItem;
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseIJItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4 || _numberOfBlocks <= 0)
    {
      _state = ParseState::State_TopLevel;
        EndCurrentBlock();
        return false;
    }

    _packet.ExtendedJitterReportItem.Jitter = *_ptrRTCPData++ << 24;
    _packet.ExtendedJitterReportItem.Jitter += *_ptrRTCPData++ << 16;
    _packet.ExtendedJitterReportItem.Jitter += *_ptrRTCPData++ << 8;
    _packet.ExtendedJitterReportItem.Jitter += *_ptrRTCPData++;

    _numberOfBlocks--;
    _packetType = RTCPPacketTypes::kExtendedIjItem;
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseSDES()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _ptrRTCPData += 4; // Skip header

    _state = ParseState::State_SDESChunk;
    _packetType = RTCPPacketTypes::kSdes;
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseSDESChunk()
{
    if(_numberOfBlocks <= 0)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _numberOfBlocks--;

    // Find CName item in a SDES chunk.
    while (_ptrRTCPData < _ptrRTCPBlockEnd)
    {
        const ptrdiff_t dataLen = _ptrRTCPBlockEnd - _ptrRTCPData;
        if (dataLen < 4)
        {
          _state = ParseState::State_TopLevel;

            EndCurrentBlock();
            return false;
        }

        uint32_t SSRC = *_ptrRTCPData++ << 24;
        SSRC += *_ptrRTCPData++ << 16;
        SSRC += *_ptrRTCPData++ << 8;
        SSRC += *_ptrRTCPData++;

        const bool foundCname = ParseSDESItem();
        if (foundCname)
        {
            _packet.CName.SenderSSRC = SSRC; // Add SSRC
            return true;
        }
    }
    _state = ParseState::State_TopLevel;

    EndCurrentBlock();
    return false;
}

bool
RTCPUtility::RTCPParserV2::ParseSDESItem()
{
    // Find CName
    // Only the CNAME item is mandatory. RFC 3550 page 46
    bool foundCName = false;

    size_t itemOctetsRead = 0;
    while (_ptrRTCPData < _ptrRTCPBlockEnd)
    {
        const uint8_t tag = *_ptrRTCPData++;
        ++itemOctetsRead;

        if (tag == 0)
        {
            // End tag! 4 oct aligned
            while ((itemOctetsRead++ % 4) != 0)
            {
                ++_ptrRTCPData;
            }
            return foundCName;
        }

        if (_ptrRTCPData < _ptrRTCPBlockEnd)
        {
            const uint8_t len = *_ptrRTCPData++;
            ++itemOctetsRead;

            if (tag == 1)
            {
                // CNAME

                // Sanity
                if ((_ptrRTCPData + len) >= _ptrRTCPBlockEnd)
                {
                  _state = ParseState::State_TopLevel;

                    EndCurrentBlock();
                    return false;
                }
                uint8_t i = 0;
                for (; i < len; ++i)
                {
                    const uint8_t c = _ptrRTCPData[i];
                    if ((c < ' ') || (c > '{') || (c == '%') || (c == '\\'))
                    {
                        // Illegal char
                      _state = ParseState::State_TopLevel;

                        EndCurrentBlock();
                        return false;
                    }
                    _packet.CName.CName[i] = c;
                }
                // Make sure we are null terminated.
                _packet.CName.CName[i] = 0;
                _packetType = RTCPPacketTypes::kSdesChunk;

                foundCName = true;
            }
            _ptrRTCPData += len;
            itemOctetsRead += len;
        }
    }

    // No end tag found!
    _state = ParseState::State_TopLevel;

    EndCurrentBlock();
    return false;
}

bool
RTCPUtility::RTCPParserV2::ParseBYE()
{
    _ptrRTCPData += 4; // Skip header

    _state = ParseState::State_BYEItem;

    return ParseBYEItem();
}

bool
RTCPUtility::RTCPParserV2::ParseBYEItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length < 4 || _numberOfBlocks == 0)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = RTCPPacketTypes::kBye;

    _packet.BYE.SenderSSRC = *_ptrRTCPData++ << 24;
    _packet.BYE.SenderSSRC += *_ptrRTCPData++ << 16;
    _packet.BYE.SenderSSRC += *_ptrRTCPData++ << 8;
    _packet.BYE.SenderSSRC += *_ptrRTCPData++;

    // we can have several CSRCs attached

    // sanity
    if(length >= 4*_numberOfBlocks)
    {
        _ptrRTCPData += (_numberOfBlocks -1)*4;
    }
    _numberOfBlocks = 0;

    return true;
}
/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|reserved |   PT=XR=207   |             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                              SSRC                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   :                         report blocks                         :
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
bool RTCPUtility::RTCPParserV2::ParseXr()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length < 8)
    {
        EndCurrentBlock();
        return false;
    }

    _ptrRTCPData += 4; // Skip header

    _packet.XR.OriginatorSSRC = *_ptrRTCPData++ << 24;
    _packet.XR.OriginatorSSRC += *_ptrRTCPData++ << 16;
    _packet.XR.OriginatorSSRC += *_ptrRTCPData++ << 8;
    _packet.XR.OriginatorSSRC += *_ptrRTCPData++;

    _packetType = RTCPPacketTypes::kXrHeader;
    _state = ParseState::State_XRItem;
    return true;
}

/*  Extended report block format (RFC 3611).
    BT: block type.
    block length: length of report block in 32-bits words minus one (including
                  the header).
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      BT       | type-specific |         block length          |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    :             type-specific block contents                      :
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
bool RTCPUtility::RTCPParserV2::ParseXrItem() {
  const int kBlockHeaderLengthInBytes = 4;
  const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
  if (length < kBlockHeaderLengthInBytes) {
    _state = ParseState::State_TopLevel;
    EndCurrentBlock();
    return false;
  }

  uint8_t block_type = *_ptrRTCPData++;
  _ptrRTCPData++;  // Ignore reserved.

  uint16_t block_length_in_4bytes = *_ptrRTCPData++ << 8;
  block_length_in_4bytes += *_ptrRTCPData++;

  switch (block_type) {
    case kBtReceiverReferenceTime:
      return ParseXrReceiverReferenceTimeItem(block_length_in_4bytes);
    case kBtDlrr:
      return ParseXrDlrr(block_length_in_4bytes);
    case kBtVoipMetric:
      return ParseXrVoipMetricItem(block_length_in_4bytes);
    default:
      return ParseXrUnsupportedBlockType(block_length_in_4bytes);
  }
}

/*  Receiver Reference Time Report Block.
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=4      |   reserved    |       block length = 2        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |              NTP timestamp, most significant word             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |             NTP timestamp, least significant word             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
bool RTCPUtility::RTCPParserV2::ParseXrReceiverReferenceTimeItem(
    int block_length_4bytes) {
  const int kBlockLengthIn4Bytes = 2;
  const int kBlockLengthInBytes = kBlockLengthIn4Bytes * 4;
  const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
  if (block_length_4bytes != kBlockLengthIn4Bytes ||
      length < kBlockLengthInBytes) {
    _state = ParseState::State_TopLevel;
    EndCurrentBlock();
    return false;
  }

  _packet.XRReceiverReferenceTimeItem.NTPMostSignificant = *_ptrRTCPData++<<24;
  _packet.XRReceiverReferenceTimeItem.NTPMostSignificant+= *_ptrRTCPData++<<16;
  _packet.XRReceiverReferenceTimeItem.NTPMostSignificant+= *_ptrRTCPData++<<8;
  _packet.XRReceiverReferenceTimeItem.NTPMostSignificant+= *_ptrRTCPData++;

  _packet.XRReceiverReferenceTimeItem.NTPLeastSignificant = *_ptrRTCPData++<<24;
  _packet.XRReceiverReferenceTimeItem.NTPLeastSignificant+= *_ptrRTCPData++<<16;
  _packet.XRReceiverReferenceTimeItem.NTPLeastSignificant+= *_ptrRTCPData++<<8;
  _packet.XRReceiverReferenceTimeItem.NTPLeastSignificant+= *_ptrRTCPData++;

  _packetType = RTCPPacketTypes::kXrReceiverReferenceTime;
  _state = ParseState::State_XRItem;
  return true;
}

/*  DLRR Report Block.
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=5      |   reserved    |         block length          |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |                 SSRC_1 (SSRC of first receiver)               | sub-
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
   |                         last RR (LRR)                         |   1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                   delay since last RR (DLRR)                  |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |                 SSRC_2 (SSRC of second receiver)              | sub-
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
   :                               ...                             :   2
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/
bool RTCPUtility::RTCPParserV2::ParseXrDlrr(int block_length_4bytes) {
  const int kSubBlockLengthIn4Bytes = 3;
  if (block_length_4bytes < 0 ||
      (block_length_4bytes % kSubBlockLengthIn4Bytes) != 0) {
    _state = ParseState::State_TopLevel;
    EndCurrentBlock();
    return false;
  }
  _packetType = RTCPPacketTypes::kXrDlrrReportBlock;
  _state = ParseState::State_XR_DLLRItem;
  _numberOfBlocks = block_length_4bytes / kSubBlockLengthIn4Bytes;
  return true;
}

bool RTCPUtility::RTCPParserV2::ParseXrDlrrItem() {
  if (_numberOfBlocks == 0) {
    _state = ParseState::State_XRItem;
    return false;
  }
  const int kSubBlockLengthInBytes = 12;
  const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
  if (length < kSubBlockLengthInBytes) {
    _state = ParseState::State_TopLevel;
    EndCurrentBlock();
    return false;
  }

  _packet.XRDLRRReportBlockItem.SSRC = *_ptrRTCPData++ << 24;
  _packet.XRDLRRReportBlockItem.SSRC += *_ptrRTCPData++ << 16;
  _packet.XRDLRRReportBlockItem.SSRC += *_ptrRTCPData++ << 8;
  _packet.XRDLRRReportBlockItem.SSRC += *_ptrRTCPData++;

  _packet.XRDLRRReportBlockItem.LastRR = *_ptrRTCPData++ << 24;
  _packet.XRDLRRReportBlockItem.LastRR += *_ptrRTCPData++ << 16;
  _packet.XRDLRRReportBlockItem.LastRR += *_ptrRTCPData++ << 8;
  _packet.XRDLRRReportBlockItem.LastRR += *_ptrRTCPData++;

  _packet.XRDLRRReportBlockItem.DelayLastRR = *_ptrRTCPData++ << 24;
  _packet.XRDLRRReportBlockItem.DelayLastRR += *_ptrRTCPData++ << 16;
  _packet.XRDLRRReportBlockItem.DelayLastRR += *_ptrRTCPData++ << 8;
  _packet.XRDLRRReportBlockItem.DelayLastRR += *_ptrRTCPData++;

  _packetType = RTCPPacketTypes::kXrDlrrReportBlockItem;
  --_numberOfBlocks;
  _state = ParseState::State_XR_DLLRItem;
  return true;
}
/*  VoIP Metrics Report Block.
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=7      |   reserved    |       block length = 8        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        SSRC of source                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   loss rate   | discard rate  | burst density |  gap density  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       burst duration          |         gap duration          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     round trip delay          |       end system delay        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | signal level  |  noise level  |     RERL      |     Gmin      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   R factor    | ext. R factor |    MOS-LQ     |    MOS-CQ     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   RX config   |   reserved    |          JB nominal           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          JB maximum           |          JB abs max           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

bool RTCPUtility::RTCPParserV2::ParseXrVoipMetricItem(int block_length_4bytes) {
  const int kBlockLengthIn4Bytes = 8;
  const int kBlockLengthInBytes = kBlockLengthIn4Bytes * 4;
  const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
  if (block_length_4bytes != kBlockLengthIn4Bytes ||
      length < kBlockLengthInBytes) {
    _state = ParseState::State_TopLevel;
    EndCurrentBlock();
    return false;
  }

  _packet.XRVOIPMetricItem.SSRC = *_ptrRTCPData++ << 24;
  _packet.XRVOIPMetricItem.SSRC += *_ptrRTCPData++ << 16;
  _packet.XRVOIPMetricItem.SSRC += *_ptrRTCPData++ << 8;
  _packet.XRVOIPMetricItem.SSRC += *_ptrRTCPData++;

  _packet.XRVOIPMetricItem.lossRate = *_ptrRTCPData++;
  _packet.XRVOIPMetricItem.discardRate = *_ptrRTCPData++;
  _packet.XRVOIPMetricItem.burstDensity = *_ptrRTCPData++;
  _packet.XRVOIPMetricItem.gapDensity = *_ptrRTCPData++;

  _packet.XRVOIPMetricItem.burstDuration = *_ptrRTCPData++ << 8;
  _packet.XRVOIPMetricItem.burstDuration += *_ptrRTCPData++;

  _packet.XRVOIPMetricItem.gapDuration = *_ptrRTCPData++ << 8;
  _packet.XRVOIPMetricItem.gapDuration += *_ptrRTCPData++;

  _packet.XRVOIPMetricItem.roundTripDelay = *_ptrRTCPData++ << 8;
  _packet.XRVOIPMetricItem.roundTripDelay += *_ptrRTCPData++;

  _packet.XRVOIPMetricItem.endSystemDelay = *_ptrRTCPData++ << 8;
  _packet.XRVOIPMetricItem.endSystemDelay += *_ptrRTCPData++;

  _packet.XRVOIPMetricItem.signalLevel = *_ptrRTCPData++;
  _packet.XRVOIPMetricItem.noiseLevel = *_ptrRTCPData++;
  _packet.XRVOIPMetricItem.RERL = *_ptrRTCPData++;
  _packet.XRVOIPMetricItem.Gmin = *_ptrRTCPData++;
  _packet.XRVOIPMetricItem.Rfactor = *_ptrRTCPData++;
  _packet.XRVOIPMetricItem.extRfactor = *_ptrRTCPData++;
  _packet.XRVOIPMetricItem.MOSLQ = *_ptrRTCPData++;
  _packet.XRVOIPMetricItem.MOSCQ = *_ptrRTCPData++;
  _packet.XRVOIPMetricItem.RXconfig = *_ptrRTCPData++;
  _ptrRTCPData++; // skip reserved

  _packet.XRVOIPMetricItem.JBnominal = *_ptrRTCPData++ << 8;
  _packet.XRVOIPMetricItem.JBnominal += *_ptrRTCPData++;

  _packet.XRVOIPMetricItem.JBmax = *_ptrRTCPData++ << 8;
  _packet.XRVOIPMetricItem.JBmax += *_ptrRTCPData++;

  _packet.XRVOIPMetricItem.JBabsMax = *_ptrRTCPData++ << 8;
  _packet.XRVOIPMetricItem.JBabsMax += *_ptrRTCPData++;

  _packetType = RTCPPacketTypes::kXrVoipMetric;
  _state = ParseState::State_XRItem;
  return true;
}

bool RTCPUtility::RTCPParserV2::ParseXrUnsupportedBlockType(
    int block_length_4bytes) {
  const int32_t kBlockLengthInBytes = block_length_4bytes * 4;
  const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
  if (length < kBlockLengthInBytes) {
    _state = ParseState::State_TopLevel;
    EndCurrentBlock();
    return false;
  }
  // Skip block.
  _ptrRTCPData += kBlockLengthInBytes;
  _state = ParseState::State_XRItem;
  return false;
}

bool RTCPUtility::RTCPParserV2::ParseFBCommon(const RtcpCommonHeader& header) {
  RTC_CHECK((header.packet_type == PT_RTPFB) ||
            (header.packet_type == PT_PSFB));  // Parser logic check

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    // 4 * 3, RFC4585 section 6.1
    if (length < 12) {
      LOG(LS_WARNING)
          << "Invalid RTCP packet: Too little data (" << length
          << " bytes) left in buffer to parse a 12 byte RTPFB/PSFB message.";
        return false;
    }

    _ptrRTCPData += 4; // Skip RTCP header

    uint32_t senderSSRC = ByteReader<uint32_t>::ReadBigEndian(_ptrRTCPData);
    _ptrRTCPData += 4;

    uint32_t mediaSSRC = ByteReader<uint32_t>::ReadBigEndian(_ptrRTCPData);
    _ptrRTCPData += 4;

    if (header.packet_type == PT_RTPFB) {
        // Transport layer feedback

        switch (header.count_or_format) {
        case 1:
        {
            // NACK
          _packetType = RTCPPacketTypes::kRtpfbNack;
            _packet.NACK.SenderSSRC = senderSSRC;
            _packet.NACK.MediaSSRC  = mediaSSRC;

            _state = ParseState::State_RTPFB_NACKItem;

            return true;
        }
        case 3:
        {
            // TMMBR
          _packetType = RTCPPacketTypes::kRtpfbTmmbr;
            _packet.TMMBR.SenderSSRC = senderSSRC;
            _packet.TMMBR.MediaSSRC  = mediaSSRC;

            _state = ParseState::State_RTPFB_TMMBRItem;

            return true;
        }
        case 4:
        {
            // TMMBN
          _packetType = RTCPPacketTypes::kRtpfbTmmbn;
            _packet.TMMBN.SenderSSRC = senderSSRC;
            _packet.TMMBN.MediaSSRC  = mediaSSRC;

            _state = ParseState::State_RTPFB_TMMBNItem;

            return true;
        }
        case 5:
         {
            // RTCP-SR-REQ Rapid Synchronisation of RTP Flows
            // draft-perkins-avt-rapid-rtp-sync-03.txt
            // trigger a new RTCP SR
          _packetType = RTCPPacketTypes::kRtpfbSrReq;

            // Note: No state transition, SR REQ is empty!
            return true;
        }
        case 15: {
          rtcp_packet_ =
              rtcp::TransportFeedback::ParseFrom(_ptrRTCPData - 12, length);
          // Since we parse the whole packet here, keep the TopLevel state and
          // just end the current block.
          EndCurrentBlock();
          if (rtcp_packet_.get()) {
            _packetType = RTCPPacketTypes::kTransportFeedback;
            return true;
          }
          break;
        }
        default:
            break;
        }
        // Unsupported RTPFB message. Skip and move to next block.
        ++num_skipped_blocks_;
        return false;
    } else if (header.packet_type == PT_PSFB) {
        // Payload specific feedback
        switch (header.count_or_format) {
        case 1:
            // PLI
          _packetType = RTCPPacketTypes::kPsfbPli;
            _packet.PLI.SenderSSRC = senderSSRC;
            _packet.PLI.MediaSSRC  = mediaSSRC;

            // Note: No state transition, PLI FCI is empty!
            return true;
        case 2:
            // SLI
          _packetType = RTCPPacketTypes::kPsfbSli;
            _packet.SLI.SenderSSRC = senderSSRC;
            _packet.SLI.MediaSSRC  = mediaSSRC;

            _state = ParseState::State_PSFB_SLIItem;

            return true;
        case 3:
          _packetType = RTCPPacketTypes::kPsfbRpsi;
            _packet.RPSI.SenderSSRC = senderSSRC;
            _packet.RPSI.MediaSSRC  = mediaSSRC;

            _state = ParseState::State_PSFB_RPSIItem;
            return true;
        case 4:
            // FIR
          _packetType = RTCPPacketTypes::kPsfbFir;
            _packet.FIR.SenderSSRC = senderSSRC;
            _packet.FIR.MediaSSRC  = mediaSSRC;

            _state = ParseState::State_PSFB_FIRItem;
            return true;
        case 15:
          _packetType = RTCPPacketTypes::kPsfbApp;
            _packet.PSFBAPP.SenderSSRC = senderSSRC;
            _packet.PSFBAPP.MediaSSRC  = mediaSSRC;

            _state = ParseState::State_PSFB_AppItem;
            return true;
        default:
            break;
        }

        return false;
    }
    else
    {
      RTC_NOTREACHED();
        return false;
    }
}

bool RTCPUtility::RTCPParserV2::ParseRPSIItem() {

    // RFC 4585 6.3.3.  Reference Picture Selection Indication (RPSI).
    //
    //  0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |      PB       |0| Payload Type|    Native RPSI bit string     |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |   defined per codec          ...                | Padding (0) |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4) {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if (length > 2 + RTCP_RPSI_DATA_SIZE) {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }


    uint8_t padding_bits = *_ptrRTCPData++;
    _packet.RPSI.PayloadType = *_ptrRTCPData++;

    if (padding_bits > static_cast<uint16_t>(length - 2) * 8) {
      _state = ParseState::State_TopLevel;

      EndCurrentBlock();
      return false;
    }

    _packetType = RTCPPacketTypes::kPsfbRpsiItem;

    memcpy(_packet.RPSI.NativeBitString, _ptrRTCPData, length - 2);
    _ptrRTCPData += length - 2;

    _packet.RPSI.NumberOfValidBits =
        static_cast<uint16_t>(length - 2) * 8 - padding_bits;
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseNACKItem()
{
    // RFC 4585 6.2.1. Generic NACK

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = RTCPPacketTypes::kRtpfbNackItem;

    _packet.NACKItem.PacketID = *_ptrRTCPData++ << 8;
    _packet.NACKItem.PacketID += *_ptrRTCPData++;

    _packet.NACKItem.BitMask = *_ptrRTCPData++ << 8;
    _packet.NACKItem.BitMask += *_ptrRTCPData++;

    return true;
}

bool
RTCPUtility::RTCPParserV2::ParsePsfbAppItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if(*_ptrRTCPData++ != 'R')
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if(*_ptrRTCPData++ != 'E')
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if(*_ptrRTCPData++ != 'M')
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if(*_ptrRTCPData++ != 'B')
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _packetType = RTCPPacketTypes::kPsfbRemb;
    _state = ParseState::State_PSFB_REMBItem;
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParsePsfbREMBItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packet.REMBItem.NumberOfSSRCs = *_ptrRTCPData++;
    const uint8_t exp = (_ptrRTCPData[0] >> 2) & 0x3F;

    uint64_t mantissa = (_ptrRTCPData[0] & 0x03) << 16;
    mantissa += (_ptrRTCPData[1] << 8);
    mantissa += (_ptrRTCPData[2]);

    _ptrRTCPData += 3; // Fwd read data
    uint64_t bitrate_bps = (mantissa << exp);
    bool shift_overflow = exp > 0 && (mantissa >> (64 - exp)) != 0;
    if (shift_overflow || bitrate_bps > kMaxBitrateBps) {
      LOG(LS_ERROR) << "Unhandled remb bitrate value : " << mantissa
                    << "*2^" << static_cast<int>(exp);
      _state = ParseState::State_TopLevel;
      EndCurrentBlock();
      return false;
    }
    _packet.REMBItem.BitRate = bitrate_bps;

    const ptrdiff_t length_ssrcs = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length_ssrcs < 4 * _packet.REMBItem.NumberOfSSRCs)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = RTCPPacketTypes::kPsfbRembItem;

    for (int i = 0; i < _packet.REMBItem.NumberOfSSRCs; i++)
    {
        _packet.REMBItem.SSRCs[i] = *_ptrRTCPData++ << 24;
        _packet.REMBItem.SSRCs[i] += *_ptrRTCPData++ << 16;
        _packet.REMBItem.SSRCs[i] += *_ptrRTCPData++ << 8;
        _packet.REMBItem.SSRCs[i] += *_ptrRTCPData++;
    }
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseTMMBRItem()
{
    // RFC 5104 4.2.1. Temporary Maximum Media Stream Bit Rate Request (TMMBR)

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = RTCPPacketTypes::kRtpfbTmmbrItem;

    _packet.TMMBRItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.TMMBRItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.TMMBRItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.TMMBRItem.SSRC += *_ptrRTCPData++;

    uint8_t exp = (_ptrRTCPData[0] >> 2) & 0x3F;

    uint64_t mantissa = (_ptrRTCPData[0] & 0x03) << 15;
    mantissa += (_ptrRTCPData[1] << 7);
    mantissa += (_ptrRTCPData[2] >> 1) & 0x7F;

    uint32_t measuredOH = (_ptrRTCPData[2] & 0x01) << 8;
    measuredOH += _ptrRTCPData[3];

    _ptrRTCPData += 4; // Fwd read data

    uint64_t bitrate_bps = (mantissa << exp);
    bool shift_overflow = exp > 0 && (mantissa >> (64 - exp)) != 0;
    if (shift_overflow || bitrate_bps > kMaxBitrateBps) {
      LOG(LS_ERROR) << "Unhandled tmmbr bitrate value : " << mantissa
                    << "*2^" << static_cast<int>(exp);
      _state = ParseState::State_TopLevel;
      EndCurrentBlock();
      return false;
    }

    _packet.TMMBRItem.MaxTotalMediaBitRate = bitrate_bps / 1000;
    _packet.TMMBRItem.MeasuredOverhead     = measuredOH;

    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseTMMBNItem()
{
    // RFC 5104 4.2.2. Temporary Maximum Media Stream Bit Rate Notification (TMMBN)

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = RTCPPacketTypes::kRtpfbTmmbnItem;

    _packet.TMMBNItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.TMMBNItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.TMMBNItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.TMMBNItem.SSRC += *_ptrRTCPData++;

    uint8_t exp = (_ptrRTCPData[0] >> 2) & 0x3F;

    uint64_t mantissa = (_ptrRTCPData[0] & 0x03) << 15;
    mantissa += (_ptrRTCPData[1] << 7);
    mantissa += (_ptrRTCPData[2] >> 1) & 0x7F;

    uint32_t measuredOH = (_ptrRTCPData[2] & 0x01) << 8;
    measuredOH += _ptrRTCPData[3];

    _ptrRTCPData += 4; // Fwd read data

    uint64_t bitrate_bps = (mantissa << exp);
    bool shift_overflow = exp > 0 && (mantissa >> (64 - exp)) != 0;
    if (shift_overflow || bitrate_bps > kMaxBitrateBps) {
      LOG(LS_ERROR) << "Unhandled tmmbn bitrate value : " << mantissa
                    << "*2^" << static_cast<int>(exp);
      _state = ParseState::State_TopLevel;
      EndCurrentBlock();
      return false;
    }

    _packet.TMMBNItem.MaxTotalMediaBitRate = bitrate_bps / 1000;
    _packet.TMMBNItem.MeasuredOverhead     = measuredOH;

    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseSLIItem()
{
    // RFC 5104 6.3.2.  Slice Loss Indication (SLI)
    /*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |            First        |        Number           | PictureID |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _packetType = RTCPPacketTypes::kPsfbSliItem;

    uint32_t buffer;
    buffer = *_ptrRTCPData++ << 24;
    buffer += *_ptrRTCPData++ << 16;
    buffer += *_ptrRTCPData++ << 8;
    buffer += *_ptrRTCPData++;

    _packet.SLIItem.FirstMB = uint16_t((buffer>>19) & 0x1fff);
    _packet.SLIItem.NumberOfMB = uint16_t((buffer>>6) & 0x1fff);
    _packet.SLIItem.PictureId = uint8_t(buffer & 0x3f);

    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseFIRItem()
{
    // RFC 5104 4.3.1. Full Intra Request (FIR)

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = RTCPPacketTypes::kPsfbFirItem;

    _packet.FIRItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.FIRItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.FIRItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.FIRItem.SSRC += *_ptrRTCPData++;

    _packet.FIRItem.CommandSequenceNumber = *_ptrRTCPData++;
    _ptrRTCPData += 3; // Skip "Reserved" bytes.
    return true;
}

bool RTCPUtility::RTCPParserV2::ParseAPP(const RtcpCommonHeader& header) {
    ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 12) // 4 * 3, RFC 3550 6.7 APP: Application-Defined RTCP Packet
    {
        EndCurrentBlock();
        return false;
    }

    _ptrRTCPData += 4; // Skip RTCP header

    uint32_t senderSSRC = *_ptrRTCPData++ << 24;
    senderSSRC += *_ptrRTCPData++ << 16;
    senderSSRC += *_ptrRTCPData++ << 8;
    senderSSRC += *_ptrRTCPData++;

    uint32_t name = *_ptrRTCPData++ << 24;
    name += *_ptrRTCPData++ << 16;
    name += *_ptrRTCPData++ << 8;
    name += *_ptrRTCPData++;

    length  = _ptrRTCPBlockEnd - _ptrRTCPData;

    _packetType = RTCPPacketTypes::kApp;

    _packet.APP.SubType = header.count_or_format;
    _packet.APP.Name = name;

    _state = ParseState::State_AppItem;
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseAPPItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length < 4)
    {
      _state = ParseState::State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _packetType = RTCPPacketTypes::kAppItem;

    if(length > kRtcpAppCode_DATA_SIZE)
    {
        memcpy(_packet.APP.Data, _ptrRTCPData, kRtcpAppCode_DATA_SIZE);
        _packet.APP.Size = kRtcpAppCode_DATA_SIZE;
        _ptrRTCPData += kRtcpAppCode_DATA_SIZE;
    }else
    {
        memcpy(_packet.APP.Data, _ptrRTCPData, length);
        _packet.APP.Size = (uint16_t)length;
        _ptrRTCPData += length;
    }
    return true;
}

size_t RTCPUtility::RTCPParserV2::NumSkippedBlocks() const {
  return num_skipped_blocks_;
}

RTCPUtility::RTCPPacketIterator::RTCPPacketIterator(uint8_t* rtcpData,
                                                    size_t rtcpDataLength)
    : _ptrBegin(rtcpData),
      _ptrEnd(rtcpData + rtcpDataLength),
      _ptrBlock(NULL) {
  memset(&_header, 0, sizeof(_header));
}

RTCPUtility::RTCPPacketIterator::~RTCPPacketIterator() {
}

const RTCPUtility::RtcpCommonHeader* RTCPUtility::RTCPPacketIterator::Begin() {
    _ptrBlock = _ptrBegin;

    return Iterate();
}

const RTCPUtility::RtcpCommonHeader*
RTCPUtility::RTCPPacketIterator::Iterate() {
  if ((_ptrEnd <= _ptrBlock) ||
      !RtcpParseCommonHeader(_ptrBlock, _ptrEnd - _ptrBlock, &_header)) {
    _ptrBlock = nullptr;
    return nullptr;
  }
  _ptrBlock += _header.BlockSize();

  if (_ptrBlock > _ptrEnd) {
    _ptrBlock = nullptr;
    return nullptr;
  }

  return &_header;
}

const RTCPUtility::RtcpCommonHeader*
RTCPUtility::RTCPPacketIterator::Current() {
    if (!_ptrBlock)
    {
        return NULL;
    }

    return &_header;
}
}  // namespace webrtc
