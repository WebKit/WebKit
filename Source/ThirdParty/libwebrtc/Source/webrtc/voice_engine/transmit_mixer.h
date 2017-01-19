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
#include "webrtc/modules/utility/include/file_player.h"
#include "webrtc/modules/utility/include/file_recorder.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/level_indicator.h"
#include "webrtc/voice_engine/monitor_module.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

namespace webrtc {

class AudioProcessing;
class ProcessThread;
class VoEExternalMedia;
class VoEMediaProcess;

namespace voe {

class ChannelManager;
class MixedAudio;
class Statistics;

class TransmitMixer : public MonitorObserver,
                      public FileCallback {
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

    // VoEExternalMedia
    int RegisterExternalMediaProcessing(VoEMediaProcess* object,
                                        ProcessingTypes type);
    int DeRegisterExternalMediaProcessing(ProcessingTypes type);

    int GetMixingFrequency();

    // VoEVolumeControl
    int SetMute(bool enable);

    bool Mute() const;

    int8_t AudioLevel() const;

    int16_t AudioLevelFullRange() const;

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

    // MonitorObserver
    void OnPeriodicProcess();


    // FileCallback
    void PlayNotification(int32_t id,
                          uint32_t durationMs);

    void RecordNotification(int32_t id,
                            uint32_t durationMs);

    void PlayFileEnded(int32_t id);

    void RecordFileEnded(int32_t id);

#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    // Typing detection
    int TimeSinceLastTyping(int &seconds);
    int SetTypingDetectionParameters(int timeWindow,
                                     int costPerTyping,
                                     int reportingThreshold,
                                     int penaltyDecay,
                                     int typeEventDelay);
#endif

  void EnableStereoChannelSwapping(bool enable);
  bool IsStereoChannelSwappingEnabled();

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

#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    void TypingDetection(bool keyPressed);
#endif

    // uses
    Statistics* _engineStatisticsPtr;
    ChannelManager* _channelManagerPtr;
    AudioProcessing* audioproc_;
    VoiceEngineObserver* _voiceEngineObserverPtr;
    ProcessThread* _processThreadPtr;

    // owns
    MonitorModule _monitorModule;
    AudioFrame _audioFrame;
    PushResampler<int16_t> resampler_;  // ADM sample rate -> mixing rate
    std::unique_ptr<FilePlayer> file_player_;
    std::unique_ptr<FileRecorder> file_recorder_;
    std::unique_ptr<FileRecorder> file_call_recorder_;
    int _filePlayerId;
    int _fileRecorderId;
    int _fileCallRecorderId;
    bool _filePlaying;
    bool _fileRecording;
    bool _fileCallRecording;
    voe::AudioLevel _audioLevel;
    // protect file instances and their variables in MixedParticipants()
    rtc::CriticalSection _critSect;
    rtc::CriticalSection _callbackCritSect;

#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    webrtc::TypingDetection _typingDetection;
    bool _typingNoiseWarningPending;
    bool _typingNoiseDetected;
#endif
    bool _saturationWarning;

    int _instanceId;
    bool _mixFileWithMicrophone;
    uint32_t _captureLevel;
    VoEMediaProcess* external_postproc_ptr_;
    VoEMediaProcess* external_preproc_ptr_;
    bool _mute;
    bool stereo_codec_;
    bool swap_stereo_channels_;
};

}  // namespace voe

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_TRANSMIT_MIXER_H
