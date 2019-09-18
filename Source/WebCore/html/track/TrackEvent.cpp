/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(VIDEO_TRACK)

#include "TrackEvent.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(TrackEvent);

static inline Optional<TrackEvent::TrackEventTrack> convertToTrackEventTrack(Ref<TrackBase>&& track)
{
    switch (track->type()) {
    case TrackBase::BaseTrack:
        return WTF::nullopt;
    case TrackBase::TextTrack:
        return TrackEvent::TrackEventTrack { RefPtr<TextTrack>(&downcast<TextTrack>(track.get())) };
    case TrackBase::AudioTrack:
        return TrackEvent::TrackEventTrack { RefPtr<AudioTrack>(&downcast<AudioTrack>(track.get())) };
    case TrackBase::VideoTrack:
        return TrackEvent::TrackEventTrack { RefPtr<VideoTrack>(&downcast<VideoTrack>(track.get())) };
    }
    
    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

TrackEvent::TrackEvent(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, Ref<TrackBase>&& track)
    : Event(type, canBubble, cancelable)
    , m_track(convertToTrackEventTrack(WTFMove(track)))
{
}

TrackEvent::TrackEvent(const AtomString& type, Init&& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
    , m_track(WTFMove(initializer.track))
{
}

TrackEvent::~TrackEvent() = default;

EventInterface TrackEvent::eventInterface() const
{
    return TrackEventInterfaceType;
}

} // namespace WebCore

#endif
