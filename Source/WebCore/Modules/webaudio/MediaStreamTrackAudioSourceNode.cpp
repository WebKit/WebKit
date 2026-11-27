/*
 * Copyright (C) 2026 Sergey Rubanov. All rights reserved.
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

#if ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)
#include "MediaStreamTrackAudioSourceNode.h"

#include "AudioContext.h"
#include "MediaStreamTrackAudioSourceOptions.h"
#include "WebAudioSourceProvider.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaStreamTrackAudioSourceNode);

ExceptionOr<Ref<MediaStreamTrackAudioSourceNode>> MediaStreamTrackAudioSourceNode::create(BaseAudioContext& context, MediaStreamTrackAudioSourceOptions&& options)
{
    Ref track = WTF::move(options.mediaStreamTrack);
    if (track->kind() != "audio"_s)
        return Exception { ExceptionCode::InvalidStateError, "MediaStreamTrack is not an audio track"_s };

    RefPtr provider = track->createAudioSourceProvider();
    if (!provider)
        return Exception { ExceptionCode::InvalidStateError, "Could not create an audio source provider for the track"_s };

    Ref node = adoptRef(*new MediaStreamTrackAudioSourceNode(context, WTF::move(track), provider.releaseNonNull()));
    node->preparePlayback();
    return node;
}

MediaStreamTrackAudioSourceNode::MediaStreamTrackAudioSourceNode(BaseAudioContext& context, Ref<MediaStreamTrack>&& track, Ref<WebAudioSourceProvider>&& provider)
    : MediaStreamAudioSourceNodeBase(context, NodeTypeMediaStreamTrackAudioSource, WTF::move(provider))
    , m_track(WTF::move(track))
{
}

MediaStreamTrackAudioSourceNode::~MediaStreamTrackAudioSourceNode()
{
    detachProvider();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)
