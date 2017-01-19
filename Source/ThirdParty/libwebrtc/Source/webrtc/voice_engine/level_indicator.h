/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_LEVEL_INDICATOR_H
#define WEBRTC_VOICE_ENGINE_LEVEL_INDICATOR_H

#include "webrtc/base/criticalsection.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class AudioFrame;
namespace voe {

class AudioLevel
{
public:
    AudioLevel();
    virtual ~AudioLevel();

    // Called on "API thread(s)" from APIs like VoEBase::CreateChannel(),
    // VoEBase::StopSend(), VoEVolumeControl::GetSpeechOutputLevel().
    int8_t Level() const;
    int16_t LevelFullRange() const;
    void Clear();

    // Called on a native capture audio thread (platform dependent) from the
    // AudioTransport::RecordedDataIsAvailable() callback.
    // In Chrome, this method is called on the AudioInputDevice thread.
    void ComputeLevel(const AudioFrame& audioFrame);

private:
    enum { kUpdateFrequency = 10};

    rtc::CriticalSection _critSect;

    int16_t _absMax;
    int16_t _count;
    int8_t _currentLevel;
    int16_t _currentLevelFullRange;
};

}  // namespace voe

}  // namespace webrtc

#endif // WEBRTC_VOICE_ENGINE_LEVEL_INDICATOR_H
