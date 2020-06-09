/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "RealtimeMediaSourceFactory.h"
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
class BaseAudioSharedUnit;
class CaptureDeviceInfo;
class WebAudioSourceProviderAVFObjC;

class CoreAudioCaptureSource : public RealtimeMediaSource {
public:
    static CaptureSourceOrError create(String&& deviceID, String&& hashSalt, const MediaConstraints*);
    static CaptureSourceOrError createForTesting(String&& deviceID, String&& label, String&& hashSalt, const MediaConstraints*, BaseAudioSharedUnit& overrideUnit);

    WEBCORE_EXPORT static AudioCaptureFactory& factory();

    CMClockRef timebaseClock();

protected:
    CoreAudioCaptureSource(String&& deviceID, String&& label, String&& hashSalt, uint32_t persistentID, BaseAudioSharedUnit* = nullptr);
    virtual ~CoreAudioCaptureSource();
    BaseAudioSharedUnit& unit();
    const BaseAudioSharedUnit& unit() const;

private:
    friend class BaseAudioSharedUnit;
    friend class CoreAudioSharedUnit;
    friend class CoreAudioCaptureSourceFactory;

    bool isCaptureSource() const final { return true; }
    void startProducingData() final;
    void stopProducingData() final;

    void delaySamples(Seconds) final;
    void setInterruptedForTesting(bool) final;

    Optional<Vector<int>> discreteSampleRates() const final { return { { 8000, 16000, 32000, 44100, 48000, 96000 } }; }

    const RealtimeMediaSourceCapabilities& capabilities() final;
    const RealtimeMediaSourceSettings& settings() final;
    void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) final;

    bool interrupted() const final;
    CaptureDevice::DeviceType deviceType() const final { return CaptureDevice::DeviceType::Microphone; }

    void initializeToStartProducingData();
    void audioUnitWillStart();

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const override { return "CoreAudioCaptureSource"; }
#endif

    uint32_t m_captureDeviceID { 0 };

    Optional<RealtimeMediaSourceCapabilities> m_capabilities;
    Optional<RealtimeMediaSourceSettings> m_currentSettings;

    bool m_isReadyToStart { false };
    
    BaseAudioSharedUnit* m_overrideUnit { nullptr };
};

class CoreAudioCaptureSourceFactory : public AudioCaptureFactory {
public:
    static CoreAudioCaptureSourceFactory& singleton();

    void beginInterruption();
    void endInterruption();
    void scheduleReconfiguration();

    void devicesChanged(const Vector<CaptureDevice>&);

private:
    CaptureSourceOrError createAudioCaptureSource(const CaptureDevice& device, String&& hashSalt, const MediaConstraints* constraints) final
    {
        return CoreAudioCaptureSource::create(String { device.persistentId() }, WTFMove(hashSalt), constraints);
    }

    CaptureDeviceManager& audioCaptureDeviceManager() final;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
