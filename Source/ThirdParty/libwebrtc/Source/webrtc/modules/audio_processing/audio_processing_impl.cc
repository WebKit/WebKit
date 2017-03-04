/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/audio_processing_impl.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/platform_file.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/common_audio/audio_converter.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_processing/aec/aec_core.h"
#include "webrtc/modules/audio_processing/aec3/echo_canceller3.h"
#include "webrtc/modules/audio_processing/agc/agc_manager_direct.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/beamformer/nonlinear_beamformer.h"
#include "webrtc/modules/audio_processing/common.h"
#include "webrtc/modules/audio_processing/echo_cancellation_impl.h"
#include "webrtc/modules/audio_processing/echo_control_mobile_impl.h"
#include "webrtc/modules/audio_processing/gain_control_for_experimental_agc.h"
#include "webrtc/modules/audio_processing/gain_control_impl.h"
#if WEBRTC_INTELLIGIBILITY_ENHANCER
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_enhancer.h"
#endif
#include "webrtc/modules/audio_processing/level_controller/level_controller.h"
#include "webrtc/modules/audio_processing/level_estimator_impl.h"
#include "webrtc/modules/audio_processing/low_cut_filter.h"
#include "webrtc/modules/audio_processing/noise_suppression_impl.h"
#include "webrtc/modules/audio_processing/residual_echo_detector.h"
#include "webrtc/modules/audio_processing/transient/transient_suppressor.h"
#include "webrtc/modules/audio_processing/voice_detection_impl.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/system_wrappers/include/file_wrapper.h"
#include "webrtc/system_wrappers/include/logging.h"
#include "webrtc/system_wrappers/include/metrics.h"

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
// Files generated at build-time by the protobuf compiler.
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/debug.pb.h"
#else
#include "webrtc/modules/audio_processing/debug.pb.h"
#endif
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP

// Check to verify that the define for the intelligibility enhancer is properly
// set.
#if !defined(WEBRTC_INTELLIGIBILITY_ENHANCER) || \
    (WEBRTC_INTELLIGIBILITY_ENHANCER != 0 &&     \
     WEBRTC_INTELLIGIBILITY_ENHANCER != 1)
#error "Set WEBRTC_INTELLIGIBILITY_ENHANCER to either 0 or 1"
#endif

#define RETURN_ON_ERR(expr) \
  do {                      \
    int err = (expr);       \
    if (err != kNoError) {  \
      return err;           \
    }                       \
  } while (0)

namespace webrtc {

constexpr int AudioProcessing::kNativeSampleRatesHz[];

namespace {

static bool LayoutHasKeyboard(AudioProcessing::ChannelLayout layout) {
  switch (layout) {
    case AudioProcessing::kMono:
    case AudioProcessing::kStereo:
      return false;
    case AudioProcessing::kMonoAndKeyboard:
    case AudioProcessing::kStereoAndKeyboard:
      return true;
  }

  RTC_NOTREACHED();
  return false;
}

bool SampleRateSupportsMultiBand(int sample_rate_hz) {
  return sample_rate_hz == AudioProcessing::kSampleRate32kHz ||
         sample_rate_hz == AudioProcessing::kSampleRate48kHz;
}

int FindNativeProcessRateToUse(int minimum_rate, bool band_splitting_required) {
#ifdef WEBRTC_ARCH_ARM_FAMILY
  constexpr int kMaxSplittingNativeProcessRate =
      AudioProcessing::kSampleRate32kHz;
#else
  constexpr int kMaxSplittingNativeProcessRate =
      AudioProcessing::kSampleRate48kHz;
#endif
  static_assert(
      kMaxSplittingNativeProcessRate <= AudioProcessing::kMaxNativeSampleRateHz,
      "");
  const int uppermost_native_rate = band_splitting_required
                                        ? kMaxSplittingNativeProcessRate
                                        : AudioProcessing::kSampleRate48kHz;

  for (auto rate : AudioProcessing::kNativeSampleRatesHz) {
    if (rate >= uppermost_native_rate) {
      return uppermost_native_rate;
    }
    if (rate >= minimum_rate) {
      return rate;
    }
  }
  RTC_NOTREACHED();
  return uppermost_native_rate;
}

// Maximum length that a frame of samples can have.
static const size_t kMaxAllowedValuesOfSamplesPerFrame = 160;
// Maximum number of frames to buffer in the render queue.
// TODO(peah): Decrease this once we properly handle hugely unbalanced
// reverse and forward call numbers.
static const size_t kMaxNumFramesToBuffer = 100;

class HighPassFilterImpl : public HighPassFilter {
 public:
  explicit HighPassFilterImpl(AudioProcessingImpl* apm) : apm_(apm) {}
  ~HighPassFilterImpl() override = default;

  // HighPassFilter implementation.
  int Enable(bool enable) override {
    apm_->MutateConfig([enable](AudioProcessing::Config* config) {
      config->high_pass_filter.enabled = enable;
    });

    return AudioProcessing::kNoError;
  }

  bool is_enabled() const override {
    return apm_->GetConfig().high_pass_filter.enabled;
  }

 private:
  AudioProcessingImpl* apm_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(HighPassFilterImpl);
};

}  // namespace

// Throughout webrtc, it's assumed that success is represented by zero.
static_assert(AudioProcessing::kNoError == 0, "kNoError must be zero");

AudioProcessingImpl::ApmSubmoduleStates::ApmSubmoduleStates() {}

bool AudioProcessingImpl::ApmSubmoduleStates::Update(
    bool low_cut_filter_enabled,
    bool echo_canceller_enabled,
    bool mobile_echo_controller_enabled,
    bool residual_echo_detector_enabled,
    bool noise_suppressor_enabled,
    bool intelligibility_enhancer_enabled,
    bool beamformer_enabled,
    bool adaptive_gain_controller_enabled,
    bool level_controller_enabled,
    bool echo_canceller3_enabled,
    bool voice_activity_detector_enabled,
    bool level_estimator_enabled,
    bool transient_suppressor_enabled) {
  bool changed = false;
  changed |= (low_cut_filter_enabled != low_cut_filter_enabled_);
  changed |= (echo_canceller_enabled != echo_canceller_enabled_);
  changed |=
      (mobile_echo_controller_enabled != mobile_echo_controller_enabled_);
  changed |=
      (residual_echo_detector_enabled != residual_echo_detector_enabled_);
  changed |= (noise_suppressor_enabled != noise_suppressor_enabled_);
  changed |=
      (intelligibility_enhancer_enabled != intelligibility_enhancer_enabled_);
  changed |= (beamformer_enabled != beamformer_enabled_);
  changed |=
      (adaptive_gain_controller_enabled != adaptive_gain_controller_enabled_);
  changed |= (level_controller_enabled != level_controller_enabled_);
  changed |= (echo_canceller3_enabled != echo_canceller3_enabled_);
  changed |= (level_estimator_enabled != level_estimator_enabled_);
  changed |=
      (voice_activity_detector_enabled != voice_activity_detector_enabled_);
  changed |= (transient_suppressor_enabled != transient_suppressor_enabled_);
  if (changed) {
    low_cut_filter_enabled_ = low_cut_filter_enabled;
    echo_canceller_enabled_ = echo_canceller_enabled;
    mobile_echo_controller_enabled_ = mobile_echo_controller_enabled;
    residual_echo_detector_enabled_ = residual_echo_detector_enabled;
    noise_suppressor_enabled_ = noise_suppressor_enabled;
    intelligibility_enhancer_enabled_ = intelligibility_enhancer_enabled;
    beamformer_enabled_ = beamformer_enabled;
    adaptive_gain_controller_enabled_ = adaptive_gain_controller_enabled;
    level_controller_enabled_ = level_controller_enabled;
    echo_canceller3_enabled_ = echo_canceller3_enabled;
    level_estimator_enabled_ = level_estimator_enabled;
    voice_activity_detector_enabled_ = voice_activity_detector_enabled;
    transient_suppressor_enabled_ = transient_suppressor_enabled;
  }

  changed |= first_update_;
  first_update_ = false;
  return changed;
}

bool AudioProcessingImpl::ApmSubmoduleStates::CaptureMultiBandSubModulesActive()
    const {
#if WEBRTC_INTELLIGIBILITY_ENHANCER
  return CaptureMultiBandProcessingActive() ||
         intelligibility_enhancer_enabled_ ||
         voice_activity_detector_enabled_ || residual_echo_detector_enabled_;
#else
  return CaptureMultiBandProcessingActive() ||
         voice_activity_detector_enabled_ || residual_echo_detector_enabled_;
#endif
}

bool AudioProcessingImpl::ApmSubmoduleStates::CaptureMultiBandProcessingActive()
    const {
  return low_cut_filter_enabled_ || echo_canceller_enabled_ ||
         mobile_echo_controller_enabled_ || noise_suppressor_enabled_ ||
         beamformer_enabled_ || adaptive_gain_controller_enabled_ ||
         echo_canceller3_enabled_;
}

bool AudioProcessingImpl::ApmSubmoduleStates::RenderMultiBandSubModulesActive()
    const {
  return RenderMultiBandProcessingActive() || echo_canceller_enabled_ ||
         mobile_echo_controller_enabled_ || adaptive_gain_controller_enabled_ ||
         residual_echo_detector_enabled_ || echo_canceller3_enabled_;
}

bool AudioProcessingImpl::ApmSubmoduleStates::RenderMultiBandProcessingActive()
    const {
#if WEBRTC_INTELLIGIBILITY_ENHANCER
  return intelligibility_enhancer_enabled_;
#else
  return false;
#endif
}

struct AudioProcessingImpl::ApmPublicSubmodules {
  ApmPublicSubmodules() {}
  // Accessed externally of APM without any lock acquired.
  std::unique_ptr<EchoCancellationImpl> echo_cancellation;
  std::unique_ptr<EchoControlMobileImpl> echo_control_mobile;
  std::unique_ptr<GainControlImpl> gain_control;
  std::unique_ptr<LevelEstimatorImpl> level_estimator;
  std::unique_ptr<NoiseSuppressionImpl> noise_suppression;
  std::unique_ptr<VoiceDetectionImpl> voice_detection;
  std::unique_ptr<GainControlForExperimentalAgc>
      gain_control_for_experimental_agc;

  // Accessed internally from both render and capture.
  std::unique_ptr<TransientSuppressor> transient_suppressor;
#if WEBRTC_INTELLIGIBILITY_ENHANCER
  std::unique_ptr<IntelligibilityEnhancer> intelligibility_enhancer;
#endif
};

struct AudioProcessingImpl::ApmPrivateSubmodules {
  explicit ApmPrivateSubmodules(NonlinearBeamformer* beamformer)
      : beamformer(beamformer) {}
  // Accessed internally from capture or during initialization
  std::unique_ptr<NonlinearBeamformer> beamformer;
  std::unique_ptr<AgcManagerDirect> agc_manager;
  std::unique_ptr<LowCutFilter> low_cut_filter;
  std::unique_ptr<LevelController> level_controller;
  std::unique_ptr<ResidualEchoDetector> residual_echo_detector;
  std::unique_ptr<EchoCanceller3> echo_canceller3;
};

AudioProcessing* AudioProcessing::Create() {
  webrtc::Config config;
  return Create(config, nullptr);
}

AudioProcessing* AudioProcessing::Create(const webrtc::Config& config) {
  return Create(config, nullptr);
}

AudioProcessing* AudioProcessing::Create(const webrtc::Config& config,
                                         NonlinearBeamformer* beamformer) {
  AudioProcessingImpl* apm = new AudioProcessingImpl(config, beamformer);
  if (apm->Initialize() != kNoError) {
    delete apm;
    apm = nullptr;
  }

  return apm;
}

AudioProcessingImpl::AudioProcessingImpl(const webrtc::Config& config)
    : AudioProcessingImpl(config, nullptr) {}

AudioProcessingImpl::AudioProcessingImpl(const webrtc::Config& config,
                                         NonlinearBeamformer* beamformer)
    : high_pass_filter_impl_(new HighPassFilterImpl(this)),
      public_submodules_(new ApmPublicSubmodules()),
      private_submodules_(new ApmPrivateSubmodules(beamformer)),
      constants_(config.Get<ExperimentalAgc>().startup_min_volume,
                 config.Get<ExperimentalAgc>().clipped_level_min,
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
                 false),
#else
                 config.Get<ExperimentalAgc>().enabled),
#endif
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
      capture_(false,
#else
      capture_(config.Get<ExperimentalNs>().enabled,
#endif
               config.Get<Beamforming>().array_geometry,
               config.Get<Beamforming>().target_direction),
      capture_nonlocked_(config.Get<Beamforming>().enabled,
                         config.Get<Intelligibility>().enabled) {
  {
    rtc::CritScope cs_render(&crit_render_);
    rtc::CritScope cs_capture(&crit_capture_);

    public_submodules_->echo_cancellation.reset(
        new EchoCancellationImpl(&crit_render_, &crit_capture_));
    public_submodules_->echo_control_mobile.reset(
        new EchoControlMobileImpl(&crit_render_, &crit_capture_));
    public_submodules_->gain_control.reset(
        new GainControlImpl(&crit_capture_, &crit_capture_));
    public_submodules_->level_estimator.reset(
        new LevelEstimatorImpl(&crit_capture_));
    public_submodules_->noise_suppression.reset(
        new NoiseSuppressionImpl(&crit_capture_));
    public_submodules_->voice_detection.reset(
        new VoiceDetectionImpl(&crit_capture_));
    public_submodules_->gain_control_for_experimental_agc.reset(
        new GainControlForExperimentalAgc(
            public_submodules_->gain_control.get(), &crit_capture_));
    private_submodules_->residual_echo_detector.reset(
        new ResidualEchoDetector());

    // TODO(peah): Move this creation to happen only when the level controller
    // is enabled.
    private_submodules_->level_controller.reset(new LevelController());
  }

  SetExtraOptions(config);
}

AudioProcessingImpl::~AudioProcessingImpl() {
  // Depends on gain_control_ and
  // public_submodules_->gain_control_for_experimental_agc.
  private_submodules_->agc_manager.reset();
  // Depends on gain_control_.
  public_submodules_->gain_control_for_experimental_agc.reset();

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  debug_dump_.debug_file->CloseFile();
#endif
}

int AudioProcessingImpl::Initialize() {
  // Run in a single-threaded manner during initialization.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);
  return InitializeLocked();
}

int AudioProcessingImpl::Initialize(int capture_input_sample_rate_hz,
                                    int capture_output_sample_rate_hz,
                                    int render_input_sample_rate_hz,
                                    ChannelLayout capture_input_layout,
                                    ChannelLayout capture_output_layout,
                                    ChannelLayout render_input_layout) {
  const ProcessingConfig processing_config = {
      {{capture_input_sample_rate_hz, ChannelsFromLayout(capture_input_layout),
        LayoutHasKeyboard(capture_input_layout)},
       {capture_output_sample_rate_hz,
        ChannelsFromLayout(capture_output_layout),
        LayoutHasKeyboard(capture_output_layout)},
       {render_input_sample_rate_hz, ChannelsFromLayout(render_input_layout),
        LayoutHasKeyboard(render_input_layout)},
       {render_input_sample_rate_hz, ChannelsFromLayout(render_input_layout),
        LayoutHasKeyboard(render_input_layout)}}};

  return Initialize(processing_config);
}

int AudioProcessingImpl::Initialize(const ProcessingConfig& processing_config) {
  // Run in a single-threaded manner during initialization.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);
  return InitializeLocked(processing_config);
}

int AudioProcessingImpl::MaybeInitializeRender(
    const ProcessingConfig& processing_config) {
  return MaybeInitialize(processing_config, false);
}

int AudioProcessingImpl::MaybeInitializeCapture(
    const ProcessingConfig& processing_config,
    bool force_initialization) {
  return MaybeInitialize(processing_config, force_initialization);
}

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP

AudioProcessingImpl::ApmDebugDumpThreadState::ApmDebugDumpThreadState()
    : event_msg(new audioproc::Event()) {}

AudioProcessingImpl::ApmDebugDumpThreadState::~ApmDebugDumpThreadState() {}

AudioProcessingImpl::ApmDebugDumpState::ApmDebugDumpState()
    : debug_file(FileWrapper::Create()) {}

AudioProcessingImpl::ApmDebugDumpState::~ApmDebugDumpState() {}

#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP

// Calls InitializeLocked() if any of the audio parameters have changed from
// their current values (needs to be called while holding the crit_render_lock).
int AudioProcessingImpl::MaybeInitialize(
    const ProcessingConfig& processing_config,
    bool force_initialization) {
  // Called from both threads. Thread check is therefore not possible.
  if (processing_config == formats_.api_format && !force_initialization) {
    return kNoError;
  }

  rtc::CritScope cs_capture(&crit_capture_);
  return InitializeLocked(processing_config);
}

int AudioProcessingImpl::InitializeLocked() {
  const int capture_audiobuffer_num_channels =
      capture_nonlocked_.beamformer_enabled
          ? formats_.api_format.input_stream().num_channels()
          : formats_.api_format.output_stream().num_channels();

  const int render_audiobuffer_num_output_frames =
      formats_.api_format.reverse_output_stream().num_frames() == 0
          ? formats_.render_processing_format.num_frames()
          : formats_.api_format.reverse_output_stream().num_frames();
  if (formats_.api_format.reverse_input_stream().num_channels() > 0) {
    render_.render_audio.reset(new AudioBuffer(
        formats_.api_format.reverse_input_stream().num_frames(),
        formats_.api_format.reverse_input_stream().num_channels(),
        formats_.render_processing_format.num_frames(),
        formats_.render_processing_format.num_channels(),
        render_audiobuffer_num_output_frames));
    if (formats_.api_format.reverse_input_stream() !=
        formats_.api_format.reverse_output_stream()) {
      render_.render_converter = AudioConverter::Create(
          formats_.api_format.reverse_input_stream().num_channels(),
          formats_.api_format.reverse_input_stream().num_frames(),
          formats_.api_format.reverse_output_stream().num_channels(),
          formats_.api_format.reverse_output_stream().num_frames());
    } else {
      render_.render_converter.reset(nullptr);
    }
  } else {
    render_.render_audio.reset(nullptr);
    render_.render_converter.reset(nullptr);
  }
  capture_.capture_audio.reset(
      new AudioBuffer(formats_.api_format.input_stream().num_frames(),
                      formats_.api_format.input_stream().num_channels(),
                      capture_nonlocked_.capture_processing_format.num_frames(),
                      capture_audiobuffer_num_channels,
                      formats_.api_format.output_stream().num_frames()));

  public_submodules_->echo_cancellation->Initialize(
      proc_sample_rate_hz(), num_reverse_channels(), num_output_channels(),
      num_proc_channels());
  AllocateRenderQueue();

  int success = public_submodules_->echo_cancellation->enable_metrics(true);
  RTC_DCHECK_EQ(0, success);
  success = public_submodules_->echo_cancellation->enable_delay_logging(true);
  RTC_DCHECK_EQ(0, success);
  public_submodules_->echo_control_mobile->Initialize(
      proc_split_sample_rate_hz(), num_reverse_channels(),
      num_output_channels());

  public_submodules_->gain_control->Initialize(num_proc_channels(),
                                               proc_sample_rate_hz());
  if (constants_.use_experimental_agc) {
    if (!private_submodules_->agc_manager.get()) {
      private_submodules_->agc_manager.reset(new AgcManagerDirect(
          public_submodules_->gain_control.get(),
          public_submodules_->gain_control_for_experimental_agc.get(),
          constants_.agc_startup_min_volume, constants_.agc_clipped_level_min));
    }
    private_submodules_->agc_manager->Initialize();
    private_submodules_->agc_manager->SetCaptureMuted(
        capture_.output_will_be_muted);
    public_submodules_->gain_control_for_experimental_agc->Initialize();
  }
  InitializeTransient();
  InitializeBeamformer();
#if WEBRTC_INTELLIGIBILITY_ENHANCER
  InitializeIntelligibility();
#endif
  InitializeLowCutFilter();
  public_submodules_->noise_suppression->Initialize(num_proc_channels(),
                                                    proc_sample_rate_hz());
  public_submodules_->voice_detection->Initialize(proc_split_sample_rate_hz());
  public_submodules_->level_estimator->Initialize();
  InitializeLevelController();
  InitializeResidualEchoDetector();
  InitializeEchoCanceller3();

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_dump_.debug_file->is_open()) {
    int err = WriteInitMessage();
    if (err != kNoError) {
      return err;
    }
  }
#endif

  return kNoError;
}

int AudioProcessingImpl::InitializeLocked(const ProcessingConfig& config) {
  for (const auto& stream : config.streams) {
    if (stream.num_channels() > 0 && stream.sample_rate_hz() <= 0) {
      return kBadSampleRateError;
    }
  }

  const size_t num_in_channels = config.input_stream().num_channels();
  const size_t num_out_channels = config.output_stream().num_channels();

  // Need at least one input channel.
  // Need either one output channel or as many outputs as there are inputs.
  if (num_in_channels == 0 ||
      !(num_out_channels == 1 || num_out_channels == num_in_channels)) {
    return kBadNumberChannelsError;
  }

  if (capture_nonlocked_.beamformer_enabled &&
      num_in_channels != capture_.array_geometry.size()) {
    return kBadNumberChannelsError;
  }

  formats_.api_format = config;

  int capture_processing_rate = FindNativeProcessRateToUse(
      std::min(formats_.api_format.input_stream().sample_rate_hz(),
               formats_.api_format.output_stream().sample_rate_hz()),
      submodule_states_.CaptureMultiBandSubModulesActive() ||
          submodule_states_.RenderMultiBandSubModulesActive());

  capture_nonlocked_.capture_processing_format =
      StreamConfig(capture_processing_rate);

  int render_processing_rate = FindNativeProcessRateToUse(
      std::min(formats_.api_format.reverse_input_stream().sample_rate_hz(),
               formats_.api_format.reverse_output_stream().sample_rate_hz()),
      submodule_states_.CaptureMultiBandSubModulesActive() ||
          submodule_states_.RenderMultiBandSubModulesActive());
  // TODO(aluebs): Remove this restriction once we figure out why the 3-band
  // splitting filter degrades the AEC performance.
  if (render_processing_rate > kSampleRate32kHz) {
    render_processing_rate = submodule_states_.RenderMultiBandProcessingActive()
                                 ? kSampleRate32kHz
                                 : kSampleRate16kHz;
  }
  // If the forward sample rate is 8 kHz, the render stream is also processed
  // at this rate.
  if (capture_nonlocked_.capture_processing_format.sample_rate_hz() ==
      kSampleRate8kHz) {
    render_processing_rate = kSampleRate8kHz;
  } else {
    render_processing_rate =
        std::max(render_processing_rate, static_cast<int>(kSampleRate16kHz));
  }

  // Always downmix the render stream to mono for analysis. This has been
  // demonstrated to work well for AEC in most practical scenarios.
  formats_.render_processing_format = StreamConfig(render_processing_rate, 1);

  if (capture_nonlocked_.capture_processing_format.sample_rate_hz() ==
          kSampleRate32kHz ||
      capture_nonlocked_.capture_processing_format.sample_rate_hz() ==
          kSampleRate48kHz) {
    capture_nonlocked_.split_rate = kSampleRate16kHz;
  } else {
    capture_nonlocked_.split_rate =
        capture_nonlocked_.capture_processing_format.sample_rate_hz();
  }

  return InitializeLocked();
}

void AudioProcessingImpl::ApplyConfig(const AudioProcessing::Config& config) {
  config_ = config;

  bool config_ok = LevelController::Validate(config_.level_controller);
  if (!config_ok) {
    LOG(LS_ERROR) << "AudioProcessing module config error" << std::endl
                  << "level_controller: "
                  << LevelController::ToString(config_.level_controller)
                  << std::endl
                  << "Reverting to default parameter set";
    config_.level_controller = AudioProcessing::Config::LevelController();
  }

  // Run in a single-threaded manner when applying the settings.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);

  // TODO(peah): Replace the use of capture_nonlocked_.level_controller_enabled
  // with the value in config_ everywhere in the code.
  if (capture_nonlocked_.level_controller_enabled !=
      config_.level_controller.enabled) {
    capture_nonlocked_.level_controller_enabled =
        config_.level_controller.enabled;
    // TODO(peah): Remove the conditional initialization to always initialize
    // the level controller regardless of whether it is enabled or not.
    InitializeLevelController();
  }
  LOG(LS_INFO) << "Level controller activated: "
               << capture_nonlocked_.level_controller_enabled;

  private_submodules_->level_controller->ApplyConfig(config_.level_controller);

  InitializeLowCutFilter();

  LOG(LS_INFO) << "Highpass filter activated: "
               << config_.high_pass_filter.enabled;

  config_ok = EchoCanceller3::Validate(config_.echo_canceller3);
  if (!config_ok) {
    LOG(LS_ERROR) << "AudioProcessing module config error" << std::endl
                  << "echo canceller 3: "
                  << EchoCanceller3::ToString(config_.echo_canceller3)
                  << std::endl
                  << "Reverting to default parameter set";
    config_.echo_canceller3 = AudioProcessing::Config::EchoCanceller3();
  }

  if (config.echo_canceller3.enabled !=
      capture_nonlocked_.echo_canceller3_enabled) {
    capture_nonlocked_.echo_canceller3_enabled =
        config_.echo_canceller3.enabled;
    InitializeEchoCanceller3();
    LOG(LS_INFO) << "Echo canceller 3 activated: "
                 << capture_nonlocked_.echo_canceller3_enabled;
  }
}

void AudioProcessingImpl::SetExtraOptions(const webrtc::Config& config) {
  // Run in a single-threaded manner when setting the extra options.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);

  public_submodules_->echo_cancellation->SetExtraOptions(config);

  if (capture_.transient_suppressor_enabled !=
      config.Get<ExperimentalNs>().enabled) {
    capture_.transient_suppressor_enabled =
        config.Get<ExperimentalNs>().enabled;
    InitializeTransient();
  }

#if WEBRTC_INTELLIGIBILITY_ENHANCER
  if(capture_nonlocked_.intelligibility_enabled !=
     config.Get<Intelligibility>().enabled) {
    capture_nonlocked_.intelligibility_enabled =
        config.Get<Intelligibility>().enabled;
    InitializeIntelligibility();
  }
#endif

#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
  if (capture_nonlocked_.beamformer_enabled !=
          config.Get<Beamforming>().enabled) {
    capture_nonlocked_.beamformer_enabled = config.Get<Beamforming>().enabled;
    if (config.Get<Beamforming>().array_geometry.size() > 1) {
      capture_.array_geometry = config.Get<Beamforming>().array_geometry;
    }
    capture_.target_direction = config.Get<Beamforming>().target_direction;
    InitializeBeamformer();
  }
#endif  // WEBRTC_ANDROID_PLATFORM_BUILD
}

int AudioProcessingImpl::proc_sample_rate_hz() const {
  // Used as callback from submodules, hence locking is not allowed.
  return capture_nonlocked_.capture_processing_format.sample_rate_hz();
}

int AudioProcessingImpl::proc_split_sample_rate_hz() const {
  // Used as callback from submodules, hence locking is not allowed.
  return capture_nonlocked_.split_rate;
}

size_t AudioProcessingImpl::num_reverse_channels() const {
  // Used as callback from submodules, hence locking is not allowed.
  return formats_.render_processing_format.num_channels();
}

size_t AudioProcessingImpl::num_input_channels() const {
  // Used as callback from submodules, hence locking is not allowed.
  return formats_.api_format.input_stream().num_channels();
}

size_t AudioProcessingImpl::num_proc_channels() const {
  // Used as callback from submodules, hence locking is not allowed.
  return capture_nonlocked_.beamformer_enabled ? 1 : num_output_channels();
}

size_t AudioProcessingImpl::num_output_channels() const {
  // Used as callback from submodules, hence locking is not allowed.
  return formats_.api_format.output_stream().num_channels();
}

void AudioProcessingImpl::set_output_will_be_muted(bool muted) {
  rtc::CritScope cs(&crit_capture_);
  capture_.output_will_be_muted = muted;
  if (private_submodules_->agc_manager.get()) {
    private_submodules_->agc_manager->SetCaptureMuted(
        capture_.output_will_be_muted);
  }
}


int AudioProcessingImpl::ProcessStream(const float* const* src,
                                       size_t samples_per_channel,
                                       int input_sample_rate_hz,
                                       ChannelLayout input_layout,
                                       int output_sample_rate_hz,
                                       ChannelLayout output_layout,
                                       float* const* dest) {
  TRACE_EVENT0("webrtc", "AudioProcessing::ProcessStream_ChannelLayout");
  StreamConfig input_stream;
  StreamConfig output_stream;
  {
    // Access the formats_.api_format.input_stream beneath the capture lock.
    // The lock must be released as it is later required in the call
    // to ProcessStream(,,,);
    rtc::CritScope cs(&crit_capture_);
    input_stream = formats_.api_format.input_stream();
    output_stream = formats_.api_format.output_stream();
  }

  input_stream.set_sample_rate_hz(input_sample_rate_hz);
  input_stream.set_num_channels(ChannelsFromLayout(input_layout));
  input_stream.set_has_keyboard(LayoutHasKeyboard(input_layout));
  output_stream.set_sample_rate_hz(output_sample_rate_hz);
  output_stream.set_num_channels(ChannelsFromLayout(output_layout));
  output_stream.set_has_keyboard(LayoutHasKeyboard(output_layout));

  if (samples_per_channel != input_stream.num_frames()) {
    return kBadDataLengthError;
  }
  return ProcessStream(src, input_stream, output_stream, dest);
}

int AudioProcessingImpl::ProcessStream(const float* const* src,
                                       const StreamConfig& input_config,
                                       const StreamConfig& output_config,
                                       float* const* dest) {
  TRACE_EVENT0("webrtc", "AudioProcessing::ProcessStream_StreamConfig");
  ProcessingConfig processing_config;
  bool reinitialization_required = false;
  {
    // Acquire the capture lock in order to safely call the function
    // that retrieves the render side data. This function accesses apm
    // getters that need the capture lock held when being called.
    rtc::CritScope cs_capture(&crit_capture_);
    EmptyQueuedRenderAudio();

    if (!src || !dest) {
      return kNullPointerError;
    }

    processing_config = formats_.api_format;
    reinitialization_required = UpdateActiveSubmoduleStates();
  }

  processing_config.input_stream() = input_config;
  processing_config.output_stream() = output_config;

  {
    // Do conditional reinitialization.
    rtc::CritScope cs_render(&crit_render_);
    RETURN_ON_ERR(
        MaybeInitializeCapture(processing_config, reinitialization_required));
  }
  rtc::CritScope cs_capture(&crit_capture_);
  RTC_DCHECK_EQ(processing_config.input_stream().num_frames(),
                formats_.api_format.input_stream().num_frames());

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_dump_.debug_file->is_open()) {
    RETURN_ON_ERR(WriteConfigMessage(false));

    debug_dump_.capture.event_msg->set_type(audioproc::Event::STREAM);
    audioproc::Stream* msg = debug_dump_.capture.event_msg->mutable_stream();
    const size_t channel_size =
        sizeof(float) * formats_.api_format.input_stream().num_frames();
    for (size_t i = 0; i < formats_.api_format.input_stream().num_channels();
         ++i)
      msg->add_input_channel(src[i], channel_size);
  }
#endif

  capture_.capture_audio->CopyFrom(src, formats_.api_format.input_stream());
  RETURN_ON_ERR(ProcessCaptureStreamLocked());
  capture_.capture_audio->CopyTo(formats_.api_format.output_stream(), dest);

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_dump_.debug_file->is_open()) {
    audioproc::Stream* msg = debug_dump_.capture.event_msg->mutable_stream();
    const size_t channel_size =
        sizeof(float) * formats_.api_format.output_stream().num_frames();
    for (size_t i = 0; i < formats_.api_format.output_stream().num_channels();
         ++i)
      msg->add_output_channel(dest[i], channel_size);
    RETURN_ON_ERR(WriteMessageToDebugFile(debug_dump_.debug_file.get(),
                                          &debug_dump_.num_bytes_left_for_log_,
                                          &crit_debug_, &debug_dump_.capture));
  }
#endif

  return kNoError;
}

void AudioProcessingImpl::QueueRenderAudio(AudioBuffer* audio) {
  EchoCancellationImpl::PackRenderAudioBuffer(audio, num_output_channels(),
                                              num_reverse_channels(),
                                              &aec_render_queue_buffer_);

  RTC_DCHECK_GE(160, audio->num_frames_per_band());

  // Insert the samples into the queue.
  if (!aec_render_signal_queue_->Insert(&aec_render_queue_buffer_)) {
    // The data queue is full and needs to be emptied.
    EmptyQueuedRenderAudio();

    // Retry the insert (should always work).
    bool result = aec_render_signal_queue_->Insert(&aec_render_queue_buffer_);
    RTC_DCHECK(result);
  }

  EchoControlMobileImpl::PackRenderAudioBuffer(audio, num_output_channels(),
                                               num_reverse_channels(),
                                               &aecm_render_queue_buffer_);

  // Insert the samples into the queue.
  if (!aecm_render_signal_queue_->Insert(&aecm_render_queue_buffer_)) {
    // The data queue is full and needs to be emptied.
    EmptyQueuedRenderAudio();

    // Retry the insert (should always work).
    bool result = aecm_render_signal_queue_->Insert(&aecm_render_queue_buffer_);
    RTC_DCHECK(result);
  }

  if (!constants_.use_experimental_agc) {
    GainControlImpl::PackRenderAudioBuffer(audio, &agc_render_queue_buffer_);
    // Insert the samples into the queue.
    if (!agc_render_signal_queue_->Insert(&agc_render_queue_buffer_)) {
      // The data queue is full and needs to be emptied.
      EmptyQueuedRenderAudio();

      // Retry the insert (should always work).
      bool result = agc_render_signal_queue_->Insert(&agc_render_queue_buffer_);
      RTC_DCHECK(result);
    }
  }

  ResidualEchoDetector::PackRenderAudioBuffer(audio, &red_render_queue_buffer_);

  // Insert the samples into the queue.
  if (!red_render_signal_queue_->Insert(&red_render_queue_buffer_)) {
    // The data queue is full and needs to be emptied.
    EmptyQueuedRenderAudio();

    // Retry the insert (should always work).
    bool result = red_render_signal_queue_->Insert(&red_render_queue_buffer_);
    RTC_DCHECK(result);
  }
}

void AudioProcessingImpl::AllocateRenderQueue() {
  const size_t new_aec_render_queue_element_max_size =
      std::max(static_cast<size_t>(1),
               kMaxAllowedValuesOfSamplesPerFrame *
                   EchoCancellationImpl::NumCancellersRequired(
                       num_output_channels(), num_reverse_channels()));

  const size_t new_aecm_render_queue_element_max_size =
      std::max(static_cast<size_t>(1),
               kMaxAllowedValuesOfSamplesPerFrame *
                   EchoControlMobileImpl::NumCancellersRequired(
                       num_output_channels(), num_reverse_channels()));

  const size_t new_agc_render_queue_element_max_size =
      std::max(static_cast<size_t>(1), kMaxAllowedValuesOfSamplesPerFrame);

  const size_t new_red_render_queue_element_max_size =
      std::max(static_cast<size_t>(1), kMaxAllowedValuesOfSamplesPerFrame);

  // Reallocate the queues if the queue item sizes are too small to fit the
  // data to put in the queues.
  if (aec_render_queue_element_max_size_ <
      new_aec_render_queue_element_max_size) {
    aec_render_queue_element_max_size_ = new_aec_render_queue_element_max_size;

    std::vector<float> template_queue_element(
        aec_render_queue_element_max_size_);

    aec_render_signal_queue_.reset(
        new SwapQueue<std::vector<float>, RenderQueueItemVerifier<float>>(
            kMaxNumFramesToBuffer, template_queue_element,
            RenderQueueItemVerifier<float>(
                aec_render_queue_element_max_size_)));

    aec_render_queue_buffer_.resize(aec_render_queue_element_max_size_);
    aec_capture_queue_buffer_.resize(aec_render_queue_element_max_size_);
  } else {
    aec_render_signal_queue_->Clear();
  }

  if (aecm_render_queue_element_max_size_ <
      new_aecm_render_queue_element_max_size) {
    aecm_render_queue_element_max_size_ =
        new_aecm_render_queue_element_max_size;

    std::vector<int16_t> template_queue_element(
        aecm_render_queue_element_max_size_);

    aecm_render_signal_queue_.reset(
        new SwapQueue<std::vector<int16_t>, RenderQueueItemVerifier<int16_t>>(
            kMaxNumFramesToBuffer, template_queue_element,
            RenderQueueItemVerifier<int16_t>(
                aecm_render_queue_element_max_size_)));

    aecm_render_queue_buffer_.resize(aecm_render_queue_element_max_size_);
    aecm_capture_queue_buffer_.resize(aecm_render_queue_element_max_size_);
  } else {
    aecm_render_signal_queue_->Clear();
  }

  if (agc_render_queue_element_max_size_ <
      new_agc_render_queue_element_max_size) {
    agc_render_queue_element_max_size_ = new_agc_render_queue_element_max_size;

    std::vector<int16_t> template_queue_element(
        agc_render_queue_element_max_size_);

    agc_render_signal_queue_.reset(
        new SwapQueue<std::vector<int16_t>, RenderQueueItemVerifier<int16_t>>(
            kMaxNumFramesToBuffer, template_queue_element,
            RenderQueueItemVerifier<int16_t>(
                agc_render_queue_element_max_size_)));

    agc_render_queue_buffer_.resize(agc_render_queue_element_max_size_);
    agc_capture_queue_buffer_.resize(agc_render_queue_element_max_size_);
  } else {
    agc_render_signal_queue_->Clear();
  }

  if (red_render_queue_element_max_size_ <
      new_red_render_queue_element_max_size) {
    red_render_queue_element_max_size_ = new_red_render_queue_element_max_size;

    std::vector<float> template_queue_element(
        red_render_queue_element_max_size_);

    red_render_signal_queue_.reset(
        new SwapQueue<std::vector<float>, RenderQueueItemVerifier<float>>(
            kMaxNumFramesToBuffer, template_queue_element,
            RenderQueueItemVerifier<float>(
                red_render_queue_element_max_size_)));

    red_render_queue_buffer_.resize(red_render_queue_element_max_size_);
    red_capture_queue_buffer_.resize(red_render_queue_element_max_size_);
  } else {
    red_render_signal_queue_->Clear();
  }
}

void AudioProcessingImpl::EmptyQueuedRenderAudio() {
  rtc::CritScope cs_capture(&crit_capture_);
  while (aec_render_signal_queue_->Remove(&aec_capture_queue_buffer_)) {
    public_submodules_->echo_cancellation->ProcessRenderAudio(
        aec_capture_queue_buffer_);
  }

  while (aecm_render_signal_queue_->Remove(&aecm_capture_queue_buffer_)) {
    public_submodules_->echo_control_mobile->ProcessRenderAudio(
        aecm_capture_queue_buffer_);
  }

  while (agc_render_signal_queue_->Remove(&agc_capture_queue_buffer_)) {
    public_submodules_->gain_control->ProcessRenderAudio(
        agc_capture_queue_buffer_);
  }

  while (red_render_signal_queue_->Remove(&red_capture_queue_buffer_)) {
    private_submodules_->residual_echo_detector->AnalyzeRenderAudio(
        red_capture_queue_buffer_);
  }
}

int AudioProcessingImpl::ProcessStream(AudioFrame* frame) {
  TRACE_EVENT0("webrtc", "AudioProcessing::ProcessStream_AudioFrame");
  {
    // Acquire the capture lock in order to safely call the function
    // that retrieves the render side data. This function accesses apm
    // getters that need the capture lock held when being called.
    // The lock needs to be released as
    // public_submodules_->echo_control_mobile->is_enabled() aquires this lock
    // as well.
    rtc::CritScope cs_capture(&crit_capture_);
    EmptyQueuedRenderAudio();
  }

  if (!frame) {
    return kNullPointerError;
  }
  // Must be a native rate.
  if (frame->sample_rate_hz_ != kSampleRate8kHz &&
      frame->sample_rate_hz_ != kSampleRate16kHz &&
      frame->sample_rate_hz_ != kSampleRate32kHz &&
      frame->sample_rate_hz_ != kSampleRate48kHz) {
    return kBadSampleRateError;
  }

  ProcessingConfig processing_config;
  bool reinitialization_required = false;
  {
    // Aquire lock for the access of api_format.
    // The lock is released immediately due to the conditional
    // reinitialization.
    rtc::CritScope cs_capture(&crit_capture_);
    // TODO(ajm): The input and output rates and channels are currently
    // constrained to be identical in the int16 interface.
    processing_config = formats_.api_format;

    reinitialization_required = UpdateActiveSubmoduleStates();
  }
  processing_config.input_stream().set_sample_rate_hz(frame->sample_rate_hz_);
  processing_config.input_stream().set_num_channels(frame->num_channels_);
  processing_config.output_stream().set_sample_rate_hz(frame->sample_rate_hz_);
  processing_config.output_stream().set_num_channels(frame->num_channels_);

  {
    // Do conditional reinitialization.
    rtc::CritScope cs_render(&crit_render_);
    RETURN_ON_ERR(
        MaybeInitializeCapture(processing_config, reinitialization_required));
  }
  rtc::CritScope cs_capture(&crit_capture_);
  if (frame->samples_per_channel_ !=
      formats_.api_format.input_stream().num_frames()) {
    return kBadDataLengthError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_dump_.debug_file->is_open()) {
    RETURN_ON_ERR(WriteConfigMessage(false));

    debug_dump_.capture.event_msg->set_type(audioproc::Event::STREAM);
    audioproc::Stream* msg = debug_dump_.capture.event_msg->mutable_stream();
    const size_t data_size =
        sizeof(int16_t) * frame->samples_per_channel_ * frame->num_channels_;
    msg->set_input_data(frame->data_, data_size);
  }
#endif

  capture_.capture_audio->DeinterleaveFrom(frame);
  RETURN_ON_ERR(ProcessCaptureStreamLocked());
  capture_.capture_audio->InterleaveTo(
      frame, submodule_states_.CaptureMultiBandProcessingActive());

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_dump_.debug_file->is_open()) {
    audioproc::Stream* msg = debug_dump_.capture.event_msg->mutable_stream();
    const size_t data_size =
        sizeof(int16_t) * frame->samples_per_channel_ * frame->num_channels_;
    msg->set_output_data(frame->data_, data_size);
    RETURN_ON_ERR(WriteMessageToDebugFile(debug_dump_.debug_file.get(),
                                          &debug_dump_.num_bytes_left_for_log_,
                                          &crit_debug_, &debug_dump_.capture));
  }
#endif

  return kNoError;
}

int AudioProcessingImpl::ProcessCaptureStreamLocked() {
  // Ensure that not both the AEC and AECM are active at the same time.
  // TODO(peah): Simplify once the public API Enable functions for these
  // are moved to APM.
  RTC_DCHECK(!(public_submodules_->echo_cancellation->is_enabled() &&
               public_submodules_->echo_control_mobile->is_enabled()));

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_dump_.debug_file->is_open()) {
    audioproc::Stream* msg = debug_dump_.capture.event_msg->mutable_stream();
    msg->set_delay(capture_nonlocked_.stream_delay_ms);
    msg->set_drift(
        public_submodules_->echo_cancellation->stream_drift_samples());
    msg->set_level(gain_control()->stream_analog_level());
    msg->set_keypress(capture_.key_pressed);
  }
#endif

  MaybeUpdateHistograms();

  AudioBuffer* capture_buffer = capture_.capture_audio.get();  // For brevity.

  capture_input_rms_.Analyze(rtc::ArrayView<const int16_t>(
      capture_buffer->channels_const()[0],
      capture_nonlocked_.capture_processing_format.num_frames()));
  const bool log_rms = ++capture_rms_interval_counter_ >= 1000;
  if (log_rms) {
    capture_rms_interval_counter_ = 0;
    RmsLevel::Levels levels = capture_input_rms_.AverageAndPeak();
    RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.ApmCaptureInputLevelAverageRms",
                                levels.average, 1, RmsLevel::kMinLevelDb, 64);
    RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.ApmCaptureInputLevelPeakRms",
                                levels.peak, 1, RmsLevel::kMinLevelDb, 64);
  }

  if (private_submodules_->echo_canceller3) {
    private_submodules_->echo_canceller3->AnalyzeCapture(capture_buffer);
  }

  if (constants_.use_experimental_agc &&
      public_submodules_->gain_control->is_enabled()) {
    private_submodules_->agc_manager->AnalyzePreProcess(
        capture_buffer->channels()[0], capture_buffer->num_channels(),
        capture_nonlocked_.capture_processing_format.num_frames());
  }

  if (submodule_states_.CaptureMultiBandSubModulesActive() &&
      SampleRateSupportsMultiBand(
          capture_nonlocked_.capture_processing_format.sample_rate_hz())) {
    capture_buffer->SplitIntoFrequencyBands();
  }

  if (private_submodules_->echo_canceller3) {
    // Force down-mixing of the number of channels after the detection of
    // capture signal saturation.
    // TODO(peah): Look into ensuring that this kind of tampering with the
    // AudioBuffer functionality should not be needed.
    capture_buffer->set_num_channels(1);
  }

  if (capture_nonlocked_.beamformer_enabled) {
    private_submodules_->beamformer->AnalyzeChunk(
        *capture_buffer->split_data_f());
    // Discards all channels by the leftmost one.
    capture_buffer->set_num_channels(1);
  }

  // TODO(peah): Move the AEC3 low-cut filter to this place.
  if (private_submodules_->low_cut_filter &&
      !private_submodules_->echo_canceller3) {
    private_submodules_->low_cut_filter->Process(capture_buffer);
  }
  RETURN_ON_ERR(
      public_submodules_->gain_control->AnalyzeCaptureAudio(capture_buffer));
  public_submodules_->noise_suppression->AnalyzeCaptureAudio(capture_buffer);

  // Ensure that the stream delay was set before the call to the
  // AEC ProcessCaptureAudio function.
  if (public_submodules_->echo_cancellation->is_enabled() &&
      !was_stream_delay_set()) {
    return AudioProcessing::kStreamParameterNotSetError;
  }

  if (private_submodules_->echo_canceller3) {
    private_submodules_->echo_canceller3->ProcessCapture(capture_buffer, false);
  } else {
    RETURN_ON_ERR(public_submodules_->echo_cancellation->ProcessCaptureAudio(
        capture_buffer, stream_delay_ms()));
  }

  if (public_submodules_->echo_control_mobile->is_enabled() &&
      public_submodules_->noise_suppression->is_enabled()) {
    capture_buffer->CopyLowPassToReference();
  }
  public_submodules_->noise_suppression->ProcessCaptureAudio(capture_buffer);
#if WEBRTC_INTELLIGIBILITY_ENHANCER
  if (capture_nonlocked_.intelligibility_enabled) {
    RTC_DCHECK(public_submodules_->noise_suppression->is_enabled());
    int gain_db = public_submodules_->gain_control->is_enabled() ?
                  public_submodules_->gain_control->compression_gain_db() :
                  0;
    float gain = std::pow(10.f, gain_db / 20.f);
    gain *= capture_nonlocked_.level_controller_enabled ?
            private_submodules_->level_controller->GetLastGain() :
            1.f;
    public_submodules_->intelligibility_enhancer->SetCaptureNoiseEstimate(
        public_submodules_->noise_suppression->NoiseEstimate(), gain);
  }
#endif

  // Ensure that the stream delay was set before the call to the
  // AECM ProcessCaptureAudio function.
  if (public_submodules_->echo_control_mobile->is_enabled() &&
      !was_stream_delay_set()) {
    return AudioProcessing::kStreamParameterNotSetError;
  }

  RETURN_ON_ERR(public_submodules_->echo_control_mobile->ProcessCaptureAudio(
      capture_buffer, stream_delay_ms()));

  if (config_.residual_echo_detector.enabled) {
    private_submodules_->residual_echo_detector->AnalyzeCaptureAudio(
        rtc::ArrayView<const float>(
            capture_buffer->split_bands_const_f(0)[kBand0To8kHz],
            capture_buffer->num_frames_per_band()));
  }

  if (capture_nonlocked_.beamformer_enabled) {
    private_submodules_->beamformer->PostFilter(capture_buffer->split_data_f());
  }

  public_submodules_->voice_detection->ProcessCaptureAudio(capture_buffer);

  if (constants_.use_experimental_agc &&
      public_submodules_->gain_control->is_enabled() &&
      (!capture_nonlocked_.beamformer_enabled ||
       private_submodules_->beamformer->is_target_present())) {
    private_submodules_->agc_manager->Process(
        capture_buffer->split_bands_const(0)[kBand0To8kHz],
        capture_buffer->num_frames_per_band(), capture_nonlocked_.split_rate);
  }
  RETURN_ON_ERR(public_submodules_->gain_control->ProcessCaptureAudio(
      capture_buffer, echo_cancellation()->stream_has_echo()));

  if (submodule_states_.CaptureMultiBandProcessingActive() &&
      SampleRateSupportsMultiBand(
          capture_nonlocked_.capture_processing_format.sample_rate_hz())) {
    capture_buffer->MergeFrequencyBands();
  }

  // TODO(aluebs): Investigate if the transient suppression placement should be
  // before or after the AGC.
  if (capture_.transient_suppressor_enabled) {
    float voice_probability =
        private_submodules_->agc_manager.get()
            ? private_submodules_->agc_manager->voice_probability()
            : 1.f;

    public_submodules_->transient_suppressor->Suppress(
        capture_buffer->channels_f()[0], capture_buffer->num_frames(),
        capture_buffer->num_channels(),
        capture_buffer->split_bands_const_f(0)[kBand0To8kHz],
        capture_buffer->num_frames_per_band(), capture_buffer->keyboard_data(),
        capture_buffer->num_keyboard_frames(), voice_probability,
        capture_.key_pressed);
  }

  if (capture_nonlocked_.level_controller_enabled) {
    private_submodules_->level_controller->Process(capture_buffer);
  }

  // The level estimator operates on the recombined data.
  public_submodules_->level_estimator->ProcessStream(capture_buffer);

  capture_output_rms_.Analyze(rtc::ArrayView<const int16_t>(
      capture_buffer->channels_const()[0],
      capture_nonlocked_.capture_processing_format.num_frames()));
  if (log_rms) {
    RmsLevel::Levels levels = capture_output_rms_.AverageAndPeak();
    RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.ApmCaptureOutputLevelAverageRms",
                                levels.average, 1, RmsLevel::kMinLevelDb, 64);
    RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.ApmCaptureOutputLevelPeakRms",
                                levels.peak, 1, RmsLevel::kMinLevelDb, 64);
  }

  capture_.was_stream_delay_set = false;
  return kNoError;
}

int AudioProcessingImpl::AnalyzeReverseStream(const float* const* data,
                                              size_t samples_per_channel,
                                              int sample_rate_hz,
                                              ChannelLayout layout) {
  TRACE_EVENT0("webrtc", "AudioProcessing::AnalyzeReverseStream_ChannelLayout");
  rtc::CritScope cs(&crit_render_);
  const StreamConfig reverse_config = {
      sample_rate_hz, ChannelsFromLayout(layout), LayoutHasKeyboard(layout),
  };
  if (samples_per_channel != reverse_config.num_frames()) {
    return kBadDataLengthError;
  }
  return AnalyzeReverseStreamLocked(data, reverse_config, reverse_config);
}

int AudioProcessingImpl::ProcessReverseStream(const float* const* src,
                                              const StreamConfig& input_config,
                                              const StreamConfig& output_config,
                                              float* const* dest) {
  TRACE_EVENT0("webrtc", "AudioProcessing::ProcessReverseStream_StreamConfig");
  rtc::CritScope cs(&crit_render_);
  RETURN_ON_ERR(AnalyzeReverseStreamLocked(src, input_config, output_config));
  if (submodule_states_.RenderMultiBandProcessingActive()) {
    render_.render_audio->CopyTo(formats_.api_format.reverse_output_stream(),
                                 dest);
  } else if (formats_.api_format.reverse_input_stream() !=
             formats_.api_format.reverse_output_stream()) {
    render_.render_converter->Convert(src, input_config.num_samples(), dest,
                                      output_config.num_samples());
  } else {
    CopyAudioIfNeeded(src, input_config.num_frames(),
                      input_config.num_channels(), dest);
  }

  return kNoError;
}

int AudioProcessingImpl::AnalyzeReverseStreamLocked(
    const float* const* src,
    const StreamConfig& input_config,
    const StreamConfig& output_config) {
  if (src == nullptr) {
    return kNullPointerError;
  }

  if (input_config.num_channels() == 0) {
    return kBadNumberChannelsError;
  }

  ProcessingConfig processing_config = formats_.api_format;
  processing_config.reverse_input_stream() = input_config;
  processing_config.reverse_output_stream() = output_config;

  RETURN_ON_ERR(MaybeInitializeRender(processing_config));
  assert(input_config.num_frames() ==
         formats_.api_format.reverse_input_stream().num_frames());

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_dump_.debug_file->is_open()) {
    debug_dump_.render.event_msg->set_type(audioproc::Event::REVERSE_STREAM);
    audioproc::ReverseStream* msg =
        debug_dump_.render.event_msg->mutable_reverse_stream();
    const size_t channel_size =
        sizeof(float) * formats_.api_format.reverse_input_stream().num_frames();
    for (size_t i = 0;
         i < formats_.api_format.reverse_input_stream().num_channels(); ++i)
      msg->add_channel(src[i], channel_size);
    RETURN_ON_ERR(WriteMessageToDebugFile(debug_dump_.debug_file.get(),
                                          &debug_dump_.num_bytes_left_for_log_,
                                          &crit_debug_, &debug_dump_.render));
  }
#endif

  render_.render_audio->CopyFrom(src,
                                 formats_.api_format.reverse_input_stream());
  return ProcessRenderStreamLocked();
}

int AudioProcessingImpl::ProcessReverseStream(AudioFrame* frame) {
  TRACE_EVENT0("webrtc", "AudioProcessing::ProcessReverseStream_AudioFrame");
  rtc::CritScope cs(&crit_render_);
  if (frame == nullptr) {
    return kNullPointerError;
  }
  // Must be a native rate.
  if (frame->sample_rate_hz_ != kSampleRate8kHz &&
      frame->sample_rate_hz_ != kSampleRate16kHz &&
      frame->sample_rate_hz_ != kSampleRate32kHz &&
      frame->sample_rate_hz_ != kSampleRate48kHz) {
    return kBadSampleRateError;
  }

  if (frame->num_channels_ <= 0) {
    return kBadNumberChannelsError;
  }

  ProcessingConfig processing_config = formats_.api_format;
  processing_config.reverse_input_stream().set_sample_rate_hz(
      frame->sample_rate_hz_);
  processing_config.reverse_input_stream().set_num_channels(
      frame->num_channels_);
  processing_config.reverse_output_stream().set_sample_rate_hz(
      frame->sample_rate_hz_);
  processing_config.reverse_output_stream().set_num_channels(
      frame->num_channels_);

  RETURN_ON_ERR(MaybeInitializeRender(processing_config));
  if (frame->samples_per_channel_ !=
      formats_.api_format.reverse_input_stream().num_frames()) {
    return kBadDataLengthError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_dump_.debug_file->is_open()) {
    debug_dump_.render.event_msg->set_type(audioproc::Event::REVERSE_STREAM);
    audioproc::ReverseStream* msg =
        debug_dump_.render.event_msg->mutable_reverse_stream();
    const size_t data_size =
        sizeof(int16_t) * frame->samples_per_channel_ * frame->num_channels_;
    msg->set_data(frame->data_, data_size);
    RETURN_ON_ERR(WriteMessageToDebugFile(debug_dump_.debug_file.get(),
                                          &debug_dump_.num_bytes_left_for_log_,
                                          &crit_debug_, &debug_dump_.render));
  }
#endif
  render_.render_audio->DeinterleaveFrom(frame);
  RETURN_ON_ERR(ProcessRenderStreamLocked());
  render_.render_audio->InterleaveTo(
      frame, submodule_states_.RenderMultiBandProcessingActive());
  return kNoError;
}

int AudioProcessingImpl::ProcessRenderStreamLocked() {
  AudioBuffer* render_buffer = render_.render_audio.get();  // For brevity.
  if (submodule_states_.RenderMultiBandSubModulesActive() &&
      SampleRateSupportsMultiBand(
          formats_.render_processing_format.sample_rate_hz())) {
    render_buffer->SplitIntoFrequencyBands();
  }

#if WEBRTC_INTELLIGIBILITY_ENHANCER
  if (capture_nonlocked_.intelligibility_enabled) {
    public_submodules_->intelligibility_enhancer->ProcessRenderAudio(
        render_buffer);
  }
#endif

  QueueRenderAudio(render_buffer);
  // TODO(peah): Perform the queueing ínside QueueRenderAudiuo().
  if (private_submodules_->echo_canceller3) {
    if (!private_submodules_->echo_canceller3->AnalyzeRender(render_buffer)) {
      // TODO(peah): Lock and empty render queue, and try again.
    }
  }

  if (submodule_states_.RenderMultiBandProcessingActive() &&
      SampleRateSupportsMultiBand(
          formats_.render_processing_format.sample_rate_hz())) {
    render_buffer->MergeFrequencyBands();
  }

  return kNoError;
}

int AudioProcessingImpl::set_stream_delay_ms(int delay) {
  rtc::CritScope cs(&crit_capture_);
  Error retval = kNoError;
  capture_.was_stream_delay_set = true;
  delay += capture_.delay_offset_ms;

  if (delay < 0) {
    delay = 0;
    retval = kBadStreamParameterWarning;
  }

  // TODO(ajm): the max is rather arbitrarily chosen; investigate.
  if (delay > 500) {
    delay = 500;
    retval = kBadStreamParameterWarning;
  }

  capture_nonlocked_.stream_delay_ms = delay;
  return retval;
}

int AudioProcessingImpl::stream_delay_ms() const {
  // Used as callback from submodules, hence locking is not allowed.
  return capture_nonlocked_.stream_delay_ms;
}

bool AudioProcessingImpl::was_stream_delay_set() const {
  // Used as callback from submodules, hence locking is not allowed.
  return capture_.was_stream_delay_set;
}

void AudioProcessingImpl::set_stream_key_pressed(bool key_pressed) {
  rtc::CritScope cs(&crit_capture_);
  capture_.key_pressed = key_pressed;
}

void AudioProcessingImpl::set_delay_offset_ms(int offset) {
  rtc::CritScope cs(&crit_capture_);
  capture_.delay_offset_ms = offset;
}

int AudioProcessingImpl::delay_offset_ms() const {
  rtc::CritScope cs(&crit_capture_);
  return capture_.delay_offset_ms;
}

int AudioProcessingImpl::StartDebugRecording(
    const char filename[AudioProcessing::kMaxFilenameSize],
    int64_t max_log_size_bytes) {
  // Run in a single-threaded manner.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);
  static_assert(kMaxFilenameSize == FileWrapper::kMaxFileNameSize, "");

  if (filename == nullptr) {
    return kNullPointerError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  debug_dump_.num_bytes_left_for_log_ = max_log_size_bytes;
  // Stop any ongoing recording.
  debug_dump_.debug_file->CloseFile();

  if (!debug_dump_.debug_file->OpenFile(filename, false)) {
    return kFileError;
  }

  RETURN_ON_ERR(WriteConfigMessage(true));
  RETURN_ON_ERR(WriteInitMessage());
  return kNoError;
#else
  return kUnsupportedFunctionError;
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP
}

int AudioProcessingImpl::StartDebugRecording(FILE* handle,
                                             int64_t max_log_size_bytes) {
  // Run in a single-threaded manner.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);

  if (handle == nullptr) {
    return kNullPointerError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  debug_dump_.num_bytes_left_for_log_ = max_log_size_bytes;

  // Stop any ongoing recording.
  debug_dump_.debug_file->CloseFile();

  if (!debug_dump_.debug_file->OpenFromFileHandle(handle)) {
    return kFileError;
  }

  RETURN_ON_ERR(WriteConfigMessage(true));
  RETURN_ON_ERR(WriteInitMessage());
  return kNoError;
#else
  return kUnsupportedFunctionError;
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP
}

int AudioProcessingImpl::StartDebugRecording(FILE* handle) {
  return StartDebugRecording(handle, -1);
}

int AudioProcessingImpl::StartDebugRecordingForPlatformFile(
    rtc::PlatformFile handle) {
  // Run in a single-threaded manner.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);
  FILE* stream = rtc::FdopenPlatformFileForWriting(handle);
  return StartDebugRecording(stream, -1);
}

int AudioProcessingImpl::StopDebugRecording() {
  // Run in a single-threaded manner.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // We just return if recording hasn't started.
  debug_dump_.debug_file->CloseFile();
  return kNoError;
#else
  return kUnsupportedFunctionError;
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP
}

AudioProcessing::AudioProcessingStatistics::AudioProcessingStatistics() {
  residual_echo_return_loss.Set(-100.0f, -100.0f, -100.0f, -100.0f);
  echo_return_loss.Set(-100.0f, -100.0f, -100.0f, -100.0f);
  echo_return_loss_enhancement.Set(-100.0f, -100.0f, -100.0f, -100.0f);
  a_nlp.Set(-100.0f, -100.0f, -100.0f, -100.0f);
}

AudioProcessing::AudioProcessingStatistics::AudioProcessingStatistics(
    const AudioProcessingStatistics& other) = default;

AudioProcessing::AudioProcessingStatistics::~AudioProcessingStatistics() =
    default;

// TODO(ivoc): Remove this when GetStatistics() becomes pure virtual.
AudioProcessing::AudioProcessingStatistics AudioProcessing::GetStatistics()
    const {
  return AudioProcessingStatistics();
}

AudioProcessing::AudioProcessingStatistics AudioProcessingImpl::GetStatistics()
    const {
  AudioProcessingStatistics stats;
  EchoCancellation::Metrics metrics;
  int success = public_submodules_->echo_cancellation->GetMetrics(&metrics);
  if (success == Error::kNoError) {
    stats.a_nlp.Set(metrics.a_nlp);
    stats.divergent_filter_fraction = metrics.divergent_filter_fraction;
    stats.echo_return_loss.Set(metrics.echo_return_loss);
    stats.echo_return_loss_enhancement.Set(
        metrics.echo_return_loss_enhancement);
    stats.residual_echo_return_loss.Set(metrics.residual_echo_return_loss);
  }
  stats.residual_echo_likelihood =
      private_submodules_->residual_echo_detector->echo_likelihood();
  stats.residual_echo_likelihood_recent_max =
      private_submodules_->residual_echo_detector->echo_likelihood_recent_max();
  public_submodules_->echo_cancellation->GetDelayMetrics(
      &stats.delay_median, &stats.delay_standard_deviation,
      &stats.fraction_poor_delays);
  return stats;
}

EchoCancellation* AudioProcessingImpl::echo_cancellation() const {
  return public_submodules_->echo_cancellation.get();
}

EchoControlMobile* AudioProcessingImpl::echo_control_mobile() const {
  return public_submodules_->echo_control_mobile.get();
}

GainControl* AudioProcessingImpl::gain_control() const {
  if (constants_.use_experimental_agc) {
    return public_submodules_->gain_control_for_experimental_agc.get();
  }
  return public_submodules_->gain_control.get();
}

HighPassFilter* AudioProcessingImpl::high_pass_filter() const {
  return high_pass_filter_impl_.get();
}

LevelEstimator* AudioProcessingImpl::level_estimator() const {
  return public_submodules_->level_estimator.get();
}

NoiseSuppression* AudioProcessingImpl::noise_suppression() const {
  return public_submodules_->noise_suppression.get();
}

VoiceDetection* AudioProcessingImpl::voice_detection() const {
  return public_submodules_->voice_detection.get();
}

void AudioProcessingImpl::MutateConfig(
    rtc::FunctionView<void(AudioProcessing::Config*)> mutator) {
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);
  mutator(&config_);
  ApplyConfig(config_);
}

AudioProcessing::Config AudioProcessingImpl::GetConfig() const {
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);
  return config_;
}

bool AudioProcessingImpl::UpdateActiveSubmoduleStates() {
  return submodule_states_.Update(
      config_.high_pass_filter.enabled,
      public_submodules_->echo_cancellation->is_enabled(),
      public_submodules_->echo_control_mobile->is_enabled(),
      config_.residual_echo_detector.enabled,
      public_submodules_->noise_suppression->is_enabled(),
      capture_nonlocked_.intelligibility_enabled,
      capture_nonlocked_.beamformer_enabled,
      public_submodules_->gain_control->is_enabled(),
      capture_nonlocked_.level_controller_enabled,
      capture_nonlocked_.echo_canceller3_enabled,
      public_submodules_->voice_detection->is_enabled(),
      public_submodules_->level_estimator->is_enabled(),
      capture_.transient_suppressor_enabled);
}


void AudioProcessingImpl::InitializeTransient() {
  if (capture_.transient_suppressor_enabled) {
    if (!public_submodules_->transient_suppressor.get()) {
      public_submodules_->transient_suppressor.reset(new TransientSuppressor());
    }
    public_submodules_->transient_suppressor->Initialize(
        capture_nonlocked_.capture_processing_format.sample_rate_hz(),
        capture_nonlocked_.split_rate, num_proc_channels());
  }
}

void AudioProcessingImpl::InitializeBeamformer() {
  if (capture_nonlocked_.beamformer_enabled) {
    if (!private_submodules_->beamformer) {
      private_submodules_->beamformer.reset(new NonlinearBeamformer(
          capture_.array_geometry, 1u, capture_.target_direction));
    }
    private_submodules_->beamformer->Initialize(kChunkSizeMs,
                                                capture_nonlocked_.split_rate);
  }
}

void AudioProcessingImpl::InitializeIntelligibility() {
#if WEBRTC_INTELLIGIBILITY_ENHANCER
  if (capture_nonlocked_.intelligibility_enabled) {
    public_submodules_->intelligibility_enhancer.reset(
        new IntelligibilityEnhancer(capture_nonlocked_.split_rate,
                                    render_.render_audio->num_channels(),
                                    render_.render_audio->num_bands(),
                                    NoiseSuppressionImpl::num_noise_bins()));
  }
#endif
}

void AudioProcessingImpl::InitializeLowCutFilter() {
  if (config_.high_pass_filter.enabled) {
    private_submodules_->low_cut_filter.reset(
        new LowCutFilter(num_proc_channels(), proc_sample_rate_hz()));
  } else {
    private_submodules_->low_cut_filter.reset();
  }
}
void AudioProcessingImpl::InitializeEchoCanceller3() {
  if (capture_nonlocked_.echo_canceller3_enabled) {
    private_submodules_->echo_canceller3.reset(
        new EchoCanceller3(proc_sample_rate_hz(), true));
  } else {
    private_submodules_->echo_canceller3.reset();
  }
}

void AudioProcessingImpl::InitializeLevelController() {
  private_submodules_->level_controller->Initialize(proc_sample_rate_hz());
}

void AudioProcessingImpl::InitializeResidualEchoDetector() {
  private_submodules_->residual_echo_detector->Initialize();
}

void AudioProcessingImpl::MaybeUpdateHistograms() {
  static const int kMinDiffDelayMs = 60;

  if (echo_cancellation()->is_enabled()) {
    // Activate delay_jumps_ counters if we know echo_cancellation is runnning.
    // If a stream has echo we know that the echo_cancellation is in process.
    if (capture_.stream_delay_jumps == -1 &&
        echo_cancellation()->stream_has_echo()) {
      capture_.stream_delay_jumps = 0;
    }
    if (capture_.aec_system_delay_jumps == -1 &&
        echo_cancellation()->stream_has_echo()) {
      capture_.aec_system_delay_jumps = 0;
    }

    // Detect a jump in platform reported system delay and log the difference.
    const int diff_stream_delay_ms =
        capture_nonlocked_.stream_delay_ms - capture_.last_stream_delay_ms;
    if (diff_stream_delay_ms > kMinDiffDelayMs &&
        capture_.last_stream_delay_ms != 0) {
      RTC_HISTOGRAM_COUNTS("WebRTC.Audio.PlatformReportedStreamDelayJump",
                           diff_stream_delay_ms, kMinDiffDelayMs, 1000, 100);
      if (capture_.stream_delay_jumps == -1) {
        capture_.stream_delay_jumps = 0;  // Activate counter if needed.
      }
      capture_.stream_delay_jumps++;
    }
    capture_.last_stream_delay_ms = capture_nonlocked_.stream_delay_ms;

    // Detect a jump in AEC system delay and log the difference.
    const int samples_per_ms =
        rtc::CheckedDivExact(capture_nonlocked_.split_rate, 1000);
    RTC_DCHECK_LT(0, samples_per_ms);
    const int aec_system_delay_ms =
        public_submodules_->echo_cancellation->GetSystemDelayInSamples() /
        samples_per_ms;
    const int diff_aec_system_delay_ms =
        aec_system_delay_ms - capture_.last_aec_system_delay_ms;
    if (diff_aec_system_delay_ms > kMinDiffDelayMs &&
        capture_.last_aec_system_delay_ms != 0) {
      RTC_HISTOGRAM_COUNTS("WebRTC.Audio.AecSystemDelayJump",
                           diff_aec_system_delay_ms, kMinDiffDelayMs, 1000,
                           100);
      if (capture_.aec_system_delay_jumps == -1) {
        capture_.aec_system_delay_jumps = 0;  // Activate counter if needed.
      }
      capture_.aec_system_delay_jumps++;
    }
    capture_.last_aec_system_delay_ms = aec_system_delay_ms;
  }
}

void AudioProcessingImpl::UpdateHistogramsOnCallEnd() {
  // Run in a single-threaded manner.
  rtc::CritScope cs_render(&crit_render_);
  rtc::CritScope cs_capture(&crit_capture_);

  if (capture_.stream_delay_jumps > -1) {
    RTC_HISTOGRAM_ENUMERATION(
        "WebRTC.Audio.NumOfPlatformReportedStreamDelayJumps",
        capture_.stream_delay_jumps, 51);
  }
  capture_.stream_delay_jumps = -1;
  capture_.last_stream_delay_ms = 0;

  if (capture_.aec_system_delay_jumps > -1) {
    RTC_HISTOGRAM_ENUMERATION("WebRTC.Audio.NumOfAecSystemDelayJumps",
                              capture_.aec_system_delay_jumps, 51);
  }
  capture_.aec_system_delay_jumps = -1;
  capture_.last_aec_system_delay_ms = 0;
}

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
int AudioProcessingImpl::WriteMessageToDebugFile(
    FileWrapper* debug_file,
    int64_t* filesize_limit_bytes,
    rtc::CriticalSection* crit_debug,
    ApmDebugDumpThreadState* debug_state) {
  int32_t size = debug_state->event_msg->ByteSize();
  if (size <= 0) {
    return kUnspecifiedError;
  }
#if defined(WEBRTC_ARCH_BIG_ENDIAN)
// TODO(ajm): Use little-endian "on the wire". For the moment, we can be
//            pretty safe in assuming little-endian.
#endif

  if (!debug_state->event_msg->SerializeToString(&debug_state->event_str)) {
    return kUnspecifiedError;
  }

  {
    // Ensure atomic writes of the message.
    rtc::CritScope cs_debug(crit_debug);

    RTC_DCHECK(debug_file->is_open());
    // Update the byte counter.
    if (*filesize_limit_bytes >= 0) {
      *filesize_limit_bytes -=
          (sizeof(int32_t) + debug_state->event_str.length());
      if (*filesize_limit_bytes < 0) {
        // Not enough bytes are left to write this message, so stop logging.
        debug_file->CloseFile();
        return kNoError;
      }
    }
    // Write message preceded by its size.
    if (!debug_file->Write(&size, sizeof(int32_t))) {
      return kFileError;
    }
    if (!debug_file->Write(debug_state->event_str.data(),
                           debug_state->event_str.length())) {
      return kFileError;
    }
  }

  debug_state->event_msg->Clear();

  return kNoError;
}

int AudioProcessingImpl::WriteInitMessage() {
  debug_dump_.capture.event_msg->set_type(audioproc::Event::INIT);
  audioproc::Init* msg = debug_dump_.capture.event_msg->mutable_init();
  msg->set_sample_rate(formats_.api_format.input_stream().sample_rate_hz());

  msg->set_num_input_channels(static_cast<google::protobuf::int32>(
      formats_.api_format.input_stream().num_channels()));
  msg->set_num_output_channels(static_cast<google::protobuf::int32>(
      formats_.api_format.output_stream().num_channels()));
  msg->set_num_reverse_channels(static_cast<google::protobuf::int32>(
      formats_.api_format.reverse_input_stream().num_channels()));
  msg->set_reverse_sample_rate(
      formats_.api_format.reverse_input_stream().sample_rate_hz());
  msg->set_output_sample_rate(
      formats_.api_format.output_stream().sample_rate_hz());
  msg->set_reverse_output_sample_rate(
      formats_.api_format.reverse_output_stream().sample_rate_hz());
  msg->set_num_reverse_output_channels(
      formats_.api_format.reverse_output_stream().num_channels());

  RETURN_ON_ERR(WriteMessageToDebugFile(debug_dump_.debug_file.get(),
                                        &debug_dump_.num_bytes_left_for_log_,
                                        &crit_debug_, &debug_dump_.capture));
  return kNoError;
}

int AudioProcessingImpl::WriteConfigMessage(bool forced) {
  audioproc::Config config;

  config.set_aec_enabled(public_submodules_->echo_cancellation->is_enabled());
  config.set_aec_delay_agnostic_enabled(
      public_submodules_->echo_cancellation->is_delay_agnostic_enabled());
  config.set_aec_drift_compensation_enabled(
      public_submodules_->echo_cancellation->is_drift_compensation_enabled());
  config.set_aec_extended_filter_enabled(
      public_submodules_->echo_cancellation->is_extended_filter_enabled());
  config.set_aec_suppression_level(static_cast<int>(
      public_submodules_->echo_cancellation->suppression_level()));

  config.set_aecm_enabled(
      public_submodules_->echo_control_mobile->is_enabled());
  config.set_aecm_comfort_noise_enabled(
      public_submodules_->echo_control_mobile->is_comfort_noise_enabled());
  config.set_aecm_routing_mode(static_cast<int>(
      public_submodules_->echo_control_mobile->routing_mode()));

  config.set_agc_enabled(public_submodules_->gain_control->is_enabled());
  config.set_agc_mode(
      static_cast<int>(public_submodules_->gain_control->mode()));
  config.set_agc_limiter_enabled(
      public_submodules_->gain_control->is_limiter_enabled());
  config.set_noise_robust_agc_enabled(constants_.use_experimental_agc);

  config.set_hpf_enabled(config_.high_pass_filter.enabled);

  config.set_ns_enabled(public_submodules_->noise_suppression->is_enabled());
  config.set_ns_level(
      static_cast<int>(public_submodules_->noise_suppression->level()));

  config.set_transient_suppression_enabled(
      capture_.transient_suppressor_enabled);
  config.set_intelligibility_enhancer_enabled(
      capture_nonlocked_.intelligibility_enabled);

  std::string experiments_description =
      public_submodules_->echo_cancellation->GetExperimentsDescription();
  // TODO(peah): Add semicolon-separated concatenations of experiment
  // descriptions for other submodules.
  if (capture_nonlocked_.level_controller_enabled) {
    experiments_description += "LevelController;";
  }
  if (constants_.agc_clipped_level_min != kClippedLevelMin) {
    experiments_description += "AgcClippingLevelExperiment;";
  }
  if (capture_nonlocked_.echo_canceller3_enabled) {
    experiments_description += "EchoCanceller3;";
  }
  config.set_experiments_description(experiments_description);

  std::string serialized_config = config.SerializeAsString();
  if (!forced &&
      debug_dump_.capture.last_serialized_config == serialized_config) {
    return kNoError;
  }

  debug_dump_.capture.last_serialized_config = serialized_config;

  debug_dump_.capture.event_msg->set_type(audioproc::Event::CONFIG);
  debug_dump_.capture.event_msg->mutable_config()->CopyFrom(config);

  RETURN_ON_ERR(WriteMessageToDebugFile(debug_dump_.debug_file.get(),
                                        &debug_dump_.num_bytes_left_for_log_,
                                        &crit_debug_, &debug_dump_.capture));
  return kNoError;
}
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP

AudioProcessingImpl::ApmCaptureState::ApmCaptureState(
    bool transient_suppressor_enabled,
    const std::vector<Point>& array_geometry,
    SphericalPointf target_direction)
    : aec_system_delay_jumps(-1),
      delay_offset_ms(0),
      was_stream_delay_set(false),
      last_stream_delay_ms(0),
      last_aec_system_delay_ms(0),
      stream_delay_jumps(-1),
      output_will_be_muted(false),
      key_pressed(false),
      transient_suppressor_enabled(transient_suppressor_enabled),
      array_geometry(array_geometry),
      target_direction(target_direction),
      capture_processing_format(kSampleRate16kHz),
      split_rate(kSampleRate16kHz) {}

AudioProcessingImpl::ApmCaptureState::~ApmCaptureState() = default;

AudioProcessingImpl::ApmRenderState::ApmRenderState() = default;

AudioProcessingImpl::ApmRenderState::~ApmRenderState() = default;

}  // namespace webrtc
