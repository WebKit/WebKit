/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PC_E2E_PEER_CONFIGURER_H_
#define TEST_PC_E2E_PEER_CONFIGURER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/async_resolver_factory.h"
#include "api/audio/audio_mixer.h"
#include "api/call/call_factory_interface.h"
#include "api/fec_controller.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/create_peer_connection_quality_test_frame_generator.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/transport/network_control.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/network.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/thread.h"
#include "test/pc/e2e/peer_connection_quality_test_params.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class PeerConfigurerImpl final
    : public PeerConnectionE2EQualityTestFixture::PeerConfigurer {
 public:
  using VideoSource =
      absl::variant<std::unique_ptr<test::FrameGeneratorInterface>,
                    PeerConnectionE2EQualityTestFixture::CapturingDeviceIndex>;

  PeerConfigurerImpl(rtc::Thread* network_thread,
                     rtc::NetworkManager* network_manager,
                     rtc::PacketSocketFactory* packet_socket_factory)
      : components_(
            std::make_unique<InjectableComponents>(network_thread,
                                                   network_manager,
                                                   packet_socket_factory)),
        params_(std::make_unique<Params>()),
        configurable_params_(std::make_unique<ConfigurableParams>()) {}

  PeerConfigurer* SetName(absl::string_view name) override {
    params_->name = std::string(name);
    return this;
  }

  // Implementation of PeerConnectionE2EQualityTestFixture::PeerConfigurer.
  PeerConfigurer* SetTaskQueueFactory(
      std::unique_ptr<TaskQueueFactory> task_queue_factory) override {
    components_->pcf_dependencies->task_queue_factory =
        std::move(task_queue_factory);
    return this;
  }
  PeerConfigurer* SetCallFactory(
      std::unique_ptr<CallFactoryInterface> call_factory) override {
    components_->pcf_dependencies->call_factory = std::move(call_factory);
    return this;
  }
  PeerConfigurer* SetEventLogFactory(
      std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory) override {
    components_->pcf_dependencies->event_log_factory =
        std::move(event_log_factory);
    return this;
  }
  PeerConfigurer* SetFecControllerFactory(
      std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory)
      override {
    components_->pcf_dependencies->fec_controller_factory =
        std::move(fec_controller_factory);
    return this;
  }
  PeerConfigurer* SetNetworkControllerFactory(
      std::unique_ptr<NetworkControllerFactoryInterface>
          network_controller_factory) override {
    components_->pcf_dependencies->network_controller_factory =
        std::move(network_controller_factory);
    return this;
  }
  PeerConfigurer* SetVideoEncoderFactory(
      std::unique_ptr<VideoEncoderFactory> video_encoder_factory) override {
    components_->pcf_dependencies->video_encoder_factory =
        std::move(video_encoder_factory);
    return this;
  }
  PeerConfigurer* SetVideoDecoderFactory(
      std::unique_ptr<VideoDecoderFactory> video_decoder_factory) override {
    components_->pcf_dependencies->video_decoder_factory =
        std::move(video_decoder_factory);
    return this;
  }

  PeerConfigurer* SetAsyncResolverFactory(
      std::unique_ptr<webrtc::AsyncResolverFactory> async_resolver_factory)
      override {
    components_->pc_dependencies->async_resolver_factory =
        std::move(async_resolver_factory);
    return this;
  }
  PeerConfigurer* SetRTCCertificateGenerator(
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator)
      override {
    components_->pc_dependencies->cert_generator = std::move(cert_generator);
    return this;
  }
  PeerConfigurer* SetSSLCertificateVerifier(
      std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier) override {
    components_->pc_dependencies->tls_cert_verifier =
        std::move(tls_cert_verifier);
    return this;
  }

  PeerConfigurer* AddVideoConfig(
      PeerConnectionE2EQualityTestFixture::VideoConfig config) override {
    video_sources_.push_back(
        CreateSquareFrameGenerator(config, /*type=*/absl::nullopt));
    configurable_params_->video_configs.push_back(std::move(config));
    return this;
  }
  PeerConfigurer* AddVideoConfig(
      PeerConnectionE2EQualityTestFixture::VideoConfig config,
      std::unique_ptr<test::FrameGeneratorInterface> generator) override {
    configurable_params_->video_configs.push_back(std::move(config));
    video_sources_.push_back(std::move(generator));
    return this;
  }
  PeerConfigurer* AddVideoConfig(
      PeerConnectionE2EQualityTestFixture::VideoConfig config,
      PeerConnectionE2EQualityTestFixture::CapturingDeviceIndex index)
      override {
    configurable_params_->video_configs.push_back(std::move(config));
    video_sources_.push_back(index);
    return this;
  }
  PeerConfigurer* SetVideoSubscription(
      PeerConnectionE2EQualityTestFixture::VideoSubscription subscription)
      override {
    configurable_params_->video_subscription = std::move(subscription);
    return this;
  }
  PeerConfigurer* SetAudioConfig(
      PeerConnectionE2EQualityTestFixture::AudioConfig config) override {
    params_->audio_config = std::move(config);
    return this;
  }
  PeerConfigurer* SetUseUlpFEC(bool value) override {
    params_->use_ulp_fec = value;
    return this;
  }
  PeerConfigurer* SetUseFlexFEC(bool value) override {
    params_->use_flex_fec = value;
    return this;
  }
  PeerConfigurer* SetVideoEncoderBitrateMultiplier(double multiplier) override {
    params_->video_encoder_bitrate_multiplier = multiplier;
    return this;
  }
  PeerConfigurer* SetNetEqFactory(
      std::unique_ptr<NetEqFactory> neteq_factory) override {
    components_->pcf_dependencies->neteq_factory = std::move(neteq_factory);
    return this;
  }
  PeerConfigurer* SetAudioProcessing(
      rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing) override {
    components_->pcf_dependencies->audio_processing = audio_processing;
    return this;
  }
  PeerConfigurer* SetAudioMixer(
      rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer) override {
    components_->pcf_dependencies->audio_mixer = audio_mixer;
    return this;
  }
  PeerConfigurer* SetRtcEventLogPath(std::string path) override {
    params_->rtc_event_log_path = std::move(path);
    return this;
  }
  PeerConfigurer* SetAecDumpPath(std::string path) override {
    params_->aec_dump_path = std::move(path);
    return this;
  }
  PeerConfigurer* SetRTCConfiguration(
      PeerConnectionInterface::RTCConfiguration configuration) override {
    params_->rtc_configuration = std::move(configuration);
    return this;
  }
  PeerConfigurer* SetRTCOfferAnswerOptions(
      PeerConnectionInterface::RTCOfferAnswerOptions options) override {
    params_->rtc_offer_answer_options = std::move(options);
    return this;
  }
  PeerConfigurer* SetBitrateSettings(
      BitrateSettings bitrate_settings) override {
    params_->bitrate_settings = bitrate_settings;
    return this;
  }

  PeerConfigurer* SetIceTransportFactory(
      std::unique_ptr<IceTransportFactory> factory) override {
    components_->pc_dependencies->ice_transport_factory = std::move(factory);
    return this;
  }

  PeerConfigurer* SetPortAllocatorExtraFlags(uint32_t extra_flags) override {
    params_->port_allocator_extra_flags = extra_flags;
    return this;
  }
  // Implementation of PeerConnectionE2EQualityTestFixture::PeerConfigurer end.

  InjectableComponents* components() { return components_.get(); }
  Params* params() { return params_.get(); }
  ConfigurableParams* configurable_params() {
    return configurable_params_.get();
  }
  const Params& params() const { return *params_; }
  const ConfigurableParams& configurable_params() const {
    return *configurable_params_;
  }
  std::vector<VideoSource>* video_sources() { return &video_sources_; }

  // Returns InjectableComponents and transfer ownership to the caller.
  // Can be called once.
  std::unique_ptr<InjectableComponents> ReleaseComponents() {
    RTC_CHECK(components_);
    auto components = std::move(components_);
    components_ = nullptr;
    return components;
  }

  // Returns Params and transfer ownership to the caller.
  // Can be called once.
  std::unique_ptr<Params> ReleaseParams() {
    RTC_CHECK(params_);
    auto params = std::move(params_);
    params_ = nullptr;
    return params;
  }

  // Returns ConfigurableParams and transfer ownership to the caller.
  // Can be called once.
  std::unique_ptr<ConfigurableParams> ReleaseConfigurableParams() {
    RTC_CHECK(configurable_params_);
    auto configurable_params = std::move(configurable_params_);
    configurable_params_ = nullptr;
    return configurable_params;
  }

  // Returns video sources and transfer frame generators ownership to the
  // caller. Can be called once.
  std::vector<VideoSource> ReleaseVideoSources() {
    auto video_sources = std::move(video_sources_);
    video_sources_.clear();
    return video_sources;
  }

 private:
  std::unique_ptr<InjectableComponents> components_;
  std::unique_ptr<Params> params_;
  std::unique_ptr<ConfigurableParams> configurable_params_;
  std::vector<VideoSource> video_sources_;
};

class DefaultNamesProvider {
 public:
  // Caller have to ensure that default names array will outlive names provider
  // instance.
  explicit DefaultNamesProvider(
      absl::string_view prefix,
      rtc::ArrayView<const absl::string_view> default_names = {});

  void MaybeSetName(absl::optional<std::string>& name);

 private:
  std::string GenerateName();

  std::string GenerateNameInternal();

  const std::string prefix_;
  const rtc::ArrayView<const absl::string_view> default_names_;

  std::set<std::string> known_names_;
  size_t counter_ = 0;
};

class PeerParamsPreprocessor {
 public:
  PeerParamsPreprocessor();

  // Set missing params to default values if it is required:
  //  * Generate video stream labels if some of them are missing
  //  * Generate audio stream labels if some of them are missing
  //  * Set video source generation mode if it is not specified
  //  * Video codecs under test
  void SetDefaultValuesForMissingParams(PeerConfigurerImpl& peer);

  // Validate peer's parameters, also ensure uniqueness of all video stream
  // labels.
  void ValidateParams(const PeerConfigurerImpl& peer);

 private:
  DefaultNamesProvider peer_names_provider_;

  std::set<std::string> peer_names_;
  std::set<std::string> video_labels_;
  std::set<std::string> audio_labels_;
  std::set<std::string> video_sync_groups_;
  std::set<std::string> audio_sync_groups_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_PEER_CONFIGURER_H_
