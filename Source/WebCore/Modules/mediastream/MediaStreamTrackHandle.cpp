/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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
#include "MediaStreamTrackHandle.h"

#if ENABLE(MEDIA_STREAM)

#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakPtrImpl.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaStreamTrackHandle::DataHolder);

ExceptionOr<Ref<MediaStreamTrackHandle>> MediaStreamTrackHandle::create(MediaStreamTrack& track)
{
    RefPtr context = track.scriptExecutionContext();
    if (!context)
        return Exception { ExceptionCode::InvalidStateError, "Track context is gone"_s };

    return create(context->identifier(), track, track.keeper());
}

Ref<MediaStreamTrackHandle> MediaStreamTrackHandle::create(DataHolder&& holder)
{
    return create(holder.contextIdentifier, WTF::move(holder.track), WTF::move(holder.trackKeeper));
}

Ref<MediaStreamTrackHandle> MediaStreamTrackHandle::create(ScriptExecutionContextIdentifier contextIdentifier, WeakPtr<MediaStreamTrack, WeakPtrImplWithEventTargetData>&& track, Ref<MediaStreamTrack::Keeper>&& trackKeeper)
{
    return adoptRef(*new MediaStreamTrackHandle(contextIdentifier, WTF::move(track), WTF::move(trackKeeper)));
}

MediaStreamTrackHandle::MediaStreamTrackHandle(ScriptExecutionContextIdentifier contextIdentifier, WeakPtr<MediaStreamTrack, WeakPtrImplWithEventTargetData>&& track, Ref<MediaStreamTrack::Keeper>&& trackKeeper)
    : m_contextIdentifier(contextIdentifier)
    , m_track(WTF::move(track))
    , m_trackKeeper(WTF::move(trackKeeper))
{
}

MediaStreamTrackHandle::~MediaStreamTrackHandle() = default;

UniqueRef<MediaStreamTrackHandle::DataHolder> MediaStreamTrackHandle::detach()
{
    ASSERT(!isDetached());
    m_isDetached = true;
    return makeUniqueRef<DataHolder>(DataHolder { m_contextIdentifier, m_track, m_trackKeeper });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

