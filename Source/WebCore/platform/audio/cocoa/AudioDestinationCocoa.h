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
#include "AudioOutputUnitAdaptor.h"
#include <AudioUnit/AudioUnit.h>
#include <wtf/RefPtr.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class AudioBus;
class MultiChannelResampler;
class PushPullFIFO;

using CreateAudioDestinationCocoaOverride = Ref<AudioDestination>(*)(AudioIOCallback&, float sampleRate);

// An AudioDestination using CoreAudio's default output AudioUnit
class AudioDestinationCocoa : public AudioDestination, public AudioUnitRenderer {
public:
    WEBCORE_EXPORT AudioDestinationCocoa(AudioIOCallback&, unsigned numberOfOutputChannels, float sampleRate, bool configureAudioOutputUnit = true);
    WEBCORE_EXPORT virtual ~AudioDestinationCocoa();

    WEBCORE_EXPORT static CreateAudioDestinationCocoaOverride createOverride;

protected:
    WEBCORE_EXPORT bool hasEnoughFrames(UInt32 numberOfFrames) const;
    WEBCORE_EXPORT OSStatus render(double sampleTime, uint64_t hostTime, UInt32 numberOfFrames, AudioBufferList* ioData) final;

    WEBCORE_EXPORT void setIsPlaying(bool);
    bool isPlaying() final { return m_isPlaying; }
    float sampleRate() const final { return m_contextSampleRate; }
    WEBCORE_EXPORT unsigned framesPerBuffer() const final;

    WEBCORE_EXPORT unsigned numberOfOutputChannels() const;

    WEBCORE_EXPORT void getAudioStreamBasicDescription(AudioStreamBasicDescription&);

private:
    friend Ref<AudioDestination> AudioDestination::create(AudioIOCallback&, const String&, unsigned, unsigned, float);

    WEBCORE_EXPORT void start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&&) final;
    WEBCORE_EXPORT void stop(CompletionHandler<void(bool)>&&) final;

    virtual void startRendering(CompletionHandler<void(bool)>&&);
    virtual void stopRendering(CompletionHandler<void(bool)>&&);

    void renderOnRenderingThead(size_t framesToRender);

    AudioOutputUnitAdaptor m_audioOutputUnitAdaptor;

    // To pass the data from FIFO to the audio device callback.
    Ref<AudioBus> m_outputBus;

    // To push the rendered result from WebAudio graph into the FIFO.
    Ref<AudioBus> m_renderBus;

    // Resolves the buffer size mismatch between the WebAudio engine and
    // the callback function from the actual audio device.
    UniqueRef<PushPullFIFO> m_fifo;
    Lock m_fifoLock;

    std::unique_ptr<MultiChannelResampler> m_resampler;
    AudioIOPosition m_outputTimestamp;

    Lock m_dispatchToRenderThreadLock;
    Function<void(Function<void()>&&)> m_dispatchToRenderThread;

    float m_contextSampleRate;

    Lock m_isPlayingLock;
    bool m_isPlaying { false };
};

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
