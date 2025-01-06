/*
 * Copyright (C) 2011, 2012 Apple Inc.  All rights reserved.
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

#if ENABLE(VIDEO)

#include "TrackListBase.h"
#include <wtf/MediaTime.h>

namespace WebCore {

class TextTrack;

class TextTrackList final : public TrackListBase {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(TextTrackList);
public:
    static Ref<TextTrackList> create(ScriptExecutionContext* context)
    {
        auto list = adoptRef(*new TextTrackList(context));
        list->suspendIfNeeded();
        return list;
    }
    virtual ~TextTrackList();

    bool isSupportedPropertyIndex(unsigned index) const { return index < length(); }
    unsigned length() const override;
    int getTrackIndex(TextTrack&);
    int getTrackIndexRelativeToRenderedTracks(TextTrack&);
    bool contains(TrackBase&) const override;

    TextTrack* item(unsigned index) const;
    TextTrack* getTrackById(const AtomString&) const;
    TextTrack* getTrackById(TrackID) const;
    TextTrack* lastItem() const { return item(length() - 1); }

    void append(Ref<TextTrack>&&);
    void remove(TrackBase&, bool scheduleEvent = true) override;

    void setDuration(MediaTime duration) { m_duration = duration; }
    const MediaTime& duration() const { return m_duration; }

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const override;

private:
    TextTrackList(ScriptExecutionContext*);

    void invalidateTrackIndexesAfterTrack(TextTrack&);

    Vector<RefPtr<TrackBase>> m_addTrackTracks;
    Vector<RefPtr<TrackBase>> m_elementTracks;
    MediaTime m_duration;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::TextTrackList)
    static bool isType(const WebCore::TrackListBase& trackList) { return trackList.type() == WebCore::TrackListBase::TextTrackList; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO)
