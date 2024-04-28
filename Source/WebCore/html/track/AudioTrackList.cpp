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
#include "AudioTrackList.h"

#if ENABLE(VIDEO)

#include "AudioTrack.h"
#include "ContextDestructionObserverInlines.h"

namespace WebCore {

AudioTrackList::AudioTrackList(ScriptExecutionContext* context)
    : TrackListBase(context, TrackListBase::VideoTrackList)
{
}

AudioTrackList::~AudioTrackList() = default;

void AudioTrackList::append(Ref<AudioTrack>&& track)
{
    // Insert tracks in the media file order.
    size_t index = track->inbandTrackIndex();
    size_t insertionIndex;
    for (insertionIndex = 0; insertionIndex < m_inbandTracks.size(); ++insertionIndex) {
        auto& otherTrack = downcast<AudioTrack>(*m_inbandTracks[insertionIndex]);
        if (otherTrack.inbandTrackIndex() > index)
            break;
    }
    m_inbandTracks.insert(insertionIndex, track.ptr());

    if (!track->trackList())
        track->setTrackList(*this);

    scheduleAddTrackEvent(WTFMove(track));
}

void AudioTrackList::remove(TrackBase& track, bool scheduleEvent)
{
    auto& audioTrack = downcast<AudioTrack>(track);
    if (audioTrack.trackList() == this)
        audioTrack.clearTrackList();

    TrackListBase::remove(track, scheduleEvent);
}

AudioTrack* AudioTrackList::item(unsigned index) const
{
    if (index < m_inbandTracks.size())
        return downcast<AudioTrack>(m_inbandTracks[index].get());
    return nullptr;
}

AudioTrack* AudioTrackList::firstEnabled() const
{
    for (auto& item : m_inbandTracks) {
        if (item && item->enabled())
            return downcast<AudioTrack>(item.get());
    }
    return nullptr;
}

AudioTrack* AudioTrackList::getTrackById(const AtomString& id) const
{
    for (auto& inbandTrack : m_inbandTracks) {
        auto& track = downcast<AudioTrack>(*inbandTrack);
        if (track.id() == id)
            return &track;
    }
    return nullptr;
}

AudioTrack* AudioTrackList::getTrackById(TrackID id) const
{
    for (auto& inbandTrack : m_inbandTracks) {
        auto& track = downcast<AudioTrack>(*inbandTrack);
        if (track.trackId() == id)
            return &track;
    }
    return nullptr;
}

enum EventTargetInterfaceType AudioTrackList::eventTargetInterface() const
{
    return EventTargetInterfaceType::AudioTrackList;
}

} // namespace WebCore
#endif
