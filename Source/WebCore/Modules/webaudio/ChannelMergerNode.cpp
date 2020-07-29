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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#include "ChannelMergerNode.h"

#include "AudioContext.h"
#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include <wtf/IsoMallocInlines.h>

const unsigned DefaultNumberOfOutputChannels = 1;

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ChannelMergerNode);

ExceptionOr<Ref<ChannelMergerNode>> ChannelMergerNode::create(BaseAudioContext& context, const ChannelMergerOptions& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };

    context.lazyInitialize();
    
    if (options.numberOfInputs > AudioContext::maxNumberOfChannels() || !options.numberOfInputs)
        return Exception { IndexSizeError, "Number of inputs is not in the allowed range."_s };
    
    auto merger = adoptRef(*new ChannelMergerNode(context, context.sampleRate(), options.numberOfInputs));
    
    auto result = merger->setChannelCount(options.channelCount.valueOr(1));
    if (result.hasException())
        return result.releaseException();
    
    result = merger->setChannelCountMode(options.channelCountMode.valueOr(ChannelCountMode::Explicit));
    if (result.hasException())
        return result.releaseException();
    
    result = merger->setChannelInterpretation(options.channelInterpretation.valueOr(ChannelInterpretation::Speakers));
    if (result.hasException())
        return result.releaseException();
    
    return merger;
}

ChannelMergerNode::ChannelMergerNode(BaseAudioContext& context, float sampleRate, unsigned numberOfInputs)
    : AudioNode(context, sampleRate)
    , m_desiredNumberOfOutputChannels(DefaultNumberOfOutputChannels)
{
    setNodeType(NodeTypeChannelMerger);

    // Create the requested number of inputs.
    for (unsigned i = 0; i < numberOfInputs; ++i)
        addInput(makeUnique<AudioNodeInput>(this));

    addOutput(makeUnique<AudioNodeOutput>(this, 1));
    
    initialize();
}

void ChannelMergerNode::process(size_t framesToProcess)
{
    AudioNodeOutput* output = this->output(0);
    ASSERT(output);
    ASSERT_UNUSED(framesToProcess, framesToProcess == output->bus()->length());

    // Output bus not updated yet, so just output silence.
    if (m_desiredNumberOfOutputChannels != output->numberOfChannels()) {
        output->bus()->zero();
        return;
    }
    
    // Merge all the channels from all the inputs into one output.
    unsigned outputChannelIndex = 0;
    for (unsigned i = 0; i < numberOfInputs(); ++i) {
        AudioNodeInput* input = this->input(i);
        if (input->isConnected()) {
            unsigned numberOfInputChannels = input->bus()->numberOfChannels();
            
            // Merge channels from this particular input.
            for (unsigned j = 0; j < numberOfInputChannels; ++j) {
                AudioChannel* inputChannel = input->bus()->channel(j);
                AudioChannel* outputChannel = output->bus()->channel(outputChannelIndex);
                outputChannel->copyFrom(inputChannel);
                
                ++outputChannelIndex;
            }
        }
    }
    
    ASSERT(outputChannelIndex == output->numberOfChannels());
}

void ChannelMergerNode::reset()
{
}

// Any time a connection or disconnection happens on any of our inputs, we potentially need to change the
// number of channels of our output.
void ChannelMergerNode::checkNumberOfChannelsForInput(AudioNodeInput* input)
{
    ASSERT(context().isAudioThread() && context().isGraphOwner());

    // Count how many channels we have all together from all of the inputs.
    unsigned numberOfOutputChannels = 0;
    for (unsigned i = 0; i < numberOfInputs(); ++i) {
        AudioNodeInput* input = this->input(i);
        if (input->isConnected())
            numberOfOutputChannels += input->numberOfChannels();
    }

    // Set the correct number of channels on the output
    AudioNodeOutput* output = this->output(0);
    ASSERT(output);
    output->setNumberOfChannels(numberOfOutputChannels);
    // There can in rare cases be a slight delay before the output bus is updated to the new number of
    // channels because of tryLocks() in the context's updating system. So record the new number of
    // output channels here.
    m_desiredNumberOfOutputChannels = numberOfOutputChannels;

    AudioNode::checkNumberOfChannelsForInput(input);
}

ExceptionOr<void> ChannelMergerNode::setChannelCount(unsigned channelCount)
{
    if (channelCount != 1)
        return Exception { InvalidStateError, "Channel count cannot be changed from 1."_s };
    
    return AudioNode::setChannelCount(channelCount);
}

ExceptionOr<void> ChannelMergerNode::setChannelCountMode(ChannelCountMode mode)
{
    if (mode != ChannelCountMode::Explicit)
        return Exception { InvalidStateError, "Channel count mode cannot be changed from explicit."_s };
    
    return AudioNode::setChannelCountMode(mode);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
