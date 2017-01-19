/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_INCLUDE_AUDIO_CONFERENCE_MIXER_H_
#define WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_INCLUDE_AUDIO_CONFERENCE_MIXER_H_

#include "webrtc/modules/audio_conference_mixer/include/audio_conference_mixer_defines.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/modules/include/module_common_types.h"

namespace webrtc {
class AudioMixerOutputReceiver;
class MixerParticipant;
class Trace;

class AudioConferenceMixer : public Module
{
public:
    enum {kMaximumAmountOfMixedParticipants = 3};
    enum Frequency
    {
        kNbInHz           = 8000,
        kWbInHz           = 16000,
        kSwbInHz          = 32000,
        kFbInHz           = 48000,
        kLowestPossible   = -1,
        kDefaultFrequency = kWbInHz
    };

    // Factory method. Constructor disabled.
    static AudioConferenceMixer* Create(int id);
    virtual ~AudioConferenceMixer() {}

    // Module functions
    int64_t TimeUntilNextProcess() override = 0;
    void Process() override = 0;

    // Register/unregister a callback class for receiving the mixed audio.
    virtual int32_t RegisterMixedStreamCallback(
        AudioMixerOutputReceiver* receiver) = 0;
    virtual int32_t UnRegisterMixedStreamCallback() = 0;

    // Add/remove participants as candidates for mixing.
    virtual int32_t SetMixabilityStatus(MixerParticipant* participant,
                                        bool mixable) = 0;
    // Returns true if a participant is a candidate for mixing.
    virtual bool MixabilityStatus(
        const MixerParticipant& participant) const = 0;

    // Inform the mixer that the participant should always be mixed and not
    // count toward the number of mixed participants. Note that a participant
    // must have been added to the mixer (by calling SetMixabilityStatus())
    // before this function can be successfully called.
    virtual int32_t SetAnonymousMixabilityStatus(
        MixerParticipant* participant, bool mixable) = 0;
    // Returns true if the participant is mixed anonymously.
    virtual bool AnonymousMixabilityStatus(
        const MixerParticipant& participant) const = 0;

    // Set the minimum sampling frequency at which to mix. The mixing algorithm
    // may still choose to mix at a higher samling frequency to avoid
    // downsampling of audio contributing to the mixed audio.
    virtual int32_t SetMinimumMixingFrequency(Frequency freq) = 0;

protected:
    AudioConferenceMixer() {}
};
}  // namespace webrtc

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_INCLUDE_AUDIO_CONFERENCE_MIXER_H_
