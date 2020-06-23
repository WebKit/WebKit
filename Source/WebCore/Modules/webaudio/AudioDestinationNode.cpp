/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#if ENABLE(WEB_AUDIO)

#include "AudioDestinationNode.h"

#include "AudioContext.h"
#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include "AudioUtilities.h"
#include "DenormalDisabler.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
    
WTF_MAKE_ISO_ALLOCATED_IMPL(AudioDestinationNode);

AudioDestinationNode::AudioDestinationNode(BaseAudioContext& context, float sampleRate)
    : AudioNode(context, sampleRate)
    , m_currentSampleFrame(0)
    , m_isSilent(true)
    , m_isEffectivelyPlayingAudio(false)
    , m_muted(false)
{
    setNodeType(NodeTypeDestination);
    addInput(makeUnique<AudioNodeInput>(this));
}

AudioDestinationNode::~AudioDestinationNode()
{
    uninitialize();
}

void AudioDestinationNode::render(AudioBus*, AudioBus* destinationBus, size_t numberOfFrames)
{
    // We don't want denormals slowing down any of the audio processing
    // since they can very seriously hurt performance.
    // This will take care of all AudioNodes because they all process within this scope.
    DenormalDisabler denormalDisabler;
    
    context().setAudioThread(Thread::current());
    
    if (!context().isInitialized()) {
        destinationBus->zero();
        setIsSilent(true);
        return;
    }

    ASSERT(numberOfFrames);
    if (!numberOfFrames) {
        destinationBus->zero();
        setIsSilent(true);
        return;
    }

    // Let the context take care of any business at the start of each render quantum.
    context().handlePreRenderTasks();

    // This will cause the node(s) connected to us to process, which in turn will pull on their input(s),
    // all the way backwards through the rendering graph.
    AudioBus* renderedBus = input(0)->pull(destinationBus, numberOfFrames);
    
    if (!renderedBus)
        destinationBus->zero();
    else if (renderedBus != destinationBus) {
        // in-place processing was not possible - so copy
        destinationBus->copyFrom(*renderedBus);
    }

    // Process nodes which need a little extra help because they are not connected to anything, but still need to process.
    context().processAutomaticPullNodes(numberOfFrames);

    // Let the context take care of any business at the end of each render quantum.
    context().handlePostRenderTasks();
    
    // Advance current sample-frame.
    m_currentSampleFrame += numberOfFrames;

    setIsSilent(destinationBus->isSilent());

    // The reason we are handling mute after the call to setIsSilent() is because the muted state does
    // not affect the audio destination node's effective playing state.
    if (m_muted)
        destinationBus->zero();
}

void AudioDestinationNode::isPlayingDidChange()
{
    updateIsEffectivelyPlayingAudio();
}

void AudioDestinationNode::setIsSilent(bool isSilent)
{
    if (m_isSilent == isSilent)
        return;

    m_isSilent = isSilent;
    updateIsEffectivelyPlayingAudio();
}

void AudioDestinationNode::updateIsEffectivelyPlayingAudio()
{
    bool isEffectivelyPlayingAudio = isPlaying() && !m_isSilent;
    if (m_isEffectivelyPlayingAudio == isEffectivelyPlayingAudio)
        return;

    m_isEffectivelyPlayingAudio = isEffectivelyPlayingAudio;
    context().isPlayingAudioDidChange();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
