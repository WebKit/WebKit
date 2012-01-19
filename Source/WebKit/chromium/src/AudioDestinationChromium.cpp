/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include "AudioDestinationChromium.h"

#include "WebKit.h"
#include "platform/WebKitPlatformSupport.h"

using namespace WebKit;

namespace WebCore {

// Buffer size at which the web audio engine will render.
const unsigned renderBufferSize = 128;

// Size of the FIFO
const size_t fifoSize = 8192;

// FIXME: add support for multi-channel.
const unsigned numberOfChannels = 2;

// Factory method: Chromium-implementation
PassOwnPtr<AudioDestination> AudioDestination::create(AudioSourceProvider& provider, float sampleRate)
{
    return adoptPtr(new AudioDestinationChromium(provider, sampleRate));
}

AudioDestinationChromium::AudioDestinationChromium(AudioSourceProvider& provider, float sampleRate)
    : m_provider(provider)
    , m_renderBus(numberOfChannels, renderBufferSize, false)
    , m_sampleRate(sampleRate)
    , m_isPlaying(false)
{
    // Use the optimal buffer size recommended by the audio backend.
    m_callbackBufferSize = webKitPlatformSupport()->audioHardwareBufferSize();

    // Quick exit if the requested size is too large.
    ASSERT(m_callbackBufferSize + renderBufferSize <= fifoSize);
    if (m_callbackBufferSize + renderBufferSize > fifoSize)
        return;
    
    m_audioDevice = adoptPtr(webKitPlatformSupport()->createAudioDevice(m_callbackBufferSize, numberOfChannels, sampleRate, this));
    ASSERT(m_audioDevice);

    // Create a FIFO to handle the possibility of the callback size
    // not being a multiple of the render size. If the FIFO already
    // contains enough data, the data will be provided directly.
    // Otherwise, the FIFO will call the provider enough times to
    // satisfy the request for data.
    m_fifo = adoptPtr(new FIFO(provider, numberOfChannels, fifoSize, renderBufferSize));
}

AudioDestinationChromium::~AudioDestinationChromium()
{
    stop();
}

void AudioDestinationChromium::start()
{
    if (!m_isPlaying && m_audioDevice) {
        m_audioDevice->start();
        m_isPlaying = true;
    }
}

void AudioDestinationChromium::stop()
{
    if (m_isPlaying && m_audioDevice) {
        m_audioDevice->stop();
        m_isPlaying = false;
    }
}

float AudioDestination::hardwareSampleRate()
{
    return static_cast<float>(webKitPlatformSupport()->audioHardwareSampleRate());
}

// Pulls on our provider to get the rendered audio stream.
void AudioDestinationChromium::render(const WebVector<float*>& audioData, size_t numberOfFrames)
{
    bool isNumberOfChannelsGood = audioData.size() == numberOfChannels;
    if (!isNumberOfChannelsGood) {
        ASSERT_NOT_REACHED();
        return;
    }
        
    bool isBufferSizeGood = numberOfFrames == m_callbackBufferSize;
    if (!isBufferSizeGood) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_renderBus.setChannelMemory(0, audioData[0], numberOfFrames);
    m_renderBus.setChannelMemory(1, audioData[1], numberOfFrames);
    m_fifo->consume(&m_renderBus, numberOfFrames);
}

AudioDestinationChromium::FIFO::FIFO(AudioSourceProvider& provider, unsigned numberOfChannels, size_t fifoLength, size_t providerSize)
    : m_provider(provider)
    , m_fifoAudioBus(numberOfChannels, fifoLength)
    , m_fifoLength(fifoLength)
    , m_framesInFifo(0)
    , m_readIndex(0)
    , m_writeIndex(0)
    , m_providerSize(providerSize)
    , m_tempBus(numberOfChannels, providerSize)
{
}

void AudioDestinationChromium::FIFO::consume(AudioBus* destination, size_t framesToConsume)
{
    bool isGood = destination && (framesToConsume <= m_fifoLength);
    ASSERT(isGood);
    if (!isGood)
        return;

    if (framesToConsume > m_framesInFifo) {
        // We don't have enough data in the FIFO to fulfill the
        // request. Ask for more data.
        fillBuffer(framesToConsume - m_framesInFifo);
    }

    // We have enough data now. Copy the requested number of samples
    // to the destination.

    size_t part1Length;
    size_t part2Length;
    findWrapLengths(m_readIndex, framesToConsume, part1Length, part2Length);

    size_t numberOfChannels = m_fifoAudioBus.numberOfChannels();

    for (size_t channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex) {
        float* destinationData = destination->channel(channelIndex)->mutableData();
        const float* sourceData = m_fifoAudioBus.channel(channelIndex)->data();

        bool isCopyGood = ((m_readIndex < m_fifoLength)
                           && (m_readIndex + part1Length) <= m_fifoLength
                           && (part1Length <= destination->length())
                           && (part1Length + part2Length) <= destination->length());
        ASSERT(isCopyGood);
        if (!isCopyGood)
            return;

        memcpy(destinationData, sourceData + m_readIndex, part1Length * sizeof(*sourceData));
        // Handle wrap around of the FIFO, if needed.
        if (part2Length > 0)
            memcpy(destinationData + part1Length, sourceData, part2Length * sizeof(*sourceData));
    }
    m_readIndex = updateIndex(m_readIndex, framesToConsume);
    m_framesInFifo -= framesToConsume;
    ASSERT(m_framesInFifo >= 0);
}

void AudioDestinationChromium::FIFO::findWrapLengths(size_t index, size_t size, size_t& part1Length, size_t& part2Length)
{
    ASSERT(index < m_fifoLength && size <= m_fifoLength);
    if (index < m_fifoLength && size <= m_fifoLength) {
        if (index + size > m_fifoLength) {
            // Need to wrap. Figure out the length of each piece.
            part1Length = m_fifoLength - index;
            part2Length = size - part1Length;
        } else {
            // No wrap needed.
            part1Length = size;
            part2Length = 0;
        }
    } else {
        // Invalid values for index or size. Set the part lengths to
        // zero so nothing is copied.
        part1Length = 0;
        part2Length = 0;
    }
}

void AudioDestinationChromium::FIFO::fillBuffer(size_t numberOfFrames)
{
    // Keep asking the provider to give us data until we have received
    // at least |numberOfFrames| of data. Stuff the data into the
    // FIFO.
    size_t framesProvided = 0;

    while (framesProvided < numberOfFrames) {
        m_provider.provideInput(&m_tempBus, m_providerSize);

        size_t part1Length;
        size_t part2Length;
        findWrapLengths(m_writeIndex, m_providerSize, part1Length, part2Length);

        size_t numberOfChannels = m_fifoAudioBus.numberOfChannels();
        
        for (size_t channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex) {
            float* destination = m_fifoAudioBus.channel(channelIndex)->mutableData();
            const float* source = m_tempBus.channel(channelIndex)->data();

            bool isCopyGood = (part1Length <= m_providerSize
                               && (part1Length + part2Length) <= m_providerSize
                               && (m_writeIndex < m_fifoLength)
                               && (m_writeIndex + part1Length) <= m_fifoLength
                               && part2Length < m_fifoLength);
            ASSERT(isCopyGood);
            if (!isCopyGood)
                return;

            memcpy(destination + m_writeIndex, source, part1Length * sizeof(*destination));
            // Handle wrap around of the FIFO, if needed.
            if (part2Length > 0)
                memcpy(destination, source + part1Length, part2Length * sizeof(*destination));
        }

        m_framesInFifo += m_providerSize;
        ASSERT(m_framesInFifo <= m_fifoLength);
        m_writeIndex = updateIndex(m_writeIndex, m_providerSize);
        framesProvided += m_providerSize;
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
