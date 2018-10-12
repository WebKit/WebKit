/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_CALL_CLIENT_H_
#define TEST_SCENARIO_CALL_CLIENT_H_
#include <memory>
#include <string>
#include <vector>

#include "call/call.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "modules/congestion_controller/test/controller_printer.h"
#include "rtc_base/constructormagic.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/scenario_config.h"

namespace webrtc {

namespace test {
class LoggingNetworkControllerFactory
    : public NetworkControllerFactoryInterface {
 public:
  LoggingNetworkControllerFactory(std::string filename,
                                  TransportControllerConfig config);
  RTC_DISALLOW_COPY_AND_ASSIGN(LoggingNetworkControllerFactory);
  ~LoggingNetworkControllerFactory();
  std::unique_ptr<NetworkControllerInterface> Create(
      NetworkControllerConfig config) override;
  TimeDelta GetProcessInterval() const override;
  // TODO(srte): Consider using the Columnprinter interface for this.
  void LogCongestionControllerStats(Timestamp at_time);
  RtcEventLog* GetEventLog() const;

 private:
  std::unique_ptr<RtcEventLog> event_log_;
  std::unique_ptr<NetworkControllerFactoryInterface> cc_factory_;
  std::unique_ptr<ControlStatePrinter> cc_printer_;
  FILE* cc_out_ = nullptr;
};

// CallClient represents a participant in a call scenario. It is created by the
// Scenario class and is used as sender and receiver when setting up a media
// stream session.
class CallClient {
 public:
  CallClient(Clock* clock, std::string log_filename, CallClientConfig config);
  RTC_DISALLOW_COPY_AND_ASSIGN(CallClient);

  ~CallClient();
  ColumnPrinter StatsPrinter();
  Call::Stats GetStats();

 private:
  friend class Scenario;
  friend class SendVideoStream;
  friend class ReceiveVideoStream;
  friend class SendAudioStream;
  friend class ReceiveAudioStream;
  friend class NetworkNodeTransport;
  // TODO(srte): Consider using the Columnprinter interface for this.
  void DeliverPacket(MediaType media_type,
                     rtc::CopyOnWriteBuffer packet,
                     Timestamp at_time);
  uint32_t GetNextVideoSsrc();
  uint32_t GetNextAudioSsrc();
  uint32_t GetNextRtxSsrc();
  std::string GetNextPriorityId();

  Clock* clock_;
  LoggingNetworkControllerFactory network_controller_factory_;
  std::unique_ptr<Call> call_;

  rtc::scoped_refptr<AudioState> InitAudio();

  rtc::scoped_refptr<AudioProcessing> apm_;
  rtc::scoped_refptr<TestAudioDeviceModule> fake_audio_device_;

  std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory_;
  int next_video_ssrc_index_ = 0;
  int next_rtx_ssrc_index_ = 0;
  int next_audio_ssrc_index_ = 0;
  int next_priority_index_ = 0;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_CALL_CLIENT_H_
