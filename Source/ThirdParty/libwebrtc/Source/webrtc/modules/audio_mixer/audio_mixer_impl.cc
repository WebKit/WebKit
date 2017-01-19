/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_mixer/audio_mixer_impl.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "webrtc/base/logging.h"
#include "webrtc/modules/audio_mixer/audio_frame_manipulator.h"
#include "webrtc/modules/utility/include/audio_frame_operations.h"

namespace webrtc {
namespace {

struct SourceFrame {
  SourceFrame(AudioMixerImpl::SourceStatus* source_status,
              AudioFrame* audio_frame,
              bool muted)
      : source_status(source_status), audio_frame(audio_frame), muted(muted) {
    RTC_DCHECK(source_status);
    RTC_DCHECK(audio_frame);
    if (!muted) {
      energy = AudioMixerCalculateEnergy(*audio_frame);
    }
  }

  SourceFrame(AudioMixerImpl::SourceStatus* source_status,
              AudioFrame* audio_frame,
              bool muted,
              uint32_t energy)
      : source_status(source_status),
        audio_frame(audio_frame),
        muted(muted),
        energy(energy) {
    RTC_DCHECK(source_status);
    RTC_DCHECK(audio_frame);
  }

  AudioMixerImpl::SourceStatus* source_status = nullptr;
  AudioFrame* audio_frame = nullptr;
  bool muted = true;
  uint32_t energy = 0;
};

// ShouldMixBefore(a, b) is used to select mixer sources.
bool ShouldMixBefore(const SourceFrame& a, const SourceFrame& b) {
  if (a.muted != b.muted) {
    return b.muted;
  }

  const auto a_activity = a.audio_frame->vad_activity_;
  const auto b_activity = b.audio_frame->vad_activity_;

  if (a_activity != b_activity) {
    return a_activity == AudioFrame::kVadActive;
  }

  return a.energy > b.energy;
}

void RampAndUpdateGain(
    const std::vector<SourceFrame>& mixed_sources_and_frames) {
  for (const auto& source_frame : mixed_sources_and_frames) {
    float target_gain = source_frame.source_status->is_mixed ? 1.0f : 0.0f;
    Ramp(source_frame.source_status->gain, target_gain,
         source_frame.audio_frame);
    source_frame.source_status->gain = target_gain;
  }
}

// Mix the AudioFrames stored in audioFrameList into mixed_audio.
int32_t MixFromList(AudioFrame* mixed_audio,
                    const AudioFrameList& audio_frame_list,
                    bool use_limiter) {
  if (audio_frame_list.empty()) {
    return 0;
  }

  if (audio_frame_list.size() == 1) {
    mixed_audio->timestamp_ = audio_frame_list.front()->timestamp_;
    mixed_audio->elapsed_time_ms_ = audio_frame_list.front()->elapsed_time_ms_;
  } else {
    // TODO(wu): Issue 3390.
    // Audio frame timestamp is only supported in one channel case.
    mixed_audio->timestamp_ = 0;
    mixed_audio->elapsed_time_ms_ = -1;
  }

  for (const auto& frame : audio_frame_list) {
    RTC_DCHECK_EQ(mixed_audio->sample_rate_hz_, frame->sample_rate_hz_);
    RTC_DCHECK_EQ(
        frame->samples_per_channel_,
        static_cast<size_t>((mixed_audio->sample_rate_hz_ *
                             webrtc::AudioMixerImpl::kFrameDurationInMs) /
                            1000));

    // Mix |f.frame| into |mixed_audio|, with saturation protection.
    // These effect is applied to |f.frame| itself prior to mixing.
    if (use_limiter) {
      // Divide by two to avoid saturation in the mixing.
      // This is only meaningful if the limiter will be used.
      *frame >>= 1;
    }
    RTC_DCHECK_EQ(frame->num_channels_, mixed_audio->num_channels_);
    *mixed_audio += *frame;
  }
  return 0;
}

AudioMixerImpl::SourceStatusList::const_iterator FindSourceInList(
    AudioMixerImpl::Source const* audio_source,
    AudioMixerImpl::SourceStatusList const* audio_source_list) {
  return std::find_if(
      audio_source_list->begin(), audio_source_list->end(),
      [audio_source](const std::unique_ptr<AudioMixerImpl::SourceStatus>& p) {
        return p->audio_source == audio_source;
      });
}

// TODO(aleloi): remove non-const version when WEBRTC only supports modern STL.
AudioMixerImpl::SourceStatusList::iterator FindSourceInList(
    AudioMixerImpl::Source const* audio_source,
    AudioMixerImpl::SourceStatusList* audio_source_list) {
  return std::find_if(
      audio_source_list->begin(), audio_source_list->end(),
      [audio_source](const std::unique_ptr<AudioMixerImpl::SourceStatus>& p) {
        return p->audio_source == audio_source;
      });
}

// Rounds the maximal audio source frequency up to an APM-native
// frequency.
int CalculateMixingFrequency(
    const AudioMixerImpl::SourceStatusList& audio_source_list) {
  if (audio_source_list.empty()) {
    return AudioMixerImpl::kDefaultFrequency;
  }
  using NativeRate = AudioProcessing::NativeRate;
  int maximal_frequency = 0;
  for (const auto& source_status : audio_source_list) {
    const int source_needed_frequency =
        source_status->audio_source->PreferredSampleRate();
    RTC_DCHECK_LE(NativeRate::kSampleRate8kHz, source_needed_frequency);
    RTC_DCHECK_LE(source_needed_frequency, NativeRate::kSampleRate48kHz);
    maximal_frequency = std::max(maximal_frequency, source_needed_frequency);
  }

  static constexpr NativeRate native_rates[] = {
      NativeRate::kSampleRate8kHz, NativeRate::kSampleRate16kHz,
      NativeRate::kSampleRate32kHz, NativeRate::kSampleRate48kHz};
  const auto rounded_up_index = std::lower_bound(
      std::begin(native_rates), std::end(native_rates), maximal_frequency);
  RTC_DCHECK(rounded_up_index != std::end(native_rates));
  return *rounded_up_index;
}

}  // namespace

AudioMixerImpl::AudioMixerImpl(std::unique_ptr<AudioProcessing> limiter)
    : audio_source_list_(),
      use_limiter_(true),
      time_stamp_(0),
      limiter_(std::move(limiter)) {
  SetOutputFrequency(kDefaultFrequency);
}

AudioMixerImpl::~AudioMixerImpl() {}

rtc::scoped_refptr<AudioMixerImpl> AudioMixerImpl::Create() {
  Config config;
  config.Set<ExperimentalAgc>(new ExperimentalAgc(false));
  std::unique_ptr<AudioProcessing> limiter(AudioProcessing::Create(config));
  if (!limiter.get()) {
    return nullptr;
  }

  if (limiter->gain_control()->set_mode(GainControl::kFixedDigital) !=
      limiter->kNoError) {
    return nullptr;
  }

  // We smoothly limit the mixed frame to -7 dbFS. -6 would correspond to the
  // divide-by-2 but -7 is used instead to give a bit of headroom since the
  // AGC is not a hard limiter.
  if (limiter->gain_control()->set_target_level_dbfs(7) != limiter->kNoError) {
    return nullptr;
  }

  if (limiter->gain_control()->set_compression_gain_db(0) !=
      limiter->kNoError) {
    return nullptr;
  }

  if (limiter->gain_control()->enable_limiter(true) != limiter->kNoError) {
    return nullptr;
  }

  if (limiter->gain_control()->Enable(true) != limiter->kNoError) {
    return nullptr;
  }

  return rtc::scoped_refptr<AudioMixerImpl>(
      new rtc::RefCountedObject<AudioMixerImpl>(std::move(limiter)));
}

void AudioMixerImpl::Mix(size_t number_of_channels,
                         AudioFrame* audio_frame_for_mixing) {
  RTC_DCHECK(number_of_channels == 1 || number_of_channels == 2);
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  const int sample_rate = [&]() {
    rtc::CritScope lock(&crit_);
    return CalculateMixingFrequency(audio_source_list_);
  }();

  if (OutputFrequency() != sample_rate) {
    SetOutputFrequency(sample_rate);
  }

  AudioFrameList mix_list;
  {
    rtc::CritScope lock(&crit_);
    mix_list = GetAudioFromSources();

    for (const auto& frame : mix_list) {
      RemixFrame(number_of_channels, frame);
    }

    audio_frame_for_mixing->UpdateFrame(
        -1, time_stamp_, NULL, 0, OutputFrequency(), AudioFrame::kNormalSpeech,
        AudioFrame::kVadPassive, number_of_channels);

    time_stamp_ += static_cast<uint32_t>(sample_size_);

    use_limiter_ = mix_list.size() > 1;

    // We only use the limiter if we're actually mixing multiple streams.
    MixFromList(audio_frame_for_mixing, mix_list, use_limiter_);
  }

  if (audio_frame_for_mixing->samples_per_channel_ == 0) {
    // Nothing was mixed, set the audio samples to silence.
    audio_frame_for_mixing->samples_per_channel_ = sample_size_;
    audio_frame_for_mixing->Mute();
  } else {
    // Only call the limiter if we have something to mix.
    LimitMixedAudio(audio_frame_for_mixing);
  }

  return;
}

void AudioMixerImpl::SetOutputFrequency(int frequency) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  output_frequency_ = frequency;
  sample_size_ = (output_frequency_ * kFrameDurationInMs) / 1000;
}

int AudioMixerImpl::OutputFrequency() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  return output_frequency_;
}

bool AudioMixerImpl::AddSource(Source* audio_source) {
  RTC_DCHECK(audio_source);
  rtc::CritScope lock(&crit_);
  RTC_DCHECK(FindSourceInList(audio_source, &audio_source_list_) ==
             audio_source_list_.end())
      << "Source already added to mixer";
  audio_source_list_.emplace_back(new SourceStatus(audio_source, false, 0));
  return true;
}

bool AudioMixerImpl::RemoveSource(Source* audio_source) {
  RTC_DCHECK(audio_source);
  rtc::CritScope lock(&crit_);
  const auto iter = FindSourceInList(audio_source, &audio_source_list_);
  RTC_DCHECK(iter != audio_source_list_.end()) << "Source not present in mixer";
  audio_source_list_.erase(iter);
  return true;
}

AudioFrameList AudioMixerImpl::GetAudioFromSources() {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  AudioFrameList result;
  std::vector<SourceFrame> audio_source_mixing_data_list;
  std::vector<SourceFrame> ramp_list;

  // Get audio from the audio sources and put it in the SourceFrame vector.
  for (auto& source_and_status : audio_source_list_) {
    const auto audio_frame_info =
        source_and_status->audio_source->GetAudioFrameWithInfo(
            OutputFrequency(), &source_and_status->audio_frame);

    if (audio_frame_info == Source::AudioFrameInfo::kError) {
      LOG_F(LS_WARNING) << "failed to GetAudioFrameWithInfo() from source";
      continue;
    }
    audio_source_mixing_data_list.emplace_back(
        source_and_status.get(), &source_and_status->audio_frame,
        audio_frame_info == Source::AudioFrameInfo::kMuted);
  }

  // Sort frames by sorting function.
  std::sort(audio_source_mixing_data_list.begin(),
            audio_source_mixing_data_list.end(), ShouldMixBefore);

  int max_audio_frame_counter = kMaximumAmountOfMixedAudioSources;

  // Go through list in order and put unmuted frames in result list.
  for (const auto& p : audio_source_mixing_data_list) {
    // Filter muted.
    if (p.muted) {
      p.source_status->is_mixed = false;
      continue;
    }

    // Add frame to result vector for mixing.
    bool is_mixed = false;
    if (max_audio_frame_counter > 0) {
      --max_audio_frame_counter;
      result.push_back(p.audio_frame);
      ramp_list.emplace_back(p.source_status, p.audio_frame, false, -1);
      is_mixed = true;
    }
    p.source_status->is_mixed = is_mixed;
  }
  RampAndUpdateGain(ramp_list);
  return result;
}


bool AudioMixerImpl::LimitMixedAudio(AudioFrame* mixed_audio) const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  if (!use_limiter_) {
    return true;
  }

  // Smoothly limit the mixed frame.
  const int error = limiter_->ProcessStream(mixed_audio);

  // And now we can safely restore the level. This procedure results in
  // some loss of resolution, deemed acceptable.
  //
  // It's possible to apply the gain in the AGC (with a target level of 0 dbFS
  // and compression gain of 6 dB). However, in the transition frame when this
  // is enabled (moving from one to two audio sources) it has the potential to
  // create discontinuities in the mixed frame.
  //
  // Instead we double the frame (with addition since left-shifting a
  // negative value is undefined).
  *mixed_audio += *mixed_audio;

  if (error != limiter_->kNoError) {
    LOG_F(LS_ERROR) << "Error from AudioProcessing: " << error;
    RTC_NOTREACHED();
    return false;
  }
  return true;
}

bool AudioMixerImpl::GetAudioSourceMixabilityStatusForTest(
    AudioMixerImpl::Source* audio_source) const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  rtc::CritScope lock(&crit_);

  const auto iter = FindSourceInList(audio_source, &audio_source_list_);
  if (iter != audio_source_list_.end()) {
    return (*iter)->is_mixed;
  }

  LOG(LS_ERROR) << "Audio source unknown";
  return false;
}
}  // namespace webrtc
