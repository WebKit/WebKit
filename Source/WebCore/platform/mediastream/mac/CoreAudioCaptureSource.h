/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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

#include "AudioSession.h"
#include "CAAudioStreamDescription.h"
#include "CaptureDevice.h"
#include "RealtimeMediaSource.h"
#include "RealtimeMediaSourceFactory.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudioTypes.h>
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
    WEBCORE_EXPORT static CaptureSourceOrError create(String&& deviceID, MediaDeviceHashSalts&&, const MediaConstraints*, PageIdentifier);
    static CaptureSourceOrError createForTesting(String&& deviceID, AtomString&& label, MediaDeviceHashSalts&&, const MediaConstraints*, BaseAudioSharedUnit& overrideUnit, PageIdentifier);

    WEBCORE_EXPORT static AudioCaptureFactory& factory();

    CMClockRef timebaseClock();

    void handleNewCurrentMicrophoneDevice(const CaptureDevice&);

protected:
    CoreAudioCaptureSource(const CaptureDevice&, uint32_t, MediaDeviceHashSalts&&, BaseAudioSharedUnit*, PageIdentifier);
    virtual ~CoreAudioCaptureSource();
    BaseAudioSharedUnit& unit();
    const BaseAudioSharedUnit& unit() const;

    bool canResumeAfterInterruption() const { return m_canResumeAfterInterruption; }
    void setCanResumeAfterInterruption(bool value) { m_canResumeAfterInterruption = value; }

private:
    friend class BaseAudioSharedUnit;
    friend class CoreAudioSharedUnit;
    friend class CoreAudioCaptureSourceFactory;

    bool isCaptureSource() const final { return true; }
    void startProducingData() final;
    void stopProducingData() final;

    void delaySamples(Seconds) final;
#if PLATFORM(IOS_FAMILY)
    void setIsInBackground(bool) final;
#endif

    std::optional<Vector<int>> discreteSampleRates() const final { return { { 8000, 16000, 32000, 44100, 48000, 96000 } }; }

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

    std::optional<RealtimeMediaSourceCapabilities> m_capabilities;
    std::optional<RealtimeMediaSourceSettings> m_currentSettings;

    bool m_canResumeAfterInterruption { true };
    bool m_isReadyToStart { false };
    
    BaseAudioSharedUnit* m_overrideUnit { nullptr };
};

class CoreAudioSpeakerSamplesProducer {
public:
    virtual ~CoreAudioSpeakerSamplesProducer() = default;
    // Main thread
    virtual const CAAudioStreamDescription& format() = 0;
    virtual void captureUnitIsStarting() = 0;
    virtual void captureUnitHasStopped() = 0;
    // Background thread.
    virtual OSStatus produceSpeakerSamples(size_t sampleCount, AudioBufferList&, uint64_t sampleTime, double hostTime, AudioUnitRenderActionFlags&) = 0;
};

class CoreAudioCaptureSourceFactory : public AudioCaptureFactory, public AudioSession::InterruptionObserver {
public:
    WEBCORE_EXPORT static CoreAudioCaptureSourceFactory& singleton();

    CoreAudioCaptureSourceFactory();
    ~CoreAudioCaptureSourceFactory();

    void scheduleReconfiguration();

    void devicesChanged(const Vector<CaptureDevice>&);

    WEBCORE_EXPORT void registerSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer&);
    WEBCORE_EXPORT void unregisterSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer&);
    WEBCORE_EXPORT bool isAudioCaptureUnitRunning();
    WEBCORE_EXPORT void whenAudioCaptureUnitIsNotRunning(Function<void()>&&);
    WEBCORE_EXPORT bool shouldAudioCaptureUnitRenderAudio();

    void setOverrideUnit(BaseAudioSharedUnit* unit) { m_overrideUnit = unit; }
    BaseAudioSharedUnit& unit();

private:
    // AudioSession::InterruptionObserver.
    void beginAudioSessionInterruption() final { beginInterruption(); }
    void endAudioSessionInterruption(AudioSession::MayResume) final { endInterruption(); }

    // AudioCaptureFactory
    CaptureSourceOrError createAudioCaptureSource(const CaptureDevice&, MediaDeviceHashSalts&&, const MediaConstraints*, PageIdentifier) override;
    CaptureDeviceManager& audioCaptureDeviceManager() final;
    const Vector<CaptureDevice>& speakerDevices() const final;

    void beginInterruption();
    void endInterruption();

    BaseAudioSharedUnit* m_overrideUnit { nullptr };
};

inline CaptureSourceOrError CoreAudioCaptureSourceFactory::createAudioCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
    return CoreAudioCaptureSource::create(String { device.persistentId() }, WTFMove(hashSalts), constraints, pageIdentifier);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
