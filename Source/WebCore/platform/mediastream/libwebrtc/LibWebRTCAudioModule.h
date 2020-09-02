/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/modules/audio_device/include/audio_device.h>
#include <wtf/MonotonicTime.h>
#include <wtf/WorkQueue.h>

ALLOW_UNUSED_PARAMETERS_END

namespace WebCore {

// LibWebRTCAudioModule is pulling streamed data to ensure audio data is passed to the audio track.
class LibWebRTCAudioModule : public webrtc::AudioDeviceModule {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LibWebRTCAudioModule();

private:
    template<typename U> U shouldNotBeCalled(U value) const
    {
        ASSERT_NOT_REACHED();
        return value;
    }

    // webrtc::AudioDeviceModule API
    int32_t StartPlayout() final;
    int32_t StopPlayout() final;
    int32_t RegisterAudioCallback(webrtc::AudioTransport*) final;
    bool Playing() const final { return m_isPlaying; }

    int32_t ActiveAudioLayer(AudioLayer*) const final { return shouldNotBeCalled(-1); }
    int32_t Init() final { return 0; }
    int32_t Terminate() final { return 0; }
    bool Initialized() const final { return true; }
    int16_t PlayoutDevices() final { return 0; }
    int16_t RecordingDevices() final { return 0; }
    int32_t PlayoutDeviceName(uint16_t, char[webrtc::kAdmMaxDeviceNameSize], char[webrtc::kAdmMaxGuidSize]) final { return 0; }
    int32_t RecordingDeviceName(uint16_t, char[webrtc::kAdmMaxDeviceNameSize], char[webrtc::kAdmMaxGuidSize]) final { return 0; }
    int32_t SetPlayoutDevice(uint16_t) final { return 0; }
    int32_t SetPlayoutDevice(WindowsDeviceType) final { return 0; }
    int32_t SetRecordingDevice(uint16_t) final { return 0; }
    int32_t SetRecordingDevice(WindowsDeviceType) final { return 0; }
    int32_t PlayoutIsAvailable(bool*) final { return shouldNotBeCalled(-1); }
    int32_t InitPlayout() final { return 0; }
    bool PlayoutIsInitialized() const final { return true; }
    int32_t RecordingIsAvailable(bool*) final { return shouldNotBeCalled(-1); }
    int32_t InitRecording() final { return 0; }
    bool RecordingIsInitialized() const final { return false; }
    int32_t StartRecording() final { return 0; }
    int32_t StopRecording() final { return 0;  }
    bool Recording() const final { return 0;  }
    int32_t InitSpeaker() final { return 0; }
    bool SpeakerIsInitialized() const final { return false; }
    int32_t InitMicrophone() final { return 0; }
    bool MicrophoneIsInitialized() const final { return false; }
    int32_t MicrophoneVolumeIsAvailable(bool*) final { return shouldNotBeCalled(-1); }
    int32_t SpeakerVolumeIsAvailable(bool*) final { return shouldNotBeCalled(-1); }
    int32_t SetSpeakerVolume(uint32_t) final { return shouldNotBeCalled(-1); }
    int32_t SpeakerVolume(uint32_t*) const final { return shouldNotBeCalled(-1); }
    int32_t MaxSpeakerVolume(uint32_t*) const final { return shouldNotBeCalled(-1); }
    int32_t MinSpeakerVolume(uint32_t*) const final { return shouldNotBeCalled(-1); }
    int32_t SetMicrophoneVolume(uint32_t) final { return shouldNotBeCalled(-1); }
    int32_t MicrophoneVolume(uint32_t*) const final { return shouldNotBeCalled(-1); }
    int32_t MaxMicrophoneVolume(uint32_t*) const final { return shouldNotBeCalled(-1); }
    int32_t MinMicrophoneVolume(uint32_t*) const final { return shouldNotBeCalled(-1); }
    int32_t SpeakerMuteIsAvailable(bool*) final { return shouldNotBeCalled(-1); }
    int32_t SetSpeakerMute(bool) final { return shouldNotBeCalled(-1); }
    int32_t SpeakerMute(bool*) const final { return shouldNotBeCalled(-1); }
    int32_t MicrophoneMuteIsAvailable(bool*) final { return shouldNotBeCalled(-1); }
    int32_t SetMicrophoneMute(bool) final { return shouldNotBeCalled(-1); }
    int32_t MicrophoneMute(bool*) const final { return shouldNotBeCalled(-1); }
    int32_t StereoPlayoutIsAvailable(bool* available) const final { *available = false; return 0; }
    int32_t SetStereoPlayout(bool) final { return 0; }
    int32_t StereoPlayout(bool*) const final { return shouldNotBeCalled(-1); }
    int32_t StereoRecordingIsAvailable(bool* available) const final { *available = false; return 0;  }
    int32_t SetStereoRecording(bool) final { return 0;  }
    int32_t StereoRecording(bool*) const final { return shouldNotBeCalled(-1); }
    int32_t PlayoutDelay(uint16_t* delay) const final { *delay = 0; return 0; }
    bool BuiltInAECIsAvailable() const final { return false; }
    bool BuiltInAGCIsAvailable() const final { return false;  }
    bool BuiltInNSIsAvailable() const final { return false;  }
    int32_t EnableBuiltInAEC(bool) final { return shouldNotBeCalled(-1); }
    int32_t EnableBuiltInAGC(bool) final { return shouldNotBeCalled(-1); }
    int32_t EnableBuiltInNS(bool) final { return shouldNotBeCalled(-1); }

#if defined(WEBRTC_IOS)
    int GetPlayoutAudioParameters(webrtc::AudioParameters*) const final { return shouldNotBeCalled(-1); }
    int GetRecordAudioParameters(webrtc::AudioParameters*) const final { return shouldNotBeCalled(-1); }
#endif

private:
    void pollAudioData();
    void pollFromSource();

    Ref<WorkQueue> m_queue;
    bool m_isPlaying { false };
    webrtc::AudioTransport* m_audioTransport { nullptr };
    MonotonicTime m_pollingTime;
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
