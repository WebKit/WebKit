/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioBuffer.h"

#include "AudioContext.h"
#include "AudioFileReader.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>

namespace WebCore {

RefPtr<AudioBuffer> AudioBuffer::create(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate)
{
    if (sampleRate < 22050 || sampleRate > 96000 || numberOfChannels > AudioContext::maxNumberOfChannels() || !numberOfFrames)
        return nullptr;

    auto buffer = adoptRef(*new AudioBuffer(numberOfChannels, numberOfFrames, sampleRate));
    if (!buffer->m_length)
        return nullptr;

    return WTFMove(buffer);
}

RefPtr<AudioBuffer> AudioBuffer::createFromAudioFileData(const void* data, size_t dataSize, bool mixToMono, float sampleRate)
{
    RefPtr<AudioBus> bus = createBusFromInMemoryAudioFile(data, dataSize, mixToMono, sampleRate);
    if (!bus)
        return nullptr;
    return adoptRef(*new AudioBuffer(*bus));
}

AudioBuffer::AudioBuffer(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate)
    : m_sampleRate(sampleRate)
    , m_length(numberOfFrames)
{
    m_channels.reserveCapacity(numberOfChannels);

    for (unsigned i = 0; i < numberOfChannels; ++i) {
        auto channelDataArray = Float32Array::tryCreate(m_length);
        if (!channelDataArray) {
            invalidate();
            break;
        }

        channelDataArray->setNeuterable(false);
        m_channels.append(WTFMove(channelDataArray));
    }
}

AudioBuffer::AudioBuffer(AudioBus& bus)
    : m_sampleRate(bus.sampleRate())
    , m_length(bus.length())
{
    // Copy audio data from the bus to the Float32Arrays we manage.
    unsigned numberOfChannels = bus.numberOfChannels();
    m_channels.reserveCapacity(numberOfChannels);
    for (unsigned i = 0; i < numberOfChannels; ++i) {
        auto channelDataArray = Float32Array::tryCreate(m_length);
        if (!channelDataArray) {
            invalidate();
            break;
        }

        channelDataArray->setNeuterable(false);
        channelDataArray->setRange(bus.channel(i)->data(), m_length, 0);
        m_channels.append(WTFMove(channelDataArray));
    }
}

void AudioBuffer::invalidate()
{
    releaseMemory();
    m_length = 0;
}

void AudioBuffer::releaseMemory()
{
    auto locker = holdLock(m_channelsLock);
    m_channels.clear();
}

ExceptionOr<Ref<Float32Array>> AudioBuffer::getChannelData(unsigned channelIndex)
{
    if (channelIndex >= m_channels.size())
        return Exception { SyntaxError };
    auto& channelData = *m_channels[channelIndex];
    return Float32Array::create(channelData.unsharedBuffer(), channelData.byteOffset(), channelData.length());
}

Float32Array* AudioBuffer::channelData(unsigned channelIndex)
{
    if (channelIndex >= m_channels.size())
        return nullptr;
    return m_channels[channelIndex].get();
}

void AudioBuffer::zero()
{
    for (auto& channel : m_channels)
        channel->zeroRange(0, length());
}

size_t AudioBuffer::memoryCost() const
{
    // memoryCost() may be invoked concurrently from a GC thread, and we need to be careful
    // about what data we access here and how. We need to hold a lock to prevent m_channels
    // from being changed while we iterate it, but calling channel->byteLength() is safe
    // because it doesn't involve chasing any pointers that can be nullified while the
    // AudioBuffer is alive.
    auto locker = holdLock(m_channelsLock);
    size_t cost = 0;
    for (auto& channel : m_channels)
        cost += channel->byteLength();
    return cost;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
