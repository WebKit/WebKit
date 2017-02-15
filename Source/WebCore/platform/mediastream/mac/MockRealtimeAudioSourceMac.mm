/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "NotImplemented.h"
#import "RealtimeMediaSourceSettings.h"
#import "WebAudioBufferList.h"
#import "WebAudioSourceProviderAVFObjC.h"
#import <AVFoundation/AVAudioBuffer.h>
#import <AudioToolbox/AudioConverter.h>
#import <CoreAudio/CoreAudioTypes.h>

#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK(AudioToolbox)

SOFT_LINK(AudioToolbox, AudioConverterNew, OSStatus, (const AudioStreamBasicDescription* inSourceFormat, const AudioStreamBasicDescription* inDestinationFormat, AudioConverterRef* outAudioConverter), (inSourceFormat, inDestinationFormat, outAudioConverter))

namespace WebCore {

static inline size_t alignTo16Bytes(size_t size)
{
    return (size + 15) & ~15;
}

RefPtr<MockRealtimeAudioSource> MockRealtimeAudioSource::create(const String& name, const MediaConstraints* constraints)
{
    auto source = adoptRef(new MockRealtimeAudioSourceMac(name));
    if (constraints && source->applyConstraints(*constraints))
        source = nullptr;

    return source;
}

MockRealtimeAudioSourceMac::MockRealtimeAudioSourceMac(const String& name)
    : MockRealtimeAudioSource(name)
{
}

RefPtr<MockRealtimeAudioSource> MockRealtimeAudioSource::createMuted(const String& name)
{
    auto source = adoptRef(new MockRealtimeAudioSource(name));
    source->m_muted = true;
    return source;
}

void MockRealtimeAudioSourceMac::addObserver(AudioSourceObserverObjC& observer)
{
    m_observers.append(&observer);
    if (m_streamFormat.mSampleRate)
        observer.prepare(&m_streamFormat);
}

void MockRealtimeAudioSourceMac::removeObserver(AudioSourceObserverObjC& observer)
{
    m_observers.removeFirst(&observer);
}

void MockRealtimeAudioSourceMac::start()
{
    startProducingData();
}


void MockRealtimeAudioSourceMac::emitSampleBuffers(uint32_t frameCount)
{
    ASSERT(m_formatDescription);

    CMTime startTime = CMTimeMake(m_bytesEmitted, m_sampleRate);
    m_bytesEmitted += frameCount;

    audioSamplesAvailable(toMediaTime(startTime), *m_audioBufferList, CAAudioStreamDescription(m_streamFormat), frameCount);

    CMSampleBufferRef sampleBuffer;
    OSStatus result = CMAudioSampleBufferCreateWithPacketDescriptions(nullptr, nullptr, true, nullptr, nullptr, m_formatDescription.get(), frameCount, startTime, nullptr, &sampleBuffer);
    ASSERT(sampleBuffer);
    ASSERT(!result);

    if (!sampleBuffer)
        return;

    auto buffer = adoptCF(sampleBuffer);
    result = CMSampleBufferSetDataBufferFromAudioBufferList(sampleBuffer, kCFAllocatorDefault, kCFAllocatorDefault, 0, m_audioBufferList->list());
    ASSERT(!result);

    result = CMSampleBufferSetDataReady(sampleBuffer);
    ASSERT(!result);

    for (const auto& observer : m_observers)
        observer->process(m_formatDescription.get(), sampleBuffer);
}

void MockRealtimeAudioSourceMac::reconfigure()
{
    m_maximiumFrameCount = WTF::roundUpToPowerOfTwo(renderInterval() / 1000. * m_sampleRate * 2);
    ASSERT(m_maximiumFrameCount);

    const int bytesPerFloat = sizeof(Float32);
    const int bitsPerByte = 8;
    const int channelCount = 2;
    const bool isFloat = true;
    const bool isBigEndian = false;
    const bool isNonInterleaved = true;
    FillOutASBDForLPCM(m_streamFormat, m_sampleRate, channelCount, bitsPerByte * bytesPerFloat, bitsPerByte * bytesPerFloat, isFloat, isBigEndian, isNonInterleaved);

    m_audioBufferList = std::make_unique<WebAudioBufferList>(m_streamFormat, m_streamFormat.mBytesPerFrame * m_maximiumFrameCount);

    CMFormatDescriptionRef formatDescription;
    CMAudioFormatDescriptionCreate(NULL, &m_streamFormat, 0, NULL, 0, NULL, NULL, &formatDescription);
    m_formatDescription = adoptCF(formatDescription);

    for (auto& observer : m_observers)
        observer->prepare(&m_streamFormat);
}

void MockRealtimeAudioSourceMac::render(double delta)
{
    if (m_muted || !m_enabled)
        return;

    static double theta;
    static const double frequencies[] = { 1500., 500. };
    static const double tau = 2 * M_PI;

    if (!m_audioBufferList)
        reconfigure();

    uint32_t totalFrameCount = alignTo16Bytes(delta * m_sampleRate);
    uint32_t frameCount = std::min(totalFrameCount, m_maximiumFrameCount);
    double elapsed = elapsedTime();

    while (frameCount) {
        for (auto& audioBuffer : m_audioBufferList->buffers()) {
            audioBuffer.mDataByteSize = frameCount * m_streamFormat.mBytesPerFrame;
            float *buffer = static_cast<float *>(audioBuffer.mData);
            for (uint32_t frame = 0; frame < frameCount; ++frame) {
                int phase = fmod(elapsed, 2) * 15;
                double increment = 0;
                bool silent = true;

                switch (phase) {
                case 0:
                case 14: {
                    int index = fmod(elapsed, 1) * 2;
                    increment = tau * frequencies[index] / m_sampleRate;
                    silent = false;
                    break;
                }
                default:
                    break;
                }

                if (silent) {
                    buffer[frame] = 0;
                    continue;
                }

                float tone = sin(theta) * 0.25;
                buffer[frame] = tone;

                theta += increment;
                if (theta > tau)
                    theta -= tau;
                elapsed += 1 / m_sampleRate;
            }
        }
        emitSampleBuffers(frameCount);
        totalFrameCount -= frameCount;
        frameCount = std::min(totalFrameCount, m_maximiumFrameCount);
    }
}

bool MockRealtimeAudioSourceMac::applySampleRate(int sampleRate)
{
    if (sampleRate < 44100 || sampleRate > 48000)
        return false;

    if (static_cast<uint32_t>(sampleRate) == m_sampleRate)
        return true;

    m_sampleRate = sampleRate;
    m_formatDescription = nullptr;
    m_audioBufferList = nullptr;
    m_audioBufferListBufferSize = 0;

    return true;
}

AudioSourceProvider* MockRealtimeAudioSourceMac::audioSourceProvider()
{
    if (!m_audioSourceProvider)
        m_audioSourceProvider = WebAudioSourceProviderAVFObjC::create(*this);

    return m_audioSourceProvider.get();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
