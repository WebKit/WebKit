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

#include <iostream>
#include <memory>
#include <utility>

#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/transport/network_types.h"
#include "call/call.h"
#include "call/rtp_transport_controller_send_factory.h"
#include "modules/audio_mixer/audio_mixer_impl.h"

namespace webrtc {
namespace test {
namespace {
static constexpr size_t kNumSsrcs = 6;
const uint32_t kSendRtxSsrcs[kNumSsrcs] = {0xBADCAFD, 0xBADCAFE, 0xBADCAFF,
                                           0xBADCB00, 0xBADCB01, 0xBADCB02};
const uint32_t kVideoSendSsrcs[kNumSsrcs] = {0xC0FFED, 0xC0FFEE, 0xC0FFEF,
                                             0xC0FFF0, 0xC0FFF1, 0xC0FFF2};
const uint32_t kVideoRecvLocalSsrcs[kNumSsrcs] = {0xDAB001, 0xDAB002, 0xDAB003,
                                                  0xDAB004, 0xDAB005, 0xDAB006};
const uint32_t kAudioSendSsrc = 0xDEADBEEF;
const uint32_t kReceiverLocalAudioSsrc = 0x1234567;

constexpr int kEventLogOutputIntervalMs = 5000;

CallClientFakeAudio InitAudio(TimeController* time_controller) {
  CallClientFakeAudio setup;
  auto capturer = TestAudioDeviceModule::CreatePulsedNoiseCapturer(256, 48000);
  auto renderer = TestAudioDeviceModule::CreateDiscardRenderer(48000);
  setup.fake_audio_device = TestAudioDeviceModule::Create(
      time_controller->GetTaskQueueFactory(), std::move(capturer),
      std::move(renderer), 1.f);
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

Call* CreateCall(TimeController* time_controller,
                 RtcEventLog* event_log,
                 CallClientConfig config,
                 LoggingNetworkControllerFactory* network_controller_factory,
                 rtc::scoped_refptr<AudioState> audio_state) {
  CallConfig call_config(event_log);
  call_config.bitrate_config.max_bitrate_bps =
      config.transport.rates.max_rate.bps_or(-1);
  call_config.bitrate_config.min_bitrate_bps =
      config.transport.rates.min_rate.bps();
  call_config.bitrate_config.start_bitrate_bps =
      config.transport.rates.start_rate.bps();
  call_config.task_queue_factory = time_controller->GetTaskQueueFactory();
  call_config.network_controller_factory = network_controller_factory;
  call_config.audio_state = audio_state;
  call_config.trials = config.field_trials;
  Clock* clock = time_controller->GetClock();
  return Call::Create(call_config, clock,
                      RtpTransportControllerSendFactory().Create(
                          call_config.ExtractTransportConfig(), clock));
}

std::unique_ptr<RtcEventLog> CreateEventLog(
    TaskQueueFactory* task_queue_factory,
    LogWriterFactoryInterface* log_writer_factory) {
  if (!log_writer_factory) {
    return std::make_unique<RtcEventLogNull>();
  }
  auto event_log = RtcEventLogFactory(task_queue_factory)
                       .CreateRtcEventLog(RtcEventLog::EncodingType::NewFormat);
  bool success = event_log->StartLogging(log_writer_factory->Create(".rtc.dat"),
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
      clock_(time_controller->GetClock()),
      log_writer_factory_(std::move(log_writer_factory)),
      network_controller_factory_(log_writer_factory_.get(), config.transport),
      header_parser_(RtpHeaderParser::CreateForTest()),
      task_queue_(time_controller->GetTaskQueueFactory()->CreateTaskQueue(
          "CallClient",
          TaskQueueFactory::Priority::NORMAL)) {
  config.field_trials = &field_trials_;
  SendTask([this, config] {
    event_log_ = CreateEventLog(time_controller_->GetTaskQueueFactory(),
                                log_writer_factory_.get());
    fake_audio_setup_ = InitAudio(time_controller_);

    call_.reset(CreateCall(time_controller_, event_log_.get(), config,
                           &network_controller_factory_,
                           fake_audio_setup_.audio_state));
    transport_ = std::make_unique<NetworkNodeTransport>(clock_, call_.get());
  });
}

CallClient::~CallClient() {
  SendTask([&] {
    call_.reset();
    fake_audio_setup_ = {};
    rtc::Event done;
    event_log_->StopLogging([&done] { done.Set(); });
    done.Wait(rtc::Event::kForever);
    event_log_.reset();
  });
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
  // This call needs to be made on the thread that |call_| was constructed on.
  Call::Stats stats;
  SendTask([this, &stats] { stats = call_->GetStats(); });
  return stats;
}

DataRate CallClient::target_rate() const {
  return network_controller_factory_.GetUpdate().target_rate->target_rate;
}

DataRate CallClient::stable_target_rate() const {
  return network_controller_factory_.GetUpdate()
      .target_rate->stable_target_rate;
}

DataRate CallClient::padding_rate() const {
  return network_controller_factory_.GetUpdate().pacer_config->pad_rate();
}

void CallClient::SetRemoteBitrate(DataRate bitrate) {
  RemoteBitrateReport msg;
  msg.bandwidth = bitrate;
  msg.receive_time = clock_->CurrentTime();
  network_controller_factory_.SetRemoteBitrateEstimate(msg);
}

void CallClient::UpdateBitrateConstraints(
    const BitrateConstraints& constraints) {
  SendTask([this, &constraints]() {
    call_->GetTransportControllerSend()->SetSdpBitrateParameters(constraints);
  });
}

void CallClient::OnPacketReceived(EmulatedIpPacket packet) {
  MediaType media_type = MediaType::ANY;
  if (!RtpHeaderParser::IsRtcp(packet.cdata(), packet.data.size())) {
    auto ssrc = RtpHeaderParser::GetSsrc(packet.cdata(), packet.data.size());
    RTC_CHECK(ssrc.has_value());
    media_type = ssrc_media_types_[*ssrc];
  }
  task_queue_.PostTask(
      [call = call_.get(), media_type, packet = std::move(packet)]() mutable {
        call->Receiver()->DeliverPacket(media_type, packet.data,
                                        packet.arrival_time.us());
      });
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

void CallClient::AddExtensions(std::vector<RtpExtension> extensions) {
  for (const auto& extension : extensions)
    header_parser_->RegisterRtpHeaderExtension(extension);
}

void CallClient::SendTask(std::function<void()> task) {
  task_queue_.SendTask(std::move(task));
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
