/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 * Copyright (C) 2020-2021, Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AudioDestinationNode.h"
#include "AudioIOCallback.h"

namespace WebCore {

class AudioContext;
class AudioDestination;
    
class DefaultAudioDestinationNode final : public AudioDestinationNode, public AudioIOCallback {
    WTF_MAKE_ISO_ALLOCATED(DefaultAudioDestinationNode);
public:
    explicit DefaultAudioDestinationNode(AudioContext&, std::optional<float> sampleRate = std::nullopt);
    ~DefaultAudioDestinationNode();

    AudioContext& context();
    const AudioContext& context() const;

    unsigned framesPerBuffer() const;
    
    void startRendering(CompletionHandler<void(std::optional<Exception>&&)>&&) final;
    void resume(CompletionHandler<void(std::optional<Exception>&&)>&&);
    void suspend(CompletionHandler<void(std::optional<Exception>&&)>&&);
    void close(CompletionHandler<void()>&&);

    void setMuted(bool muted) { m_muted = muted; }
    bool isPlayingAudio() const { return m_isEffectivelyPlayingAudio; }

private:
    void createDestination();
    void clearDestination();
    void recreateDestination();

    // AudioIOCallback
    // The audio hardware calls render() to get the next render quantum of audio into destinationBus.
    // It will optionally give us local/live audio input in sourceBus (if it's not 0).
    void render(AudioBus* sourceBus, AudioBus* destinationBus, size_t numberOfFrames, const AudioIOPosition& outputPosition) final;
    void isPlayingDidChange() final;

    void setIsSilent(bool);
    void updateIsEffectivelyPlayingAudio();

    Function<void(Function<void()>&&)> dispatchToRenderThreadFunction();

    void initialize() final;
    void uninitialize() final;
    ExceptionOr<void> setChannelCount(unsigned) final;

    bool requiresTailProcessing() const final { return false; }

    void enableInput(const String& inputDeviceId) final;
    void restartRendering() final;
    unsigned maxChannelCount() const final;

    RefPtr<AudioDestination> m_destination;
    String m_inputDeviceId;
    unsigned m_numberOfInputChannels { 0 };
    bool m_wasDestinationStarted { false };
    bool m_isEffectivelyPlayingAudio { false };
    bool m_isSilent { true };
    bool m_muted { false };
};

} // namespace WebCore
