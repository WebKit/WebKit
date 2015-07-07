/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#ifndef MediaStreamAudioSourceNode_h
#define MediaStreamAudioSourceNode_h

#if ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)

#include "AudioNode.h"
#include "AudioSourceProvider.h"
#include "AudioSourceProviderClient.h"
#include "MediaStream.h"
#include <mutex>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class AudioContext;

class MediaStreamAudioSourceNode : public AudioNode, public AudioSourceProviderClient {
public:
    static PassRefPtr<MediaStreamAudioSourceNode> create(AudioContext*, MediaStream*, MediaStreamTrack*, AudioSourceProvider*);

    virtual ~MediaStreamAudioSourceNode();

    MediaStream* mediaStream() { return m_mediaStream.get(); }

    // AudioNode
    virtual void process(size_t framesToProcess);
    virtual void reset();

    // AudioSourceProviderClient
    virtual void setFormat(size_t numberOfChannels, float sampleRate);

    AudioSourceProvider* audioSourceProvider() const { return m_audioSourceProvider; }

private:
    MediaStreamAudioSourceNode(AudioContext*, MediaStream*, MediaStreamTrack*, AudioSourceProvider*);

    virtual double tailTime() const override { return 0; }
    virtual double latencyTime() const override { return 0; }

    // As an audio source, we will never propagate silence.
    virtual bool propagatesSilence() const override { return false; }

    RefPtr<MediaStream> m_mediaStream;
    RefPtr<MediaStreamTrack> m_audioTrack;
    AudioSourceProvider* m_audioSourceProvider;

    std::mutex m_processMutex;

    unsigned m_sourceNumberOfChannels;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)

#endif // MediaStreamAudioSourceNode_h
