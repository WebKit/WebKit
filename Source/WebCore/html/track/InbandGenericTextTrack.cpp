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
#include "InbandGenericTextTrack.h"

#if ENABLE(VIDEO_TRACK)

#include "HTMLMediaElement.h"
#include "InbandTextTrackPrivate.h"
#include "Logging.h"
#include "VTTRegionList.h"
#include <math.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/CString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(InbandGenericTextTrack);

void GenericTextTrackCueMap::add(InbandGenericCueIdentifier inbandCueIdentifier, TextTrackCueGeneric& publicCue)
{
    m_dataToCueMap.add(inbandCueIdentifier, &publicCue);
    m_cueToDataMap.add(&publicCue, inbandCueIdentifier);
}

TextTrackCueGeneric* GenericTextTrackCueMap::find(InbandGenericCueIdentifier inbandCueIdentifier)
{
    return m_dataToCueMap.get(inbandCueIdentifier);
}

void GenericTextTrackCueMap::remove(InbandGenericCueIdentifier inbandCueIdentifier)
{
    if (auto publicCue = m_dataToCueMap.take(inbandCueIdentifier))
        m_cueToDataMap.remove(publicCue.get());
}

void GenericTextTrackCueMap::remove(TextTrackCue& publicCue)
{
    if (auto cueIdentifier = m_cueToDataMap.take(&publicCue))
        m_dataToCueMap.remove(cueIdentifier);
}

inline InbandGenericTextTrack::InbandGenericTextTrack(Document& document, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
    : InbandTextTrack(document, client, trackPrivate)
{
}

Ref<InbandGenericTextTrack> InbandGenericTextTrack::create(Document& document, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
{
    return adoptRef(*new InbandGenericTextTrack(document, client, trackPrivate));
}

InbandGenericTextTrack::~InbandGenericTextTrack() = default;

void InbandGenericTextTrack::updateCueFromCueData(TextTrackCueGeneric& cue, InbandGenericCue& inbandCue)
{
    cue.willChange();

    cue.setStartTime(inbandCue.startTime());
    MediaTime endTime = inbandCue.endTime();
    if (endTime.isPositiveInfinite() && mediaElement())
        endTime = mediaElement()->durationMediaTime();
    cue.setEndTime(endTime);
    cue.setText(inbandCue.content());
    cue.setId(inbandCue.id());
    cue.setBaseFontSizeRelativeToVideoHeight(inbandCue.baseFontSize());
    cue.setFontSizeMultiplier(inbandCue.relativeFontSize());
    cue.setFontName(inbandCue.fontName());

    if (inbandCue.position() > 0)
        cue.setPosition(std::round(inbandCue.position()));
    if (inbandCue.line() > 0)
        cue.setLine(std::round(inbandCue.line()));
    if (inbandCue.size() > 0)
        cue.setSize(std::round(inbandCue.size()));
    if (inbandCue.backgroundColor().isValid())
        cue.setBackgroundColor(inbandCue.backgroundColor());
    if (inbandCue.foregroundColor().isValid())
        cue.setForegroundColor(inbandCue.foregroundColor());
    if (inbandCue.highlightColor().isValid())
        cue.setHighlightColor(inbandCue.highlightColor());

    if (inbandCue.align() == GenericCueData::Alignment::Start)
        cue.setAlign("start"_s);
    else if (inbandCue.align() == GenericCueData::Alignment::Middle)
        cue.setAlign("middle"_s);
    else if (inbandCue.align() == GenericCueData::Alignment::End)
        cue.setAlign("end"_s);
    cue.setSnapToLines(false);

    cue.didChange();
}

void InbandGenericTextTrack::addGenericCue(InbandGenericCue& inbandCue)
{
    if (m_cueMap.find(inbandCue.uniqueId()))
        return;

    auto cue = TextTrackCueGeneric::create(document(), inbandCue.startTime(), inbandCue.endTime(), inbandCue.content());
    updateCueFromCueData(cue.get(), inbandCue);
    if (hasCue(cue, TextTrackCue::IgnoreDuration)) {
        INFO_LOG(LOGIDENTIFIER, "ignoring already added cue: ", cue.get());
        return;
    }

    INFO_LOG(LOGIDENTIFIER, "added cue: ", cue.get());

    if (inbandCue.status() != GenericCueData::Status::Complete)
        m_cueMap.add(inbandCue.uniqueId(), cue);

    addCue(WTFMove(cue));
}

void InbandGenericTextTrack::updateGenericCue(InbandGenericCue& inbandCue)
{
    auto cue = makeRefPtr(m_cueMap.find(inbandCue.uniqueId()));
    if (!cue)
        return;

    updateCueFromCueData(*cue, inbandCue);

    if (inbandCue.status() == GenericCueData::Status::Complete)
        m_cueMap.remove(inbandCue.uniqueId());
}

void InbandGenericTextTrack::removeGenericCue(InbandGenericCue& inbandCue)
{
    auto cue = makeRefPtr(m_cueMap.find(inbandCue.uniqueId()));
    if (cue) {
        INFO_LOG(LOGIDENTIFIER, *cue);
        removeCue(*cue);
    } else
        INFO_LOG(LOGIDENTIFIER, "UNABLE to find cue: ", inbandCue);

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
        m_webVTTParser = makeUnique<WebVTTParser>(static_cast<WebVTTParserClient&>(*this), document());
    return *m_webVTTParser;
}

void InbandGenericTextTrack::parseWebVTTCueData(ISOWebVTTCue&& cueData)
{
    parser().parseCueData(WTFMove(cueData));
}

void InbandGenericTextTrack::parseWebVTTFileHeader(String&& header)
{
    parser().parseFileHeader(WTFMove(header));
}

void InbandGenericTextTrack::newCuesParsed()
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

void InbandGenericTextTrack::newRegionsParsed()
{
    for (auto& region : parser().takeRegions()) {
        region->setTrack(this);
        regions()->add(WTFMove(region));
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
