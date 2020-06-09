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
#include "WebAudioSourceProviderAVFObjC.h"
#include <AudioToolbox/AudioConverter.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreMedia/CMSync.h>
#include <mach/mach_time.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/spi/cf/CoreAudioSPI.h>
#include <sys/time.h>
#include <wtf/Algorithms.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <pal/cf/CoreMediaSoftLink.h>

#if PLATFORM(IOS_FAMILY)
#include "AVAudioSessionCaptureDevice.h"
#include "AVAudioSessionCaptureDeviceManager.h"
#include "CoreAudioCaptureSourceIOS.h"
#endif

namespace WebCore {
using namespace PAL;

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

    void devicesChanged(const Vector<CaptureDevice>&);

private:
    static size_t preferredIOBufferSize();

    CapabilityValueOrRange sampleRateCapacities() const final { return CapabilityValueOrRange(8000, 96000); }
    const CAAudioStreamDescription& microphoneFormat() const { return m_microphoneProcFormat; }

    bool hasAudioUnit() const final { return m_ioUnit; }
    void setCaptureDevice(String&&, uint32_t) final;
    OSStatus reconfigureAudioUnit() final;

    OSStatus setupAudioUnit();
    void cleanupAudioUnit() final;

    OSStatus startInternal() final;
    void stopInternal() final;
    bool isProducingData() const final { return m_ioUnitStarted; }

    OSStatus configureSpeakerProc();
    OSStatus configureMicrophoneProc();
    OSStatus defaultOutputDevice(uint32_t*);
    OSStatus defaultInputDevice(uint32_t*);

    static OSStatus microphoneCallback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
    OSStatus processMicrophoneSamples(AudioUnitRenderActionFlags&, const AudioTimeStamp&, UInt32, UInt32, AudioBufferList*);

    static OSStatus speakerCallback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
    OSStatus provideSpeakerData(AudioUnitRenderActionFlags&, const AudioTimeStamp&, UInt32, UInt32, AudioBufferList*);

    void unduck();

    void verifyIsCapturing();
    void devicesChanged();

    AudioUnit m_ioUnit { nullptr };

    // Only read/modified from the IO thread.
    Vector<Ref<AudioSampleDataSource>> m_activeSources;

#if PLATFORM(MAC)
    uint32_t m_captureDeviceID { 0 };
#endif

    CAAudioStreamDescription m_microphoneProcFormat;
    RefPtr<AudioSampleBufferList> m_microphoneSampleBuffer;
    uint64_t m_latestMicTimeStamp { 0 };

    CAAudioStreamDescription m_speakerProcFormat;
    RefPtr<AudioSampleBufferList> m_speakerSampleBuffer;

    double m_DTSConversionRatio { 0 };

    bool m_ioUnitInitialized { false };
    bool m_ioUnitStarted { false };

    mutable std::unique_ptr<RealtimeMediaSourceCapabilities> m_capabilities;
    mutable Optional<RealtimeMediaSourceSettings> m_currentSettings;

#if !LOG_DISABLED
    void checkTimestamps(const AudioTimeStamp&, uint64_t, double);

    String m_ioUnitName;
    uint64_t m_speakerProcsCalled { 0 };
#endif

    String m_persistentID;

    uint64_t m_microphoneProcsCalled { 0 };
    uint64_t m_microphoneProcsCalledLastTime { 0 };
    Timer m_verifyCapturingTimer;
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

void CoreAudioSharedUnit::setCaptureDevice(String&& persistentID, uint32_t captureDeviceID)
{
    m_persistentID = WTFMove(persistentID);

#if PLATFORM(MAC)
    if (m_captureDeviceID == captureDeviceID)
        return;

    m_captureDeviceID = captureDeviceID;
    reconfigureAudioUnit();
#else
    UNUSED_PARAM(captureDeviceID);
#endif
}

void CoreAudioSharedUnit::devicesChanged(const Vector<CaptureDevice>& devices)
{
    if (!m_ioUnit)
        return;

    if (WTF::anyOf(devices, [this] (auto& device) { return m_persistentID == device.persistentId(); }))
        return;

    captureFailed();
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
    AudioComponent ioComponent = AudioComponentFindNext(nullptr, &ioUnitDescription);
    ASSERT(ioComponent);
    if (!ioComponent) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to find vpio unit component", this);
        return -1;
    }

#if !LOG_DISABLED
    CFStringRef name = nullptr;
    AudioComponentCopyName(ioComponent, &name);
    if (name) {
        m_ioUnitName = name;
        CFRelease(name);
        RELEASE_LOG(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) created \"%s\" component", this, m_ioUnitName.utf8().data());
    }
#endif

    auto err = AudioComponentInstanceNew(ioComponent, &m_ioUnit);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to open vpio unit, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    if (!enableEchoCancellation()) {
        uint32_t param = 0;
        err = AudioUnitSetProperty(m_ioUnit, kAUVoiceIOProperty_VoiceProcessingEnableAGC, kAudioUnitScope_Global, inputBus, &param, sizeof(param));
        if (err) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to set vpio automatic gain control, error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
        param = 1;
        err = AudioUnitSetProperty(m_ioUnit, kAUVoiceIOProperty_BypassVoiceProcessing, kAudioUnitScope_Global, inputBus, &param, sizeof(param));
        if (err) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to set vpio unit echo cancellation, error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }

#if PLATFORM(IOS_FAMILY)
    uint32_t param = 1;
    err = AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, inputBus, &param, sizeof(param));
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to enable vpio unit input, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
#else
    if (!m_captureDeviceID) {
        err = defaultInputDevice(&m_captureDeviceID);
        if (err)
            return err;
    }

    err = AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, inputBus, &m_captureDeviceID, sizeof(m_captureDeviceID));
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to set vpio unit capture device ID %d, error %d (%.4s)", this, (int)m_captureDeviceID, (int)err, (char*)&err);
        return err;
    }
#endif

    err = configureMicrophoneProc();
    if (err)
        return err;

    err = configureSpeakerProc();
    if (err)
        return err;

    err = AudioUnitInitialize(m_ioUnit);
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
    AURenderCallbackStruct callback = { microphoneCallback, this };
    auto err = AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, inputBus, &callback, sizeof(callback));
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureMicrophoneProc(%p) unable to set vpio unit mic proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription microphoneProcFormat = { };

    UInt32 size = sizeof(microphoneProcFormat);
    err = AudioUnitGetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, inputBus, &microphoneProcFormat, &size);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureMicrophoneProc(%p) unable to get output stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    microphoneProcFormat.mSampleRate = sampleRate();
    err = AudioUnitSetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, inputBus, &microphoneProcFormat, size);
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
    auto err = AudioUnitSetProperty(m_ioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, outputBus, &callback, sizeof(callback));
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureSpeakerProc(%p) unable to set vpio unit speaker proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription speakerProcFormat = { };

    UInt32 size = sizeof(speakerProcFormat);
    err = AudioUnitGetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, outputBus, &speakerProcFormat, &size);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    speakerProcFormat.mSampleRate = sampleRate();
    err = AudioUnitSetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, outputBus, &speakerProcFormat, size);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    m_speakerSampleBuffer = AudioSampleBufferList::create(speakerProcFormat, preferredIOBufferSize() * 2);
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

OSStatus CoreAudioSharedUnit::provideSpeakerData(AudioUnitRenderActionFlags& /*ioActionFlags*/, const AudioTimeStamp& timeStamp, UInt32 /*inBusNumber*/, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    // Called when the audio unit needs data to play through the speakers.
#if !LOG_DISABLED
    ++m_speakerProcsCalled;
#endif

    if (m_speakerSampleBuffer->sampleCapacity() < inNumberFrames) {
        if (m_activeSources.isEmpty())
            return 0;
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::provideSpeakerData: speaker sample buffer size (%d) too small for amount of sample data requested (%d)!", m_speakerSampleBuffer->sampleCapacity(), (int)inNumberFrames);
        // FIXME: This fails the capture, we should thus either reconfigure the audio unit or notify all clients that capture is failing.
        return kAudio_ParamError;
    }

    if (m_activeSources.isEmpty())
        return 0;

    double adjustedHostTime = m_DTSConversionRatio * timeStamp.mHostTime;
    uint64_t sampleTime = timeStamp.mSampleTime;
#if !LOG_DISABLED
    checkTimestamps(timeStamp, sampleTime, adjustedHostTime);
#endif
    m_speakerSampleBuffer->setTimes(adjustedHostTime, sampleTime);

    AudioBufferList& bufferList = m_speakerSampleBuffer->bufferList();
    for (uint32_t i = 0; i < bufferList.mNumberBuffers; ++i)
        bufferList.mBuffers[i] = ioData->mBuffers[i];

    bool firstSource = true;
    for (auto& source : m_activeSources) {
        source->pullSamples(*m_speakerSampleBuffer.get(), inNumberFrames, adjustedHostTime, sampleTime, firstSource ? AudioSampleDataSource::Copy : AudioSampleDataSource::Mix);
        firstSource = false;
    }

    return noErr;
}

OSStatus CoreAudioSharedUnit::speakerCallback(void *inRefCon, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    ASSERT(ioActionFlags);
    ASSERT(inTimeStamp);
    auto dataSource = static_cast<CoreAudioSharedUnit*>(inRefCon);
    return dataSource->provideSpeakerData(*ioActionFlags, *inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

OSStatus CoreAudioSharedUnit::processMicrophoneSamples(AudioUnitRenderActionFlags& ioActionFlags, const AudioTimeStamp& timeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* /*ioData*/)
{
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

    ++m_microphoneProcsCalled;

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
        AudioComponentInstanceDispose(m_ioUnit);
        m_ioUnit = nullptr;
    }

    m_microphoneSampleBuffer = nullptr;
    m_speakerSampleBuffer = nullptr;
#if !LOG_DISABLED
    m_ioUnitName = emptyString();
#endif
}

OSStatus CoreAudioSharedUnit::reconfigureAudioUnit()
{
    OSStatus err;
    if (!hasAudioUnit())
        return 0;

    if (m_ioUnitStarted) {
        err = AudioOutputUnitStop(m_ioUnit);
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
        err = AudioOutputUnitStart(m_ioUnit);
        if (err) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::reconfigureAudioUnit(%p) AudioOutputUnitStart failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }
    return err;
}

OSStatus CoreAudioSharedUnit::startInternal()
{
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

    err = AudioOutputUnitStart(m_ioUnit);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::start(%p) AudioOutputUnitStart failed with error %d (%.4s)", this, (int)err, (char*)&err);
        cleanupAudioUnit();
        ASSERT(!m_ioUnit);
        return err;
    }

    m_ioUnitStarted = true;

    m_verifyCapturingTimer.startRepeating(10_s);
    m_microphoneProcsCalled = 0;
    m_microphoneProcsCalledLastTime = 0;

    return 0;
}

void CoreAudioSharedUnit::verifyIsCapturing()
{
    if (m_microphoneProcsCalledLastTime != m_microphoneProcsCalled) {
        m_microphoneProcsCalledLastTime = m_microphoneProcsCalled;
        if (m_verifyCapturingTimer.repeatInterval() == 10_s)
            m_verifyCapturingTimer.startRepeating(2_s);
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

    auto err = AudioOutputUnitStop(m_ioUnit);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::stop(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return;
    }

    m_ioUnitStarted = false;
}

OSStatus CoreAudioSharedUnit::defaultInputDevice(uint32_t* deviceID)
{
    ASSERT(m_ioUnit);

    UInt32 propertySize = sizeof(*deviceID);
    auto err = AudioUnitGetProperty(m_ioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, inputBus, deviceID, &propertySize);
    if (err)
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::defaultInputDevice(%p) unable to get default input device ID, error %d (%.4s)", this, (int)err, (char*)&err);

    return err;
}

OSStatus CoreAudioSharedUnit::defaultOutputDevice(uint32_t* deviceID)
{
    OSErr err = -1;
#if PLATFORM(MAC)
    AudioObjectPropertyAddress address = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };

    if (AudioObjectHasProperty(kAudioObjectSystemObject, &address)) {
        UInt32 propertySize = sizeof(AudioDeviceID);
        err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &propertySize, deviceID);
    }
#else
    UNUSED_PARAM(deviceID);
#endif
    return err;
}

static CaptureSourceOrError initializeCoreAudioCaptureSource(Ref<CoreAudioCaptureSource>&& source, const MediaConstraints* constraints)
{
    if (constraints) {
        if (auto result = source->applyConstraints(*constraints))
            return WTFMove(result->badConstraint);
    }
    return CaptureSourceOrError(WTFMove(source));
}

CaptureSourceOrError CoreAudioCaptureSource::create(String&& deviceID, String&& hashSalt, const MediaConstraints* constraints)
{
#if PLATFORM(MAC)
    auto device = CoreAudioCaptureDeviceManager::singleton().coreAudioDeviceWithUID(deviceID);
    if (!device)
        return { "No CoreAudioCaptureSource device"_s };

    auto source = adoptRef(*new CoreAudioCaptureSource(WTFMove(deviceID), String { device->label() }, WTFMove(hashSalt), device->deviceID()));
#elif PLATFORM(IOS_FAMILY)
    auto device = AVAudioSessionCaptureDeviceManager::singleton().audioSessionDeviceWithUID(WTFMove(deviceID));
    if (!device)
        return { };

    auto source = adoptRef(*new CoreAudioCaptureSource(WTFMove(deviceID), String { device->label() }, WTFMove(hashSalt), 0));

    // We ensure that we unsuspend ourselves on the constructor as a capture source
    // is created when getUserMedia grants access which only happens when the process is foregrounded.
    source->unit().prepareForNewCapture();
#endif
    return initializeCoreAudioCaptureSource(WTFMove(source), constraints);
}

CaptureSourceOrError CoreAudioCaptureSource::createForTesting(String&& deviceID, String&& label, String&& hashSalt, const MediaConstraints* constraints, BaseAudioSharedUnit& overrideUnit)
{
    auto source = adoptRef(*new CoreAudioCaptureSource(WTFMove(deviceID), WTFMove(label), WTFMove(hashSalt), 0, &overrideUnit));
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
    if (!isMainThread()) {
        callOnMainThread([this] {
            beginInterruption();
        });
        return;
    }
    CoreAudioSharedUnit::singleton().suspend();
}

void CoreAudioCaptureSourceFactory::endInterruption()
{
    if (!isMainThread()) {
        callOnMainThread([this] {
            endInterruption();
        });
        return;
    }
    CoreAudioSharedUnit::singleton().resume();
}

void CoreAudioCaptureSourceFactory::scheduleReconfiguration()
{
    if (!isMainThread()) {
        callOnMainThread([this] {
            scheduleReconfiguration();
        });
        return;
    }
    CoreAudioSharedUnit::singleton().reconfigure();
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

void CoreAudioCaptureSourceFactory::devicesChanged(const Vector<CaptureDevice>& devices)
{
    CoreAudioSharedUnit::unit().devicesChanged(devices);
}

CoreAudioCaptureSource::CoreAudioCaptureSource(String&& deviceID, String&& label, String&& hashSalt, uint32_t captureDeviceID, BaseAudioSharedUnit* overrideUnit)
    : RealtimeMediaSource(RealtimeMediaSource::Type::Audio, WTFMove(label), WTFMove(deviceID), WTFMove(hashSalt))
    , m_captureDeviceID(captureDeviceID)
    , m_overrideUnit(overrideUnit)
{
    auto& unit = this->unit();
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

    if (shouldReconfigure)
        unit.reconfigure();

    unit.addClient(*this);
}

CoreAudioCaptureSource::~CoreAudioCaptureSource()
{
#if PLATFORM(IOS_FAMILY)
    CoreAudioCaptureSourceFactory::singleton().unsetActiveSource(*this);
#endif

    unit().removeClient(*this);
}

void CoreAudioCaptureSource::startProducingData()
{
#if PLATFORM(IOS_FAMILY)
    CoreAudioCaptureSourceFactory::singleton().setActiveSource(*this);
#endif

    initializeToStartProducingData();
    unit().startProducingData();
}

void CoreAudioCaptureSource::stopProducingData()
{
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

    m_currentSettings = WTF::nullopt;
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
