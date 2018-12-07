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
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "rtc_base/constructormagic.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/network_node.h"
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

struct CallClientFakeAudio {
  rtc::scoped_refptr<AudioProcessing> apm;
  rtc::scoped_refptr<TestAudioDeviceModule> fake_audio_device;
  rtc::scoped_refptr<AudioState> audio_state;
};
// CallClient represents a participant in a call scenario. It is created by the
// Scenario class and is used as sender and receiver when setting up a media
// stream session.
class CallClient : public NetworkReceiverInterface {
 public:
  CallClient(Clock* clock, std::string log_filename, CallClientConfig config);
  RTC_DISALLOW_COPY_AND_ASSIGN(CallClient);

  ~CallClient();
  ColumnPrinter StatsPrinter();
  Call::Stats GetStats();
  DataRate send_bandwidth() {
    return DataRate::bps(GetStats().send_bandwidth_bps);
  }

  bool TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                        uint64_t receiver,
                        Timestamp at_time) override;

 private:
  friend class Scenario;
  friend class CallClientPair;
  friend class SendVideoStream;
  friend class VideoStreamPair;
  friend class ReceiveVideoStream;
  friend class SendAudioStream;
  friend class ReceiveAudioStream;
  friend class AudioStreamPair;
  friend class NetworkNodeTransport;
  uint32_t GetNextVideoSsrc();
  uint32_t GetNextAudioSsrc();
  uint32_t GetNextRtxSsrc();
  std::string GetNextPriorityId();
  void AddExtensions(std::vector<RtpExtension> extensions);

  Clock* clock_;
  LoggingNetworkControllerFactory network_controller_factory_;
  CallClientFakeAudio fake_audio_setup_;
  std::unique_ptr<Call> call_;
  NetworkNodeTransport transport_;
  RtpHeaderParser* const header_parser_;

  std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory_;
  // Stores the configured overhead per known incomming route. This is used to
  // subtract the overhead before processing.
  std::map<uint64_t, DataSize> route_overhead_;
  int next_video_ssrc_index_ = 0;
  int next_rtx_ssrc_index_ = 0;
  int next_audio_ssrc_index_ = 0;
  int next_priority_index_ = 0;
  std::map<uint32_t, MediaType> ssrc_media_types_;
};

class CallClientPair {
 public:
  RTC_DISALLOW_COPY_AND_ASSIGN(CallClientPair);
  ~CallClientPair();
  CallClient* first() { return first_; }
  CallClient* second() { return second_; }
  std::pair<CallClient*, CallClient*> forward() { return {first(), second()}; }
  std::pair<CallClient*, CallClient*> reverse() { return {second(), first()}; }

 private:
  friend class Scenario;
  CallClientPair(CallClient* first, CallClient* second)
      : first_(first), second_(second) {}
  CallClient* const first_;
  CallClient* const second_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_CALL_CLIENT_H_
