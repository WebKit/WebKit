/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_OUTPUT_MIXER_H_
#define WEBRTC_VOICE_ENGINE_OUTPUT_MIXER_H_

#include <memory>

#include "webrtc/base/criticalsection.h"
#include "webrtc/common_audio/resampler/include/push_resampler.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_conference_mixer/include/audio_conference_mixer.h"
#include "webrtc/modules/audio_conference_mixer/include/audio_conference_mixer_defines.h"
#include "webrtc/voice_engine/file_recorder.h"

namespace webrtc {

class AudioProcessing;
class FileWrapper;

namespace voe {

class Statistics;

class OutputMixer : public AudioMixerOutputReceiver,
                    public FileCallback
{
public:
    static int32_t Create(OutputMixer*& mixer, uint32_t instanceId);

    static void Destroy(OutputMixer*& mixer);

    int32_t SetEngineInformation(Statistics& engineStatistics);

    int32_t SetAudioProcessingModule(
        AudioProcessing* audioProcessingModule);

    int32_t MixActiveChannels();

    int32_t DoOperationsOnCombinedSignal(bool feed_data_to_apm);

    int32_t SetMixabilityStatus(MixerParticipant& participant,
                                bool mixable);

    int32_t SetAnonymousMixabilityStatus(MixerParticipant& participant,
                                         bool mixable);

    int GetMixedAudio(int sample_rate_hz, size_t num_channels,
                      AudioFrame* audioFrame);

    // VoEFile
    int StartRecordingPlayout(const char* fileName,
                              const CodecInst* codecInst);

    int StartRecordingPlayout(OutStream* stream,
                              const CodecInst* codecInst);
    int StopRecordingPlayout();

    virtual ~OutputMixer();

    // from AudioMixerOutputReceiver
    virtual void NewMixedAudio(
        int32_t id,
        const AudioFrame& generalAudioFrame,
        const AudioFrame** uniqueAudioFrames,
        uint32_t size);

    // For file recording
    void PlayNotification(int32_t id, uint32_t durationMs);

    void RecordNotification(int32_t id, uint32_t durationMs);

    void PlayFileEnded(int32_t id);
    void RecordFileEnded(int32_t id);

private:
    OutputMixer(uint32_t instanceId);

    // uses
    Statistics* _engineStatisticsPtr;
    AudioProcessing* _audioProcessingModulePtr;

    // Protects output_file_recorder_ and _outputFileRecording.
    rtc::CriticalSection _fileCritSect;
    AudioConferenceMixer& _mixerModule;
    AudioFrame _audioFrame;
    // Converts mixed audio to the audio device output rate.
    PushResampler<int16_t> resampler_;
    // Converts mixed audio to the audio processing rate.
    PushResampler<int16_t> audioproc_resampler_;
    int _instanceId;
    int _mixingFrequencyHz;
    std::unique_ptr<FileRecorder> output_file_recorder_;
    bool _outputFileRecording;
};

}  // namespace voe

}  // namespace werbtc

#endif  // VOICE_ENGINE_OUTPUT_MIXER_H_
