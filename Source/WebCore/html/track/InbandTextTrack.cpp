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
#include "InbandTextTrack.h"

#if ENABLE(VIDEO_TRACK)

#include "HTMLMediaElement.h"
#include "InbandDataTextTrack.h"
#include "InbandGenericTextTrack.h"
#include "InbandTextTrackPrivate.h"
#include "InbandWebVTTTextTrack.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(InbandTextTrack);

Ref<InbandTextTrack> InbandTextTrack::create(ScriptExecutionContext& context, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
{
    switch (trackPrivate.cueFormat()) {
    case InbandTextTrackPrivate::Data:
        return InbandDataTextTrack::create(context, client, trackPrivate);
    case InbandTextTrackPrivate::Generic:
        return InbandGenericTextTrack::create(context, client, trackPrivate);
    case InbandTextTrackPrivate::WebVTT:
        return InbandWebVTTTextTrack::create(context, client, trackPrivate);
    }
    ASSERT_NOT_REACHED();
    return InbandDataTextTrack::create(context, client, trackPrivate);
}

InbandTextTrack::InbandTextTrack(ScriptExecutionContext& context, TextTrackClient& client, InbandTextTrackPrivate& trackPrivate)
    : TextTrack(&context, &client, emptyAtom(), trackPrivate.id(), trackPrivate.label(), trackPrivate.language(), InBand)
    , m_private(trackPrivate)
{
#if !RELEASE_LOG_DISABLED
    m_private->setLogger(logger(), logIdentifier());
#endif
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
        return InbandTextTrackPrivate::Disabled;
    case TextTrack::Mode::Hidden:
        return InbandTextTrackPrivate::Hidden;
    case TextTrack::Mode::Showing:
        return InbandTextTrackPrivate::Showing;
    }
    ASSERT_NOT_REACHED();
    return InbandTextTrackPrivate::Disabled;
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
    auto element = makeRefPtr(mediaElement());
    if (!element)
        return;
    element->removeTextTrack(*this);
}

void InbandTextTrack::updateKindFromPrivate()
{
    switch (m_private->kind()) {
    case InbandTextTrackPrivate::Subtitles:
        setKind(Kind::Subtitles);
        return;
    case InbandTextTrackPrivate::Captions:
        setKind(Kind::Captions);
        return;
    case InbandTextTrackPrivate::Descriptions:
        setKind(Kind::Descriptions);
        return;
    case InbandTextTrackPrivate::Chapters:
        setKind(Kind::Chapters);
        return;
    case InbandTextTrackPrivate::Metadata:
        setKind(Kind::Metadata);
        return;
    case InbandTextTrackPrivate::Forced:
        setKind(Kind::Forced);
        return;
    case InbandTextTrackPrivate::None:
        break;
    }
    ASSERT_NOT_REACHED();
}

MediaTime InbandTextTrack::startTimeVariance() const
{
    return m_private->startTimeVariance();
}

void InbandTextTrack::setMediaElement(HTMLMediaElement* element)
{
    TrackBase::setMediaElement(element);
#if !RELEASE_LOG_DISABLED
    m_private->setLogger(logger(), logIdentifier());
#endif
}


} // namespace WebCore

#endif
