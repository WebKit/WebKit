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

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ChannelMergerNode);

ExceptionOr<Ref<ChannelMergerNode>> ChannelMergerNode::create(BaseAudioContext& context, const ChannelMergerOptions& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };
    
    if (options.numberOfInputs > AudioContext::maxNumberOfChannels() || !options.numberOfInputs)
        return Exception { IndexSizeError, "Number of inputs is not in the allowed range."_s };
    
    auto merger = adoptRef(*new ChannelMergerNode(context, options.numberOfInputs));
    
    auto result = merger->handleAudioNodeOptions(options, { 1, ChannelCountMode::Explicit, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();
    
    return merger;
}

ChannelMergerNode::ChannelMergerNode(BaseAudioContext& context, unsigned numberOfInputs)
    : AudioNode(context, NodeTypeChannelMerger)
{
    // Create the requested number of inputs.
    for (unsigned i = 0; i < numberOfInputs; ++i)
        addInput();

    addOutput(numberOfInputs);
    
    initialize();

    BaseAudioContext::AutoLocker contextLocker(context);
    disableOutputs();
}

void ChannelMergerNode::process(size_t framesToProcess)
{
    AudioNodeOutput* output = this->output(0);
    ASSERT(output);
    ASSERT_UNUSED(framesToProcess, framesToProcess == output->bus()->length());
    ASSERT(numberOfInputs() == output->numberOfChannels());
    
    // Merge all the channels from all the inputs into one output.
    for (unsigned i = 0; i < numberOfInputs(); ++i) {
        AudioNodeInput* input = this->input(i);
        ASSERT(input->numberOfChannels() == 1u);
        auto* outputChannel = output->bus()->channel(i);
        if (input->isConnected()) {
            // The mixing rules will be applied so multiple channels are down-
            // mixed to mono (when the mixing rule is defined). Note that only
            // the first channel will be taken for the undefined input channel
            // layout.
            //
            // See:
            // http://webaudio.github.io/web-audio-api/#channel-up-mixing-and-down-mixing
            auto* inputChannel = input->bus()->channel(0);
            outputChannel->copyFrom(inputChannel);
        } else {
            // If input is unconnected, fill zeros in the channel.
            outputChannel->zero();
        }
    }
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
