/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/transmit_mixer.h"

#include <memory>

#include "webrtc/audio/utility/audio_frame_operations.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/base/location.h"
#include "webrtc/base/logging.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/channel_manager.h"
#include "webrtc/voice_engine/statistics.h"
#include "webrtc/voice_engine/utility.h"
#include "webrtc/voice_engine/voe_base_impl.h"

namespace webrtc {
namespace voe {

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
// TODO(ajm): The thread safety of this is dubious...
void TransmitMixer::OnPeriodicProcess()
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::OnPeriodicProcess()");

    bool send_typing_noise_warning = false;
    bool typing_noise_detected = false;
    {
      rtc::CritScope cs(&_critSect);
      if (_typingNoiseWarningPending) {
        send_typing_noise_warning = true;
        typing_noise_detected = _typingNoiseDetected;
        _typingNoiseWarningPending = false;
      }
    }
    if (send_typing_noise_warning) {
        rtc::CritScope cs(&_callbackCritSect);
        if (_voiceEngineObserverPtr) {
            if (typing_noise_detected) {
                WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                             "TransmitMixer::OnPeriodicProcess() => "
                             "CallbackOnError(VE_TYPING_NOISE_WARNING)");
                _voiceEngineObserverPtr->CallbackOnError(
                    -1,
                    VE_TYPING_NOISE_WARNING);
            } else {
                WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                             "TransmitMixer::OnPeriodicProcess() => "
                             "CallbackOnError(VE_TYPING_NOISE_OFF_WARNING)");
                _voiceEngineObserverPtr->CallbackOnError(
                    -1,
                    VE_TYPING_NOISE_OFF_WARNING);
            }
        }
    }
}
#endif  // WEBRTC_VOICE_ENGINE_TYPING_DETECTION

void TransmitMixer::PlayNotification(int32_t id,
                                     uint32_t durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PlayNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}

void TransmitMixer::RecordNotification(int32_t id,
                                       uint32_t durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "TransmitMixer::RecordNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}

void TransmitMixer::PlayFileEnded(int32_t id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PlayFileEnded(id=%d)", id);

    assert(id == _filePlayerId);

    rtc::CritScope cs(&_critSect);

    _filePlaying = false;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PlayFileEnded() =>"
                 "file player module is shutdown");
}

void
TransmitMixer::RecordFileEnded(int32_t id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::RecordFileEnded(id=%d)", id);

    if (id == _fileRecorderId)
    {
        rtc::CritScope cs(&_critSect);
        _fileRecording = false;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::RecordFileEnded() => fileRecorder module"
                     "is shutdown");
    } else if (id == _fileCallRecorderId)
    {
        rtc::CritScope cs(&_critSect);
        _fileCallRecording = false;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::RecordFileEnded() => fileCallRecorder"
                     "module is shutdown");
    }
}

int32_t
TransmitMixer::Create(TransmitMixer*& mixer, uint32_t instanceId)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId, -1),
                 "TransmitMixer::Create(instanceId=%d)", instanceId);
    mixer = new TransmitMixer(instanceId);
    if (mixer == NULL)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId, -1),
                     "TransmitMixer::Create() unable to allocate memory"
                     "for mixer");
        return -1;
    }
    return 0;
}

void
TransmitMixer::Destroy(TransmitMixer*& mixer)
{
    if (mixer)
    {
        delete mixer;
        mixer = NULL;
    }
}

TransmitMixer::TransmitMixer(uint32_t instanceId) :
    // Avoid conflict with other channels by adding 1024 - 1026,
    // won't use as much as 1024 channels.
    _filePlayerId(instanceId + 1024),
    _fileRecorderId(instanceId + 1025),
    _fileCallRecorderId(instanceId + 1026),
#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    _monitorModule(this),
#endif
    _instanceId(instanceId)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::TransmitMixer() - ctor");
}

TransmitMixer::~TransmitMixer()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::~TransmitMixer() - dtor");
#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    if (_processThreadPtr)
        _processThreadPtr->DeRegisterModule(&_monitorModule);
#endif
    {
        rtc::CritScope cs(&_critSect);
        if (file_recorder_) {
          file_recorder_->RegisterModuleFileCallback(NULL);
          file_recorder_->StopRecording();
        }
        if (file_call_recorder_) {
          file_call_recorder_->RegisterModuleFileCallback(NULL);
          file_call_recorder_->StopRecording();
        }
        if (file_player_) {
          file_player_->RegisterModuleFileCallback(NULL);
          file_player_->StopPlayingFile();
        }
    }
}

int32_t
TransmitMixer::SetEngineInformation(ProcessThread& processThread,
                                    Statistics& engineStatistics,
                                    ChannelManager& channelManager)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::SetEngineInformation()");

    _processThreadPtr = &processThread;
    _engineStatisticsPtr = &engineStatistics;
    _channelManagerPtr = &channelManager;

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    _processThreadPtr->RegisterModule(&_monitorModule, RTC_FROM_HERE);
#endif
    return 0;
}

int32_t
TransmitMixer::RegisterVoiceEngineObserver(VoiceEngineObserver& observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::RegisterVoiceEngineObserver()");
    rtc::CritScope cs(&_callbackCritSect);

    if (_voiceEngineObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterVoiceEngineObserver() observer already enabled");
        return -1;
    }
    _voiceEngineObserverPtr = &observer;
    return 0;
}

int32_t
TransmitMixer::SetAudioProcessingModule(AudioProcessing* audioProcessingModule)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::SetAudioProcessingModule("
                 "audioProcessingModule=0x%x)",
                 audioProcessingModule);
    audioproc_ = audioProcessingModule;
    return 0;
}

void TransmitMixer::GetSendCodecInfo(int* max_sample_rate,
                                     size_t* max_channels) {
  *max_sample_rate = 8000;
  *max_channels = 1;
  for (ChannelManager::Iterator it(_channelManagerPtr); it.IsValid();
       it.Increment()) {
    Channel* channel = it.GetChannel();
    if (channel->Sending()) {
      CodecInst codec;
      channel->GetSendCodec(codec);
      *max_sample_rate = std::max(*max_sample_rate, codec.plfreq);
      *max_channels = std::max(*max_channels, codec.channels);
    }
  }
}

int32_t
TransmitMixer::PrepareDemux(const void* audioSamples,
                            size_t nSamples,
                            size_t nChannels,
                            uint32_t samplesPerSec,
                            uint16_t totalDelayMS,
                            int32_t clockDrift,
                            uint16_t currentMicLevel,
                            bool keyPressed)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PrepareDemux(nSamples=%" PRIuS ", "
                 "nChannels=%" PRIuS ", samplesPerSec=%u, totalDelayMS=%u, "
                 "clockDrift=%d, currentMicLevel=%u)",
                 nSamples, nChannels, samplesPerSec, totalDelayMS, clockDrift,
                 currentMicLevel);

    // --- Resample input audio and create/store the initial audio frame
    GenerateAudioFrame(static_cast<const int16_t*>(audioSamples),
                       nSamples,
                       nChannels,
                       samplesPerSec);

    // --- Near-end audio processing.
    ProcessAudio(totalDelayMS, clockDrift, currentMicLevel, keyPressed);

    if (swap_stereo_channels_ && stereo_codec_)
      // Only bother swapping if we're using a stereo codec.
      AudioFrameOperations::SwapStereoChannels(&_audioFrame);

    // --- Annoying typing detection (utilizes the APM/VAD decision)
#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    TypingDetection(keyPressed);
#endif

    // --- Mix with file (does not affect the mixing frequency)
    if (_filePlaying)
    {
        MixOrReplaceAudioWithFile(_audioFrame.sample_rate_hz_);
    }

    // --- Record to file
    bool file_recording = false;
    {
        rtc::CritScope cs(&_critSect);
        file_recording =  _fileRecording;
    }
    if (file_recording)
    {
        RecordAudioToFile(_audioFrame.sample_rate_hz_);
    }

    // --- Measure audio level of speech after all processing.
    _audioLevel.ComputeLevel(_audioFrame);
    return 0;
}

void TransmitMixer::ProcessAndEncodeAudio() {
  RTC_DCHECK_GT(_audioFrame.samples_per_channel_, 0);
  for (ChannelManager::Iterator it(_channelManagerPtr); it.IsValid();
       it.Increment()) {
    Channel* const channel = it.GetChannel();
    if (channel->Sending()) {
      channel->ProcessAndEncodeAudio(_audioFrame);
    }
  }
}

uint32_t TransmitMixer::CaptureLevel() const
{
    return _captureLevel;
}

int32_t
TransmitMixer::StopSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "TransmitMixer::StopSend()");
    _audioLevel.Clear();
    return 0;
}

int TransmitMixer::StartPlayingFileAsMicrophone(const char* fileName,
                                                bool loop,
                                                FileFormats format,
                                                int startPosition,
                                                float volumeScaling,
                                                int stopPosition,
                                                const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StartPlayingFileAsMicrophone("
                 "fileNameUTF8[]=%s,loop=%d, format=%d, volumeScaling=%5.3f,"
                 " startPosition=%d, stopPosition=%d)", fileName, loop,
                 format, volumeScaling, startPosition, stopPosition);

    if (_filePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceWarning,
            "StartPlayingFileAsMicrophone() is already playing");
        return 0;
    }

    rtc::CritScope cs(&_critSect);

    // Destroy the old instance
    if (file_player_) {
      file_player_->RegisterModuleFileCallback(NULL);
      file_player_.reset();
    }

    // Dynamically create the instance
    file_player_ =
        FilePlayer::CreateFilePlayer(_filePlayerId, (const FileFormats)format);

    if (!file_player_) {
      _engineStatisticsPtr->SetLastError(
          VE_INVALID_ARGUMENT, kTraceError,
          "StartPlayingFileAsMicrophone() filePlayer format isnot correct");
      return -1;
    }

    const uint32_t notificationTime(0);

    if (file_player_->StartPlayingFile(
            fileName, loop, startPosition, volumeScaling, notificationTime,
            stopPosition, (const CodecInst*)codecInst) != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_BAD_FILE, kTraceError,
          "StartPlayingFile() failed to start file playout");
      file_player_->StopPlayingFile();
      file_player_.reset();
      return -1;
    }

    file_player_->RegisterModuleFileCallback(this);
    _filePlaying = true;

    return 0;
}

int TransmitMixer::StartPlayingFileAsMicrophone(InStream* stream,
                                                FileFormats format,
                                                int startPosition,
                                                float volumeScaling,
                                                int stopPosition,
                                                const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "TransmitMixer::StartPlayingFileAsMicrophone(format=%d,"
                 " volumeScaling=%5.3f, startPosition=%d, stopPosition=%d)",
                 format, volumeScaling, startPosition, stopPosition);

    if (stream == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFileAsMicrophone() NULL as input stream");
        return -1;
    }

    if (_filePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceWarning,
            "StartPlayingFileAsMicrophone() is already playing");
        return 0;
    }

    rtc::CritScope cs(&_critSect);

    // Destroy the old instance
    if (file_player_) {
      file_player_->RegisterModuleFileCallback(NULL);
      file_player_.reset();
    }

    // Dynamically create the instance
    file_player_ =
        FilePlayer::CreateFilePlayer(_filePlayerId, (const FileFormats)format);

    if (!file_player_) {
      _engineStatisticsPtr->SetLastError(
          VE_INVALID_ARGUMENT, kTraceWarning,
          "StartPlayingFileAsMicrophone() filePlayer format isnot correct");
      return -1;
    }

    const uint32_t notificationTime(0);

    if (file_player_->StartPlayingFile(stream, startPosition, volumeScaling,
                                       notificationTime, stopPosition,
                                       (const CodecInst*)codecInst) != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_BAD_FILE, kTraceError,
          "StartPlayingFile() failed to start file playout");
      file_player_->StopPlayingFile();
      file_player_.reset();
      return -1;
    }
    file_player_->RegisterModuleFileCallback(this);
    _filePlaying = true;

    return 0;
}

int TransmitMixer::StopPlayingFileAsMicrophone()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "TransmitMixer::StopPlayingFileAsMicrophone()");

    if (!_filePlaying)
    {
        return 0;
    }

    rtc::CritScope cs(&_critSect);

    if (file_player_->StopPlayingFile() != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_CANNOT_STOP_PLAYOUT, kTraceError,
          "StopPlayingFile() couldnot stop playing file");
      return -1;
    }

    file_player_->RegisterModuleFileCallback(NULL);
    file_player_.reset();
    _filePlaying = false;

    return 0;
}

int TransmitMixer::IsPlayingFileAsMicrophone() const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::IsPlayingFileAsMicrophone()");
    return _filePlaying;
}

int TransmitMixer::StartRecordingMicrophone(const char* fileName,
                                            const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StartRecordingMicrophone(fileName=%s)",
                 fileName);

    rtc::CritScope cs(&_critSect);

    if (_fileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "StartRecordingMicrophone() is already recording");
        return 0;
    }

    FileFormats format;
    const uint32_t notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec = { 100, "L16", 16000, 320, 1, 320000 };

    if (codecInst != NULL && codecInst->channels > 2)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingMicrophone() invalid compression");
        return (-1);
    }
    if (codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst = &dummyCodec;
    } else if ((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    } else
    {
        format = kFileFormatCompressedFile;
    }

    // Destroy the old instance
    if (file_recorder_) {
      file_recorder_->RegisterModuleFileCallback(NULL);
      file_recorder_.reset();
    }

    file_recorder_ = FileRecorder::CreateFileRecorder(
        _fileRecorderId, (const FileFormats)format);
    if (!file_recorder_) {
      _engineStatisticsPtr->SetLastError(
          VE_INVALID_ARGUMENT, kTraceError,
          "StartRecordingMicrophone() fileRecorder format isnot correct");
      return -1;
    }

    if (file_recorder_->StartRecordingAudioFile(
            fileName, (const CodecInst&)*codecInst, notificationTime) != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_BAD_FILE, kTraceError,
          "StartRecordingAudioFile() failed to start file recording");
      file_recorder_->StopRecording();
      file_recorder_.reset();
      return -1;
    }
    file_recorder_->RegisterModuleFileCallback(this);
    _fileRecording = true;

    return 0;
}

int TransmitMixer::StartRecordingMicrophone(OutStream* stream,
                                            const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "TransmitMixer::StartRecordingMicrophone()");

    rtc::CritScope cs(&_critSect);

    if (_fileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "StartRecordingMicrophone() is already recording");
        return 0;
    }

    FileFormats format;
    const uint32_t notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec = { 100, "L16", 16000, 320, 1, 320000 };

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingMicrophone() invalid compression");
        return (-1);
    }
    if (codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst = &dummyCodec;
    } else if ((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    } else
    {
        format = kFileFormatCompressedFile;
    }

    // Destroy the old instance
    if (file_recorder_) {
      file_recorder_->RegisterModuleFileCallback(NULL);
      file_recorder_.reset();
    }

    file_recorder_ = FileRecorder::CreateFileRecorder(
        _fileRecorderId, (const FileFormats)format);
    if (!file_recorder_) {
      _engineStatisticsPtr->SetLastError(
          VE_INVALID_ARGUMENT, kTraceError,
          "StartRecordingMicrophone() fileRecorder format isnot correct");
      return -1;
    }

    if (file_recorder_->StartRecordingAudioFile(stream, *codecInst,
                                                notificationTime) != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_BAD_FILE, kTraceError,
          "StartRecordingAudioFile() failed to start file recording");
      file_recorder_->StopRecording();
      file_recorder_.reset();
      return -1;
    }

    file_recorder_->RegisterModuleFileCallback(this);
    _fileRecording = true;

    return 0;
}


int TransmitMixer::StopRecordingMicrophone()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StopRecordingMicrophone()");

    rtc::CritScope cs(&_critSect);

    if (!_fileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "StopRecordingMicrophone() isnot recording");
        return 0;
    }

    if (file_recorder_->StopRecording() != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_STOP_RECORDING_FAILED, kTraceError,
          "StopRecording(), could not stop recording");
      return -1;
    }
    file_recorder_->RegisterModuleFileCallback(NULL);
    file_recorder_.reset();
    _fileRecording = false;

    return 0;
}

int TransmitMixer::StartRecordingCall(const char* fileName,
                                      const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StartRecordingCall(fileName=%s)", fileName);

    if (_fileCallRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "StartRecordingCall() is already recording");
        return 0;
    }

    FileFormats format;
    const uint32_t notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec = { 100, "L16", 16000, 320, 1, 320000 };

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingCall() invalid compression");
        return (-1);
    }
    if (codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst = &dummyCodec;
    } else if ((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    } else
    {
        format = kFileFormatCompressedFile;
    }

    rtc::CritScope cs(&_critSect);

    // Destroy the old instance
    if (file_call_recorder_) {
      file_call_recorder_->RegisterModuleFileCallback(NULL);
      file_call_recorder_.reset();
    }

    file_call_recorder_ = FileRecorder::CreateFileRecorder(
        _fileCallRecorderId, (const FileFormats)format);
    if (!file_call_recorder_) {
      _engineStatisticsPtr->SetLastError(
          VE_INVALID_ARGUMENT, kTraceError,
          "StartRecordingCall() fileRecorder format isnot correct");
      return -1;
    }

    if (file_call_recorder_->StartRecordingAudioFile(
            fileName, (const CodecInst&)*codecInst, notificationTime) != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_BAD_FILE, kTraceError,
          "StartRecordingAudioFile() failed to start file recording");
      file_call_recorder_->StopRecording();
      file_call_recorder_.reset();
      return -1;
    }
    file_call_recorder_->RegisterModuleFileCallback(this);
    _fileCallRecording = true;

    return 0;
}

int TransmitMixer::StartRecordingCall(OutStream* stream,
                                      const  CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StartRecordingCall()");

    if (_fileCallRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "StartRecordingCall() is already recording");
        return 0;
    }

    FileFormats format;
    const uint32_t notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec = { 100, "L16", 16000, 320, 1, 320000 };

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingCall() invalid compression");
        return (-1);
    }
    if (codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst = &dummyCodec;
    } else if ((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    } else
    {
        format = kFileFormatCompressedFile;
    }

    rtc::CritScope cs(&_critSect);

    // Destroy the old instance
    if (file_call_recorder_) {
      file_call_recorder_->RegisterModuleFileCallback(NULL);
      file_call_recorder_.reset();
    }

    file_call_recorder_ = FileRecorder::CreateFileRecorder(
        _fileCallRecorderId, (const FileFormats)format);
    if (!file_call_recorder_) {
      _engineStatisticsPtr->SetLastError(
          VE_INVALID_ARGUMENT, kTraceError,
          "StartRecordingCall() fileRecorder format isnot correct");
      return -1;
    }

    if (file_call_recorder_->StartRecordingAudioFile(stream, *codecInst,
                                                     notificationTime) != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_BAD_FILE, kTraceError,
          "StartRecordingAudioFile() failed to start file recording");
      file_call_recorder_->StopRecording();
      file_call_recorder_.reset();
      return -1;
    }

    file_call_recorder_->RegisterModuleFileCallback(this);
    _fileCallRecording = true;

    return 0;
}

int TransmitMixer::StopRecordingCall()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StopRecordingCall()");

    if (!_fileCallRecording)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                     "StopRecordingCall() file isnot recording");
        return -1;
    }

    rtc::CritScope cs(&_critSect);

    if (file_call_recorder_->StopRecording() != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_STOP_RECORDING_FAILED, kTraceError,
          "StopRecording(), could not stop recording");
      return -1;
    }

    file_call_recorder_->RegisterModuleFileCallback(NULL);
    file_call_recorder_.reset();
    _fileCallRecording = false;

    return 0;
}

void
TransmitMixer::SetMixWithMicStatus(bool mix)
{
    _mixFileWithMicrophone = mix;
}

int8_t TransmitMixer::AudioLevel() const
{
    // Speech + file level [0,9]
    return _audioLevel.Level();
}

int16_t TransmitMixer::AudioLevelFullRange() const
{
    // Speech + file level [0,32767]
    return _audioLevel.LevelFullRange();
}

bool TransmitMixer::IsRecordingCall()
{
    return _fileCallRecording;
}

bool TransmitMixer::IsRecordingMic()
{
    rtc::CritScope cs(&_critSect);
    return _fileRecording;
}

void TransmitMixer::GenerateAudioFrame(const int16_t* audio,
                                       size_t samples_per_channel,
                                       size_t num_channels,
                                       int sample_rate_hz) {
  int codec_rate;
  size_t num_codec_channels;
  GetSendCodecInfo(&codec_rate, &num_codec_channels);
  stereo_codec_ = num_codec_channels == 2;

  // We want to process at the lowest rate possible without losing information.
  // Choose the lowest native rate at least equal to the input and codec rates.
  const int min_processing_rate = std::min(sample_rate_hz, codec_rate);
  for (size_t i = 0; i < AudioProcessing::kNumNativeSampleRates; ++i) {
    _audioFrame.sample_rate_hz_ = AudioProcessing::kNativeSampleRatesHz[i];
    if (_audioFrame.sample_rate_hz_ >= min_processing_rate) {
      break;
    }
  }
  _audioFrame.num_channels_ = std::min(num_channels, num_codec_channels);
  RemixAndResample(audio, samples_per_channel, num_channels, sample_rate_hz,
                   &resampler_, &_audioFrame);
}

int32_t TransmitMixer::RecordAudioToFile(
    uint32_t mixingFrequency)
{
    rtc::CritScope cs(&_critSect);
    if (!file_recorder_) {
      WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "TransmitMixer::RecordAudioToFile() filerecorder doesnot"
                   "exist");
      return -1;
    }

    if (file_recorder_->RecordAudioToFile(_audioFrame) != 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "TransmitMixer::RecordAudioToFile() file recording"
                   "failed");
      return -1;
    }

    return 0;
}

int32_t TransmitMixer::MixOrReplaceAudioWithFile(
    int mixingFrequency)
{
    std::unique_ptr<int16_t[]> fileBuffer(new int16_t[640]);

    size_t fileSamples(0);
    {
        rtc::CritScope cs(&_critSect);
        if (!file_player_) {
          WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                       "TransmitMixer::MixOrReplaceAudioWithFile()"
                       "fileplayer doesnot exist");
          return -1;
        }

        if (file_player_->Get10msAudioFromFile(fileBuffer.get(), &fileSamples,
                                               mixingFrequency) == -1) {
          WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                       "TransmitMixer::MixOrReplaceAudioWithFile() file"
                       " mixing failed");
          return -1;
        }
    }

    assert(_audioFrame.samples_per_channel_ == fileSamples);

    if (_mixFileWithMicrophone)
    {
        // Currently file stream is always mono.
        // TODO(xians): Change the code when FilePlayer supports real stereo.
        MixWithSat(_audioFrame.mutable_data(),
                   _audioFrame.num_channels_,
                   fileBuffer.get(),
                   1,
                   fileSamples);
    } else
    {
        // Replace ACM audio with file.
        // Currently file stream is always mono.
        // TODO(xians): Change the code when FilePlayer supports real stereo.
        _audioFrame.UpdateFrame(-1,
                                0xFFFFFFFF,
                                fileBuffer.get(),
                                fileSamples,
                                mixingFrequency,
                                AudioFrame::kNormalSpeech,
                                AudioFrame::kVadUnknown,
                                1);
    }
    return 0;
}

void TransmitMixer::ProcessAudio(int delay_ms, int clock_drift,
                                 int current_mic_level, bool key_pressed) {
  if (audioproc_->set_stream_delay_ms(delay_ms) != 0) {
    // Silently ignore this failure to avoid flooding the logs.
  }

  GainControl* agc = audioproc_->gain_control();
  if (agc->set_stream_analog_level(current_mic_level) != 0) {
    LOG(LS_ERROR) << "set_stream_analog_level failed: current_mic_level = "
                  << current_mic_level;
    assert(false);
  }

  EchoCancellation* aec = audioproc_->echo_cancellation();
  if (aec->is_drift_compensation_enabled()) {
    aec->set_stream_drift_samples(clock_drift);
  }

  audioproc_->set_stream_key_pressed(key_pressed);

  int err = audioproc_->ProcessStream(&_audioFrame);
  if (err != 0) {
    LOG(LS_ERROR) << "ProcessStream() error: " << err;
    assert(false);
  }

  // Store new capture level. Only updated when analog AGC is enabled.
  _captureLevel = agc->stream_analog_level();
}

#if WEBRTC_VOICE_ENGINE_TYPING_DETECTION
void TransmitMixer::TypingDetection(bool keyPressed)
{
  // We let the VAD determine if we're using this feature or not.
  if (_audioFrame.vad_activity_ == AudioFrame::kVadUnknown) {
    return;
  }

  bool vadActive = _audioFrame.vad_activity_ == AudioFrame::kVadActive;
  if (_typingDetection.Process(keyPressed, vadActive)) {
    rtc::CritScope cs(&_critSect);
    _typingNoiseWarningPending = true;
    _typingNoiseDetected = true;
  } else {
    rtc::CritScope cs(&_critSect);
    // If there is already a warning pending, do not change the state.
    // Otherwise set a warning pending if last callback was for noise detected.
    if (!_typingNoiseWarningPending && _typingNoiseDetected) {
      _typingNoiseWarningPending = true;
      _typingNoiseDetected = false;
    }
  }
}
#endif

void TransmitMixer::EnableStereoChannelSwapping(bool enable) {
  swap_stereo_channels_ = enable;
}

bool TransmitMixer::IsStereoChannelSwappingEnabled() {
  return swap_stereo_channels_;
}

}  // namespace voe
}  // namespace webrtc
