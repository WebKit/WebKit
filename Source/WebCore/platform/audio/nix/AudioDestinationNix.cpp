/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

#include "AudioDestinationNix.h"

#include "AudioFIFO.h"
#include "AudioPullFIFO.h"
#include <public/Platform.h>
#include <wtf/text/CString.h>

namespace WebCore {

// Buffer size at which the web audio engine will render.
const unsigned renderBufferSize = 128;

// Size of the FIFO
const size_t fifoSize = 8192;

PassOwnPtr<AudioDestination> AudioDestination::create(AudioIOCallback& callback, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    return adoptPtr(new AudioDestinationNix(callback, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate));
}

AudioDestinationNix::AudioDestinationNix(AudioIOCallback& callback, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
    : m_callback(callback)
    , m_numberOfOutputChannels(numberOfOutputChannels)
    , m_inputBus(AudioBus::create(numberOfInputChannels, renderBufferSize))
    , m_renderBus(AudioBus::create(numberOfOutputChannels, renderBufferSize, false))
    , m_sampleRate(sampleRate)
    , m_isPlaying(false)
    , m_inputDeviceId(inputDeviceId)
{
    // Use the optimal buffer size recommended by the audio backend.
    m_callbackBufferSize = Nix::Platform::current()->audioHardwareBufferSize();

    // Quick exit if the requested size is too large.
    ASSERT(m_callbackBufferSize + renderBufferSize <= fifoSize);
    if (m_callbackBufferSize + renderBufferSize > fifoSize)
        return;

    m_audioDevice = adoptPtr(Nix::Platform::current()->createAudioDevice(m_inputDeviceId.utf8().data(), m_callbackBufferSize, numberOfInputChannels, numberOfOutputChannels, sampleRate, this));

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
        RefPtr<AudioBus> silence = AudioBus::create(2, renderBufferSize);
        m_inputFifo->push(silence.get());
    }
}

AudioDestinationNix::~AudioDestinationNix()
{
    stop();
}

void AudioDestinationNix::start()
{
    if (!m_isPlaying && m_audioDevice) {
        m_audioDevice->start();
        m_isPlaying = true;
    }
}

void AudioDestinationNix::stop()
{
    if (m_isPlaying && m_audioDevice) {
        m_audioDevice->stop();
        m_isPlaying = false;
    }
}

bool AudioDestinationNix::isPlaying()
{
    return m_isPlaying;
}

float AudioDestinationNix::sampleRate() const
{
    return m_sampleRate;
}

float AudioDestination::hardwareSampleRate()
{
    return Nix::Platform::current()->audioHardwareSampleRate();
}

unsigned long AudioDestination::maxChannelCount()
{
    return Nix::Platform::current()->audioHardwareOutputChannels();
}

void AudioDestinationNix::render(const std::vector<float*>& sourceData, const std::vector<float*>& audioData, size_t numberOfFrames)
{
    if (!m_isPlaying || !m_audioDevice)
        return;

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
    if (sourceData.size()) {
        RefPtr<AudioBus> wrapperBus = AudioBus::create(sourceData.size(), numberOfFrames, false);
        for (unsigned i = 0; i < sourceData.size(); ++i)
            wrapperBus->setChannelMemory(i, sourceData[i], numberOfFrames);
        m_inputFifo->push(wrapperBus.get());
    }

    for (unsigned i = 0; i < m_numberOfOutputChannels; ++i)
        m_renderBus->setChannelMemory(i, audioData[i], numberOfFrames);

    m_fifo->consume(m_renderBus.get(), numberOfFrames);
}

void AudioDestinationNix::provideInput(AudioBus* bus, size_t framesToProcess)
{
    RefPtr<AudioBus> sourceBus;
    if (m_inputFifo->framesInFifo() >= framesToProcess) {
        m_inputFifo->consume(m_inputBus.get(), framesToProcess);
        sourceBus = m_inputBus;
    }

    m_callback.render(sourceBus.get(), bus, framesToProcess);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
