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

#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/tools/bwe_rtp.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "rtc_base/format_macros.h"
#include "test/rtp_file_reader.h"

class Observer : public webrtc::RemoteBitrateObserver {
 public:
  explicit Observer(webrtc::Clock* clock) : clock_(clock) {}

  // Called when a receive channel group has a new bitrate estimate for the
  // incoming streams.
  virtual void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                       uint32_t bitrate) {
    printf("[%u] Num SSRCs: %d, bitrate: %u\n",
           static_cast<uint32_t>(clock_->TimeInMilliseconds()),
           static_cast<int>(ssrcs.size()), bitrate);
  }

  virtual ~Observer() {}

 private:
  webrtc::Clock* clock_;
};

int main(int argc, char* argv[]) {
  webrtc::test::RtpFileReader* reader;
  webrtc::RemoteBitrateEstimator* estimator;
  webrtc::RtpHeaderParser* parser;
  std::string estimator_used;
  webrtc::SimulatedClock clock(0);
  Observer observer(&clock);
  if (!ParseArgsAndSetupEstimator(argc, argv, &clock, &observer, &reader,
                                  &parser, &estimator, &estimator_used)) {
    return -1;
  }
  std::unique_ptr<webrtc::test::RtpFileReader> rtp_reader(reader);
  std::unique_ptr<webrtc::RtpHeaderParser> rtp_parser(parser);
  std::unique_ptr<webrtc::RemoteBitrateEstimator> rbe(estimator);

  // Process the file.
  int packet_counter = 0;
  int64_t next_rtp_time_ms = 0;
  int64_t first_rtp_time_ms = -1;
  int abs_send_time_count = 0;
  int ts_offset_count = 0;
  webrtc::test::RtpPacket packet;
  if (!rtp_reader->NextPacket(&packet)) {
    printf("No RTP packet found\n");
    return 0;
  }
  first_rtp_time_ms = packet.time_ms;
  packet.time_ms = packet.time_ms - first_rtp_time_ms;
  while (true) {
    if (next_rtp_time_ms <= clock.TimeInMilliseconds()) {
      if (!parser->IsRtcp(packet.data, packet.length)) {
        webrtc::RTPHeader header;
        parser->Parse(packet.data, packet.length, &header);
        if (header.extension.hasAbsoluteSendTime)
          ++abs_send_time_count;
        if (header.extension.hasTransmissionTimeOffset)
          ++ts_offset_count;
        size_t packet_length = packet.length;
        // Some RTP dumps only include the header, in which case packet.length
        // is equal to the header length. In those cases packet.original_length
        // usually contains the original packet length.
        if (packet.original_length > 0) {
          packet_length = packet.original_length;
        }
        rbe->IncomingPacket(clock.TimeInMilliseconds(),
                            packet_length - header.headerLength, header);
        ++packet_counter;
      }
      if (!rtp_reader->NextPacket(&packet)) {
        break;
      }
      packet.time_ms = packet.time_ms - first_rtp_time_ms;
      next_rtp_time_ms = packet.time_ms;
    }
    int64_t time_until_process_ms = rbe->TimeUntilNextProcess();
    if (time_until_process_ms <= 0) {
      rbe->Process();
    }
    int64_t time_until_next_event =
        std::min(rbe->TimeUntilNextProcess(),
                 next_rtp_time_ms - clock.TimeInMilliseconds());
    clock.AdvanceTimeMilliseconds(std::max<int64_t>(time_until_next_event, 0));
  }
  printf("Parsed %d packets\nTime passed: %" PRId64 " ms\n", packet_counter,
         clock.TimeInMilliseconds());
  printf("Estimator used: %s\n", estimator_used.c_str());
  printf("Packets with absolute send time: %d\n", abs_send_time_count);
  printf("Packets with timestamp offset: %d\n", ts_offset_count);
  printf("Packets with no extension: %d\n",
         packet_counter - ts_offset_count - abs_send_time_count);
  return 0;
}
