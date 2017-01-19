/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/localaudiosource.h"

#include <vector>

#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/media/base/mediaengine.h"

using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;

namespace webrtc {

namespace {

// Convert constraints to audio options. Return false if constraints are
// invalid.
void FromConstraints(const MediaConstraintsInterface::Constraints& constraints,
                     cricket::AudioOptions* options) {
  // This design relies on the fact that all the audio constraints are actually
  // "options", i.e. boolean-valued and always satisfiable.  If the constraints
  // are extended to include non-boolean values or actual format constraints,
  // a different algorithm will be required.
  struct {
    const char* name;
    rtc::Optional<bool>& value;
  } key_to_value[] = {
      {MediaConstraintsInterface::kGoogEchoCancellation,
       options->echo_cancellation},
      {MediaConstraintsInterface::kExtendedFilterEchoCancellation,
       options->extended_filter_aec},
      {MediaConstraintsInterface::kDAEchoCancellation,
       options->delay_agnostic_aec},
      {MediaConstraintsInterface::kAutoGainControl, options->auto_gain_control},
      {MediaConstraintsInterface::kExperimentalAutoGainControl,
       options->experimental_agc},
      {MediaConstraintsInterface::kNoiseSuppression,
       options->noise_suppression},
      {MediaConstraintsInterface::kExperimentalNoiseSuppression,
       options->experimental_ns},
      {MediaConstraintsInterface::kIntelligibilityEnhancer,
       options->intelligibility_enhancer},
      {MediaConstraintsInterface::kLevelControl, options->level_control},
      {MediaConstraintsInterface::kHighpassFilter, options->highpass_filter},
      {MediaConstraintsInterface::kTypingNoiseDetection,
       options->typing_detection},
      {MediaConstraintsInterface::kAudioMirroring, options->stereo_swapping}
  };

  for (const auto& constraint : constraints) {
    bool value = false;
    if (!rtc::FromString(constraint.value, &value))
      continue;

    for (auto& entry : key_to_value) {
      if (constraint.key.compare(entry.name) == 0)
        entry.value = rtc::Optional<bool>(value);
    }
  }

  // Set non-boolean constraints.
  std::string value;
  if (constraints.FindFirst(
          MediaConstraintsInterface::kLevelControlInitialPeakLevelDBFS,
          &value)) {
    float level_control_initial_peak_level_dbfs;
    if (rtc::FromString(value, &level_control_initial_peak_level_dbfs)) {
      options->level_control_initial_peak_level_dbfs =
          rtc::Optional<float>(level_control_initial_peak_level_dbfs);
    }
  }
}

}  // namespace

rtc::scoped_refptr<LocalAudioSource> LocalAudioSource::Create(
    const PeerConnectionFactoryInterface::Options& options,
    const MediaConstraintsInterface* constraints) {
  rtc::scoped_refptr<LocalAudioSource> source(
      new rtc::RefCountedObject<LocalAudioSource>());
  source->Initialize(options, constraints);
  return source;
}

rtc::scoped_refptr<LocalAudioSource> LocalAudioSource::Create(
    const PeerConnectionFactoryInterface::Options& options,
    const cricket::AudioOptions* audio_options) {
  rtc::scoped_refptr<LocalAudioSource> source(
      new rtc::RefCountedObject<LocalAudioSource>());
  source->Initialize(options, audio_options);
  return source;
}

void LocalAudioSource::Initialize(
    const PeerConnectionFactoryInterface::Options& options,
    const MediaConstraintsInterface* constraints) {
  if (!constraints)
    return;

  // Apply optional constraints first, they will be overwritten by mandatory
  // constraints.
  FromConstraints(constraints->GetOptional(), &options_);

  cricket::AudioOptions mandatory_options;
  FromConstraints(constraints->GetMandatory(), &mandatory_options);
  options_.SetAll(mandatory_options);
}

void LocalAudioSource::Initialize(
    const PeerConnectionFactoryInterface::Options& options,
    const cricket::AudioOptions* audio_options) {
  if (!audio_options)
    return;

  options_ = *audio_options;
}

}  // namespace webrtc
