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

#include "AudioBuffer.h"
#include "AudioDestinationNode.h"
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

class AudioBus;
class AudioContext;
    
class OfflineAudioDestinationNode final : public AudioDestinationNode {
    WTF_MAKE_ISO_ALLOCATED(OfflineAudioDestinationNode);
public:
    static Ref<OfflineAudioDestinationNode> create(AudioContextBase& context, AudioBuffer* renderTarget)
    {
        return adoptRef(*new OfflineAudioDestinationNode(context, renderTarget));
    }

    virtual ~OfflineAudioDestinationNode();
    
    // AudioNode   
    void initialize() override;
    void uninitialize() override;

    // AudioDestinationNode
    void enableInput(const String&) override { }
    void startRendering() override;

    float sampleRate() const override { return m_renderTarget->sampleRate(); }

private:
    OfflineAudioDestinationNode(AudioContextBase&, AudioBuffer* renderTarget);

    // This AudioNode renders into this AudioBuffer.
    RefPtr<AudioBuffer> m_renderTarget;
    
    // Temporary AudioBus for each render quantum.
    RefPtr<AudioBus> m_renderBus;
    
    // Rendering thread.
    RefPtr<Thread> m_renderThread;
    bool m_startedRendering;
    bool offlineRender();
};

} // namespace WebCore
