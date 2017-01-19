/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/modules/video_coding/test/receiver_tests.h"
#include "webrtc/modules/video_coding/test/vcm_payload_sink_factory.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace {

const bool kConfigProtectionEnabled = true;
const webrtc::VCMVideoProtection kConfigProtectionMethod =
    webrtc::kProtectionNack;
const float kConfigLossRate = 0.0f;
const bool kConfigReordering = false;
const int64_t kConfigRttMs = 0;
const uint32_t kConfigRenderDelayMs = 0;
const uint32_t kConfigMinPlayoutDelayMs = 0;
const int64_t kConfigMaxRuntimeMs = -1;
const uint8_t kDefaultUlpFecPayloadType = 97;
const uint8_t kDefaultRedPayloadType = 96;
const uint8_t kDefaultVp8PayloadType = 100;
}  // namespace

int RtpPlay(const CmdArgs& args) {
  std::string trace_file = webrtc::test::OutputPath() + "receiverTestTrace.txt";
  webrtc::Trace::CreateTrace();
  webrtc::Trace::SetTraceFile(trace_file.c_str());
  webrtc::Trace::set_level_filter(webrtc::kTraceAll);

  webrtc::rtpplayer::PayloadTypes payload_types;
  payload_types.push_back(webrtc::rtpplayer::PayloadCodecTuple(
      kDefaultUlpFecPayloadType, "ULPFEC", webrtc::kVideoCodecULPFEC));
  payload_types.push_back(webrtc::rtpplayer::PayloadCodecTuple(
      kDefaultRedPayloadType, "RED", webrtc::kVideoCodecRED));
  payload_types.push_back(webrtc::rtpplayer::PayloadCodecTuple(
      kDefaultVp8PayloadType, "VP8", webrtc::kVideoCodecVP8));

  std::string output_file = args.outputFile;
  if (output_file.empty())
    output_file = webrtc::test::OutputPath() + "RtpPlay_decoded.yuv";

  webrtc::SimulatedClock clock(0);
  webrtc::rtpplayer::VcmPayloadSinkFactory factory(
      output_file, &clock, kConfigProtectionEnabled, kConfigProtectionMethod,
      kConfigRttMs, kConfigRenderDelayMs, kConfigMinPlayoutDelayMs);
  std::unique_ptr<webrtc::rtpplayer::RtpPlayerInterface> rtp_player(
      webrtc::rtpplayer::Create(args.inputFile, &factory, &clock, payload_types,
                                kConfigLossRate, kConfigRttMs,
                                kConfigReordering));
  if (rtp_player.get() == NULL) {
    return -1;
  }

  int ret = 0;
  while ((ret = rtp_player->NextPacket(clock.TimeInMilliseconds())) == 0) {
    ret = factory.DecodeAndProcessAll(true);
    if (ret < 0 || (kConfigMaxRuntimeMs > -1 &&
                    clock.TimeInMilliseconds() >= kConfigMaxRuntimeMs)) {
      break;
    }
    clock.AdvanceTimeMilliseconds(1);
  }

  rtp_player->Print();

  switch (ret) {
    case 1:
      printf("Success\n");
      return 0;
    case -1:
      printf("Failed\n");
      return -1;
    case 0:
      printf("Timeout\n");
      return -1;
  }

  webrtc::Trace::ReturnTrace();
  return 0;
}
