/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_AUDIO_PROCESSING_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_AUDIO_PROCESSING_IMPL_H

#include "webrtc/voice_engine/include/voe_audio_processing.h"

#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoEAudioProcessingImpl : public VoEAudioProcessing {
 public:
  int SetNsStatus(bool enable, NsModes mode = kNsUnchanged) override;

  int GetNsStatus(bool& enabled, NsModes& mode) override;

  int SetAgcStatus(bool enable, AgcModes mode = kAgcUnchanged) override;

  int GetAgcStatus(bool& enabled, AgcModes& mode) override;

  int SetAgcConfig(AgcConfig config) override;

  int GetAgcConfig(AgcConfig& config) override;

  int SetEcStatus(bool enable, EcModes mode = kEcUnchanged) override;
  int GetEcStatus(bool& enabled, EcModes& mode) override;
  int EnableDriftCompensation(bool enable) override;
  bool DriftCompensationEnabled() override;

  void SetDelayOffsetMs(int offset) override;
  int DelayOffsetMs() override;

  int SetAecmMode(AecmModes mode = kAecmSpeakerphone,
                  bool enableCNG = true) override;

  int GetAecmMode(AecmModes& mode, bool& enabledCNG) override;

  int EnableHighPassFilter(bool enable) override;
  bool IsHighPassFilterEnabled() override;

  int VoiceActivityIndicator(int channel) override;

  int SetEcMetricsStatus(bool enable) override;

  int GetEcMetricsStatus(bool& enabled) override;

  int GetEchoMetrics(int& ERL, int& ERLE, int& RERL, int& A_NLP) override;

  int GetEcDelayMetrics(int& delay_median,
                        int& delay_std,
                        float& fraction_poor_delays) override;

  int StartDebugRecording(const char* fileNameUTF8) override;
  int StartDebugRecording(FILE* file_handle) override;

  int StopDebugRecording() override;

  int SetTypingDetectionStatus(bool enable) override;

  int GetTypingDetectionStatus(bool& enabled) override;

  int TimeSinceLastTyping(int& seconds) override;

  // TODO(niklase) Remove default argument as soon as libJingle is updated!
  int SetTypingDetectionParameters(int timeWindow,
                                   int costPerTyping,
                                   int reportingThreshold,
                                   int penaltyDecay,
                                   int typeEventDelay = 0) override;

  void EnableStereoChannelSwapping(bool enable) override;
  bool IsStereoChannelSwappingEnabled() override;

 protected:
  VoEAudioProcessingImpl(voe::SharedData* shared);
  ~VoEAudioProcessingImpl() override;

 private:
  bool _isAecMode;
  voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_AUDIO_PROCESSING_IMPL_H
