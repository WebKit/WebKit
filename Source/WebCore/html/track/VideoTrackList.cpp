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

#include "config.h"

#if ENABLE(VIDEO_TRACK)

#include "VideoTrackList.h"

#include "HTMLMediaElement.h"
#include "VideoTrack.h"

namespace WebCore {

VideoTrackList::VideoTrackList(WeakPtr<HTMLMediaElement> element, ScriptExecutionContext* context)
    : TrackListBase(element, context)
{
}

VideoTrackList::~VideoTrackList() = default;

void VideoTrackList::append(Ref<VideoTrack>&& track)
{
    // Insert tracks in the media file order.
    size_t index = track->inbandTrackIndex();
    size_t insertionIndex;
    for (insertionIndex = 0; insertionIndex < m_inbandTracks.size(); ++insertionIndex) {
        auto& otherTrack = downcast<VideoTrack>(*m_inbandTracks[insertionIndex]);
        if (otherTrack.inbandTrackIndex() > index)
            break;
    }
    m_inbandTracks.insert(insertionIndex, track.ptr());

    ASSERT(!track->mediaElement() || track->mediaElement() == mediaElement());
    track->setMediaElement(mediaElement());

    scheduleAddTrackEvent(WTFMove(track));
}

VideoTrack* VideoTrackList::item(unsigned index) const
{
    if (index < m_inbandTracks.size())
        return downcast<VideoTrack>(m_inbandTracks[index].get());
    return nullptr;
}

VideoTrack* VideoTrackList::getTrackById(const AtomString& id) const
{
    for (auto& inbandTracks : m_inbandTracks) {
        auto& track = downcast<VideoTrack>(*inbandTracks);
        if (track.id() == id)
            return &track;
    }
    return nullptr;
}

int VideoTrackList::selectedIndex() const
{
    // 4.8.10.10.1 AudioTrackList and VideoTrackList objects
    // The VideoTrackList.selectedIndex attribute must return the index of the
    // currently selected track, if any. If the VideoTrackList object does not
    // currently represent any tracks, or if none of the tracks are selected,
    // it must instead return −1.
    for (unsigned i = 0; i < length(); ++i) {
        if (downcast<VideoTrack>(*m_inbandTracks[i]).selected())
            return i;
    }
    return -1;
}

EventTargetInterface VideoTrackList::eventTargetInterface() const
{
    return VideoTrackListEventTargetInterfaceType;
}

} // namespace WebCore
#endif
