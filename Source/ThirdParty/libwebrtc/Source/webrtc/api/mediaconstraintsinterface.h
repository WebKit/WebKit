/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the interface for MediaConstraints, corresponding to
// the definition at
// http://www.w3.org/TR/mediacapture-streams/#mediastreamconstraints and also
// used in WebRTC: http://dev.w3.org/2011/webrtc/editor/webrtc.html#constraints.

// This interface is being deprecated in Chrome, and may be removed
// from WebRTC too.
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5617

#ifndef WEBRTC_API_MEDIACONSTRAINTSINTERFACE_H_
#define WEBRTC_API_MEDIACONSTRAINTSINTERFACE_H_

#include <string>
#include <vector>

#include "webrtc/base/optional.h"
#include "webrtc/api/peerconnectioninterface.h"

namespace webrtc {

// Interface used for passing arguments about media constraints
// to the MediaStream and PeerConnection implementation.
//
// Constraints may be either "mandatory", which means that unless satisfied,
// the method taking the constraints should fail, or "optional", which means
// they may not be satisfied..
class MediaConstraintsInterface {
 public:
  struct Constraint {
    Constraint() {}
    Constraint(const std::string& key, const std::string value)
        : key(key), value(value) {
    }
    std::string key;
    std::string value;
  };

  class Constraints : public std::vector<Constraint> {
   public:
    bool FindFirst(const std::string& key, std::string* value) const;
  };

  virtual const Constraints& GetMandatory() const = 0;
  virtual const Constraints& GetOptional() const = 0;

  // Constraint keys used by a local video source.
  // Specified by draft-alvestrand-constraints-resolution-00b
  static const char kMinAspectRatio[];  // minAspectRatio
  static const char kMaxAspectRatio[];  // maxAspectRatio
  static const char kMaxWidth[];  // maxWidth
  static const char kMinWidth[];  // minWidth
  static const char kMaxHeight[];  // maxHeight
  static const char kMinHeight[];  // minHeight
  static const char kMaxFrameRate[];  // maxFrameRate
  static const char kMinFrameRate[];  // minFrameRate

  // Constraint keys used by a local audio source.
  static const char kEchoCancellation[];  // echoCancellation

  // These keys are google specific.
  static const char kGoogEchoCancellation[];  // googEchoCancellation

  static const char kExtendedFilterEchoCancellation[];  // googEchoCancellation2
  static const char kDAEchoCancellation[];  // googDAEchoCancellation
  static const char kAutoGainControl[];  // googAutoGainControl
  static const char kExperimentalAutoGainControl[];  // googAutoGainControl2
  static const char kNoiseSuppression[];  // googNoiseSuppression
  static const char kExperimentalNoiseSuppression[];  // googNoiseSuppression2
  static const char kIntelligibilityEnhancer[];  // intelligibilityEnhancer
  static const char kLevelControl[];             // levelControl
  static const char
      kLevelControlInitialPeakLevelDBFS[];  // levelControlInitialPeakLevelDBFS
  static const char kHighpassFilter[];  // googHighpassFilter
  static const char kTypingNoiseDetection[];  // googTypingNoiseDetection
  static const char kAudioMirroring[];  // googAudioMirroring
  static const char
      kAudioNetworkAdaptorConfig[];  // goodAudioNetworkAdaptorConfig

  // Google-specific constraint keys for a local video source
  static const char kNoiseReduction[];  // googNoiseReduction

  // Constraint keys for CreateOffer / CreateAnswer
  // Specified by the W3C PeerConnection spec
  static const char kOfferToReceiveVideo[];  // OfferToReceiveVideo
  static const char kOfferToReceiveAudio[];  // OfferToReceiveAudio
  static const char kVoiceActivityDetection[];  // VoiceActivityDetection
  static const char kIceRestart[];  // IceRestart
  // These keys are google specific.
  static const char kUseRtpMux[];  // googUseRtpMUX

  // Constraints values.
  static const char kValueTrue[];  // true
  static const char kValueFalse[];  // false

  // PeerConnection constraint keys.
  // Temporary pseudo-constraints used to enable DTLS-SRTP
  static const char kEnableDtlsSrtp[];  // Enable DTLS-SRTP
  // Temporary pseudo-constraints used to enable DataChannels
  static const char kEnableRtpDataChannels[];  // Enable RTP DataChannels
  // Google-specific constraint keys.
  // Temporary pseudo-constraint for enabling DSCP through JS.
  static const char kEnableDscp[];  // googDscp
  // Constraint to enable IPv6 through JS.
  static const char kEnableIPv6[];  // googIPv6
  // Temporary constraint to enable suspend below min bitrate feature.
  static const char kEnableVideoSuspendBelowMinBitrate[];
      // googSuspendBelowMinBitrate
  // Constraint to enable combined audio+video bandwidth estimation.
  static const char kCombinedAudioVideoBwe[];  // googCombinedAudioVideoBwe
  static const char kScreencastMinBitrate[];  // googScreencastMinBitrate
  static const char kCpuOveruseDetection[];  // googCpuOveruseDetection
  static const char kPayloadPadding[];  // googPayloadPadding

  // The prefix of internal-only constraints whose JS set values should be
  // stripped by Chrome before passed down to Libjingle.
  static const char kInternalConstraintPrefix[];

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface
  virtual ~MediaConstraintsInterface() {}
};

bool FindConstraint(const MediaConstraintsInterface* constraints,
                    const std::string& key, bool* value,
                    size_t* mandatory_constraints);

bool FindConstraint(const MediaConstraintsInterface* constraints,
                    const std::string& key,
                    int* value,
                    size_t* mandatory_constraints);

// Copy all relevant constraints into an RTCConfiguration object.
void CopyConstraintsIntoRtcConfiguration(
    const MediaConstraintsInterface* constraints,
    PeerConnectionInterface::RTCConfiguration* configuration);

// Copy all relevant constraints into an AudioOptions object.
void CopyConstraintsIntoAudioOptions(
    const MediaConstraintsInterface* constraints,
    cricket::AudioOptions* options);

}  // namespace webrtc

#endif  // WEBRTC_API_MEDIACONSTRAINTSINTERFACE_H_
