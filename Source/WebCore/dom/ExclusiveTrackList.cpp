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
#include "ExclusiveTrackList.h"

#if ENABLE(MEDIA_STREAM) || ENABLE(VIDEO_TRACK)

#include "Event.h"
#include "EventNames.h"

namespace WebCore {

PassRefPtr<ExclusiveTrackList> ExclusiveTrackList::create(const TrackVector& tracks, long selectedIndex)
{
    return adoptRef(new ExclusiveTrackList(tracks, selectedIndex));
}

ExclusiveTrackList::ExclusiveTrackList(const TrackVector& tracks, long selectedIndex)
    : TrackList(tracks)
    , m_selectedIndex(selectedIndex)
{
    ASSERT(m_selectedIndex >= NoSelection && m_selectedIndex < static_cast<long>(length()));
}

ExclusiveTrackList::~ExclusiveTrackList()
{
}

void ExclusiveTrackList::clear()
{
    m_selectedIndex = NoSelection;
    TrackList::clear();
}

void ExclusiveTrackList::select(long index, ExceptionCode& ec)
{
    if (!checkIndex(index, ec, NoSelection))
        return;

    m_selectedIndex = index;
    postChangeEvent();
}

ExclusiveTrackList* ExclusiveTrackList::toExclusiveTrackList()
{
    return this;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) || ENABLE(VIDEO_TRACK)
