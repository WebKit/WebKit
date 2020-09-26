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

#include "DelayNode.h"

#include "DelayOptions.h"
#include "DelayProcessor.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DelayNode);

constexpr double maximumAllowedDelayTime = 180;

inline DelayNode::DelayNode(BaseAudioContext& context, double maxDelayTime)
    : AudioBasicProcessorNode(context, NodeTypeDelay)
{
    m_processor = makeUnique<DelayProcessor>(context, context.sampleRate(), 1, maxDelayTime);

    // Initialize so that AudioParams can be processed.
    initialize();
}

ExceptionOr<Ref<DelayNode>> DelayNode::create(BaseAudioContext& context, const DelayOptions& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };

    if (options.maxDelayTime <= 0 || options.maxDelayTime >= maximumAllowedDelayTime)
        return Exception { NotSupportedError };

    auto delayNode = adoptRef(*new DelayNode(context, options.maxDelayTime));

    auto result = delayNode->handleAudioNodeOptions(options, { 2, ChannelCountMode::Max, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();

    delayNode->delayTime().setValue(options.delayTime);

    return delayNode;
}

AudioParam& DelayNode::delayTime()
{
    return static_cast<DelayProcessor&>(*m_processor).delayTime();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
