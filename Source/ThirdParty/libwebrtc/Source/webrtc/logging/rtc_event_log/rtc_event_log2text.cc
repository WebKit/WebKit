/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <iomanip>  // setfill, setw
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>  // pair

#include "call/video_config.h"
#include "common_types.h"  // NOLINT(build/include)
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "modules/rtp_rtcp/source/rtcp_packet/fir.h"
#include "modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "modules/rtp_rtcp/source/rtcp_packet/pli.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rapid_resync_request.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/tmmbn.h"
#include "modules/rtp_rtcp/source/rtcp_packet/tmmbr.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/flags.h"
#include "rtc_base/logging.h"

namespace {

DEFINE_bool(unknown, true, "Use --nounknown to exclude unknown events.");
DEFINE_bool(startstop, true, "Use --nostartstop to exclude start/stop events.");
DEFINE_bool(config, true, "Use --noconfig to exclude stream configurations.");
DEFINE_bool(bwe, true, "Use --nobwe to exclude BWE events.");
DEFINE_bool(incoming, true, "Use --noincoming to exclude incoming packets.");
DEFINE_bool(outgoing, true, "Use --nooutgoing to exclude packets.");
// TODO(terelius): Note that the media type doesn't work with outgoing packets.
DEFINE_bool(audio, true, "Use --noaudio to exclude audio packets.");
// TODO(terelius): Note that the media type doesn't work with outgoing packets.
DEFINE_bool(video, true, "Use --novideo to exclude video packets.");
// TODO(terelius): Note that the media type doesn't work with outgoing packets.
DEFINE_bool(data, true, "Use --nodata to exclude data packets.");
DEFINE_bool(rtp, true, "Use --nortp to exclude RTP packets.");
DEFINE_bool(rtcp, true, "Use --nortcp to exclude RTCP packets.");
DEFINE_bool(playout, true, "Use --noplayout to exclude audio playout events.");
DEFINE_bool(ana, true, "Use --noana to exclude ANA events.");
DEFINE_bool(probe, true, "Use --noprobe to exclude probe events.");

DEFINE_bool(print_full_packets,
            false,
            "Print the full RTP headers and RTCP packets in hex.");

// TODO(terelius): Allow a list of SSRCs.
DEFINE_string(ssrc,
              "",
              "Print only packets with this SSRC (decimal or hex, the latter "
              "starting with 0x).");
DEFINE_bool(help, false, "Prints this message.");

using MediaType = webrtc::ParsedRtcEventLog::MediaType;

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
                   MediaType media_type,
                   uint32_t packet_ssrc) {
  if (!FLAG_outgoing && direction == webrtc::kOutgoingPacket)
    return true;
  if (!FLAG_incoming && direction == webrtc::kIncomingPacket)
    return true;
  if (!FLAG_audio && media_type == MediaType::AUDIO)
    return true;
  if (!FLAG_video && media_type == MediaType::VIDEO)
    return true;
  if (!FLAG_data && media_type == MediaType::DATA)
    return true;
  if (strlen(FLAG_ssrc) > 0 && packet_ssrc != filtered_ssrc)
    return true;
  return false;
}

const char* StreamInfo(webrtc::PacketDirection direction,
                       MediaType media_type) {
  if (direction == webrtc::kOutgoingPacket) {
    if (media_type == MediaType::AUDIO)
      return "(out,audio)";
    else if (media_type == MediaType::VIDEO)
      return "(out,video)";
    else if (media_type == MediaType::DATA)
      return "(out,data)";
    else
      return "(out)";
  }
  if (direction == webrtc::kIncomingPacket) {
    if (media_type == MediaType::AUDIO)
      return "(in,audio)";
    else if (media_type == MediaType::VIDEO)
      return "(in,video)";
    else if (media_type == MediaType::DATA)
      return "(in,data)";
    else
      return "(in)";
  }
  return "(unknown)";
}

// Return default values for header extensions, to use on streams without stored
// mapping data. Currently this only applies to audio streams, since the mapping
// is not stored in the event log.
// TODO(ivoc): Remove this once this mapping is stored in the event log for
//             audio streams. Tracking bug: webrtc:6399
webrtc::RtpHeaderExtensionMap GetDefaultHeaderExtensionMap() {
  webrtc::RtpHeaderExtensionMap default_map;
  default_map.Register<webrtc::AudioLevel>(
      webrtc::RtpExtension::kAudioLevelDefaultId);
  default_map.Register<webrtc::TransmissionOffset>(
      webrtc::RtpExtension::kTimestampOffsetDefaultId);
  default_map.Register<webrtc::AbsoluteSendTime>(
      webrtc::RtpExtension::kAbsSendTimeDefaultId);
  default_map.Register<webrtc::VideoOrientation>(
      webrtc::RtpExtension::kVideoRotationDefaultId);
  default_map.Register<webrtc::VideoContentTypeExtension>(
      webrtc::RtpExtension::kVideoContentTypeDefaultId);
  default_map.Register<webrtc::VideoTimingExtension>(
      webrtc::RtpExtension::kVideoTimingDefaultId);
  default_map.Register<webrtc::TransportSequenceNumber>(
      webrtc::RtpExtension::kTransportSequenceNumberDefaultId);
  default_map.Register<webrtc::PlayoutDelayLimits>(
      webrtc::RtpExtension::kPlayoutDelayDefaultId);
  return default_map;
}

void PrintSenderReport(const webrtc::ParsedRtcEventLog& parsed_stream,
                       const webrtc::rtcp::CommonHeader& rtcp_block,
                       uint64_t log_timestamp,
                       webrtc::PacketDirection direction) {
  webrtc::rtcp::SenderReport sr;
  if (!sr.Parse(rtcp_block))
    return;
  MediaType media_type =
      parsed_stream.GetMediaType(sr.sender_ssrc(), direction);
  if (ExcludePacket(direction, media_type, sr.sender_ssrc()))
    return;
  std::cout << log_timestamp << "\t"
            << "RTCP_SR" << StreamInfo(direction, media_type)
            << "\tssrc=" << sr.sender_ssrc()
            << "\ttimestamp=" << sr.rtp_timestamp() << std::endl;
}

void PrintReceiverReport(const webrtc::ParsedRtcEventLog& parsed_stream,
                         const webrtc::rtcp::CommonHeader& rtcp_block,
                         uint64_t log_timestamp,
                         webrtc::PacketDirection direction) {
  webrtc::rtcp::ReceiverReport rr;
  if (!rr.Parse(rtcp_block))
    return;
  MediaType media_type =
      parsed_stream.GetMediaType(rr.sender_ssrc(), direction);
  if (ExcludePacket(direction, media_type, rr.sender_ssrc()))
    return;
  std::cout << log_timestamp << "\t"
            << "RTCP_RR" << StreamInfo(direction, media_type)
            << "\tssrc=" << rr.sender_ssrc() << std::endl;
}

void PrintXr(const webrtc::ParsedRtcEventLog& parsed_stream,
             const webrtc::rtcp::CommonHeader& rtcp_block,
             uint64_t log_timestamp,
             webrtc::PacketDirection direction) {
  webrtc::rtcp::ExtendedReports xr;
  if (!xr.Parse(rtcp_block))
    return;
  MediaType media_type =
      parsed_stream.GetMediaType(xr.sender_ssrc(), direction);
  if (ExcludePacket(direction, media_type, xr.sender_ssrc()))
    return;
  std::cout << log_timestamp << "\t"
            << "RTCP_XR" << StreamInfo(direction, media_type)
            << "\tssrc=" << xr.sender_ssrc() << std::endl;
}

void PrintSdes(const webrtc::rtcp::CommonHeader& rtcp_block,
               uint64_t log_timestamp,
               webrtc::PacketDirection direction) {
  std::cout << log_timestamp << "\t"
            << "RTCP_SDES" << StreamInfo(direction, MediaType::ANY)
            << std::endl;
  RTC_NOTREACHED() << "SDES should have been redacted when writing the log";
}

void PrintBye(const webrtc::ParsedRtcEventLog& parsed_stream,
              const webrtc::rtcp::CommonHeader& rtcp_block,
              uint64_t log_timestamp,
              webrtc::PacketDirection direction) {
  webrtc::rtcp::Bye bye;
  if (!bye.Parse(rtcp_block))
    return;
  MediaType media_type =
      parsed_stream.GetMediaType(bye.sender_ssrc(), direction);
  if (ExcludePacket(direction, media_type, bye.sender_ssrc()))
    return;
  std::cout << log_timestamp << "\t"
            << "RTCP_BYE" << StreamInfo(direction, media_type)
            << "\tssrc=" << bye.sender_ssrc() << std::endl;
}

void PrintRtpFeedback(const webrtc::ParsedRtcEventLog& parsed_stream,
                      const webrtc::rtcp::CommonHeader& rtcp_block,
                      uint64_t log_timestamp,
                      webrtc::PacketDirection direction) {
  switch (rtcp_block.fmt()) {
    case webrtc::rtcp::Nack::kFeedbackMessageType: {
      webrtc::rtcp::Nack nack;
      if (!nack.Parse(rtcp_block))
        return;
      MediaType media_type =
          parsed_stream.GetMediaType(nack.sender_ssrc(), direction);
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
      MediaType media_type =
          parsed_stream.GetMediaType(tmmbr.sender_ssrc(), direction);
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
      MediaType media_type =
          parsed_stream.GetMediaType(tmmbn.sender_ssrc(), direction);
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
      MediaType media_type =
          parsed_stream.GetMediaType(sr_req.sender_ssrc(), direction);
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
      MediaType media_type = parsed_stream.GetMediaType(
          transport_feedback.sender_ssrc(), direction);
      if (ExcludePacket(direction, media_type,
                        transport_feedback.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_NEWFB" << StreamInfo(direction, media_type)
                << "\tsender_ssrc=" << transport_feedback.sender_ssrc()
                << "\tmedia_ssrc=" << transport_feedback.media_ssrc()
                << std::endl;
      break;
    }
    default:
      std::cout << log_timestamp << "\t"
                << "RTCP_RTPFB(UNKNOWN)" << std::endl;
      break;
  }
}

void PrintPsFeedback(const webrtc::ParsedRtcEventLog& parsed_stream,
                     const webrtc::rtcp::CommonHeader& rtcp_block,
                     uint64_t log_timestamp,
                     webrtc::PacketDirection direction) {
  switch (rtcp_block.fmt()) {
    case webrtc::rtcp::Pli::kFeedbackMessageType: {
      webrtc::rtcp::Pli pli;
      if (!pli.Parse(rtcp_block))
        return;
      MediaType media_type =
          parsed_stream.GetMediaType(pli.sender_ssrc(), direction);
      if (ExcludePacket(direction, media_type, pli.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_PLI" << StreamInfo(direction, media_type)
                << "\tssrc=" << pli.sender_ssrc() << std::endl;
      break;
    }
    case webrtc::rtcp::Fir::kFeedbackMessageType: {
      webrtc::rtcp::Fir fir;
      if (!fir.Parse(rtcp_block))
        return;
      MediaType media_type =
          parsed_stream.GetMediaType(fir.sender_ssrc(), direction);
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
      MediaType media_type =
          parsed_stream.GetMediaType(remb.sender_ssrc(), direction);
      if (ExcludePacket(direction, media_type, remb.sender_ssrc()))
        return;
      std::cout << log_timestamp << "\t"
                << "RTCP_REMB" << StreamInfo(direction, media_type)
                << "\tssrc=" << remb.sender_ssrc() << std::endl;
      break;
    }
    default:
      std::cout << log_timestamp << "\t"
                << "RTCP_PSFB(UNKNOWN)" << std::endl;
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
      " --help for usage.\n"
      "Example usage:\n" +
      program_name + " input.rel\n";
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true) ||
      FLAG_help || argc != 2) {
    std::cout << usage;
    if (FLAG_help) {
      rtc::FlagList::Print(nullptr, false);
      return 0;
    }
    return 1;
  }
  std::string input_file = argv[1];

  if (strlen(FLAG_ssrc) > 0)
    RTC_CHECK(ParseSsrc(FLAG_ssrc)) << "Flag verification has failed.";

  webrtc::RtpHeaderExtensionMap default_map = GetDefaultHeaderExtensionMap();
  bool default_map_used = false;

  webrtc::ParsedRtcEventLog parsed_stream;
  if (!parsed_stream.ParseFile(input_file)) {
    std::cerr << "Error while parsing input file: " << input_file << std::endl;
    return -1;
  }

  for (size_t i = 0; i < parsed_stream.GetNumberOfEvents(); i++) {
    bool event_recognized = false;
    switch (parsed_stream.GetEventType(i)) {
      case webrtc::ParsedRtcEventLog::UNKNOWN_EVENT: {
        if (FLAG_unknown) {
          std::cout << parsed_stream.GetTimestamp(i) << "\tUNKNOWN_EVENT"
                    << std::endl;
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::LOG_START: {
        if (FLAG_startstop) {
          std::cout << parsed_stream.GetTimestamp(i) << "\tLOG_START"
                    << std::endl;
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::LOG_END: {
        if (FLAG_startstop) {
          std::cout << parsed_stream.GetTimestamp(i) << "\tLOG_END"
                    << std::endl;
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::RTP_EVENT: {
        if (FLAG_rtp) {
          size_t header_length;
          size_t total_length;
          uint8_t header[IP_PACKET_SIZE];
          webrtc::PacketDirection direction;
          webrtc::RtpHeaderExtensionMap* extension_map =
              parsed_stream.GetRtpHeader(i, &direction, header, &header_length,
                                         &total_length, nullptr);

          if (extension_map == nullptr) {
            extension_map = &default_map;
            if (!default_map_used)
              RTC_LOG(LS_WARNING) << "Using default header extension map";
            default_map_used = true;
          }

          // Parse header to get SSRC and RTP time.
          webrtc::RtpUtility::RtpHeaderParser rtp_parser(header, header_length);
          webrtc::RTPHeader parsed_header;
          rtp_parser.Parse(&parsed_header, extension_map);
          MediaType media_type =
              parsed_stream.GetMediaType(parsed_header.ssrc, direction);

          if (ExcludePacket(direction, media_type, parsed_header.ssrc)) {
            event_recognized = true;
            break;
          }

          std::cout << parsed_stream.GetTimestamp(i) << "\tRTP"
                    << StreamInfo(direction, media_type)
                    << "\tssrc=" << parsed_header.ssrc
                    << "\ttimestamp=" << parsed_header.timestamp;
          if (parsed_header.extension.hasAbsoluteSendTime) {
            std::cout << "\tAbsSendTime="
                      << parsed_header.extension.absoluteSendTime;
          }
          if (parsed_header.extension.hasVideoContentType) {
            std::cout << "\tContentType="
                      << static_cast<int>(
                             parsed_header.extension.videoContentType);
          }
          if (parsed_header.extension.hasVideoRotation) {
            std::cout << "\tRotation="
                      << static_cast<int>(
                             parsed_header.extension.videoRotation);
          }
          if (parsed_header.extension.hasTransportSequenceNumber) {
            std::cout << "\tTransportSeq="
                      << parsed_header.extension.transportSequenceNumber;
          }
          if (parsed_header.extension.hasTransmissionTimeOffset) {
            std::cout << "\tTransmTimeOffset="
                      << parsed_header.extension.transmissionTimeOffset;
          }
          if (parsed_header.extension.hasAudioLevel) {
            std::cout << "\tAudioLevel=" <<
                static_cast<int>(parsed_header.extension.audioLevel);
          }
          std::cout << std::endl;
          if (FLAG_print_full_packets) {
            // TODO(terelius): Rewrite this file to use printf instead of cout.
            std::cout << "\t\t" << std::hex;
            char prev_fill = std::cout.fill('0');
            for (size_t i = 0; i < header_length; i++) {
              std::cout << std::setw(2) << static_cast<unsigned>(header[i]);
              if (i % 4 == 3)
                std::cout << " ";  // Separator between 32-bit words.
            }
            std::cout.fill(prev_fill);
            std::cout << std::dec << std::endl;
          }
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::RTCP_EVENT: {
        if (FLAG_rtcp) {
          size_t length;
          uint8_t packet[IP_PACKET_SIZE];
          webrtc::PacketDirection direction;
          parsed_stream.GetRtcpPacket(i, &direction, packet, &length);

          webrtc::rtcp::CommonHeader rtcp_block;
          const uint8_t* packet_end = packet + length;
          for (const uint8_t* next_block = packet; next_block != packet_end;
               next_block = rtcp_block.NextPacket()) {
            ptrdiff_t remaining_blocks_size = packet_end - next_block;
            RTC_DCHECK_GT(remaining_blocks_size, 0);
            if (!rtcp_block.Parse(next_block, remaining_blocks_size)) {
              RTC_LOG(LS_WARNING) << "Failed to parse RTCP";
              break;
            }

            uint64_t log_timestamp = parsed_stream.GetTimestamp(i);
            switch (rtcp_block.type()) {
              case webrtc::rtcp::SenderReport::kPacketType:
                PrintSenderReport(parsed_stream, rtcp_block, log_timestamp,
                                  direction);
                break;
              case webrtc::rtcp::ReceiverReport::kPacketType:
                PrintReceiverReport(parsed_stream, rtcp_block, log_timestamp,
                                    direction);
                break;
              case webrtc::rtcp::Sdes::kPacketType:
                PrintSdes(rtcp_block, log_timestamp, direction);
                break;
              case webrtc::rtcp::ExtendedReports::kPacketType:
                PrintXr(parsed_stream, rtcp_block, log_timestamp, direction);
                break;
              case webrtc::rtcp::Bye::kPacketType:
                PrintBye(parsed_stream, rtcp_block, log_timestamp, direction);
                break;
              case webrtc::rtcp::Rtpfb::kPacketType:
                PrintRtpFeedback(parsed_stream, rtcp_block, log_timestamp,
                                 direction);
                break;
              case webrtc::rtcp::Psfb::kPacketType:
                PrintPsFeedback(parsed_stream, rtcp_block, log_timestamp,
                                direction);
                break;
              default:
                break;
            }
            if (FLAG_print_full_packets) {
              std::cout << "\t\t" << std::hex;
              char prev_fill = std::cout.fill('0');
              for (const uint8_t* p = next_block; p < rtcp_block.NextPacket();
                   p++) {
                std::cout << std::setw(2) << static_cast<unsigned>(*p);
                ptrdiff_t chars_printed = p - next_block;
                if (chars_printed % 4 == 3)
                  std::cout << " ";  // Separator between 32-bit words.
              }
              std::cout.fill(prev_fill);
              std::cout << std::dec << std::endl;
            }
          }
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::AUDIO_PLAYOUT_EVENT: {
        if (FLAG_playout) {
          uint32_t ssrc;
          parsed_stream.GetAudioPlayout(i, &ssrc);
          std::cout << parsed_stream.GetTimestamp(i) << "\tAUDIO_PLAYOUT"
                    << "\tssrc=" << ssrc << std::endl;
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::LOSS_BASED_BWE_UPDATE: {
        if (FLAG_bwe) {
          int32_t bitrate_bps;
          uint8_t fraction_loss;
          int32_t total_packets;
          parsed_stream.GetLossBasedBweUpdate(i, &bitrate_bps, &fraction_loss,
                                              &total_packets);
          std::cout << parsed_stream.GetTimestamp(i) << "\tBWE(LOSS_BASED)"
                    << "\tbitrate_bps=" << bitrate_bps << "\tfraction_loss="
                    << static_cast<unsigned>(fraction_loss)
                    << "\ttotal_packets=" << total_packets << std::endl;
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::DELAY_BASED_BWE_UPDATE: {
        if (FLAG_bwe) {
          auto bwe_update = parsed_stream.GetDelayBasedBweUpdate(i);
          std::cout << parsed_stream.GetTimestamp(i) << "\tBWE(DELAY_BASED)"
                    << "\tbitrate_bps=" << bwe_update.bitrate_bps
                    << "\tdetector_state="
                    << static_cast<int>(bwe_update.detector_state) << std::endl;
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::VIDEO_RECEIVER_CONFIG_EVENT: {
        if (FLAG_config && FLAG_video && FLAG_incoming) {
          webrtc::rtclog::StreamConfig config =
              parsed_stream.GetVideoReceiveConfig(i);
          std::cout << parsed_stream.GetTimestamp(i) << "\tVIDEO_RECV_CONFIG"
                    << "\tssrc=" << config.remote_ssrc
                    << "\tfeedback_ssrc=" << config.local_ssrc;
          std::cout << "\textensions={";
          for (const auto& extension : config.rtp_extensions) {
            std::cout << extension.ToString() << ",";
          }
          std::cout << "}";
          std::cout << "\tcodecs={";
          for (const auto& codec : config.codecs) {
            std::cout << "{name: " << codec.payload_name
                      << ", payload_type: " << codec.payload_type
                      << ", rtx_payload_type: " << codec.rtx_payload_type
                      << "}";
          }
          std::cout << "}" << std::endl;
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::VIDEO_SENDER_CONFIG_EVENT: {
        if (FLAG_config && FLAG_video && FLAG_outgoing) {
          std::vector<webrtc::rtclog::StreamConfig> configs =
              parsed_stream.GetVideoSendConfig(i);
          for (const auto& config : configs) {
            std::cout << parsed_stream.GetTimestamp(i) << "\tVIDEO_SEND_CONFIG";
            std::cout << "\tssrcs=" << config.local_ssrc;
            std::cout << "\trtx_ssrcs=" << config.rtx_ssrc;
            std::cout << "\textensions={";
            for (const auto& extension : config.rtp_extensions) {
              std::cout << extension.ToString() << ",";
            }
            std::cout << "}";
            std::cout << "\tcodecs={";
            for (const auto& codec : config.codecs) {
              std::cout << "{name: " << codec.payload_name
                        << ", payload_type: " << codec.payload_type
                        << ", rtx_payload_type: " << codec.rtx_payload_type
                        << "}";
            }
            std::cout << "}" << std::endl;
          }
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::AUDIO_RECEIVER_CONFIG_EVENT: {
        if (FLAG_config && FLAG_audio && FLAG_incoming) {
          webrtc::rtclog::StreamConfig config =
              parsed_stream.GetAudioReceiveConfig(i);
          std::cout << parsed_stream.GetTimestamp(i) << "\tAUDIO_RECV_CONFIG"
                    << "\tssrc=" << config.remote_ssrc
                    << "\tfeedback_ssrc=" << config.local_ssrc;
          std::cout << "\textensions={";
          for (const auto& extension : config.rtp_extensions) {
            std::cout << extension.ToString() << ",";
          }
          std::cout << "}";
          std::cout << "\tcodecs={";
          for (const auto& codec : config.codecs) {
            std::cout << "{name: " << codec.payload_name
                      << ", payload_type: " << codec.payload_type
                      << ", rtx_payload_type: " << codec.rtx_payload_type
                      << "}";
          }
          std::cout << "}" << std::endl;
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::AUDIO_SENDER_CONFIG_EVENT: {
        if (FLAG_config && FLAG_audio && FLAG_outgoing) {
          webrtc::rtclog::StreamConfig config =
              parsed_stream.GetAudioSendConfig(i);
          std::cout << parsed_stream.GetTimestamp(i) << "\tAUDIO_SEND_CONFIG"
                    << "\tssrc=" << config.local_ssrc;
          std::cout << "\textensions={";
          for (const auto& extension : config.rtp_extensions) {
            std::cout << extension.ToString() << ",";
          }
          std::cout << "}";
          std::cout << "\tcodecs={";
          for (const auto& codec : config.codecs) {
            std::cout << "{name: " << codec.payload_name
                      << ", payload_type: " << codec.payload_type
                      << ", rtx_payload_type: " << codec.rtx_payload_type
                      << "}";
          }
          std::cout << "}" << std::endl;
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::AUDIO_NETWORK_ADAPTATION_EVENT: {
        if (FLAG_ana) {
          webrtc::AudioEncoderRuntimeConfig ana_config;
          parsed_stream.GetAudioNetworkAdaptation(i, &ana_config);
          std::stringstream ss;
          ss << parsed_stream.GetTimestamp(i) << "\tANA_UPDATE";
          if (ana_config.bitrate_bps) {
            ss << "\tbitrate_bps=" << *ana_config.bitrate_bps;
          }
          if (ana_config.frame_length_ms) {
            ss << "\tframe_length_ms=" << *ana_config.frame_length_ms;
          }
          if (ana_config.uplink_packet_loss_fraction) {
            ss << "\tuplink_packet_loss_fraction="
               << *ana_config.uplink_packet_loss_fraction;
          }
          if (ana_config.enable_fec) {
            ss << "\tenable_fec=" << *ana_config.enable_fec;
          }
          if (ana_config.enable_dtx) {
            ss << "\tenable_dtx=" << *ana_config.enable_dtx;
          }
          if (ana_config.num_channels) {
            ss << "\tnum_channels=" << *ana_config.num_channels;
          }
          std::cout << ss.str() << std::endl;
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::BWE_PROBE_CLUSTER_CREATED_EVENT: {
        if (FLAG_probe) {
          webrtc::ParsedRtcEventLog::BweProbeClusterCreatedEvent probe_event =
              parsed_stream.GetBweProbeClusterCreated(i);
          std::cout << parsed_stream.GetTimestamp(i) << "\tPROBE_CREATED("
                    << probe_event.id << ")"
                    << "\tbitrate_bps=" << probe_event.bitrate_bps
                    << "\tmin_packets=" << probe_event.min_packets
                    << "\tmin_bytes=" << probe_event.min_bytes << std::endl;
        }
        event_recognized = true;
        break;
      }

      case webrtc::ParsedRtcEventLog::BWE_PROBE_RESULT_EVENT: {
        if (FLAG_probe) {
          webrtc::ParsedRtcEventLog::BweProbeResultEvent probe_result =
              parsed_stream.GetBweProbeResult(i);
          if (probe_result.failure_reason) {
            std::cout << parsed_stream.GetTimestamp(i) << "\tPROBE_SUCCESS("
                      << probe_result.id << ")"
                      << "\tfailure_reason="
                      << static_cast<int>(*probe_result.failure_reason)
                      << std::endl;
          } else {
            std::cout << parsed_stream.GetTimestamp(i) << "\tPROBE_SUCCESS("
                      << probe_result.id << ")"
                      << "\tbitrate_bps=" << *probe_result.bitrate_bps
                      << std::endl;
          }
        }
        event_recognized = true;
        break;
      }
    }

    if (!event_recognized) {
      std::cout << "Unrecognized event (" << parsed_stream.GetEventType(i)
                << ")" << std::endl;
    }
  }
  return 0;
}
