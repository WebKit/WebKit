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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioDestinationNode.h"

#include "AudioBus.h"
#include "AudioContext.h"
#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include <wtf/Threading.h>

namespace WebCore {

AudioDestinationNode::AudioDestinationNode(AudioContext* context)
    : AudioNode(context, AudioDestination::hardwareSampleRate())
    , m_currentTime(0.0)
{
    addInput(adoptPtr(new AudioNodeInput(this)));
    
    setType(NodeTypeDestination);
    
    initialize();
}

AudioDestinationNode::~AudioDestinationNode()
{
    uninitialize();
}

void AudioDestinationNode::initialize()
{
    if (isInitialized())
        return;

    double hardwareSampleRate = AudioDestination::hardwareSampleRate();
#ifndef NDEBUG    
    fprintf(stderr, ">>>> hardwareSampleRate = %f\n", hardwareSampleRate);
#endif
    
    m_destination = AudioDestination::create(*this, hardwareSampleRate);
    m_destination->start();
    
    m_isInitialized = true;
}

void AudioDestinationNode::uninitialize()
{
    if (!isInitialized())
        return;

    m_destination->stop();

    m_isInitialized = false;
}

// The audio hardware calls us back here to gets its input stream.
void AudioDestinationNode::provideInput(AudioBus* destinationBus, size_t numberOfFrames)
{
    context()->setAudioThread(currentThread());
    
    if (!context()->isRunnable()) {
        destinationBus->zero();
        return;
    }

    // This will cause the node(s) connected to us to process, which in turn will pull on their input(s),
    // all the way backwards through the rendering graph.
    AudioBus* renderedBus = input(0)->pull(destinationBus, numberOfFrames);
    
    if (!renderedBus)
        destinationBus->zero();
    else if (renderedBus != destinationBus) {
        // in-place processing was not possible - so copy
        destinationBus->copyFrom(*renderedBus);
    }

    // Let the context take care of any business at the end of each render quantum.
    context()->handlePostRenderTasks();
    
    // Advance current time.
    m_currentTime += numberOfFrames / sampleRate();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
