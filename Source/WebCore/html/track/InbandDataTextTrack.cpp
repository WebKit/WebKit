/*
 * Copyright (C) 2014 Cable Television Labs Inc.  All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)
#include "InbandDataTextTrack.h"

#include "DataCue.h"
#include "ExceptionCodePlaceholder.h"
#include "HTMLMediaElement.h"
#include "InbandTextTrackPrivate.h"
#include "Logging.h"
#include <runtime/ArrayBuffer.h>

namespace WebCore {

PassRefPtr<InbandDataTextTrack> InbandDataTextTrack::create(ScriptExecutionContext* context, TextTrackClient* client, PassRefPtr<InbandTextTrackPrivate> playerPrivate)
{
    return adoptRef(new InbandDataTextTrack(context, client, playerPrivate));
}

InbandDataTextTrack::InbandDataTextTrack(ScriptExecutionContext* context, TextTrackClient* client, PassRefPtr<InbandTextTrackPrivate> trackPrivate)
    : InbandTextTrack(context, client, trackPrivate)
{
}

InbandDataTextTrack::~InbandDataTextTrack()
{
}

void InbandDataTextTrack::addDataCue(InbandTextTrackPrivate*, const MediaTime& start, const MediaTime& end, const void* data, unsigned length)
{
    RefPtr<DataCue> cue = DataCue::create(*scriptExecutionContext(), start, end, data, length);
    addCue(cue.release(), ASSERT_NO_EXCEPTION);
}

#if ENABLE(DATACUE_VALUE)
void InbandDataTextTrack::addDataCue(InbandTextTrackPrivate*, const MediaTime& start, const MediaTime& end, PassRefPtr<SerializedPlatformRepresentation> prpPlatformValue, const String& type)
{
    RefPtr<SerializedPlatformRepresentation> platformValue = prpPlatformValue;
    if (m_incompleteCueMap.find(platformValue.get()) != m_incompleteCueMap.end())
        return;

    RefPtr<DataCue> cue = DataCue::create(*scriptExecutionContext(), start, end, platformValue, type);
    if (hasCue(cue.get(), TextTrackCue::IgnoreDuration)) {
        LOG(Media, "InbandDataTextTrack::addDataCue ignoring already added cue: start=%s, end=%s\n", toString(cue->startTime()).utf8().data(), toString(cue->endTime()).utf8().data());
        return;
    }

    if (end.isPositiveInfinite() && mediaElement()) {
        cue->setEndTime(mediaElement()->durationMediaTime());
        m_incompleteCueMap.add(platformValue, cue);
    }

    addCue(cue.release(), ASSERT_NO_EXCEPTION);
}

void InbandDataTextTrack::updateDataCue(InbandTextTrackPrivate*, const MediaTime& start, const MediaTime& inEnd, PassRefPtr<SerializedPlatformRepresentation> prpPlatformValue)
{
    MediaTime end = inEnd;

    RefPtr<SerializedPlatformRepresentation> platformValue = prpPlatformValue;
    auto iter = m_incompleteCueMap.find(platformValue.get());
    if (iter == m_incompleteCueMap.end())
        return;

    RefPtr<DataCue> cue = iter->value;
    if (!cue)
        return;

    cue->willChange();

    if (end.isPositiveInfinite() && mediaElement())
        end = mediaElement()->durationMediaTime();
    else
        m_incompleteCueMap.remove(platformValue.get());

    LOG(Media, "InbandDataTextTrack::updateDataCue: was start=%s, end=%s, will be start=%s, end=%s\n", toString(cue->startTime()).utf8().data(), toString(cue->endTime()).utf8().data(), toString(start).utf8().data(), toString(end).utf8().data());

    cue->setStartTime(start);
    cue->setEndTime(end);

    cue->didChange();
}

void InbandDataTextTrack::removeDataCue(InbandTextTrackPrivate*, const MediaTime&, const MediaTime&, PassRefPtr<SerializedPlatformRepresentation> prpPlatformValue)
{
    RefPtr<SerializedPlatformRepresentation> platformValue = prpPlatformValue;
    auto iter = m_incompleteCueMap.find(platformValue.get());
    if (iter == m_incompleteCueMap.end())
        return;

    RefPtr<DataCue> cue = iter->value;
    if (cue) {
        LOG(Media, "InbandDataTextTrack::removeDataCue removing cue: start=%s, end=%s\n", toString(cue->startTime()).utf8().data(), toString(cue->endTime()).utf8().data());
        removeCue(cue.get(), IGNORE_EXCEPTION);
    }
}

void InbandDataTextTrack::removeCue(TextTrackCue* cue, ExceptionCode& ec)
{
    ASSERT(cue->cueType() == TextTrackCue::Data);

    RefPtr<SerializedPlatformRepresentation> platformValue = toDataCue(cue)->platformValue().get();
    auto iter = m_incompleteCueMap.find(platformValue.get());
    if (iter != m_incompleteCueMap.end())
        m_incompleteCueMap.remove(platformValue.get());

    InbandTextTrack::removeCue(cue, ec);
}

#endif

} // namespace WebCore

#endif
