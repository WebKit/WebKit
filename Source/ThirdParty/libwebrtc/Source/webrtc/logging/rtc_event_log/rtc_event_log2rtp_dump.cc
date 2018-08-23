/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "logging/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_event_log_parser_new.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/flags.h"
#include "test/rtp_file_writer.h"

namespace {

using MediaType = webrtc::ParsedRtcEventLogNew::MediaType;

DEFINE_bool(
    audio,
    true,
    "Use --noaudio to exclude audio packets from the converted RTPdump file.");
DEFINE_bool(
    video,
    true,
    "Use --novideo to exclude video packets from the converted RTPdump file.");
DEFINE_bool(
    data,
    true,
    "Use --nodata to exclude data packets from the converted RTPdump file.");
DEFINE_bool(
    rtp,
    true,
    "Use --nortp to exclude RTP packets from the converted RTPdump file.");
DEFINE_bool(
    rtcp,
    true,
    "Use --nortcp to exclude RTCP packets from the converted RTPdump file.");
DEFINE_string(ssrc,
              "",
              "Store only packets with this SSRC (decimal or hex, the latter "
              "starting with 0x).");
DEFINE_bool(help, false, "Prints this message.");

// Parses the input string for a valid SSRC. If a valid SSRC is found, it is
// written to the output variable |ssrc|, and true is returned. Otherwise,
// false is returned.
// The empty string must be validated as true, because it is the default value
// of the command-line flag. In this case, no value is written to the output
// variable.
bool ParseSsrc(std::string str, uint32_t* ssrc) {
  // If the input string starts with 0x or 0X it indicates a hexadecimal number.
  auto read_mode = std::dec;
  if (str.size() > 2 &&
      (str.substr(0, 2) == "0x" || str.substr(0, 2) == "0X")) {
    read_mode = std::hex;
    str = str.substr(2);
  }
  std::stringstream ss(str);
  ss >> read_mode >> *ssrc;
  return str.empty() || (!ss.fail() && ss.eof());
}

}  // namespace

// This utility will convert a stored event log to the rtpdump format.
int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Tool for converting an RtcEventLog file to an RTP dump file.\n"
      "Run " +
      program_name +
      " --help for usage.\n"
      "Example usage:\n" +
      program_name + " input.rel output.rtp\n";
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true) || FLAG_help ||
      argc != 3) {
    std::cout << usage;
    if (FLAG_help) {
      rtc::FlagList::Print(nullptr, false);
      return 0;
    }
    return 1;
  }

  std::string input_file = argv[1];
  std::string output_file = argv[2];

  uint32_t ssrc_filter = 0;
  if (strlen(FLAG_ssrc) > 0)
    RTC_CHECK(ParseSsrc(FLAG_ssrc, &ssrc_filter))
        << "Flag verification has failed.";

  webrtc::ParsedRtcEventLogNew parsed_stream;
  if (!parsed_stream.ParseFile(input_file)) {
    std::cerr << "Error while parsing input file: " << input_file << std::endl;
    return -1;
  }

  std::unique_ptr<webrtc::test::RtpFileWriter> rtp_writer(
      webrtc::test::RtpFileWriter::Create(
          webrtc::test::RtpFileWriter::FileFormat::kRtpDump, output_file));

  if (!rtp_writer.get()) {
    std::cerr << "Error while opening output file: " << output_file
              << std::endl;
    return -1;
  }

  std::cout << "Found " << parsed_stream.GetNumberOfEvents()
            << " events in the input file." << std::endl;
  int rtp_counter = 0, rtcp_counter = 0;
  bool header_only = false;
  for (size_t i = 0; i < parsed_stream.GetNumberOfEvents(); i++) {
    // The parsed_stream will assert if the protobuf event is missing
    // some required fields and we attempt to access them. We could consider
    // a softer failure option, but it does not seem useful to generate
    // RTP dumps based on broken event logs.
    if (FLAG_rtp && parsed_stream.GetEventType(i) ==
                        webrtc::ParsedRtcEventLogNew::EventType::RTP_EVENT) {
      webrtc::test::RtpPacket packet;
      webrtc::PacketDirection direction;
      parsed_stream.GetRtpHeader(i, &direction, packet.data, &packet.length,
                                 &packet.original_length, nullptr);
      if (packet.original_length > packet.length)
        header_only = true;
      packet.time_ms = parsed_stream.GetTimestamp(i) / 1000;

      webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet.data,
                                                     packet.length);

      // TODO(terelius): Maybe add a flag to dump outgoing traffic instead?
      if (direction == webrtc::kOutgoingPacket)
        continue;

      webrtc::RTPHeader parsed_header;
      rtp_parser.Parse(&parsed_header);
      MediaType media_type =
          parsed_stream.GetMediaType(parsed_header.ssrc, direction);
      if (!FLAG_audio && media_type == MediaType::AUDIO)
        continue;
      if (!FLAG_video && media_type == MediaType::VIDEO)
        continue;
      if (!FLAG_data && media_type == MediaType::DATA)
        continue;
      if (strlen(FLAG_ssrc) > 0) {
        const uint32_t packet_ssrc =
            webrtc::ByteReader<uint32_t>::ReadBigEndian(
                reinterpret_cast<const uint8_t*>(packet.data + 8));
        if (packet_ssrc != ssrc_filter)
          continue;
      }

      rtp_writer->WritePacket(&packet);
      rtp_counter++;
    }
    if (FLAG_rtcp && parsed_stream.GetEventType(i) ==
                         webrtc::ParsedRtcEventLogNew::EventType::RTCP_EVENT) {
      webrtc::test::RtpPacket packet;
      webrtc::PacketDirection direction;
      parsed_stream.GetRtcpPacket(i, &direction, packet.data, &packet.length);
      // For RTCP packets the original_length should be set to 0 in the
      // RTPdump format.
      packet.original_length = 0;
      packet.time_ms = parsed_stream.GetTimestamp(i) / 1000;

      // TODO(terelius): Maybe add a flag to dump outgoing traffic instead?
      if (direction == webrtc::kOutgoingPacket)
        continue;

      // Note that |packet_ssrc| is the sender SSRC. An RTCP message may contain
      // report blocks for many streams, thus several SSRCs and they doen't
      // necessarily have to be of the same media type.
      const uint32_t packet_ssrc = webrtc::ByteReader<uint32_t>::ReadBigEndian(
          reinterpret_cast<const uint8_t*>(packet.data + 4));
      MediaType media_type = parsed_stream.GetMediaType(packet_ssrc, direction);
      if (!FLAG_audio && media_type == MediaType::AUDIO)
        continue;
      if (!FLAG_video && media_type == MediaType::VIDEO)
        continue;
      if (!FLAG_data && media_type == MediaType::DATA)
        continue;
      if (strlen(FLAG_ssrc) > 0) {
        if (packet_ssrc != ssrc_filter)
          continue;
      }

      rtp_writer->WritePacket(&packet);
      rtcp_counter++;
    }
  }
  std::cout << "Wrote " << rtp_counter << (header_only ? " header-only" : "")
            << " RTP packets and " << rtcp_counter << " RTCP packets to the "
            << "output file." << std::endl;
  return 0;
}
