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
#include <AudioUnit/AudioUnit.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class AudioBus;

using CreateAudioDestinationCocoaOverride = std::unique_ptr<AudioDestination>(*)(AudioIOCallback&, float sampleRate);

// An AudioDestination using CoreAudio's default output AudioUnit
class AudioDestinationCocoa : public AudioDestination {
public:
    AudioDestinationCocoa(AudioIOCallback&, float sampleRate);
    virtual ~AudioDestinationCocoa();

    WEBCORE_EXPORT static CreateAudioDestinationCocoaOverride createOverride;

protected:
    void setIsPlaying(bool);

    bool isPlaying() final { return m_isPlaying; }
    float sampleRate() const final { return m_sampleRate; }
    unsigned framesPerBuffer() const final;
    AudioUnit& outputUnit() { return m_outputUnit; }
    
    // DefaultOutputUnit callback
    static OSStatus inputProc(void* userData, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32 busNumber, UInt32 numberOfFrames, AudioBufferList* ioData);

    void setAudioStreamBasicDescription(AudioStreamBasicDescription&, float sampleRate);

private:
    void start() override;
    void stop() override;

    friend std::unique_ptr<AudioDestination> AudioDestination::create(AudioIOCallback&, const String&, unsigned, unsigned, float);
    
    void configure();
    void processBusAfterRender(AudioBus&, UInt32 numberOfFrames);

    OSStatus render(UInt32 numberOfFrames, AudioBufferList* ioData);

    AudioUnit m_outputUnit;
    AudioIOCallback& m_callback;
    Ref<AudioBus> m_renderBus;
    Ref<AudioBus> m_spareBus;

    float m_sampleRate;
    bool m_isPlaying { false };

    unsigned m_startSpareFrame { 0 };
    unsigned m_endSpareFrame { 0 };
};

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
