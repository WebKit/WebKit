/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
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

#include "MediaStreamTrackList.h"

#include "ExceptionCode.h"
#include "MediaStream.h"
#include "MediaStreamCenter.h"
#include "MediaStreamTrackEvent.h"

namespace WebCore {

PassRefPtr<MediaStreamTrackList> MediaStreamTrackList::create(MediaStream* owner, const MediaStreamTrackVector& trackVector)
{
    ASSERT(owner);
    RefPtr<MediaStreamTrackList> trackList = adoptRef(new MediaStreamTrackList(owner, trackVector));
    trackList->suspendIfNeeded();
    return trackList.release();
}

MediaStreamTrackList::MediaStreamTrackList(MediaStream* owner, const MediaStreamTrackVector& trackVector)
    : ActiveDOMObject(owner->scriptExecutionContext(), this)
    , m_owner(owner)
    , m_trackVector(trackVector)
{
}

MediaStreamTrackList::~MediaStreamTrackList()
{
}

void MediaStreamTrackList::detachOwner()
{
    m_owner = 0;
}

unsigned MediaStreamTrackList::length() const
{
    return m_trackVector.size();
}

MediaStreamTrack* MediaStreamTrackList::item(unsigned index) const
{
    if (index >= m_trackVector.size())
        return 0;
    return m_trackVector[index].get();
}

void MediaStreamTrackList::add(PassRefPtr<MediaStreamTrack> prpTrack, ExceptionCode& ec)
{
    if (!m_owner || m_owner->ended()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    RefPtr<MediaStreamTrack> track = prpTrack;
    if (!track) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    if (m_trackVector.contains(track))
        return;

    m_trackVector.append(track);
    if (!MediaStreamCenter::instance().didAddMediaStreamTrack(m_owner->descriptor(), track->component())) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    dispatchEvent(MediaStreamTrackEvent::create(eventNames().addtrackEvent, false, false, track));
}

void MediaStreamTrackList::remove(PassRefPtr<MediaStreamTrack> prpTrack, ExceptionCode& ec)
{
    if (!m_owner || m_owner->ended()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    RefPtr<MediaStreamTrack> track = prpTrack;
    if (!track) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    size_t index = m_trackVector.find(track);
    if (index == notFound)
        return;

    m_trackVector.remove(index);
    if (!MediaStreamCenter::instance().didRemoveMediaStreamTrack(m_owner->descriptor(), track->component())) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    dispatchEvent(MediaStreamTrackEvent::create(eventNames().removetrackEvent, false, false, track));
}

void MediaStreamTrackList::remove(MediaStreamComponent* component)
{
    if (!m_owner || m_owner->ended())
        return;

    size_t index = notFound;
    for (unsigned i = 0; i < m_trackVector.size(); ++i) {
        if (m_trackVector[i]->component() == component) {
            index = i;
            break;
        }
    }

    if (index == notFound)
        return;

    RefPtr<MediaStreamTrack> track = m_trackVector[index];
    m_trackVector.remove(index);
    MediaStreamCenter::instance().didRemoveMediaStreamTrack(m_owner->descriptor(), component);
    dispatchEvent(MediaStreamTrackEvent::create(eventNames().removetrackEvent, false, false, track));
}

void MediaStreamTrackList::stop()
{
    detachOwner();
}

const AtomicString& MediaStreamTrackList::interfaceName() const
{
    return eventNames().interfaceForMediaStreamTrackList;
}

ScriptExecutionContext* MediaStreamTrackList::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

EventTargetData* MediaStreamTrackList::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* MediaStreamTrackList::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
