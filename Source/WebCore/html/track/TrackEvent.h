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

#pragma once

#if ENABLE(VIDEO_TRACK)

#include "AudioTrack.h"
#include "Event.h"
#include "TextTrack.h"
#include "VideoTrack.h"

namespace WebCore {

class TrackEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(TrackEvent);
public:
    virtual ~TrackEvent();

    static Ref<TrackEvent> create(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, Ref<TrackBase>&& track)
    {
        return adoptRef(*new TrackEvent(type, canBubble, cancelable, WTFMove(track)));
    }

    using TrackEventTrack = Variant<RefPtr<VideoTrack>, RefPtr<AudioTrack>, RefPtr<TextTrack>>;

    struct Init : public EventInit {
        Optional<TrackEventTrack> track;
    };

    static Ref<TrackEvent> create(const AtomString& type, Init&& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new TrackEvent(type, WTFMove(initializer), isTrusted));
    }

    Optional<TrackEventTrack> track() const { return m_track; }

private:
    TrackEvent(const AtomString& type, CanBubble, IsCancelable, Ref<TrackBase>&&);
    TrackEvent(const AtomString& type, Init&& initializer, IsTrusted);

    EventInterface eventInterface() const override;

    Optional<TrackEventTrack> m_track;
};

} // namespace WebCore

#endif // ENABLE(VIDEO_TRACK)
