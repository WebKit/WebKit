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

#if ENABLE(VIDEO)

#include "TextTrackList.h"

#include "HTMLMediaElement.h"
#include "InbandTextTrack.h"
#include "InbandTextTrackPrivate.h"
#include "LoadableTextTrack.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(TextTrackList);

TextTrackList::TextTrackList(WeakPtr<HTMLMediaElement> element, ScriptExecutionContext* context)
    : TrackListBase(element, context)
{
}

TextTrackList::~TextTrackList()
{
    clearElement();
}

void TextTrackList::clearElement()
{
    TrackListBase::clearElement();
    for (auto& track : m_elementTracks) {
        track->setMediaElement(nullptr);
        track->clearClient();
    }
    for (auto& track : m_addTrackTracks) {
        track->setMediaElement(nullptr);
        track->clearClient();
    }
}

unsigned TextTrackList::length() const
{
    return m_addTrackTracks.size() + m_elementTracks.size() + m_inbandTracks.size();
}

int TextTrackList::getTrackIndex(TextTrack& textTrack)
{
    if (is<LoadableTextTrack>(textTrack))
        return downcast<LoadableTextTrack>(textTrack).trackElementIndex();

    if (textTrack.trackType() == TextTrack::AddTrack)
        return m_elementTracks.size() + m_addTrackTracks.find(&textTrack);

    if (textTrack.trackType() == TextTrack::InBand)
        return m_elementTracks.size() + m_addTrackTracks.size() + m_inbandTracks.find(&textTrack);

    ASSERT_NOT_REACHED();

    return -1;
}

int TextTrackList::getTrackIndexRelativeToRenderedTracks(TextTrack& textTrack)
{
    // Calculate the "Let n be the number of text tracks whose text track mode is showing and that are in the media element's list of text tracks before track."
    int trackIndex = 0;

    for (auto& elementTrack : m_elementTracks) {
        if (!downcast<TextTrack>(*elementTrack).isRendered())
            continue;
        if (elementTrack == &textTrack)
            return trackIndex;
        ++trackIndex;
    }

    for (auto& addTrack : m_addTrackTracks) {
        if (!downcast<TextTrack>(*addTrack).isRendered())
            continue;
        if (addTrack == &textTrack)
            return trackIndex;
        ++trackIndex;
    }

    for (auto& inbandTrack : m_inbandTracks) {
        if (!downcast<TextTrack>(*inbandTrack).isRendered())
            continue;
        if (inbandTrack == &textTrack)
            return trackIndex;
        ++trackIndex;
    }
    ASSERT_NOT_REACHED();
    return -1;
}

TextTrack* TextTrackList::item(unsigned index) const
{
    // 4.8.10.12.1 Text track model
    // The text tracks are sorted as follows:
    // 1. The text tracks corresponding to track element children of the media element, in tree order.
    // 2. Any text tracks added using the addTextTrack() method, in the order they were added, oldest first.
    // 3. Any media-resource-specific text tracks (text tracks corresponding to data in the media
    // resource), in the order defined by the media resource's format specification.

    if (index < m_elementTracks.size())
        return downcast<TextTrack>(m_elementTracks[index].get());

    index -= m_elementTracks.size();
    if (index < m_addTrackTracks.size())
        return downcast<TextTrack>(m_addTrackTracks[index].get());

    index -= m_addTrackTracks.size();
    if (index < m_inbandTracks.size())
        return downcast<TextTrack>(m_inbandTracks[index].get());

    return nullptr;
}

TextTrack* TextTrackList::getTrackById(const AtomString& id)
{
    // 4.8.10.12.5 Text track API
    // The getTrackById(id) method must return the first TextTrack in the
    // TextTrackList object whose id IDL attribute would return a value equal
    // to the value of the id argument.
    for (unsigned i = 0; i < length(); ++i) {
        auto& track = *item(i);
        if (track.id() == id)
            return &track;
    }

    // When no tracks match the given argument, the method must return null.
    return nullptr;
}

void TextTrackList::invalidateTrackIndexesAfterTrack(TextTrack& track)
{
    Vector<RefPtr<TrackBase>>* tracks = nullptr;

    switch (track.trackType()) {
    case TextTrack::TrackElement:
        tracks = &m_elementTracks;
        for (auto& addTrack : m_addTrackTracks)
            downcast<TextTrack>(addTrack.get())->invalidateTrackIndex();
        for (auto& inbandTrack : m_inbandTracks)
            downcast<TextTrack>(inbandTrack.get())->invalidateTrackIndex();
        break;
    case TextTrack::AddTrack:
        tracks = &m_addTrackTracks;
        for (auto& inbandTrack : m_inbandTracks)
            downcast<TextTrack>(inbandTrack.get())->invalidateTrackIndex();
        break;
    case TextTrack::InBand:
        tracks = &m_inbandTracks;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    size_t index = tracks->find(&track);
    if (index == notFound)
        return;

    for (size_t i = index; i < tracks->size(); ++i)
        downcast<TextTrack>(*tracks->at(index)).invalidateTrackIndex();
}

void TextTrackList::append(Ref<TextTrack>&& track)
{
    if (track->trackType() == TextTrack::AddTrack)
        m_addTrackTracks.append(track.ptr());
    else if (is<LoadableTextTrack>(track)) {
        // Insert tracks added for <track> element in tree order.
        size_t index = downcast<LoadableTextTrack>(track.get()).trackElementIndex();
        m_elementTracks.insert(index, track.ptr());
    } else if (track->trackType() == TextTrack::InBand) {
        // Insert tracks added for in-band in the media file order.
        size_t index = downcast<InbandTextTrack>(track.get()).inbandTrackIndex();
        m_inbandTracks.insert(index, track.ptr());
    } else
        ASSERT_NOT_REACHED();

    invalidateTrackIndexesAfterTrack(track);

    ASSERT(!track->mediaElement() || track->mediaElement() == mediaElement());
    track->setMediaElement(mediaElement());

    scheduleAddTrackEvent(WTFMove(track));
}

void TextTrackList::remove(TrackBase& track, bool scheduleEvent)
{
    auto& textTrack = downcast<TextTrack>(track);
    Vector<RefPtr<TrackBase>>* tracks = nullptr;
    switch (textTrack.trackType()) {
    case TextTrack::TrackElement:
        tracks = &m_elementTracks;
        break;
    case TextTrack::AddTrack:
        tracks = &m_addTrackTracks;
        break;
    case TextTrack::InBand:
        tracks = &m_inbandTracks;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    size_t index = tracks->find(&track);
    if (index == notFound)
        return;

    invalidateTrackIndexesAfterTrack(textTrack);

    ASSERT(!track.mediaElement() || !element() || track.mediaElement() == element());
    track.setMediaElement(nullptr);

    Ref<TrackBase> trackRef = *(*tracks)[index];
    tracks->remove(index);

    if (scheduleEvent)
        scheduleRemoveTrackEvent(WTFMove(trackRef));
}

bool TextTrackList::contains(TrackBase& track) const
{
    const Vector<RefPtr<TrackBase>>* tracks = nullptr;
    switch (downcast<TextTrack>(track).trackType()) {
    case TextTrack::TrackElement:
        tracks = &m_elementTracks;
        break;
    case TextTrack::AddTrack:
        tracks = &m_addTrackTracks;
        break;
    case TextTrack::InBand:
        tracks = &m_inbandTracks;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    
    return tracks->find(&track) != notFound;
}

EventTargetInterface TextTrackList::eventTargetInterface() const
{
    return TextTrackListEventTargetInterfaceType;
}

const char* TextTrackList::activeDOMObjectName() const
{
    return "TextTrackList";
}

} // namespace WebCore
#endif
