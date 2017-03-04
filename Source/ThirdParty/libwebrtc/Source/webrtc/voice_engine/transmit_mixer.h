/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_TRANSMIT_MIXER_H
#define WEBRTC_VOICE_ENGINE_TRANSMIT_MIXER_H

#include <memory>

#include "webrtc/base/criticalsection.h"
#include "webrtc/common_audio/resampler/include/push_resampler.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_processing/typing_detection.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/voice_engine/file_player.h"
#include "webrtc/voice_engine/file_recorder.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/level_indicator.h"
#include "webrtc/voice_engine/monitor_module.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

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
class Statistics;

class TransmitMixer : public FileCallback {
public:
    static int32_t Create(TransmitMixer*& mixer, uint32_t instanceId);

    static void Destroy(TransmitMixer*& mixer);

    int32_t SetEngineInformation(ProcessThread& processThread,
                                 Statistics& engineStatistics,
                                 ChannelManager& channelManager);

    int32_t SetAudioProcessingModule(
        AudioProcessing* audioProcessingModule);

    int32_t PrepareDemux(const void* audioSamples,
                         size_t nSamples,
                         size_t nChannels,
                         uint32_t samplesPerSec,
                         uint16_t totalDelayMS,
                         int32_t  clockDrift,
                         uint16_t currentMicLevel,
                         bool keyPressed);


    int32_t DemuxAndMix();
    // Used by the Chrome to pass the recording data to the specific VoE
    // channels for demux.
    void DemuxAndMix(const int voe_channels[], size_t number_of_voe_channels);

    int32_t EncodeAndSend();
    // Used by the Chrome to pass the recording data to the specific VoE
    // channels for encoding and sending to the network.
    void EncodeAndSend(const int voe_channels[], size_t number_of_voe_channels);

    // Must be called on the same thread as PrepareDemux().
    uint32_t CaptureLevel() const;

    int32_t StopSend();

    // VoEVolumeControl
    int SetMute(bool enable);

    bool Mute() const;

    int8_t AudioLevel() const;

    // 'virtual' to allow mocking.
    virtual int16_t AudioLevelFullRange() const;

    bool IsRecordingCall();

    bool IsRecordingMic();

    int StartPlayingFileAsMicrophone(const char* fileName,
                                     bool loop,
                                     FileFormats format,
                                     int startPosition,
                                     float volumeScaling,
                                     int stopPosition,
                                     const CodecInst* codecInst);

    int StartPlayingFileAsMicrophone(InStream* stream,
                                     FileFormats format,
                                     int startPosition,
                                     float volumeScaling,
                                     int stopPosition,
                                     const CodecInst* codecInst);

    int StopPlayingFileAsMicrophone();

    int IsPlayingFileAsMicrophone() const;

    int StartRecordingMicrophone(const char* fileName,
                                 const CodecInst* codecInst);

    int StartRecordingMicrophone(OutStream* stream,
                                 const CodecInst* codecInst);

    int StopRecordingMicrophone();

    int StartRecordingCall(const char* fileName, const CodecInst* codecInst);

    int StartRecordingCall(OutStream* stream, const CodecInst* codecInst);

    int StopRecordingCall();

    void SetMixWithMicStatus(bool mix);

    int32_t RegisterVoiceEngineObserver(VoiceEngineObserver& observer);

    virtual ~TransmitMixer();

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    // Periodic callback from the MonitorModule.
    void OnPeriodicProcess();
#endif

    // FileCallback
    void PlayNotification(const int32_t id,
                          const uint32_t durationMs);

    void RecordNotification(const int32_t id,
                            const uint32_t durationMs);

    void PlayFileEnded(const int32_t id);

    void RecordFileEnded(const int32_t id);

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    // Typing detection
    int TimeSinceLastTyping(int &seconds);
    int SetTypingDetectionParameters(int timeWindow,
                                     int costPerTyping,
                                     int reportingThreshold,
                                     int penaltyDecay,
                                     int typeEventDelay);
#endif

  // Virtual to allow mocking.
  virtual void EnableStereoChannelSwapping(bool enable);
  bool IsStereoChannelSwappingEnabled();

protected:
#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    TransmitMixer() : _monitorModule(this) {}
#else
    TransmitMixer() = default;
#endif

private:
    TransmitMixer(uint32_t instanceId);

    // Gets the maximum sample rate and number of channels over all currently
    // sending codecs.
    void GetSendCodecInfo(int* max_sample_rate, size_t* max_channels);

    void GenerateAudioFrame(const int16_t audioSamples[],
                            size_t nSamples,
                            size_t nChannels,
                            int samplesPerSec);
    int32_t RecordAudioToFile(uint32_t mixingFrequency);

    int32_t MixOrReplaceAudioWithFile(
        int mixingFrequency);

    void ProcessAudio(int delay_ms, int clock_drift, int current_mic_level,
                      bool key_pressed);

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    void TypingDetection(bool keyPressed);
#endif

    // uses
    Statistics* _engineStatisticsPtr = nullptr;
    ChannelManager* _channelManagerPtr = nullptr;
    AudioProcessing* audioproc_ = nullptr;
    VoiceEngineObserver* _voiceEngineObserverPtr = nullptr;
    ProcessThread* _processThreadPtr = nullptr;

    // owns
    AudioFrame _audioFrame;
    PushResampler<int16_t> resampler_;  // ADM sample rate -> mixing rate
    std::unique_ptr<FilePlayer> file_player_;
    std::unique_ptr<FileRecorder> file_recorder_;
    std::unique_ptr<FileRecorder> file_call_recorder_;
    int _filePlayerId = 0;
    int _fileRecorderId = 0;
    int _fileCallRecorderId = 0;
    bool _filePlaying = false;
    bool _fileRecording = false;
    bool _fileCallRecording = false;
    voe::AudioLevel _audioLevel;
    // protect file instances and their variables in MixedParticipants()
    rtc::CriticalSection _critSect;
    rtc::CriticalSection _callbackCritSect;

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    MonitorModule<TransmitMixer> _monitorModule;
    webrtc::TypingDetection _typingDetection;
    bool _typingNoiseWarningPending = false;
    bool _typingNoiseDetected = false;
#endif

    int _instanceId = 0;
    bool _mixFileWithMicrophone = false;
    uint32_t _captureLevel = 0;
    bool _mute = false;
    bool stereo_codec_ = false;
    bool swap_stereo_channels_ = false;
};
}  // namespace voe
}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_TRANSMIT_MIXER_H
