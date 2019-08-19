/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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
#include "InbandWebVTTTextTrack.h"

#if ENABLE(VIDEO_TRACK)

#include "InbandTextTrackPrivate.h"
#include "Logging.h"
#include "VTTCue.h"
#include "VTTRegionList.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/CString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(InbandWebVTTTextTrack);

inline InbandWebVTTTextTrack::InbandWebVTTTextTrack(ScriptExecutionContext& context, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
    : InbandTextTrack(context, client, trackPrivate)
{
}

Ref<InbandTextTrack> InbandWebVTTTextTrack::create(ScriptExecutionContext& context, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
{
    return adoptRef(*new InbandWebVTTTextTrack(context, client, trackPrivate));
}

InbandWebVTTTextTrack::~InbandWebVTTTextTrack() = default;

WebVTTParser& InbandWebVTTTextTrack::parser()
{
    if (!m_webVTTParser)
        m_webVTTParser = makeUnique<WebVTTParser>(static_cast<WebVTTParserClient*>(this), scriptExecutionContext());
    return *m_webVTTParser;
}

void InbandWebVTTTextTrack::parseWebVTTCueData(const char* data, unsigned length)
{
    parser().parseBytes(data, length);
}

void InbandWebVTTTextTrack::parseWebVTTCueData(const ISOWebVTTCue& cueData)
{
    parser().parseCueData(cueData);
}

void InbandWebVTTTextTrack::newCuesParsed()
{
    Vector<RefPtr<WebVTTCueData>> cues;
    parser().getNewCues(cues);
    for (auto& cueData : cues) {
        auto vttCue = VTTCue::create(*scriptExecutionContext(), *cueData);
        if (hasCue(vttCue.ptr(), TextTrackCue::IgnoreDuration)) {
            INFO_LOG(LOGIDENTIFIER, "ignoring already added cue: ", vttCue.get());
            return;
        }

        INFO_LOG(LOGIDENTIFIER, vttCue.get());

        addCue(WTFMove(vttCue));
    }
}
    
void InbandWebVTTTextTrack::newRegionsParsed()
{
    Vector<RefPtr<VTTRegion>> newRegions;
    parser().getNewRegions(newRegions);
    for (auto& region : newRegions) {
        region->setTrack(this);
        regions()->add(region.releaseNonNull());
    }
}

void InbandWebVTTTextTrack::newStyleSheetsParsed()
{
}

void InbandWebVTTTextTrack::fileFailedToParse()
{
    ERROR_LOG(LOGIDENTIFIER, "Error parsing WebVTT stream.");
}

} // namespace WebCore

#endif
