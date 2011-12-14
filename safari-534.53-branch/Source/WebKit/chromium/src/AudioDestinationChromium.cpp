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

#include "AudioSourceProvider.h"
#include "WebKit.h"
#include "WebKitClient.h"

using namespace WebKit;

namespace WebCore {

// Buffer size that the Chromium audio system will call us back with.
#if OS(DARWIN)
// For Mac OS X the chromium audio backend uses a low-latency CoreAudio API, so a low buffer size is possible.
const unsigned callbackBufferSize = 128;
#else
// This value may need to be tuned based on the OS.
// FIXME: It may be possible to reduce this value once real-time threads
// and other Chromium audio improvements are made.
const unsigned callbackBufferSize = 2048;
#endif

// Buffer size at which the web audio engine will render.
const unsigned renderBufferSize = 128;

const unsigned renderCountPerCallback = callbackBufferSize / renderBufferSize;

// FIXME: add support for multi-channel.
const unsigned numberOfChannels = 2;

// Factory method: Chromium-implementation
PassOwnPtr<AudioDestination> AudioDestination::create(AudioSourceProvider& provider, double sampleRate)
{
    return adoptPtr(new AudioDestinationChromium(provider, sampleRate));
}

AudioDestinationChromium::AudioDestinationChromium(AudioSourceProvider& provider, double sampleRate)
    : m_provider(provider)
    , m_renderBus(numberOfChannels, renderBufferSize, false)
    , m_sampleRate(sampleRate)
    , m_isPlaying(false)
{
    m_audioDevice = adoptPtr(webKitClient()->createAudioDevice(callbackBufferSize, numberOfChannels, sampleRate, this));
    ASSERT(m_audioDevice.get());
}

AudioDestinationChromium::~AudioDestinationChromium()
{
    stop();
}

void AudioDestinationChromium::start()
{
    if (!m_isPlaying && m_audioDevice.get()) {
        m_audioDevice->start();
        m_isPlaying = true;
    }
}

void AudioDestinationChromium::stop()
{
    if (m_isPlaying && m_audioDevice.get()) {
        m_audioDevice->stop();
        m_isPlaying = false;
    }
}

double AudioDestination::hardwareSampleRate()
{
    return webKitClient()->audioHardwareSampleRate();
}

// Pulls on our provider to get the rendered audio stream.
void AudioDestinationChromium::render(const WebVector<float*>& audioData, size_t numberOfFrames)
{
    bool isNumberOfChannelsGood = audioData.size() == numberOfChannels;
    if (!isNumberOfChannelsGood) {
        ASSERT_NOT_REACHED();
        return;
    }
        
    bool isBufferSizeGood = numberOfFrames == callbackBufferSize;
    if (!isBufferSizeGood) {
        ASSERT_NOT_REACHED();
        return;
    }

    // Split up the callback buffer into smaller chunks which we'll render one after the other.
    for (unsigned i = 0; i < renderCountPerCallback; ++i) {
        m_renderBus.setChannelMemory(0, audioData[0] + i * renderBufferSize, renderBufferSize);
        m_renderBus.setChannelMemory(1, audioData[1] + i * renderBufferSize, renderBufferSize);
        m_provider.provideInput(&m_renderBus, renderBufferSize);
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
