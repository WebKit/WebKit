/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#import "WebAudioSourceProviderAVFObjC.h"
#import <AVFoundation/AVAudioBuffer.h>
#import <AudioToolbox/AudioConverter.h>
#import <CoreAudio/CoreAudioTypes.h>

#import <pal/cf/CoreMediaSoftLink.h>

SOFT_LINK_FRAMEWORK(AudioToolbox)

SOFT_LINK(AudioToolbox, AudioConverterNew, OSStatus, (const AudioStreamBasicDescription* inSourceFormat, const AudioStreamBasicDescription* inDestinationFormat, AudioConverterRef* outAudioConverter), (inSourceFormat, inDestinationFormat, outAudioConverter))

namespace WebCore {
using namespace PAL;

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

CaptureSourceOrError MockRealtimeAudioSource::create(String&& deviceID, String&& name, String&& hashSalt, const MediaConstraints* constraints)
{
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    if (!device)
        return { "No mock microphone device"_s };

    MockAudioSharedUnit::singleton().setDeviceID(deviceID);
    return CoreAudioCaptureSource::createForTesting(WTFMove(deviceID),  WTFMove(name), WTFMove(hashSalt), constraints, MockAudioSharedUnit::singleton());
}

MockAudioSharedUnit& MockAudioSharedUnit::singleton()
{
    static NeverDestroyed<MockAudioSharedUnit> singleton;
    return singleton;
}

MockAudioSharedUnit::MockAudioSharedUnit()
    : m_timer(RunLoop::current(), this, &MockAudioSharedUnit::tick)
    , m_workQueue(WorkQueue::create("MockAudioSharedUnit Capture Queue"))
{
}

void MockAudioSharedUnit::resetSampleRate()
{
    if (auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(m_deviceID))
        setSampleRate(WTF::get<MockMicrophoneProperties>(device->properties).defaultSampleRate);
}

bool MockAudioSharedUnit::hasAudioUnit() const
{
    return m_hasAudioUnit;
}

void MockAudioSharedUnit::setCaptureDevice(String&& deviceID, uint32_t)
{
    m_deviceID = WTFMove(deviceID);
    reconfigureAudioUnit();
}

OSStatus MockAudioSharedUnit::reconfigureAudioUnit()
{
    if (!hasAudioUnit())
        return 0;

    m_timer.stop();
    m_lastRenderTime = MonotonicTime::nan();
    m_workQueue->dispatch([this] {
        reconfigure();
        callOnMainThread([this] {
            m_lastRenderTime = MonotonicTime::now();
            m_timer.startRepeating(renderInterval());
        });
    });
    return 0;
}

void MockAudioSharedUnit::cleanupAudioUnit()
{
    m_hasAudioUnit = false;
    m_timer.stop();
    m_lastRenderTime = MonotonicTime::nan();
}

OSStatus MockAudioSharedUnit::startInternal()
{
    if (!m_hasAudioUnit)
        m_hasAudioUnit = true;

    m_lastRenderTime = MonotonicTime::now();
    m_timer.startRepeating(renderInterval());
    return 0;
}

void MockAudioSharedUnit::stopInternal()
{
    if (!m_hasAudioUnit)
        return;
    m_timer.stop();
    m_lastRenderTime = MonotonicTime::nan();
}

bool MockAudioSharedUnit::isProducingData() const
{
    return m_timer.isActive();
}

void MockAudioSharedUnit::tick()
{
    if (std::isnan(m_lastRenderTime))
        m_lastRenderTime = MonotonicTime::now();

    MonotonicTime now = MonotonicTime::now();

    if (m_delayUntil) {
        if (m_delayUntil < now)
            return;
        m_delayUntil = MonotonicTime();
    }

    Seconds delta = now - m_lastRenderTime;
    m_lastRenderTime = now;

    m_workQueue->dispatch([this, delta] {
        render(delta);
    });
}

void MockAudioSharedUnit::delaySamples(Seconds delta)
{
    m_delayUntil = MonotonicTime::now() + delta;
}

void MockAudioSharedUnit::reconfigure()
{
    ASSERT(!isMainThread());

    auto rate = sampleRate();
    ASSERT(rate);

    m_maximiumFrameCount = WTF::roundUpToPowerOfTwo(renderInterval().seconds() * rate * 2);
    ASSERT(m_maximiumFrameCount);

    const int bytesPerFloat = sizeof(Float32);
    const int bitsPerByte = 8;
    const int channelCount = m_channelCount;
    const bool isFloat = true;
    const bool isBigEndian = false;
    const bool isNonInterleaved = true;
    FillOutASBDForLPCM(m_streamFormat, rate, channelCount, bitsPerByte * bytesPerFloat, bitsPerByte * bytesPerFloat, isFloat, isBigEndian, isNonInterleaved);

    m_audioBufferList = makeUnique<WebAudioBufferList>(m_streamFormat, m_maximiumFrameCount);

    CMFormatDescriptionRef formatDescription;
    CMAudioFormatDescriptionCreate(NULL, &m_streamFormat, 0, NULL, 0, NULL, NULL, &formatDescription);
    m_formatDescription = adoptCF(formatDescription);

    size_t sampleCount = 2 * rate;
    m_bipBopBuffer.resize(sampleCount);
    m_bipBopBuffer.fill(0);

    size_t bipBopSampleCount = ceil(BipBopDuration * rate);
    size_t bipStart = 0;
    size_t bopStart = rate;

    addHum(BipBopVolume, BipFrequency, rate, 0, m_bipBopBuffer.data() + bipStart, bipBopSampleCount);
    addHum(BipBopVolume, BopFrequency, rate, 0, m_bipBopBuffer.data() + bopStart, bipBopSampleCount);
    if (!enableEchoCancellation())
        addHum(NoiseVolume, NoiseFrequency, rate, 0, m_bipBopBuffer.data(), sampleCount);
}

void MockAudioSharedUnit::emitSampleBuffers(uint32_t frameCount)
{
    ASSERT(!isMainThread());
    ASSERT(m_formatDescription);

    CMTime startTime = CMTimeMake(m_samplesEmitted, sampleRate());
    m_samplesEmitted += frameCount;

    audioSamplesAvailable(PAL::toMediaTime(startTime), *m_audioBufferList, CAAudioStreamDescription(m_streamFormat), frameCount);
}

void MockAudioSharedUnit::render(Seconds delta)
{
    ASSERT(!isMainThread());
    if (!m_audioBufferList || !m_bipBopBuffer.size())
        reconfigure();

    uint32_t totalFrameCount = alignTo16Bytes(delta.seconds() * sampleRate());
    uint32_t frameCount = std::min(totalFrameCount, m_maximiumFrameCount);

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
        frameCount = std::min(totalFrameCount, m_maximiumFrameCount);
    }
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
