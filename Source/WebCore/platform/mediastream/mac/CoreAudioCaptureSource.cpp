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
#include "Logging.h"
#include "MediaTimeAVFoundation.h"
#include <AudioToolbox/AudioConverter.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreMedia/CMSync.h>
#include <mach/mach_time.h>
#include <sys/time.h>
#include <wtf/NeverDestroyed.h>
#include "CoreMediaSoftLink.h"

namespace WebCore {

class CoreAudioCaptureSourceFactory : public RealtimeMediaSource::CaptureFactory {
public:
    RefPtr<RealtimeMediaSource> createMediaSourceForCaptureDeviceWithConstraints(const CaptureDevice& captureDevice, const MediaConstraints* constraints, String& invalidConstraint) final {
        return CoreAudioCaptureSource::create(captureDevice, constraints, invalidConstraint);
    }
};

const UInt32 outputBus = 0;
const UInt32 inputBus = 1;

RefPtr<CoreAudioCaptureSource> CoreAudioCaptureSource::create(const CaptureDevice& deviceInfo, const MediaConstraints* constraints, String& invalidConstraint)
{
    auto source = adoptRef(new CoreAudioCaptureSource(deviceInfo));
    if (constraints) {
        auto result = source->applyConstraints(*constraints);
        if (result) {
            invalidConstraint = result.value().first;
            return nullptr;
        }
    }

    return source;
}


RealtimeMediaSource::CaptureFactory& CoreAudioCaptureSource::factory()
{
    static NeverDestroyed<CoreAudioCaptureSourceFactory> factory;
    return factory.get();
}


CoreAudioCaptureSource::CoreAudioCaptureSource(const CaptureDevice& deviceInfo)
    : RealtimeMediaSource(emptyString(), RealtimeMediaSource::Type::Audio, deviceInfo.label())
    , m_captureDeviceID(0)
{
    // FIXME: use deviceInfo to set the m_captureDeviceID

    setPersistentID(deviceInfo.persistentId());
    setMuted(true);

    m_currentSettings.setVolume(1.0);
    m_currentSettings.setSampleRate(preferredSampleRate());
    m_currentSettings.setDeviceId(id());
    m_currentSettings.setEchoCancellation(true);

    mach_timebase_info_data_t timebaseInfo;
    mach_timebase_info(&timebaseInfo);
    m_DTSConversionRatio = 1e-9 * static_cast<double>(timebaseInfo.numer) / static_cast<double>(timebaseInfo.denom);
}

CoreAudioCaptureSource::~CoreAudioCaptureSource()
{
    suspend();

    std::lock_guard<Lock> lock(m_internalStateLock);

    if (m_ioUnit)
        AudioComponentInstanceDispose(m_ioUnit);

    m_ioUnitName = emptyString();

    m_microphoneSampleBuffer = nullptr;
    m_speakerSampleBuffer = nullptr;

    m_speakerProcsCalled = 0;
    m_microphoneProcsCalled  = 0;
    m_latestMicTimeStamp = 0;

    m_activeSources.clear();
    m_pendingSources.clear();

    m_speakerSampleBuffer = nullptr;

    m_ioUnitInitialized = false;
    m_ioUnitStarted  = false;
}

double CoreAudioCaptureSource::preferredSampleRate()
{
    // FIXME: Get the preferred rate dynamically, kAUVoiceIOProperty_PreferredHWSampleRate/ [[AVAudioSession sharedInstance] preferredSampleRate]
    static const float preferredRate = 24000.;
    return preferredRate;
}

double CoreAudioCaptureSource::preferredIOBufferDuration()
{
    // FIXME: Get the preferred duration dynamically - kAUVoiceIOProperty_PreferredHWBlockSizeInSeconds / [[AVAudioSession sharedInstance] preferredIOBufferDuration]
    static const float preferredDuration = 0.02;
    return preferredDuration;
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
    err = AudioUnitGetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, inputBus, &microphoneProcFormat, &size);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::configureMicrophoneProc(%p) unable to get output stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    m_microphoneSampleBuffer = AudioSampleBufferList::create(microphoneProcFormat, preferredIOBufferDuration() * microphoneProcFormat.mSampleRate * 2);
    m_microphoneProcFormat = microphoneProcFormat;

    return err;
}

OSStatus CoreAudioCaptureSource::configureSpeakerProc()
{
    AURenderCallbackStruct callback = { speakerCallback, this };
    auto err = AudioUnitSetProperty(m_ioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Global, outputBus, &callback, sizeof(callback));
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::configureSpeakerProc(%p) unable to set vpio unit speaker proc, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    AudioStreamBasicDescription speakerProcFormat = { };

    UInt32 size = sizeof(speakerProcFormat);
    err  = AudioUnitGetProperty(m_ioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, outputBus, &speakerProcFormat, &size);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::configureSpeakerProc(%p) unable to get input stream format, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    m_speakerSampleBuffer = AudioSampleBufferList::create(speakerProcFormat, preferredIOBufferDuration() * speakerProcFormat.mSampleRate * 2);
    m_speakerProcFormat = speakerProcFormat;

    return err;
}

uint64_t CoreAudioCaptureSource::addMicrophoneDataConsumer(MicrophoneDataCallback&& callback)
{
    std::lock_guard<Lock> lock(m_pendingSourceQueueLock);
    m_microphoneDataCallbacks.add(++m_nextMicrophoneDataCallbackID, callback);

    return m_nextMicrophoneDataCallbackID;
}

void CoreAudioCaptureSource::removeMicrophoneDataConsumer(uint64_t callbackID)
{
    std::lock_guard<Lock> lock(m_pendingSourceQueueLock);
    m_microphoneDataCallbacks.remove(callbackID);
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

void CoreAudioCaptureSource::checkTimestamps(const AudioTimeStamp& timeStamp, uint64_t sampleTime, double hostTime)
{
    if (!timeStamp.mSampleTime || sampleTime == m_latestMicTimeStamp || !hostTime)
        LOG(Media, "CoreAudioCaptureSource::checkTimestamps: unusual timestamps, sample time = %lld, previous sample time = %lld, hostTime %f", sampleTime, m_latestMicTimeStamp, hostTime);
}

OSStatus CoreAudioCaptureSource::provideSpeakerData(AudioUnitRenderActionFlags& /*ioActionFlags*/, const AudioTimeStamp& timeStamp, UInt32 /*inBusNumber*/, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    // Called when the audio unit needs data to play through the speakers.
    ++m_speakerProcsCalled;

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
    checkTimestamps(timeStamp, sampleTime, adjustedHostTime);

    m_speakerSampleBuffer->reset();
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
    ++m_microphoneProcsCalled;

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
    checkTimestamps(timeStamp, sampleTime, adjustedHostTime);
    m_latestMicTimeStamp = sampleTime;

    m_microphoneSampleBuffer->setTimes(adjustedHostTime, sampleTime);

    audioSamplesAvailable(MediaTime(sampleTime, m_microphoneProcFormat.sampleRate()), m_microphoneSampleBuffer->bufferList(), m_microphoneProcFormat, inNumberFrames);

    if (m_microphoneDataCallbacks.isEmpty())
        return 0;

    for (auto& callback : m_microphoneDataCallbacks.values())
        callback(MediaTime(sampleTime, m_microphoneProcFormat.sampleRate()), m_microphoneSampleBuffer->bufferList(), m_microphoneProcFormat, inNumberFrames);

    return noErr;
}

OSStatus CoreAudioCaptureSource::microphoneCallback(void *inRefCon, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    ASSERT(ioActionFlags);
    ASSERT(inTimeStamp);
    CoreAudioCaptureSource* dataSource = static_cast<CoreAudioCaptureSource*>(inRefCon);
    return dataSource->processMicrophoneSamples(*ioActionFlags, *inTimeStamp, inBusNumber, inNumberFrames, ioData);
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
    ASSERT(m_internalStateLock.isHeld());

    if (m_ioUnit)
        return 0;

    AudioComponentDescription ioUnitDescription = { kAudioUnitType_Output, kAudioUnitSubType_VoiceProcessingIO, kAudioUnitManufacturer_Apple, 0, 0 };
    AudioComponent ioComponent = AudioComponentFindNext(nullptr, &ioUnitDescription);
    ASSERT(ioComponent);
    if (!ioComponent) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) unable to find vpio unit component", this);
        return -1;
    }

    CFStringRef name = nullptr;
    AudioComponentCopyName(ioComponent, &name);
    if (name) {
        m_ioUnitName = name;
        CFRelease(name);
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) created \"%s\" component", this, m_ioUnitName.utf8().data());
    }

    auto err = AudioComponentInstanceNew(ioComponent, &m_ioUnit);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) unable to open vpio unit, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

#if PLATFORM(IOS)
    uint32_t param = 1;
    err = AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, inputBus, &param, sizeof(param));
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) unable to enable vpio unit input, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

    param = 1;
    err = AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, outputBus, &param, sizeof(param));
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) unable to enable vpio unit output, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }
#endif

    if (!m_captureDeviceID) {
        err = defaultInputDevice(&m_captureDeviceID);
        if (err)
            return err;
    }

    UInt32 propertySize = sizeof(m_captureDeviceID);
    err = AudioUnitSetProperty(m_ioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, inputBus, &m_captureDeviceID, propertySize);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::setupAudioUnits(%p) unable to set vpio unit capture device ID, error %d (%.4s)", this, (int)err, (char*)&err);
        return err;
    }

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
    std::lock_guard<Lock> lock(m_internalStateLock);
    if (m_ioUnitStarted)
        return;

    OSStatus err;
    if (!m_ioUnit) {
        err = setupAudioUnits();
        if (err)
            return;
    }

    err = AudioOutputUnitStart(m_ioUnit);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::start(%p) AudioOutputUnitStart failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return;
    }

    m_ioUnitStarted = true;
    setMuted(false);
}

void CoreAudioCaptureSource::stopProducingData()
{
    std::lock_guard<Lock> lock(m_internalStateLock);

    ASSERT(m_ioUnit);

    auto err = AudioOutputUnitStop(m_ioUnit);
    if (err) {
        LOG(Media, "CoreAudioCaptureSource::stop(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
        return;
    }
    m_ioUnitStarted = false;
    setMuted(true);
}

OSStatus CoreAudioCaptureSource::suspend()
{
    std::lock_guard<Lock> lock(m_internalStateLock);

    ASSERT(m_ioUnit);

    if (m_ioUnitStarted) {
        auto err = AudioOutputUnitStop(m_ioUnit);
        if (err) {
            LOG(Media, "CoreAudioCaptureSource::resume(%p) AudioOutputUnitStop failed with error %d (%.4s)", this, (int)err, (char*)&err);
            return err;
        }
        m_ioUnitStarted = false;
    }

    if (m_ioUnitInitialized) {
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
    std::lock_guard<Lock> lock(m_internalStateLock);

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
    return m_currentSettings;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
