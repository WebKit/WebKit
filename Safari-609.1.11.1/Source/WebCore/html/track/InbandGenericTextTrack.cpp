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
#include "InbandGenericTextTrack.h"

#if ENABLE(VIDEO_TRACK)

#include "DataCue.h"
#include "HTMLMediaElement.h"
#include "InbandTextTrackPrivate.h"
#include "Logging.h"
#include "VTTRegionList.h"
#include <math.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/CString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(InbandGenericTextTrack);

void GenericTextTrackCueMap::add(GenericCueData& cueData, TextTrackCueGeneric& cue)
{
    m_dataToCueMap.add(&cueData, &cue);
    m_cueToDataMap.add(&cue, &cueData);
}

TextTrackCueGeneric* GenericTextTrackCueMap::find(GenericCueData& cueData)
{
    return m_dataToCueMap.get(&cueData);
}

GenericCueData* GenericTextTrackCueMap::find(TextTrackCue& cue)
{
    return m_cueToDataMap.get(&cue);
}

void GenericTextTrackCueMap::remove(GenericCueData& cueData)
{
    if (auto cue = m_dataToCueMap.take(&cueData))
        m_cueToDataMap.remove(cue);
}

void GenericTextTrackCueMap::remove(TextTrackCue& cue)
{
    if (auto data = m_cueToDataMap.take(&cue))
        m_dataToCueMap.remove(data);
}

inline InbandGenericTextTrack::InbandGenericTextTrack(ScriptExecutionContext& context, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
    : InbandTextTrack(context, client, trackPrivate)
{
}

Ref<InbandGenericTextTrack> InbandGenericTextTrack::create(ScriptExecutionContext& context, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
{
    return adoptRef(*new InbandGenericTextTrack(context, client, trackPrivate));
}

InbandGenericTextTrack::~InbandGenericTextTrack() = default;

void InbandGenericTextTrack::updateCueFromCueData(TextTrackCueGeneric& cue, GenericCueData& cueData)
{
    cue.willChange();

    cue.setStartTime(cueData.startTime());
    MediaTime endTime = cueData.endTime();
    if (endTime.isPositiveInfinite() && mediaElement())
        endTime = mediaElement()->durationMediaTime();
    cue.setEndTime(endTime);
    cue.setText(cueData.content());
    cue.setId(cueData.id());
    cue.setBaseFontSizeRelativeToVideoHeight(cueData.baseFontSize());
    cue.setFontSizeMultiplier(cueData.relativeFontSize());
    cue.setFontName(cueData.fontName());

    if (cueData.position() > 0)
        cue.setPosition(std::round(cueData.position()));
    if (cueData.line() > 0)
        cue.setLine(std::round(cueData.line()));
    if (cueData.size() > 0)
        cue.setSize(std::round(cueData.size()));
    if (cueData.backgroundColor().isValid())
        cue.setBackgroundColor(cueData.backgroundColor().rgb());
    if (cueData.foregroundColor().isValid())
        cue.setForegroundColor(cueData.foregroundColor().rgb());
    if (cueData.highlightColor().isValid())
        cue.setHighlightColor(cueData.highlightColor().rgb());

    if (cueData.align() == GenericCueData::Start)
        cue.setAlign("start"_s);
    else if (cueData.align() == GenericCueData::Middle)
        cue.setAlign("middle"_s);
    else if (cueData.align() == GenericCueData::End)
        cue.setAlign("end"_s);
    cue.setSnapToLines(false);

    cue.didChange();
}

void InbandGenericTextTrack::addGenericCue(GenericCueData& cueData)
{
    if (m_cueMap.find(cueData))
        return;

    auto cue = TextTrackCueGeneric::create(*scriptExecutionContext(), cueData.startTime(), cueData.endTime(), cueData.content());
    updateCueFromCueData(cue.get(), cueData);
    if (hasCue(cue.ptr(), TextTrackCue::IgnoreDuration)) {
        INFO_LOG(LOGIDENTIFIER, "ignoring already added cue: ", cue.get());
        return;
    }

    INFO_LOG(LOGIDENTIFIER, "added cue: ", cue.get());

    if (cueData.status() != GenericCueData::Complete)
        m_cueMap.add(cueData, cue);

    addCue(WTFMove(cue));
}

void InbandGenericTextTrack::updateGenericCue(GenericCueData& cueData)
{
    auto cue = makeRefPtr(m_cueMap.find(cueData));
    if (!cue)
        return;

    updateCueFromCueData(*cue, cueData);

    if (cueData.status() == GenericCueData::Complete)
        m_cueMap.remove(cueData);
}

void InbandGenericTextTrack::removeGenericCue(GenericCueData& cueData)
{
    auto cue = makeRefPtr(m_cueMap.find(cueData));
    if (cue) {
        INFO_LOG(LOGIDENTIFIER, *cue);
        removeCue(*cue);
    } else
        INFO_LOG(LOGIDENTIFIER, "UNABLE to find cue: ", cueData);

}

ExceptionOr<void> InbandGenericTextTrack::removeCue(TextTrackCue& cue)
{
    auto result = TextTrack::removeCue(cue);
    if (!result.hasException())
        m_cueMap.remove(cue);
    return result;
}

WebVTTParser& InbandGenericTextTrack::parser()
{
    if (!m_webVTTParser)
        m_webVTTParser = makeUnique<WebVTTParser>(static_cast<WebVTTParserClient*>(this), scriptExecutionContext());
    return *m_webVTTParser;
}

void InbandGenericTextTrack::parseWebVTTCueData(const ISOWebVTTCue& cueData)
{
    parser().parseCueData(cueData);
}

void InbandGenericTextTrack::parseWebVTTFileHeader(String&& header)
{
    parser().parseFileHeader(WTFMove(header));
}

void InbandGenericTextTrack::newCuesParsed()
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

void InbandGenericTextTrack::newRegionsParsed()
{
    Vector<RefPtr<VTTRegion>> newRegions;
    parser().getNewRegions(newRegions);

    for (auto& region : newRegions) {
        region->setTrack(this);
        regions()->add(region.releaseNonNull());
    }
}

void InbandGenericTextTrack::newStyleSheetsParsed()
{
}

void InbandGenericTextTrack::fileFailedToParse()
{
    ERROR_LOG(LOGIDENTIFIER);
}

} // namespace WebCore

#endif
