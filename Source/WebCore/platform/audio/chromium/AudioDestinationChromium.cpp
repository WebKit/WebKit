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

#include "AudioFIFO.h"
#include "AudioPullFIFO.h"
#include <public/Platform.h>

namespace WebCore {

// Buffer size at which the web audio engine will render.
const unsigned renderBufferSize = 128;

// Size of the FIFO
const size_t fifoSize = 8192;

// Factory method: Chromium-implementation
PassOwnPtr<AudioDestination> AudioDestination::create(AudioIOCallback& callback, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    return adoptPtr(new AudioDestinationChromium(callback, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate));
}

AudioDestinationChromium::AudioDestinationChromium(AudioIOCallback& callback, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
    : m_callback(callback)
    , m_numberOfOutputChannels(numberOfOutputChannels)
    , m_inputBus(numberOfInputChannels, renderBufferSize)
    , m_renderBus(numberOfOutputChannels, renderBufferSize, false)
    , m_sampleRate(sampleRate)
    , m_isPlaying(false)
{
    // Use the optimal buffer size recommended by the audio backend.
    m_callbackBufferSize = WebKit::Platform::current()->audioHardwareBufferSize();

    // Quick exit if the requested size is too large.
    ASSERT(m_callbackBufferSize + renderBufferSize <= fifoSize);
    if (m_callbackBufferSize + renderBufferSize > fifoSize)
        return;

    // FIXME: switch over to new method using inputDeviceId, once chromium supports it.
    m_audioDevice = adoptPtr(WebKit::Platform::current()->createAudioDevice(m_callbackBufferSize, numberOfInputChannels, numberOfOutputChannels, sampleRate, this));
    ASSERT(m_audioDevice);

    // Create a FIFO to handle the possibility of the callback size
    // not being a multiple of the render size. If the FIFO already
    // contains enough data, the data will be provided directly.
    // Otherwise, the FIFO will call the provider enough times to
    // satisfy the request for data.
    m_fifo = adoptPtr(new AudioPullFIFO(*this, numberOfOutputChannels, fifoSize, renderBufferSize));

    // Input buffering.
    m_inputFifo = adoptPtr(new AudioFIFO(numberOfInputChannels, fifoSize));

    // If the callback size does not match the render size, then we need to buffer some
    // extra silence for the input. Otherwise, we can over-consume the input FIFO.
    if (m_callbackBufferSize != renderBufferSize) {
        // FIXME: handle multi-channel input and don't hard-code to stereo.
        AudioBus silence(2, renderBufferSize);
        m_inputFifo->push(&silence);
    }
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
    return static_cast<float>(WebKit::Platform::current()->audioHardwareSampleRate());
}

void AudioDestinationChromium::render(const WebKit::WebVector<float*>& sourceData, const WebKit::WebVector<float*>& audioData, size_t numberOfFrames)
{
    bool isNumberOfChannelsGood = audioData.size() == m_numberOfOutputChannels;
    if (!isNumberOfChannelsGood) {
        ASSERT_NOT_REACHED();
        return;
    }

    bool isBufferSizeGood = numberOfFrames == m_callbackBufferSize;
    if (!isBufferSizeGood) {
        ASSERT_NOT_REACHED();
        return;
    }

    // Buffer optional live input.
    if (sourceData.size() >= 2) {
        // FIXME: handle multi-channel input and don't hard-code to stereo.
        AudioBus wrapperBus(2, numberOfFrames, false);
        wrapperBus.setChannelMemory(0, sourceData[0], numberOfFrames);
        wrapperBus.setChannelMemory(1, sourceData[1], numberOfFrames);
        m_inputFifo->push(&wrapperBus);
    }

    m_renderBus.setChannelMemory(0, audioData[0], numberOfFrames);
    m_renderBus.setChannelMemory(1, audioData[1], numberOfFrames);

    m_fifo->consume(&m_renderBus, numberOfFrames);
}

void AudioDestinationChromium::provideInput(AudioBus* bus, size_t framesToProcess)
{
    AudioBus* sourceBus = 0;
    if (m_inputFifo->framesInFifo() >= framesToProcess) {
        m_inputFifo->consume(&m_inputBus, framesToProcess);
        sourceBus = &m_inputBus;
    }

    m_callback.render(sourceBus, bus, framesToProcess);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
