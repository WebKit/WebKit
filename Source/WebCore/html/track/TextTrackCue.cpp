/*
 * Copyright (C) 2011, 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#include "TextTrackCue.h"

#if ENABLE(VIDEO_TRACK)

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Event.h"
#include "Logging.h"
#include "NodeTraversal.h"
#include "Text.h"
#include "TextTrack.h"
#include "TextTrackCueList.h"
#include "VTTCue.h"
#include "VTTRegionList.h"
#include <wtf/HexNumber.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(TextTrackCue);

const AtomString& TextTrackCue::cueShadowPseudoId()
{
    static NeverDestroyed<const AtomString> cue("cue", AtomString::ConstructFromLiteral);
    return cue;
}

TextTrackCue::TextTrackCue(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end)
    : m_startTime(start)
    , m_endTime(end)
    , m_scriptExecutionContext(context)
    , m_isActive(false)
    , m_pauseOnExit(false)
{
    ASSERT(m_scriptExecutionContext.isDocument());
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

void TextTrackCue::setStartTime(double value)
{
    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (m_startTime.toDouble() == value || value < 0)
        return;

    setStartTime(MediaTime::createWithDouble(value));
}

void TextTrackCue::setStartTime(const MediaTime& value)
{
    willChange();
    m_startTime = value;
    didChange();
}
    
void TextTrackCue::setEndTime(double value)
{
    // TODO(93143): Add spec-compliant behavior for negative time values.
    if (m_endTime.toDouble() == value || value < 0)
        return;

    setEndTime(MediaTime::createWithDouble(value));
}

void TextTrackCue::setEndTime(const MediaTime& value)
{
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

void TextTrackCue::dispatchEvent(Event& event)
{
    // When a TextTrack's mode is disabled: no cues are active, no events fired.
    if (!track() || track()->mode() == TextTrack::Mode::Disabled)
        return;

    EventTarget::dispatchEvent(event);
}

bool TextTrackCue::isActive()
{
    return m_isActive && track() && track()->mode() != TextTrack::Mode::Disabled;
}

void TextTrackCue::setIsActive(bool active)
{
    m_isActive = active;
}

bool TextTrackCue::isOrderedBefore(const TextTrackCue* other) const
{
    return startMediaTime() < other->startMediaTime() || (startMediaTime() == other->startMediaTime() && endMediaTime() > other->endMediaTime());
}

bool TextTrackCue::cueContentsMatch(const TextTrackCue& cue) const
{
    if (cueType() != cue.cueType())
        return false;

    if (id() != cue.id())
        return false;

    return true;
}

bool TextTrackCue::isEqual(const TextTrackCue& cue, TextTrackCue::CueMatchRules match) const
{
    if (cueType() != cue.cueType())
        return false;

    if (match != IgnoreDuration && endMediaTime() != cue.endMediaTime())
        return false;
    if (!hasEquivalentStartTime(cue))
        return false;
    if (!cueContentsMatch(cue))
        return false;

    return true;
}

bool TextTrackCue::hasEquivalentStartTime(const TextTrackCue& cue) const
{
    MediaTime startTimeVariance = MediaTime::zeroTime();
    if (track())
        startTimeVariance = track()->startTimeVariance();
    else if (cue.track())
        startTimeVariance = cue.track()->startTimeVariance();

    return abs(abs(startMediaTime()) - abs(cue.startMediaTime())) <= startTimeVariance;
}

bool TextTrackCue::doesExtendCue(const TextTrackCue& cue) const
{
    if (!cueContentsMatch(cue))
        return false;

    if (endMediaTime() != cue.startMediaTime())
        return false;
    
    return true;
}

void TextTrackCue::toJSON(JSON::Object& value) const
{
    ASCIILiteral type = "Generic"_s;
    switch (cueType()) {
    case TextTrackCue::Generic:
        type = "Generic"_s;
        break;
    case TextTrackCue::WebVTT:
        type = "WebVTT"_s;
        break;
    case TextTrackCue::Data:
        type = "Data"_s;
        break;
    }

    value.setString("type"_s, type);
    value.setDouble("startTime"_s, startTime());
    value.setDouble("endTime"_s, endTime());
}

String TextTrackCue::toJSONString() const
{
    auto object = JSON::Object::create();

    toJSON(object.get());

    return object->toJSONString();
}

String TextTrackCue::debugString() const
{
    String text;
    if (isRenderable())
        text = toVTTCue(this)->text();
    return makeString("0x", hex(reinterpret_cast<uintptr_t>(this)), " id=", id(), " interval=", startTime(), "-->", endTime(), " cue=", text, ')');
}

} // namespace WebCore

#endif
