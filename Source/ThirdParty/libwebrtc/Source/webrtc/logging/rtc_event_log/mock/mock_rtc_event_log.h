/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LOGGING_RTC_EVENT_LOG_MOCK_MOCK_RTC_EVENT_LOG_H_
#define WEBRTC_LOGGING_RTC_EVENT_LOG_MOCK_MOCK_RTC_EVENT_LOG_H_

#include <string>

#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

class MockRtcEventLog : public RtcEventLog {
 public:
  MOCK_METHOD2(StartLogging,
               bool(const std::string& file_name, int64_t max_size_bytes));

  MOCK_METHOD2(StartLogging,
               bool(rtc::PlatformFile log_file, int64_t max_size_bytes));

  MOCK_METHOD0(StopLogging, void());

  MOCK_METHOD1(LogVideoReceiveStreamConfig,
               void(const webrtc::VideoReceiveStream::Config& config));

  MOCK_METHOD1(LogVideoSendStreamConfig,
               void(const webrtc::VideoSendStream::Config& config));

  MOCK_METHOD1(LogAudioReceiveStreamConfig,
               void(const webrtc::AudioReceiveStream::Config& config));

  MOCK_METHOD1(LogAudioSendStreamConfig,
               void(const webrtc::AudioSendStream::Config& config));

  MOCK_METHOD4(LogRtpHeader,
               void(PacketDirection direction,
                    MediaType media_type,
                    const uint8_t* header,
                    size_t packet_length));

  MOCK_METHOD5(LogRtpHeader,
               void(PacketDirection direction,
                    MediaType media_type,
                    const uint8_t* header,
                    size_t packet_length,
                    int probe_cluster_id));

  MOCK_METHOD4(LogRtcpPacket,
               void(PacketDirection direction,
                    MediaType media_type,
                    const uint8_t* packet,
                    size_t length));

  MOCK_METHOD1(LogAudioPlayout, void(uint32_t ssrc));

  MOCK_METHOD3(LogLossBasedBweUpdate,
               void(int32_t bitrate_bps,
                    uint8_t fraction_loss,
                    int32_t total_packets));

  MOCK_METHOD2(LogDelayBasedBweUpdate,
               void(int32_t bitrate_bps, BandwidthUsage detector_state));

  MOCK_METHOD1(LogAudioNetworkAdaptation,
               void(const AudioNetworkAdaptor::EncoderRuntimeConfig& config));

  MOCK_METHOD4(LogProbeClusterCreated,
               void(int id, int bitrate_bps, int min_probes, int min_bytes));

  MOCK_METHOD2(LogProbeResultSuccess, void(int id, int bitrate_bps));
  MOCK_METHOD2(LogProbeResultFailure,
               void(int id, ProbeFailureReason failure_reason));
};

}  // namespace webrtc

#endif  // WEBRTC_LOGGING_RTC_EVENT_LOG_MOCK_MOCK_RTC_EVENT_LOG_H_
