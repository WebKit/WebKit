/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_UNITTEST_HELPER_H_
#define WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_UNITTEST_HELPER_H_

#include "webrtc/call/call.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log_parser.h"

namespace webrtc {

class RtcEventLogTestHelper {
 public:
  static void VerifyVideoReceiveStreamConfig(
      const ParsedRtcEventLog& parsed_log,
      size_t index,
      const rtclog::StreamConfig& config);
  static void VerifyVideoSendStreamConfig(const ParsedRtcEventLog& parsed_log,
                                          size_t index,
                                          const rtclog::StreamConfig& config);
  static void VerifyAudioReceiveStreamConfig(
      const ParsedRtcEventLog& parsed_log,
      size_t index,
      const rtclog::StreamConfig& config);
  static void VerifyAudioSendStreamConfig(const ParsedRtcEventLog& parsed_log,
                                          size_t index,
                                          const rtclog::StreamConfig& config);
  static void VerifyRtpEvent(const ParsedRtcEventLog& parsed_log,
                             size_t index,
                             PacketDirection direction,
                             const uint8_t* header,
                             size_t header_size,
                             size_t total_size);
  static void VerifyRtcpEvent(const ParsedRtcEventLog& parsed_log,
                              size_t index,
                              PacketDirection direction,
                              const uint8_t* packet,
                              size_t total_size);
  static void VerifyPlayoutEvent(const ParsedRtcEventLog& parsed_log,
                                 size_t index,
                                 uint32_t ssrc);
  static void VerifyBweLossEvent(const ParsedRtcEventLog& parsed_log,
                                 size_t index,
                                 int32_t bitrate,
                                 uint8_t fraction_loss,
                                 int32_t total_packets);
  static void VerifyBweDelayEvent(const ParsedRtcEventLog& parsed_log,
                                  size_t index,
                                  int32_t bitrate,
                                  BandwidthUsage detector_state);

  static void VerifyAudioNetworkAdaptation(
      const ParsedRtcEventLog& parsed_log,
      size_t index,
      const AudioEncoderRuntimeConfig& config);

  static void VerifyLogStartEvent(const ParsedRtcEventLog& parsed_log,
                                  size_t index);
  static void VerifyLogEndEvent(const ParsedRtcEventLog& parsed_log,
                                size_t index);

  static void VerifyBweProbeCluster(const ParsedRtcEventLog& parsed_log,
                                    size_t index,
                                    uint32_t id,
                                    uint32_t bitrate_bps,
                                    uint32_t min_probes,
                                    uint32_t min_bytes);

  static void VerifyProbeResultSuccess(const ParsedRtcEventLog& parsed_log,
                                       size_t index,
                                       uint32_t id,
                                       uint32_t bitrate_bps);

  static void VerifyProbeResultFailure(const ParsedRtcEventLog& parsed_log,
                                       size_t index,
                                       uint32_t id,
                                       ProbeFailureReason failure_reason);
};

}  // namespace webrtc

#endif  // WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_UNITTEST_HELPER_H_
