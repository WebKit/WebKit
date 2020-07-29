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

#include "ChannelSplitterNode.h"

#include "AudioContext.h"
#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ChannelSplitterNode);

ExceptionOr<Ref<ChannelSplitterNode>> ChannelSplitterNode::create(BaseAudioContext& context, const ChannelSplitterOptions& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };

    context.lazyInitialize();
    
    if (options.numberOfOutputs > AudioContext::maxNumberOfChannels() || !options.numberOfOutputs)
        return Exception { IndexSizeError, "Number of outputs is not in the allowed range"_s };
    
    auto splitter = adoptRef(*new ChannelSplitterNode(context, context.sampleRate(), options.numberOfOutputs));
    
    auto result = splitter->setChannelCount(options.channelCount.valueOr(options.numberOfOutputs));
    if (result.hasException())
        return result.releaseException();
    
    result = splitter->setChannelCountMode(options.channelCountMode.valueOr(ChannelCountMode::Explicit));
    if (result.hasException())
        return result.releaseException();
    
    result = splitter->setChannelInterpretation(options.channelInterpretation.valueOr(ChannelInterpretation::Discrete));
    if (result.hasException())
        return result.releaseException();
    
    return splitter;
}

ChannelSplitterNode::ChannelSplitterNode(BaseAudioContext& context, float sampleRate, unsigned numberOfOutputs)
    : AudioNode(context, sampleRate)
{
    setNodeType(NodeTypeChannelSplitter);

    addInput(makeUnique<AudioNodeInput>(this));

    // Create a fixed number of outputs (able to handle the maximum number of channels fed to an input).
    for (unsigned i = 0; i < numberOfOutputs; ++i)
        addOutput(makeUnique<AudioNodeOutput>(this, 1));
    
    initialize();
}

void ChannelSplitterNode::process(size_t framesToProcess)
{
    AudioBus* source = input(0)->bus();
    ASSERT(source);
    ASSERT_UNUSED(framesToProcess, framesToProcess == source->length());
    
    unsigned numberOfSourceChannels = source->numberOfChannels();
    
    for (unsigned i = 0; i < numberOfOutputs(); ++i) {
        AudioBus* destination = output(i)->bus();
        ASSERT(destination);
        
        if (i < numberOfSourceChannels) {
            // Split the channel out if it exists in the source.
            // It would be nice to avoid the copy and simply pass along pointers, but this becomes extremely difficult with fanout and fanin.
            destination->channel(0)->copyFrom(source->channel(i));
        } else if (output(i)->renderingFanOutCount() > 0) {
            // Only bother zeroing out the destination if it's connected to anything
            destination->zero();
        }
    }
}

void ChannelSplitterNode::reset()
{
}

ExceptionOr<void> ChannelSplitterNode::setChannelCount(unsigned channelCount)
{
    if (channelCount != numberOfOutputs())
        return Exception { IndexSizeError, "Channel count must be set to number of outputs."_s };
    
    return AudioNode::setChannelCount(channelCount);
}

ExceptionOr<void> ChannelSplitterNode::setChannelCountMode(ChannelCountMode mode)
{
    if (mode != ChannelCountMode::Explicit)
        return Exception { InvalidStateError, "Channel count mode cannot be changed from explicit."_s };
    
    return AudioNode::setChannelCountMode(mode);
}

ExceptionOr<void> ChannelSplitterNode::setChannelInterpretation(ChannelInterpretation interpretation)
{
    if (interpretation != ChannelInterpretation::Discrete)
        return Exception { InvalidStateError, "Channel interpretation cannot be changed from discrete."_s };
    
    return AudioNode::setChannelInterpretation(interpretation);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
