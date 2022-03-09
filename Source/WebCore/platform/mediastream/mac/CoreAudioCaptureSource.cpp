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

#include "config.h"
#include "CoreAudioCaptureSource.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioSampleBufferList.h"
#include "AudioSampleDataSource.h"
#include "AudioSession.h"
#include "BaseAudioSharedUnit.h"
#include "CoreAudioCaptureDevice.h"
#include "CoreAudioCaptureDeviceManager.h"
#include "Logging.h"
#include "PlatformMediaSessionManager.h"
#include "Timer.h"
#include "WebAudioSourceProviderCocoa.h"
#include <AudioToolbox/AudioConverter.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreMedia/CMSync.h>
#include <mach/mach_time.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/spi/cf/CoreAudioSPI.h>
#include <sys/time.h>
#include <wtf/Algorithms.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>

#include <pal/cf/AudioToolboxSoftLink.h>
#include <pal/cf/CoreMediaSoftLink.h>

#if PLATFORM(IOS_FAMILY)
#include "AVAudioSessionCaptureDevice.h"
#include "AVAudioSessionCaptureDeviceManager.h"
#include "CoreAudioCaptureSourceIOS.h"
#endif

namespace WebCore {

#if PLATFORM(MAC)
CoreAudioCaptureSourceFactory& CoreAudioCaptureSourceFactory::singleton()
{
    static NeverDestroyed<CoreAudioCaptureSourceFactory> factory;
    return factory.get();
}
#endif

const UInt32 outputBus = 0;
const UInt32 inputBus = 1;

class CoreAudioSharedUnit final : public BaseAudioSharedUnit {
public:
    static CoreAudioSharedUnit& unit();
    static BaseAudioSharedUnit& singleton()  { return unit(); }
    CoreAudioSharedUnit();

    void registerSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer&);
    void unregisterSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer&);
    bool isRunning() const { return m_ioUnitStarted; }

private:
    static size_t preferredIOBufferSize();

    CapabilityValueOrRange sampleRateCapacities() const final { return CapabilityValueOrRange(8000, 96000); }
    const CAAudioStreamDescription& microphoneFormat() const { return m_microphoneProcFormat; }

    bool hasAudioUnit() const final { return m_ioUnit; }
    void captureDeviceChanged() final;
    OSStatus reconfigureAudioUnit() final;

    OSStatus setupAudioUnit();
    void cleanupAudioUnit() final;

    OSStatus startInternal() final;
    void stopInternal() final;
    bool isProducingData() const final { return m_ioUnitStarted; }
    void isProducingMicrophoneSamplesChanged() final;
    void validateOutputDevice(uint32_t deviceID) final;

    OSStatus configureSpeakerProc();
    OSStatus configureMicrophoneProc();
    OSStatus defaultOutputDevice(uint32_t*);
    OSStatus defaultInputDevice(uint32_t*);

    static OSStatus microphoneCallback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
    OSStatus processMicrophoneSamples(AudioUnitRenderActionFlags&, const AudioTimeStamp&, UInt32, UInt32, AudioBufferList*);

    static OSStatus speakerCallback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
    OSStatus provideSpeakerData(AudioUnitRenderActionFlags&, const AudioTimeStamp&, UInt32, UInt32, AudioBufferList&);

    void unduck();

    void verifyIsCapturing();

    Seconds verifyCaptureInterval() { return isProducingMicrophoneSamples() ? 10_s : 2_s; }

    AudioUnit m_ioUnit { nullptr };

    // Only read/modified from the IO thread.
    Vector<Ref<AudioSampleDataSource>> m_activeSources;

    CAAudioStreamDescription m_microphoneProcFormat;
    RefPtr<AudioSampleBufferList> m_microphoneSampleBuffer;
    uint64_t m_latestMicTimeStamp { 0 };

    CAAudioStreamDescription m_speakerProcFormat;

    double m_DTSConversionRatio { 0 };

    bool m_ioUnitInitialized { false };
    bool m_ioUnitStarted { false };

    mutable std::unique_ptr<RealtimeMediaSourceCapabilities> m_capabilities;
    mutable std::optional<RealtimeMediaSourceSettings> m_currentSettings;

#if !LOG_DISABLED
    void checkTimestamps(const AudioTimeStamp&, uint64_t, double);

    String m_ioUnitName;
#endif

    uint64_t m_microphoneProcsCalled { 0 };
    uint64_t m_microphoneProcsCalledLastTime { 0 };
    Timer m_verifyCapturingTimer;

    bool m_isReconfiguring { false };
    Lock m_speakerSamplesProducerLock;
    CoreAudioSpeakerSamplesProducer* m_speakerSamplesProducer WTF_GUARDED_BY_LOCK(m_speakerSamplesProducerLock) { nullptr };
};

CoreAudioSharedUnit& CoreAudioSharedUnit::unit()
{
    static NeverDestroyed<CoreAudioSharedUnit> singleton;
    return singleton;
}

CoreAudioSharedUnit::CoreAudioSharedUnit()
    : m_verifyCapturingTimer(*this, &CoreAudioSharedUnit::verifyIsCapturing)
{
}

void CoreAudioSharedUnit::captureDeviceChanged()
{
#if PLATFORM(MAC)
    reconfigureAudioUnit();
#else
    AVAudioSessionCaptureDeviceManager::singleton().setPreferredAudioSessionDeviceUID(persistentID());
#endif
}

size_t CoreAudioSharedUnit::preferredIOBufferSize()
{
    return AudioSession::sharedSession().bufferSize();
}

OSStatus CoreAudioSharedUnit::setupAudioUnit()
{
    if (m_ioUnit)
        return 0;

    ASSERT(hasClients());

    mach_timebase_info_data_t timebaseInfo;
    mach_timebase_info(&timebaseInfo);
    m_DTSConversionRatio = 1e-9 * static_cast<double>(timebaseInfo.numer) / static_cast<double>(timebaseInfo.denom);

    AudioComponentDescription ioUnitDescription = { kAudioUnitType_Output, kAudioUnitSubType_VoiceProcessingIO, kAudioUnitManufacturer_Apple, 0, 0 };
    AudioComponent ioComponent = PAL::AudioComponentFindNext(nullptr, &ioUnitDescription);
    ASSERT(ioComponent);
    if (!ioComponent) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to find vpio unit component", this);
        return -1;
    }

#if !LOG_DISABLED
    CFStringRef name = nullptr;
    PAL::AudioComponentCopyName(ioComponent, &name);
    if (name) {
        m_ioUnitName = name;
        CFRelease(name);
        RELEASE_LOG(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) created \"%s\" component", this, m_ioUnitName.utf8().data());
    }
#endif

    auto err = PAL::AudioComponentInstanceNew(ioComponent, &m_ioUnit);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to open vpio unit, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    if (!enableEchoCancellation()) {
        uint32_t param = 0;
        err = PAL::AudioUnitSetProperty(m_ioUnit, kAUVoiceIOProperty_VoiceProcessingEnableAGC, kAudioUnitScope_Global, inputBus, &param, sizeof(param));
        if (err) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to set vpio automatic gain control, error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
        param = 1;
        err = PAL::AudioUnitSetProperty(m_ioUnit, kAUVoiceIOProperty_BypassVoiceProcessing, kAudioUnitScope_Global, inputBus, &param, sizeof(param));
        if (err) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to set vpio unit echo cancellation, error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }

#if PLATFORM(IOS_FAMILY)
    uint32_t param = 1;
    err = PAL::AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, inputBus, &param, sizeof(param));
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to enable vpio unit input, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
#else
    auto deviceID = captureDeviceID();
    if (!deviceID) {
        err = defaultInputDevice(&deviceID);
        if (err)
            return err;
    }

    err = PAL::AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, inputBus, &deviceID, sizeof(deviceID));
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to set vpio unit capture device ID %d, error %d (%.4s)", this, (int)deviceID, (int)err, (char*)&err);
        return err;
    }

    uint32_t defaultOutputDeviceID;
    err = defaultOutputDevice(&defaultOutputDeviceID);
    if (!err) {
        err = PAL::AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, outputBus, &defaultOutputDeviceID, sizeof(defaultOutputDeviceID));
        RELEASE_LOG_ERROR_IF(err, WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to set vpio unit output device ID %d, error %d (%.4s)", this, (int)defaultOutputDeviceID, (int)err, (char*)&err);
    }
    setOutputDeviceID(!err ? defaultOutputDeviceID : 0);
#endif

    err = configureMicrophoneProc();
    if (err)
        return err;

    err = configureSpeakerProc();
    if (err)
        return err;

    err = PAL::AudioUnitInitialize(m_ioUnit);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) AudioUnitInitialize() failed, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
    m_ioUnitInitialized = true;

    unduck();

    return err;
}

void CoreAudioSharedUnit::unduck()
{
    uint32_t outputDevice;
    if (!defaultOutputDevice(&outputDevice))
        AudioDeviceDuck(outputDevice, 1.0, nullptr, 0);
}

OSStatus CoreAudioSharedUnit::configureMicrophoneProc()
{
    if (!isProducingMicrophoneSamples())
        return noErr;

    AURenderCallbackStruct callback = { microphoneCallback, this };
    auto err = PAL::AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, inputBus, &callback, sizeof(callback));
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureMicrophoneProc(%p) unable to set vpio unit mic proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription microphoneProcFormat = { };

    UInt32 size = sizeof(microphoneProcFormat);
    err = PAL::AudioUnitGetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, inputBus, &microphoneProcFormat, &size);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureMicrophoneProc(%p) unable to get output stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    microphoneProcFormat.mSampleRate = sampleRate();
    err = PAL::AudioUnitSetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, inputBus, &microphoneProcFormat, size);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureMicrophoneProc(%p) unable to set output stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    m_microphoneSampleBuffer = AudioSampleBufferList::create(microphoneProcFormat, preferredIOBufferSize() * 2);
    m_microphoneProcFormat = microphoneProcFormat;

    return err;
}

OSStatus CoreAudioSharedUnit::configureSpeakerProc()
{
    AURenderCallbackStruct callback = { speakerCallback, this };
    auto err = PAL::AudioUnitSetProperty(m_ioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, outputBus, &callback, sizeof(callback));
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureSpeakerProc(%p) unable to set vpio unit speaker proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription speakerProcFormat;
    UInt32 size = sizeof(speakerProcFormat);
    {
        Locker locker { m_speakerSamplesProducerLock };
        if (m_speakerSamplesProducer)
            speakerProcFormat = m_speakerSamplesProducer->format().streamDescription();
        else {
            err = PAL::AudioUnitGetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, outputBus, &speakerProcFormat, &size);
            if (err) {
                RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
                return err;
            }
            speakerProcFormat.mSampleRate = sampleRate();
        }
    }

    err = PAL::AudioUnitSetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, outputBus, &speakerProcFormat, size);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    m_speakerProcFormat = speakerProcFormat;

    return err;
}

#if !LOG_DISABLED
void CoreAudioSharedUnit::checkTimestamps(const AudioTimeStamp& timeStamp, uint64_t sampleTime, double hostTime)
{
    if (!timeStamp.mSampleTime || sampleTime == m_latestMicTimeStamp || !hostTime)
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::checkTimestamps: unusual timestamps, sample time = %lld, previous sample time = %lld, hostTime %f", sampleTime, m_latestMicTimeStamp, hostTime);
}
#endif

OSStatus CoreAudioSharedUnit::provideSpeakerData(AudioUnitRenderActionFlags& flags, const AudioTimeStamp& timeStamp, UInt32 /*inBusNumber*/, UInt32 inNumberFrames, AudioBufferList& ioData)
{
    if (m_isReconfiguring || !m_speakerSamplesProducerLock.tryLock()) {
        AudioSampleBufferList::zeroABL(ioData, static_cast<size_t>(inNumberFrames * m_speakerProcFormat.bytesPerFrame()));
        flags = kAudioUnitRenderAction_OutputIsSilence;
        return noErr;
    }

    Locker locker { AdoptLock, m_speakerSamplesProducerLock };
    if (!m_speakerSamplesProducer) {
        AudioSampleBufferList::zeroABL(ioData, static_cast<size_t>(inNumberFrames * m_speakerProcFormat.bytesPerFrame()));
        flags = kAudioUnitRenderAction_OutputIsSilence;
        return noErr;
    }
    return m_speakerSamplesProducer->produceSpeakerSamples(inNumberFrames, ioData, timeStamp.mSampleTime, timeStamp.mHostTime, flags);
}

OSStatus CoreAudioSharedUnit::speakerCallback(void *inRefCon, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    ASSERT(ioActionFlags);
    ASSERT(inTimeStamp);
    auto dataSource = static_cast<CoreAudioSharedUnit*>(inRefCon);
    return dataSource->provideSpeakerData(*ioActionFlags, *inTimeStamp, inBusNumber, inNumberFrames, *ioData);
}

OSStatus CoreAudioSharedUnit::processMicrophoneSamples(AudioUnitRenderActionFlags& ioActionFlags, const AudioTimeStamp& timeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* /*ioData*/)
{
    ++m_microphoneProcsCalled;

    if (m_isReconfiguring)
        return false;

    // Pull through the vpio unit to our mic buffer.
    m_microphoneSampleBuffer->reset();
    AudioBufferList& bufferList = m_microphoneSampleBuffer->bufferList();
    auto err = AudioUnitRender(m_ioUnit, &ioActionFlags, &timeStamp, inBusNumber, inNumberFrames, &bufferList);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::processMicrophoneSamples(%p) AudioUnitRender failed with error %d (%.4s), bufferList size %d, inNumberFrames %d ", this, (int)err, (char*)&err, (int)bufferList.mBuffers[0].mDataByteSize, (int)inNumberFrames);
        if (err == kAudio_ParamError) {
            // Our buffer might be too small, the preferred buffer size or sample rate might have changed.
            callOnMainThread([] {
                CoreAudioSharedUnit::singleton().reconfigure();
            });
        }
        // We return early so that if this error happens, we do not increment m_microphoneProcsCalled and fail the capture once timer kicks in.
        return err;
    }

    if (!isProducingMicrophoneSamples())
        return noErr;

    double adjustedHostTime = m_DTSConversionRatio * timeStamp.mHostTime;
    uint64_t sampleTime = timeStamp.mSampleTime;
#if !LOG_DISABLED
    checkTimestamps(timeStamp, sampleTime, adjustedHostTime);
#endif
    m_latestMicTimeStamp = sampleTime;
    m_microphoneSampleBuffer->setTimes(adjustedHostTime, sampleTime);

    if (volume() != 1.0)
        m_microphoneSampleBuffer->applyGain(volume());

    audioSamplesAvailable(MediaTime(sampleTime, m_microphoneProcFormat.sampleRate()), m_microphoneSampleBuffer->bufferList(), m_microphoneProcFormat, inNumberFrames);
    return noErr;
}

OSStatus CoreAudioSharedUnit::microphoneCallback(void *inRefCon, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    ASSERT(ioActionFlags);
    ASSERT(inTimeStamp);
    CoreAudioSharedUnit* dataSource = static_cast<CoreAudioSharedUnit*>(inRefCon);
    return dataSource->processMicrophoneSamples(*ioActionFlags, *inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

void CoreAudioSharedUnit::cleanupAudioUnit()
{
    if (m_ioUnitInitialized) {
        ASSERT(m_ioUnit);
        auto err = AudioUnitUninitialize(m_ioUnit);
        if (err)
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::cleanupAudioUnit(%p) AudioUnitUninitialize failed with error %d (%.4s)", this, (int)err, (char*)&err);
        m_ioUnitInitialized = false;
    }

    if (m_ioUnit) {
        PAL::AudioComponentInstanceDispose(m_ioUnit);
        m_ioUnit = nullptr;
    }

    m_microphoneSampleBuffer = nullptr;
#if !LOG_DISABLED
    m_ioUnitName = emptyString();
#endif
}

OSStatus CoreAudioSharedUnit::reconfigureAudioUnit()
{
    ASSERT(isMainThread());
    OSStatus err;
    if (!hasAudioUnit())
        return 0;

    m_isReconfiguring = true;
    auto scope = makeScopeExit([this] { m_isReconfiguring = false; });

    if (m_ioUnitStarted) {
        err = PAL::AudioOutputUnitStop(m_ioUnit);
        if (err) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::reconfigureAudioUnit(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }

    cleanupAudioUnit();
    err = setupAudioUnit();
    if (err)
        return err;

    if (m_ioUnitStarted) {
        err = PAL::AudioOutputUnitStart(m_ioUnit);
        if (err) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::reconfigureAudioUnit(%p) AudioOutputUnitStart failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }
    return err;
}

OSStatus CoreAudioSharedUnit::startInternal()
{
    setIsProducingMicrophoneSamples(true);

    OSStatus err;
    if (!m_ioUnit) {
        err = setupAudioUnit();
        if (err) {
            cleanupAudioUnit();
            ASSERT(!m_ioUnit);
            return err;
        }
        ASSERT(m_ioUnit);
    }

    unduck();

    {
        Locker locker { m_speakerSamplesProducerLock };
        if (m_speakerSamplesProducer)
            m_speakerSamplesProducer->captureUnitIsStarting();
    }

    err = PAL::AudioOutputUnitStart(m_ioUnit);
    if (err) {
        {
            Locker locker { m_speakerSamplesProducerLock };
            if (m_speakerSamplesProducer)
                m_speakerSamplesProducer->captureUnitHasStopped();
        }

        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::start(%p) AudioOutputUnitStart failed with error %d (%.4s)", this, (int)err, (char*)&err);
        cleanupAudioUnit();
        ASSERT(!m_ioUnit);
        return err;
    }

    m_ioUnitStarted = true;

    m_verifyCapturingTimer.startRepeating(verifyCaptureInterval());
    m_microphoneProcsCalled = 0;
    m_microphoneProcsCalledLastTime = 0;

    return 0;
}

void CoreAudioSharedUnit::isProducingMicrophoneSamplesChanged()
{
    if (!isProducingData())
        return;
    m_verifyCapturingTimer.startRepeating(verifyCaptureInterval());
}

void CoreAudioSharedUnit::validateOutputDevice(uint32_t currentOutputDeviceID)
{
#if PLATFORM(MAC)
    uint32_t currentDefaultOutputDeviceID = 0;
    if (auto err = defaultOutputDevice(&currentDefaultOutputDeviceID))
        return;

    if (!currentDefaultOutputDeviceID || currentOutputDeviceID == currentDefaultOutputDeviceID)
        return;

    reconfigure();
#else
    UNUSED_PARAM(currentOutputDeviceID);
#endif
}

void CoreAudioSharedUnit::verifyIsCapturing()
{
    if (m_microphoneProcsCalledLastTime != m_microphoneProcsCalled) {
        m_microphoneProcsCalledLastTime = m_microphoneProcsCalled;
        return;
    }

    RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::verifyIsCapturing - no audio received in %d seconds, failing", static_cast<int>(m_verifyCapturingTimer.repeatInterval().value()));
    captureFailed();
}

void CoreAudioSharedUnit::stopInternal()
{
    m_verifyCapturingTimer.stop();

    if (!m_ioUnit || !m_ioUnitStarted)
        return;

    auto err = PAL::AudioOutputUnitStop(m_ioUnit);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::stop(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return;
    }
    {
        Locker locker { m_speakerSamplesProducerLock };
        if (m_speakerSamplesProducer)
            m_speakerSamplesProducer->captureUnitHasStopped();
    }

    m_ioUnitStarted = false;
}

OSStatus CoreAudioSharedUnit::defaultInputDevice(uint32_t* deviceID)
{
    ASSERT(m_ioUnit);

    UInt32 propertySize = sizeof(*deviceID);
    auto err = PAL::AudioUnitGetProperty(m_ioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, inputBus, deviceID, &propertySize);
    if (err)
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::defaultInputDevice(%p) unable to get default input device ID, error %d (%.4s)", this, (int)err, (char*)&err);

    return err;
}

OSStatus CoreAudioSharedUnit::defaultOutputDevice(uint32_t* deviceID)
{
    OSErr err = -1;
#if PLATFORM(MAC)
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
#if HAVE(AUDIO_OBJECT_PROPERTY_ELEMENT_MAIN)
        kAudioObjectPropertyElementMain
#else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        kAudioObjectPropertyElementMaster
        ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    };

    if (AudioObjectHasProperty(kAudioObjectSystemObject, &address)) {
        UInt32 propertySize = sizeof(AudioDeviceID);
        err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &propertySize, deviceID);
    }
#else
    UNUSED_PARAM(deviceID);
#endif
    return err;
}

void CoreAudioSharedUnit::registerSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer& producer)
{
    setIsRenderingAudio(true);

    CoreAudioSpeakerSamplesProducer* oldProducer;
    {
        Locker locker { m_speakerSamplesProducerLock };
        oldProducer = m_speakerSamplesProducer;
        m_speakerSamplesProducer = &producer;
    }
    if (oldProducer && oldProducer != &producer)
        oldProducer->captureUnitHasStopped();

    if (hasAudioUnit() && producer.format() != m_speakerProcFormat)
        reconfigure();
}

void CoreAudioSharedUnit::unregisterSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer& producer)
{
    {
        Locker locker { m_speakerSamplesProducerLock };
        if (m_speakerSamplesProducer != &producer)
            return;
        m_speakerSamplesProducer = nullptr;
    }

    setIsRenderingAudio(false);
}

static CaptureSourceOrError initializeCoreAudioCaptureSource(Ref<CoreAudioCaptureSource>&& source, const MediaConstraints* constraints)
{
    if (constraints) {
        if (auto result = source->applyConstraints(*constraints))
            return WTFMove(result->badConstraint);
    }
    return CaptureSourceOrError(WTFMove(source));
}

CaptureSourceOrError CoreAudioCaptureSource::create(String&& deviceID, String&& hashSalt, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
#if PLATFORM(MAC)
    auto device = CoreAudioCaptureDeviceManager::singleton().coreAudioDeviceWithUID(deviceID);
    if (!device)
        return { "No CoreAudioCaptureSource device"_s };

    auto source = adoptRef(*new CoreAudioCaptureSource(WTFMove(deviceID), String { device->label() }, WTFMove(hashSalt), device->deviceID(), nullptr, pageIdentifier));
#elif PLATFORM(IOS_FAMILY)
    auto device = AVAudioSessionCaptureDeviceManager::singleton().audioSessionDeviceWithUID(WTFMove(deviceID));
    if (!device)
        return { "No AVAudioSessionCaptureDevice device"_s };

    auto source = adoptRef(*new CoreAudioCaptureSource(WTFMove(deviceID), String { device->label() }, WTFMove(hashSalt), 0, nullptr, pageIdentifier));
#endif
    return initializeCoreAudioCaptureSource(WTFMove(source), constraints);
}

CaptureSourceOrError CoreAudioCaptureSource::createForTesting(String&& deviceID, String&& label, String&& hashSalt, const MediaConstraints* constraints, BaseAudioSharedUnit& overrideUnit, PageIdentifier pageIdentifier)
{
    auto source = adoptRef(*new CoreAudioCaptureSource(WTFMove(deviceID), WTFMove(label), WTFMove(hashSalt), 0, &overrideUnit, pageIdentifier));
    return initializeCoreAudioCaptureSource(WTFMove(source), constraints);
}

BaseAudioSharedUnit& CoreAudioCaptureSource::unit()
{
    return m_overrideUnit ? *m_overrideUnit : CoreAudioSharedUnit::singleton();
}

const BaseAudioSharedUnit& CoreAudioCaptureSource::unit() const
{
    return m_overrideUnit ? *m_overrideUnit : CoreAudioSharedUnit::singleton();
}

void CoreAudioCaptureSourceFactory::beginInterruption()
{
    ensureOnMainThread([] {
        CoreAudioSharedUnit::singleton().suspend();
    });
}

void CoreAudioCaptureSourceFactory::endInterruption()
{
    ensureOnMainThread([] {
        CoreAudioSharedUnit::singleton().resume();
    });
}

void CoreAudioCaptureSourceFactory::scheduleReconfiguration()
{
    ensureOnMainThread([] {
        CoreAudioSharedUnit::singleton().reconfigure();
    });
}

AudioCaptureFactory& CoreAudioCaptureSource::factory()
{
    return CoreAudioCaptureSourceFactory::singleton();
}

CaptureDeviceManager& CoreAudioCaptureSourceFactory::audioCaptureDeviceManager()
{
#if PLATFORM(MAC)
    return CoreAudioCaptureDeviceManager::singleton();
#else
    return AVAudioSessionCaptureDeviceManager::singleton();
#endif
}

const Vector<CaptureDevice>& CoreAudioCaptureSourceFactory::speakerDevices() const
{
#if PLATFORM(MAC)
    return CoreAudioCaptureDeviceManager::singleton().speakerDevices();
#else
    return AVAudioSessionCaptureDeviceManager::singleton().speakerDevices();
#endif
}

void CoreAudioCaptureSourceFactory::devicesChanged(const Vector<CaptureDevice>& devices)
{
    CoreAudioSharedUnit::unit().devicesChanged(devices);
}

void CoreAudioCaptureSourceFactory::registerSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer& producer)
{
    CoreAudioSharedUnit::unit().registerSpeakerSamplesProducer(producer);
}

void CoreAudioCaptureSourceFactory::unregisterSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer& producer)
{
    CoreAudioSharedUnit::unit().unregisterSpeakerSamplesProducer(producer);
}

bool CoreAudioCaptureSourceFactory::isAudioCaptureUnitRunning()
{
    return CoreAudioSharedUnit::unit().isRunning();
}

void CoreAudioCaptureSourceFactory::whenAudioCaptureUnitIsNotRunning(Function<void()>&& callback)
{
    return CoreAudioSharedUnit::unit().whenAudioCaptureUnitIsNotRunning(WTFMove(callback));
}

CoreAudioCaptureSource::CoreAudioCaptureSource(String&& deviceID, String&& label, String&& hashSalt, uint32_t captureDeviceID, BaseAudioSharedUnit* overrideUnit, PageIdentifier pageIdentifier)
    : RealtimeMediaSource(RealtimeMediaSource::Type::Audio, WTFMove(label), WTFMove(deviceID), WTFMove(hashSalt), pageIdentifier)
    , m_captureDeviceID(captureDeviceID)
    , m_overrideUnit(overrideUnit)
{
    auto& unit = this->unit();

    // We ensure that we unsuspend ourselves on the constructor as a capture source
    // is created when getUserMedia grants access which only happens when the process is foregrounded.
    // We also reset unit capture values to default.
    unit.prepareForNewCapture();

    initializeEchoCancellation(unit.enableEchoCancellation());
    initializeSampleRate(unit.sampleRate());
    initializeVolume(unit.volume());
}

void CoreAudioCaptureSource::initializeToStartProducingData()
{
    if (m_isReadyToStart)
        return;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);
    m_isReadyToStart = true;

    auto& unit = this->unit();
    unit.setCaptureDevice(String { persistentID() }, m_captureDeviceID);

    bool shouldReconfigure = echoCancellation() != unit.enableEchoCancellation() || sampleRate() != unit.sampleRate() || volume() != unit.volume();
    unit.setEnableEchoCancellation(echoCancellation());
    unit.setSampleRate(sampleRate());
    unit.setVolume(volume());

    unit.addClient(*this);

    if (shouldReconfigure)
        unit.reconfigure();
}

CoreAudioCaptureSource::~CoreAudioCaptureSource()
{
    unit().removeClient(*this);
}

void CoreAudioCaptureSource::startProducingData()
{
    initializeToStartProducingData();
    unit().startProducingData();
}

void CoreAudioCaptureSource::stopProducingData()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);
    unit().stopProducingData();
}

const RealtimeMediaSourceCapabilities& CoreAudioCaptureSource::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());
        capabilities.setDeviceId(hashedId());
        capabilities.setEchoCancellation(RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite);
        capabilities.setVolume(CapabilityValueOrRange(0.0, 1.0));
        capabilities.setSampleRate(unit().sampleRateCapacities());
        m_capabilities = WTFMove(capabilities);
    }
    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& CoreAudioCaptureSource::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setVolume(volume());
        settings.setSampleRate(sampleRate());
        settings.setDeviceId(hashedId());
        settings.setLabel(name());
        settings.setEchoCancellation(echoCancellation());

        RealtimeMediaSourceSupportedConstraints supportedConstraints;
        supportedConstraints.setSupportsDeviceId(true);
        supportedConstraints.setSupportsEchoCancellation(true);
        supportedConstraints.setSupportsVolume(true);
        supportedConstraints.setSupportsSampleRate(true);
        settings.setSupportedConstraints(supportedConstraints);

        m_currentSettings = WTFMove(settings);
    }
    return m_currentSettings.value();
}

void CoreAudioCaptureSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    bool shouldReconfigure = false;
    if (settings.contains(RealtimeMediaSourceSettings::Flag::EchoCancellation)) {
        unit().setEnableEchoCancellation(echoCancellation());
        shouldReconfigure = true;
    }
    if (settings.contains(RealtimeMediaSourceSettings::Flag::SampleRate)) {
        unit().setSampleRate(sampleRate());
        shouldReconfigure = true;
    }
    if (shouldReconfigure)
        unit().reconfigure();

    m_currentSettings = std::nullopt;
}

bool CoreAudioCaptureSource::interrupted() const
{
    return unit().isSuspended() ? true : RealtimeMediaSource::interrupted();
}

void CoreAudioCaptureSource::delaySamples(Seconds seconds)
{
    unit().delaySamples(seconds);
}

void CoreAudioCaptureSource::audioUnitWillStart()
{
    forEachObserver([](auto& observer) {
        observer.audioUnitWillStart();
    });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
