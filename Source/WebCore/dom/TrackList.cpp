/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "TrackList.h"

#if ENABLE(MEDIA_STREAM) || ENABLE(VIDEO_TRACK)

#include "Event.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

PassRefPtr<Track> Track::create(const String& id, const AtomicString& kind, const String& label, const String& language)
{
    return adoptRef(new Track(id, kind, label, language));
}

Track::Track(const String& id, const AtomicString& kind, const String& label, const String& language)
    : m_id(id)
    , m_kind(kind)
    , m_label(label)
    , m_language(language)
{
}

PassRefPtr<TrackList> TrackList::create(const TrackVector& tracks)
{
    return adoptRef(new TrackList(tracks));
}

TrackList::TrackList(const TrackVector& tracks)
    : m_tracks(tracks)
{
}

TrackList::~TrackList()
{
}

unsigned long TrackList::length() const
{
    return m_tracks.size();
}

bool TrackList::checkIndex(long index, ExceptionCode& ec, long allowed) const
{
    if (index >= static_cast<long>(length()) || (index < 0 && index != allowed)) {
        ec = INDEX_SIZE_ERR;
        return false;
    }

    ec = 0;
    return true;
}

const String& TrackList::getID(unsigned long index, ExceptionCode& ec) const
{
    DEFINE_STATIC_LOCAL(String, nullString, ());
    return checkIndex(index, ec) ? m_tracks[index]->id() : nullString;
}

const AtomicString& TrackList::getKind(unsigned long index, ExceptionCode& ec) const
{
    DEFINE_STATIC_LOCAL(AtomicString, nullString, ());
    return checkIndex(index, ec) ? m_tracks[index]->kind() : nullString;
}

const String& TrackList::getLabel(unsigned long index, ExceptionCode& ec) const
{
    DEFINE_STATIC_LOCAL(String, nullString, ());
    return checkIndex(index, ec) ? m_tracks[index]->label() : nullString;
}

const String& TrackList::getLanguage(unsigned long index, ExceptionCode& ec) const
{
    DEFINE_STATIC_LOCAL(String, nullString, ());
    return checkIndex(index, ec) ? m_tracks[index]->language() : nullString;
}

void TrackList::clear()
{
#if ENABLE(MEDIA_STREAM)
    m_associatedStreamLabel = String();
#endif

    if (m_tracks.isEmpty())
        return;

    m_tracks.clear();

    postChangeEvent();
}

void TrackList::postChangeEvent()
{
    // Dispatch a change event asynchronously. Don't assert since this may be called during shutdown,
    // and the ScriptExecutionContext may be not available in the Media Stream case.
    if (!scriptExecutionContext())
        return;

    ASSERT(scriptExecutionContext()->isContextThread());
    scriptExecutionContext()->postTask(DispatchTask<TrackList, int>::create(this, &TrackList::dispatchChangeEvent, 0));
}

void TrackList::dispatchChangeEvent(int)
{
    dispatchEvent(Event::create(eventNames().changeEvent, false, false));
}

#if ENABLE(MEDIA_STREAM)
void TrackList::associateStream(const String& label)
{
    ASSERT(m_associatedStreamLabel.isNull());
    m_associatedStreamLabel = label;
}

const String& TrackList::associatedStreamLabel() const
{
    return m_associatedStreamLabel;
}

void TrackList::trackFailed(unsigned long index)
{
    ASSERT(index < length());
    m_tracks.remove(index);
    postChangeEvent();
}
#endif

TrackList* TrackList::toTrackList()
{
    return this;
}

ScriptExecutionContext* TrackList::scriptExecutionContext() const
{
#if ENABLE(MEDIA_STREAM)
    if (mediaStreamFrameController())
        return mediaStreamFrameController()->scriptExecutionContext();
#endif

    // FIXME: provide an script execution context for HTML Media Element use case.
    // https://bugs.webkit.org/show_bug.cgi?id=61127
    return 0;
}

EventTargetData* TrackList::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* TrackList::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) || ENABLE(VIDEO_TRACK)
