/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Noise Suppression (NS).
//  - Automatic Gain Control (AGC).
//  - Echo Control (EC).
//  - Receiving side VAD, NS and AGC.
//  - Measurements of instantaneous speech, noise and echo levels.
//  - Generation of AP debug recordings.
//  - Detection of keyboard typing which can disrupt a voice conversation.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface();
//  VoEAudioProcessing* ap = VoEAudioProcessing::GetInterface(voe);
//  base->Init();
//  ap->SetEcStatus(true, kAgcAdaptiveAnalog);
//  ...
//  base->Terminate();
//  base->Release();
//  ap->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_AUDIO_PROCESSING_H
#define WEBRTC_VOICE_ENGINE_VOE_AUDIO_PROCESSING_H

#include <stdio.h>

#include "webrtc/common_types.h"

namespace webrtc {

class VoiceEngine;

// VoEAudioProcessing
class WEBRTC_DLLEXPORT VoEAudioProcessing {
 public:
  // Factory for the VoEAudioProcessing sub-API. Increases an internal
  // reference counter if successful. Returns NULL if the API is not
  // supported or if construction fails.
  static VoEAudioProcessing* GetInterface(VoiceEngine* voiceEngine);

  // Releases the VoEAudioProcessing sub-API and decreases an internal
  // reference counter. Returns the new reference count. This value should
  // be zero for all sub-API:s before the VoiceEngine object can be safely
  // deleted.
  virtual int Release() = 0;

  // Sets Noise Suppression (NS) status and mode.
  // The NS reduces noise in the microphone signal.
  virtual int SetNsStatus(bool enable, NsModes mode = kNsUnchanged) = 0;

  // Gets the NS status and mode.
  virtual int GetNsStatus(bool& enabled, NsModes& mode) = 0;

  // Sets the Automatic Gain Control (AGC) status and mode.
  // The AGC adjusts the microphone signal to an appropriate level.
  virtual int SetAgcStatus(bool enable, AgcModes mode = kAgcUnchanged) = 0;

  // Gets the AGC status and mode.
  virtual int GetAgcStatus(bool& enabled, AgcModes& mode) = 0;

  // Sets the AGC configuration.
  // Should only be used in situations where the working environment
  // is well known.
  virtual int SetAgcConfig(AgcConfig config) = 0;

  // Gets the AGC configuration.
  virtual int GetAgcConfig(AgcConfig& config) = 0;

  // Sets the Echo Control (EC) status and mode.
  // The EC mitigates acoustic echo where a user can hear their own
  // speech repeated back due to an acoustic coupling between the
  // speaker and the microphone at the remote end.
  virtual int SetEcStatus(bool enable, EcModes mode = kEcUnchanged) = 0;

  // Gets the EC status and mode.
  virtual int GetEcStatus(bool& enabled, EcModes& mode) = 0;

  // Enables the compensation of clock drift between the capture and render
  // streams by the echo canceller (i.e. only using EcMode==kEcAec). It will
  // only be enabled if supported on the current platform; otherwise an error
  // will be returned. Check if the platform is supported by calling
  // |DriftCompensationSupported()|.
  virtual int EnableDriftCompensation(bool enable) = 0;
  virtual bool DriftCompensationEnabled() = 0;
  static bool DriftCompensationSupported();

  // Sets a delay |offset| in ms to add to the system delay reported by the
  // OS, which is used by the AEC to synchronize far- and near-end streams.
  // In some cases a system may introduce a delay which goes unreported by the
  // OS, but which is known to the user. This method can be used to compensate
  // for the unreported delay.
  virtual void SetDelayOffsetMs(int offset) = 0;
  virtual int DelayOffsetMs() = 0;

  // Modifies settings for the AEC designed for mobile devices (AECM).
  virtual int SetAecmMode(AecmModes mode = kAecmSpeakerphone,
                          bool enableCNG = true) = 0;

  // Gets settings for the AECM.
  virtual int GetAecmMode(AecmModes& mode, bool& enabledCNG) = 0;

  // Enables a high pass filter on the capture signal. This removes DC bias
  // and low-frequency noise. Recommended to be enabled.
  virtual int EnableHighPassFilter(bool enable) = 0;
  virtual bool IsHighPassFilterEnabled() = 0;

  // Gets the VAD/DTX activity for the specified |channel|.
  // The returned value is 1 if frames of audio contains speech
  // and 0 if silence. The output is always 1 if VAD is disabled.
  virtual int VoiceActivityIndicator(int channel) = 0;

  // Enables or disables the possibility to retrieve echo metrics and delay
  // logging values during an active call. The metrics are only supported in
  // AEC.
  virtual int SetEcMetricsStatus(bool enable) = 0;

  // Gets the current EC metric status.
  virtual int GetEcMetricsStatus(bool& enabled) = 0;

  // Gets the instantaneous echo level metrics.
  virtual int GetEchoMetrics(int& ERL, int& ERLE, int& RERL, int& A_NLP) = 0;

  // Gets the EC internal |delay_median| and |delay_std| in ms between
  // near-end and far-end. The metric |fraction_poor_delays| is the amount of
  // delay values that potentially can break the EC. The values are aggregated
  // over one second and the last updated metrics are returned.
  virtual int GetEcDelayMetrics(int& delay_median,
                                int& delay_std,
                                float& fraction_poor_delays) = 0;

  // Enables recording of Audio Processing (AP) debugging information.
  // The file can later be used for off-line analysis of the AP performance.
  virtual int StartDebugRecording(const char* fileNameUTF8) = 0;

  // Same as above but sets and uses an existing file handle. Takes ownership
  // of |file_handle| and passes it on to the audio processing module.
  virtual int StartDebugRecording(FILE* file_handle) = 0;

  // Disables recording of AP debugging information.
  virtual int StopDebugRecording() = 0;

  // Enables or disables detection of disturbing keyboard typing.
  // An error notification will be given as a callback upon detection.
  virtual int SetTypingDetectionStatus(bool enable) = 0;

  // Gets the current typing detection status.
  virtual int GetTypingDetectionStatus(bool& enabled) = 0;

  // Reports the lower of:
  // * Time in seconds since the last typing event.
  // * Time in seconds since the typing detection was enabled.
  // Returns error if typing detection is disabled.
  virtual int TimeSinceLastTyping(int& seconds) = 0;

  // Optional setting of typing detection parameters
  // Parameter with value == 0 will be ignored
  // and left with default config.
  // TODO(niklase) Remove default argument as soon as libJingle is updated!
  virtual int SetTypingDetectionParameters(int timeWindow,
                                           int costPerTyping,
                                           int reportingThreshold,
                                           int penaltyDecay,
                                           int typeEventDelay = 0) = 0;

  // Swaps the capture-side left and right audio channels when enabled. It
  // only has an effect when using a stereo send codec. The setting is
  // persistent; it will be applied whenever a stereo send codec is enabled.
  //
  // The swap is applied only to the captured audio, and not mixed files. The
  // swap will appear in file recordings and when accessing audio through the
  // external media interface.
  virtual void EnableStereoChannelSwapping(bool enable) = 0;
  virtual bool IsStereoChannelSwappingEnabled() = 0;

 protected:
  VoEAudioProcessing() {}
  virtual ~VoEAudioProcessing() {}
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_AUDIO_PROCESSING_H
