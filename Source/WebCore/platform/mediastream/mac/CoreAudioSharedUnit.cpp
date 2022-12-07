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

#include "config.h"
#include "CoreAudioSharedUnit.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioSampleBufferList.h"
#include "AudioSession.h"
#include "CoreAudioCaptureSource.h"
#include "Logging.h"
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

#if PLATFORM(IOS_FAMILY)
#include "AVAudioSessionCaptureDeviceManager.h"
#include "MediaCaptureStatusBarManager.h"
#endif

#include <pal/cf/AudioToolboxSoftLink.h>
#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

const UInt32 outputBus = 0;
const UInt32 inputBus = 1;

class CoreAudioSharedInternalUnit :  public CoreAudioSharedUnit::InternalUnit {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Expected<UniqueRef<InternalUnit>, OSStatus> create();
    explicit CoreAudioSharedInternalUnit(AudioUnit);
    ~CoreAudioSharedInternalUnit() final;

private:
    OSStatus initialize() final;
    OSStatus uninitialize() final;
    OSStatus start() final;
    OSStatus stop() final;
    OSStatus set(AudioUnitPropertyID, AudioUnitScope, AudioUnitElement, const void*, UInt32) final;
    OSStatus get(AudioUnitPropertyID, AudioUnitScope, AudioUnitElement, void*, UInt32*) final;
    OSStatus render(AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*) final;
    OSStatus defaultInputDevice(uint32_t*) final;
    OSStatus defaultOutputDevice(uint32_t*) final;

    AudioUnit m_ioUnit { nullptr };
};

Expected<UniqueRef<CoreAudioSharedUnit::InternalUnit>, OSStatus> CoreAudioSharedInternalUnit::create()
{
    AudioComponentDescription ioUnitDescription = { kAudioUnitType_Output, kAudioUnitSubType_VoiceProcessingIO, kAudioUnitManufacturer_Apple, 0, 0 };
    AudioComponent ioComponent = PAL::AudioComponentFindNext(nullptr, &ioUnitDescription);
    ASSERT(ioComponent);
    if (!ioComponent) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedInternalUnit unable to find vpio unit component");
        return makeUnexpected(-1);
    }

#if !LOG_DISABLED
    CFStringRef name = nullptr;
    PAL::AudioComponentCopyName(ioComponent, &name);
    if (name) {
        String ioUnitName = name;
        CFRelease(name);
        RELEASE_LOG(WebRTC, "CoreAudioSharedInternalUnit created \"%{private}s\" component", ioUnitName.utf8().data());
    }
#endif

    AudioUnit ioUnit;
    auto err = PAL::AudioComponentInstanceNew(ioComponent, &ioUnit);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedInternalUnit unable to open vpio unit, error %d (%.4s)", (int)err, (char*)&err);
        return makeUnexpected(err);
    }

    RELEASE_LOG(WebRTC, "Successfully created a CoreAudioSharedInternalUnit");

    UniqueRef<CoreAudioSharedUnit::InternalUnit> result = makeUniqueRef<CoreAudioSharedInternalUnit>(ioUnit);
    return result;
}

CoreAudioSharedInternalUnit::CoreAudioSharedInternalUnit(AudioUnit ioUnit)
    : m_ioUnit(ioUnit)
{
}

CoreAudioSharedInternalUnit::~CoreAudioSharedInternalUnit()
{
    PAL::AudioComponentInstanceDispose(m_ioUnit);
}

OSStatus CoreAudioSharedInternalUnit::initialize()
{
    return PAL::AudioUnitInitialize(m_ioUnit);
}

OSStatus CoreAudioSharedInternalUnit::uninitialize()
{
    return AudioUnitUninitialize(m_ioUnit);
}

OSStatus CoreAudioSharedInternalUnit::start()
{
    return PAL::AudioOutputUnitStart(m_ioUnit);
}

OSStatus CoreAudioSharedInternalUnit::stop()
{
    return PAL::AudioOutputUnitStop(m_ioUnit);
}

OSStatus CoreAudioSharedInternalUnit::set(AudioUnitPropertyID propertyID, AudioUnitScope scope, AudioUnitElement element, const void* value, UInt32 size)
{
    return PAL::AudioUnitSetProperty(m_ioUnit, propertyID, scope, element, value, size);
}

OSStatus CoreAudioSharedInternalUnit::get(AudioUnitPropertyID propertyID, AudioUnitScope scope, AudioUnitElement element, void* value, UInt32* size)
{
    return PAL::AudioUnitGetProperty(m_ioUnit, propertyID, scope, element, value, size);
}

OSStatus CoreAudioSharedInternalUnit::render(AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inOutputBusNumber, UInt32 inNumberFrames, AudioBufferList* list)
{
    return AudioUnitRender(m_ioUnit, ioActionFlags, inTimeStamp, inOutputBusNumber, inNumberFrames, list);
}

OSStatus CoreAudioSharedInternalUnit::defaultInputDevice(uint32_t* deviceID)
{
#if PLATFORM(MAC)
    UInt32 propertySize = sizeof(*deviceID);
    auto err = get(kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, inputBus, deviceID, &propertySize);
    RELEASE_LOG_ERROR_IF(err, WebRTC, "CoreAudioSharedInternalUnit unable to get default input device ID, error %d (%.4s)", (int)err, (char*)&err);
    return err;
#else
    UNUSED_PARAM(deviceID);
    return -1;
#endif
}

OSStatus CoreAudioSharedInternalUnit::defaultOutputDevice(uint32_t* deviceID)
{
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
        return AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &propertySize, deviceID);
    }
#else
    UNUSED_PARAM(deviceID);
#endif
    return -1;
}

CoreAudioSharedUnit& CoreAudioSharedUnit::unit()
{
    static NeverDestroyed<CoreAudioSharedUnit> singleton;
    return singleton;
}

CoreAudioSharedUnit::CoreAudioSharedUnit()
    : m_sampleRateCapabilities(8000, 96000)
    , m_verifyCapturingTimer(*this, &CoreAudioSharedUnit::verifyIsCapturing)
{
}

CoreAudioSharedUnit::~CoreAudioSharedUnit()
{
}

void CoreAudioSharedUnit::resetSampleRate()
{
    setSampleRate(m_getSampleRateCallback ? m_getSampleRateCallback() : AudioSession::sharedSession().sampleRate());
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

    auto result = m_creationCallback ? m_creationCallback() : CoreAudioSharedInternalUnit::create();
    if (!result.has_value())
        return result.error();

    m_ioUnit = WTFMove(result.value()).moveToUniquePtr();
    if (!enableEchoCancellation()) {
        uint32_t param = 0;
        if (auto err = m_ioUnit->set(kAUVoiceIOProperty_VoiceProcessingEnableAGC, kAudioUnitScope_Global, inputBus, &param, sizeof(param))) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to set vpio automatic gain control, error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
        param = 1;
        if (auto err = m_ioUnit->set(kAUVoiceIOProperty_BypassVoiceProcessing, kAudioUnitScope_Global, inputBus, &param, sizeof(param))) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to set vpio unit echo cancellation, error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }

#if HAVE(VPIO_DUCKING_LEVEL_API)
    AUVoiceIOOtherAudioDuckingConfiguration configuration { true, kAUVoiceIOOtherAudioDuckingLevelMin };
    if (auto err = m_ioUnit->set(kAUVoiceIOProperty_OtherAudioDuckingConfiguration, kAudioUnitScope_Global, inputBus, &configuration, sizeof(configuration))) {
        if (err != kAudioUnitErr_InvalidProperty) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to set ducking level, error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }
#endif // HAVE(VPIO_DUCKING_LEVEL_API)

#if PLATFORM(IOS_FAMILY)
    uint32_t param = 1;
    if (auto err = m_ioUnit->set(kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, inputBus, &param, sizeof(param))) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to enable vpio unit input, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
#else
    auto deviceID = captureDeviceID();
    // FIXME: We probably want to make default input/output devices.
    if (!deviceID) {
        if (auto err = m_ioUnit->defaultInputDevice(&deviceID))
            return err;
    }

    if (auto err = m_ioUnit->set(kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, inputBus, &deviceID, sizeof(deviceID))) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to set vpio unit capture device ID %d, error %d (%.4s)", this, (int)deviceID, (int)err, (char*)&err);
        return err;
    }

    uint32_t defaultOutputDeviceID;
    auto err = m_ioUnit->defaultOutputDevice(&defaultOutputDeviceID);
    if (!err) {
        err = m_ioUnit->set(kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, outputBus, &defaultOutputDeviceID, sizeof(defaultOutputDeviceID));
        RELEASE_LOG_ERROR_IF(err, WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) unable to set vpio unit output device ID %d, error %d (%.4s)", this, (int)defaultOutputDeviceID, (int)err, (char*)&err);
    }
    setOutputDeviceID(!err ? defaultOutputDeviceID : 0);
#endif

    // FIXME: Add support for different speaker/microphone sample rates.
    int actualSampleRate = this->actualSampleRate();
    if (auto err = configureMicrophoneProc(actualSampleRate))
        return err;

    if (auto err = configureSpeakerProc(actualSampleRate))
        return err;

    if (auto err = m_ioUnit->initialize()) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::setupAudioUnit(%p) AudioUnitInitialize() failed, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
    m_ioUnitInitialized = true;

    unduck();

    return 0;
}

void CoreAudioSharedUnit::unduck()
{
    uint32_t outputDevice;
    if (m_ioUnit && !m_ioUnit->defaultOutputDevice(&outputDevice))
        AudioDeviceDuck(outputDevice, 1.0, nullptr, 0);
}

int CoreAudioSharedUnit::actualSampleRate() const
{
    Locker locker { m_speakerSamplesProducerLock };
    return m_speakerSamplesProducer ? m_speakerSamplesProducer->format().streamDescription().mSampleRate : sampleRate();
}

OSStatus CoreAudioSharedUnit::configureMicrophoneProc(int sampleRate)
{
    ASSERT(isMainThread());

    AURenderCallbackStruct callback = { microphoneCallback, this };
    if (auto err = m_ioUnit->set(kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, inputBus, &callback, sizeof(callback))) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureMicrophoneProc(%p) unable to set vpio unit mic proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription microphoneProcFormat = { };

    UInt32 size = sizeof(microphoneProcFormat);
    if (auto err = m_ioUnit->get(kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, inputBus, &microphoneProcFormat, &size)) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureMicrophoneProc(%p) unable to get output stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    microphoneProcFormat.mSampleRate = sampleRate;
    if (auto err = m_ioUnit->set(kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, inputBus, &microphoneProcFormat, size)) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureMicrophoneProc(%p) unable to set output stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    auto bufferSize = preferredIOBufferSize();
    if (m_minimumMicrophoneSampleFrames) {
        auto minBufferSize = *m_minimumMicrophoneSampleFrames * microphoneProcFormat.mBytesPerPacket;
        if (minBufferSize > bufferSize)
            bufferSize = minBufferSize;
        m_minimumMicrophoneSampleFrames = { };
    }
    m_microphoneSampleBuffer = AudioSampleBufferList::create(microphoneProcFormat, bufferSize * 2);
    m_microphoneProcFormat = microphoneProcFormat;

    return noErr;
}

OSStatus CoreAudioSharedUnit::configureSpeakerProc(int sampleRate)
{
    ASSERT(isMainThread());

    AURenderCallbackStruct callback = { speakerCallback, this };
    if (auto err = m_ioUnit->set(kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, outputBus, &callback, sizeof(callback))) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureSpeakerProc(%p) unable to set vpio unit speaker proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription speakerProcFormat;
    UInt32 size = sizeof(speakerProcFormat);
    {
        Locker locker { m_speakerSamplesProducerLock };
        if (m_speakerSamplesProducer) {
            speakerProcFormat = m_speakerSamplesProducer->format().streamDescription();
            ASSERT(speakerProcFormat.mSampleRate == sampleRate);
        } else {
            if (auto err = m_ioUnit->get(kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, outputBus, &speakerProcFormat, &size)) {
                RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
                return err;
            }
        }
    }
    speakerProcFormat.mSampleRate = sampleRate;

    if (auto err = m_ioUnit->set(kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, outputBus, &speakerProcFormat, size)) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    m_speakerProcFormat = speakerProcFormat;

    return noErr;
}

#if !LOG_DISABLED
void CoreAudioSharedUnit::checkTimestamps(const AudioTimeStamp& timeStamp, double hostTime)
{
    if (!timeStamp.mSampleTime || timeStamp.mSampleTime == m_latestMicTimeStamp || !hostTime)
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::checkTimestamps: unusual timestamps, sample time = %f, previous sample time = %f, hostTime %f", timeStamp.mSampleTime, m_latestMicTimeStamp, hostTime);
}
#endif

OSStatus CoreAudioSharedUnit::provideSpeakerData(AudioUnitRenderActionFlags& flags, const AudioTimeStamp& timeStamp, UInt32 /*inBusNumber*/, UInt32 inNumberFrames, AudioBufferList& ioData)
{
    if (m_isReconfiguring || m_shouldNotifySpeakerSamplesProducer || !m_hasNotifiedSpeakerSamplesProducer || !m_speakerSamplesProducerLock.tryLock()) {
        if (m_shouldNotifySpeakerSamplesProducer) {
            m_shouldNotifySpeakerSamplesProducer = false;
            callOnMainThread([this, weakThis = WeakPtr { *this }] {
                if (!weakThis)
                    return;
                m_hasNotifiedSpeakerSamplesProducer = true;
                Locker locker { m_speakerSamplesProducerLock };
                if (m_speakerSamplesProducer)
                    m_speakerSamplesProducer->captureUnitIsStarting();
            });
        }

        AudioSampleBufferList::zeroABL(ioData, static_cast<size_t>(inNumberFrames * m_speakerProcFormat->bytesPerFrame()));
        flags = kAudioUnitRenderAction_OutputIsSilence;
        return noErr;
    }

    Locker locker { AdoptLock, m_speakerSamplesProducerLock };
    if (!m_speakerSamplesProducer) {
        AudioSampleBufferList::zeroABL(ioData, static_cast<size_t>(inNumberFrames * m_speakerProcFormat->bytesPerFrame()));
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
    if (m_isReconfiguring)
        return false;

    // Pull through the vpio unit to our mic buffer.
    m_microphoneSampleBuffer->reset();
    AudioBufferList& bufferList = m_microphoneSampleBuffer->bufferList();
    if (auto err = m_ioUnit->render(&ioActionFlags, &timeStamp, inBusNumber, inNumberFrames, &bufferList)) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::processMicrophoneSamples(%p) AudioUnitRender failed with error %d (%.4s), bufferList size %d, inNumberFrames %d ", this, (int)err, (char*)&err, (int)bufferList.mBuffers[0].mDataByteSize, (int)inNumberFrames);
        if (err == kAudio_ParamError && !m_minimumMicrophoneSampleFrames) {
            m_minimumMicrophoneSampleFrames = inNumberFrames;
            // Our buffer might be too small, the preferred buffer size or sample rate might have changed.
            callOnMainThread([weakThis = WeakPtr { *this }] {
                if (weakThis)
                    weakThis->reconfigure();
            });
        }
        return err;
    }

    ++m_microphoneProcsCalled;

    if (!isProducingMicrophoneSamples())
        return noErr;

    double adjustedHostTime = m_DTSConversionRatio * timeStamp.mHostTime;
    uint64_t sampleTime = timeStamp.mSampleTime;
#if !LOG_DISABLED
    checkTimestamps(timeStamp, adjustedHostTime);
#endif
    m_latestMicTimeStamp = timeStamp.mSampleTime;
    m_microphoneSampleBuffer->setTimes(adjustedHostTime, sampleTime);

    if (volume() != 1.0)
        m_microphoneSampleBuffer->applyGain(volume());

    audioSamplesAvailable(MediaTime(sampleTime, m_microphoneProcFormat->sampleRate()), m_microphoneSampleBuffer->bufferList(), *m_microphoneProcFormat, inNumberFrames);
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
        if (auto err = m_ioUnit->uninitialize())
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::cleanupAudioUnit(%p) AudioUnitUninitialize failed with error %d (%.4s)", this, (int)err, (char*)&err);
        m_ioUnitInitialized = false;
    }

    m_ioUnit = nullptr;

    m_microphoneSampleBuffer = nullptr;
}

void CoreAudioSharedUnit::delaySamples(Seconds seconds)
{
    if (m_ioUnit)
        m_ioUnit->delaySamples(seconds);
}

OSStatus CoreAudioSharedUnit::reconfigureAudioUnit()
{
    ASSERT(isMainThread());
    if (!hasAudioUnit())
        return 0;

    m_isReconfiguring = true;
    auto scope = makeScopeExit([this] { m_isReconfiguring = false; });

    if (m_ioUnitStarted) {
        if (auto err = m_ioUnit->stop()) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::reconfigureAudioUnit(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }

    cleanupAudioUnit();
    if (auto err = setupAudioUnit())
        return err;

    if (m_ioUnitStarted) {
        if (auto err = m_ioUnit->start()) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::reconfigureAudioUnit(%p) AudioOutputUnitStart failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }
    return noErr;
}

OSStatus CoreAudioSharedUnit::startInternal()
{
    ASSERT(isMainThread());

    setIsProducingMicrophoneSamples(true);

    if (!m_ioUnit) {
        if (auto err = setupAudioUnit()) {
            cleanupAudioUnit();
            ASSERT(!m_ioUnit);
            return err;
        }
        ASSERT(m_ioUnit);
    }

    unduck();

    m_shouldNotifySpeakerSamplesProducer = true;
    m_hasNotifiedSpeakerSamplesProducer = false;

    if (auto err = m_ioUnit->start()) {
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

    m_verifyCapturingTimer.startRepeating(m_ioUnit->verifyCaptureInterval(isProducingMicrophoneSamples()));
    m_microphoneProcsCalled = 0;
    m_microphoneProcsCalledLastTime = 0;

    return noErr;
}

void CoreAudioSharedUnit::isProducingMicrophoneSamplesChanged()
{
    if (!isProducingData())
        return;
    m_verifyCapturingTimer.startRepeating(m_ioUnit->verifyCaptureInterval(isProducingMicrophoneSamples()));
}

void CoreAudioSharedUnit::validateOutputDevice(uint32_t currentOutputDeviceID)
{
#if PLATFORM(MAC)
    if (!m_ioUnit)
        return;

    uint32_t currentDefaultOutputDeviceID = 0;
    if (auto err = m_ioUnit->defaultOutputDevice(&currentDefaultOutputDeviceID))
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
    ASSERT(isMainThread());

    m_verifyCapturingTimer.stop();

    if (!m_ioUnit || !m_ioUnitStarted)
        return;

    if (auto err = m_ioUnit->stop()) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit::stop(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return;
    }
    {
        Locker locker { m_speakerSamplesProducerLock };
        if (m_speakerSamplesProducer)
            m_speakerSamplesProducer->captureUnitHasStopped();
    }

    m_ioUnitStarted = false;
#if PLATFORM(IOS_FAMILY)
    setIsInBackground(false);
#endif
}

void CoreAudioSharedUnit::registerSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer& producer)
{
    ASSERT(isMainThread());

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
    ASSERT(isMainThread());

    {
        Locker locker { m_speakerSamplesProducerLock };
        if (m_speakerSamplesProducer != &producer)
            return;
        m_speakerSamplesProducer = nullptr;
    }

    setIsRenderingAudio(false);
}

#if PLATFORM(IOS_FAMILY)
void CoreAudioSharedUnit::setIsInBackground(bool isInBackground)
{
    if (!MediaCaptureStatusBarManager::hasSupport())
        return;

    if (!isInBackground) {
        if (m_statusBarManager) {
            m_statusBarManager->stop();
            m_statusBarManager = nullptr;
        }
        return;
    }

    if (m_statusBarManager)
        return;

    m_statusBarManager = makeUnique<MediaCaptureStatusBarManager>([this](auto&& completionHandler) {
        if (m_statusBarWasTappedCallback)
            m_statusBarWasTappedCallback(WTFMove(completionHandler));
    }, [this] {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioSharedUnit status bar failed");
        auto statusBarManager = std::exchange(m_statusBarManager, { });
        statusBarManager->stop();
        if (isRunning())
            captureFailed();
    });
    m_statusBarManager->start();
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
