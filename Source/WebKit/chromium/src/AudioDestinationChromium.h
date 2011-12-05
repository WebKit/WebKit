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

#ifndef AudioDestinationChromium_h
#define AudioDestinationChromium_h

#include "AudioBus.h"
#include "AudioDestination.h"
#include "AudioSourceProvider.h"
#include "platform/WebAudioDevice.h"
#include "platform/WebVector.h"

namespace WebKit { class WebAudioDevice; }

namespace WebCore { 

// An AudioDestination using Chromium's audio system

class AudioDestinationChromium : public AudioDestination, public WebKit::WebAudioDevice::RenderCallback {
public:
    AudioDestinationChromium(AudioSourceProvider&, float sampleRate);
    virtual ~AudioDestinationChromium();

    virtual void start();
    virtual void stop();
    bool isPlaying() { return m_isPlaying; }

    float sampleRate() const { return m_sampleRate; }

    // WebKit::WebAudioDevice::RenderCallback
    virtual void render(const WebKit::WebVector<float*>& audioData, size_t numberOfFrames);

private:
    // A FIFO (First In First Out) buffer to handle mismatches in the
    // audio backend hardware buffer size and the Web Audio render size.
    class FIFO {
    public:
        // Create a FIFO that gets data from |provider|. The FIFO will
        // be large enough to hold |fifoLength| frames of data of
        // |numberOfChannels| channels. The AudioSourceProvider will
        // be asked to produce |providerSize| frames when the FIFO
        // needs more data.
        FIFO(AudioSourceProvider& provider, unsigned numberOfChannels, size_t fifoLength, size_t providerSize);

        // Read |framesToConsume| frames from the FIFO into the
        // destination. If the FIFO does not have enough data, we ask
        // the |provider| to get more data to fulfill the request.
        void consume(AudioBus* destination, size_t framesToConsume);

    private:
        // Update the FIFO index by the step, with appropriate
        // wrapping around the endpoint.
        int updateIndex(int index, int step) { return (index + step) % m_fifoLength; }

        void findWrapLengths(size_t index, size_t providerSize, size_t& part1Length, size_t& part2Length);
        
        // Fill the FIFO buffer with at least |numberOfFrames| more data.
        void fillBuffer(size_t numberOfFrames);

        // The provider of the data in our FIFO.
        AudioSourceProvider& m_provider;

        // The FIFO itself. In reality, the FIFO is a circular buffer.
        AudioBus m_fifoAudioBus;

        // The total available space in the FIFO.
        size_t m_fifoLength;

        // The number of actual elements in the FIFO
        size_t m_framesInFifo;

        // Where to start reading from the FIFO.
        size_t m_readIndex;

        // Where to start writing to the FIFO.
        size_t m_writeIndex;

        // Number of frames of data that the provider will produce per call.
        unsigned int m_providerSize;

        // Temporary workspace to hold the data from the provider.
        AudioBus m_tempBus;
    };

AudioSourceProvider& m_provider;
    AudioBus m_renderBus;
    float m_sampleRate;
    bool m_isPlaying;
    OwnPtr<WebKit::WebAudioDevice> m_audioDevice;
    size_t m_callbackBufferSize;
    OwnPtr<FIFO> m_fifo;
};

} // namespace WebCore

#endif // AudioDestinationChromium_h
