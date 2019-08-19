/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "MockRealtimeAudioSourceMac.h"

#if ENABLE(MEDIA_STREAM)
#import "AudioSampleBufferList.h"
#import "CAAudioStreamDescription.h"
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
#ifndef NDEBUG
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    if (!device)
        return { };
#endif

    auto source = adoptRef(*new MockRealtimeAudioSourceMac(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)));
    // FIXME: We should report error messages
    if (constraints && source->applyConstraints(*constraints))
        return { };

    return CaptureSourceOrError(WTFMove(source));
}

MockRealtimeAudioSourceMac::MockRealtimeAudioSourceMac(String&& deviceID, String&& name, String&& hashSalt)
    : MockRealtimeAudioSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt))
{
    ASSERT(isMainThread());
}

void MockRealtimeAudioSourceMac::emitSampleBuffers(uint32_t frameCount)
{
    ASSERT(!isMainThread());
    ASSERT(m_formatDescription);

    CMTime startTime = CMTimeMake(m_samplesEmitted, sampleRate());
    m_samplesEmitted += frameCount;

    audioSamplesAvailable(PAL::toMediaTime(startTime), *m_audioBufferList, CAAudioStreamDescription(m_streamFormat), frameCount);
}

void MockRealtimeAudioSourceMac::reconfigure()
{
    ASSERT(!isMainThread());
    m_maximiumFrameCount = WTF::roundUpToPowerOfTwo(renderInterval().seconds() * sampleRate() * 2);
    ASSERT(m_maximiumFrameCount);

    const int bytesPerFloat = sizeof(Float32);
    const int bitsPerByte = 8;
    const int channelCount = m_channelCount;
    const bool isFloat = true;
    const bool isBigEndian = false;
    const bool isNonInterleaved = true;
    FillOutASBDForLPCM(m_streamFormat, sampleRate(), channelCount, bitsPerByte * bytesPerFloat, bitsPerByte * bytesPerFloat, isFloat, isBigEndian, isNonInterleaved);

    m_audioBufferList = makeUnique<WebAudioBufferList>(m_streamFormat, m_streamFormat.mBytesPerFrame * m_maximiumFrameCount);

    CMFormatDescriptionRef formatDescription;
    CMAudioFormatDescriptionCreate(NULL, &m_streamFormat, 0, NULL, 0, NULL, NULL, &formatDescription);
    m_formatDescription = adoptCF(formatDescription);
}

void MockRealtimeAudioSourceMac::render(Seconds delta)
{
    ASSERT(!isMainThread());
    if (!m_audioBufferList)
        reconfigure();

    uint32_t totalFrameCount = alignTo16Bytes(delta.seconds() * sampleRate());
    uint32_t frameCount = std::min(totalFrameCount, m_maximiumFrameCount);

    while (frameCount) {
        uint32_t bipBopStart = m_samplesRendered % m_bipBopBuffer.size();
        uint32_t bipBopRemain = m_bipBopBuffer.size() - bipBopStart;
        uint32_t bipBopCount = std::min(frameCount, bipBopRemain);
        for (auto& audioBuffer : m_audioBufferList->buffers()) {
            audioBuffer.mDataByteSize = frameCount * m_streamFormat.mBytesPerFrame;
            if (!muted()) {
                memcpy(audioBuffer.mData, &m_bipBopBuffer[bipBopStart], sizeof(Float32) * bipBopCount);
                addHum(HumVolume, HumFrequency, sampleRate(), m_samplesRendered, static_cast<float*>(audioBuffer.mData), bipBopCount);
            } else
                memset(audioBuffer.mData, 0, sizeof(Float32) * bipBopCount);
        }
        emitSampleBuffers(bipBopCount);
        m_samplesRendered += bipBopCount;
        totalFrameCount -= bipBopCount;
        frameCount = std::min(totalFrameCount, m_maximiumFrameCount);
    }
}

void MockRealtimeAudioSourceMac::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    if (settings.contains(RealtimeMediaSourceSettings::Flag::SampleRate)) {
        m_workQueue->dispatch([this, protectedThis = makeRef(*this)] {
            m_formatDescription = nullptr;
            m_audioBufferList = nullptr;

            auto rate = sampleRate();
            size_t sampleCount = 2 * rate;

            m_bipBopBuffer.grow(sampleCount);
            m_bipBopBuffer.fill(0);

            size_t bipBopSampleCount = ceil(BipBopDuration * rate);
            size_t bipStart = 0;
            size_t bopStart = rate;

            addHum(BipBopVolume, BipFrequency, rate, 0, m_bipBopBuffer.data() + bipStart, bipBopSampleCount);
            addHum(BipBopVolume, BopFrequency, rate, 0, m_bipBopBuffer.data() + bopStart, bipBopSampleCount);
        });
    }

    MockRealtimeAudioSource::settingsDidChange(settings);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
