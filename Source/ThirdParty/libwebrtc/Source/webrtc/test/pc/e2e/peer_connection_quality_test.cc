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
#include <set>
#include <utility>

#include "absl/memory/memory.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log_output_file.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/test/video_quality_analyzer_interface.h"
#include "api/units/time_delta.h"
#include "pc/sdp_utils.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/bind.h"
#include "rtc_base/gunit.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/cpu_info.h"
#include "system_wrappers/include/field_trial.h"
#include "test/pc/e2e/analyzer/audio/default_audio_quality_analyzer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"
#include "test/pc/e2e/stats_poller.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;

constexpr int kDefaultTimeoutMs = 10000;
constexpr char kSignalThreadName[] = "signaling_thread";
// 1 signaling, 2 network, 2 worker and 2 extra for codecs etc.
constexpr int kPeerConnectionUsedThreads = 7;
// Framework has extra thread for network layer and extra thread for peer
// connection stats polling.
constexpr int kFrameworkUsedThreads = 2;
constexpr int kMaxVideoAnalyzerThreads = 8;

constexpr TimeDelta kStatsUpdateInterval = TimeDelta::Seconds<1>();

constexpr TimeDelta kAliveMessageLogInterval = TimeDelta::Seconds<30>();

constexpr int kQuickTestModeRunDurationMs = 100;

// Field trials to enable Flex FEC advertising and receiving.
constexpr char kFlexFecEnabledFieldTrials[] =
    "WebRTC-FlexFEC-03-Advertised/Enabled/WebRTC-FlexFEC-03/Enabled/";

std::string VideoConfigSourcePresenceToString(const VideoConfig& video_config) {
  char buf[1024];
  rtc::SimpleStringBuilder builder(buf);
  builder << "video_config.generator=" << video_config.generator.has_value()
          << "; video_config.input_file_name="
          << video_config.input_file_name.has_value()
          << "; video_config.screen_share_config="
          << video_config.screen_share_config.has_value() << ";";
  return builder.str();
}

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
    std::unique_ptr<AudioQualityAnalyzerInterface> audio_quality_analyzer,
    std::unique_ptr<VideoQualityAnalyzerInterface> video_quality_analyzer)
    : clock_(Clock::GetRealTimeClock()),
      task_queue_factory_(CreateDefaultTaskQueueFactory()),
      test_case_name_(std::move(test_case_name)) {
  // Create default video quality analyzer. We will always create an analyzer,
  // even if there are no video streams, because it will be installed into video
  // encoder/decoder factories.
  if (video_quality_analyzer == nullptr) {
    video_quality_analyzer = absl::make_unique<DefaultVideoQualityAnalyzer>();
  }
  encoded_image_id_controller_ =
      absl::make_unique<SingleProcessEncodedImageDataInjector>();
  video_quality_analyzer_injection_helper_ =
      absl::make_unique<VideoQualityAnalyzerInjectionHelper>(
          std::move(video_quality_analyzer), encoded_image_id_controller_.get(),
          encoded_image_id_controller_.get());

  if (audio_quality_analyzer == nullptr) {
    audio_quality_analyzer = absl::make_unique<DefaultAudioQualityAnalyzer>();
  }
  audio_quality_analyzer_.swap(audio_quality_analyzer);
}

void PeerConnectionE2EQualityTest::ExecuteAt(
    TimeDelta target_time_since_start,
    std::function<void(TimeDelta)> func) {
  ExecuteTask(target_time_since_start, absl::nullopt, func);
}

void PeerConnectionE2EQualityTest::ExecuteEvery(
    TimeDelta initial_delay_since_start,
    TimeDelta interval,
    std::function<void(TimeDelta)> func) {
  ExecuteTask(initial_delay_since_start, interval, func);
}

void PeerConnectionE2EQualityTest::ExecuteTask(
    TimeDelta initial_delay_since_start,
    absl::optional<TimeDelta> interval,
    std::function<void(TimeDelta)> func) {
  RTC_CHECK(initial_delay_since_start.IsFinite() &&
            initial_delay_since_start >= TimeDelta::Zero());
  RTC_CHECK(!interval ||
            (interval->IsFinite() && *interval > TimeDelta::Zero()));
  rtc::CritScope crit(&lock_);
  ScheduledActivity activity(initial_delay_since_start, interval, func);
  if (start_time_.IsInfinite()) {
    scheduled_activities_.push(std::move(activity));
  } else {
    PostTask(std::move(activity));
  }
}

void PeerConnectionE2EQualityTest::PostTask(ScheduledActivity activity) {
  // Because start_time_ will never change at this point copy it to local
  // variable to capture in in lambda without requirement to hold a lock.
  Timestamp start_time = start_time_;

  TimeDelta remaining_delay =
      activity.initial_delay_since_start == TimeDelta::Zero()
          ? TimeDelta::Zero()
          : activity.initial_delay_since_start - (Now() - start_time_);
  if (remaining_delay < TimeDelta::Zero()) {
    RTC_LOG(WARNING) << "Executing late task immediately, late by="
                     << ToString(remaining_delay.Abs());
    remaining_delay = TimeDelta::Zero();
  }

  if (activity.interval) {
    if (remaining_delay == TimeDelta::Zero()) {
      repeating_task_handles_.push_back(RepeatingTaskHandle::Start(
          task_queue_->Get(), [activity, start_time, this]() {
            activity.func(Now() - start_time);
            return *activity.interval;
          }));
      return;
    }
    repeating_task_handles_.push_back(RepeatingTaskHandle::DelayedStart(
        task_queue_->Get(), remaining_delay, [activity, start_time, this]() {
          activity.func(Now() - start_time);
          return *activity.interval;
        }));
    return;
  }

  if (remaining_delay == TimeDelta::Zero()) {
    task_queue_->PostTask(
        [activity, start_time, this]() { activity.func(Now() - start_time); });
    return;
  }

  task_queue_->PostDelayedTask(
      [activity, start_time, this]() { activity.func(Now() - start_time); },
      remaining_delay.ms());
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
      absl::make_unique<PeerConfigurerImpl>(network_thread, network_manager));
  configurer(peer_configurations_.back().get());
}

void PeerConnectionE2EQualityTest::Run(RunParams run_params) {
  RTC_CHECK_EQ(peer_configurations_.size(), 2)
      << "Only peer to peer calls are allowed, please add 2 peers";

  std::unique_ptr<Params> alice_params =
      peer_configurations_[0]->ReleaseParams();
  std::unique_ptr<InjectableComponents> alice_components =
      peer_configurations_[0]->ReleaseComponents();
  std::unique_ptr<Params> bob_params = peer_configurations_[1]->ReleaseParams();
  std::unique_ptr<InjectableComponents> bob_components =
      peer_configurations_[1]->ReleaseComponents();
  peer_configurations_.clear();

  SetDefaultValuesForMissingParams({alice_params.get(), bob_params.get()});
  ValidateParams(run_params, {alice_params.get(), bob_params.get()});
  SetupRequiredFieldTrials(run_params);

  // Print test summary
  RTC_LOG(INFO)
      << "Media quality test: Alice will make a call to Bob with media video="
      << !alice_params->video_configs.empty()
      << "; audio=" << alice_params->audio_config.has_value()
      << ". Bob will respond with media video="
      << !bob_params->video_configs.empty()
      << "; audio=" << bob_params->audio_config.has_value();

  const std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName(kSignalThreadName, nullptr);
  signaling_thread->Start();

  // Create a |task_queue_|.
  task_queue_ = absl::make_unique<TaskQueueForTest>("pc_e2e_quality_test");

  // Create call participants: Alice and Bob.
  // Audio streams are intercepted in AudioDeviceModule, so if it is required to
  // catch output of Alice's stream, Alice's output_dump_file_name should be
  // passed to Bob's TestPeer setup as audio output file name.
  absl::optional<TestPeer::RemotePeerAudioConfig> alice_remote_audio_config =
      TestPeer::CreateRemoteAudioConfig(bob_params->audio_config);
  absl::optional<TestPeer::RemotePeerAudioConfig> bob_remote_audio_config =
      TestPeer::CreateRemoteAudioConfig(alice_params->audio_config);
  // Copy Alice and Bob video configs to correctly pass them into lambdas.
  std::vector<VideoConfig> alice_video_configs = alice_params->video_configs;
  std::vector<VideoConfig> bob_video_configs = bob_params->video_configs;

  alice_ = TestPeer::CreateTestPeer(
      std::move(alice_components), std::move(alice_params),
      absl::make_unique<FixturePeerConnectionObserver>(
          [this, bob_video_configs](
              rtc::scoped_refptr<RtpTransceiverInterface> transceiver) {
            OnTrackCallback(transceiver, bob_video_configs);
          },
          [this]() { StartVideo(alice_video_sources_); }),
      video_quality_analyzer_injection_helper_.get(), signaling_thread.get(),
      alice_remote_audio_config, run_params.video_encoder_bitrate_multiplier,
      run_params.echo_emulation_config, task_queue_.get());
  bob_ = TestPeer::CreateTestPeer(
      std::move(bob_components), std::move(bob_params),
      absl::make_unique<FixturePeerConnectionObserver>(
          [this, alice_video_configs](
              rtc::scoped_refptr<RtpTransceiverInterface> transceiver) {
            OnTrackCallback(transceiver, alice_video_configs);
          },
          [this]() { StartVideo(bob_video_sources_); }),
      video_quality_analyzer_injection_helper_.get(), signaling_thread.get(),
      bob_remote_audio_config, run_params.video_encoder_bitrate_multiplier,
      run_params.echo_emulation_config, task_queue_.get());

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

  video_quality_analyzer_injection_helper_->Start(test_case_name_,
                                                  video_analyzer_threads);
  audio_quality_analyzer_->Start(test_case_name_, &analyzer_helper_);
  for (auto& reporter : quality_metrics_reporters_) {
    reporter->Start(test_case_name_);
  }

  // Start RTCEventLog recording if requested.
  if (alice_->params()->rtc_event_log_path) {
    auto alice_rtc_event_log = absl::make_unique<webrtc::RtcEventLogOutputFile>(
        alice_->params()->rtc_event_log_path.value());
    alice_->pc()->StartRtcEventLog(std::move(alice_rtc_event_log),
                                   webrtc::RtcEventLog::kImmediateOutput);
  }
  if (bob_->params()->rtc_event_log_path) {
    auto bob_rtc_event_log = absl::make_unique<webrtc::RtcEventLogOutputFile>(
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

  RTC_LOG(INFO) << "Configuration is done. Now Alice is calling to Bob...";

  // Setup call.
  signaling_thread->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&PeerConnectionE2EQualityTest::SetupCallOnSignalingThread, this,
                run_params));
  {
    rtc::CritScope crit(&lock_);
    start_time_ = Now();
    while (!scheduled_activities_.empty()) {
      PostTask(std::move(scheduled_activities_.front()));
      scheduled_activities_.pop();
    }
  }

  std::vector<StatsObserverInterface*> observers = {
      audio_quality_analyzer_.get(),
      video_quality_analyzer_injection_helper_.get()};
  for (auto& reporter : quality_metrics_reporters_) {
    observers.push_back(reporter.get());
  }
  StatsPoller stats_poller(observers,
                           {{"alice", alice_.get()}, {"bob", bob_.get()}});

  task_queue_->PostTask([&stats_poller, this]() {
    RTC_DCHECK_RUN_ON(task_queue_.get());
    stats_polling_task_ =
        RepeatingTaskHandle::Start(task_queue_->Get(), [this, &stats_poller]() {
          RTC_DCHECK_RUN_ON(task_queue_.get());
          stats_poller.PollStatsAndNotifyObservers();
          return kStatsUpdateInterval;
        });
  });

  rtc::Event done;
  bool is_quick_test_enabled = field_trial::IsEnabled("WebRTC-QuickPerfTest");
  if (is_quick_test_enabled) {
    done.Wait(kQuickTestModeRunDurationMs);
  } else {
    done.Wait(run_params.run_duration.ms());
  }

  RTC_LOG(INFO) << "Test is done, initiating disconnect sequence.";

  task_queue_->SendTask([this]() {
    RTC_DCHECK_RUN_ON(task_queue_.get());
    stats_polling_task_.Stop();
  });

  // We need to detach AEC dumping from peers, because dump uses |task_queue_|
  // inside.
  alice_->DetachAecDump();
  bob_->DetachAecDump();
  // Stop all client started tasks on task queue to prevent their access to any
  // call related objects after these objects will be destroyed during call tear
  // down.
  task_queue_->SendTask([this]() {
    rtc::CritScope crit(&lock_);
    for (auto& handle : repeating_task_handles_) {
      handle.Stop();
    }
  });
  // Tear down the call.
  signaling_thread->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&PeerConnectionE2EQualityTest::TearDownCallOnSignalingThread,
                this));
  Timestamp end_time = Now();
  RTC_LOG(INFO) << "All peers are disconnected.";
  {
    rtc::CritScope crit(&lock_);
    real_test_duration_ = end_time - start_time_;
  }

  audio_quality_analyzer_->Stop();
  video_quality_analyzer_injection_helper_->Stop();
  for (auto& reporter : quality_metrics_reporters_) {
    reporter->StopAndReportResults();
  }

  // Reset |task_queue_| after test to cleanup.
  task_queue_.reset();

  // Ensuring that TestPeers have been destroyed in order to correctly close
  // Audio dumps.
  RTC_CHECK(!alice_);
  RTC_CHECK(!bob_);
  // Ensuring that FrameGeneratorCapturerVideoTrackSource and VideoFrameWriter
  // are destroyed on the right thread.
  RTC_CHECK(alice_video_sources_.empty());
  RTC_CHECK(bob_video_sources_.empty());
  RTC_CHECK(video_writers_.empty());
}

void PeerConnectionE2EQualityTest::SetDefaultValuesForMissingParams(
    std::vector<Params*> params) {
  int video_counter = 0;
  int audio_counter = 0;
  std::set<std::string> video_labels;
  std::set<std::string> audio_labels;
  for (auto* p : params) {
    for (auto& video_config : p->video_configs) {
      if (!video_config.generator && !video_config.input_file_name &&
          !video_config.screen_share_config) {
        video_config.generator = VideoGeneratorType::kDefault;
      }
      if (!video_config.stream_label) {
        std::string label;
        do {
          label = "_auto_video_stream_label_" + std::to_string(video_counter);
          ++video_counter;
        } while (!video_labels.insert(label).second);
        video_config.stream_label = label;
      }
    }
    if (p->audio_config) {
      if (!p->audio_config->stream_label) {
        std::string label;
        do {
          label = "_auto_audio_stream_label_" + std::to_string(audio_counter);
          ++audio_counter;
        } while (!audio_labels.insert(label).second);
        p->audio_config->stream_label = label;
      }
    }
  }
}

void PeerConnectionE2EQualityTest::ValidateParams(const RunParams& run_params,
                                                  std::vector<Params*> params) {
  RTC_CHECK_GT(run_params.video_encoder_bitrate_multiplier, 0.0);

  std::set<std::string> video_labels;
  std::set<std::string> audio_labels;
  int media_streams_count = 0;

  for (size_t i = 0; i < params.size(); ++i) {
    Params* p = params[i];
    if (p->audio_config) {
      media_streams_count++;
    }
    media_streams_count += p->video_configs.size();

    // Validate that each video config has exactly one of |generator|,
    // |input_file_name| or |screen_share_config| set. Also validate that all
    // video stream labels are unique.
    for (auto& video_config : p->video_configs) {
      RTC_CHECK(video_config.stream_label);
      bool inserted =
          video_labels.insert(video_config.stream_label.value()).second;
      RTC_CHECK(inserted) << "Duplicate video_config.stream_label="
                          << video_config.stream_label.value();
      RTC_CHECK(video_config.generator || video_config.input_file_name ||
                video_config.screen_share_config)
          << VideoConfigSourcePresenceToString(video_config);
      RTC_CHECK(!(video_config.input_file_name && video_config.generator))
          << VideoConfigSourcePresenceToString(video_config);
      RTC_CHECK(
          !(video_config.input_file_name && video_config.screen_share_config))
          << VideoConfigSourcePresenceToString(video_config);
      RTC_CHECK(!(video_config.screen_share_config && video_config.generator))
          << VideoConfigSourcePresenceToString(video_config);

      if (video_config.screen_share_config) {
        if (video_config.screen_share_config->slides_yuv_file_names.empty()) {
          if (video_config.screen_share_config->scrolling_params) {
            // If we have scrolling params, then its |source_width| and
            // |source_heigh| will be used as width and height of video input,
            // so we have to validate it against width and height of default
            // input.
            RTC_CHECK_EQ(video_config.screen_share_config->scrolling_params
                             ->source_width,
                         kDefaultSlidesWidth);
            RTC_CHECK_EQ(video_config.screen_share_config->scrolling_params
                             ->source_height,
                         kDefaultSlidesHeight);
          } else {
            RTC_CHECK_EQ(video_config.width, kDefaultSlidesWidth);
            RTC_CHECK_EQ(video_config.height, kDefaultSlidesHeight);
          }
        }
        if (video_config.screen_share_config->scrolling_params) {
          RTC_CHECK_LE(
              video_config.screen_share_config->scrolling_params->duration,
              video_config.screen_share_config->slide_change_interval);
          RTC_CHECK_GE(
              video_config.screen_share_config->scrolling_params->source_width,
              video_config.width);
          RTC_CHECK_GE(
              video_config.screen_share_config->scrolling_params->source_height,
              video_config.height);
        }
      }
      if (video_config.simulcast_config) {
        // We support simulcast only from caller.
        RTC_CHECK_EQ(i, 0)
            << "Only simulcast stream from first peer is supported";
      }
    }
    if (p->audio_config) {
      bool inserted =
          audio_labels.insert(p->audio_config->stream_label.value()).second;
      RTC_CHECK(inserted) << "Duplicate audio_config.stream_label="
                          << p->audio_config->stream_label.value();
      // Check that if mode input file name specified only if mode is kFile.
      if (p->audio_config.value().mode == AudioConfig::Mode::kGenerated) {
        RTC_CHECK(!p->audio_config.value().input_file_name);
      }
      if (p->audio_config.value().mode == AudioConfig::Mode::kFile) {
        RTC_CHECK(p->audio_config.value().input_file_name);
        RTC_CHECK(
            test::FileExists(p->audio_config.value().input_file_name.value()))
            << p->audio_config.value().input_file_name.value()
            << " doesn't exist";
      }
    }
  }

  RTC_CHECK_GT(media_streams_count, 0) << "No media in the call.";
}

void PeerConnectionE2EQualityTest::SetupRequiredFieldTrials(
    const RunParams& run_params) {
  std::string field_trials = "";
  if (run_params.use_flex_fec) {
    field_trials += kFlexFecEnabledFieldTrials;
  }
  if (!field_trials.empty()) {
    override_field_trials_ = absl::make_unique<test::ScopedFieldTrials>(
        field_trial::GetFieldTrialString() + field_trials);
  }
}

void PeerConnectionE2EQualityTest::OnTrackCallback(
    rtc::scoped_refptr<RtpTransceiverInterface> transceiver,
    std::vector<VideoConfig> remote_video_configs) {
  const rtc::scoped_refptr<MediaStreamTrackInterface>& track =
      transceiver->receiver()->track();
  RTC_CHECK_EQ(transceiver->receiver()->stream_ids().size(), 1);
  std::string stream_label = transceiver->receiver()->stream_ids().front();
  analyzer_helper_.AddTrackToStreamMapping(track->id(), stream_label);
  if (track->kind() != MediaStreamTrackInterface::kVideoKind) {
    return;
  }

  VideoConfig* video_config = nullptr;
  for (auto& config : remote_video_configs) {
    if (config.stream_label == stream_label) {
      video_config = &config;
      break;
    }
  }
  RTC_CHECK(video_config);
  test::VideoFrameWriter* writer = MaybeCreateVideoWriter(
      video_config->output_dump_file_name, *video_config);
  // It is safe to cast here, because it is checked above that
  // track->kind() is kVideoKind.
  auto* video_track = static_cast<VideoTrackInterface*>(track.get());
  std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>> video_sink =
      video_quality_analyzer_injection_helper_->CreateVideoSink(writer);
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

  for (auto& video_config : alice_->params()->video_configs) {
    if (video_config.simulcast_config) {
      RtpTransceiverInit transceiver_params;
      transceiver_params.direction = RtpTransceiverDirection::kSendOnly;
      if (run_params.video_codec_name == cricket::kVp8CodecName) {
        // For Vp8 simulcast we need to add as many RtpEncodingParameters to the
        // track as many simulcast streams requested.
        for (int i = 0;
             i < video_config.simulcast_config->simulcast_streams_count; ++i) {
          RtpEncodingParameters enc_params;
          // We need to be sure, that all rids will be unique with all mids.
          enc_params.rid = std::to_string(alice_transceivers_counter) + "000" +
                           std::to_string(i);
          transceiver_params.send_encodings.push_back(enc_params);
        }
      }
      RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> result =
          alice_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO,
                                 transceiver_params);
      RTC_CHECK(result.ok());
      alice_transceivers_counter++;
    }
  }
  for (size_t i = 0; i < bob_->params()->video_configs.size(); ++i) {
    RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> result =
        alice_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO,
                               receive_only_transceiver_init);
    RTC_CHECK(result.ok());
    alice_transceivers_counter++;
  }
  // Then add media for Alice and Bob
  alice_video_sources_ = MaybeAddMedia(alice_.get());
  bob_video_sources_ = MaybeAddMedia(bob_.get());

  SetPeerCodecPreferences(alice_.get(), run_params);
  SetPeerCodecPreferences(bob_.get(), run_params);

  SetupCall(run_params);
}

void PeerConnectionE2EQualityTest::TearDownCallOnSignalingThread() {
  TearDownCall();
}

std::vector<rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource>>
PeerConnectionE2EQualityTest::MaybeAddMedia(TestPeer* peer) {
  MaybeAddAudio(peer);
  return MaybeAddVideo(peer);
}

std::vector<rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource>>
PeerConnectionE2EQualityTest::MaybeAddVideo(TestPeer* peer) {
  // Params here valid because of pre-run validation.
  Params* params = peer->params();
  std::vector<rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource>> out;
  for (auto video_config : params->video_configs) {
    // Create video generator.
    std::unique_ptr<test::FrameGenerator> frame_generator =
        CreateFrameGenerator(video_config);

    // Wrap it to inject video quality analyzer and enable dump of input video
    // if required.
    test::VideoFrameWriter* writer =
        MaybeCreateVideoWriter(video_config.input_dump_file_name, video_config);
    frame_generator =
        video_quality_analyzer_injection_helper_->WrapFrameGenerator(
            video_config.stream_label.value(), std::move(frame_generator),
            writer);

    // Setup FrameGenerator into peer connection.
    auto capturer = absl::make_unique<test::FrameGeneratorCapturer>(
        clock_, std::move(frame_generator), video_config.fps,
        *task_queue_factory_);
    capturer->Init();
    rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource> source =
        new rtc::RefCountedObject<FrameGeneratorCapturerVideoTrackSource>(
            std::move(capturer),
            /*is_screencast=*/video_config.screen_share_config.has_value());
    out.push_back(source);
    RTC_LOG(INFO) << "Adding video with video_config.stream_label="
                  << video_config.stream_label.value();
    rtc::scoped_refptr<VideoTrackInterface> track =
        peer->pc_factory()->CreateVideoTrack(video_config.stream_label.value(),
                                             source);
    if (video_config.screen_share_config) {
      track->set_content_hint(VideoTrackInterface::ContentHint::kText);
    }
    RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> sender =
        peer->AddTrack(track, {video_config.stream_label.value()});
    RTC_CHECK(sender.ok());
    if (video_config.temporal_layers_count) {
      RtpParameters rtp_parameters = sender.value()->GetParameters();
      for (auto& encoding_parameters : rtp_parameters.encodings) {
        encoding_parameters.num_temporal_layers =
            video_config.temporal_layers_count;
      }
      RTCError res = sender.value()->SetParameters(rtp_parameters);
      RTC_CHECK(res.ok()) << "Failed to set RTP parameters";
    }
  }
  return out;
}

std::unique_ptr<test::FrameGenerator>
PeerConnectionE2EQualityTest::CreateFrameGenerator(
    const VideoConfig& video_config) {
  if (video_config.generator) {
    absl::optional<test::FrameGenerator::OutputType> frame_generator_type =
        absl::nullopt;
    if (video_config.generator == VideoGeneratorType::kDefault) {
      frame_generator_type = test::FrameGenerator::OutputType::kI420;
    } else if (video_config.generator == VideoGeneratorType::kI420A) {
      frame_generator_type = test::FrameGenerator::OutputType::kI420A;
    } else if (video_config.generator == VideoGeneratorType::kI010) {
      frame_generator_type = test::FrameGenerator::OutputType::kI010;
    }
    return test::FrameGenerator::CreateSquareGenerator(
        static_cast<int>(video_config.width),
        static_cast<int>(video_config.height), frame_generator_type,
        absl::nullopt);
  }
  if (video_config.input_file_name) {
    return test::FrameGenerator::CreateFromYuvFile(
        std::vector<std::string>(/*count=*/1,
                                 video_config.input_file_name.value()),
        video_config.width, video_config.height, /*frame_repeat_count=*/1);
  }
  if (video_config.screen_share_config) {
    return CreateScreenShareFrameGenerator(video_config);
  }
  RTC_NOTREACHED() << "Unsupported video_config input source";
  return nullptr;
}

std::unique_ptr<test::FrameGenerator>
PeerConnectionE2EQualityTest::CreateScreenShareFrameGenerator(
    const VideoConfig& video_config) {
  RTC_CHECK(video_config.screen_share_config);
  if (video_config.screen_share_config->generate_slides) {
    return test::FrameGenerator::CreateSlideGenerator(
        video_config.width, video_config.height,
        video_config.screen_share_config->slide_change_interval.seconds() *
            video_config.fps);
  }
  std::vector<std::string> slides =
      video_config.screen_share_config->slides_yuv_file_names;
  if (slides.empty()) {
    // If slides is empty we need to add default slides as source. In such case
    // video width and height is validated to be equal to kDefaultSlidesWidth
    // and kDefaultSlidesHeight.
    slides.push_back(test::ResourcePath("web_screenshot_1850_1110", "yuv"));
    slides.push_back(test::ResourcePath("presentation_1850_1110", "yuv"));
    slides.push_back(test::ResourcePath("photo_1850_1110", "yuv"));
    slides.push_back(test::ResourcePath("difficult_photo_1850_1110", "yuv"));
  }
  if (!video_config.screen_share_config->scrolling_params) {
    // Cycle image every slide_change_interval seconds.
    return test::FrameGenerator::CreateFromYuvFile(
        slides, video_config.width, video_config.height,
        video_config.screen_share_config->slide_change_interval.seconds() *
            video_config.fps);
  }

  // |pause_duration| is nonnegative. It is validated in ValidateParams(...).
  TimeDelta pause_duration =
      video_config.screen_share_config->slide_change_interval -
      video_config.screen_share_config->scrolling_params->duration;

  return test::FrameGenerator::CreateScrollingInputFromYuvFiles(
      clock_, slides,
      video_config.screen_share_config->scrolling_params->source_width,
      video_config.screen_share_config->scrolling_params->source_height,
      video_config.width, video_config.height,
      video_config.screen_share_config->scrolling_params->duration.ms(),
      pause_duration.ms());
}

void PeerConnectionE2EQualityTest::MaybeAddAudio(TestPeer* peer) {
  if (!peer->params()->audio_config) {
    return;
  }
  const AudioConfig& audio_config = peer->params()->audio_config.value();
  rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
      peer->pc_factory()->CreateAudioSource(audio_config.audio_options);
  rtc::scoped_refptr<AudioTrackInterface> track =
      peer->pc_factory()->CreateAudioTrack(*audio_config.stream_label, source);
  peer->AddTrack(track, {*audio_config.stream_label});
}

void PeerConnectionE2EQualityTest::SetPeerCodecPreferences(
    TestPeer* peer,
    const RunParams& run_params) {
  std::vector<RtpCodecCapability> with_rtx_video_capabilities =
      FilterVideoCodecCapabilities(
          run_params.video_codec_name, run_params.video_codec_required_params,
          true, run_params.use_ulp_fec, run_params.use_flex_fec,
          peer->pc_factory()
              ->GetRtpSenderCapabilities(cricket::MediaType::MEDIA_TYPE_VIDEO)
              .codecs);
  std::vector<RtpCodecCapability> without_rtx_video_capabilities =
      FilterVideoCodecCapabilities(
          run_params.video_codec_name, run_params.video_codec_required_params,
          false, run_params.use_ulp_fec, run_params.use_flex_fec,
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

void PeerConnectionE2EQualityTest::SetupCall(const RunParams& run_params) {
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
  PatchingParams patching_params(run_params.video_codec_name,
                                 run_params.use_conference_mode,
                                 stream_label_to_simulcast_streams_count);
  SignalingInterceptor signaling_interceptor(patching_params);
  // Connect peers.
  ExchangeOfferAnswer(&signaling_interceptor);
  // Do the SDP negotiation, and also exchange ice candidates.
  ASSERT_EQ_WAIT(alice_->signaling_state(), PeerConnectionInterface::kStable,
                 kDefaultTimeoutMs);
  ASSERT_TRUE_WAIT(alice_->IsIceGatheringDone(), kDefaultTimeoutMs);
  ASSERT_TRUE_WAIT(bob_->IsIceGatheringDone(), kDefaultTimeoutMs);

  ExchangeIceCandidates(&signaling_interceptor);
  // This means that ICE and DTLS are connected.
  ASSERT_TRUE_WAIT(bob_->IsIceConnected(), kDefaultTimeoutMs);
  ASSERT_TRUE_WAIT(alice_->IsIceConnected(), kDefaultTimeoutMs);
  RTC_LOG(INFO) << "Call is started (all peers are connected).";
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
    RTC_LOG(INFO) << "Alice ICE candidate(mid= " << candidate->sdp_mid()
                  << "): " << candidate_str;
  }
  ASSERT_TRUE(bob_->AddIceCandidates(std::move(alice_candidates)));
  std::vector<std::unique_ptr<IceCandidateInterface>> bob_candidates =
      signaling_interceptor->PatchAnswererIceCandidates(
          bob_->observer()->GetAllCandidates());
  for (auto& candidate : bob_candidates) {
    std::string candidate_str;
    RTC_CHECK(candidate->ToString(&candidate_str));
    RTC_LOG(INFO) << "Bob ICE candidate(mid= " << candidate->sdp_mid()
                  << "): " << candidate_str;
  }
  ASSERT_TRUE(alice_->AddIceCandidates(std::move(bob_candidates)));
}

void PeerConnectionE2EQualityTest::StartVideo(
    const std::vector<
        rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource>>& sources) {
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

  alice_->pc()->Close();
  bob_->pc()->Close();

  for (const auto& video_writer : video_writers_) {
    video_writer->Close();
  }

  alice_video_sources_.clear();
  bob_video_sources_.clear();
  video_writers_.clear();
  alice_.reset();
  bob_.reset();
}

test::VideoFrameWriter* PeerConnectionE2EQualityTest::MaybeCreateVideoWriter(
    absl::optional<std::string> file_name,
    const VideoConfig& config) {
  if (!file_name) {
    return nullptr;
  }
  // TODO(titovartem) create only one file writer for simulcast video track.
  auto video_writer = absl::make_unique<test::Y4mVideoFrameWriterImpl>(
      file_name.value(), config.width, config.height, config.fps);
  test::VideoFrameWriter* out = video_writer.get();
  video_writers_.push_back(std::move(video_writer));
  return out;
}

Timestamp PeerConnectionE2EQualityTest::Now() const {
  return clock_->CurrentTime();
}

PeerConnectionE2EQualityTest::ScheduledActivity::ScheduledActivity(
    TimeDelta initial_delay_since_start,
    absl::optional<TimeDelta> interval,
    std::function<void(TimeDelta)> func)
    : initial_delay_since_start(initial_delay_since_start),
      interval(std::move(interval)),
      func(std::move(func)) {}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
