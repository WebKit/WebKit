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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "api/array_view.h"
#include "api/audio/builtin_audio_processing_builder.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/media_types.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/rtc_event_log_output.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/network_emulation/network_emulation_interfaces.h"
#include "api/test/time_controller.h"
#include "api/transport/bitrate_settings.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "call/audio_state.h"
#include "call/call.h"
#include "call/call_config.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "test/create_test_environment.h"
#include "test/logging/log_writer.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/network_node.h"
#include "test/scenario/scenario_config.h"

namespace webrtc {
namespace test {
namespace {
constexpr size_t kNumSsrcs = 6;
const uint32_t kSendRtxSsrcs[kNumSsrcs] = {0xBADCAFD, 0xBADCAFE, 0xBADCAFF,
                                           0xBADCB00, 0xBADCB01, 0xBADCB02};
const uint32_t kVideoSendSsrcs[kNumSsrcs] = {0xC0FFED, 0xC0FFEE, 0xC0FFEF,
                                             0xC0FFF0, 0xC0FFF1, 0xC0FFF2};
const uint32_t kVideoRecvLocalSsrcs[kNumSsrcs] = {0xDAB001, 0xDAB002, 0xDAB003,
                                                  0xDAB004, 0xDAB005, 0xDAB006};
const uint32_t kAudioSendSsrc = 0xDEADBEEF;
const uint32_t kReceiverLocalAudioSsrc = 0x1234567;

constexpr int kEventLogOutputIntervalMs = 5000;

CallClientFakeAudio InitAudio(const Environment& env) {
  CallClientFakeAudio setup;
  auto capturer = TestAudioDeviceModule::CreatePulsedNoiseCapturer(256, 48000);
  auto renderer = TestAudioDeviceModule::CreateDiscardRenderer(48000);
  setup.fake_audio_device = TestAudioDeviceModule::Create(
      env, std::move(capturer), std::move(renderer), 1.f);
  setup.apm = BuiltinAudioProcessingBuilder().Build(env);
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

std::unique_ptr<Call> CreateCall(
    const Environment& env,
    CallClientConfig config,
    LoggingNetworkControllerFactory* network_controller_factory,
    scoped_refptr<AudioState> audio_state) {
  CallConfig call_config(env);
  call_config.bitrate_config.max_bitrate_bps =
      config.transport.rates.max_rate.bps_or(-1);
  call_config.bitrate_config.min_bitrate_bps =
      config.transport.rates.min_rate.bps();
  call_config.bitrate_config.start_bitrate_bps =
      config.transport.rates.start_rate.bps();
  call_config.network_controller_factory = network_controller_factory;
  call_config.audio_state = audio_state;
  return Call::Create(std::move(call_config));
}

std::unique_ptr<RtcEventLog> CreateEventLog(
    const Environment& env,
    LogWriterFactoryInterface& log_writer_factory) {
  auto event_log = RtcEventLogFactory().Create(env);
  bool success = event_log->StartLogging(log_writer_factory.Create(".rtc.dat"),
                                         kEventLogOutputIntervalMs);
  RTC_CHECK(success);
  return event_log;
}
}  // namespace
NetworkControleUpdateCache::NetworkControleUpdateCache(
    std::unique_ptr<NetworkControllerInterface> controller)
    : controller_(std::move(controller)) {}
NetworkControlUpdate NetworkControleUpdateCache::OnNetworkAvailability(
    NetworkAvailability msg) {
  return Update(controller_->OnNetworkAvailability(msg));
}
NetworkControlUpdate NetworkControleUpdateCache::OnNetworkRouteChange(
    NetworkRouteChange msg) {
  return Update(controller_->OnNetworkRouteChange(msg));
}
NetworkControlUpdate NetworkControleUpdateCache::OnProcessInterval(
    ProcessInterval msg) {
  return Update(controller_->OnProcessInterval(msg));
}
NetworkControlUpdate NetworkControleUpdateCache::OnRemoteBitrateReport(
    RemoteBitrateReport msg) {
  return Update(controller_->OnRemoteBitrateReport(msg));
}
NetworkControlUpdate NetworkControleUpdateCache::OnRoundTripTimeUpdate(
    RoundTripTimeUpdate msg) {
  return Update(controller_->OnRoundTripTimeUpdate(msg));
}
NetworkControlUpdate NetworkControleUpdateCache::OnSentPacket(SentPacket msg) {
  return Update(controller_->OnSentPacket(msg));
}
NetworkControlUpdate NetworkControleUpdateCache::OnReceivedPacket(
    ReceivedPacket msg) {
  return Update(controller_->OnReceivedPacket(msg));
}
NetworkControlUpdate NetworkControleUpdateCache::OnStreamsConfig(
    StreamsConfig msg) {
  return Update(controller_->OnStreamsConfig(msg));
}
NetworkControlUpdate NetworkControleUpdateCache::OnTargetRateConstraints(
    TargetRateConstraints msg) {
  return Update(controller_->OnTargetRateConstraints(msg));
}
NetworkControlUpdate NetworkControleUpdateCache::OnTransportLossReport(
    TransportLossReport msg) {
  return Update(controller_->OnTransportLossReport(msg));
}
NetworkControlUpdate NetworkControleUpdateCache::OnTransportPacketsFeedback(
    TransportPacketsFeedback msg) {
  return Update(controller_->OnTransportPacketsFeedback(msg));
}
NetworkControlUpdate NetworkControleUpdateCache::OnNetworkStateEstimate(
    NetworkStateEstimate msg) {
  return Update(controller_->OnNetworkStateEstimate(msg));
}

NetworkControlUpdate NetworkControleUpdateCache::update_state() const {
  return update_state_;
}
NetworkControlUpdate NetworkControleUpdateCache::Update(
    NetworkControlUpdate update) {
  if (update.target_rate)
    update_state_.target_rate = update.target_rate;
  if (update.pacer_config)
    update_state_.pacer_config = update.pacer_config;
  if (update.congestion_window)
    update_state_.congestion_window = update.congestion_window;
  if (!update.probe_cluster_configs.empty())
    update_state_.probe_cluster_configs = update.probe_cluster_configs;
  return update;
}

LoggingNetworkControllerFactory::LoggingNetworkControllerFactory(
    LogWriterFactoryInterface* log_writer_factory,
    TransportControllerConfig config) {
  if (config.cc_factory) {
    cc_factory_ = config.cc_factory;
    if (log_writer_factory)
      RTC_LOG(LS_WARNING)
          << "Can't log controller state for injected network controllers";
  } else {
    if (log_writer_factory) {
      goog_cc_factory_.AttachWriter(
          log_writer_factory->Create(".cc_state.txt"));
      print_cc_state_ = true;
    }
    cc_factory_ = &goog_cc_factory_;
  }
}

LoggingNetworkControllerFactory::~LoggingNetworkControllerFactory() {}

void LoggingNetworkControllerFactory::LogCongestionControllerStats(
    Timestamp at_time) {
  if (print_cc_state_)
    goog_cc_factory_.PrintState(at_time);
}

NetworkControlUpdate LoggingNetworkControllerFactory::GetUpdate() const {
  if (last_controller_)
    return last_controller_->update_state();
  return NetworkControlUpdate();
}

std::unique_ptr<NetworkControllerInterface>
LoggingNetworkControllerFactory::Create(NetworkControllerConfig config) {
  auto controller =
      std::make_unique<NetworkControleUpdateCache>(cc_factory_->Create(config));
  last_controller_ = controller.get();
  return controller;
}

TimeDelta LoggingNetworkControllerFactory::GetProcessInterval() const {
  return cc_factory_->GetProcessInterval();
}

void LoggingNetworkControllerFactory::SetRemoteBitrateEstimate(
    RemoteBitrateReport msg) {
  if (last_controller_)
    last_controller_->OnRemoteBitrateReport(msg);
}

CallClient::CallClient(
    TimeController* time_controller,
    std::unique_ptr<LogWriterFactoryInterface> log_writer_factory,
    CallClientConfig config)
    : time_controller_(time_controller),
      env_(
          CreateTestEnvironment({.field_trials = std::move(config.field_trials),
                                 .time = time_controller_})),
      log_writer_factory_(std::move(log_writer_factory)),
      network_controller_factory_(log_writer_factory_.get(), config.transport),
      task_queue_(env_.task_queue_factory().CreateTaskQueue(
          "CallClient",
          TaskQueueFactory::Priority::kNormal)) {
  SendTask([this, config] {
    if (log_writer_factory_ != nullptr) {
      EnvironmentFactory env_factory(env_);
      env_factory.Set(CreateEventLog(env_, *log_writer_factory_));
      env_ = env_factory.Create();
    }
    fake_audio_setup_ = InitAudio(env_);

    call_ = CreateCall(env_, config, &network_controller_factory_,
                       fake_audio_setup_.audio_state);
    transport_ =
        std::make_unique<NetworkNodeTransport>(&env_.clock(), call_.get());
  });
}

CallClient::~CallClient() {
  SendTask([&] {
    call_.reset();
    fake_audio_setup_ = {};
    Event done;
    env_.event_log().StopLogging([&done] { done.Set(); });
    done.Wait(Event::kForever);
  });
}

ColumnPrinter CallClient::StatsPrinter() {
  return ColumnPrinter::Lambda(
      "pacer_delay call_send_bw",
      [this](SimpleStringBuilder& sb) {
        Call::Stats call_stats = call_->GetStats();
        sb.AppendFormat("%.3lf %.0lf", call_stats.pacer_delay_ms / 1000.0,
                        call_stats.send_bandwidth_bps / 8.0);
      },
      64);
}

Call::Stats CallClient::GetStats() {
  // This call needs to be made on the thread that `call_` was constructed on.
  Call::Stats stats;
  SendTask([this, &stats] { stats = call_->GetStats(); });
  return stats;
}

DataRate CallClient::target_rate() const {
  return network_controller_factory_.GetUpdate().target_rate->target_rate;
}

DataRate CallClient::padding_rate() const {
  return network_controller_factory_.GetUpdate().pacer_config->pad_rate();
}

void CallClient::SetRemoteBitrate(DataRate bitrate) {
  RemoteBitrateReport msg;
  msg.bandwidth = bitrate;
  msg.receive_time = env_.clock().CurrentTime();
  network_controller_factory_.SetRemoteBitrateEstimate(msg);
}

void CallClient::UpdateBitrateConstraints(
    const BitrateConstraints& constraints) {
  SendTask([this, &constraints]() {
    call_->GetTransportControllerSend()->SetSdpBitrateParameters(constraints);
  });
}

void CallClient::SetAudioReceiveRtpHeaderExtensions(
    ArrayView<RtpExtension> extensions) {
  SendTask([this, &extensions]() {
    audio_extensions_ = RtpHeaderExtensionMap(extensions);
  });
}

void CallClient::SetVideoReceiveRtpHeaderExtensions(
    ArrayView<RtpExtension> extensions) {
  SendTask([this, &extensions]() {
    video_extensions_ = RtpHeaderExtensionMap(extensions);
  });
}

void CallClient::OnPacketReceived(EmulatedIpPacket packet) {
  MediaType media_type = MediaType::ANY;
  if (IsRtpPacket(packet.data)) {
    media_type = ssrc_media_types_[ParseRtpSsrc(packet.data)];
    task_queue_.PostTask([this, media_type,
                          packet = std::move(packet)]() mutable {
      RtpHeaderExtensionMap& extension_map = media_type == MediaType::AUDIO
                                                 ? audio_extensions_
                                                 : video_extensions_;
      RtpPacketReceived received_packet(&extension_map, packet.arrival_time);
      RTC_CHECK(received_packet.Parse(packet.data));
      call_->Receiver()->DeliverRtpPacket(media_type, received_packet,
                                          /*undemuxable_packet_handler=*/
                                          [](const RtpPacketReceived& packet) {
                                            RTC_CHECK_NOTREACHED();
                                            return false;
                                          });
    });
  } else {
    task_queue_.PostTask(
        [call = call_.get(), packet = std::move(packet)]() mutable {
          call->Receiver()->DeliverRtcpPacket(packet.data);
        });
  }
}

std::unique_ptr<RtcEventLogOutput> CallClient::GetLogWriter(std::string name) {
  if (!log_writer_factory_ || name.empty())
    return nullptr;
  return log_writer_factory_->Create(name);
}

uint32_t CallClient::GetNextVideoSsrc() {
  RTC_CHECK_LT(next_video_ssrc_index_, kNumSsrcs);
  return kVideoSendSsrcs[next_video_ssrc_index_++];
}

uint32_t CallClient::GetNextVideoLocalSsrc() {
  RTC_CHECK_LT(next_video_local_ssrc_index_, kNumSsrcs);
  return kVideoRecvLocalSsrcs[next_video_local_ssrc_index_++];
}

uint32_t CallClient::GetNextAudioSsrc() {
  RTC_CHECK_LT(next_audio_ssrc_index_, 1);
  next_audio_ssrc_index_++;
  return kAudioSendSsrc;
}

uint32_t CallClient::GetNextAudioLocalSsrc() {
  RTC_CHECK_LT(next_audio_local_ssrc_index_, 1);
  next_audio_local_ssrc_index_++;
  return kReceiverLocalAudioSsrc;
}

uint32_t CallClient::GetNextRtxSsrc() {
  RTC_CHECK_LT(next_rtx_ssrc_index_, kNumSsrcs);
  return kSendRtxSsrcs[next_rtx_ssrc_index_++];
}

void CallClient::SendTask(std::function<void()> task) {
  task_queue_.SendTask(std::move(task));
}

void CallClient::UpdateNetworkAdapterId(int adapter_id) {
  transport_->UpdateAdapterId(adapter_id);
}

int16_t CallClient::Bind(EmulatedEndpoint* endpoint) {
  uint16_t port = endpoint->BindReceiver(0, this).value();
  endpoints_.push_back({endpoint, port});
  return port;
}

void CallClient::UnBind() {
  for (auto ep_port : endpoints_)
    ep_port.first->UnbindReceiver(ep_port.second);
}

CallClientPair::~CallClientPair() = default;

}  // namespace test
}  // namespace webrtc
