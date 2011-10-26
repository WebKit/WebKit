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

#include "config.h"

#if ENABLE(WEB_AUDIO) && ENABLE(VIDEO)

#include "MediaElementAudioSourceNode.h"

#include "AudioContext.h"
#include "AudioNodeOutput.h"
#include "MediaPlayer.h"

namespace WebCore {

PassRefPtr<MediaElementAudioSourceNode> MediaElementAudioSourceNode::create(AudioContext* context, HTMLMediaElement* mediaElement)
{
    return adoptRef(new MediaElementAudioSourceNode(context, mediaElement));      
}

MediaElementAudioSourceNode::MediaElementAudioSourceNode(AudioContext* context, HTMLMediaElement* mediaElement)
    : AudioSourceNode(context, context->sampleRate())
    , m_mediaElement(mediaElement)
{
    // Default to stereo. This could change depending on what the media element .src is set to.
    addOutput(adoptPtr(new AudioNodeOutput(this, 2)));
    
    setNodeType(NodeTypeMediaElementAudioSource);

    initialize();
}

MediaElementAudioSourceNode::~MediaElementAudioSourceNode()
{
    m_mediaElement->setAudioSourceNode(0);
    uninitialize();
}

void MediaElementAudioSourceNode::setFormat(size_t, float)
{
    // FIXME: setup a sample-rate converter if necessary to convert to the AudioContext sample-rate.
    ASSERT_NOT_REACHED();
}

void MediaElementAudioSourceNode::process(size_t numberOfFrames)
{
    AudioBus* outputBus = output(0)->bus();

    if (!mediaElement()) {
        outputBus->zero();
        return;
    }
    
    // Use a tryLock() to avoid contention in the real-time audio thread.
    // If we fail to acquire the lock then the HTMLMediaElement must be in the middle of
    // reconfiguring its playback engine, so we output silence in this case.
    if (m_processLock.tryLock()) {
        if (AudioSourceProvider* provider = mediaElement()->audioSourceProvider())
            provider->provideInput(outputBus, numberOfFrames);
        else
            outputBus->zero();
        m_processLock.unlock();
    } else
        outputBus->zero();
}

void MediaElementAudioSourceNode::reset()
{
}

void MediaElementAudioSourceNode::lock()
{
    ref();
    m_processLock.lock();
}

void MediaElementAudioSourceNode::unlock()
{
    m_processLock.unlock();
    deref();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
