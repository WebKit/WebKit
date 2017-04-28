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

#include "AudioSampleBufferList.h"
#include "AudioSampleDataSource.h"
#include "AudioSession.h"
#include "CoreAudioCaptureDevice.h"
#include "CoreAudioCaptureDeviceManager.h"
#include "Logging.h"
#include "MediaTimeAVFoundation.h"
#include <AudioToolbox/AudioConverter.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreMedia/CMSync.h>
#include <mach/mach_time.h>
#include <sys/time.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include "CoreMediaSoftLink.h"

namespace WebCore {

class CoreAudioCaptureSourceFactory : public RealtimeMediaSource::AudioCaptureFactory {
public:
    CaptureSourceOrError createAudioCaptureSource(const String& deviceID, const MediaConstraints* constraints) final {
        return CoreAudioCaptureSource::create(deviceID, constraints);
    }
};

const UInt32 outputBus = 0;
const UInt32 inputBus = 1;

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
    static NeverDestroyed<CoreAudioCaptureSourceFactory> factory;
    return factory.get();
}

CoreAudioCaptureSource::CoreAudioCaptureSource(const String& deviceID, const String& label, uint32_t persistentID)
    : RealtimeMediaSource(deviceID, RealtimeMediaSource::Type::Audio, label)
    , m_captureDeviceID(persistentID)
{
    m_muted = true;

    setVolume(1.0);
    setSampleRate(preferredSampleRate());
    setEchoCancellation(true);

    mach_timebase_info_data_t timebaseInfo;
    mach_timebase_info(&timebaseInfo);
    m_DTSConversionRatio = 1e-9 * static_cast<double>(timebaseInfo.numer) / static_cast<double>(timebaseInfo.denom);
}

CoreAudioCaptureSource::~CoreAudioCaptureSource()
{
    suspend();
    cleanupAudioUnits();

    m_activeSources.clear();
    m_pendingSources.clear();

#if !LOG_DISABLED
    m_speakerProcsCalled = 0;
    m_microphoneProcsCalled  = 0;
#endif
}

double CoreAudioCaptureSource::preferredSampleRate()
{
    return AudioSession::sharedSession().sampleRate();
}

size_t CoreAudioCaptureSource::preferredIOBufferSize()
{
    return AudioSession::sharedSession().bufferSize();
}

OSStatus CoreAudioCaptureSource::configureMicrophoneProc()
{
    AURenderCallbackStruct callback = { microphoneCallback, this };
    auto err = AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, inputBus, &callback, sizeof(callback));
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::configureMicrophoneProc(%p) unable to set vpio unit mic proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription microphoneProcFormat = { };

    UInt32 size = sizeof(microphoneProcFormat);
    err = AudioUnitGetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, inputBus, &microphoneProcFormat, &size);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::configureMicrophoneProc(%p) unable to get output stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    microphoneProcFormat.mSampleRate = sampleRate();
    err = AudioUnitSetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, inputBus, &microphoneProcFormat, size);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::configureMicrophoneProc(%p) unable to set output stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    m_microphoneSampleBuffer = AudioSampleBufferList::create(microphoneProcFormat, preferredIOBufferSize() * 2);
    m_microphoneProcFormat = microphoneProcFormat;

    return err;
}

OSStatus CoreAudioCaptureSource::configureSpeakerProc()
{
    AURenderCallbackStruct callback = { speakerCallback, this };
    auto err = AudioUnitSetProperty(m_ioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, outputBus, &callback, sizeof(callback));
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::configureSpeakerProc(%p) unable to set vpio unit speaker proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription speakerProcFormat = { };

    UInt32 size = sizeof(speakerProcFormat);
    err = AudioUnitGetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, outputBus, &speakerProcFormat, &size);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    speakerProcFormat.mSampleRate = sampleRate();
    err = AudioUnitSetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, outputBus, &speakerProcFormat, size);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    m_speakerSampleBuffer = AudioSampleBufferList::create(speakerProcFormat, preferredIOBufferSize() * 2);
    m_speakerProcFormat = speakerProcFormat;

    return err;
}

void CoreAudioCaptureSource::addEchoCancellationSource(AudioSampleDataSource& source)
{
    if (!source.setOutputFormat(m_speakerProcFormat)) {
        LOG(Media, "CoreAudioCaptureSource::addEchoCancellationSource: source %p configureOutput failed", &source);
        return;
    }

    std::lock_guard<Lock> lock(m_pendingSourceQueueLock);
    m_pendingSources.append({ QueueAction::Add, source });
}

void CoreAudioCaptureSource::removeEchoCancellationSource(AudioSampleDataSource& source)
{
    std::lock_guard<Lock> lock(m_pendingSourceQueueLock);
    m_pendingSources.append({ QueueAction::Remove, source });
}

#if !LOG_DISABLED
void CoreAudioCaptureSource::checkTimestamps(const AudioTimeStamp& timeStamp, uint64_t sampleTime, double hostTime)
{
    if (!timeStamp.mSampleTime || sampleTime == m_latestMicTimeStamp || !hostTime)
        LOG(Media, "CoreAudioCaptureSource::checkTimestamps: unusual timestamps, sample time = %lld, previous sample time = %lld, hostTime %f", sampleTime, m_latestMicTimeStamp, hostTime);
}
#endif

OSStatus CoreAudioCaptureSource::provideSpeakerData(AudioUnitRenderActionFlags& /*ioActionFlags*/, const AudioTimeStamp& timeStamp, UInt32 /*inBusNumber*/, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    // Called when the audio unit needs data to play through the speakers.
#if !LOG_DISABLED
    ++m_speakerProcsCalled;
#endif

    if (m_speakerSampleBuffer->sampleCapacity() < inNumberFrames) {
        LOG(Media, "CoreAudioCaptureSource::provideSpeakerData: speaker sample buffer size (%d) too small for amount of sample data requested (%d)!", m_speakerSampleBuffer->sampleCapacity(), (int)inNumberFrames);
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

OSStatus CoreAudioCaptureSource::speakerCallback(void *inRefCon, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    ASSERT(ioActionFlags);
    ASSERT(inTimeStamp);
    auto dataSource = static_cast<CoreAudioCaptureSource*>(inRefCon);
    return dataSource->provideSpeakerData(*ioActionFlags, *inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

OSStatus CoreAudioCaptureSource::processMicrophoneSamples(AudioUnitRenderActionFlags& ioActionFlags, const AudioTimeStamp& timeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* /*ioData*/)
{
#if !LOG_DISABLED
    ++m_microphoneProcsCalled;
#endif

    // Pull through the vpio unit to our mic buffer.
    m_microphoneSampleBuffer->reset();
    AudioBufferList& bufferList = m_microphoneSampleBuffer->bufferList();
    auto err = AudioUnitRender(m_ioUnit, &ioActionFlags, &timeStamp, inBusNumber, inNumberFrames, &bufferList);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::processMicrophoneSamples(%p) AudioUnitRender failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    double adjustedHostTime = m_DTSConversionRatio * timeStamp.mHostTime;
    uint64_t sampleTime = timeStamp.mSampleTime;
#if !LOG_DISABLED
    checkTimestamps(timeStamp, sampleTime, adjustedHostTime);
#endif
    m_latestMicTimeStamp = sampleTime;
    m_microphoneSampleBuffer->setTimes(adjustedHostTime, sampleTime);


    audioSamplesAvailable(MediaTime(sampleTime, m_microphoneProcFormat.sampleRate()), m_microphoneSampleBuffer->bufferList(), m_microphoneProcFormat, inNumberFrames);

    return noErr;
}

OSStatus CoreAudioCaptureSource::microphoneCallback(void *inRefCon, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    ASSERT(ioActionFlags);
    ASSERT(inTimeStamp);
    CoreAudioCaptureSource* dataSource = static_cast<CoreAudioCaptureSource*>(inRefCon);
    return dataSource->processMicrophoneSamples(*ioActionFlags, *inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

void CoreAudioCaptureSource::cleanupAudioUnits()
{
    if (m_ioUnitInitialized) {
        ASSERT(m_ioUnit);
        auto err = AudioUnitUninitialize(m_ioUnit);
        if (err)
            LOG(Media, "CoreAudioCaptureSource::cleanupAudioUnits(%p) AudioUnitUninitialize failed with error %d (%.4s)", this, (int)err, (char*)&err);
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

OSStatus CoreAudioCaptureSource::defaultInputDevice(uint32_t* deviceID)
{
    ASSERT(m_ioUnit);

    UInt32 propertySize = sizeof(*deviceID);
    auto err = AudioUnitGetProperty(m_ioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, inputBus, deviceID, &propertySize);
    if (err)
        LOG(Media, "CoreAudioCaptureSource::defaultInputDevice(%p) unable to get default input device ID, error %d (%.4s)", this, (int)err, (char*)&err);

    return err;
}

OSStatus CoreAudioCaptureSource::setupAudioUnits()
{
    if (m_ioUnit)
        return 0;

    AudioComponentDescription ioUnitDescription = { kAudioUnitType_Output, kAudioUnitSubType_VoiceProcessingIO, kAudioUnitManufacturer_Apple, 0, 0 };
    AudioComponent ioComponent = AudioComponentFindNext(nullptr, &ioUnitDescription);
    ASSERT(ioComponent);
    if (!ioComponent) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) unable to find vpio unit component", this);
        return -1;
    }

#if !LOG_DISABLED
    CFStringRef name = nullptr;
    AudioComponentCopyName(ioComponent, &name);
    if (name) {
        m_ioUnitName = name;
        CFRelease(name);
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) created \"%s\" component", this, m_ioUnitName.utf8().data());
    }
#endif

    auto err = AudioComponentInstanceNew(ioComponent, &m_ioUnit);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) unable to open vpio unit, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    uint32_t param = echoCancellation();
    err = AudioUnitSetProperty(m_ioUnit, kAUVoiceIOProperty_VoiceProcessingEnableAGC, kAudioUnitScope_Global, inputBus, &param, sizeof(param));
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) unable to set vpio unit echo cancellation, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

#if PLATFORM(IOS)
    param = 1;
    err = AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, inputBus, &param, sizeof(param));
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) unable to enable vpio unit input, error %d (%.4s)", this, (int)err, (char*)&err);
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
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) unable to set vpio unit capture device ID, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
#endif

    err = configureMicrophoneProc();
    if (err)
        return err;

    err = AudioUnitInitialize(m_ioUnit);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) AudioUnitInitialize() failed, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
    m_ioUnitInitialized = true;

    err = configureSpeakerProc();
    if (err)
        return err;

    return err;
}

void CoreAudioCaptureSource::startProducingData()
{
    ASSERT(isMainThread());

    if (m_ioUnitStarted)
        return;

    OSStatus err;
    if (!m_ioUnit) {
        err = setupAudioUnits();
        if (err) {
            cleanupAudioUnits();
            ASSERT(!m_ioUnit);
            return;
        }
        ASSERT(m_ioUnit);
    }

    err = AudioOutputUnitStart(m_ioUnit);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::start(%p) AudioOutputUnitStart failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return;
    }

    m_ioUnitStarted = true;
    m_muted = false;
}

void CoreAudioCaptureSource::stopProducingData()
{
    ASSERT(isMainThread());

    if (!m_ioUnit || !m_ioUnitStarted)
        return;

    auto err = AudioOutputUnitStop(m_ioUnit);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::stop(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return;
    }

    m_ioUnitStarted = false;
    m_muted = true;
}

OSStatus CoreAudioCaptureSource::suspend()
{
    ASSERT(isMainThread());

    if (m_ioUnitStarted) {
        ASSERT(m_ioUnit);
        auto err = AudioOutputUnitStop(m_ioUnit);
        if (err) {
            LOG(Media, "CoreAudioCaptureSource::resume(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
        m_ioUnitStarted = false;
    }

    if (m_ioUnitInitialized) {
        ASSERT(m_ioUnit);
        auto err = AudioUnitUninitialize(m_ioUnit);
        if (err) {
            LOG(Media, "CoreAudioCaptureSource::resume(%p) AudioUnitUninitialize failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
        m_ioUnitInitialized = false;
    }

    return 0;
}

OSStatus CoreAudioCaptureSource::resume()
{
    ASSERT(isMainThread());
    ASSERT(m_ioUnit);
    ASSERT(!m_ioUnitStarted);

    auto err = AudioOutputUnitStart(m_ioUnit);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::resume(%p) AudioOutputUnitStart failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
    m_ioUnitStarted = false;

    return err;
}

const RealtimeMediaSourceCapabilities& CoreAudioCaptureSource::capabilities() const
{
    if (m_capabilities)
        return *m_capabilities;

    m_supportedConstraints.setSupportsDeviceId(true);
    m_supportedConstraints.setSupportsEchoCancellation(true);
    m_supportedConstraints.setSupportsVolume(true);

    // FIXME: finish this.
    m_capabilities = std::make_unique<RealtimeMediaSourceCapabilities>(m_supportedConstraints);
    m_capabilities->setDeviceId(id());
    m_capabilities->setEchoCancellation(RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite);
    m_capabilities->setVolume(CapabilityValueOrRange(0.0, 1.0));

    return *m_capabilities;
}

const RealtimeMediaSourceSettings& CoreAudioCaptureSource::settings() const
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setVolume(volume());
        settings.setSampleRate(sampleRate());
        settings.setDeviceId(id());
        settings.setEchoCancellation(echoCancellation());

        m_currentSettings = WTFMove(settings);

    }
    return m_currentSettings.value();
}

void CoreAudioCaptureSource::settingsDidChange()
{
    m_currentSettings = std::nullopt;
    RealtimeMediaSource::settingsDidChange();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
