/*
 * Copyright (C) 2012, 2014 Apple Inc.  All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "InbandWebVTTTextTrack.h"

#include "InbandTextTrackPrivate.h"
#include "Logging.h"
#include "NotImplemented.h"
#include <wtf/text/CString.h>

#if ENABLE(WEBVTT_REGIONS)
#include "VTTRegionList.h"
#endif

namespace WebCore {

PassRefPtr<InbandTextTrack> InbandWebVTTTextTrack::create(ScriptExecutionContext* context, TextTrackClient* client, PassRefPtr<InbandTextTrackPrivate> playerPrivate)
{
    return adoptRef(new InbandWebVTTTextTrack(context, client, playerPrivate));
}

InbandWebVTTTextTrack::InbandWebVTTTextTrack(ScriptExecutionContext* context, TextTrackClient* client, PassRefPtr<InbandTextTrackPrivate> trackPrivate)
    : InbandTextTrack(context, client, trackPrivate)
{
}

InbandWebVTTTextTrack::~InbandWebVTTTextTrack()
{
}

WebVTTParser& InbandWebVTTTextTrack::parser()
{
    if (!m_webVTTParser)
        m_webVTTParser = std::make_unique<WebVTTParser>(static_cast<WebVTTParserClient*>(this), scriptExecutionContext());
    return *m_webVTTParser;
}

void InbandWebVTTTextTrack::parseWebVTTCueData(InbandTextTrackPrivate* trackPrivate, const char* data, unsigned length)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    parser().parseBytes(data, length);
}

void InbandWebVTTTextTrack::parseWebVTTCueData(InbandTextTrackPrivate* trackPrivate, const ISOWebVTTCue& cueData)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    parser().parseCueData(cueData);
}

void InbandWebVTTTextTrack::newCuesParsed()
{
    Vector<RefPtr<WebVTTCueData>> cues;
    parser().getNewCues(cues);

    for (auto& cueData : cues) {
        RefPtr<VTTCue> vttCue = VTTCue::create(*scriptExecutionContext(), *cueData);

        if (hasCue(vttCue.get(), TextTrackCue::IgnoreDuration)) {
            LOG(Media, "InbandWebVTTTextTrack::newCuesParsed ignoring already added cue: start=%.2f, end=%.2f, content=\"%s\"\n", vttCue->startTime(), vttCue->endTime(), vttCue->text().utf8().data());
            return;
        }
        addCue(vttCue.release(), ASSERT_NO_EXCEPTION);
    }
}
    
#if ENABLE(WEBVTT_REGIONS)
void InbandWebVTTTextTrack::newRegionsParsed()
{
    Vector<RefPtr<VTTRegion>> newRegions;
    parser().getNewRegions(newRegions);

    for (auto& region : newRegions) {
        region->setTrack(this);
        regions()->add(region);
    }
}
#endif
    
void InbandWebVTTTextTrack::fileFailedToParse()
{
    LOG(Media, "Error parsing WebVTT stream.");
}

} // namespace WebCore

#endif
