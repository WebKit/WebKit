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

#include "config.h"
#include "CoreAudioCaptureSource.h"

#if ENABLE(MEDIA_STREAM)

#include "AVAudioSessionCaptureDevice.h"
#include "AVAudioSessionCaptureDeviceManager.h"
#include "AudioSampleBufferList.h"
#include "AudioSampleDataSource.h"
#include "AudioSession.h"
#include "CoreAudioCaptureDevice.h"
#include "CoreAudioCaptureDeviceManager.h"
#include "Logging.h"
#include "MediaTimeAVFoundation.h"
#include "WebAudioSourceProviderAVFObjC.h"
#include <AudioToolbox/AudioConverter.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreMedia/CMSync.h>
#include <mach/mach_time.h>
#include <sys/time.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include "CoreMediaSoftLink.h"

namespace WebCore {

class CoreAudioCaptureSourceFactory : public RealtimeMediaSource::AudioCaptureFactory
#if PLATFORM(IOS)
    , public RealtimeMediaSource::SingleSourceFactory<CoreAudioCaptureSource>
#endif
{
public:
    CaptureSourceOrError createAudioCaptureSource(const String& deviceID, const MediaConstraints* constraints) final {
        return CoreAudioCaptureSource::create(deviceID, constraints);
    }
};

static CoreAudioCaptureSourceFactory& coreAudioCaptureSourceFactory()
{
    static NeverDestroyed<CoreAudioCaptureSourceFactory> factory;
    return factory.get();
}

const UInt32 outputBus = 0;
const UInt32 inputBus = 1;

class CoreAudioSharedUnit {
public:
    static CoreAudioSharedUnit& singleton();
    CoreAudioSharedUnit();

    void addClient(CoreAudioCaptureSource&);
    void removeClient(CoreAudioCaptureSource&);

    void startProducingData();
    void stopProducingData();
    bool isProducingData() { return m_ioUnitStarted; }

    OSStatus suspend();

    OSStatus setupAudioUnit();
    void cleanupAudioUnit();
    OSStatus reconfigureAudioUnit();

    void addEchoCancellationSource(AudioSampleDataSource&);
    void removeEchoCancellationSource(AudioSampleDataSource&);

    static size_t preferredIOBufferSize();

    const CAAudioStreamDescription& microphoneFormat() const { return m_microphoneProcFormat; }

    double volume() const { return m_volume; }
    int sampleRate() const { return m_sampleRate; }
    bool enableEchoCancellation() const { return m_enableEchoCancellation; }

    void setVolume(double volume) { m_volume = volume; }
    void setSampleRate(int sampleRate) { m_sampleRate = sampleRate; }
    void setEnableEchoCancellation(bool enableEchoCancellation) { m_enableEchoCancellation = enableEchoCancellation; }

    bool hasAudioUnit() const { return m_ioUnit; }

private:
    OSStatus configureSpeakerProc();
    OSStatus configureMicrophoneProc();
    OSStatus defaultOutputDevice(uint32_t*);
    OSStatus defaultInputDevice(uint32_t*);

    static OSStatus microphoneCallback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
    OSStatus processMicrophoneSamples(AudioUnitRenderActionFlags&, const AudioTimeStamp&, UInt32, UInt32, AudioBufferList*);

    static OSStatus speakerCallback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
    OSStatus provideSpeakerData(AudioUnitRenderActionFlags&, const AudioTimeStamp&, UInt32, UInt32, AudioBufferList*);

    Vector<std::reference_wrapper<CoreAudioCaptureSource>> m_clients;

    AudioUnit m_ioUnit { nullptr };

    // Only read/modified from the IO thread.
    Vector<Ref<AudioSampleDataSource>> m_activeSources;

    enum QueueAction { Add, Remove };
    Vector<std::pair<QueueAction, Ref<AudioSampleDataSource>>> m_pendingSources;

    uint32_t m_captureDeviceID { 0 };

    CAAudioStreamDescription m_microphoneProcFormat;
    RefPtr<AudioSampleBufferList> m_microphoneSampleBuffer;
    uint64_t m_latestMicTimeStamp { 0 };

    CAAudioStreamDescription m_speakerProcFormat;
    RefPtr<AudioSampleBufferList> m_speakerSampleBuffer;

    double m_DTSConversionRatio { 0 };

    bool m_ioUnitInitialized { false };
    bool m_ioUnitStarted { false };

    Lock m_pendingSourceQueueLock;
    Lock m_internalStateLock;

    int32_t m_suspendCount { 0 };
    int32_t m_producingCount { 0 };

    mutable std::unique_ptr<RealtimeMediaSourceCapabilities> m_capabilities;
    mutable std::optional<RealtimeMediaSourceSettings> m_currentSettings;

#if !LOG_DISABLED
    void checkTimestamps(const AudioTimeStamp&, uint64_t, double);

    String m_ioUnitName;
    uint64_t m_speakerProcsCalled { 0 };
    uint64_t m_microphoneProcsCalled { 0 };
#endif

    bool m_enableEchoCancellation { true };
    double m_volume { 1 };
    int m_sampleRate;
};

CoreAudioSharedUnit& CoreAudioSharedUnit::singleton()
{
    static NeverDestroyed<CoreAudioSharedUnit> singleton;
    return singleton;
}

CoreAudioSharedUnit::CoreAudioSharedUnit()
{
    m_sampleRate = AudioSession::sharedSession().sampleRate();
}

void CoreAudioSharedUnit::addClient(CoreAudioCaptureSource& client)
{
    m_clients.append(client);
}

void CoreAudioSharedUnit::removeClient(CoreAudioCaptureSource& client)
{
    m_clients.removeAllMatching([&](const auto& item) {
        return &client == &item.get();
    });
}

void CoreAudioSharedUnit::addEchoCancellationSource(AudioSampleDataSource& source)
{
    if (!source.setOutputFormat(m_speakerProcFormat)) {
        LOG(Media, "CoreAudioSharedUnit::addEchoCancellationSource: source %p configureOutput failed", &source);
        return;
    }

    std::lock_guard<Lock> lock(m_pendingSourceQueueLock);
    m_pendingSources.append({ QueueAction::Add, source });
}

void CoreAudioSharedUnit::removeEchoCancellationSource(AudioSampleDataSource& source)
{
    std::lock_guard<Lock> lock(m_pendingSourceQueueLock);
    m_pendingSources.append({ QueueAction::Remove, source });
}

size_t CoreAudioSharedUnit::preferredIOBufferSize()
{
    return AudioSession::sharedSession().bufferSize();
}

OSStatus CoreAudioSharedUnit::setupAudioUnit()
{
    if (m_ioUnit)
        return 0;

    ASSERT(!m_clients.isEmpty());

    mach_timebase_info_data_t timebaseInfo;
    mach_timebase_info(&timebaseInfo);
    m_DTSConversionRatio = 1e-9 * static_cast<double>(timebaseInfo.numer) / static_cast<double>(timebaseInfo.denom);

    AudioComponentDescription ioUnitDescription = { kAudioUnitType_Output, kAudioUnitSubType_VoiceProcessingIO, kAudioUnitManufacturer_Apple, 0, 0 };
    AudioComponent ioComponent = AudioComponentFindNext(nullptr, &ioUnitDescription);
    ASSERT(ioComponent);
    if (!ioComponent) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnit(%p) unable to find vpio unit component", this);
        return -1;
    }

#if !LOG_DISABLED
    CFStringRef name = nullptr;
    AudioComponentCopyName(ioComponent, &name);
    if (name) {
        m_ioUnitName = name;
        CFRelease(name);
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnit(%p) created \"%s\" component", this, m_ioUnitName.utf8().data());
    }
#endif

    auto err = AudioComponentInstanceNew(ioComponent, &m_ioUnit);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnit(%p) unable to open vpio unit, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    if (!m_enableEchoCancellation) {
        uint32_t param = 0;
        err = AudioUnitSetProperty(m_ioUnit, kAUVoiceIOProperty_VoiceProcessingEnableAGC, kAudioUnitScope_Global, inputBus, &param, sizeof(param));
        if (err) {
            LOG(Media, "CoreAudioCaptureSource::setupAudioUnit(%p) unable to set vpio automatic gain control, error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
        param = 1;
        err = AudioUnitSetProperty(m_ioUnit, kAUVoiceIOProperty_BypassVoiceProcessing, kAudioUnitScope_Global, inputBus, &param, sizeof(param));
        if (err) {
            LOG(Media, "CoreAudioCaptureSource::setupAudioUnit(%p) unable to set vpio unit echo cancellation, error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }

#if PLATFORM(IOS)
    uint32_t param = 1;
    err = AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, inputBus, &param, sizeof(param));
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnit(%p) unable to enable vpio unit input, error %d (%.4s)", this, (int)err, (char*)&err);
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
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnit(%p) unable to set vpio unit capture device ID, error %d (%.4s)", this, (int)err, (char*)&err);
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
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnit(%p) AudioUnitInitialize() failed, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
    m_ioUnitInitialized = true;

    return err;
}

OSStatus CoreAudioSharedUnit::configureMicrophoneProc()
{
    AURenderCallbackStruct callback = { microphoneCallback, this };
    auto err = AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, inputBus, &callback, sizeof(callback));
    if (err) {
        LOG(Media, "CoreAudioSharedUnit::configureMicrophoneProc(%p) unable to set vpio unit mic proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription microphoneProcFormat = { };

    UInt32 size = sizeof(microphoneProcFormat);
    err = AudioUnitGetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, inputBus, &microphoneProcFormat, &size);
    if (err) {
        LOG(Media, "CoreAudioSharedUnit::configureMicrophoneProc(%p) unable to get output stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    microphoneProcFormat.mSampleRate = m_sampleRate;
    err = AudioUnitSetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, inputBus, &microphoneProcFormat, size);
    if (err) {
        LOG(Media, "CoreAudioSharedUnit::configureMicrophoneProc(%p) unable to set output stream format, error %d (%.4s)", this, (int)err, (char*)&err);
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
        LOG(Media, "CoreAudioSharedUnit::configureSpeakerProc(%p) unable to set vpio unit speaker proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription speakerProcFormat = { };

    UInt32 size = sizeof(speakerProcFormat);
    err = AudioUnitGetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, outputBus, &speakerProcFormat, &size);
    if (err) {
        LOG(Media, "CoreAudioSharedUnit::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    speakerProcFormat.mSampleRate = m_sampleRate;
    err = AudioUnitSetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, outputBus, &speakerProcFormat, size);
    if (err) {
        LOG(Media, "CoreAudioSharedUnit::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
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
        LOG(Media, "CoreAudioSharedUnit::checkTimestamps: unusual timestamps, sample time = %lld, previous sample time = %lld, hostTime %f", sampleTime, m_latestMicTimeStamp, hostTime);
}
#endif

OSStatus CoreAudioSharedUnit::provideSpeakerData(AudioUnitRenderActionFlags& /*ioActionFlags*/, const AudioTimeStamp& timeStamp, UInt32 /*inBusNumber*/, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    // Called when the audio unit needs data to play through the speakers.
#if !LOG_DISABLED
    ++m_speakerProcsCalled;
#endif

    if (m_speakerSampleBuffer->sampleCapacity() < inNumberFrames) {
        LOG(Media, "CoreAudioSharedUnit::provideSpeakerData: speaker sample buffer size (%d) too small for amount of sample data requested (%d)!", m_speakerSampleBuffer->sampleCapacity(), (int)inNumberFrames);
        return kAudio_ParamError;
    }

    // Add/remove sources from the queue, but only if we get the lock immediately. Otherwise try
    // again on the next callback.
    {
        std::unique_lock<Lock> lock(m_pendingSourceQueueLock, std::try_to_lock);
        if (lock.owns_lock()) {
            for (auto& pair : m_pendingSources) {
                if (pair.first == QueueAction::Add)
                    m_activeSources.append(pair.second.copyRef());
                else {
                    auto removeSource = pair.second.copyRef();
                    m_activeSources.removeFirstMatching([&removeSource](auto& source) {
                        return source.ptr() == removeSource.ptr();
                    });
                }
            }
            m_pendingSources.clear();
        }
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
#if !LOG_DISABLED
    ++m_microphoneProcsCalled;
#endif

    // Pull through the vpio unit to our mic buffer.
    m_microphoneSampleBuffer->reset();
    AudioBufferList& bufferList = m_microphoneSampleBuffer->bufferList();
    auto err = AudioUnitRender(m_ioUnit, &ioActionFlags, &timeStamp, inBusNumber, inNumberFrames, &bufferList);
    if (err) {
        LOG(Media, "CoreAudioSharedUnit::processMicrophoneSamples(%p) AudioUnitRender failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    double adjustedHostTime = m_DTSConversionRatio * timeStamp.mHostTime;
    uint64_t sampleTime = timeStamp.mSampleTime;
#if !LOG_DISABLED
    checkTimestamps(timeStamp, sampleTime, adjustedHostTime);
#endif
    m_latestMicTimeStamp = sampleTime;
    m_microphoneSampleBuffer->setTimes(adjustedHostTime, sampleTime);

    if (m_volume != 1.0)
        m_microphoneSampleBuffer->applyGain(m_volume);

    for (CoreAudioCaptureSource& client : m_clients) {
        if (client.isProducingData())
            client.audioSamplesAvailable(MediaTime(sampleTime, m_microphoneProcFormat.sampleRate()), m_microphoneSampleBuffer->bufferList(), m_microphoneProcFormat, inNumberFrames);
    }
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
            LOG(Media, "CoreAudioSharedUnit::cleanupAudioUnit(%p) AudioUnitUninitialize failed with error %d (%.4s)", this, (int)err, (char*)&err);
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
            LOG(Media, "CoreAudioSharedUnit::reconfigureAudioUnit(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
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
            LOG(Media, "CoreAudioSharedUnit::reconfigureAudioUnit(%p) AudioOutputUnitStart failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
    }
    return err;
}

void CoreAudioSharedUnit::startProducingData()
{
    ASSERT(isMainThread());

    if (++m_producingCount != 1)
        return;

    if (m_ioUnitStarted)
        return;

    OSStatus err;
    if (!m_ioUnit) {
        err = setupAudioUnit();
        if (err) {
            cleanupAudioUnit();
            ASSERT(!m_ioUnit);
            return;
        }
        ASSERT(m_ioUnit);
    }

    err = AudioOutputUnitStart(m_ioUnit);
    if (err) {
        LOG(Media, "CoreAudioSharedUnit::start(%p) AudioOutputUnitStart failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return;
    }

    m_ioUnitStarted = true;
}

void CoreAudioSharedUnit::stopProducingData()
{
    ASSERT(isMainThread());

    if (--m_producingCount)
        return;

    if (!m_ioUnit || !m_ioUnitStarted)
        return;

    auto err = AudioOutputUnitStop(m_ioUnit);
    if (err) {
        LOG(Media, "CoreAudioSharedUnit::stop(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return;
    }

    m_ioUnitStarted = false;
}

OSStatus CoreAudioSharedUnit::suspend()
{
    ASSERT(isMainThread());

    m_activeSources.clear();
    m_pendingSources.clear();

    if (m_ioUnitStarted) {
        ASSERT(m_ioUnit);
        auto err = AudioOutputUnitStop(m_ioUnit);
        if (err) {
            LOG(Media, "CoreAudioSharedUnit::resume(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
        m_ioUnitStarted = false;
    }

    if (m_ioUnitInitialized) {
        ASSERT(m_ioUnit);
        auto err = AudioUnitUninitialize(m_ioUnit);
        if (err) {
            LOG(Media, "CoreAudioSharedUnit::resume(%p) AudioUnitUninitialize failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
        m_ioUnitInitialized = false;
    }

    return 0;
}

OSStatus CoreAudioSharedUnit::defaultInputDevice(uint32_t* deviceID)
{
    ASSERT(m_ioUnit);

    UInt32 propertySize = sizeof(*deviceID);
    auto err = AudioUnitGetProperty(m_ioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, inputBus, deviceID, &propertySize);
    if (err)
        LOG(Media, "CoreAudioSharedUnit::defaultInputDevice(%p) unable to get default input device ID, error %d (%.4s)", this, (int)err, (char*)&err);

    return err;
}

CaptureSourceOrError CoreAudioCaptureSource::create(const String& deviceID, const MediaConstraints* constraints)
{
    String label;
    uint32_t persistentID = 0;
#if PLATFORM(MAC)
    auto device = CoreAudioCaptureDeviceManager::singleton().coreAudioDeviceWithUID(deviceID);
    if (!device)
        return { };

    label = device->label();
    persistentID = device->deviceID();
#elif PLATFORM(IOS)
    auto device = AVAudioSessionCaptureDeviceManager::singleton().audioSessionDeviceWithUID(deviceID);
    if (!device)
        return { };

    label = device->label();
#endif
    auto source = adoptRef(*new CoreAudioCaptureSource(deviceID, label, persistentID));

    if (constraints) {
        auto result = source->applyConstraints(*constraints);
        if (result)
            return WTFMove(result.value().first);
    }
    return CaptureSourceOrError(WTFMove(source));
}

RealtimeMediaSource::AudioCaptureFactory& CoreAudioCaptureSource::factory()
{
    return coreAudioCaptureSourceFactory();
}

CoreAudioCaptureSource::CoreAudioCaptureSource(const String& deviceID, const String& label, uint32_t persistentID)
    : RealtimeMediaSource(deviceID, RealtimeMediaSource::Type::Audio, label)
    , m_captureDeviceID(persistentID)
{
    auto& unit = CoreAudioSharedUnit::singleton();

    initializeEchoCancellation(unit.enableEchoCancellation());
    initializeSampleRate(unit.sampleRate());
    initializeVolume(unit.volume());

    unit.addClient(*this);
}

CoreAudioCaptureSource::~CoreAudioCaptureSource()
{
#if PLATFORM(IOS)
    coreAudioCaptureSourceFactory().unsetActiveSource(*this);
#endif

    CoreAudioSharedUnit::singleton().removeClient(*this);
}

void CoreAudioCaptureSource::addEchoCancellationSource(AudioSampleDataSource& source)
{
    CoreAudioSharedUnit::singleton().addEchoCancellationSource(source);
}

void CoreAudioCaptureSource::removeEchoCancellationSource(AudioSampleDataSource& source)
{
    CoreAudioSharedUnit::singleton().removeEchoCancellationSource(source);
}

void CoreAudioCaptureSource::startProducingData()
{
#if PLATFORM(IOS)
    coreAudioCaptureSourceFactory().setActiveSource(*this);
#endif

    CoreAudioSharedUnit::singleton().startProducingData();

    if (m_audioSourceProvider)
        m_audioSourceProvider->prepare(&CoreAudioSharedUnit::singleton().microphoneFormat().streamDescription());
}

void CoreAudioCaptureSource::stopProducingData()
{
    CoreAudioSharedUnit::singleton().stopProducingData();

    if (m_audioSourceProvider)
        m_audioSourceProvider->unprepare();
}

const RealtimeMediaSourceCapabilities& CoreAudioCaptureSource::capabilities() const
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());
        capabilities.setDeviceId(id());
        capabilities.setEchoCancellation(RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite);
        capabilities.setVolume(CapabilityValueOrRange(0.0, 1.0));
        capabilities.setSampleRate(CapabilityValueOrRange(8000, 96000));
        m_capabilities = WTFMove(capabilities);
    }
    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& CoreAudioCaptureSource::settings() const
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setVolume(volume());
        settings.setSampleRate(sampleRate());
        settings.setDeviceId(id());
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

void CoreAudioCaptureSource::settingsDidChange()
{
    m_currentSettings = std::nullopt;
    RealtimeMediaSource::settingsDidChange();
}

AudioSourceProvider* CoreAudioCaptureSource::audioSourceProvider()
{
    if (!m_audioSourceProvider) {
        m_audioSourceProvider = WebAudioSourceProviderAVFObjC::create(*this);
        if (isProducingData())
            m_audioSourceProvider->prepare(&CoreAudioSharedUnit::singleton().microphoneFormat().streamDescription());
    }

    return m_audioSourceProvider.get();
}

bool CoreAudioCaptureSource::applySampleRate(int sampleRate)
{
    // FIXME: We should be able to describe sample rate as a discreet range constraint so that we only enter here with values that can be applied.
    switch (sampleRate) {
    case 8000:
    case 16000:
    case 32000:
    case 44100:
    case 48000:
    case 96000:
        break;
    default:
        return false;
    }

    CoreAudioSharedUnit::singleton().setSampleRate(sampleRate);

    scheduleReconfiguration();
    return true;
}

bool CoreAudioCaptureSource::applyEchoCancellation(bool enableEchoCancellation)
{
    CoreAudioSharedUnit::singleton().setEnableEchoCancellation(enableEchoCancellation);

    scheduleReconfiguration();
    return true;
}

void CoreAudioCaptureSource::scheduleReconfiguration()
{
    ASSERT(isMainThread());
    auto& unit = CoreAudioSharedUnit::singleton();
    if (!unit.hasAudioUnit() || m_reconfigurationOngoing)
        return;

    m_reconfigurationOngoing = true;
    scheduleDeferredTask([this, &unit] {
        unit.reconfigureAudioUnit();
        m_reconfigurationOngoing = false;
    });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
