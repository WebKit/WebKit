/*
 * Copyright (C) 2011, 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2011, 2012, 2013, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(VIDEO_TRACK)

#include "TextTrackCue.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "DocumentFragment.h"
#include "Event.h"
#include "HTMLDivElement.h"
#include "HTMLSpanElement.h"
#include "Logging.h"
#include "NodeTraversal.h"
#include "Text.h"
#include "TextTrack.h"
#include "TextTrackCueList.h"
#include "VTTCue.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(WEBVTT_REGIONS)
#include "VTTRegionList.h"
#endif

namespace WebCore {

static const int invalidCueIndex = -1;

PassRefPtr<TextTrackCue> TextTrackCue::create(ScriptExecutionContext& context, double start, double end, const String& content)
{
    return VTTCue::create(context, start, end, content);
}

TextTrackCue::TextTrackCue(ScriptExecutionContext& context, double start, double end)
    : m_startTime(start)
    , m_endTime(end)
    , m_cueIndex(invalidCueIndex)
    , m_processingCueChanges(0)
    , m_track(0)
    , m_scriptExecutionContext(context)
    , m_isActive(false)
    , m_pauseOnExit(false)
{
    ASSERT(m_scriptExecutionContext.isDocument());
}

TextTrackCue::~TextTrackCue()
{
}

void TextTrackCue::willChange()
{
    if (++m_processingCueChanges > 1)
        return;

    if (m_track)
        m_track->cueWillChange(this);
}

void TextTrackCue::didChange()
{
    ASSERT(m_processingCueChanges);
    if (--m_processingCueChanges)
        return;

    if (m_track)
        m_track->cueDidChange(this);
}

TextTrack* TextTrackCue::track() const
{
    return m_track;
}

void TextTrackCue::setTrack(TextTrack* track)
{
    m_track = track;
}

void TextTrackCue::setId(const String& id)
{
    if (m_id == id)
        return;

    willChange();
    m_id = id;
    didChange();
}

void TextTrackCue::setStartTime(double value, ExceptionCode& ec)
{
    // NaN, Infinity and -Infinity values should trigger a TypeError.
    if (std::isinf(value) || std::isnan(value)) {
        ec = TypeError;
        return;
    }
    
    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (m_startTime == value || value < 0)
        return;

    willChange();
    m_startTime = value;
    didChange();
}
    
void TextTrackCue::setEndTime(double value, ExceptionCode& ec)
{
    // NaN, Infinity and -Infinity values should trigger a TypeError.
    if (std::isinf(value) || std::isnan(value)) {
        ec = TypeError;
        return;
    }

    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (m_endTime == value || value < 0)
        return;

    willChange();
    m_endTime = value;
    didChange();
}
    
void TextTrackCue::setPauseOnExit(bool value)
{
    if (m_pauseOnExit == value)
        return;
    
    m_pauseOnExit = value;
}

int TextTrackCue::cueIndex()
{
    if (m_cueIndex == invalidCueIndex) {
        ASSERT(track());
        ASSERT(track()->cues());
        if (TextTrackCueList* cueList = track()->cues())
            m_cueIndex = cueList->getCueIndex(this);
    }

    return m_cueIndex;
}

void TextTrackCue::invalidateCueIndex()
{
    m_cueIndex = invalidCueIndex;
}

bool TextTrackCue::dispatchEvent(PassRefPtr<Event> event)
{
    // When a TextTrack's mode is disabled: no cues are active, no events fired.
    if (!track() || track()->mode() == TextTrack::disabledKeyword())
        return false;

    return EventTarget::dispatchEvent(event);
}

bool TextTrackCue::isActive()
{
    return m_isActive && track() && track()->mode() != TextTrack::disabledKeyword();
}

void TextTrackCue::setIsActive(bool active)
{
    m_isActive = active;
}

bool TextTrackCue::isOrderedBefore(const TextTrackCue* other) const
{
    return startTime() < other->startTime() || (startTime() == other->startTime() && endTime() > other->endTime());
}

bool TextTrackCue::isEqual(const TextTrackCue& cue, TextTrackCue::CueMatchRules match) const
{
    if (cueType() != cue.cueType())
        return false;

    if (match != IgnoreDuration && endTime() != cue.endTime())
        return false;
    if (startTime() != cue.startTime())
        return false;
    if (id() != cue.id())
        return false;

    return true;
}

} // namespace WebCore

#endif
