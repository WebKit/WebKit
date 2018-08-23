/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <memory>
#include <sstream>

#include "modules/remote_bitrate_estimator/tools/bwe_rtp.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "rtc_base/format_macros.h"
#include "test/rtp_file_reader.h"

int main(int argc, char* argv[]) {
  webrtc::test::RtpFileReader* reader;
  webrtc::RtpHeaderParser* parser;
  if (!ParseArgsAndSetupEstimator(argc, argv, NULL, NULL, &reader, &parser,
                                  NULL, NULL)) {
    return -1;
  }
  bool arrival_time_only = (argc >= 5 && strncmp(argv[4], "-t", 2) == 0);
  std::unique_ptr<webrtc::test::RtpFileReader> rtp_reader(reader);
  std::unique_ptr<webrtc::RtpHeaderParser> rtp_parser(parser);
  fprintf(stdout,
          "seqnum timestamp ts_offset abs_sendtime recvtime "
          "markerbit ssrc size original_size\n");
  int packet_counter = 0;
  int non_zero_abs_send_time = 0;
  int non_zero_ts_offsets = 0;
  webrtc::test::RtpPacket packet;
  while (rtp_reader->NextPacket(&packet)) {
    webrtc::RTPHeader header;
    parser->Parse(packet.data, packet.length, &header);
    if (header.extension.absoluteSendTime != 0)
      ++non_zero_abs_send_time;
    if (header.extension.transmissionTimeOffset != 0)
      ++non_zero_ts_offsets;
    if (arrival_time_only) {
      std::stringstream ss;
      ss << static_cast<int64_t>(packet.time_ms) * 1000000;
      fprintf(stdout, "%s\n", ss.str().c_str());
    } else {
      fprintf(stdout, "%u %u %d %u %u %d %u %" PRIuS " %" PRIuS "\n",
              header.sequenceNumber, header.timestamp,
              header.extension.transmissionTimeOffset,
              header.extension.absoluteSendTime, packet.time_ms,
              header.markerBit, header.ssrc, packet.length,
              packet.original_length);
    }
    ++packet_counter;
  }
  fprintf(stderr, "Parsed %d packets\n", packet_counter);
  fprintf(stderr, "Packets with non-zero absolute send time: %d\n",
          non_zero_abs_send_time);
  fprintf(stderr, "Packets with non-zero timestamp offset: %d\n",
          non_zero_ts_offsets);
  return 0;
}
