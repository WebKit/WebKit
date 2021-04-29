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

#include "AudioNodeInput.h"

#include "AudioContext.h"
#include "AudioNode.h"
#include "AudioNodeOutput.h"
#include "AudioUtilities.h"
#include "ChannelCountMode.h"
#include <algorithm>

namespace WebCore {

AudioNodeInput::AudioNodeInput(AudioNode* node)
    : AudioSummingJunction(node->context())
    , m_node(node)
{
    // Set to mono by default.
    m_internalSummingBus = AudioBus::create(1, AudioUtilities::renderQuantumSize);
}

void AudioNodeInput::connect(AudioNodeOutput* output)
{
    ASSERT(context());
    ASSERT(context()->isGraphOwner());
    
    ASSERT(output && node());
    if (!output || !node())
        return;

    if (addOutput(*output))
        output->addInput(this);
}

void AudioNodeInput::disconnect(AudioNodeOutput* output)
{
    ASSERT(context());
    ASSERT(context()->isGraphOwner());

    ASSERT(output && node());
    if (!output || !node())
        return;

    // First try to disconnect from "active" connections.
    if (removeOutput(*output)) {
        output->removeInput(this); // Note: it's important to return immediately after this since the node may be deleted.
        return;
    }

    ASSERT_NOT_REACHED();
}

void AudioNodeInput::outputEnabledStateChanged(AudioNodeOutput& output)
{
    ASSERT(context());
    ASSERT(context()->isGraphOwner());
    AudioSummingJunction::outputEnabledStateChanged(output);

    // Propagate disabled state to outputs.
    ASSERT(node());
    if (!node())
        return;
    if (output.isEnabled())
        node()->enableOutputsIfNecessary();
    else
        node()->disableOutputsIfNecessary();
}

void AudioNodeInput::didUpdate()
{
    node()->checkNumberOfChannelsForInput(this);
}

void AudioNodeInput::updateInternalBus()
{
    ASSERT(context());
    ASSERT(context()->isAudioThread() && context()->isGraphOwner());

    unsigned numberOfInputChannels = numberOfChannels();

    if (numberOfInputChannels == m_internalSummingBus->numberOfChannels())
        return;

    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    m_internalSummingBus = AudioBus::create(numberOfInputChannels, AudioUtilities::renderQuantumSize);
}

unsigned AudioNodeInput::numberOfChannels() const
{
    auto mode = node()->channelCountMode();
    if (mode == ChannelCountMode::Explicit)
        return node()->channelCount();

    // Find the number of channels of the connection with the largest number of channels.
    unsigned maxChannels = std::max(maximumNumberOfChannels(), 1u); // One channel is the minimum allowed.

    if (mode == ChannelCountMode::ClampedMax)
        maxChannels = std::min(maxChannels, static_cast<unsigned>(node()->channelCount()));

    return maxChannels;
}

AudioBus* AudioNodeInput::bus()
{
    ASSERT(context());
    ASSERT(context()->isAudioThread());

    // Handle single connection specially to allow for in-place processing.
    if (numberOfRenderingConnections() == 1 && node()->channelCountMode() == ChannelCountMode::Max)
        return (*renderingOutputs().begin())->bus();

    // Multiple connections case or complex ChannelCountMode (or no connections).
    return internalSummingBus();
}

AudioBus* AudioNodeInput::internalSummingBus()
{
    ASSERT(context());
    ASSERT(context()->isAudioThread());

    return m_internalSummingBus.get();
}

void AudioNodeInput::sumAllConnections(AudioBus* summingBus, size_t framesToProcess)
{
    ASSERT(context());
    ASSERT(context()->isAudioThread());

    // We shouldn't be calling this method if there's only one connection, since it's less efficient.
    ASSERT(numberOfRenderingConnections() > 1 || node()->channelCountMode() != ChannelCountMode::Max);

    ASSERT(summingBus);
    if (!summingBus)
        return;
        
    summingBus->zero();

    auto interpretation = node()->channelInterpretation();

    for (auto* output : renderingOutputs()) {
        ASSERT(output);

        // Render audio from this output.
        AudioBus* connectionBus = output->pull(0, framesToProcess);

        // Sum, with unity-gain.
        summingBus->sumFrom(*connectionBus, interpretation);
    }
}

AudioBus* AudioNodeInput::pull(AudioBus* inPlaceBus, size_t framesToProcess)
{
    ASSERT(context());
    ASSERT(context()->isAudioThread());

    auto numberOfRenderingConnections = this->numberOfRenderingConnections();
    // Handle single connection case.
    if (numberOfRenderingConnections == 1 && node()->channelCountMode() == ChannelCountMode::Max) {
        // The output will optimize processing using inPlaceBus if it's able.
        AudioNodeOutput* output = *renderingOutputs().begin();
        return output->pull(inPlaceBus, framesToProcess);
    }

    AudioBus* internalSummingBus = this->internalSummingBus();

    if (!numberOfRenderingConnections) {
        // At least, generate silence if we're not connected to anything.
        // FIXME: if we wanted to get fancy, we could propagate a 'silent hint' here to optimize the downstream graph processing.
        internalSummingBus->zero();
        return internalSummingBus;
    }

    // Handle multiple connections case.
    sumAllConnections(internalSummingBus, framesToProcess);
    
    return internalSummingBus;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
