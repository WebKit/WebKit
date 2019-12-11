/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PC_E2E_PEER_CONNECTION_QUALITY_TEST_H_
#define TEST_PC_E2E_PEER_CONNECTION_QUALITY_TEST_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/audio_quality_analyzer_interface.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "pc/test/frame_generator_capturer_video_track_source.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/pc/e2e/analyzer/video/single_process_encoded_image_data_injector.h"
#include "test/pc/e2e/analyzer/video/video_quality_analyzer_injection_helper.h"
#include "test/pc/e2e/analyzer_helper.h"
#include "test/pc/e2e/peer_connection_quality_test_params.h"
#include "test/pc/e2e/sdp/sdp_changer.h"
#include "test/pc/e2e/test_peer.h"
#include "test/testsupport/video_frame_writer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class PeerConfigurerImpl final
    : public PeerConnectionE2EQualityTestFixture::PeerConfigurer {
 public:
  PeerConfigurerImpl(rtc::Thread* network_thread,
                     rtc::NetworkManager* network_manager)
      : components_(absl::make_unique<InjectableComponents>(network_thread,
                                                            network_manager)),
        params_(absl::make_unique<Params>()) {}

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
  PeerConfigurer* SetMediaTransportFactory(
      std::unique_ptr<MediaTransportFactory> media_transport_factory) override {
    components_->pcf_dependencies->media_transport_factory =
        std::move(media_transport_factory);
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
    params_->video_configs.push_back(std::move(config));
    return this;
  }
  PeerConfigurer* SetAudioConfig(
      PeerConnectionE2EQualityTestFixture::AudioConfig config) override {
    params_->audio_config = std::move(config);
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
  PeerConfigurer* SetBitrateParameters(
      PeerConnectionInterface::BitrateParameters bitrate_params) override {
    params_->bitrate_params = bitrate_params;
    return this;
  }

 protected:
  friend class PeerConnectionE2EQualityTest;

  std::unique_ptr<InjectableComponents> ReleaseComponents() {
    return std::move(components_);
  }
  std::unique_ptr<Params> ReleaseParams() { return std::move(params_); }

 private:
  std::unique_ptr<InjectableComponents> components_;
  std::unique_ptr<Params> params_;
};

class PeerConnectionE2EQualityTest
    : public PeerConnectionE2EQualityTestFixture {
 public:
  using VideoGeneratorType =
      PeerConnectionE2EQualityTestFixture::VideoGeneratorType;
  using RunParams = PeerConnectionE2EQualityTestFixture::RunParams;
  using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;
  using VideoSimulcastConfig =
      PeerConnectionE2EQualityTestFixture::VideoSimulcastConfig;
  using PeerConfigurer = PeerConnectionE2EQualityTestFixture::PeerConfigurer;
  using QualityMetricsReporter =
      PeerConnectionE2EQualityTestFixture::QualityMetricsReporter;

  PeerConnectionE2EQualityTest(
      std::string test_case_name,
      std::unique_ptr<AudioQualityAnalyzerInterface> audio_quality_analyzer,
      std::unique_ptr<VideoQualityAnalyzerInterface> video_quality_analyzer);

  ~PeerConnectionE2EQualityTest() override = default;

  void ExecuteAt(TimeDelta target_time_since_start,
                 std::function<void(TimeDelta)> func) override;
  void ExecuteEvery(TimeDelta initial_delay_since_start,
                    TimeDelta interval,
                    std::function<void(TimeDelta)> func) override;

  void AddQualityMetricsReporter(std::unique_ptr<QualityMetricsReporter>
                                     quality_metrics_reporter) override;

  void AddPeer(rtc::Thread* network_thread,
               rtc::NetworkManager* network_manager,
               rtc::FunctionView<void(PeerConfigurer*)> configurer) override;
  void Run(RunParams run_params) override;

  TimeDelta GetRealTestDuration() const override {
    rtc::CritScope crit(&lock_);
    RTC_CHECK_NE(real_test_duration_, TimeDelta::Zero());
    return real_test_duration_;
  }

 private:
  struct ScheduledActivity {
    ScheduledActivity(TimeDelta initial_delay_since_start,
                      absl::optional<TimeDelta> interval,
                      std::function<void(TimeDelta)> func);

    TimeDelta initial_delay_since_start;
    absl::optional<TimeDelta> interval;
    std::function<void(TimeDelta)> func;
  };

  void ExecuteTask(TimeDelta initial_delay_since_start,
                   absl::optional<TimeDelta> interval,
                   std::function<void(TimeDelta)> func);
  void PostTask(ScheduledActivity activity) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Set missing params to default values if it is required:
  //  * Generate video stream labels if some of them missed
  //  * Generate audio stream labels if some of them missed
  //  * Set video source generation mode if it is not specified
  void SetDefaultValuesForMissingParams(std::vector<Params*> params);
  // Validate peer's parameters, also ensure uniqueness of all video stream
  // labels.
  void ValidateParams(const RunParams& run_params, std::vector<Params*> params);
  // For some functionality some field trials have to be enabled, so we will
  // enable them here.
  void SetupRequiredFieldTrials(const RunParams& run_params);
  void OnTrackCallback(rtc::scoped_refptr<RtpTransceiverInterface> transceiver,
                       std::vector<VideoConfig> remote_video_configs);
  // Have to be run on the signaling thread.
  void SetupCallOnSignalingThread(const RunParams& run_params);
  void TearDownCallOnSignalingThread();
  std::vector<rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource>>
  MaybeAddMedia(TestPeer* peer);
  std::vector<rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource>>
  MaybeAddVideo(TestPeer* peer);
  std::unique_ptr<test::FrameGenerator> CreateFrameGenerator(
      const VideoConfig& video_config);
  std::unique_ptr<test::FrameGenerator> CreateScreenShareFrameGenerator(
      const VideoConfig& video_config);
  void MaybeAddAudio(TestPeer* peer);
  void SetPeerCodecPreferences(TestPeer* peer, const RunParams& run_params);
  void SetupCall(const RunParams& run_params);
  void ExchangeOfferAnswer(SignalingInterceptor* signaling_interceptor);
  void ExchangeIceCandidates(SignalingInterceptor* signaling_interceptor);
  void StartVideo(
      const std::vector<
          rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource>>& sources);
  void TearDownCall();
  test::VideoFrameWriter* MaybeCreateVideoWriter(
      absl::optional<std::string> file_name,
      const VideoConfig& config);
  Timestamp Now() const;

  Clock* const clock_;
  const std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  std::string test_case_name_;
  std::unique_ptr<VideoQualityAnalyzerInjectionHelper>
      video_quality_analyzer_injection_helper_;
  std::unique_ptr<SingleProcessEncodedImageDataInjector>
      encoded_image_id_controller_;
  std::unique_ptr<AudioQualityAnalyzerInterface> audio_quality_analyzer_;

  std::vector<std::unique_ptr<PeerConfigurerImpl>> peer_configurations_;

  std::unique_ptr<test::ScopedFieldTrials> override_field_trials_ = nullptr;

  std::unique_ptr<TestPeer> alice_;
  std::unique_ptr<TestPeer> bob_;
  std::vector<std::unique_ptr<QualityMetricsReporter>>
      quality_metrics_reporters_;

  std::vector<rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource>>
      alice_video_sources_;
  std::vector<rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource>>
      bob_video_sources_;
  std::vector<std::unique_ptr<test::VideoFrameWriter>> video_writers_;
  std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>
      output_video_sinks_;
  AnalyzerHelper analyzer_helper_;

  rtc::CriticalSection lock_;
  // Time when test call was started. Minus infinity means that call wasn't
  // started yet.
  Timestamp start_time_ RTC_GUARDED_BY(lock_) = Timestamp::MinusInfinity();
  TimeDelta real_test_duration_ RTC_GUARDED_BY(lock_) = TimeDelta::Zero();
  // Queue of activities that were added before test call was started.
  // Activities from this queue will be posted on the |task_queue_| after test
  // call will be set up and then this queue will be unused.
  std::queue<ScheduledActivity> scheduled_activities_ RTC_GUARDED_BY(lock_);
  // List of task handles for activities, that are posted on |task_queue_| as
  // repeated during the call.
  std::vector<RepeatingTaskHandle> repeating_task_handles_
      RTC_GUARDED_BY(lock_);

  RepeatingTaskHandle stats_polling_task_ RTC_GUARDED_BY(&task_queue_);

  // Task queue, that is used for running activities during test call.
  // This task queue will be created before call set up and will be destroyed
  // immediately before call tear down.
  std::unique_ptr<TaskQueueForTest> task_queue_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_PEER_CONNECTION_QUALITY_TEST_H_
