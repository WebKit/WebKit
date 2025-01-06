/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/test_peer_factory.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/audio/audio_device.h"
#include "api/audio/audio_processing.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/create_time_controller.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/pclf/peer_configurer.h"
#include "api/test/time_controller.h"
#include "api/transport/field_trial_based_config.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "p2p/base/port_allocator.h"
#include "p2p/client/basic_port_allocator.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/file_wrapper.h"
#include "rtc_base/thread.h"
#include "test/pc/e2e/analyzer/video/quality_analyzing_video_encoder.h"
#include "test/pc/e2e/analyzer/video/video_quality_analyzer_injection_helper.h"
#include "test/pc/e2e/echo/echo_emulation.h"
#include "test/pc/e2e/test_peer.h"
#include "test/testsupport/copy_to_file_audio_capturer.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using EmulatedSFUConfigMap =
    ::webrtc::webrtc_pc_e2e::QualityAnalyzingVideoEncoder::EmulatedSFUConfigMap;

constexpr int16_t kGeneratedAudioMaxAmplitude = 32000;
constexpr int kDefaultSamplingFrequencyInHz = 48000;

// Sets mandatory entities in injectable components like `pcf_dependencies`
// and `pc_dependencies` if they are omitted. Also setup required
// dependencies, that won't be specially provided by factory and will be just
// transferred to peer connection creation code.
void SetMandatoryEntities(InjectableComponents* components) {
  RTC_DCHECK(components->pcf_dependencies);
  RTC_DCHECK(components->pc_dependencies);

  // Setup required peer connection factory dependencies.
  if (components->pcf_dependencies->event_log_factory == nullptr) {
    components->pcf_dependencies->event_log_factory =
        std::make_unique<RtcEventLogFactory>();
  }
  if (!components->pcf_dependencies->trials) {
    components->pcf_dependencies->trials =
        std::make_unique<FieldTrialBasedConfig>();
  }
}

// Returns mapping from stream label to optional spatial index.
// If we have stream label "Foo" and mapping contains
// 1. `std::nullopt` means all simulcast/SVC streams are required
// 2. Concrete value means that particular simulcast/SVC stream have to be
//    analyzed.
EmulatedSFUConfigMap CalculateRequiredSpatialIndexPerStream(
    const std::vector<VideoConfig>& video_configs) {
  EmulatedSFUConfigMap result;
  for (auto& video_config : video_configs) {
    // Stream label should be set by fixture implementation here.
    RTC_DCHECK(video_config.stream_label);
    bool res = result
                   .insert({*video_config.stream_label,
                            video_config.emulated_sfu_config})
                   .second;
    RTC_DCHECK(res) << "Duplicate video_config.stream_label="
                    << *video_config.stream_label;
  }
  return result;
}

std::unique_ptr<TestAudioDeviceModule::Renderer> CreateAudioRenderer(
    const std::optional<RemotePeerAudioConfig>& config) {
  if (!config) {
    // Return default renderer because we always require some renderer.
    return TestAudioDeviceModule::CreateDiscardRenderer(
        kDefaultSamplingFrequencyInHz);
  }
  if (config->output_file_name) {
    return TestAudioDeviceModule::CreateBoundedWavFileWriter(
        config->output_file_name.value(), config->sampling_frequency_in_hz);
  }
  return TestAudioDeviceModule::CreateDiscardRenderer(
      config->sampling_frequency_in_hz);
}

std::unique_ptr<TestAudioDeviceModule::Capturer> CreateAudioCapturer(
    const std::optional<AudioConfig>& audio_config) {
  if (!audio_config) {
    // If we have no audio config we still need to provide some audio device.
    // In such case use generated capturer. Despite of we provided audio here,
    // in test media setup audio stream won't be added into peer connection.
    return TestAudioDeviceModule::CreatePulsedNoiseCapturer(
        kGeneratedAudioMaxAmplitude, kDefaultSamplingFrequencyInHz);
  }
  if (audio_config->input_file_name) {
    return TestAudioDeviceModule::CreateWavFileReader(
        *audio_config->input_file_name, /*repeat=*/true);
  } else {
    return TestAudioDeviceModule::CreatePulsedNoiseCapturer(
        kGeneratedAudioMaxAmplitude, audio_config->sampling_frequency_in_hz);
  }
}

rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceModule(
    std::optional<AudioConfig> audio_config,
    std::optional<RemotePeerAudioConfig> remote_audio_config,
    std::optional<EchoEmulationConfig> echo_emulation_config,
    TaskQueueFactory* task_queue_factory) {
  std::unique_ptr<TestAudioDeviceModule::Renderer> renderer =
      CreateAudioRenderer(remote_audio_config);
  std::unique_ptr<TestAudioDeviceModule::Capturer> capturer =
      CreateAudioCapturer(audio_config);
  RTC_DCHECK(renderer);
  RTC_DCHECK(capturer);

  // Setup echo emulation if required.
  if (echo_emulation_config) {
    capturer = std::make_unique<EchoEmulatingCapturer>(std::move(capturer),
                                                       *echo_emulation_config);
    renderer = std::make_unique<EchoEmulatingRenderer>(
        std::move(renderer),
        static_cast<EchoEmulatingCapturer*>(capturer.get()));
  }

  // Setup input stream dumping if required.
  if (audio_config && audio_config->input_dump_file_name) {
    capturer = std::make_unique<test::CopyToFileAudioCapturer>(
        std::move(capturer), audio_config->input_dump_file_name.value());
  }

  return TestAudioDeviceModule::Create(task_queue_factory, std::move(capturer),
                                       std::move(renderer), /*speed=*/1.f);
}

void WrapVideoEncoderFactory(
    absl::string_view peer_name,
    double bitrate_multiplier,
    EmulatedSFUConfigMap stream_to_sfu_config,
    PeerConnectionFactoryComponents* pcf_dependencies,
    VideoQualityAnalyzerInjectionHelper* video_analyzer_helper) {
  std::unique_ptr<VideoEncoderFactory> video_encoder_factory;
  if (pcf_dependencies->video_encoder_factory != nullptr) {
    video_encoder_factory = std::move(pcf_dependencies->video_encoder_factory);
  } else {
    video_encoder_factory = CreateBuiltinVideoEncoderFactory();
  }
  pcf_dependencies->video_encoder_factory =
      video_analyzer_helper->WrapVideoEncoderFactory(
          peer_name, std::move(video_encoder_factory), bitrate_multiplier,
          std::move(stream_to_sfu_config));
}

void WrapVideoDecoderFactory(
    absl::string_view peer_name,
    PeerConnectionFactoryComponents* pcf_dependencies,
    VideoQualityAnalyzerInjectionHelper* video_analyzer_helper) {
  std::unique_ptr<VideoDecoderFactory> video_decoder_factory;
  if (pcf_dependencies->video_decoder_factory != nullptr) {
    video_decoder_factory = std::move(pcf_dependencies->video_decoder_factory);
  } else {
    video_decoder_factory = CreateBuiltinVideoDecoderFactory();
  }
  pcf_dependencies->video_decoder_factory =
      video_analyzer_helper->WrapVideoDecoderFactory(
          peer_name, std::move(video_decoder_factory));
}

// Creates PeerConnectionFactoryDependencies objects, providing entities
// from InjectableComponents::PeerConnectionFactoryComponents.
PeerConnectionFactoryDependencies CreatePCFDependencies(
    std::unique_ptr<PeerConnectionFactoryComponents> pcf_dependencies,
    TimeController& time_controller,
    rtc::scoped_refptr<AudioDeviceModule> audio_device_module,
    rtc::Thread* signaling_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* network_thread) {
  PeerConnectionFactoryDependencies pcf_deps;
  pcf_deps.signaling_thread = signaling_thread;
  pcf_deps.worker_thread = worker_thread;
  pcf_deps.network_thread = network_thread;

  pcf_deps.event_log_factory = std::move(pcf_dependencies->event_log_factory);
  pcf_deps.task_queue_factory = time_controller.CreateTaskQueueFactory();

  if (pcf_dependencies->fec_controller_factory != nullptr) {
    pcf_deps.fec_controller_factory =
        std::move(pcf_dependencies->fec_controller_factory);
  }
  if (pcf_dependencies->network_controller_factory != nullptr) {
    pcf_deps.network_controller_factory =
        std::move(pcf_dependencies->network_controller_factory);
  }
  if (pcf_dependencies->neteq_factory != nullptr) {
    pcf_deps.neteq_factory = std::move(pcf_dependencies->neteq_factory);
  }
  if (pcf_dependencies->trials != nullptr) {
    pcf_deps.trials = std::move(pcf_dependencies->trials);
  }

  // Media dependencies
  pcf_deps.adm = std::move(audio_device_module);
  if (pcf_dependencies->audio_processing != nullptr) {
    pcf_deps.audio_processing_builder =
        CustomAudioProcessing(pcf_dependencies->audio_processing);
  }
  pcf_deps.audio_mixer = pcf_dependencies->audio_mixer;
  pcf_deps.video_encoder_factory =
      std::move(pcf_dependencies->video_encoder_factory);
  pcf_deps.video_decoder_factory =
      std::move(pcf_dependencies->video_decoder_factory);
  pcf_deps.audio_encoder_factory = pcf_dependencies->audio_encoder_factory;
  pcf_deps.audio_decoder_factory = pcf_dependencies->audio_decoder_factory;
  EnableMediaWithDefaultsAndTimeController(time_controller, pcf_deps);

  return pcf_deps;
}

// Creates PeerConnectionDependencies objects, providing entities
// from InjectableComponents::PeerConnectionComponents.
PeerConnectionDependencies CreatePCDependencies(
    MockPeerConnectionObserver* observer,
    std::optional<uint32_t> port_allocator_flags,
    std::unique_ptr<PeerConnectionComponents> pc_dependencies) {
  PeerConnectionDependencies pc_deps(observer);

  auto port_allocator = std::make_unique<cricket::BasicPortAllocator>(
      pc_dependencies->network_manager, pc_dependencies->packet_socket_factory);

  // This test does not support TCP by default.
  int flags =
      cricket::kDefaultPortAllocatorFlags | cricket::PORTALLOCATOR_DISABLE_TCP;
  if (port_allocator_flags.has_value()) {
    flags = *port_allocator_flags;
  }
  port_allocator->set_flags(port_allocator->flags() | flags);

  pc_deps.allocator = std::move(port_allocator);

  if (pc_dependencies->async_dns_resolver_factory != nullptr) {
    pc_deps.async_dns_resolver_factory =
        std::move(pc_dependencies->async_dns_resolver_factory);
  }
  if (pc_dependencies->cert_generator != nullptr) {
    pc_deps.cert_generator = std::move(pc_dependencies->cert_generator);
  }
  if (pc_dependencies->tls_cert_verifier != nullptr) {
    pc_deps.tls_cert_verifier = std::move(pc_dependencies->tls_cert_verifier);
  }
  if (pc_dependencies->ice_transport_factory != nullptr) {
    pc_deps.ice_transport_factory =
        std::move(pc_dependencies->ice_transport_factory);
  }
  return pc_deps;
}

}  // namespace

std::optional<RemotePeerAudioConfig> RemotePeerAudioConfig::Create(
    std::optional<AudioConfig> config) {
  if (!config) {
    return std::nullopt;
  }
  return RemotePeerAudioConfig(config.value());
}

std::unique_ptr<TestPeer> TestPeerFactory::CreateTestPeer(
    std::unique_ptr<PeerConfigurer> configurer,
    std::unique_ptr<MockPeerConnectionObserver> observer,
    std::optional<RemotePeerAudioConfig> remote_audio_config,
    std::optional<EchoEmulationConfig> echo_emulation_config) {
  std::unique_ptr<InjectableComponents> components =
      configurer->ReleaseComponents();
  std::unique_ptr<Params> params = configurer->ReleaseParams();
  std::unique_ptr<ConfigurableParams> configurable_params =
      configurer->ReleaseConfigurableParams();
  std::vector<PeerConfigurer::VideoSource> video_sources =
      configurer->ReleaseVideoSources();
  RTC_DCHECK(components);
  RTC_DCHECK(params);
  RTC_DCHECK(configurable_params);
  RTC_DCHECK_EQ(configurable_params->video_configs.size(),
                video_sources.size());
  SetMandatoryEntities(components.get());
  params->rtc_configuration.sdp_semantics = SdpSemantics::kUnifiedPlan;

  // Create peer connection factory.
  rtc::scoped_refptr<AudioDeviceModule> audio_device_module =
      CreateAudioDeviceModule(params->audio_config, remote_audio_config,
                              echo_emulation_config,
                              time_controller_.GetTaskQueueFactory());
  WrapVideoEncoderFactory(
      params->name.value(), params->video_encoder_bitrate_multiplier,
      CalculateRequiredSpatialIndexPerStream(
          configurable_params->video_configs),
      components->pcf_dependencies.get(), video_analyzer_helper_);
  WrapVideoDecoderFactory(params->name.value(),
                          components->pcf_dependencies.get(),
                          video_analyzer_helper_);

  std::unique_ptr<rtc::Thread> owned_worker_thread =
      components->worker_thread != nullptr
          ? nullptr
          : time_controller_.CreateThread("worker_thread");
  if (components->worker_thread == nullptr) {
    components->worker_thread = owned_worker_thread.get();
  }

  PeerConnectionFactoryDependencies pcf_deps = CreatePCFDependencies(
      std::move(components->pcf_dependencies), time_controller_,
      std::move(audio_device_module), signaling_thread_,
      components->worker_thread, components->network_thread);
  rtc::scoped_refptr<PeerConnectionFactoryInterface> peer_connection_factory =
      CreateModularPeerConnectionFactory(std::move(pcf_deps));
  peer_connection_factory->SetOptions(params->peer_connection_factory_options);
  if (params->aec_dump_path) {
    peer_connection_factory->StartAecDump(
        FileWrapper::OpenWriteOnly(*params->aec_dump_path).Release(), -1);
  }

  // Create peer connection.
  PeerConnectionDependencies pc_deps =
      CreatePCDependencies(observer.get(), params->port_allocator_flags,
                           std::move(components->pc_dependencies));
  rtc::scoped_refptr<PeerConnectionInterface> peer_connection =
      peer_connection_factory
          ->CreatePeerConnectionOrError(params->rtc_configuration,
                                        std::move(pc_deps))
          .MoveValue();
  peer_connection->SetBitrate(params->bitrate_settings);

  return absl::WrapUnique(new TestPeer(
      peer_connection_factory, peer_connection, std::move(observer),
      std::move(*params), std::move(*configurable_params),
      std::move(video_sources), std::move(owned_worker_thread)));
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
