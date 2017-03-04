/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <sstream>
#include <string>

#include "gflags/gflags.h"
#include "webrtc/base/checks.h"
#include "webrtc/call/call.h"
#include "webrtc/common_types.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log_parser.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/fir.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/pli.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rapid_resync_request.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rpsi.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sli.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbn.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

namespace {

DEFINE_bool(noconfig, true, "Excludes stream configurations.");
DEFINE_bool(noincoming, false, "Excludes incoming packets.");
DEFINE_bool(nooutgoing, false, "Excludes outgoing packets.");
// TODO(terelius): Note that the media type doesn't work with outgoing packets.
DEFINE_bool(noaudio, false, "Excludes audio packets.");
// TODO(terelius): Note that the media type doesn't work with outgoing packets.
DEFINE_bool(novideo, false, "Excludes video packets.");
// TODO(terelius): Note that the media type doesn't work with outgoing packets.
DEFINE_bool(nodata, false, "Excludes data packets.");
DEFINE_bool(nortp, false, "Excludes RTP packets.");
DEFINE_bool(nortcp, false, "Excludes RTCP packets.");
// TODO(terelius): Allow a list of SSRCs.
DEFINE_string(ssrc,
              "",
              "Print only packets with this SSRC (decimal or hex, the latter "
              "starting with 0x).");

static uint32_t filtered_ssrc = 0;

// Parses the input string for a valid SSRC. If a valid SSRC is found, it is
// written to the static global variable |filtered_ssrc|, and true is returned.
// Otherwise, false is returned.
// The empty string must be validated as true, because it is the default value
// of the command-line flag. In this case, no value is written to the output
// variable.
bool ParseSsrc(std::string str) {
  // If the input string starts with 0x or 0X it indicates a hexadecimal number.
  auto read_mode = std::dec;
  if (str.size() > 2 &&
      (str.substr(0, 2) == "0x" || str.substr(0, 2) == "0X")) {
    read_mode = std::hex;
    str = str.substr(2);
  }
  std::stringstream ss(str);
  ss >> read_mode >> filtered_ssrc;
  return str.empty() || (!ss.fail() && ss.eof());
}

bool ExcludePacket(webrtc::PacketDirection direction,
                   webrtc::MediaType media_type,
                   uint32_t packet_ssrc) {
  if (FLAGS_nooutgoing && direction == webrtc::kOutgoingPacket)
    return true;
  if (FLAGS_noincoming && direction == webrtc::kIncomingPacket)
    return true;
  if (FLAGS_noaudio && media_type == webrtc::MediaType::AUDIO)
    return true;
  if (FLAGS_novideo && media_type == webrtc::MediaType::VIDEO)
    return true;
  if (FLAGS_nodata && media_type == webrtc::MediaType::DATA)
    return true;
  if (!FLAGS_ssrc.empty() && packet_ssrc != filtered_ssrc)
    return true;
  return false;
}

const char* StreamInfo(webrtc::PacketDirection direction,
                       webrtc::MediaType media_type) {
  if (direction == webrtc::kOutgoingPacket) {
    if (media_type == webrtc::MediaType::AUDIO)
      return "(out,audio)";
    else if (media_type == webrtc::MediaType::VIDEO)
      return "(out,video)";
    else if (media_type == webrtc::MediaType::DATA)
      return "(out,data)";
    else
      return "(out)";
  }
  if (direction == webrtc::kIncomingPacket) {
    if (media_type == webrtc::MediaType::AUDIO)
      return "(in,audio)";
    else if (media_type == webrtc::MediaType::VIDEO)
      return "(in,video)";
    else if (media_type == webrtc::MediaType::DATA)
      return "(in,data)";
    else
      return "(in)";
  }
  return "(unknown)";
}

void PrintSenderReport(const webrtc::rtcp::CommonHeader& rtcp_block,
                       uint64_t log_timestamp,
                       webrtc::PacketDirection direction,
                       webrtc::MediaType media_type) {
  webrtc::rtcp::SenderReport sr;
  if (!sr.Parse(rtcp_block))
    return;
  if (ExcludePacket(direction, media_type, sr.sender_ssrc()))
    return;
  std::cout << log_timestamp << "\t"
            << "RTCP_SR" << StreamInfo(direction, media_type)
            << "\tssrc=" << sr.sender_ssrc()
            << "\ttimestamp=" << sr.rtp_timestamp() << std::endl;
}

void PrintReceiverReport(const webrtc::rtcp::CommonHeader& rtcp_block,
                         uint64_t log_timestamp,
                         webrtc::PacketDirection direction,
                         webrtc::MediaType media_type) {
  webrtc::rtcp::ReceiverReport rr;
  if (!rr.Parse(rtcp_block))
    return;
  if (ExcludePacket(direction, media_type, rr.sender_ssrc()))
    return;
  std::cout << log_timestamp << "\t"
            << "RTCP_RR" << StreamInfo(direction, media_type)
            << "\tssrc=" << rr.sender_ssrc() << std::endl;
}

void PrintXr(const webrtc::rtcp::CommonHeader& rtcp_block,
             uint64_t log_timestamp,
             webrtc::PacketDirection direction,
             webrtc::MediaType media_type) {
  webrtc::rtcp::ExtendedReports xr;
  if (!xr.Parse(rtcp_block))
    return;
  if (ExcludePacket(direction, media_type, xr.sender_ssrc()))
    return;
  std::cout << log_timestamp << "\t"
            << "RTCP_XR" << StreamInfo(direction, media_type)
            << "\tssrc=" << xr.sender_ssrc() << std::endl;
}

void PrintSdes(const webrtc::rtcp::CommonHeader& rtcp_block,
               uint64_t log_timestamp,
               webrtc::PacketDirection direction,
               webrtc::MediaType media_type) {
  std::cout << log_timestamp << "\t"
            << "RTCP_SDES" << StreamInfo(direction, media_type) << std::endl;
  RTC_NOTREACHED() << "SDES should have been redacted when writing the log";
}

void PrintBye(const webrtc::rtcp::CommonHeader& rtcp_block,
              uint64_t log_timestamp,
              webrtc::PacketDirection direction,
              webrtc::MediaType media_type) {
  webrtc::rtcp::Bye bye;
  if (!bye.Parse(rtcp_block))
    return;
  if (ExcludePacket(direction, media_type, bye.sender_ssrc()))
    return;
  std::cout << log_timestamp << "\t"
            << "RTCP_BYE" << StreamInfo(direction, media_type)
            << "\tssrc=" << bye.sender_ssrc() << std::endl;
}

void PrintRtpFeedback(const webrtc::rtcp::CommonHeader& rtcp_block,
                      uint64_t log_timestamp,
                      webrtc::PacketDirection direction,
                      webrtc::MediaType media_type) {
  switch (rtcp_block.fmt()) {
    case webrtc::rtcp::Nack::kFeedbackMessageType: {
      webrtc::rtcp::Nack nack;
      if (!nack.Parse(rtcp_block))
        return;
      if (ExcludePacket(direction, media_type, nack.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_NACK" << StreamInfo(direction, media_type)
                << "\tssrc=" << nack.sender_ssrc() << std::endl;
      break;
    }
    case webrtc::rtcp::Tmmbr::kFeedbackMessageType: {
      webrtc::rtcp::Tmmbr tmmbr;
      if (!tmmbr.Parse(rtcp_block))
        return;
      if (ExcludePacket(direction, media_type, tmmbr.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_TMMBR" << StreamInfo(direction, media_type)
                << "\tssrc=" << tmmbr.sender_ssrc() << std::endl;
      break;
    }
    case webrtc::rtcp::Tmmbn::kFeedbackMessageType: {
      webrtc::rtcp::Tmmbn tmmbn;
      if (!tmmbn.Parse(rtcp_block))
        return;
      if (ExcludePacket(direction, media_type, tmmbn.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_TMMBN" << StreamInfo(direction, media_type)
                << "\tssrc=" << tmmbn.sender_ssrc() << std::endl;
      break;
    }
    case webrtc::rtcp::RapidResyncRequest::kFeedbackMessageType: {
      webrtc::rtcp::RapidResyncRequest sr_req;
      if (!sr_req.Parse(rtcp_block))
        return;
      if (ExcludePacket(direction, media_type, sr_req.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_SRREQ" << StreamInfo(direction, media_type)
                << "\tssrc=" << sr_req.sender_ssrc() << std::endl;
      break;
    }
    case webrtc::rtcp::TransportFeedback::kFeedbackMessageType: {
      webrtc::rtcp::TransportFeedback transport_feedback;
      if (!transport_feedback.Parse(rtcp_block))
        return;
      if (ExcludePacket(direction, media_type,
                        transport_feedback.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_NEWFB" << StreamInfo(direction, media_type)
                << "\tssrc=" << transport_feedback.sender_ssrc() << std::endl;
      break;
    }
    default:
      break;
  }
}

void PrintPsFeedback(const webrtc::rtcp::CommonHeader& rtcp_block,
                     uint64_t log_timestamp,
                     webrtc::PacketDirection direction,
                     webrtc::MediaType media_type) {
  switch (rtcp_block.fmt()) {
    case webrtc::rtcp::Pli::kFeedbackMessageType: {
      webrtc::rtcp::Pli pli;
      if (!pli.Parse(rtcp_block))
        return;
      if (ExcludePacket(direction, media_type, pli.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_PLI" << StreamInfo(direction, media_type)
                << "\tssrc=" << pli.sender_ssrc() << std::endl;
      break;
    }
    case webrtc::rtcp::Sli::kFeedbackMessageType: {
      webrtc::rtcp::Sli sli;
      if (!sli.Parse(rtcp_block))
        return;
      if (ExcludePacket(direction, media_type, sli.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_SLI" << StreamInfo(direction, media_type)
                << "\tssrc=" << sli.sender_ssrc() << std::endl;
      break;
    }
    case webrtc::rtcp::Rpsi::kFeedbackMessageType: {
      webrtc::rtcp::Rpsi rpsi;
      if (!rpsi.Parse(rtcp_block))
        return;
      if (ExcludePacket(direction, media_type, rpsi.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_RPSI" << StreamInfo(direction, media_type)
                << "\tssrc=" << rpsi.sender_ssrc() << std::endl;
      break;
    }
    case webrtc::rtcp::Fir::kFeedbackMessageType: {
      webrtc::rtcp::Fir fir;
      if (!fir.Parse(rtcp_block))
        return;
      if (ExcludePacket(direction, media_type, fir.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_FIR" << StreamInfo(direction, media_type)
                << "\tssrc=" << fir.sender_ssrc() << std::endl;
      break;
    }
    case webrtc::rtcp::Remb::kFeedbackMessageType: {
      webrtc::rtcp::Remb remb;
      if (!remb.Parse(rtcp_block))
        return;
      if (ExcludePacket(direction, media_type, remb.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_REMB" << StreamInfo(direction, media_type)
                << "\tssrc=" << remb.sender_ssrc() << std::endl;
      break;
    }
    default:
      break;
  }
}

}  // namespace

// This utility will print basic information about each packet to stdout.
// Note that parser will assert if the protobuf event is missing some required
// fields and we attempt to access them. We don't handle this at the moment.
int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Tool for printing packet information from an RtcEventLog as text.\n"
      "Run " +
      program_name +
      " --helpshort for usage.\n"
      "Example usage:\n" +
      program_name + " input.rel\n";
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (argc != 2) {
    std::cout << google::ProgramUsage();
    return 0;
  }
  std::string input_file = argv[1];

  if (!FLAGS_ssrc.empty())
    RTC_CHECK(ParseSsrc(FLAGS_ssrc)) << "Flag verification has failed.";

  webrtc::ParsedRtcEventLog parsed_stream;
  if (!parsed_stream.ParseFile(input_file)) {
    std::cerr << "Error while parsing input file: " << input_file << std::endl;
    return -1;
  }

  for (size_t i = 0; i < parsed_stream.GetNumberOfEvents(); i++) {
    if (!FLAGS_noconfig && !FLAGS_novideo && !FLAGS_noincoming &&
        parsed_stream.GetEventType(i) ==
            webrtc::ParsedRtcEventLog::VIDEO_RECEIVER_CONFIG_EVENT) {
      webrtc::VideoReceiveStream::Config config(nullptr);
      parsed_stream.GetVideoReceiveConfig(i, &config);
      std::cout << parsed_stream.GetTimestamp(i) << "\tVIDEO_RECV_CONFIG"
                << "\tssrc=" << config.rtp.remote_ssrc
                << "\tfeedback_ssrc=" << config.rtp.local_ssrc << std::endl;
    }
    if (!FLAGS_noconfig && !FLAGS_novideo && !FLAGS_nooutgoing &&
        parsed_stream.GetEventType(i) ==
            webrtc::ParsedRtcEventLog::VIDEO_SENDER_CONFIG_EVENT) {
      webrtc::VideoSendStream::Config config(nullptr);
      parsed_stream.GetVideoSendConfig(i, &config);
      std::cout << parsed_stream.GetTimestamp(i) << "\tVIDEO_SEND_CONFIG";
      std::cout << "\tssrcs=";
      for (const auto& ssrc : config.rtp.ssrcs)
        std::cout << ssrc << ',';
      std::cout << "\trtx_ssrcs=";
      for (const auto& ssrc : config.rtp.rtx.ssrcs)
        std::cout << ssrc << ',';
      std::cout << std::endl;
    }
    if (!FLAGS_noconfig && !FLAGS_noaudio && !FLAGS_noincoming &&
        parsed_stream.GetEventType(i) ==
            webrtc::ParsedRtcEventLog::AUDIO_RECEIVER_CONFIG_EVENT) {
      webrtc::AudioReceiveStream::Config config;
      parsed_stream.GetAudioReceiveConfig(i, &config);
      std::cout << parsed_stream.GetTimestamp(i) << "\tAUDIO_RECV_CONFIG"
                << "\tssrc=" << config.rtp.remote_ssrc
                << "\tfeedback_ssrc=" << config.rtp.local_ssrc << std::endl;
    }
    if (!FLAGS_noconfig && !FLAGS_noaudio && !FLAGS_nooutgoing &&
        parsed_stream.GetEventType(i) ==
            webrtc::ParsedRtcEventLog::AUDIO_SENDER_CONFIG_EVENT) {
      webrtc::AudioSendStream::Config config(nullptr);
      parsed_stream.GetAudioSendConfig(i, &config);
      std::cout << parsed_stream.GetTimestamp(i) << "\tAUDIO_SEND_CONFIG"
                << "\tssrc=" << config.rtp.ssrc << std::endl;
    }
    if (!FLAGS_nortp &&
        parsed_stream.GetEventType(i) == webrtc::ParsedRtcEventLog::RTP_EVENT) {
      size_t header_length;
      size_t total_length;
      uint8_t header[IP_PACKET_SIZE];
      webrtc::PacketDirection direction;
      webrtc::MediaType media_type;
      parsed_stream.GetRtpHeader(i, &direction, &media_type, header,
                                 &header_length, &total_length);

      // Parse header to get SSRC and RTP time.
      webrtc::RtpUtility::RtpHeaderParser rtp_parser(header, header_length);
      webrtc::RTPHeader parsed_header;
      rtp_parser.Parse(&parsed_header);

      if (ExcludePacket(direction, media_type, parsed_header.ssrc))
        continue;

      std::cout << parsed_stream.GetTimestamp(i) << "\tRTP"
                << StreamInfo(direction, media_type)
                << "\tssrc=" << parsed_header.ssrc
                << "\ttimestamp=" << parsed_header.timestamp << std::endl;
    }
    if (!FLAGS_nortcp &&
        parsed_stream.GetEventType(i) ==
            webrtc::ParsedRtcEventLog::RTCP_EVENT) {
      size_t length;
      uint8_t packet[IP_PACKET_SIZE];
      webrtc::PacketDirection direction;
      webrtc::MediaType media_type;
      parsed_stream.GetRtcpPacket(i, &direction, &media_type, packet, &length);

      webrtc::rtcp::CommonHeader rtcp_block;
      const uint8_t* packet_end = packet + length;
      for (const uint8_t* next_block = packet; next_block != packet_end;
           next_block = rtcp_block.NextPacket()) {
        ptrdiff_t remaining_blocks_size = packet_end - next_block;
        RTC_DCHECK_GT(remaining_blocks_size, 0);
        if (!rtcp_block.Parse(next_block, remaining_blocks_size)) {
          break;
        }

        uint64_t log_timestamp = parsed_stream.GetTimestamp(i);
        switch (rtcp_block.type()) {
          case webrtc::rtcp::SenderReport::kPacketType:
            PrintSenderReport(rtcp_block, log_timestamp, direction, media_type);
            break;
          case webrtc::rtcp::ReceiverReport::kPacketType:
            PrintReceiverReport(rtcp_block, log_timestamp, direction,
                                media_type);
            break;
          case webrtc::rtcp::Sdes::kPacketType:
            PrintSdes(rtcp_block, log_timestamp, direction, media_type);
            break;
          case webrtc::rtcp::ExtendedReports::kPacketType:
            PrintXr(rtcp_block, log_timestamp, direction, media_type);
            break;
          case webrtc::rtcp::Bye::kPacketType:
            PrintBye(rtcp_block, log_timestamp, direction, media_type);
            break;
          case webrtc::rtcp::Rtpfb::kPacketType:
            PrintRtpFeedback(rtcp_block, log_timestamp, direction, media_type);
            break;
          case webrtc::rtcp::Psfb::kPacketType:
            PrintPsFeedback(rtcp_block, log_timestamp, direction, media_type);
            break;
          default:
            break;
        }
      }
    }
  }
  return 0;
}
