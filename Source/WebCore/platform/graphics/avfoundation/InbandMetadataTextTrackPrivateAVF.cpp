/*
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
#include "InbandMetadataTextTrackPrivateAVF.h"

#if ENABLE(VIDEO) && ENABLE(DATACUE_VALUE) && (USE(AVFOUNDATION) || PLATFORM(IOS_FAMILY))

#include "InbandTextTrackPrivateClient.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include <CoreMedia/CoreMedia.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

Ref<InbandMetadataTextTrackPrivateAVF> InbandMetadataTextTrackPrivateAVF::create(InbandTextTrackPrivate::Kind kind, InbandTextTrackPrivate::CueFormat cueFormat, const AtomString& id)
{
    return adoptRef(*new InbandMetadataTextTrackPrivateAVF(kind, cueFormat, id));
}

InbandMetadataTextTrackPrivateAVF::InbandMetadataTextTrackPrivateAVF(InbandTextTrackPrivate::Kind kind, InbandTextTrackPrivate::CueFormat cueFormat, const AtomString& id)
    : InbandTextTrackPrivate(cueFormat)
    , m_kind(kind)
    , m_id(id)
{
}

InbandMetadataTextTrackPrivateAVF::~InbandMetadataTextTrackPrivateAVF() = default;

#if ENABLE(DATACUE_VALUE)

void InbandMetadataTextTrackPrivateAVF::addDataCue(const MediaTime& start, const MediaTime& end, Ref<SerializedPlatformRepresentation>&& cueData, const String& type)
{
    ASSERT(cueFormat() == Data);
    ASSERT(start >= MediaTime::zeroTime());

    if (!client())
        return;

    m_currentCueStartTime = start;
    if (end.isPositiveInfinite())
        m_incompleteCues.append(IncompleteMetaDataCue { cueData.ptr(), start });
    client()->addDataCue(start, end, WTFMove(cueData), type);
}

void InbandMetadataTextTrackPrivateAVF::updatePendingCueEndTimes(const MediaTime& time)
{
    ASSERT(time >= MediaTime::zeroTime());

    if (time >= m_currentCueStartTime) {
        if (client()) {
            for (auto& partialCue : m_incompleteCues) {
                INFO_LOG(LOGIDENTIFIER, "updating cue: start = ", partialCue.startTime, ", end = ", time);
                client()->updateDataCue(partialCue.startTime, time, *partialCue.cueData);
            }
        }
    } else
        WARNING_LOG(LOGIDENTIFIER, "negative length cue(s) ignored: start = ", m_currentCueStartTime, ", end = ", time);

    m_incompleteCues.shrink(0);
    m_currentCueStartTime = MediaTime::zeroTime();
}

#endif

void InbandMetadataTextTrackPrivateAVF::flushPartialCues()
{
    if (m_currentCueStartTime && m_incompleteCues.size())
        INFO_LOG(LOGIDENTIFIER, "flushing incomplete data for cues: start = ", m_currentCueStartTime);

    if (client()) {
        for (auto& partialCue : m_incompleteCues)
            client()->removeDataCue(partialCue.startTime, MediaTime::positiveInfiniteTime(), *partialCue.cueData);
    }

    m_incompleteCues.shrink(0);
    m_currentCueStartTime = MediaTime::zeroTime();
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && (USE(AVFOUNDATION) || PLATFORM(IOS_FAMILY))
