/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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


#import "config.h"
#import "MockAudioSharedUnit.h"

#if ENABLE(MEDIA_STREAM)
#import "AudioSampleBufferList.h"
#import "AudioSession.h"
#import "BaseAudioSharedUnit.h"
#import "CAAudioStreamDescription.h"
#import "CoreAudioCaptureSource.h"
#import "MediaConstraints.h"
#import "MediaSampleAVFObjC.h"
#import "MockRealtimeMediaSourceCenter.h"
#import "NotImplemented.h"
#import "RealtimeMediaSourceSettings.h"
#import "WebAudioBufferList.h"
#import "WebAudioSourceProviderCocoa.h"
#import <AVFoundation/AVAudioBuffer.h>
#import <AudioToolbox/AudioConverter.h>
#import <CoreAudio/CoreAudioTypes.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>

#import <pal/cf/AudioToolboxSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

static inline size_t alignTo16Bytes(size_t size)
{
    return (size + 15) & ~15;
}

static const double Tau = 2 * M_PI;
static const double BipBopDuration = 0.07;
static const double BipBopVolume = 0.5;
static const double BipFrequency = 1500;
static const double BopFrequency = 500;
static const double HumFrequency = 150;
static const double HumVolume = 0.1;
static const double NoiseFrequency = 3000;
static const double NoiseVolume = 0.05;

template <typename AudioSampleType>
static void writeHum(float amplitude, float frequency, float sampleRate, AudioSampleType *p, uint64_t count)
{
    float humPeriod = sampleRate / frequency;
    for (uint64_t i = 0; i < count; ++i)
        *p++ = amplitude * sin(i * Tau / humPeriod);
}

template <typename AudioSampleType>
static void addHum(float amplitude, float frequency, float sampleRate, uint64_t start, AudioSampleType *p, uint64_t count)
{
    float humPeriod = sampleRate / frequency;
    for (uint64_t i = start, end = start + count; i < end; ++i) {
        AudioSampleType a = amplitude * sin(i * Tau / humPeriod);
        a += *p;
        *p++ = a;
    }
}

CaptureSourceOrError MockRealtimeAudioSource::create(String&& deviceID, AtomString&& name, String&& hashSalt, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    if (!device)
        return { "No mock microphone device"_s };

    MockAudioSharedUnit::singleton().setCaptureDevice(String { deviceID }, 0);
    return CoreAudioCaptureSource::createForTesting(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt), constraints, MockAudioSharedUnit::singleton(), pageIdentifier);
}

class MockAudioSharedInternalUnitState : public ThreadSafeRefCounted<MockAudioSharedInternalUnitState> {
public:
    static Ref<MockAudioSharedInternalUnitState> create() { return adoptRef(*new MockAudioSharedInternalUnitState()); }

    bool isProducingData() const { return m_isProducingData; }
    void setIsProducingData(bool value) { m_isProducingData = value; }

private:
    bool m_isProducingData { false };
};

class MockAudioSharedInternalUnit :  public CoreAudioSharedUnit::InternalUnit {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MockAudioSharedInternalUnit();
    ~MockAudioSharedInternalUnit();

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
    void delaySamples(Seconds) final;
    Seconds verifyCaptureInterval(bool) const final { return 1_s; }

    int sampleRate() const { return m_streamFormat.mSampleRate; }
    void tick();

    void generateSampleBuffers(MonotonicTime);
    void emitSampleBuffers(uint32_t frameCount);
    void reconfigure();

    static Seconds renderInterval() { return 20_ms; }

    std::unique_ptr<WebAudioBufferList> m_audioBufferList;

    size_t m_maximiumFrameCount;
    uint64_t m_samplesEmitted { 0 };
    uint64_t m_samplesRendered { 0 };

    RetainPtr<CMFormatDescriptionRef> m_formatDescription;
    AudioStreamBasicDescription m_outputStreamFormat;
    AudioStreamBasicDescription m_streamFormat;

    Vector<float> m_bipBopBuffer;
    bool m_hasAudioUnit { false };
    Ref<MockAudioSharedInternalUnitState> m_internalState;
    bool m_enableEchoCancellation { true };
    RunLoop::Timer<MockAudioSharedInternalUnit> m_timer;
    MonotonicTime m_lastRenderTime { MonotonicTime::nan() };
    MonotonicTime m_delayUntil;

    Ref<WorkQueue> m_workQueue;
    unsigned m_channelCount { 2 };
    
    AURenderCallbackStruct m_microphoneCallback;
    AURenderCallbackStruct m_speakerCallback;
};

static bool s_shouldIncreaseBufferSize;
CoreAudioSharedUnit& MockAudioSharedUnit::singleton()
{
    static NeverDestroyed<CoreAudioSharedUnit> unit;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] () {
        s_shouldIncreaseBufferSize = false;
        unit->setSampleRateRange(CapabilityValueOrRange(44100, 48000));
        unit->setInternalUnitCreationCallback([] {
            UniqueRef<CoreAudioSharedUnit::InternalUnit> result = makeUniqueRef<MockAudioSharedInternalUnit>();
            return result;
        });
        unit->setInternalUnitGetSampleRateCallback([] { return 44100; });
    });
    return unit;
}

void MockAudioSharedUnit::increaseBufferSize()
{
    s_shouldIncreaseBufferSize = true;
}

static AudioStreamBasicDescription createAudioFormat(Float64 sampleRate, UInt32 channelCount)
{
    AudioStreamBasicDescription format;
    const int bytesPerFloat = sizeof(Float32);
    const int bitsPerByte = 8;
    const bool isFloat = true;
    const bool isBigEndian = false;
    const bool isNonInterleaved = true;
    FillOutASBDForLPCM(format, sampleRate, channelCount, bitsPerByte * bytesPerFloat, bitsPerByte * bytesPerFloat, isFloat, isBigEndian, isNonInterleaved);
    return format;
}

MockAudioSharedInternalUnit::MockAudioSharedInternalUnit()
    : m_internalState(MockAudioSharedInternalUnitState::create())
    , m_timer(RunLoop::current(), [this] { this->start(); })
    , m_workQueue(WorkQueue::create("MockAudioSharedInternalUnit Capture Queue", WorkQueue::QOS::UserInteractive))
{
    m_streamFormat = m_outputStreamFormat = createAudioFormat(44100, 2);
}

MockAudioSharedInternalUnit::~MockAudioSharedInternalUnit()
{
    ASSERT(!m_internalState->isProducingData());
}

OSStatus MockAudioSharedInternalUnit::initialize()
{
    ASSERT(m_outputStreamFormat.mSampleRate == m_streamFormat.mSampleRate);
    if (m_outputStreamFormat.mSampleRate != m_streamFormat.mSampleRate)
        return -1;

    return 0;
}

OSStatus MockAudioSharedInternalUnit::start()
{
    if (!m_hasAudioUnit)
        m_hasAudioUnit = true;

    m_lastRenderTime = MonotonicTime::now();

    m_internalState->setIsProducingData(true);
    m_workQueue->dispatch([this, renderTime = m_lastRenderTime] {
        generateSampleBuffers(renderTime);
    });
    return 0;
}

OSStatus MockAudioSharedInternalUnit::stop()
{
    m_internalState->setIsProducingData(false);
    if (m_hasAudioUnit)
        m_lastRenderTime = MonotonicTime::nan();

    m_workQueue->dispatchSync([] { });

    return 0;
}

OSStatus MockAudioSharedInternalUnit::uninitialize()
{
    ASSERT(!m_internalState->isProducingData());
    return 0;
}

void MockAudioSharedInternalUnit::delaySamples(Seconds delta)
{
    stop();
    m_timer.startOneShot(delta);
}

void MockAudioSharedInternalUnit::reconfigure()
{
    ASSERT(!isMainThread());

    auto rate = sampleRate();
    ASSERT(rate);

    m_maximiumFrameCount = WTF::roundUpToPowerOfTwo(renderInterval().seconds() * rate * 2);
    ASSERT(m_maximiumFrameCount);

    m_audioBufferList = makeUnique<WebAudioBufferList>(m_streamFormat, m_maximiumFrameCount);

    CMFormatDescriptionRef formatDescription;
    PAL::CMAudioFormatDescriptionCreate(NULL, &m_streamFormat, 0, NULL, 0, NULL, NULL, &formatDescription);
    m_formatDescription = adoptCF(formatDescription);

    size_t sampleCount = 2 * rate;
    m_bipBopBuffer.resize(sampleCount);
    m_bipBopBuffer.fill(0);

    size_t bipBopSampleCount = ceil(BipBopDuration * rate);
    size_t bipStart = 0;
    size_t bopStart = rate;

    addHum(BipBopVolume, BipFrequency, rate, 0, m_bipBopBuffer.data() + bipStart, bipBopSampleCount);
    addHum(BipBopVolume, BopFrequency, rate, 0, m_bipBopBuffer.data() + bopStart, bipBopSampleCount);
    if (!m_enableEchoCancellation)
        addHum(NoiseVolume, NoiseFrequency, rate, 0, m_bipBopBuffer.data(), sampleCount);
}

void MockAudioSharedInternalUnit::emitSampleBuffers(uint32_t frameCount)
{
    ASSERT(!isMainThread());
    ASSERT(m_formatDescription);

    CMTime startTime = PAL::CMTimeMake(m_samplesEmitted, sampleRate());
    auto sampleTime = PAL::CMTimeGetSeconds(startTime);
    m_samplesEmitted += frameCount;

    auto* bufferList = m_audioBufferList->list();
    AudioUnitRenderActionFlags ioActionFlags = 0;
    
    AudioTimeStamp timeStamp;
    memset(&timeStamp, 0, sizeof(AudioTimeStamp));
    timeStamp.mSampleTime = sampleTime;
    timeStamp.mHostTime = static_cast<UInt64>(sampleTime);

    auto exposedFrameCount = s_shouldIncreaseBufferSize ? 10 * frameCount : frameCount;
    if (m_microphoneCallback.inputProc)
        m_microphoneCallback.inputProc(m_microphoneCallback.inputProcRefCon, &ioActionFlags, &timeStamp, 1, exposedFrameCount, nullptr);

    ioActionFlags = 0;
    if (m_speakerCallback.inputProc)
        m_speakerCallback.inputProc(m_speakerCallback.inputProcRefCon, &ioActionFlags, &timeStamp, 1, frameCount, bufferList);
}

void MockAudioSharedInternalUnit::generateSampleBuffers(MonotonicTime renderTime)
{
    auto delta = renderInterval();
    auto currentTime = MonotonicTime::now();
    auto nextRenderTime = renderTime + delta;
    Seconds nextRenderDelay = nextRenderTime.secondsSinceEpoch() - currentTime.secondsSinceEpoch();
    if (nextRenderDelay.seconds() < 0) {
        nextRenderTime = currentTime;
        nextRenderDelay = 0_s;
    }

    m_workQueue->dispatchAfter(nextRenderDelay, [this, nextRenderTime, state = m_internalState] {
        if (state->isProducingData())
            generateSampleBuffers(nextRenderTime);
    });

    if (!m_audioBufferList || !m_bipBopBuffer.size())
        reconfigure();

    uint32_t totalFrameCount = alignTo16Bytes(delta.seconds() * sampleRate());
    uint32_t frameCount = std::min(totalFrameCount, static_cast<uint32_t>(AudioSession::sharedSession().bufferSize()));

    while (frameCount) {
        uint32_t bipBopStart = m_samplesRendered % m_bipBopBuffer.size();
        uint32_t bipBopRemain = m_bipBopBuffer.size() - bipBopStart;
        uint32_t bipBopCount = std::min(frameCount, bipBopRemain);
        for (auto& audioBuffer : m_audioBufferList->buffers()) {
            audioBuffer.mDataByteSize = frameCount * m_streamFormat.mBytesPerFrame;
            memcpy(audioBuffer.mData, &m_bipBopBuffer[bipBopStart], sizeof(Float32) * bipBopCount);
            addHum(HumVolume, HumFrequency, sampleRate(), m_samplesRendered, static_cast<float*>(audioBuffer.mData), bipBopCount);
        }
        emitSampleBuffers(bipBopCount);
        m_samplesRendered += bipBopCount;
        totalFrameCount -= bipBopCount;
        frameCount = std::min(totalFrameCount, static_cast<uint32_t>(AudioSession::sharedSession().bufferSize()));
    }
}

OSStatus MockAudioSharedInternalUnit::render(AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32 frameCount, AudioBufferList* buffer)
{
    if (s_shouldIncreaseBufferSize) {
        auto copySize = frameCount * m_streamFormat.mBytesPerPacket;
        if (buffer->mNumberBuffers && copySize <= buffer->mBuffers[0].mDataByteSize)
            s_shouldIncreaseBufferSize = false;
        // We still return an error in case s_shouldIncreaseBufferSize is false since we do not have enough data to write.
        return kAudio_ParamError;
    }

    auto* sourceBuffer = m_audioBufferList->list();
    if (buffer->mNumberBuffers > sourceBuffer->mNumberBuffers)
        return kAudio_ParamError;

    auto copySize = frameCount * m_streamFormat.mBytesPerPacket;
    for (uint32_t i = 0; i < buffer->mNumberBuffers; i++) {
        ASSERT(copySize <= sourceBuffer->mBuffers[i].mDataByteSize);
        if (copySize > buffer->mBuffers[i].mDataByteSize)
            return kAudio_ParamError;

        auto* source = static_cast<uint8_t*>(sourceBuffer->mBuffers[i].mData);
        auto* destination = static_cast<uint8_t*>(buffer->mBuffers[i].mData);
        memcpy(destination, source, copySize);
    }

    return 0;
}

OSStatus MockAudioSharedInternalUnit::set(AudioUnitPropertyID property, AudioUnitScope scope, AudioUnitElement, const void* value, UInt32)
{
    if (property == kAudioUnitProperty_StreamFormat) {
        auto& typedValue = *static_cast<const AudioStreamBasicDescription*>(value);
        if (scope == kAudioUnitScope_Input)
            m_streamFormat = typedValue;
        else
            m_outputStreamFormat = typedValue;
        return 0;
    }
    if (property == kAUVoiceIOProperty_VoiceProcessingEnableAGC) {
        m_enableEchoCancellation = !!*static_cast<const uint32_t*>(value);
        return 0;
    }
    if (property == kAudioOutputUnitProperty_SetInputCallback) {
        m_microphoneCallback = *static_cast<const AURenderCallbackStruct*>(value);
        return 0;
    }
    if (property == kAudioUnitProperty_SetRenderCallback) {
        m_speakerCallback = *static_cast<const AURenderCallbackStruct*>(value);
        return 0;
    }
    if (property == kAudioOutputUnitProperty_CurrentDevice) {
        ASSERT(!*static_cast<const uint32_t*>(value));
        if (auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(MockAudioSharedUnit::singleton().persistentIDForTesting()))
            m_streamFormat.mSampleRate = m_outputStreamFormat.mSampleRate = std::get<MockMicrophoneProperties>(device->properties).defaultSampleRate;
        return 0;
    }
    
    return 0;
}

OSStatus MockAudioSharedInternalUnit::get(AudioUnitPropertyID property, AudioUnitScope scope, AudioUnitElement, void* value, UInt32* valueSize)
{
    if (property == kAudioUnitProperty_StreamFormat) {
        auto& typedValue = *static_cast<AudioStreamBasicDescription*>(value);
        if (scope == kAudioUnitScope_Input)
            typedValue = m_streamFormat;
        else
            typedValue = m_outputStreamFormat;
        *valueSize = sizeof(AudioStreamBasicDescription);
        return 0;
    }

    return 0;
}

OSStatus MockAudioSharedInternalUnit::defaultInputDevice(uint32_t* device)
{
    *device = 0;
    return 0;
}

OSStatus MockAudioSharedInternalUnit::defaultOutputDevice(uint32_t* device)
{
    *device = 0;
    return 0;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
