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
#import "MediaConstraints.h"
#import "MediaSampleAVFObjC.h"
#import "NotImplemented.h"
#import "RealtimeMediaSourceSettings.h"
#import "WebAudioSourceProviderAVFObjC.h"
#import <AVFoundation/AVAudioBuffer.h>
#import <AudioToolbox/AudioConverter.h>
#import <CoreAudio/CoreAudioTypes.h>

#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK(AudioToolbox)

SOFT_LINK(AudioToolbox, AudioConverterNew, OSStatus, (const AudioStreamBasicDescription* inSourceFormat, const AudioStreamBasicDescription* inDestinationFormat, AudioConverterRef* outAudioConverter), (inSourceFormat, inDestinationFormat, outAudioConverter))

namespace WebCore {

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

    CMTime startTime = CMTimeMake(elapsedTime() * m_sampleRate, m_sampleRate);
    CMSampleBufferRef sampleBuffer;
    OSStatus result = CMAudioSampleBufferCreateWithPacketDescriptions(nullptr, nullptr, true, nullptr, nullptr, m_formatDescription.get(), frameCount, startTime, nullptr, &sampleBuffer);
    ASSERT(sampleBuffer);
    ASSERT(!result);

    if (!sampleBuffer)
        return;

    auto buffer = adoptCF(sampleBuffer);
    result = CMSampleBufferSetDataBufferFromAudioBufferList(sampleBuffer, kCFAllocatorDefault, kCFAllocatorDefault, 0, m_audioBufferList.get());
    ASSERT(!result);

    result = CMSampleBufferSetDataReady(sampleBuffer);
    ASSERT(!result);

    mediaDataUpdated(MediaSampleAVFObjC::create(sampleBuffer));

    for (auto& observer : m_observers)
        observer->process(m_formatDescription.get(), sampleBuffer);
}

void MockRealtimeAudioSourceMac::reconfigure()
{
    m_maximiumFrameCount = WTF::roundUpToPowerOfTwo(renderInterval() / 1000. * m_sampleRate * 2);
    ASSERT(m_maximiumFrameCount);

    // AudioBufferList is a variable-length struct, so create on the heap with a generic new() operator
    // with a custom size, and initialize the struct manually.
    uint32_t bufferDataSize = m_bytesPerFrame * m_maximiumFrameCount;
    uint32_t baseSize = offsetof(AudioBufferList, mBuffers) + sizeof(AudioBuffer);
    uint64_t bufferListSize = baseSize + bufferDataSize;
    ASSERT(bufferListSize <= SIZE_MAX);
    if (bufferListSize > SIZE_MAX)
        return;

    m_audioBufferListBufferSize = static_cast<size_t>(bufferListSize);
    m_audioBufferList = std::unique_ptr<AudioBufferList>(static_cast<AudioBufferList*>(::operator new (m_audioBufferListBufferSize)));
    memset(m_audioBufferList.get(), 0, m_audioBufferListBufferSize);

    m_audioBufferList->mNumberBuffers = 1;
    auto& buffer = m_audioBufferList->mBuffers[0];
    buffer.mNumberChannels = 1;
    buffer.mDataByteSize = bufferDataSize;
    buffer.mData = reinterpret_cast<uint8_t*>(m_audioBufferList.get()) + baseSize;

    const int bytesPerFloat = sizeof(Float32);
    const int bitsPerByte = 8;
    m_streamFormat = { };
    m_streamFormat.mSampleRate = m_sampleRate;
    m_streamFormat.mFormatID = kAudioFormatLinearPCM;
    m_streamFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    m_streamFormat.mBytesPerPacket = bytesPerFloat;
    m_streamFormat.mFramesPerPacket = 1;
    m_streamFormat.mBytesPerFrame = bytesPerFloat;
    m_streamFormat.mChannelsPerFrame = 1;
    m_streamFormat.mBitsPerChannel = bitsPerByte * bytesPerFloat;

    CMFormatDescriptionRef formatDescription;
    CMAudioFormatDescriptionCreate(NULL, &m_streamFormat, 0, NULL, 0, NULL, NULL, &formatDescription);
    m_formatDescription = adoptCF(formatDescription);

    for (auto& observer : m_observers)
        observer->prepare(&m_streamFormat);
}

void MockRealtimeAudioSourceMac::render(double delta)
{
    static double theta;
    static const double frequencies[] = { 1500., 500. };

    if (!m_audioBufferList)
        reconfigure();

    uint32_t totalFrameCount = delta * m_sampleRate;
    uint32_t frameCount = std::min(totalFrameCount, m_maximiumFrameCount);
    double elapsed = elapsedTime();
    while (frameCount) {
        float *buffer = static_cast<float *>(m_audioBufferList->mBuffers[0].mData);
        for (uint32_t frame = 0; frame < frameCount; ++frame) {
            int phase = fmod(elapsed, 2) * 15;
            double increment = 0;
            bool silent = true;

            switch (phase) {
            case 0:
            case 14: {
                int index = fmod(elapsed, 1) * 2;
                increment = 2.0 * M_PI * frequencies[index] / m_sampleRate;
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

            buffer[frame] = sin(theta) * 0.25;
            theta += increment;
            if (theta > 2.0 * M_PI)
                theta -= 2.0 * M_PI;
            elapsed += 1 / m_sampleRate;
        }

        m_audioBufferList->mBuffers[0].mDataByteSize = frameCount * sizeof(float);
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
