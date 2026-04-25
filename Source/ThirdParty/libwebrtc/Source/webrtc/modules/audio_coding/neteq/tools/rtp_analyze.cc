/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "api/rtp_headers.h"
#include "modules/audio_coding/neteq/tools/rtp_file_source.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"

ABSL_FLAG(int, red, 117, "RTP payload type for RED");
ABSL_FLAG(int,
          audio_level,
          -1,
          "Extension ID for audio level (RFC 6464); "
          "-1 not to print audio level");
ABSL_FLAG(int,
          abs_send_time,
          -1,
          "Extension ID for absolute sender time; "
          "-1 not to print absolute send time");

namespace {

struct RedHeader {
  uint32_t rtp_timestamp;
  int payload_type;
};
std::vector<RedHeader> ExtractRedHeaders(const webrtc::RtpPacket& packet) {
  //
  //  0                   1                    2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3  4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |1|   block PT  |  timestamp offset         |   block length    |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |1|    ...                                                      |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |0|   block PT  |
  // +-+-+-+-+-+-+-+-+
  //
  const uint8_t* payload_ptr = packet.payload().data();
  const uint8_t* payload_end_ptr =
      packet.payload().data() + packet.payload().size();

  // Find all RED headers with the extension bit set to 1. That is, all headers
  // but the last one.
  std::vector<RedHeader> red_headers;
  while ((payload_ptr < payload_end_ptr) && (*payload_ptr & 0x80)) {
    RedHeader header;
    header.payload_type = payload_ptr[0] & 0x7F;
    uint32_t offset = (payload_ptr[1] << 6) + ((payload_ptr[2] & 0xFC) >> 2);
    header.rtp_timestamp = packet.Timestamp() - offset;
    red_headers.push_back(header);
    payload_ptr += 4;
  }
  // Last header.
  RTC_DCHECK_LT(payload_ptr, payload_end_ptr);
  if (payload_ptr >= payload_end_ptr) {
    return {};  // Payload too short.
  }
  RedHeader header;
  header.payload_type = payload_ptr[0] & 0x7F;
  header.rtp_timestamp = packet.Timestamp();
  red_headers.push_back(header);
  std::reverse(red_headers.begin(), red_headers.end());
  return red_headers;
}

}  // namespace

int main(int argc, char* argv[]) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  std::string usage =
      "Tool for parsing an RTP dump file to text output.\n"
      "Example usage:\n"
      "./rtp_analyze input.rtp output.txt\n\n"
      "Output is sent to stdout if no output file is given. "
      "Note that this tool can read files with or without payloads.\n";
  if (args.size() != 2 && args.size() != 3) {
    printf("%s", usage.c_str());
    return 1;
  }

  RTC_CHECK(absl::GetFlag(FLAGS_red) >= 0 &&
            absl::GetFlag(FLAGS_red) <= 127);          // Payload type
  RTC_CHECK(absl::GetFlag(FLAGS_audio_level) == -1 ||  // Default
            (absl::GetFlag(FLAGS_audio_level) > 0 &&
             absl::GetFlag(FLAGS_audio_level) <= 255));  // Extension ID
  RTC_CHECK(absl::GetFlag(FLAGS_abs_send_time) == -1 ||  // Default
            (absl::GetFlag(FLAGS_abs_send_time) > 0 &&
             absl::GetFlag(FLAGS_abs_send_time) <= 255));  // Extension ID

  printf("Input file: %s\n", args[1]);
  std::unique_ptr<webrtc::test::RtpFileSource> file_source(
      webrtc::test::RtpFileSource::Create(args[1]));
  RTC_DCHECK(file_source.get());
  // Set RTP extension IDs.
  bool print_audio_level = false;
  if (absl::GetFlag(FLAGS_audio_level) != -1) {
    print_audio_level = true;
    file_source->RegisterRtpHeaderExtension(webrtc::kRtpExtensionAudioLevel,
                                            absl::GetFlag(FLAGS_audio_level));
  }
  bool print_abs_send_time = false;
  if (absl::GetFlag(FLAGS_abs_send_time) != -1) {
    print_abs_send_time = true;
    file_source->RegisterRtpHeaderExtension(
        webrtc::kRtpExtensionAbsoluteSendTime,
        absl::GetFlag(FLAGS_abs_send_time));
  }

  FILE* out_file;
  if (args.size() == 3) {
    out_file = fopen(args[2], "wt");
    if (!out_file) {
      printf("Cannot open output file %s\n", args[2]);
      return -1;
    }
    printf("Output file: %s\n\n", args[2]);
  } else {
    out_file = stdout;
  }

  // Print file header.
  fprintf(out_file, "SeqNo  TimeStamp   SendTime  Size    PT  M       SSRC");
  if (print_audio_level) {
    fprintf(out_file, " AuLvl (V)");
  }
  if (print_abs_send_time) {
    fprintf(out_file, " AbsSendTime");
  }
  fprintf(out_file, "\n");

  uint32_t max_abs_send_time = 0;
  int cycles = -1;
  std::unique_ptr<webrtc::RtpPacketReceived> packet;
  while (true) {
    packet = file_source->NextPacket();
    if (!packet) {
      // End of file reached.
      break;
    }
    // Write packet data to file. Use virtual_packet_length_bytes so that the
    // correct packet sizes are printed also for RTP header-only dumps.
    fprintf(out_file, "%5u %10u %10i %5zu %5i %2i %#08X",
            packet->SequenceNumber(), packet->Timestamp(),
            packet->arrival_time().ms<int>(), packet->size(),
            packet->PayloadType(), packet->Marker(), packet->Ssrc());
    webrtc::AudioLevel audio_level;
    if (print_audio_level &&
        packet->GetExtension<webrtc::AudioLevelExtension>(&audio_level)) {
      fprintf(out_file, " %5d (%1i)", audio_level.level(),
              audio_level.voice_activity());
    }
    uint32_t abs_sent_time;
    if (print_abs_send_time &&
        packet->GetExtension<webrtc::AbsoluteSendTime>(&abs_sent_time)) {
      if (cycles == -1) {
        // Initialize.
        max_abs_send_time = abs_sent_time;
        cycles = 0;
      }
      // Abs sender time is 24 bit 6.18 fixed point. Shift by 8 to normalize to
      // 32 bits (unsigned). Calculate the difference between this packet's
      // send time and the maximum observed. Cast to signed 32-bit to get the
      // desired wrap-around behavior.
      if (static_cast<int32_t>((abs_sent_time << 8) -
                               (max_abs_send_time << 8)) >= 0) {
        // The difference is non-negative, meaning that this packet is newer
        // than the previously observed maximum absolute send time.
        if (abs_sent_time < max_abs_send_time) {
          // Wrap detected.
          cycles++;
        }
        max_abs_send_time = abs_sent_time;
      }
      // Abs sender time is 24 bit 6.18 fixed point. Divide by 2^18 to convert
      // to floating point representation.
      double send_time_seconds =
          static_cast<double>(abs_sent_time) / 262144 + 64.0 * cycles;
      fprintf(out_file, " %11f", send_time_seconds);
    }
    fprintf(out_file, "\n");

    if (packet->PayloadType() == absl::GetFlag(FLAGS_red)) {
      for (const RedHeader& red : ExtractRedHeaders(*packet)) {
        fprintf(out_file, "* %5u %10u %10i %5i\n", packet->SequenceNumber(),
                red.rtp_timestamp, packet->arrival_time().ms<int>(),
                red.payload_type);
      }
    }
  }

  fclose(out_file);

  return 0;
}
