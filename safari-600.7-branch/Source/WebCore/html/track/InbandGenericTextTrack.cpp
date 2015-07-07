/*
 * Copyright (C) 2012-2014 Apple Inc.  All rights reserved.
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

#include "InbandGenericTextTrack.h"

#include "ExceptionCodePlaceholder.h"
#include "HTMLMediaElement.h"
#include "InbandTextTrackPrivate.h"
#include "Logging.h"
#include <math.h>
#include <wtf/text/CString.h>

#if ENABLE(WEBVTT_REGIONS)
#include "VTTRegionList.h"
#endif

namespace WebCore {

GenericTextTrackCueMap::GenericTextTrackCueMap()
{
}

GenericTextTrackCueMap::~GenericTextTrackCueMap()
{
}

void GenericTextTrackCueMap::add(GenericCueData* cueData, TextTrackCueGeneric* cue)
{
    m_dataToCueMap.add(cueData, cue);
    m_cueToDataMap.add(cue, cueData);
}

PassRefPtr<TextTrackCueGeneric> GenericTextTrackCueMap::find(GenericCueData* cueData)
{
    CueDataToCueMap::iterator iter = m_dataToCueMap.find(cueData);
    if (iter == m_dataToCueMap.end())
        return 0;

    return iter->value;
}

PassRefPtr<GenericCueData> GenericTextTrackCueMap::find(TextTrackCue* cue)
{
    CueToDataMap::iterator iter = m_cueToDataMap.find(cue);
    if (iter == m_cueToDataMap.end())
        return 0;

    return iter->value;
}

void GenericTextTrackCueMap::remove(GenericCueData* cueData)
{
    RefPtr<TextTrackCueGeneric> cue = find(cueData);

    if (cue)
        m_cueToDataMap.remove(cue);
    m_dataToCueMap.remove(cueData);
}

void GenericTextTrackCueMap::remove(TextTrackCue* cue)
{
    RefPtr<GenericCueData> genericData = find(cue);
    if (genericData) {
        m_dataToCueMap.remove(genericData);
        m_cueToDataMap.remove(cue);
    }
}

PassRefPtr<InbandGenericTextTrack> InbandGenericTextTrack::create(ScriptExecutionContext* context, TextTrackClient* client, PassRefPtr<InbandTextTrackPrivate> playerPrivate)
{
    return adoptRef(new InbandGenericTextTrack(context, client, playerPrivate));
}

InbandGenericTextTrack::InbandGenericTextTrack(ScriptExecutionContext* context, TextTrackClient* client, PassRefPtr<InbandTextTrackPrivate> trackPrivate)
    : InbandTextTrack(context, client, trackPrivate)
{
}

InbandGenericTextTrack::~InbandGenericTextTrack()
{
}

void InbandGenericTextTrack::updateCueFromCueData(TextTrackCueGeneric* cue, GenericCueData* cueData)
{
    cue->willChange();

    cue->setStartTime(cueData->startTime());
    MediaTime endTime = cueData->endTime();
    if (endTime.isPositiveInfinite() && mediaElement())
        endTime = mediaElement()->durationMediaTime();
    cue->setEndTime(endTime);
    cue->setText(cueData->content());
    cue->setId(cueData->id());
    cue->setBaseFontSizeRelativeToVideoHeight(cueData->baseFontSize());
    cue->setFontSizeMultiplier(cueData->relativeFontSize());
    cue->setFontName(cueData->fontName());

    if (cueData->position() > 0)
        cue->setPosition(lround(cueData->position()), IGNORE_EXCEPTION);
    if (cueData->line() > 0)
        cue->setLine(lround(cueData->line()), IGNORE_EXCEPTION);
    if (cueData->size() > 0)
        cue->setSize(lround(cueData->size()), IGNORE_EXCEPTION);
    if (cueData->backgroundColor().isValid())
        cue->setBackgroundColor(cueData->backgroundColor().rgb());
    if (cueData->foregroundColor().isValid())
        cue->setForegroundColor(cueData->foregroundColor().rgb());
    if (cueData->highlightColor().isValid())
        cue->setHighlightColor(cueData->highlightColor().rgb());

    if (cueData->align() == GenericCueData::Start)
        cue->setAlign(ASCIILiteral("start"), IGNORE_EXCEPTION);
    else if (cueData->align() == GenericCueData::Middle)
        cue->setAlign(ASCIILiteral("middle"), IGNORE_EXCEPTION);
    else if (cueData->align() == GenericCueData::End)
        cue->setAlign(ASCIILiteral("end"), IGNORE_EXCEPTION);
    cue->setSnapToLines(false);

    cue->didChange();
}

void InbandGenericTextTrack::addGenericCue(InbandTextTrackPrivate* trackPrivate, PassRefPtr<GenericCueData> prpCueData)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);

    RefPtr<GenericCueData> cueData = prpCueData;
    if (m_cueMap.find(cueData.get()))
        return;

    RefPtr<TextTrackCueGeneric> cue = TextTrackCueGeneric::create(*scriptExecutionContext(), cueData->startTime(), cueData->endTime(), cueData->content());
    updateCueFromCueData(cue.get(), cueData.get());
    if (hasCue(cue.get(), TextTrackCue::IgnoreDuration)) {
        LOG(Media, "InbandGenericTextTrack::addGenericCue ignoring already added cue: start=%s, end=%s, content=\"%s\"\n", toString(cueData->startTime()).utf8().data(), toString(cueData->endTime()).utf8().data(), cueData->content().utf8().data());
        return;
    }

    LOG(Media, "InbandGenericTextTrack::addGenericCue added cue: start=%.2f, end=%.2f, content=\"%s\"\n", cueData->startTime().toDouble(), cueData->endTime().toDouble(), cueData->content().utf8().data());

    if (cueData->status() != GenericCueData::Complete)
        m_cueMap.add(cueData.get(), cue.get());

    addCue(cue, ASSERT_NO_EXCEPTION);
}

void InbandGenericTextTrack::updateGenericCue(InbandTextTrackPrivate*, GenericCueData* cueData)
{
    RefPtr<TextTrackCueGeneric> cue = m_cueMap.find(cueData);
    if (!cue)
        return;

    updateCueFromCueData(cue.get(), cueData);

    if (cueData->status() == GenericCueData::Complete)
        m_cueMap.remove(cueData);
}

void InbandGenericTextTrack::removeGenericCue(InbandTextTrackPrivate*, GenericCueData* cueData)
{
    RefPtr<TextTrackCueGeneric> cue = m_cueMap.find(cueData);
    if (cue) {
        LOG(Media, "InbandGenericTextTrack::removeGenericCue removing cue: start=%s, end=%s, content=\"%s\"\n",  toString(cueData->startTime()).utf8().data(), toString(cueData->endTime()).utf8().data(), cueData->content().utf8().data());
        removeCue(cue.get(), IGNORE_EXCEPTION);
    } else {
        LOG(Media, "InbandGenericTextTrack::removeGenericCue UNABLE to find cue: start=%.2f, end=%.2f, content=\"%s\"\n", cueData->startTime().toDouble(), cueData->endTime().toDouble(), cueData->content().utf8().data());
        m_cueMap.remove(cueData);
    }
}

void InbandGenericTextTrack::removeCue(TextTrackCue* cue, ExceptionCode& ec)
{
    m_cueMap.remove(cue);
    TextTrack::removeCue(cue, ec);
}

WebVTTParser& InbandGenericTextTrack::parser()
{
    if (!m_webVTTParser)
        m_webVTTParser = std::make_unique<WebVTTParser>(static_cast<WebVTTParserClient*>(this), scriptExecutionContext());
    return *m_webVTTParser;
}

void InbandGenericTextTrack::parseWebVTTCueData(InbandTextTrackPrivate* trackPrivate, const ISOWebVTTCue& cueData)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    parser().parseCueData(cueData);
}

void InbandGenericTextTrack::parseWebVTTFileHeader(InbandTextTrackPrivate* trackPrivate, String header)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    parser().parseFileHeader(header);
}

void InbandGenericTextTrack::newCuesParsed()
{
    Vector<RefPtr<WebVTTCueData>> cues;
    parser().getNewCues(cues);

    for (auto& cueData : cues) {
        RefPtr<VTTCue> vttCue = VTTCue::create(*scriptExecutionContext(), *cueData);

        if (hasCue(vttCue.get(), TextTrackCue::IgnoreDuration)) {
            LOG(Media, "InbandGenericTextTrack::newCuesParsed ignoring already added cue: start=%.2f, end=%.2f, content=\"%s\"\n", vttCue->startTime(), vttCue->endTime(), vttCue->text().utf8().data());
            return;
        }
        addCue(vttCue.release(), ASSERT_NO_EXCEPTION);
    }
}

#if ENABLE(WEBVTT_REGIONS)
void InbandGenericTextTrack::newRegionsParsed()
{
    Vector<RefPtr<VTTRegion>> newRegions;
    parser().getNewRegions(newRegions);

    for (auto& region : newRegions) {
        region->setTrack(this);
        regions()->add(region);
    }
}
#endif

void InbandGenericTextTrack::fileFailedToParse()
{
    LOG(Media, "Error parsing WebVTT stream.");
}

} // namespace WebCore

#endif
