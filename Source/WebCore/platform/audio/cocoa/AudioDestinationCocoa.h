/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_AUDIO)

#include "AudioDestination.h"
#include "AudioIOCallback.h"
#include "AudioSourceProvider.h"
#include <AudioUnit/AudioUnit.h>
#include <wtf/RefPtr.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class AudioBus;
class MultiChannelResampler;
class PushPullFIFO;

using CreateAudioDestinationCocoaOverride = std::unique_ptr<AudioDestination>(*)(AudioIOCallback&, float sampleRate);

// An AudioDestination using CoreAudio's default output AudioUnit
class AudioDestinationCocoa : public AudioDestination, public AudioSourceProvider {
public:
    AudioDestinationCocoa(AudioIOCallback&, unsigned numberOfOutputChannels, float sampleRate);
    virtual ~AudioDestinationCocoa();

    WEBCORE_EXPORT static CreateAudioDestinationCocoaOverride createOverride;

protected:
    void setIsPlaying(bool);

    bool isPlaying() final { return m_isPlaying; }
    float sampleRate() const final { return m_contextSampleRate; }
    unsigned framesPerBuffer() const final;
    AudioUnit& outputUnit() { return m_outputUnit; }

    unsigned numberOfOutputChannels() const;
    
    // DefaultOutputUnit callback
    static OSStatus inputProc(void* userData, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32 busNumber, UInt32 numberOfFrames, AudioBufferList* ioData);

    void setAudioStreamBasicDescription(AudioStreamBasicDescription&);

private:
    void start() override;
    void stop() override;

    friend std::unique_ptr<AudioDestination> AudioDestination::create(AudioIOCallback&, const String&, unsigned, unsigned, float);
    
    // AudioSourceProvider.
    void provideInput(AudioBus*, size_t framesToProcess) final;

    void configure();
    void processBusAfterRender(AudioBus&, UInt32 numberOfFrames);

    OSStatus render(const AudioTimeStamp*, UInt32 numberOfFrames, AudioBufferList* ioData);

    AudioUnit m_outputUnit;
    AudioIOCallback& m_callback;

    // To pass the data from FIFO to the audio device callback.
    Ref<AudioBus> m_outputBus;

    // To push the rendered result from WebAudio graph into the FIFO.
    Ref<AudioBus> m_renderBus;

    // Resolves the buffer size mismatch between the WebAudio engine and
    // the callback function from the actual audio device.
    UniqueRef<PushPullFIFO> m_fifo;

    std::unique_ptr<MultiChannelResampler> m_resampler;
    AudioIOPosition m_outputTimestamp;

    float m_contextSampleRate;
    bool m_isPlaying { false };
};

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
