/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#if ENABLE(MEDIA_STREAM)

#include "MediaStreamTrackEvent.h"

#include "MediaStreamTrack.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaStreamTrackEvent);

Ref<MediaStreamTrackEvent> MediaStreamTrackEvent::create(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, RefPtr<MediaStreamTrack>&& track)
{
    return adoptRef(*new MediaStreamTrackEvent(type, canBubble, cancelable, WTFMove(track)));
}

Ref<MediaStreamTrackEvent> MediaStreamTrackEvent::create(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new MediaStreamTrackEvent(type, initializer, isTrusted));
}

MediaStreamTrackEvent::MediaStreamTrackEvent(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, RefPtr<MediaStreamTrack>&& track)
    : Event(type, canBubble, cancelable)
    , m_track(WTFMove(track))
{
}

MediaStreamTrackEvent::MediaStreamTrackEvent(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
    , m_track(initializer.track)
{
}

MediaStreamTrackEvent::~MediaStreamTrackEvent() = default;

MediaStreamTrack* MediaStreamTrackEvent::track() const
{
    return m_track.get();
}

EventInterface MediaStreamTrackEvent::eventInterface() const
{
    return MediaStreamTrackEventInterfaceType;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

