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

#include "AudioSampleDataSource.h"
#include "BaseAudioSharedUnit.h"
#include "CAAudioStreamDescription.h"
#include "Timer.h"

typedef UInt32 AudioUnitPropertyID;
typedef UInt32 AudioUnitScope;
typedef UInt32 AudioUnitElement;
typedef UInt32 AudioUnitRenderActionFlags;

struct AudioBufferList;
struct AudioTimeStamp;

namespace WebCore {

#if PLATFORM(IOS_FAMILY)
class MediaCaptureStatusBarManager;
#endif

class CoreAudioSpeakerSamplesProducer;

class CoreAudioSharedUnit final : public BaseAudioSharedUnit {
public:
    class InternalUnit {
    public:
        virtual ~InternalUnit() = default;
        virtual OSStatus initialize() = 0;
        virtual OSStatus uninitialize() = 0;
        virtual OSStatus start() = 0;
        virtual OSStatus stop() = 0;
        virtual OSStatus set(AudioUnitPropertyID, AudioUnitScope, AudioUnitElement, const void*, UInt32) = 0;
        virtual OSStatus get(AudioUnitPropertyID, AudioUnitScope, AudioUnitElement, void*, UInt32*) = 0;
        virtual OSStatus render(AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*) = 0;
        virtual OSStatus defaultInputDevice(uint32_t*) = 0;
        virtual OSStatus defaultOutputDevice(uint32_t*) = 0;
        virtual void delaySamples(Seconds) { }
        virtual Seconds verifyCaptureInterval(bool isProducingSamples) const { return isProducingSamples ? 10_s : 2_s; }
    };

    WEBCORE_EXPORT static CoreAudioSharedUnit& unit();
    static BaseAudioSharedUnit& singleton()  { return unit(); }
    CoreAudioSharedUnit();
    ~CoreAudioSharedUnit();

    using CreationCallback = Function<Expected<UniqueRef<InternalUnit>, OSStatus>(bool enableEchoCancellation)>;
    void setInternalUnitCreationCallback(CreationCallback&& callback) { m_creationCallback = WTFMove(callback); }
    using GetSampleRateCallback = Function<int()>;
    void setInternalUnitGetSampleRateCallback(GetSampleRateCallback&& callback) { m_getSampleRateCallback = WTFMove(callback); }

    void registerSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer&);
    void unregisterSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer&);
    bool isRunning() const { return m_ioUnitStarted; }
    void setSampleRateRange(CapabilityValueOrRange range) { m_sampleRateCapabilities = range; }

#if PLATFORM(IOS_FAMILY)
    void setIsInBackground(bool);
    void setStatusBarWasTappedCallback(Function<void(CompletionHandler<void()>&&)>&& callback) { m_statusBarWasTappedCallback = WTFMove(callback); }
#endif

    bool isUsingVPIO() const { return m_shouldUseVPIO; }

private:
    static size_t preferredIOBufferSize();

    CapabilityValueOrRange sampleRateCapacities() const final { return m_sampleRateCapabilities; }

    bool hasAudioUnit() const final { return !!m_ioUnit; }
    void captureDeviceChanged() final;
    OSStatus reconfigureAudioUnit() final;
    void delaySamples(Seconds) final;

    OSStatus setupAudioUnit();
    void cleanupAudioUnit() final;

    OSStatus startInternal() final;
    void stopInternal() final;
    bool isProducingData() const final { return m_ioUnitStarted; }
    void isProducingMicrophoneSamplesChanged() final;
    void validateOutputDevice(uint32_t deviceID) final;
    int actualSampleRate() const final;
    void resetSampleRate();

    OSStatus configureSpeakerProc(int sampleRate);
    OSStatus configureMicrophoneProc(int sampleRate);

    static OSStatus microphoneCallback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
    OSStatus processMicrophoneSamples(AudioUnitRenderActionFlags&, const AudioTimeStamp&, UInt32, UInt32, AudioBufferList*);

    static OSStatus speakerCallback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
    OSStatus provideSpeakerData(AudioUnitRenderActionFlags&, const AudioTimeStamp&, UInt32, UInt32, AudioBufferList&);

    void unduck();

    void verifyIsCapturing();

    CreationCallback m_creationCallback;
    GetSampleRateCallback m_getSampleRateCallback;
    std::unique_ptr<InternalUnit> m_ioUnit;

    // Only read/modified from the IO thread.
    Vector<Ref<AudioSampleDataSource>> m_activeSources;

    std::optional<CAAudioStreamDescription> m_microphoneProcFormat;
    RefPtr<AudioSampleBufferList> m_microphoneSampleBuffer;
    double m_latestMicTimeStamp { 0 };

    std::optional<CAAudioStreamDescription> m_speakerProcFormat;

    double m_DTSConversionRatio { 0 };

    bool m_ioUnitInitialized { false };
    bool m_ioUnitStarted { false };

    mutable std::unique_ptr<RealtimeMediaSourceCapabilities> m_capabilities;
    mutable std::optional<RealtimeMediaSourceSettings> m_currentSettings;

#if !LOG_DISABLED
    void checkTimestamps(const AudioTimeStamp&, double);

    String m_ioUnitName;
#endif

    CapabilityValueOrRange m_sampleRateCapabilities;

    uint64_t m_microphoneProcsCalled { 0 };
    uint64_t m_microphoneProcsCalledLastTime { 0 };
    Timer m_verifyCapturingTimer;

    std::optional<size_t> m_minimumMicrophoneSampleFrames;
    bool m_isReconfiguring { false };
    bool m_shouldNotifySpeakerSamplesProducer { false };
    bool m_hasNotifiedSpeakerSamplesProducer { false };
    mutable Lock m_speakerSamplesProducerLock;
    CoreAudioSpeakerSamplesProducer* m_speakerSamplesProducer WTF_GUARDED_BY_LOCK(m_speakerSamplesProducerLock) { nullptr };

#if PLATFORM(IOS_FAMILY)
    std::unique_ptr<MediaCaptureStatusBarManager> m_statusBarManager;
    Function<void(CompletionHandler<void()>&&)> m_statusBarWasTappedCallback;
#endif

    bool m_shouldUseVPIO { true };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
