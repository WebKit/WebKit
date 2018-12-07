/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/call_client.h"

#include <utility>

#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/congestion_controller/bbr/test/bbr_printer.h"
#include "modules/congestion_controller/goog_cc/test/goog_cc_printer.h"
#include "test/call_test.h"

namespace webrtc {
namespace test {
namespace {
const char* kPriorityStreamId = "priority-track";

CallClientFakeAudio InitAudio() {
  CallClientFakeAudio setup;
  auto capturer = TestAudioDeviceModule::CreatePulsedNoiseCapturer(256, 48000);
  auto renderer = TestAudioDeviceModule::CreateDiscardRenderer(48000);
  setup.fake_audio_device = TestAudioDeviceModule::CreateTestAudioDeviceModule(
      std::move(capturer), std::move(renderer), 1.f);
  setup.apm = AudioProcessingBuilder().Create();
  setup.fake_audio_device->Init();
  AudioState::Config audio_state_config;
  audio_state_config.audio_mixer = AudioMixerImpl::Create();
  audio_state_config.audio_processing = setup.apm;
  audio_state_config.audio_device_module = setup.fake_audio_device;
  setup.audio_state = AudioState::Create(audio_state_config);
  setup.fake_audio_device->RegisterAudioCallback(
      setup.audio_state->audio_transport());
  return setup;
}

Call* CreateCall(CallClientConfig config,
                 LoggingNetworkControllerFactory* network_controller_factory_,
                 rtc::scoped_refptr<AudioState> audio_state) {
  CallConfig call_config(network_controller_factory_->GetEventLog());
  call_config.bitrate_config.max_bitrate_bps =
      config.transport.rates.max_rate.bps_or(-1);
  call_config.bitrate_config.min_bitrate_bps =
      config.transport.rates.min_rate.bps();
  call_config.bitrate_config.start_bitrate_bps =
      config.transport.rates.start_rate.bps();
  call_config.network_controller_factory = network_controller_factory_;
  call_config.audio_state = audio_state;
  return Call::Create(call_config);
}
}

LoggingNetworkControllerFactory::LoggingNetworkControllerFactory(
    std::string filename,
    TransportControllerConfig config) {
  if (filename.empty()) {
    event_log_ = RtcEventLog::CreateNull();
  } else {
    event_log_ = RtcEventLog::Create(RtcEventLog::EncodingType::Legacy);
    bool success = event_log_->StartLogging(
        absl::make_unique<RtcEventLogOutputFile>(filename + ".rtc.dat",
                                                 RtcEventLog::kUnlimitedOutput),
        RtcEventLog::kImmediateOutput);
    RTC_CHECK(success);
    cc_out_ = fopen((filename + ".cc_state.txt").c_str(), "w");
    switch (config.cc) {
      case TransportControllerConfig::CongestionController::kBbr: {
        auto bbr_printer = absl::make_unique<BbrStatePrinter>();
        cc_factory_.reset(new BbrDebugFactory(bbr_printer.get()));
        cc_printer_.reset(
            new ControlStatePrinter(cc_out_, std::move(bbr_printer)));
        break;
      }
      case TransportControllerConfig::CongestionController::kGoogCc: {
        auto goog_printer = absl::make_unique<GoogCcStatePrinter>();
        cc_factory_.reset(
            new GoogCcDebugFactory(event_log_.get(), goog_printer.get()));
        cc_printer_.reset(
            new ControlStatePrinter(cc_out_, std::move(goog_printer)));
        break;
      }
      case TransportControllerConfig::CongestionController::kGoogCcFeedback: {
        auto goog_printer = absl::make_unique<GoogCcStatePrinter>();
        cc_factory_.reset(new GoogCcFeedbackDebugFactory(event_log_.get(),
                                                         goog_printer.get()));
        cc_printer_.reset(
            new ControlStatePrinter(cc_out_, std::move(goog_printer)));
        break;
      }
    }
    cc_printer_->PrintHeaders();
  }
  if (!cc_factory_) {
    switch (config.cc) {
      case TransportControllerConfig::CongestionController::kBbr:
        cc_factory_.reset(new BbrNetworkControllerFactory());
        break;
      case TransportControllerConfig::CongestionController::kGoogCcFeedback:
        cc_factory_.reset(
            new GoogCcFeedbackNetworkControllerFactory(event_log_.get()));
        break;
      case TransportControllerConfig::CongestionController::kGoogCc:
        cc_factory_.reset(new GoogCcNetworkControllerFactory(event_log_.get()));
        break;
    }
  }
}

LoggingNetworkControllerFactory::~LoggingNetworkControllerFactory() {
  if (cc_out_)
    fclose(cc_out_);
}

void LoggingNetworkControllerFactory::LogCongestionControllerStats(
    Timestamp at_time) {
  if (cc_printer_)
    cc_printer_->PrintState(at_time);
}

RtcEventLog* LoggingNetworkControllerFactory::GetEventLog() const {
  return event_log_.get();
}

std::unique_ptr<NetworkControllerInterface>
LoggingNetworkControllerFactory::Create(NetworkControllerConfig config) {
  return cc_factory_->Create(config);
}

TimeDelta LoggingNetworkControllerFactory::GetProcessInterval() const {
  return cc_factory_->GetProcessInterval();
}

CallClient::CallClient(Clock* clock,
                       std::string log_filename,
                       CallClientConfig config)
    : clock_(clock),
      network_controller_factory_(log_filename, config.transport),
      fake_audio_setup_(InitAudio()),
      call_(CreateCall(config,
                       &network_controller_factory_,
                       fake_audio_setup_.audio_state)),
      transport_(clock_, call_.get()),
      header_parser_(RtpHeaderParser::Create()) {
}  // namespace test

CallClient::~CallClient() {
  delete header_parser_;
}

ColumnPrinter CallClient::StatsPrinter() {
  return ColumnPrinter::Lambda(
      "pacer_delay call_send_bw",
      [this](rtc::SimpleStringBuilder& sb) {
        Call::Stats call_stats = call_->GetStats();
        sb.AppendFormat("%.3lf %.0lf", call_stats.pacer_delay_ms / 1000.0,
                        call_stats.send_bandwidth_bps / 8.0);
      },
      64);
}

Call::Stats CallClient::GetStats() {
  return call_->GetStats();
}

bool CallClient::TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                                  uint64_t receiver,
                                  Timestamp at_time) {
  // Removes added overhead before delivering packet to sender.
  RTC_DCHECK_GE(packet.size(), route_overhead_.at(receiver).bytes());
  packet.SetSize(packet.size() - route_overhead_.at(receiver).bytes());

  MediaType media_type = MediaType::ANY;
  if (!RtpHeaderParser::IsRtcp(packet.cdata(), packet.size())) {
    RTPHeader header;
    bool success =
        header_parser_->Parse(packet.cdata(), packet.size(), &header);
    if (!success)
      return false;
    media_type = ssrc_media_types_[header.ssrc];
  }
  call_->Receiver()->DeliverPacket(media_type, packet, at_time.us());
  return true;
}

uint32_t CallClient::GetNextVideoSsrc() {
  RTC_CHECK_LT(next_video_ssrc_index_, CallTest::kNumSsrcs);
  return CallTest::kVideoSendSsrcs[next_video_ssrc_index_++];
}

uint32_t CallClient::GetNextAudioSsrc() {
  RTC_CHECK_LT(next_audio_ssrc_index_, 1);
  next_audio_ssrc_index_++;
  return CallTest::kAudioSendSsrc;
}

uint32_t CallClient::GetNextRtxSsrc() {
  RTC_CHECK_LT(next_rtx_ssrc_index_, CallTest::kNumSsrcs);
  return CallTest::kSendRtxSsrcs[next_rtx_ssrc_index_++];
}

std::string CallClient::GetNextPriorityId() {
  RTC_CHECK_LT(next_priority_index_++, 1);
  return kPriorityStreamId;
}

void CallClient::AddExtensions(std::vector<RtpExtension> extensions) {
  for (const auto& extension : extensions)
    header_parser_->RegisterRtpHeaderExtension(extension);
}

CallClientPair::~CallClientPair() = default;

}  // namespace test
}  // namespace webrtc
