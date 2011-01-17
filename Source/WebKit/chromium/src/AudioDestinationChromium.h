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
#include "WebAudioDevice.h"
#include "WebVector.h"

namespace WebKit { class WebAudioDevice; }

namespace WebCore { 

// An AudioDestination using Chromium's audio system

class AudioDestinationChromium : public AudioDestination, public WebKit::WebAudioDevice::RenderCallback {
public:
    AudioDestinationChromium(AudioSourceProvider&, double sampleRate);
    virtual ~AudioDestinationChromium();

    virtual void start();
    virtual void stop();
    bool isPlaying() { return m_isPlaying; }

    double sampleRate() const { return m_sampleRate; }

    // WebKit::WebAudioDevice::RenderCallback
    virtual void render(const WebKit::WebVector<float*>& audioData, size_t numberOfFrames);

private:
    AudioSourceProvider& m_provider;
    AudioBus m_renderBus;
    double m_sampleRate;
    bool m_isPlaying;
    OwnPtr<WebKit::WebAudioDevice> m_audioDevice;
};

} // namespace WebCore

#endif // AudioDestinationChromium_h
