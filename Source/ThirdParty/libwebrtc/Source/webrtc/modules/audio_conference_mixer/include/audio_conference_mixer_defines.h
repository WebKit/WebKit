/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_INCLUDE_AUDIO_CONFERENCE_MIXER_DEFINES_H_
#define WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_INCLUDE_AUDIO_CONFERENCE_MIXER_DEFINES_H_

#include "webrtc/base/checks.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class MixHistory;

// A callback class that all mixer participants must inherit from/implement.
class MixerParticipant
{
public:
    // The implementation of this function should update audioFrame with new
    // audio every time it's called.
    //
    // If it returns -1, the frame will not be added to the mix.
    //
    // NOTE: This function should not be called. It will remain for a short
    // time so that subclasses can override it without getting warnings.
    // TODO(henrik.lundin) Remove this function.
    virtual int32_t GetAudioFrame(int32_t id,
                                  AudioFrame* audioFrame) {
      RTC_CHECK(false);
      return -1;
    }


    // The implementation of GetAudioFrameWithMuted should update audio_frame
    // with new audio every time it's called. The return value will be
    // interpreted as follows.
    enum class AudioFrameInfo {
      kNormal,  // The samples in audio_frame are valid and should be used.
      kMuted,   // The samples in audio_frame should not be used, but should be
                // implicitly interpreted as zero. Other fields in audio_frame
                // may be read and should contain meaningful values.
      kError    // audio_frame will not be used.
    };

    virtual AudioFrameInfo GetAudioFrameWithMuted(int32_t id,
                                                  AudioFrame* audio_frame) {
      return GetAudioFrame(id, audio_frame) == -1 ?
          AudioFrameInfo::kError :
          AudioFrameInfo::kNormal;
    }

    // Returns true if the participant was mixed this mix iteration.
    bool IsMixed() const;

    // This function specifies the sampling frequency needed for the AudioFrame
    // for future GetAudioFrame(..) calls.
    virtual int32_t NeededFrequency(int32_t id) const = 0;

    MixHistory* _mixHistory;
protected:
    MixerParticipant();
    virtual ~MixerParticipant();
};

class AudioMixerOutputReceiver
{
public:
    // This callback function provides the mixed audio for this mix iteration.
    // Note that uniqueAudioFrames is an array of AudioFrame pointers with the
    // size according to the size parameter.
    virtual void NewMixedAudio(const int32_t id,
                               const AudioFrame& generalAudioFrame,
                               const AudioFrame** uniqueAudioFrames,
                               const uint32_t size) = 0;
protected:
    AudioMixerOutputReceiver() {}
    virtual ~AudioMixerOutputReceiver() {}
};
}  // namespace webrtc

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_INCLUDE_AUDIO_CONFERENCE_MIXER_DEFINES_H_
