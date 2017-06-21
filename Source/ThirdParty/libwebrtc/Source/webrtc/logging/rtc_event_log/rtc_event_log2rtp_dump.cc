/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "gflags/gflags.h"
#include "webrtc/base/checks.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log_parser.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/test/rtp_file_writer.h"

namespace {

using MediaType = webrtc::ParsedRtcEventLog::MediaType;

DEFINE_bool(noaudio,
            false,
            "Excludes audio packets from the converted RTPdump file.");
DEFINE_bool(novideo,
            false,
            "Excludes video packets from the converted RTPdump file.");
DEFINE_bool(nodata,
            false,
            "Excludes data packets from the converted RTPdump file.");
DEFINE_bool(nortp,
            false,
            "Excludes RTP packets from the converted RTPdump file.");
DEFINE_bool(nortcp,
            false,
            "Excludes RTCP packets from the converted RTPdump file.");
DEFINE_string(ssrc,
              "",
              "Store only packets with this SSRC (decimal or hex, the latter "
              "starting with 0x).");

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
      " --helpshort for usage.\n"
      "Example usage:\n" +
      program_name + " input.rel output.rtp\n";
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (argc != 3) {
    std::cout << google::ProgramUsage();
    return 0;
  }
  std::string input_file = argv[1];
  std::string output_file = argv[2];

  uint32_t ssrc_filter = 0;
  if (!FLAGS_ssrc.empty())
    RTC_CHECK(ParseSsrc(FLAGS_ssrc, &ssrc_filter))
        << "Flag verification has failed.";

  webrtc::ParsedRtcEventLog parsed_stream;
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
    if (!FLAGS_nortp &&
        parsed_stream.GetEventType(i) == webrtc::ParsedRtcEventLog::RTP_EVENT) {
      webrtc::test::RtpPacket packet;
      webrtc::PacketDirection direction;
      parsed_stream.GetRtpHeader(i, &direction, packet.data, &packet.length,
                                 &packet.original_length);
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
      if (FLAGS_noaudio && media_type == MediaType::AUDIO)
        continue;
      if (FLAGS_novideo && media_type == MediaType::VIDEO)
        continue;
      if (FLAGS_nodata && media_type == MediaType::DATA)
        continue;
      if (!FLAGS_ssrc.empty()) {
        const uint32_t packet_ssrc =
            webrtc::ByteReader<uint32_t>::ReadBigEndian(
                reinterpret_cast<const uint8_t*>(packet.data + 8));
        if (packet_ssrc != ssrc_filter)
          continue;
      }

      rtp_writer->WritePacket(&packet);
      rtp_counter++;
    }
    if (!FLAGS_nortcp &&
        parsed_stream.GetEventType(i) ==
            webrtc::ParsedRtcEventLog::RTCP_EVENT) {
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
      if (FLAGS_noaudio && media_type == MediaType::AUDIO)
        continue;
      if (FLAGS_novideo && media_type == MediaType::VIDEO)
        continue;
      if (FLAGS_nodata && media_type == MediaType::DATA)
        continue;
      if (!FLAGS_ssrc.empty()) {
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
