/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/peer_connection_quality_test.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log_output_file.h"
#include "api/scoped_refptr.h"
#include "api/test/time_controller.h"
#include "api/test/video_quality_analyzer_interface.h"
#include "pc/sdp_utils.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/gunit.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/cpu_info.h"
#include "system_wrappers/include/field_trial.h"
#include "test/pc/e2e/analyzer/audio/default_audio_quality_analyzer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"
#include "test/pc/e2e/analyzer/video/video_quality_metrics_reporter.h"
#include "test/pc/e2e/cross_media_metrics_reporter.h"
#include "test/pc/e2e/stats_poller.h"
#include "test/pc/e2e/test_peer_factory.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;
using VideoCodecConfig = PeerConnectionE2EQualityTestFixture::VideoCodecConfig;

constexpr TimeDelta kDefaultTimeout = TimeDelta::Seconds(10);
constexpr char kSignalThreadName[] = "signaling_thread";
// 1 signaling, 2 network, 2 worker and 2 extra for codecs etc.
constexpr int kPeerConnectionUsedThreads = 7;
// Framework has extra thread for network layer and extra thread for peer
// connection stats polling.
constexpr int kFrameworkUsedThreads = 2;
constexpr int kMaxVideoAnalyzerThreads = 8;

constexpr TimeDelta kStatsUpdateInterval = TimeDelta::Seconds(1);

constexpr TimeDelta kAliveMessageLogInterval = TimeDelta::Seconds(30);

constexpr TimeDelta kQuickTestModeRunDuration = TimeDelta::Millis(100);

// Field trials to enable Flex FEC advertising and receiving.
constexpr char kFlexFecEnabledFieldTrials[] =
    "WebRTC-FlexFEC-03-Advertised/Enabled/WebRTC-FlexFEC-03/Enabled/";

class FixturePeerConnectionObserver : public MockPeerConnectionObserver {
 public:
  // |on_track_callback| will be called when any new track will be added to peer
  // connection.
  // |on_connected_callback| will be called when peer connection will come to
  // either connected or completed state. Client should notice that in the case
  // of reconnect this callback can be called again, so it should be tolerant
  // to such behavior.
  FixturePeerConnectionObserver(
      std::function<void(rtc::scoped_refptr<RtpTransceiverInterface>)>
          on_track_callback,
      std::function<void()> on_connected_callback)
      : on_track_callback_(std::move(on_track_callback)),
        on_connected_callback_(std::move(on_connected_callback)) {}

  void OnTrack(
      rtc::scoped_refptr<RtpTransceiverInterface> transceiver) override {
    MockPeerConnectionObserver::OnTrack(transceiver);
    on_track_callback_(transceiver);
  }

  void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override {
    MockPeerConnectionObserver::OnIceConnectionChange(new_state);
    if (ice_connected_) {
      on_connected_callback_();
    }
  }

 private:
  std::function<void(rtc::scoped_refptr<RtpTransceiverInterface>)>
      on_track_callback_;
  std::function<void()> on_connected_callback_;
};

}  // namespace

PeerConnectionE2EQualityTest::PeerConnectionE2EQualityTest(
    std::string test_case_name,
    TimeController& time_controller,
    std::unique_ptr<AudioQualityAnalyzerInterface> audio_quality_analyzer,
    std::unique_ptr<VideoQualityAnalyzerInterface> video_quality_analyzer)
    : time_controller_(time_controller),
      task_queue_factory_(time_controller_.CreateTaskQueueFactory()),
      test_case_name_(std::move(test_case_name)),
      executor_(std::make_unique<TestActivitiesExecutor>(
          time_controller_.GetClock())) {
  // Create default video quality analyzer. We will always create an analyzer,
  // even if there are no video streams, because it will be installed into video
  // encoder/decoder factories.
  if (video_quality_analyzer == nullptr) {
    video_quality_analyzer = std::make_unique<DefaultVideoQualityAnalyzer>(
        time_controller_.GetClock());
  }
  encoded_image_id_controller_ =
      std::make_unique<SingleProcessEncodedImageDataInjector>();
  video_quality_analyzer_injection_helper_ =
      std::make_unique<VideoQualityAnalyzerInjectionHelper>(
          std::move(video_quality_analyzer), encoded_image_id_controller_.get(),
          encoded_image_id_controller_.get());

  if (audio_quality_analyzer == nullptr) {
    audio_quality_analyzer = std::make_unique<DefaultAudioQualityAnalyzer>();
  }
  audio_quality_analyzer_.swap(audio_quality_analyzer);
}

void PeerConnectionE2EQualityTest::ExecuteAt(
    TimeDelta target_time_since_start,
    std::function<void(TimeDelta)> func) {
  executor_->ScheduleActivity(target_time_since_start, absl::nullopt, func);
}

void PeerConnectionE2EQualityTest::ExecuteEvery(
    TimeDelta initial_delay_since_start,
    TimeDelta interval,
    std::function<void(TimeDelta)> func) {
  executor_->ScheduleActivity(initial_delay_since_start, interval, func);
}

void PeerConnectionE2EQualityTest::AddQualityMetricsReporter(
    std::unique_ptr<QualityMetricsReporter> quality_metrics_reporter) {
  quality_metrics_reporters_.push_back(std::move(quality_metrics_reporter));
}

void PeerConnectionE2EQualityTest::AddPeer(
    rtc::Thread* network_thread,
    rtc::NetworkManager* network_manager,
    rtc::FunctionView<void(PeerConfigurer*)> configurer) {
  peer_configurations_.push_back(
      std::make_unique<PeerConfigurerImpl>(network_thread, network_manager));
  configurer(peer_configurations_.back().get());
}

void PeerConnectionE2EQualityTest::Run(RunParams run_params) {
  SetDefaultValuesForMissingParams(&run_params, &peer_configurations_);
  ValidateParams(run_params, peer_configurations_);
  RTC_CHECK_EQ(peer_configurations_.size(), 2)
      << "Only peer to peer calls are allowed, please add 2 peers";

  std::unique_ptr<PeerConfigurerImpl> alice_configurer =
      std::move(peer_configurations_[0]);
  std::unique_ptr<PeerConfigurerImpl> bob_configurer =
      std::move(peer_configurations_[1]);
  peer_configurations_.clear();

  for (size_t i = 0; i < bob_configurer->params()->video_configs.size(); ++i) {
    // We support simulcast only from caller.
    RTC_CHECK(!bob_configurer->params()->video_configs[i].simulcast_config)
        << "Only simulcast stream from first peer is supported";
  }

  SetupRequiredFieldTrials(run_params);

  // Print test summary
  RTC_LOG(INFO) << "Media quality test: " << *alice_configurer->params()->name
                << " will make a call to " << *bob_configurer->params()->name
                << " with media video="
                << !alice_configurer->params()->video_configs.empty()
                << "; audio="
                << alice_configurer->params()->audio_config.has_value() << ". "
                << *bob_configurer->params()->name
                << " will respond with media video="
                << !bob_configurer->params()->video_configs.empty()
                << "; audio="
                << bob_configurer->params()->audio_config.has_value();

  const std::unique_ptr<rtc::Thread> signaling_thread =
      time_controller_.CreateThread(kSignalThreadName);
  media_helper_ = std::make_unique<MediaHelper>(
      video_quality_analyzer_injection_helper_.get(), task_queue_factory_.get(),
      time_controller_.GetClock());

  // Create a |task_queue_|.
  task_queue_ = std::make_unique<webrtc::TaskQueueForTest>(
      time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
          "pc_e2e_quality_test", webrtc::TaskQueueFactory::Priority::NORMAL));

  // Create call participants: Alice and Bob.
  // Audio streams are intercepted in AudioDeviceModule, so if it is required to
  // catch output of Alice's stream, Alice's output_dump_file_name should be
  // passed to Bob's TestPeer setup as audio output file name.
  absl::optional<RemotePeerAudioConfig> alice_remote_audio_config =
      RemotePeerAudioConfig::Create(bob_configurer->params()->audio_config);
  absl::optional<RemotePeerAudioConfig> bob_remote_audio_config =
      RemotePeerAudioConfig::Create(alice_configurer->params()->audio_config);
  // Copy Alice and Bob video configs and names to correctly pass them into
  // lambdas.
  std::vector<VideoConfig> alice_video_configs =
      alice_configurer->params()->video_configs;
  std::string alice_name = alice_configurer->params()->name.value();
  std::vector<VideoConfig> bob_video_configs =
      bob_configurer->params()->video_configs;
  std::string bob_name = bob_configurer->params()->name.value();

  TestPeerFactory test_peer_factory(
      signaling_thread.get(), time_controller_,
      video_quality_analyzer_injection_helper_.get(), task_queue_.get());
  alice_ = test_peer_factory.CreateTestPeer(
      std::move(alice_configurer),
      std::make_unique<FixturePeerConnectionObserver>(
          [this, bob_video_configs, alice_name](
              rtc::scoped_refptr<RtpTransceiverInterface> transceiver) {
            OnTrackCallback(alice_name, transceiver, bob_video_configs);
          },
          [this]() { StartVideo(alice_video_sources_); }),
      alice_remote_audio_config, run_params.video_encoder_bitrate_multiplier,
      run_params.echo_emulation_config);
  bob_ = test_peer_factory.CreateTestPeer(
      std::move(bob_configurer),
      std::make_unique<FixturePeerConnectionObserver>(
          [this, alice_video_configs,
           bob_name](rtc::scoped_refptr<RtpTransceiverInterface> transceiver) {
            OnTrackCallback(bob_name, transceiver, alice_video_configs);
          },
          [this]() { StartVideo(bob_video_sources_); }),
      bob_remote_audio_config, run_params.video_encoder_bitrate_multiplier,
      run_params.echo_emulation_config);

  int num_cores = CpuInfo::DetectNumberOfCores();
  RTC_DCHECK_GE(num_cores, 1);

  int video_analyzer_threads =
      num_cores - kPeerConnectionUsedThreads - kFrameworkUsedThreads;
  if (video_analyzer_threads <= 0) {
    video_analyzer_threads = 1;
  }
  video_analyzer_threads =
      std::min(video_analyzer_threads, kMaxVideoAnalyzerThreads);
  RTC_LOG(INFO) << "video_analyzer_threads=" << video_analyzer_threads;
  quality_metrics_reporters_.push_back(
      std::make_unique<VideoQualityMetricsReporter>(
          time_controller_.GetClock()));
  quality_metrics_reporters_.push_back(
      std::make_unique<CrossMediaMetricsReporter>());

  video_quality_analyzer_injection_helper_->Start(
      test_case_name_,
      std::vector<std::string>{alice_->params()->name.value(),
                               bob_->params()->name.value()},
      video_analyzer_threads);
  audio_quality_analyzer_->Start(test_case_name_, &analyzer_helper_);
  for (auto& reporter : quality_metrics_reporters_) {
    reporter->Start(test_case_name_, &analyzer_helper_);
  }

  // Start RTCEventLog recording if requested.
  if (alice_->params()->rtc_event_log_path) {
    auto alice_rtc_event_log = std::make_unique<webrtc::RtcEventLogOutputFile>(
        alice_->params()->rtc_event_log_path.value());
    alice_->pc()->StartRtcEventLog(std::move(alice_rtc_event_log),
                                   webrtc::RtcEventLog::kImmediateOutput);
  }
  if (bob_->params()->rtc_event_log_path) {
    auto bob_rtc_event_log = std::make_unique<webrtc::RtcEventLogOutputFile>(
        bob_->params()->rtc_event_log_path.value());
    bob_->pc()->StartRtcEventLog(std::move(bob_rtc_event_log),
                                 webrtc::RtcEventLog::kImmediateOutput);
  }

  // Setup alive logging. It is done to prevent test infra to think that test is
  // dead.
  RepeatingTaskHandle::DelayedStart(task_queue_->Get(),
                                    kAliveMessageLogInterval, []() {
                                      std::printf("Test is still running...\n");
                                      return kAliveMessageLogInterval;
                                    });

  RTC_LOG(INFO) << "Configuration is done. Now " << *alice_->params()->name
                << " is calling to " << *bob_->params()->name << "...";

  // Setup stats poller.
  std::vector<StatsObserverInterface*> observers = {
      audio_quality_analyzer_.get(),
      video_quality_analyzer_injection_helper_.get()};
  for (auto& reporter : quality_metrics_reporters_) {
    observers.push_back(reporter.get());
  }
  StatsPoller stats_poller(observers, {{*alice_->params()->name, alice_.get()},
                                       {*bob_->params()->name, bob_.get()}});
  executor_->ScheduleActivity(TimeDelta::Zero(), kStatsUpdateInterval,
                              [&stats_poller](TimeDelta) {
                                stats_poller.PollStatsAndNotifyObservers();
                              });

  // Setup call.
  signaling_thread->Invoke<void>(RTC_FROM_HERE, [this, &run_params] {
    SetupCallOnSignalingThread(run_params);
  });
  std::unique_ptr<SignalingInterceptor> signaling_interceptor =
      CreateSignalingInterceptor(run_params);
  // Connect peers.
  signaling_thread->Invoke<void>(RTC_FROM_HERE, [this, &signaling_interceptor] {
    ExchangeOfferAnswer(signaling_interceptor.get());
  });
  WaitUntilIceCandidatesGathered(signaling_thread.get());

  signaling_thread->Invoke<void>(RTC_FROM_HERE, [this, &signaling_interceptor] {
    ExchangeIceCandidates(signaling_interceptor.get());
  });
  WaitUntilPeersAreConnected(signaling_thread.get());

  executor_->Start(task_queue_.get());
  Timestamp start_time = Now();

  bool is_quick_test_enabled = field_trial::IsEnabled("WebRTC-QuickPerfTest");
  if (is_quick_test_enabled) {
    time_controller_.AdvanceTime(kQuickTestModeRunDuration);
  } else {
    time_controller_.AdvanceTime(run_params.run_duration);
  }

  RTC_LOG(INFO) << "Test is done, initiating disconnect sequence.";

  // Stop all client started tasks to prevent their access to any call related
  // objects after these objects will be destroyed during call tear down.
  executor_->Stop();
  // There is no guarantee, that last stats collection will happen at the end
  // of the call, so we force it after executor, which is among others is doing
  // stats collection, was stopped.
  task_queue_->SendTask(
      [&stats_poller]() {
        // Get final end-of-call stats.
        stats_poller.PollStatsAndNotifyObservers();
      },
      RTC_FROM_HERE);
  // We need to detach AEC dumping from peers, because dump uses |task_queue_|
  // inside.
  alice_->DetachAecDump();
  bob_->DetachAecDump();
  // Tear down the call.
  signaling_thread->Invoke<void>(RTC_FROM_HERE,
                                 [this] { TearDownCallOnSignalingThread(); });

  Timestamp end_time = Now();
  RTC_LOG(INFO) << "All peers are disconnected.";
  {
    MutexLock lock(&lock_);
    real_test_duration_ = end_time - start_time;
  }

  ReportGeneralTestResults();
  audio_quality_analyzer_->Stop();
  video_quality_analyzer_injection_helper_->Stop();
  for (auto& reporter : quality_metrics_reporters_) {
    reporter->StopAndReportResults();
  }

  // Reset |task_queue_| after test to cleanup.
  task_queue_.reset();

  alice_ = nullptr;
  bob_ = nullptr;
  // Ensuring that TestVideoCapturerVideoTrackSource are destroyed on the right
  // thread.
  RTC_CHECK(alice_video_sources_.empty());
  RTC_CHECK(bob_video_sources_.empty());
}

void PeerConnectionE2EQualityTest::SetupRequiredFieldTrials(
    const RunParams& run_params) {
  std::string field_trials = "";
  if (run_params.use_flex_fec) {
    field_trials += kFlexFecEnabledFieldTrials;
  }
  if (!field_trials.empty()) {
    override_field_trials_ = std::make_unique<test::ScopedFieldTrials>(
        field_trial::GetFieldTrialString() + field_trials);
  }
}

void PeerConnectionE2EQualityTest::OnTrackCallback(
    absl::string_view peer_name,
    rtc::scoped_refptr<RtpTransceiverInterface> transceiver,
    std::vector<VideoConfig> remote_video_configs) {
  const rtc::scoped_refptr<MediaStreamTrackInterface>& track =
      transceiver->receiver()->track();
  RTC_CHECK_EQ(transceiver->receiver()->stream_ids().size(), 2)
      << "Expected 2 stream ids: 1st - sync group, 2nd - unique stream label";
  std::string sync_group = transceiver->receiver()->stream_ids()[0];
  std::string stream_label = transceiver->receiver()->stream_ids()[1];
  analyzer_helper_.AddTrackToStreamMapping(track->id(), stream_label,
                                           sync_group);
  if (track->kind() != MediaStreamTrackInterface::kVideoKind) {
    return;
  }

  // It is safe to cast here, because it is checked above that
  // track->kind() is kVideoKind.
  auto* video_track = static_cast<VideoTrackInterface*>(track.get());
  std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>> video_sink =
      video_quality_analyzer_injection_helper_->CreateVideoSink(peer_name);
  video_track->AddOrUpdateSink(video_sink.get(), rtc::VideoSinkWants());
  output_video_sinks_.push_back(std::move(video_sink));
}

void PeerConnectionE2EQualityTest::SetupCallOnSignalingThread(
    const RunParams& run_params) {
  // We need receive-only transceivers for Bob's media stream, so there will
  // be media section in SDP for that streams in Alice's offer, because it is
  // forbidden to add new media sections in answer in Unified Plan.
  RtpTransceiverInit receive_only_transceiver_init;
  receive_only_transceiver_init.direction = RtpTransceiverDirection::kRecvOnly;
  int alice_transceivers_counter = 0;
  if (bob_->params()->audio_config) {
    // Setup receive audio transceiver if Bob has audio to send. If we'll need
    // multiple audio streams, then we need transceiver for each Bob's audio
    // stream.
    RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> result =
        alice_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO,
                               receive_only_transceiver_init);
    RTC_CHECK(result.ok());
    alice_transceivers_counter++;
  }

  size_t alice_video_transceivers_non_simulcast_counter = 0;
  for (auto& video_config : alice_->params()->video_configs) {
    RtpTransceiverInit transceiver_params;
    if (video_config.simulcast_config) {
      transceiver_params.direction = RtpTransceiverDirection::kSendOnly;
      // Because simulcast enabled |run_params.video_codecs| has only 1 element.
      if (run_params.video_codecs[0].name == cricket::kVp8CodecName) {
        // For Vp8 simulcast we need to add as many RtpEncodingParameters to the
        // track as many simulcast streams requested. If they specified in
        // |video_config.simulcast_config| it should be copied from there.
        for (int i = 0;
             i < video_config.simulcast_config->simulcast_streams_count; ++i) {
          RtpEncodingParameters enc_params;
          if (video_config.simulcast_config->encoding_params.size() > 0) {
            enc_params = video_config.simulcast_config->encoding_params[i];
          }
          // We need to be sure, that all rids will be unique with all mids.
          enc_params.rid = std::to_string(alice_transceivers_counter) + "000" +
                           std::to_string(i);
          transceiver_params.send_encodings.push_back(enc_params);
        }
      }
    } else {
      transceiver_params.direction = RtpTransceiverDirection::kSendRecv;
      RtpEncodingParameters enc_params;
      enc_params.max_bitrate_bps = video_config.max_encode_bitrate_bps;
      enc_params.min_bitrate_bps = video_config.min_encode_bitrate_bps;
      transceiver_params.send_encodings.push_back(enc_params);

      alice_video_transceivers_non_simulcast_counter++;
    }
    RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> result =
        alice_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO,
                               transceiver_params);
    RTC_CHECK(result.ok());

    alice_transceivers_counter++;
  }

  // Add receive only transceivers in case Bob has more video_configs than
  // Alice.
  for (size_t i = alice_video_transceivers_non_simulcast_counter;
       i < bob_->params()->video_configs.size(); ++i) {
    RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> result =
        alice_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO,
                               receive_only_transceiver_init);
    RTC_CHECK(result.ok());
    alice_transceivers_counter++;
  }

  // Then add media for Alice and Bob
  media_helper_->MaybeAddAudio(alice_.get());
  alice_video_sources_ = media_helper_->MaybeAddVideo(alice_.get());
  media_helper_->MaybeAddAudio(bob_.get());
  bob_video_sources_ = media_helper_->MaybeAddVideo(bob_.get());

  SetPeerCodecPreferences(alice_.get(), run_params);
  SetPeerCodecPreferences(bob_.get(), run_params);
}

void PeerConnectionE2EQualityTest::TearDownCallOnSignalingThread() {
  TearDownCall();
}

void PeerConnectionE2EQualityTest::SetPeerCodecPreferences(
    TestPeer* peer,
    const RunParams& run_params) {
  std::vector<RtpCodecCapability> with_rtx_video_capabilities =
      FilterVideoCodecCapabilities(
          run_params.video_codecs, true, run_params.use_ulp_fec,
          run_params.use_flex_fec,
          peer->pc_factory()
              ->GetRtpSenderCapabilities(cricket::MediaType::MEDIA_TYPE_VIDEO)
              .codecs);
  std::vector<RtpCodecCapability> without_rtx_video_capabilities =
      FilterVideoCodecCapabilities(
          run_params.video_codecs, false, run_params.use_ulp_fec,
          run_params.use_flex_fec,
          peer->pc_factory()
              ->GetRtpSenderCapabilities(cricket::MediaType::MEDIA_TYPE_VIDEO)
              .codecs);

  // Set codecs for transceivers
  for (auto transceiver : peer->pc()->GetTransceivers()) {
    if (transceiver->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO) {
      if (transceiver->sender()->init_send_encodings().size() > 1) {
        // If transceiver's sender has more then 1 send encodings, it means it
        // has multiple simulcast streams, so we need disable RTX on it.
        RTCError result =
            transceiver->SetCodecPreferences(without_rtx_video_capabilities);
        RTC_CHECK(result.ok());
      } else {
        RTCError result =
            transceiver->SetCodecPreferences(with_rtx_video_capabilities);
        RTC_CHECK(result.ok());
      }
    }
  }
}

std::unique_ptr<SignalingInterceptor>
PeerConnectionE2EQualityTest::CreateSignalingInterceptor(
    const RunParams& run_params) {
  std::map<std::string, int> stream_label_to_simulcast_streams_count;
  // We add only Alice here, because simulcast/svc is supported only from the
  // first peer.
  for (auto& video_config : alice_->params()->video_configs) {
    if (video_config.simulcast_config) {
      stream_label_to_simulcast_streams_count.insert(
          {*video_config.stream_label,
           video_config.simulcast_config->simulcast_streams_count});
    }
  }
  PatchingParams patching_params(run_params.video_codecs,
                                 run_params.use_conference_mode,
                                 stream_label_to_simulcast_streams_count);
  return std::make_unique<SignalingInterceptor>(patching_params);
}

void PeerConnectionE2EQualityTest::WaitUntilIceCandidatesGathered(
    rtc::Thread* signaling_thread) {
  ASSERT_TRUE(time_controller_.Wait(
      [&]() {
        return signaling_thread->Invoke<bool>(RTC_FROM_HERE, [&]() {
          return alice_->IsIceGatheringDone() && bob_->IsIceGatheringDone();
        });
      },
      2 * kDefaultTimeout));
}

void PeerConnectionE2EQualityTest::WaitUntilPeersAreConnected(
    rtc::Thread* signaling_thread) {
  // This means that ICE and DTLS are connected.
  alice_connected_ = time_controller_.Wait(
      [&]() {
        return signaling_thread->Invoke<bool>(
            RTC_FROM_HERE, [&]() { return alice_->IsIceConnected(); });
      },
      kDefaultTimeout);
  bob_connected_ = time_controller_.Wait(
      [&]() {
        return signaling_thread->Invoke<bool>(
            RTC_FROM_HERE, [&]() { return bob_->IsIceConnected(); });
      },
      kDefaultTimeout);
}

void PeerConnectionE2EQualityTest::ExchangeOfferAnswer(
    SignalingInterceptor* signaling_interceptor) {
  std::string log_output;

  auto offer = alice_->CreateOffer();
  RTC_CHECK(offer);
  offer->ToString(&log_output);
  RTC_LOG(INFO) << "Original offer: " << log_output;
  LocalAndRemoteSdp patch_result =
      signaling_interceptor->PatchOffer(std::move(offer));
  patch_result.local_sdp->ToString(&log_output);
  RTC_LOG(INFO) << "Offer to set as local description: " << log_output;
  patch_result.remote_sdp->ToString(&log_output);
  RTC_LOG(INFO) << "Offer to set as remote description: " << log_output;

  bool set_local_offer =
      alice_->SetLocalDescription(std::move(patch_result.local_sdp));
  RTC_CHECK(set_local_offer);
  bool set_remote_offer =
      bob_->SetRemoteDescription(std::move(patch_result.remote_sdp));
  RTC_CHECK(set_remote_offer);
  auto answer = bob_->CreateAnswer();
  RTC_CHECK(answer);
  answer->ToString(&log_output);
  RTC_LOG(INFO) << "Original answer: " << log_output;
  patch_result = signaling_interceptor->PatchAnswer(std::move(answer));
  patch_result.local_sdp->ToString(&log_output);
  RTC_LOG(INFO) << "Answer to set as local description: " << log_output;
  patch_result.remote_sdp->ToString(&log_output);
  RTC_LOG(INFO) << "Answer to set as remote description: " << log_output;

  bool set_local_answer =
      bob_->SetLocalDescription(std::move(patch_result.local_sdp));
  RTC_CHECK(set_local_answer);
  bool set_remote_answer =
      alice_->SetRemoteDescription(std::move(patch_result.remote_sdp));
  RTC_CHECK(set_remote_answer);
}

void PeerConnectionE2EQualityTest::ExchangeIceCandidates(
    SignalingInterceptor* signaling_interceptor) {
  // Connect an ICE candidate pairs.
  std::vector<std::unique_ptr<IceCandidateInterface>> alice_candidates =
      signaling_interceptor->PatchOffererIceCandidates(
          alice_->observer()->GetAllCandidates());
  for (auto& candidate : alice_candidates) {
    std::string candidate_str;
    RTC_CHECK(candidate->ToString(&candidate_str));
    RTC_LOG(INFO) << *alice_->params()->name
                  << " ICE candidate(mid= " << candidate->sdp_mid()
                  << "): " << candidate_str;
  }
  ASSERT_TRUE(bob_->AddIceCandidates(std::move(alice_candidates)));
  std::vector<std::unique_ptr<IceCandidateInterface>> bob_candidates =
      signaling_interceptor->PatchAnswererIceCandidates(
          bob_->observer()->GetAllCandidates());
  for (auto& candidate : bob_candidates) {
    std::string candidate_str;
    RTC_CHECK(candidate->ToString(&candidate_str));
    RTC_LOG(INFO) << *bob_->params()->name
                  << " ICE candidate(mid= " << candidate->sdp_mid()
                  << "): " << candidate_str;
  }
  ASSERT_TRUE(alice_->AddIceCandidates(std::move(bob_candidates)));
}

void PeerConnectionE2EQualityTest::StartVideo(
    const std::vector<rtc::scoped_refptr<TestVideoCapturerVideoTrackSource>>&
        sources) {
  for (auto& source : sources) {
    if (source->state() != MediaSourceInterface::SourceState::kLive) {
      source->Start();
    }
  }
}

void PeerConnectionE2EQualityTest::TearDownCall() {
  for (const auto& video_source : alice_video_sources_) {
    video_source->Stop();
  }
  for (const auto& video_source : bob_video_sources_) {
    video_source->Stop();
  }

  alice_video_sources_.clear();
  bob_video_sources_.clear();

  alice_->Close();
  bob_->Close();

  media_helper_ = nullptr;
}

void PeerConnectionE2EQualityTest::ReportGeneralTestResults() {
  test::PrintResult(*alice_->params()->name + "_connected", "", test_case_name_,
                    alice_connected_, "unitless",
                    /*important=*/false,
                    test::ImproveDirection::kBiggerIsBetter);
  test::PrintResult(*bob_->params()->name + "_connected", "", test_case_name_,
                    bob_connected_, "unitless",
                    /*important=*/false,
                    test::ImproveDirection::kBiggerIsBetter);
}

Timestamp PeerConnectionE2EQualityTest::Now() const {
  return time_controller_.GetClock()->CurrentTime();
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
