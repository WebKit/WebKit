/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
#include "CoreAudioCaptureUnit.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioSampleBufferList.h"
#include "AudioSession.h"
#include "CoreAudioCaptureInternalUnit.h"
#include "CoreAudioCaptureSource.h"
#include "Logging.h"
#include "SpanCoreAudio.h"
#include <AudioToolbox/AudioConverter.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreMedia/CMSync.h>
#include <mach/mach_time.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/spi/cf/CoreAudioSPI.h>
#include <sys/time.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/NativePromise.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WorkQueue.h>

#if PLATFORM(IOS_FAMILY)
#include "AVAudioSessionCaptureDeviceManager.h"
#include "MediaCaptureStatusBarManager.h"
#endif

#if PLATFORM(MAC)
#include "CoreAudioCaptureDevice.h"
#include "CoreAudioCaptureDeviceManager.h"
#endif

#include <pal/cf/AudioToolboxSoftLink.h>
#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

const UInt32 outputBus = 0;
const UInt32 inputBus = 1;

void CoreAudioCaptureUnit::AudioUnitDeallocator::operator()(AudioUnit unit) const
{
    PAL::AudioComponentInstanceDispose(unit);
}

static Expected<CoreAudioCaptureUnit::StoredAudioUnit, OSStatus> createAudioUnit(bool shouldUseVPIO)
{
    OSType unitSubType = kAudioUnitSubType_VoiceProcessingIO;
    if (!shouldUseVPIO) {
#if PLATFORM(MAC)
        unitSubType = kAudioUnitSubType_HALOutput;
#else
        unitSubType = kAudioUnitSubType_RemoteIO;
#endif
    }

    AudioComponentDescription ioUnitDescription = { kAudioUnitType_Output, unitSubType, kAudioUnitManufacturer_Apple, 0, 0 };
    AudioComponent ioComponent = PAL::AudioComponentFindNext(nullptr, &ioUnitDescription);
    ASSERT(ioComponent);
    if (!ioComponent) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureInternalUnit unable to find capture unit component");
        return makeUnexpected(-1);
    }

#if !LOG_DISABLED
    CFStringRef name = nullptr;
    PAL::AudioComponentCopyName(ioComponent, &name);
    if (name) {
        String ioUnitName = name;
        CFRelease(name);
        RELEASE_LOG(WebRTC, "CoreAudioCaptureInternalUnit created \"%" PRIVATE_LOG_STRING "\" component", ioUnitName.utf8().data());
    }
#endif

    AudioUnit ioUnit;
    auto err = PAL::AudioComponentInstanceNew(ioComponent, &ioUnit);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureInternalUnit unable to open capture unit, error %d (%.4s)", (int)err, (char*)&err);
        return makeUnexpected(err);
    }

    return CoreAudioCaptureUnit::StoredAudioUnit(ioUnit);
}

Expected<UniqueRef<CoreAudioCaptureUnit::InternalUnit>, OSStatus> CoreAudioCaptureInternalUnit::create(bool shouldUseVPIO)
{
#if PLATFORM(MAC)
    if (shouldUseVPIO) {
        if (auto ioUnit = CoreAudioCaptureUnit::defaultSingleton().takeStoredVPIOUnit()) {
            RELEASE_LOG(WebRTC, "Creating a CoreAudioCaptureInternalUnit with a stored VPIO unit");
            UniqueRef<CoreAudioCaptureUnit::InternalUnit> result = makeUniqueRef<CoreAudioCaptureInternalUnit>(WTFMove(ioUnit), shouldUseVPIO);
            return result;
        }
    }
#endif

    auto audioUnitOrError = createAudioUnit(shouldUseVPIO);
    if (!audioUnitOrError.has_value())
        return makeUnexpected(audioUnitOrError.error());

    RELEASE_LOG(WebRTC, "Successfully created a CoreAudioCaptureInternalUnit");
    UniqueRef<CoreAudioCaptureUnit::InternalUnit> result = makeUniqueRef<CoreAudioCaptureInternalUnit>(WTFMove(audioUnitOrError.value()), shouldUseVPIO);
    return result;
}

CoreAudioCaptureInternalUnit::CoreAudioCaptureInternalUnit(CoreAudioCaptureUnit::StoredAudioUnit&& audioUnit, bool shouldUseVPIO)
    : m_audioUnit(WTFMove(audioUnit))
    , m_shouldUseVPIO(shouldUseVPIO)
{
}

CoreAudioCaptureInternalUnit::~CoreAudioCaptureInternalUnit()
{
#if PLATFORM(MAC)
    if (m_shouldUseVPIO)
        CoreAudioCaptureUnit::defaultSingleton().setStoredVPIOUnit(std::exchange(m_audioUnit, { }));
#endif
}

OSStatus CoreAudioCaptureInternalUnit::initialize()
{
    return PAL::AudioUnitInitialize(m_audioUnit.get());
}

OSStatus CoreAudioCaptureInternalUnit::uninitialize()
{
    return PAL::AudioUnitUninitialize(m_audioUnit.get());
}

OSStatus CoreAudioCaptureInternalUnit::start()
{
    return PAL::AudioOutputUnitStart(m_audioUnit.get());
}

OSStatus CoreAudioCaptureInternalUnit::stop()
{
    return PAL::AudioOutputUnitStop(m_audioUnit.get());
}

OSStatus CoreAudioCaptureInternalUnit::set(AudioUnitPropertyID propertyID, AudioUnitScope scope, AudioUnitElement element, const void* value, UInt32 size)
{
    return PAL::AudioUnitSetProperty(m_audioUnit.get(), propertyID, scope, element, value, size);
}

OSStatus CoreAudioCaptureInternalUnit::get(AudioUnitPropertyID propertyID, AudioUnitScope scope, AudioUnitElement element, void* value, UInt32* size)
{
    return PAL::AudioUnitGetProperty(m_audioUnit.get(), propertyID, scope, element, value, size);
}

OSStatus CoreAudioCaptureInternalUnit::render(AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inOutputBusNumber, UInt32 inNumberFrames, AudioBufferList* list)
{
    return PAL::AudioUnitRender(m_audioUnit.get(), ioActionFlags, inTimeStamp, inOutputBusNumber, inNumberFrames, list);
}

OSStatus CoreAudioCaptureInternalUnit::defaultInputDevice(uint32_t* deviceID)
{
#if PLATFORM(MAC)
    UInt32 propertySize = sizeof(*deviceID);
    auto err = get(kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, inputBus, deviceID, &propertySize);
    RELEASE_LOG_ERROR_IF(err, WebRTC, "CoreAudioCaptureInternalUnit unable to get default input device ID, error %d (%.4s)", (int)err, (char*)&err);
    return err;
#else
    UNUSED_PARAM(deviceID);
    return -1;
#endif
}

OSStatus CoreAudioCaptureInternalUnit::defaultOutputDevice(uint32_t* deviceID)
{
#if PLATFORM(MAC)
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
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

CoreAudioCaptureUnit& CoreAudioCaptureUnit::defaultSingleton()
{
    static NeverDestroyed<Ref<CoreAudioCaptureUnit>> singleton(adoptRef(*new CoreAudioCaptureUnit(CanEnableEchoCancellation::Yes)));
    return singleton.get();
}

Ref<CoreAudioCaptureUnit> CoreAudioCaptureUnit::createNonVPIOUnit()
{
    return adoptRef(*new CoreAudioCaptureUnit(CanEnableEchoCancellation::No));
}

static WeakHashSet<CoreAudioCaptureUnit>& allCoreAudioCaptureUnits()
{
    ASSERT(isMainThread());

    static NeverDestroyed<WeakHashSet<CoreAudioCaptureUnit>> units;
    return units;
}

bool CoreAudioCaptureUnit::isAnyUnitCapturingExceptFor(CoreAudioCaptureUnit* unitToNotTest)
{
    return std::ranges::any_of(allCoreAudioCaptureUnits(), [unitToNotTest = WeakPtr { unitToNotTest }](auto& unit) {
        return unit.isProducingMicrophoneSamples() && &unit != unitToNotTest.get();
    });
}

bool CoreAudioCaptureUnit::isAnyUnitCapturing()
{
    return isAnyUnitCapturingExceptFor(nullptr);
}

void CoreAudioCaptureUnit::forEach(NOESCAPE Function<void(CoreAudioCaptureUnit&)>&& callback)
{
    allCoreAudioCaptureUnits().forEach(WTFMove(callback));
}

static Function<void(CoreAudioCaptureUnit&)>& coreAudioCaptureNewUnitCallback()
{
    ASSERT(isMainThread());

    static NeverDestroyed<Function<void(CoreAudioCaptureUnit&)>> callback;
    return callback.get();
}

void CoreAudioCaptureUnit::forNewUnit(Function<void(CoreAudioCaptureUnit&)>&& callback)
{
    coreAudioCaptureNewUnitCallback() = WTFMove(callback);
}

CoreAudioCaptureUnit::CoreAudioCaptureUnit(CanEnableEchoCancellation canEnableEchoCancellation)
    : BaseAudioCaptureUnit(canEnableEchoCancellation)
    , m_sampleRateCapabilities(8000, 96000)
#if PLATFORM(MAC)
    , m_storedVPIOUnitDeallocationTimer(*this, &CoreAudioCaptureUnit::deallocateStoredVPIOUnit)
#endif
{
    allCoreAudioCaptureUnits().add(*this);
    if (coreAudioCaptureNewUnitCallback())
        coreAudioCaptureNewUnitCallback()(*this);
}

CoreAudioCaptureUnit::~CoreAudioCaptureUnit()
{
    allCoreAudioCaptureUnits().remove(*this);

    updateVoiceActivityDetection();
    setMuteStatusChangedCallback({ });
}

#if PLATFORM(MAC)
void CoreAudioCaptureUnit::setStoredVPIOUnit(StoredAudioUnit&& unit)
{
    RELEASE_LOG(WebRTC, "CoreAudioCaptureUnit::setStoredVPIOUnit(%p)", this);

    static constexpr Seconds delayBeforeStoredVPIOUnitDeallocation = 3_s;
    m_storedVPIOUnit = WTFMove(unit);
    m_storedVPIOUnitDeallocationTimer.startOneShot(delayBeforeStoredVPIOUnitDeallocation);
}

CoreAudioCaptureUnit::StoredAudioUnit CoreAudioCaptureUnit::takeStoredVPIOUnit()
{
    RELEASE_LOG(WebRTC, "CoreAudioCaptureUnit::takeStoredVPIOUnit(%p)", this);

    m_storedVPIOUnitDeallocationTimer.stop();
    return std::exchange(m_storedVPIOUnit, nullptr);
}
#endif

void CoreAudioCaptureUnit::resetSampleRate()
{
    setSampleRate(m_getSampleRateCallback ? m_getSampleRateCallback() : AudioSession::singleton().sampleRate());
}

void CoreAudioCaptureUnit::captureDeviceChanged()
{
#if PLATFORM(MAC)
    reconfigureAudioUnit();
#else
    AVAudioSessionCaptureDeviceManager::singleton().setPreferredMicrophoneID(isCapturingWithDefaultMicrophone() ? String { } : persistentID());
#endif
    updateVoiceActivityDetection();
}

size_t CoreAudioCaptureUnit::preferredIOBufferSize()
{
    return AudioSession::singleton().bufferSize();
}

OSStatus CoreAudioCaptureUnit::setupAudioUnit()
{
    if (m_ioUnit)
        return 0;

    ASSERT(hasClients());

    mach_timebase_info_data_t timebaseInfo;
    mach_timebase_info(&timebaseInfo);
    m_DTSConversionRatio = 1e-9 * static_cast<double>(timebaseInfo.numer) / static_cast<double>(timebaseInfo.denom);

    bool isEchoCancellationChanging = m_shouldUseVPIO != enableEchoCancellation();
    m_shouldUseVPIO = enableEchoCancellation();
    auto result = m_creationCallback ? m_creationCallback(m_shouldUseVPIO) : CoreAudioCaptureInternalUnit::create(m_shouldUseVPIO);
    if (!result.has_value())
        return result.error();

    m_ioUnit = WTFMove(result.value()).moveToUniquePtr();

    bool canRenderAudio = m_ioUnit->canRenderAudio();
    if (m_canRenderAudio != canRenderAudio) {
        m_canRenderAudio = canRenderAudio;
        {
            Locker locker { m_speakerSamplesProducerLock };
            if (m_speakerSamplesProducer)
                m_speakerSamplesProducer->canRenderAudioChanged();
        }
    }

    if (isEchoCancellationChanging) {
        forEachClient([](auto& client) {
            client.echoCancellationChanged();
        });
    }

#if HAVE(VPIO_DUCKING_LEVEL_API)
    if (m_shouldUseVPIO) {
        AUVoiceIOOtherAudioDuckingConfiguration configuration { true, kAUVoiceIOOtherAudioDuckingLevelMin };
        if (auto err = m_ioUnit->set(kAUVoiceIOProperty_OtherAudioDuckingConfiguration, kAudioUnitScope_Global, inputBus, &configuration, sizeof(configuration))) {
            if (err != kAudioUnitErr_InvalidProperty) {
                RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::setupAudioUnit(%p) unable to set ducking level, error %d (%.4s)", this, (int)err, (char*)&err);
                return err;
            }
        }
    }
#endif // HAVE(VPIO_DUCKING_LEVEL_API)

#if PLATFORM(IOS_FAMILY)
    uint32_t param = 1;
    if (auto err = m_ioUnit->set(kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, inputBus, &param, sizeof(param))) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::setupAudioUnit(%p) unable to enable capture unit input, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
#else
    if (!m_shouldUseVPIO) {
        uint32_t param = 1;
        if (auto err = m_ioUnit->set(kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, inputBus, &param, sizeof(param))) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::setupAudioUnit(%p) unable to enable capture unit input, error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
        param = 0;
        if (auto err = m_ioUnit->set(kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, outputBus, &param, sizeof(param))) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::setupAudioUnit(%p) unable to enable capture unit output, error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }

    auto deviceID = captureDeviceID();
    // FIXME: We probably want to make default input/output devices.
    if (!deviceID) {
        if (auto err = m_ioUnit->defaultInputDevice(&deviceID))
            return err;
    }

    if (auto err = m_ioUnit->set(kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, inputBus, &deviceID, sizeof(deviceID))) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::setupAudioUnit(%p) unable to set capture unit capture device ID %d, error %d (%.4s)", this, (int)deviceID, (int)err, (char*)&err);
        return err;
    }

    if (m_shouldUseVPIO) {
        uint32_t defaultOutputDeviceID;
        auto err = m_ioUnit->defaultOutputDevice(&defaultOutputDeviceID);
        if (!err) {
            err = m_ioUnit->set(kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, outputBus, &defaultOutputDeviceID, sizeof(defaultOutputDeviceID));
            RELEASE_LOG_ERROR_IF(err, WebRTC, "CoreAudioCaptureUnit::setupAudioUnit(%p) unable to set capture unit output device ID %d, error %d (%.4s)", this, (int)defaultOutputDeviceID, (int)err, (char*)&err);
        }
        setOutputDeviceID(!err ? defaultOutputDeviceID : 0);
    } else {
        // With HALOutput, we cannot rely on sample rate conversions, we stick to hardware sample rate.
        static const AudioObjectPropertyAddress nominalSampleRateAddress = {
            kAudioDevicePropertyNominalSampleRate,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };

        Float64 nominalSampleRate;
        UInt32 nominalSampleRateSize = sizeof(Float64);
        if (AudioObjectGetPropertyData(deviceID, &nominalSampleRateAddress, 0, 0, &nominalSampleRateSize, (void*)&nominalSampleRate) == noErr)
            setSampleRate(nominalSampleRate);
    }
#endif

    // FIXME: Add support for different speaker/microphone sample rates.
    int actualSampleRate = this->actualSampleRate();
    if (auto err = configureMicrophoneProc(actualSampleRate))
        return err;

    if (auto err = configureSpeakerProc(actualSampleRate))
        return err;

    if (auto err = m_ioUnit->initialize()) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::setupAudioUnit(%p) AudioUnitInitialize() failed, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
    m_ioUnitInitialized = true;

    unduck();

    return 0;
}

void CoreAudioCaptureUnit::unduck()
{
    uint32_t outputDevice;
    if (m_ioUnit && !m_ioUnit->defaultOutputDevice(&outputDevice))
        AudioDeviceDuck(outputDevice, 1.0, nullptr, 0);
}

int CoreAudioCaptureUnit::actualSampleRate() const
{
    Locker locker { m_speakerSamplesProducerLock };
    return m_speakerSamplesProducer ? m_speakerSamplesProducer->format().streamDescription().mSampleRate : sampleRate();
}

OSStatus CoreAudioCaptureUnit::configureMicrophoneProc(int sampleRate)
{
    ASSERT(isMainThread());

    AURenderCallbackStruct callback = { microphoneCallback, this };
    if (auto err = m_ioUnit->set(kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, inputBus, &callback, sizeof(callback))) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::configureMicrophoneProc(%p) unable to set capture unit mic proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription microphoneProcFormat = { };

    UInt32 size = sizeof(microphoneProcFormat);
    if (auto err = m_ioUnit->get(kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, inputBus, &microphoneProcFormat, &size)) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::configureMicrophoneProc(%p) unable to get output stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    microphoneProcFormat.mSampleRate = sampleRate;
    if (auto err = m_ioUnit->set(kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, inputBus, &microphoneProcFormat, size)) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::configureMicrophoneProc(%p) unable to set output stream format, error %d (%.4s)", this, (int)err, (char*)&err);
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

OSStatus CoreAudioCaptureUnit::configureSpeakerProc(int sampleRate)
{
    ASSERT(isMainThread());

    AURenderCallbackStruct callback = { speakerCallback, this };
    if (auto err = m_ioUnit->set(kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, outputBus, &callback, sizeof(callback))) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::configureSpeakerProc(%p) unable to set capture unit speaker proc, error %d (%.4s)", this, (int)err, (char*)&err);
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
                RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
                return err;
            }
        }
    }
    speakerProcFormat.mSampleRate = sampleRate;

    if (auto err = m_ioUnit->set(kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, outputBus, &speakerProcFormat, size)) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    m_speakerProcFormat = speakerProcFormat;

    return noErr;
}

#if !LOG_DISABLED
void CoreAudioCaptureUnit::checkTimestamps(const AudioTimeStamp& timeStamp, double hostTime)
{
    if (!timeStamp.mSampleTime || timeStamp.mSampleTime == m_latestMicTimeStamp || !hostTime)
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::checkTimestamps(%p): unusual timestamps, sample time = %f, previous sample time = %f, hostTime %f", this, timeStamp.mSampleTime, m_latestMicTimeStamp, hostTime);
}
#endif

OSStatus CoreAudioCaptureUnit::provideSpeakerData(AudioUnitRenderActionFlags& flags, const AudioTimeStamp& timeStamp, UInt32 /*inBusNumber*/, UInt32 inNumberFrames, AudioBufferList& ioData)
{
    if (m_isReconfiguring || m_shouldNotifySpeakerSamplesProducer || !m_hasNotifiedSpeakerSamplesProducer || !m_speakerSamplesProducerLock.tryLock()) {
        if (m_shouldNotifySpeakerSamplesProducer) {
            m_shouldNotifySpeakerSamplesProducer = false;
            callOnMainThread([weakThis = WeakPtr { *this }] {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                protectedThis->m_hasNotifiedSpeakerSamplesProducer = true;
                Locker locker { protectedThis->m_speakerSamplesProducerLock };
                if (protectedThis->m_speakerSamplesProducer)
                    protectedThis->m_speakerSamplesProducer->captureUnitIsStarting();
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

OSStatus CoreAudioCaptureUnit::speakerCallback(void *inRefCon, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    ASSERT(ioActionFlags);
    ASSERT(inTimeStamp);
    auto dataSource = static_cast<CoreAudioCaptureUnit*>(inRefCon);
    return dataSource->provideSpeakerData(*ioActionFlags, *inTimeStamp, inBusNumber, inNumberFrames, *ioData);
}

OSStatus CoreAudioCaptureUnit::processMicrophoneSamples(AudioUnitRenderActionFlags& ioActionFlags, const AudioTimeStamp& timeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* /*ioData*/)
{
    if (m_isReconfiguring)
        return false;

    // Pull through the capture unit to our mic buffer.
    RefPtr microphoneSampleBuffer = m_microphoneSampleBuffer;
    microphoneSampleBuffer->reset();
    AudioBufferList& bufferList = microphoneSampleBuffer->bufferList();
    if (auto err = m_ioUnit->render(&ioActionFlags, &timeStamp, inBusNumber, inNumberFrames, &bufferList)) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::processMicrophoneSamples(%p) AudioUnitRender failed with error %d (%.4s), bufferList size %d, inNumberFrames %d ", this, (int)err, (char*)&err, (int)span(bufferList)[0].mDataByteSize, (int)inNumberFrames);
        if (err == kAudio_ParamError && !m_minimumMicrophoneSampleFrames) {
            m_minimumMicrophoneSampleFrames = inNumberFrames;
            // Our buffer might be too small, the preferred buffer size or sample rate might have changed.
            callOnMainThread([weakThis = WeakPtr { *this }] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->reconfigure();
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
    microphoneSampleBuffer->setTimes(adjustedHostTime, sampleTime);

    if (volume() != 1.0)
        microphoneSampleBuffer->applyGain(volume());

    audioSamplesAvailable(MediaTime(sampleTime, m_microphoneProcFormat->sampleRate()), microphoneSampleBuffer->bufferList(), *m_microphoneProcFormat, inNumberFrames);
    return noErr;
}

OSStatus CoreAudioCaptureUnit::microphoneCallback(void *inRefCon, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    ASSERT(ioActionFlags);
    ASSERT(inTimeStamp);
    CoreAudioCaptureUnit* dataSource = static_cast<CoreAudioCaptureUnit*>(inRefCon);
    return dataSource->processMicrophoneSamples(*ioActionFlags, *inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

void CoreAudioCaptureUnit::cleanupAudioUnit()
{
    if (m_ioUnitInitialized) {
        ASSERT(m_ioUnit);
        if (auto err = m_ioUnit->uninitialize())
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::cleanupAudioUnit(%p) AudioUnitUninitialize failed with error %d (%.4s)", this, (int)err, (char*)&err);
        m_ioUnitInitialized = false;
    }

    updateVoiceActivityDetection();
    updateMutedState();

    m_ioUnit = nullptr;

    m_microphoneSampleBuffer = nullptr;
}

void CoreAudioCaptureUnit::delaySamples(Seconds seconds)
{
    if (m_ioUnit)
        m_ioUnit->delaySamples(seconds);
}

OSStatus CoreAudioCaptureUnit::reconfigureAudioUnit()
{
    ASSERT(isMainThread());
    if (!hasAudioUnit())
        return 0;

    if (!hasClients()) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::reconfigureAudioUnit(%p) stopping since there are no clients", this);
        stopRunning();
        return 0;
    }

    m_isReconfiguring = true;
    auto scope = makeScopeExit([this] {
        m_isReconfiguring = false;
    });

    if (m_ioUnitStarted) {
        if (auto err = m_ioUnit->stop()) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::reconfigureAudioUnit(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }

    cleanupAudioUnit();
    if (auto err = setupAudioUnit())
        return err;

    if (m_ioUnitStarted) {
        if (auto err = m_ioUnit->start()) {
            RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::reconfigureAudioUnit(%p) AudioOutputUnitStart failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }
    return noErr;
}

OSStatus CoreAudioCaptureUnit::startInternal()
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

        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::start(%p) AudioOutputUnitStart failed with error %d (%.4s)", this, (int)err, (char*)&err);
        cleanupAudioUnit();
        ASSERT(!m_ioUnit);
        return err;
    }

    m_ioUnitStarted = true;

    m_microphoneProcsCalled = 0;
    m_microphoneProcsCalledLastTime = 0;
    if (!m_verifyCapturingTimer)
        m_verifyCapturingTimer = makeUnique<Timer>(*this, &CoreAudioCaptureUnit::verifyIsCapturing);
    m_verifyCapturingTimer->startRepeating(m_ioUnit->verifyCaptureInterval(!m_microphoneProcsCalledLastTime || isProducingMicrophoneSamples()));

    updateVoiceActivityDetection();
    updateMutedState();

    return noErr;
}

void CoreAudioCaptureUnit::isProducingMicrophoneSamplesChanged()
{
    updateMutedState();
    updateVoiceActivityDetection();

    if (!isProducingData())
        return;

    if (!m_verifyCapturingTimer)
        m_verifyCapturingTimer = makeUnique<Timer>(*this, &CoreAudioCaptureUnit::verifyIsCapturing);
    m_verifyCapturingTimer->startRepeating(m_ioUnit->verifyCaptureInterval(!m_microphoneProcsCalledLastTime || isProducingMicrophoneSamples()));
}

void CoreAudioCaptureUnit::updateMutedState(SyncUpdate syncUpdate)
{
    UInt32 muteUplinkOutput = m_ioUnit && isProducingData() && !isProducingMicrophoneSamples();

    if (syncUpdate == SyncUpdate::No && muteUplinkOutput) {
        RELEASE_LOG_INFO(WebRTC, "CoreAudioCaptureUnit::updateMutedState(%p) delaying mute in case unit gets stopped or unmuted soon", this);
        // We leave some time for playback to stop or for capture to restart, but not too long if the user decided to stop capture.
        static constexpr Seconds mutedStateDelay = 500_ms;

        if (!m_updateMutedStateTimer)
            m_updateMutedStateTimer = makeUnique<Timer>(*this, &CoreAudioCaptureUnit::updateMutedStateTimerFired);
        m_updateMutedStateTimer->startOneShot(mutedStateDelay);
        return;
    }
    if (m_updateMutedStateTimer)
        m_updateMutedStateTimer->stop();

    if (m_ioUnit) {
        auto error = m_ioUnit->set(kAUVoiceIOProperty_MuteOutput, kAudioUnitScope_Global, outputBus, &muteUplinkOutput, sizeof(muteUplinkOutput));
        RELEASE_LOG_ERROR_IF(error, WebRTC, "CoreAudioCaptureUnit::updateMutedState(%p) unable to set kAUVoiceIOProperty_MuteOutput, error %d (%.4s)", this, (int)error, (char*)&error);
    }

    bool isAnyOtherUnitCapturing = isAnyUnitCapturingExceptFor(this);
    RELEASE_LOG_INFO(WebRTC, "CoreAudioCaptureUnit::updateMutedState muteUplinkOutput=%d isAnyOtherUnitCapturing=%d", muteUplinkOutput, isAnyOtherUnitCapturing);

    if (isAnyOtherUnitCapturing)
        return;

    setMutedState(muteUplinkOutput);
}

void CoreAudioCaptureUnit::updateMutedStateTimerFired()
{
    updateMutedState(SyncUpdate::Yes);
}

void CoreAudioCaptureUnit::validateOutputDevice(uint32_t currentOutputDeviceID)
{
#if PLATFORM(MAC)
    if (!m_shouldUseVPIO || !m_ioUnit)
        return;

    uint32_t currentDefaultOutputDeviceID = 0;
    if (m_ioUnit->defaultOutputDevice(&currentDefaultOutputDeviceID))
        return;

    if (!currentDefaultOutputDeviceID || currentOutputDeviceID == currentDefaultOutputDeviceID)
        return;

    reconfigure();
#else
    UNUSED_PARAM(currentOutputDeviceID);
#endif
}

#if PLATFORM(MAC)
bool CoreAudioCaptureUnit::migrateToNewDefaultDevice(const CaptureDevice& captureDevice)
{
    auto newDefaultDevicePersistentId = captureDevice.persistentId();
    auto device = CoreAudioCaptureDeviceManager::singleton().coreAudioDeviceWithUID(newDefaultDevicePersistentId);
    if (!device)
        return false;

    // We were capturing with the default device which disappeared, let's move capture to the new default device.
    setCaptureDevice(WTFMove(newDefaultDevicePersistentId), device->deviceID(), true);
    handleNewCurrentMicrophoneDevice(WTFMove(*device));
    return true;
}

void CoreAudioCaptureUnit::prewarmAudioUnitCreation(CompletionHandler<void()>&& callback)
{
    if (RefPtr audioUnitCreationWarmupPromise = m_audioUnitCreationWarmupPromise) {
        audioUnitCreationWarmupPromise->whenSettled(RunLoop::mainSingleton(), WTFMove(callback));
        return;
    }

    if (!enableEchoCancellation()) {
        callback();
        return;
    }

    m_audioUnitCreationWarmupPromise = invokeAsync(WorkQueue::create("CoreAudioCaptureUnit AudioUnit creation"_s, WorkQueue::QOS::UserInitiated).get(), [] {
        return createAudioUnit(true);
    })->whenSettled(RunLoop::mainSingleton(), [weakThis = WeakPtr { *this }, callback = WTFMove(callback)] (auto&& vpioUnitOrError) mutable {
        if (weakThis && vpioUnitOrError.has_value())
            weakThis->setStoredVPIOUnit(WTFMove(vpioUnitOrError.value()));
        callback();
        return GenericNonExclusivePromise::createAndResolve();
    });
}

void CoreAudioCaptureUnit::deallocateStoredVPIOUnit()
{
    if (!m_storedVPIOUnit)
        return;

    RELEASE_LOG(WebRTC, "CoreAudioCaptureUnit::deallocateStoredVPIOUnit");

    m_storedVPIOUnit = { };
    m_audioUnitCreationWarmupPromise = nullptr;
}
#endif

void CoreAudioCaptureUnit::verifyIsCapturing()
{
    if (m_microphoneProcsCalledLastTime != m_microphoneProcsCalled) {
        m_microphoneProcsCalledLastTime = m_microphoneProcsCalled;
        return;
    }

    RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::verifyIsCapturing - no audio received in %d seconds, failing", static_cast<int>(m_verifyCapturingTimer->repeatInterval().value()));
    captureFailed();
}

void CoreAudioCaptureUnit::stopInternal()
{
    ASSERT(isMainThread());

    if (m_verifyCapturingTimer)
        m_verifyCapturingTimer->stop();

    if (!m_ioUnit || !m_ioUnitStarted)
        return;

    if (auto err = m_ioUnit->stop()) {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit::stop(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
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
    updateVoiceActivityDetection();
    updateMutedState();
}

void CoreAudioCaptureUnit::registerSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer& producer)
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

void CoreAudioCaptureUnit::unregisterSpeakerSamplesProducer(CoreAudioSpeakerSamplesProducer& producer)
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

bool CoreAudioCaptureUnit::shouldEnableVoiceActivityDetection() const
{
    return hasVoiceActivityListenerCallback()
#if PLATFORM(IOS_FAMILY)
        && !isProducingMicrophoneSamples()
#endif
        && m_ioUnitStarted;
}

void CoreAudioCaptureUnit::updateVoiceActivityDetection(bool shouldDisableVoiceActivityDetection)
{
    if (!m_ioUnit)
        return;

    if (m_voiceActivityDetectionEnabled) {
        if (shouldEnableVoiceActivityDetection() && !shouldDisableVoiceActivityDetection)
            return;
        if (m_ioUnit->setVoiceActivityDetection(false))
            m_voiceActivityDetectionEnabled = false;
        return;
    }

    if (!shouldEnableVoiceActivityDetection())
        return;
    if (m_ioUnit->setVoiceActivityDetection(true))
        m_voiceActivityDetectionEnabled = true;
}

void CoreAudioCaptureUnit::enableMutedSpeechActivityEventListener(Function<void()>&& callback)
{
    setVoiceActivityListenerCallback(WTFMove(callback));
    updateVoiceActivityDetection();
}

void CoreAudioCaptureUnit::disableMutedSpeechActivityEventListener()
{
    setVoiceActivityListenerCallback({ });
    updateVoiceActivityDetection();
}

void CoreAudioCaptureUnit::handleMuteStatusChangedNotification(bool isMuting)
{
    RELEASE_LOG_INFO(WebRTC, "CoreAudioCaptureUnit::handleMuteStatusChangedNotification isMuting=%d isAnyUnitCapturing=%d", isMuting, isAnyUnitCapturing());

    if (m_muteStatusChangedCallback && isMuting == isAnyUnitCapturing())
        m_muteStatusChangedCallback(isMuting);
}

void CoreAudioCaptureUnit::willChangeCaptureDeviceTo(const String& persistentId)
{
#if PLATFORM(MAC)
    if (m_shouldUseVPIO) {
        forEachClient([&persistentId](auto& client) {
            client.vpioUnitWillChangeCaptureDeviceTo(persistentId);
        });
    }
#else
    UNUSED_PARAM(persistentId);
#endif

    if (!m_voiceActivityDetectionEnabled)
        return;

    bool shouldDisableVoiceActivityDetection = true;
    updateVoiceActivityDetection(shouldDisableVoiceActivityDetection);
}

#if PLATFORM(IOS_FAMILY)
void CoreAudioCaptureUnit::setIsInBackground(bool isInBackground)

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

    m_statusBarManager = MediaCaptureStatusBarManager::create([weakThis = WeakPtr { *this }](CompletionHandler<void()>&& completionHandler) {
        RefPtr protectedThis = weakThis.get();
        if (protectedThis && protectedThis->m_statusBarWasTappedCallback)
            protectedThis->m_statusBarWasTappedCallback(WTFMove(completionHandler));
    }, [weakThis = WeakPtr { *this }] {
        RELEASE_LOG_ERROR(WebRTC, "CoreAudioCaptureUnit status bar failed");
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        auto statusBarManager = std::exchange(protectedThis->m_statusBarManager, { });
        statusBarManager->stop();
        if (protectedThis->isRunning())
            protectedThis->captureFailed();
    });
    m_statusBarManager->start();
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
