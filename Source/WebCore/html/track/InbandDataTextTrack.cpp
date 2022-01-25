/*
 * Copyright (C) 2014 Cable Television Labs Inc.  All rights reserved.
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "DataCue.h"
#include "InbandTextTrackPrivate.h"
#include "TextTrackList.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(InbandDataTextTrack);

inline InbandDataTextTrack::InbandDataTextTrack(Document& document, InbandTextTrackPrivate& trackPrivate)
    : InbandTextTrack(document, trackPrivate)
{
}

Ref<InbandDataTextTrack> InbandDataTextTrack::create(Document& document, InbandTextTrackPrivate& trackPrivate)
{
    auto textTrack = adoptRef(*new InbandDataTextTrack(document, trackPrivate));
    textTrack->suspendIfNeeded();
    return textTrack;
}

InbandDataTextTrack::~InbandDataTextTrack() = default;

void InbandDataTextTrack::addDataCue(const MediaTime& start, const MediaTime& end, const void* data, unsigned length)
{
    addCue(DataCue::create(document(), start, end, data, length));
}

#if ENABLE(DATACUE_VALUE)

void InbandDataTextTrack::addDataCue(const MediaTime& start, const MediaTime& end, Ref<SerializedPlatformDataCue>&& platformValue, const String& type)
{
    if (findIncompleteCue(platformValue))
        return;

    auto cue = DataCue::create(document(), start, end, platformValue.copyRef(), type);
    if (hasCue(cue, TextTrackCue::IgnoreDuration)) {
        INFO_LOG(LOGIDENTIFIER, "ignoring already added cue: ", cue.get());
        return;
    }

    auto* textTrackList = downcast<TextTrackList>(trackList());
    if (end.isPositiveInfinite() && textTrackList && textTrackList->duration().isValid()) {
        cue->setEndTime(textTrackList->duration());
        m_incompleteCueMap.append(&cue.get());
    }

    INFO_LOG(LOGIDENTIFIER, cue.get());

    addCue(WTFMove(cue));
}

RefPtr<DataCue> InbandDataTextTrack::findIncompleteCue(const SerializedPlatformDataCue& cueToFind)
{
    auto index = m_incompleteCueMap.findIf([&](const auto& cue) {
        return cueToFind.isEqual(*cue->platformValue());
    });

    if (index == notFound)
        return nullptr;

    return m_incompleteCueMap[index];
}

void InbandDataTextTrack::updateDataCue(const MediaTime& start, const MediaTime& inEnd, SerializedPlatformDataCue& platformValue)
{
    auto cue = findIncompleteCue(platformValue);
    if (!cue)
        return;

    cue->willChange();

    MediaTime end = inEnd;
    auto* textTrackList = downcast<TextTrackList>(trackList());
    if (end.isPositiveInfinite() && textTrackList && textTrackList->duration().isValid())
        end = textTrackList->duration();
    else
        m_incompleteCueMap.removeFirst(cue);

    INFO_LOG(LOGIDENTIFIER, "was start = ", cue->startMediaTime(), ", end = ", cue->endMediaTime(), ", will be start = ", start, ", end = ", end);

    cue->setStartTime(start);
    cue->setEndTime(end);

    cue->didChange();
}

void InbandDataTextTrack::removeDataCue(const MediaTime&, const MediaTime&, SerializedPlatformDataCue& platformValue)
{
    if (auto cue = findIncompleteCue(platformValue)) {
        INFO_LOG(LOGIDENTIFIER, "removing: ", *cue);
        m_incompleteCueMap.removeFirst(cue);
        InbandTextTrack::removeCue(*cue);
    }
}

ExceptionOr<void> InbandDataTextTrack::removeCue(TextTrackCue& cue)
{
    ASSERT(cue.cueType() == TextTrackCue::Data);

    if (auto platformValue = const_cast<SerializedPlatformDataCue*>(downcast<DataCue>(cue).platformValue()))
        removeDataCue({ }, { }, *platformValue);

    return InbandTextTrack::removeCue(cue);
}

#endif

} // namespace WebCore

#endif
