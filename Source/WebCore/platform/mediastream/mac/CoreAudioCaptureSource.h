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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "CAAudioStreamDescription.h"
#include "CaptureDevice.h"
#include "RealtimeMediaSource.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

typedef struct OpaqueCMClock* CMClockRef;

namespace WTF {
class MediaTime;
}

namespace WebCore {

class AudioSampleBufferList;
class AudioSampleDataSource;
class CaptureDeviceInfo;
class WebAudioSourceProviderAVFObjC;

class CoreAudioCaptureSource final : public RealtimeMediaSource {
public:

    static CaptureSourceOrError create(const String& deviceID, const MediaConstraints*);

    WEBCORE_EXPORT static AudioCaptureFactory& factory();

    void addEchoCancellationSource(AudioSampleDataSource&);
    void removeEchoCancellationSource(AudioSampleDataSource&);

    using MicrophoneDataCallback = std::function<void(const MediaTime& sampleTime, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t sampleCount)>;

    uint64_t addMicrophoneDataConsumer(MicrophoneDataCallback&&);
    void removeMicrophoneDataConsumer(uint64_t);

    CMClockRef timebaseClock();

private:
    CoreAudioCaptureSource(const String& deviceID, const String& label, uint32_t persistentID);
    virtual ~CoreAudioCaptureSource();

    friend class CoreAudioSharedUnit;
    friend class CoreAudioCaptureSourceFactory;

    void scheduleReconfiguration();

    bool isCaptureSource() const final { return true; }
    void startProducingData() final;
    void stopProducingData() final;

    bool applyVolume(double) final { return true; }
    bool applySampleRate(int) final;
    bool applyEchoCancellation(bool) final;

    const RealtimeMediaSourceCapabilities& capabilities() const final;
    const RealtimeMediaSourceSettings& settings() const final;
    void settingsDidChange() final;
    AudioSourceProvider* audioSourceProvider() final;

    uint32_t m_captureDeviceID { 0 };

    bool m_isSuspended { false };

    mutable std::optional<RealtimeMediaSourceCapabilities> m_capabilities;
    mutable std::optional<RealtimeMediaSourceSettings> m_currentSettings;

    RefPtr<WebAudioSourceProviderAVFObjC> m_audioSourceProvider;
    bool m_reconfigurationOngoing { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
