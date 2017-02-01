/*
 * Copyright (C) 2014 Cable Television Labs Inc.  All rights reserved.
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InbandDataTextTrack.h"

#if ENABLE(VIDEO_TRACK)

#include "DataCue.h"
#include "HTMLMediaElement.h"
#include "InbandTextTrackPrivate.h"
#include "Logging.h"

namespace WebCore {

inline InbandDataTextTrack::InbandDataTextTrack(ScriptExecutionContext& context, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
    : InbandTextTrack(context, client, trackPrivate)
{
}

Ref<InbandDataTextTrack> InbandDataTextTrack::create(ScriptExecutionContext& context, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
{
    return adoptRef(*new InbandDataTextTrack(context, client, trackPrivate));
}

InbandDataTextTrack::~InbandDataTextTrack()
{
}

void InbandDataTextTrack::addDataCue(const MediaTime& start, const MediaTime& end, const void* data, unsigned length)
{
    addCue(DataCue::create(*scriptExecutionContext(), start, end, data, length));
}

#if ENABLE(DATACUE_VALUE)

void InbandDataTextTrack::addDataCue(const MediaTime& start, const MediaTime& end, Ref<SerializedPlatformRepresentation>&& platformValue, const String& type)
{
    if (m_incompleteCueMap.contains(platformValue.ptr()))
        return;

    auto cue = DataCue::create(*scriptExecutionContext(), start, end, platformValue.copyRef(), type);
    if (hasCue(cue.ptr(), TextTrackCue::IgnoreDuration)) {
        LOG(Media, "InbandDataTextTrack::addDataCue ignoring already added cue: start=%s, end=%s\n", toString(cue->startTime()).utf8().data(), toString(cue->endTime()).utf8().data());
        return;
    }

    if (end.isPositiveInfinite() && mediaElement()) {
        cue->setEndTime(mediaElement()->durationMediaTime());
        m_incompleteCueMap.add(WTFMove(platformValue), cue.copyRef());
    }

    addCue(WTFMove(cue));
}

void InbandDataTextTrack::updateDataCue(const MediaTime& start, const MediaTime& inEnd, SerializedPlatformRepresentation& platformValue)
{
    RefPtr<DataCue> cue = m_incompleteCueMap.get(&platformValue);
    if (!cue)
        return;

    cue->willChange();

    MediaTime end = inEnd;
    if (end.isPositiveInfinite() && mediaElement())
        end = mediaElement()->durationMediaTime();
    else
        m_incompleteCueMap.remove(&platformValue);

    LOG(Media, "InbandDataTextTrack::updateDataCue: was start=%s, end=%s, will be start=%s, end=%s\n", toString(cue->startTime()).utf8().data(), toString(cue->endTime()).utf8().data(), toString(start).utf8().data(), toString(end).utf8().data());

    cue->setStartTime(start);
    cue->setEndTime(end);

    cue->didChange();
}

void InbandDataTextTrack::removeDataCue(const MediaTime&, const MediaTime&, SerializedPlatformRepresentation& platformValue)
{
    if (auto cue = m_incompleteCueMap.take(&platformValue)) {
        LOG(Media, "InbandDataTextTrack::removeDataCue removing cue: start=%s, end=%s\n", toString(cue->startTime()).utf8().data(), toString(cue->endTime()).utf8().data());
        InbandTextTrack::removeCue(*cue);
    }
}

ExceptionOr<void> InbandDataTextTrack::removeCue(TextTrackCue& cue)
{
    ASSERT(cue.cueType() == TextTrackCue::Data);

    m_incompleteCueMap.remove(const_cast<SerializedPlatformRepresentation*>(toDataCue(&cue)->platformValue()));

    return InbandTextTrack::removeCue(cue);
}

#endif

} // namespace WebCore

#endif
