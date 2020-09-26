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

#include "AnalyserNode.h"

#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AnalyserNode);

ExceptionOr<Ref<AnalyserNode>> AnalyserNode::create(BaseAudioContext& context, const AnalyserOptions& options)
{
    auto analyser = adoptRef(*new AnalyserNode(context));
    
    auto result = analyser->handleAudioNodeOptions(options, { 2, ChannelCountMode::Max, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();
    
    result = analyser->setMinMaxDecibels(options.minDecibels, options.maxDecibels);
    if (result.hasException())
        return result.releaseException();
    
    result = analyser->setFftSize(options.fftSize);
    if (result.hasException())
        return result.releaseException();

    result = analyser->setSmoothingTimeConstant(options.smoothingTimeConstant);
    if (result.hasException())
        return result.releaseException();
    
    return analyser;
}

AnalyserNode::AnalyserNode(BaseAudioContext& context)
    : AudioBasicInspectorNode(context, NodeTypeAnalyser)
{
    addOutput(2);
    
    initialize();
}

AnalyserNode::~AnalyserNode()
{
    uninitialize();
}

void AnalyserNode::process(size_t framesToProcess)
{
    AudioBus* outputBus = output(0)->bus();

    if (!isInitialized()) {
        outputBus->zero();
        return;
    }

    AudioBus* inputBus = input(0)->bus();
    
    // Give the analyser the audio which is passing through this AudioNode. This must always
    // be done so that the state of the Analyser reflects the current input.
    m_analyser.writeInput(inputBus, framesToProcess);

    if (!input(0)->isConnected()) {
        outputBus->zero();
        return;
    }

    // For in-place processing, our override of pullInputs() will just pass the audio data through unchanged if the channel count matches from input to output
    // (resulting in inputBus == outputBus). Otherwise, do an up-mix to stereo.
    if (inputBus != outputBus)
        outputBus->copyFrom(*inputBus);
}

ExceptionOr<void> AnalyserNode::setFftSize(unsigned size)
{
    if (!m_analyser.setFftSize(size))
        return Exception { IndexSizeError, "fftSize must be power of 2 in the range 32 to 32768."_s };
    return { };
}

ExceptionOr<void> AnalyserNode::setMinMaxDecibels(double minDecibels, double maxDecibels)
{
    if (maxDecibels <= minDecibels)
        return Exception { IndexSizeError, "minDecibels must be less than maxDecibels."_s };
    
    m_analyser.setMinDecibels(minDecibels);
    m_analyser.setMaxDecibels(maxDecibels);
    return { };
}

ExceptionOr<void> AnalyserNode::setMinDecibels(double k)
{
    if (k >= maxDecibels())
        return Exception { IndexSizeError, "minDecibels must be less than maxDecibels."_s };

    m_analyser.setMinDecibels(k);
    return { };
}

ExceptionOr<void> AnalyserNode::setMaxDecibels(double k)
{
    if (k <= minDecibels())
        return Exception { IndexSizeError, "maxDecibels must be greater than minDecibels."_s };

    m_analyser.setMaxDecibels(k);
    return { };
}

ExceptionOr<void> AnalyserNode::setSmoothingTimeConstant(double k)
{
    if (k < 0 || k > 1)
        return Exception { IndexSizeError, "Smoothing time constant needs to be between 0 and 1."_s };

    m_analyser.setSmoothingTimeConstant(k);
    return { };
}

bool AnalyserNode::requiresTailProcessing() const
{
    // Tail time is always non-zero so tail processing is required.
    return true;
}

void AnalyserNode::updatePullStatus()
{
    ASSERT(context().isGraphOwner());

    if (output(0)->isConnected()) {
        // When an AudioBasicInspectorNode is connected to a downstream node, it
        // will get pulled by the downstream node, thus remove it from the context's
        // automatic pull list.
        if (m_needAutomaticPull) {
            context().removeAutomaticPullNode(*this);
            m_needAutomaticPull = false;
        }
    } else {
        unsigned numberOfInputConnections = input(0)->numberOfRenderingConnections();
        // When an AnalyserNode is not connected to any downstream node
        // while still connected from upstream node(s), add it to the context's
        // automatic pull list.
        //
        // But don't remove the AnalyserNode if there are no inputs
        // connected to the node. The node needs to be pulled so that the
        // internal state is updated with the correct input signal (of
        // zeroes).
        if (numberOfInputConnections && !m_needAutomaticPull) {
            context().addAutomaticPullNode(*this);
            m_needAutomaticPull = true;
        }
    }
}

bool AnalyserNode::propagatesSilence() const
{
    // An AnalyserNode does actually propogate silence, but to get the
    // time and FFT data updated correctly, process() needs to be
    // called even if all the inputs are silent.
    return false;
}

double AnalyserNode::tailTime() const
{
    return RealtimeAnalyser::MaxFFTSize / static_cast<double>(context().sampleRate());
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
