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

#include "config.h"
#include "MediaStreamAudioDestinationNode.h"

#if ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)

#include "AudioContext.h"
#include "AudioNodeInput.h"
#include "Document.h"
#include "MediaStream.h"
#include "MediaStreamAudioSource.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Locker.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaStreamAudioDestinationNode);

ExceptionOr<Ref<MediaStreamAudioDestinationNode>> MediaStreamAudioDestinationNode::create(BaseAudioContext& context, const AudioNodeOptions& options)
{
    // This behavior is not part of the specification. This is done for consistency with Blink.
    if (context.isStopped() || !context.scriptExecutionContext())
        return Exception { NotAllowedError, "Cannot create a MediaStreamAudioDestinationNode in a detached frame"_s };

    auto node = adoptRef(*new MediaStreamAudioDestinationNode(context));

    auto result = node->handleAudioNodeOptions(options, { 2, ChannelCountMode::Explicit, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();

    return node;
}

MediaStreamAudioDestinationNode::MediaStreamAudioDestinationNode(BaseAudioContext& context)
    : AudioBasicInspectorNode(context, NodeTypeMediaStreamAudioDestination)
    , m_source(MediaStreamAudioSource::create(context.sampleRate()))
    , m_stream(MediaStream::create(*context.document(), MediaStreamPrivate::create(context.document()->logger(), m_source.copyRef())))
{
    initialize();
}

MediaStreamAudioDestinationNode::~MediaStreamAudioDestinationNode()
{
    uninitialize();
}

void MediaStreamAudioDestinationNode::process(size_t numberOfFrames)
{
    m_source->consumeAudio(*input(0)->bus(), numberOfFrames);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)
