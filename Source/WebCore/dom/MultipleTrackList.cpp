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
#include "MultipleTrackList.h"

#if ENABLE(MEDIA_STREAM) || ENABLE(VIDEO_TRACK)

#include "Event.h"
#include "EventNames.h"

namespace WebCore {

PassRefPtr<MultipleTrackList> MultipleTrackList::create(const TrackVector& tracks, const EnabledTracks& enabledTracks)
{
    return adoptRef(new MultipleTrackList(tracks, enabledTracks));
}

MultipleTrackList::MultipleTrackList(const TrackVector& tracks, const EnabledTracks& enabledTracks)
    : TrackList(tracks)
    , m_isEnabled(enabledTracks)
{
    ASSERT(m_isEnabled.size() == length());
}

MultipleTrackList::~MultipleTrackList()
{
}

void MultipleTrackList::clear()
{
    m_isEnabled.clear();
    TrackList::clear();
}

bool MultipleTrackList::isEnabled(unsigned long index, ExceptionCode& ec) const
{
    return checkIndex(index, ec) ? m_isEnabled[index] : false;
}

void MultipleTrackList::enable(unsigned long index, ExceptionCode& ec)
{
    if (!checkIndex(index, ec))
        return;

    m_isEnabled[index] = true;
    postChangeEvent();
}

void MultipleTrackList::disable(unsigned long index, ExceptionCode& ec)
{
    if (!checkIndex(index, ec))
        return;

    m_isEnabled[index] = false;
    postChangeEvent();
}

MultipleTrackList* MultipleTrackList::toMultipleTrackList()
{
    return this;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) || ENABLE(VIDEO_TRACK)
