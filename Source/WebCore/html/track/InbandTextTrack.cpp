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

#include "InbandTextTrack.h"

#include "Document.h"
#include "Event.h"
#include "ExceptionCodePlaceholder.h"
#include "HTMLMediaElement.h"
#include "InbandGenericTextTrack.h"
#include "InbandTextTrackPrivate.h"
#include "InbandWebVTTTextTrack.h"
#include "Logging.h"
#include "TextTrackCueList.h"
#include <math.h>
#include <wtf/text/CString.h>

namespace WebCore {

PassRefPtr<InbandTextTrack> InbandTextTrack::create(ScriptExecutionContext* context,
    TextTrackClient* client, PassRefPtr<InbandTextTrackPrivate> trackPrivate)
{
    switch (trackPrivate->cueFormat()) {
    case InbandTextTrackPrivate::Generic:
        return InbandGenericTextTrack::create(context, client, trackPrivate);
    case InbandTextTrackPrivate::WebVTT:
        return InbandWebVTTTextTrack::create(context, client, trackPrivate);
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

InbandTextTrack::InbandTextTrack(ScriptExecutionContext* context, TextTrackClient* client, PassRefPtr<InbandTextTrackPrivate> trackPrivate)
    : TextTrack(context, client, emptyAtom, trackPrivate->id(), trackPrivate->label(), trackPrivate->language(), InBand)
    , m_private(trackPrivate)
{
    m_private->setClient(this);
    
    switch (m_private->kind()) {
    case InbandTextTrackPrivate::Subtitles:
        setKind(TextTrack::subtitlesKeyword());
        break;
    case InbandTextTrackPrivate::Captions:
        setKind(TextTrack::captionsKeyword());
        break;
    case InbandTextTrackPrivate::Descriptions:
        setKind(TextTrack::descriptionsKeyword());
        break;
    case InbandTextTrackPrivate::Chapters:
        setKind(TextTrack::chaptersKeyword());
        break;
    case InbandTextTrackPrivate::Metadata:
        setKind(TextTrack::metadataKeyword());
        break;
    case InbandTextTrackPrivate::Forced:
        setKind(TextTrack::forcedKeyword());
        break;
    case InbandTextTrackPrivate::None:
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

InbandTextTrack::~InbandTextTrack()
{
    m_private->setClient(0);
}

void InbandTextTrack::setMode(const AtomicString& mode)
{
    TextTrack::setMode(mode);

    if (mode == TextTrack::disabledKeyword())
        m_private->setMode(InbandTextTrackPrivate::Disabled);
    else if (mode == TextTrack::hiddenKeyword())
        m_private->setMode(InbandTextTrackPrivate::Hidden);
    else if (mode == TextTrack::showingKeyword())
        m_private->setMode(InbandTextTrackPrivate::Showing);
    else
        ASSERT_NOT_REACHED();
}

bool InbandTextTrack::isClosedCaptions() const
{
    if (!m_private)
        return false;

    return m_private->isClosedCaptions();
}

bool InbandTextTrack::isSDH() const
{
    if (!m_private)
        return false;
    
    return m_private->isSDH();
}

bool InbandTextTrack::containsOnlyForcedSubtitles() const
{
    if (!m_private)
        return false;
    
    return m_private->containsOnlyForcedSubtitles();
}

bool InbandTextTrack::isMainProgramContent() const
{
    if (!m_private)
        return false;
    
    return m_private->isMainProgramContent();
}

bool InbandTextTrack::isEasyToRead() const
{
    if (!m_private)
        return false;
    
    return m_private->isEasyToRead();
}
    
size_t InbandTextTrack::inbandTrackIndex()
{
    ASSERT(m_private);
    return m_private->trackIndex();
}

void InbandTextTrack::idChanged(TrackPrivateBase* trackPrivate, const AtomicString& id)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    setId(id);
}

void InbandTextTrack::labelChanged(TrackPrivateBase* trackPrivate, const AtomicString& label)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    setLabel(label);
}

void InbandTextTrack::languageChanged(TrackPrivateBase* trackPrivate, const AtomicString& language)
{
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    setLanguage(language);
}

void InbandTextTrack::willRemove(TrackPrivateBase* trackPrivate)
{
    if (!mediaElement())
        return;
    ASSERT_UNUSED(trackPrivate, trackPrivate == m_private);
    mediaElement()->removeTextTrack(this);
}

} // namespace WebCore

#endif
