/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#include "TextTrackCue.h"

#include "DocumentFragment.h"
#include "TextTrack.h"

namespace WebCore {

TextTrackCue::TextTrackCue(const String& id, double start, double end, const String& content, const String& settings, bool pauseOnExit)
    : m_id(id)
    , m_startTime(start)
    , m_endTime(end)
    , m_content(content)
    , m_settings(settings)
    , m_pauseOnExit(pauseOnExit)
{
}

TextTrackCue::~TextTrackCue()
{
}

TextTrack* TextTrackCue::track() const
{
    return m_track;
}

void TextTrackCue::setTrack(TextTrack* track)
{
    m_track = track;
}

String TextTrackCue::id() const
{
    return m_id;
}

double TextTrackCue::startTime() const
{
    return m_startTime;
}

double TextTrackCue::endTime() const
{
    return m_endTime;
}

bool TextTrackCue::pauseOnExit() const
{
    return m_pauseOnExit;
}

String TextTrackCue::getCueAsSource()
{
    // FIXME(62883): Implement.
    return emptyString();
}

PassRefPtr<DocumentFragment> TextTrackCue::getCueAsHTML()
{
    // FIXME(62883): Implement.
    return DocumentFragment::create(0);
}

bool TextTrackCue::isActive()
{
    // FIXME(62885): Implement.
    return false;
}

void TextTrackCue::setIsActive(bool active)
{
    m_isActive = active;
}

} // namespace WebCore

#endif
