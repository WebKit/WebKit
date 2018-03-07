/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/mediaconstraintsinterface.h"

#include "api/peerconnectioninterface.h"
#include "rtc_base/stringencode.h"

namespace {

// Find the highest-priority instance of the T-valued constraint named by
// |key| and return its value as |value|. |constraints| can be null.
// If |mandatory_constraints| is non-null, it is incremented if the key appears
// among the mandatory constraints.
// Returns true if the key was found and has a valid value for type T.
// If the key appears multiple times as an optional constraint, appearances
// after the first are ignored.
// Note: Because this uses FindFirst, repeated optional constraints whose
// first instance has an unrecognized value are not handled precisely in
// accordance with the specification.
template <typename T>
bool FindConstraint(const webrtc::MediaConstraintsInterface* constraints,
                    const std::string& key,
                    T* value,
                    size_t* mandatory_constraints) {
  std::string string_value;
  if (!FindConstraint(constraints, key, &string_value, mandatory_constraints)) {
    return false;
  }
  return rtc::FromString(string_value, value);
}

// Specialization for std::string, since a string doesn't need conversion.
template <>
bool FindConstraint(const webrtc::MediaConstraintsInterface* constraints,
                    const std::string& key,
                    std::string* value,
                    size_t* mandatory_constraints) {
  if (!constraints) {
    return false;
  }
  if (constraints->GetMandatory().FindFirst(key, value)) {
    if (mandatory_constraints) {
      ++*mandatory_constraints;
    }
    return true;
  }
  if (constraints->GetOptional().FindFirst(key, value)) {
    return true;
  }
  return false;
}

// Converts a constraint (mandatory takes precedence over optional) to an
// rtc::Optional.
template <typename T>
void ConstraintToOptional(const webrtc::MediaConstraintsInterface* constraints,
                          const std::string& key,
                          rtc::Optional<T>* value_out) {
  T value;
  bool present = FindConstraint<T>(constraints, key, &value, nullptr);
  if (present) {
    *value_out = value;
  }
}
}  // namespace

namespace webrtc {

const char MediaConstraintsInterface::kValueTrue[] = "true";
const char MediaConstraintsInterface::kValueFalse[] = "false";

// Constraints declared as static members in mediastreaminterface.h
// Specified by draft-alvestrand-constraints-resolution-00b
const char MediaConstraintsInterface::kMinAspectRatio[] = "minAspectRatio";
const char MediaConstraintsInterface::kMaxAspectRatio[] = "maxAspectRatio";
const char MediaConstraintsInterface::kMaxWidth[] = "maxWidth";
const char MediaConstraintsInterface::kMinWidth[] = "minWidth";
const char MediaConstraintsInterface::kMaxHeight[] = "maxHeight";
const char MediaConstraintsInterface::kMinHeight[] = "minHeight";
const char MediaConstraintsInterface::kMaxFrameRate[] = "maxFrameRate";
const char MediaConstraintsInterface::kMinFrameRate[] = "minFrameRate";

// Audio constraints.
const char MediaConstraintsInterface::kEchoCancellation[] =
    "echoCancellation";
const char MediaConstraintsInterface::kGoogEchoCancellation[] =
    "googEchoCancellation";
const char MediaConstraintsInterface::kExtendedFilterEchoCancellation[] =
    "googEchoCancellation2";
const char MediaConstraintsInterface::kDAEchoCancellation[] =
    "googDAEchoCancellation";
const char MediaConstraintsInterface::kAutoGainControl[] =
    "googAutoGainControl";
const char MediaConstraintsInterface::kExperimentalAutoGainControl[] =
    "googAutoGainControl2";
const char MediaConstraintsInterface::kNoiseSuppression[] =
    "googNoiseSuppression";
const char MediaConstraintsInterface::kExperimentalNoiseSuppression[] =
    "googNoiseSuppression2";
const char MediaConstraintsInterface::kIntelligibilityEnhancer[] =
    "intelligibilityEnhancer";
const char MediaConstraintsInterface::kLevelControl[] = "levelControl";
const char MediaConstraintsInterface::kLevelControlInitialPeakLevelDBFS[] =
    "levelControlInitialPeakLevelDBFS";
const char MediaConstraintsInterface::kHighpassFilter[] =
    "googHighpassFilter";
const char MediaConstraintsInterface::kTypingNoiseDetection[] =
    "googTypingNoiseDetection";
const char MediaConstraintsInterface::kAudioMirroring[] = "googAudioMirroring";
const char MediaConstraintsInterface::kAudioNetworkAdaptorConfig[] =
    "googAudioNetworkAdaptorConfig";

// Google-specific constraint keys for a local video source (getUserMedia).
const char MediaConstraintsInterface::kNoiseReduction[] = "googNoiseReduction";

// Constraint keys for CreateOffer / CreateAnswer defined in W3C specification.
const char MediaConstraintsInterface::kOfferToReceiveAudio[] =
    "OfferToReceiveAudio";
const char MediaConstraintsInterface::kOfferToReceiveVideo[] =
    "OfferToReceiveVideo";
const char MediaConstraintsInterface::kVoiceActivityDetection[] =
    "VoiceActivityDetection";
const char MediaConstraintsInterface::kIceRestart[] =
    "IceRestart";
// Google specific constraint for BUNDLE enable/disable.
const char MediaConstraintsInterface::kUseRtpMux[] =
    "googUseRtpMUX";

// Below constraints should be used during PeerConnection construction.
const char MediaConstraintsInterface::kEnableDtlsSrtp[] =
    "DtlsSrtpKeyAgreement";
const char MediaConstraintsInterface::kEnableRtpDataChannels[] =
    "RtpDataChannels";
// Google-specific constraint keys.
const char MediaConstraintsInterface::kEnableDscp[] = "googDscp";
const char MediaConstraintsInterface::kEnableIPv6[] = "googIPv6";
const char MediaConstraintsInterface::kEnableVideoSuspendBelowMinBitrate[] =
    "googSuspendBelowMinBitrate";
const char MediaConstraintsInterface::kCombinedAudioVideoBwe[] =
    "googCombinedAudioVideoBwe";
const char MediaConstraintsInterface::kScreencastMinBitrate[] =
    "googScreencastMinBitrate";
// TODO(ronghuawu): Remove once cpu overuse detection is stable.
const char MediaConstraintsInterface::kCpuOveruseDetection[] =
    "googCpuOveruseDetection";
const char MediaConstraintsInterface::kPayloadPadding[] = "googPayloadPadding";


// Set |value| to the value associated with the first appearance of |key|, or
// return false if |key| is not found.
bool MediaConstraintsInterface::Constraints::FindFirst(
    const std::string& key, std::string* value) const {
  for (Constraints::const_iterator iter = begin(); iter != end(); ++iter) {
    if (iter->key == key) {
      *value = iter->value;
      return true;
    }
  }
  return false;
}

bool FindConstraint(const MediaConstraintsInterface* constraints,
                    const std::string& key, bool* value,
                    size_t* mandatory_constraints) {
  return ::FindConstraint<bool>(constraints, key, value, mandatory_constraints);
}

bool FindConstraint(const MediaConstraintsInterface* constraints,
                    const std::string& key,
                    int* value,
                    size_t* mandatory_constraints) {
  return ::FindConstraint<int>(constraints, key, value, mandatory_constraints);
}

void CopyConstraintsIntoRtcConfiguration(
    const MediaConstraintsInterface* constraints,
    PeerConnectionInterface::RTCConfiguration* configuration) {
  // Copy info from constraints into configuration, if present.
  if (!constraints) {
    return;
  }

  bool enable_ipv6;
  if (FindConstraint(constraints, MediaConstraintsInterface::kEnableIPv6,
                     &enable_ipv6, nullptr)) {
    configuration->disable_ipv6 = !enable_ipv6;
  }
  FindConstraint(constraints, MediaConstraintsInterface::kEnableDscp,
                 &configuration->media_config.enable_dscp, nullptr);
  FindConstraint(
      constraints, MediaConstraintsInterface::kCpuOveruseDetection,
      &configuration->media_config.video.enable_cpu_overuse_detection, nullptr);
  FindConstraint(constraints, MediaConstraintsInterface::kEnableRtpDataChannels,
                 &configuration->enable_rtp_data_channel, nullptr);
  // Find Suspend Below Min Bitrate constraint.
  FindConstraint(constraints,
                 MediaConstraintsInterface::kEnableVideoSuspendBelowMinBitrate,
                 &configuration->media_config.video.suspend_below_min_bitrate,
                 nullptr);
  ConstraintToOptional<int>(constraints,
                            MediaConstraintsInterface::kScreencastMinBitrate,
                            &configuration->screencast_min_bitrate);
  ConstraintToOptional<bool>(constraints,
                             MediaConstraintsInterface::kCombinedAudioVideoBwe,
                             &configuration->combined_audio_video_bwe);
  ConstraintToOptional<bool>(constraints,
                             MediaConstraintsInterface::kEnableDtlsSrtp,
                             &configuration->enable_dtls_srtp);
}

void CopyConstraintsIntoAudioOptions(
    const MediaConstraintsInterface* constraints,
    cricket::AudioOptions* options) {
  if (!constraints) {
    return;
  }

  ConstraintToOptional<bool>(constraints,
                             MediaConstraintsInterface::kGoogEchoCancellation,
                             &options->echo_cancellation);
  ConstraintToOptional<bool>(
      constraints, MediaConstraintsInterface::kExtendedFilterEchoCancellation,
      &options->extended_filter_aec);
  ConstraintToOptional<bool>(constraints,
                             MediaConstraintsInterface::kDAEchoCancellation,
                             &options->delay_agnostic_aec);
  ConstraintToOptional<bool>(constraints,
                             MediaConstraintsInterface::kAutoGainControl,
                             &options->auto_gain_control);
  ConstraintToOptional<bool>(
      constraints, MediaConstraintsInterface::kExperimentalAutoGainControl,
      &options->experimental_agc);
  ConstraintToOptional<bool>(constraints,
                             MediaConstraintsInterface::kNoiseSuppression,
                             &options->noise_suppression);
  ConstraintToOptional<bool>(
      constraints, MediaConstraintsInterface::kExperimentalNoiseSuppression,
      &options->experimental_ns);
  ConstraintToOptional<bool>(
      constraints, MediaConstraintsInterface::kIntelligibilityEnhancer,
      &options->intelligibility_enhancer);
  ConstraintToOptional<bool>(constraints,
                             MediaConstraintsInterface::kLevelControl,
                             &options->level_control);
  ConstraintToOptional<bool>(constraints,
                             MediaConstraintsInterface::kHighpassFilter,
                             &options->highpass_filter);
  ConstraintToOptional<bool>(constraints,
                             MediaConstraintsInterface::kTypingNoiseDetection,
                             &options->typing_detection);
  ConstraintToOptional<bool>(constraints,
                             MediaConstraintsInterface::kAudioMirroring,
                             &options->stereo_swapping);
  ConstraintToOptional<float>(
      constraints, MediaConstraintsInterface::kLevelControlInitialPeakLevelDBFS,
      &options->level_control_initial_peak_level_dbfs);
  ConstraintToOptional<std::string>(
      constraints, MediaConstraintsInterface::kAudioNetworkAdaptorConfig,
      &options->audio_network_adaptor_config);
  // When |kAudioNetworkAdaptorConfig| is defined, it both means that audio
  // network adaptor is desired, and provides the config string.
  if (options->audio_network_adaptor_config) {
    options->audio_network_adaptor = true;
  }
}

}  // namespace webrtc
