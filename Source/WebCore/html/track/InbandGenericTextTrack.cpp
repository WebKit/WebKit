/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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

#include "InbandGenericTextTrack.h"

#include "ExceptionCodePlaceholder.h"
#include "HTMLMediaElement.h"
#include "InbandTextTrackPrivate.h"
#include "Logging.h"
#include <math.h>
#include <wtf/text/CString.h>

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

    cue->setStartTime(cueData->startTime(), IGNORE_EXCEPTION);
    double endTime = cueData->endTime();
    if (std::isinf(endTime) && mediaElement())
        endTime = mediaElement()->duration();
    cue->setEndTime(endTime, IGNORE_EXCEPTION);
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
    if (hasCue(cue.get(), VTTCue::IgnoreDuration)) {
        LOG(Media, "InbandGenericTextTrack::addGenericCue ignoring already added cue: start=%.2f, end=%.2f, content=\"%s\"\n", cueData->startTime(), cueData->endTime(), cueData->content().utf8().data());
        return;
    }

    if (cueData->status() != GenericCueData::Complete)
        m_cueMap.add(cueData.get(), cue.get());

    addCue(cue);
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
        LOG(Media, "InbandGenericTextTrack::removeGenericCue removing cue: start=%.2f, end=%.2f, content=\"%s\"\n", cueData->startTime(), cueData->endTime(), cueData->content().utf8().data());
        removeCue(cue.get(), IGNORE_EXCEPTION);
    } else
        m_cueMap.remove(cueData);
}

void InbandGenericTextTrack::removeCue(TextTrackCue* cue, ExceptionCode& ec)
{
    m_cueMap.remove(cue);
    TextTrack::removeCue(cue, ec);
}

} // namespace WebCore

#endif
