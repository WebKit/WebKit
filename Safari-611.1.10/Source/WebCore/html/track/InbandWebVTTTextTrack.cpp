/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "InbandTextTrackPrivate.h"
#include "Logging.h"
#include "VTTCue.h"
#include "VTTRegionList.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/CString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(InbandWebVTTTextTrack);

inline InbandWebVTTTextTrack::InbandWebVTTTextTrack(Document& document, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
    : InbandTextTrack(document, client, trackPrivate)
{
}

Ref<InbandTextTrack> InbandWebVTTTextTrack::create(Document& document, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
{
    return adoptRef(*new InbandWebVTTTextTrack(document, client, trackPrivate));
}

InbandWebVTTTextTrack::~InbandWebVTTTextTrack() = default;

WebVTTParser& InbandWebVTTTextTrack::parser()
{
    if (!m_webVTTParser)
        m_webVTTParser = makeUnique<WebVTTParser>(static_cast<WebVTTParserClient&>(*this), document());
    return *m_webVTTParser;
}

void InbandWebVTTTextTrack::parseWebVTTCueData(const char* data, unsigned length)
{
    parser().parseBytes(data, length);
}

void InbandWebVTTTextTrack::parseWebVTTCueData(ISOWebVTTCue&& cueData)
{
    parser().parseCueData(WTFMove(cueData));
}

void InbandWebVTTTextTrack::newCuesParsed()
{
    for (auto& cueData : parser().takeCues()) {
        auto cue = VTTCue::create(document(), cueData);
        if (hasCue(cue, TextTrackCue::IgnoreDuration)) {
            INFO_LOG(LOGIDENTIFIER, "ignoring already added cue: ", cue.get());
            return;
        }
        INFO_LOG(LOGIDENTIFIER, cue.get());
        addCue(WTFMove(cue));
    }
}
    
void InbandWebVTTTextTrack::newRegionsParsed()
{
    for (auto& region : parser().takeRegions()) {
        region->setTrack(this);
        regions()->add(WTFMove(region));
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
