/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VOICE_ENGINE_TRANSMIT_MIXER_H_
#define VOICE_ENGINE_TRANSMIT_MIXER_H_

#include <memory>

#include "common_audio/resampler/include/push_resampler.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_processing/typing_detection.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/criticalsection.h"
#include "voice_engine/audio_level.h"
#include "voice_engine/include/voe_base.h"

#if !defined(WEBRTC_ANDROID) && !defined(WEBRTC_IOS)
#define WEBRTC_VOICE_ENGINE_TYPING_DETECTION 1
#else
#define WEBRTC_VOICE_ENGINE_TYPING_DETECTION 0
#endif

namespace webrtc {
class AudioProcessing;
class ProcessThread;

namespace voe {

class ChannelManager;
class MixedAudio;

class TransmitMixer {
public:
    static int32_t Create(TransmitMixer*& mixer);

    static void Destroy(TransmitMixer*& mixer);

    void SetEngineInformation(ChannelManager* channelManager);

    int32_t SetAudioProcessingModule(AudioProcessing* audioProcessingModule);

    int32_t PrepareDemux(const void* audioSamples,
                         size_t nSamples,
                         size_t nChannels,
                         uint32_t samplesPerSec,
                         uint16_t totalDelayMS,
                         int32_t  clockDrift,
                         uint16_t currentMicLevel,
                         bool keyPressed);

    void ProcessAndEncodeAudio();

    // Must be called on the same thread as PrepareDemux().
    uint32_t CaptureLevel() const;

    int32_t StopSend();

    // TODO(solenberg): Remove, once AudioMonitor is gone.
    int8_t AudioLevel() const;

    // 'virtual' to allow mocking.
    virtual int16_t AudioLevelFullRange() const;

    // See description of "totalAudioEnergy" in the WebRTC stats spec:
    // https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats-totalaudioenergy
    // 'virtual' to allow mocking.
    virtual double GetTotalInputEnergy() const;

    // 'virtual' to allow mocking.
    virtual double GetTotalInputDuration() const;

    virtual ~TransmitMixer();

  // Virtual to allow mocking.
  virtual void EnableStereoChannelSwapping(bool enable);
  bool IsStereoChannelSwappingEnabled();

  // Virtual to allow mocking.
  virtual bool typing_noise_detected() const;

protected:
    TransmitMixer() = default;

private:
    // Gets the maximum sample rate and number of channels over all currently
    // sending codecs.
    void GetSendCodecInfo(int* max_sample_rate, size_t* max_channels);

    void GenerateAudioFrame(const int16_t audioSamples[],
                            size_t nSamples,
                            size_t nChannels,
                            int samplesPerSec);

    void ProcessAudio(int delay_ms, int clock_drift, int current_mic_level,
                      bool key_pressed);

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    void TypingDetection(bool key_pressed);
#endif

    // uses
    ChannelManager* _channelManagerPtr = nullptr;
    AudioProcessing* audioproc_ = nullptr;

    // owns
    AudioFrame _audioFrame;
    PushResampler<int16_t> resampler_;  // ADM sample rate -> mixing rate
    voe::AudioLevel _audioLevel;

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    webrtc::TypingDetection typing_detection_;
#endif

    rtc::CriticalSection lock_;
    bool typing_noise_detected_ RTC_GUARDED_BY(lock_) = false;

    uint32_t _captureLevel = 0;
    bool stereo_codec_ = false;
    bool swap_stereo_channels_ = false;
};
}  // namespace voe
}  // namespace webrtc

#endif  // VOICE_ENGINE_TRANSMIT_MIXER_H_
