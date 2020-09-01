/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 * Copyright (C) 2020, Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUDIO) && ENABLE(VIDEO)

#include "AudioNode.h"
#include "AudioSourceProviderClient.h"
#include "HTMLMediaElement.h"
#include "MultiChannelResampler.h"
#include <memory>
#include <wtf/Lock.h>

namespace WebCore {

class AudioContext;
struct MediaElementAudioSourceOptions;
    
class MediaElementAudioSourceNode final : public AudioNode, public AudioSourceProviderClient {
    WTF_MAKE_ISO_ALLOCATED(MediaElementAudioSourceNode);
public:
    static ExceptionOr<Ref<MediaElementAudioSourceNode>> create(BaseAudioContext&, MediaElementAudioSourceOptions&&);

    virtual ~MediaElementAudioSourceNode();

    HTMLMediaElement& mediaElement() { return m_mediaElement; }

    // AudioNode
    void process(size_t framesToProcess) override;
    void reset() override;
    
    // AudioSourceProviderClient
    void setFormat(size_t numberOfChannels, float sampleRate) override;

    void lock();
    void unlock();

private:
    MediaElementAudioSourceNode(BaseAudioContext&, Ref<HTMLMediaElement>&&);

    double tailTime() const override { return 0; }
    double latencyTime() const override { return 0; }
    bool requiresTailProcessing() const final { return false; }

    // As an audio source, we will never propagate silence.
    bool propagatesSilence() const override { return false; }

    bool wouldTaintOrigin();

    Ref<HTMLMediaElement> m_mediaElement;
    Lock m_processMutex;

    unsigned m_sourceNumberOfChannels { 0 };
    double m_sourceSampleRate { 0 };
    bool m_muted { false };

    std::unique_ptr<MultiChannelResampler> m_multiChannelResampler;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && ENABLE(VIDEO)
