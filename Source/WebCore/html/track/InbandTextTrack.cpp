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
#include "InbandTextTrack.h"

#if ENABLE(VIDEO)

#include "HTMLMediaElement.h"
#include "InbandDataTextTrack.h"
#include "InbandGenericTextTrack.h"
#include "InbandTextTrackPrivate.h"
#include "InbandWebVTTTextTrack.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(InbandTextTrack);

Ref<InbandTextTrack> InbandTextTrack::create(Document& document, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
{
    switch (trackPrivate.cueFormat()) {
    case InbandTextTrackPrivate::CueFormat::Data:
        return InbandDataTextTrack::create(document, client, trackPrivate);
    case InbandTextTrackPrivate::CueFormat::Generic:
        return InbandGenericTextTrack::create(document, client, trackPrivate);
    case InbandTextTrackPrivate::CueFormat::WebVTT:
        return InbandWebVTTTextTrack::create(document, client, trackPrivate);
    }
    ASSERT_NOT_REACHED();
    auto textTrack = InbandDataTextTrack::create(document, client, trackPrivate);
    textTrack->suspendIfNeeded();
    return textTrack;
}

InbandTextTrack::InbandTextTrack(Document& document, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
    : TextTrack(&document, &client, emptyAtom(), trackPrivate.id(), trackPrivate.label(), trackPrivate.language(), InBand)
    , m_private(trackPrivate)
{
    m_private->setClient(this);
    updateKindFromPrivate();
}

InbandTextTrack::~InbandTextTrack()
{
    m_private->setClient(nullptr);
}

void InbandTextTrack::setPrivate(InbandTextTrackPrivate& trackPrivate)
{
    if (m_private.ptr() == &trackPrivate)
        return;

    m_private->setClient(nullptr);
    m_private = trackPrivate;
    m_private->setClient(this);

    setModeInternal(mode());
    updateKindFromPrivate();
    setId(m_private->id());
}

void InbandTextTrack::setMode(Mode mode)
{
    TextTrack::setMode(mode);
    setModeInternal(mode);
}

static inline InbandTextTrackPrivate::Mode toPrivate(TextTrack::Mode mode)
{
    switch (mode) {
    case TextTrack::Mode::Disabled:
        return InbandTextTrackPrivate::Mode::Disabled;
    case TextTrack::Mode::Hidden:
        return InbandTextTrackPrivate::Mode::Hidden;
    case TextTrack::Mode::Showing:
        return InbandTextTrackPrivate::Mode::Showing;
    }
    ASSERT_NOT_REACHED();
    return InbandTextTrackPrivate::Mode::Disabled;
}

void InbandTextTrack::setModeInternal(Mode mode)
{
    m_private->setMode(toPrivate(mode));
}

bool InbandTextTrack::isClosedCaptions() const
{
    return m_private->isClosedCaptions();
}

bool InbandTextTrack::isSDH() const
{
    return m_private->isSDH();
}

bool InbandTextTrack::containsOnlyForcedSubtitles() const
{
    return m_private->containsOnlyForcedSubtitles();
}

bool InbandTextTrack::isMainProgramContent() const
{
    return m_private->isMainProgramContent();
}

bool InbandTextTrack::isEasyToRead() const
{
    return m_private->isEasyToRead();
}
    
size_t InbandTextTrack::inbandTrackIndex()
{
    return m_private->trackIndex();
}

AtomString InbandTextTrack::inBandMetadataTrackDispatchType() const
{
    return m_private->inBandMetadataTrackDispatchType();
}

void InbandTextTrack::idChanged(const AtomString& id)
{
    setId(id);
}

void InbandTextTrack::labelChanged(const AtomString& label)
{
    setLabel(label);
}

void InbandTextTrack::languageChanged(const AtomString& language)
{
    setLanguage(language);
}

void InbandTextTrack::willRemove()
{
    auto element = makeRefPtr(mediaElement().get());
    if (!element)
        return;
    element->removeTextTrack(*this);
}

void InbandTextTrack::updateKindFromPrivate()
{
    switch (m_private->kind()) {
    case InbandTextTrackPrivate::Kind::Subtitles:
        setKind(Kind::Subtitles);
        return;
    case InbandTextTrackPrivate::Kind::Captions:
        setKind(Kind::Captions);
        return;
    case InbandTextTrackPrivate::Kind::Descriptions:
        setKind(Kind::Descriptions);
        return;
    case InbandTextTrackPrivate::Kind::Chapters:
        setKind(Kind::Chapters);
        return;
    case InbandTextTrackPrivate::Kind::Metadata:
        setKind(Kind::Metadata);
        return;
    case InbandTextTrackPrivate::Kind::Forced:
        setKind(Kind::Forced);
        return;
    case InbandTextTrackPrivate::Kind::None:
        break;
    }
    ASSERT_NOT_REACHED();
}

MediaTime InbandTextTrack::startTimeVariance() const
{
    return m_private->startTimeVariance();
}

void InbandTextTrack::setMediaElement(WeakPtr<HTMLMediaElement> element)
{
    TrackBase::setMediaElement(element);
#if !RELEASE_LOG_DISABLED
    m_private->setLogger(logger(), logIdentifier());
#endif
}

#if !RELEASE_LOG_DISABLED
void InbandTextTrack::setLogger(const Logger& logger, const void* logIdentifier)
{
    TextTrack::setLogger(logger, logIdentifier);
    m_private->setLogger(logger, this->logIdentifier());
}
#endif

} // namespace WebCore

#endif
