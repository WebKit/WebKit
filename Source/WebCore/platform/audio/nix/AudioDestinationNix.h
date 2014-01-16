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

#ifndef AudioDestinationNix_h
#define AudioDestinationNix_h

#include "AudioBus.h"
#include "AudioDestination.h"
#include "AudioIOCallback.h"
#include "AudioSourceProvider.h"
#include <public/AudioDevice.h>
#include <vector>

namespace WebCore {

class AudioFIFO;
class AudioPullFIFO;

class AudioDestinationNix : public AudioDestination, public Nix::AudioDevice::RenderCallback, public AudioSourceProvider {
public:
    AudioDestinationNix(AudioIOCallback&, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate);
    virtual ~AudioDestinationNix();

    virtual void start() override;
    virtual void stop() override;
    virtual bool isPlaying() override;
    virtual float sampleRate() const override;

    // WebKit::WebAudioDevice::RenderCallback
    virtual void render(const std::vector<float*>& sourceData, const std::vector<float*>& audioData, size_t numberOfFrames) override;

    // WebCore::AudioSourceProvider
    virtual void provideInput(AudioBus*, size_t framesToProcess) override;

private:
    AudioIOCallback& m_callback;
    unsigned m_numberOfOutputChannels;
    RefPtr<AudioBus> m_inputBus;
    RefPtr<AudioBus> m_renderBus;
    float m_sampleRate;
    bool m_isPlaying;
    OwnPtr<Nix::AudioDevice> m_audioDevice;
    size_t m_callbackBufferSize;
    String m_inputDeviceId;

    OwnPtr<AudioFIFO> m_inputFifo;
    OwnPtr<AudioPullFIFO> m_fifo;
};

} // namespace WebCore

#endif // AudioDestinationNix_h
