/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

namespace WebCore {

class AudioDestination;
    
class DefaultAudioDestinationNode final : public AudioDestinationNode {
    WTF_MAKE_ISO_ALLOCATED(DefaultAudioDestinationNode);
public:
    static Ref<DefaultAudioDestinationNode> create(BaseAudioContext& context, Optional<float> sampleRate = WTF::nullopt)
    {
        return adoptRef(*new DefaultAudioDestinationNode(context, sampleRate));
    }
    
    virtual ~DefaultAudioDestinationNode();

    unsigned framesPerBuffer() const;
    float sampleRate() const final { return m_sampleRate; }
    
private:
    explicit DefaultAudioDestinationNode(BaseAudioContext&, Optional<float>);
    void createDestination();

    Function<void(Function<void()>&&)> dispatchToRenderThreadFunction();

    void initialize() final;
    void uninitialize() final;
    ExceptionOr<void> setChannelCount(unsigned) final;

    bool requiresTailProcessing() const final { return false; }

    void enableInput(const String& inputDeviceId) final;
    ExceptionOr<void> startRendering() final;
    void resume(Function<void ()>&&) final;
    void suspend(Function<void ()>&&) final;
    void close(Function<void ()>&&) final;
    unsigned maxChannelCount() const final;
    bool isPlaying() final;

    RefPtr<AudioDestination> m_destination;
    String m_inputDeviceId;
    unsigned m_numberOfInputChannels { 0 };
    float m_sampleRate { 0 };
};

} // namespace WebCore
