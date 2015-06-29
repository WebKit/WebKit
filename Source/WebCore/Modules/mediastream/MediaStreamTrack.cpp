/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaStreamTrack.h"

#if ENABLE(MEDIA_STREAM)

#include "AllAudioCapabilities.h"
#include "AllVideoCapabilities.h"
#include "Dictionary.h"
#include "Event.h"
#include "ExceptionCode.h"
#include "ExceptionCodePlaceholder.h"
#include "MediaConstraintsImpl.h"
#include "MediaSourceStates.h"
#include "MediaStream.h"
#include "MediaStreamPrivate.h"
#include "MediaTrackConstraints.h"
#include "NotImplemented.h"
#include <wtf/Functional.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

Ref<MediaStreamTrack> MediaStreamTrack::create(ScriptExecutionContext& context, MediaStreamTrackPrivate& privateTrack)
{
    return adoptRef(*new MediaStreamTrack(context, privateTrack));
}

MediaStreamTrack::MediaStreamTrack(ScriptExecutionContext& context, MediaStreamTrackPrivate& privateTrack)
    : RefCounted()
    , ActiveDOMObject(&context)
    , m_private(privateTrack)
{
    suspendIfNeeded();

    m_private->setClient(this);
}

MediaStreamTrack::~MediaStreamTrack()
{
    m_private->setClient(nullptr);
}

const AtomicString& MediaStreamTrack::kind() const
{
    static NeverDestroyed<AtomicString> audioKind("audio", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> videoKind("video", AtomicString::ConstructFromLiteral);

    if (m_private->type() == RealtimeMediaSource::Audio)
        return audioKind;
    return videoKind;
}

const String& MediaStreamTrack::id() const
{
    return m_private->id();
}

const String& MediaStreamTrack::label() const
{
    return m_private->label();
}

bool MediaStreamTrack::enabled() const
{
    return m_private->enabled();
}

void MediaStreamTrack::setEnabled(bool enabled)
{
    m_private->setEnabled(enabled);
}

bool MediaStreamTrack::muted() const
{
    return m_private->muted();
}

bool MediaStreamTrack::readonly() const
{
    return m_private->readonly();
}

bool MediaStreamTrack::remote() const
{
    return m_private->remote();
}

const AtomicString& MediaStreamTrack::readyState() const
{
    static NeverDestroyed<AtomicString> endedState("ended", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> liveState("live", AtomicString::ConstructFromLiteral);

    return ended() ? endedState : liveState;
}

bool MediaStreamTrack::ended() const
{
    return m_private->ended();
}

RefPtr<MediaStreamTrack> MediaStreamTrack::clone()
{
    return MediaStreamTrack::create(*scriptExecutionContext(), *m_private->clone());
}

void MediaStreamTrack::stopProducingData()
{
    // NOTE: this method is called when the "stop" method is called from JS, using
    // the "ImplementedAs" IDL attribute. This is done because ActiveDOMObject requires
    // a "stop" method.

    if (remote() || ended())
        return;

    m_private->endTrack();
}

RefPtr<MediaTrackConstraints> MediaStreamTrack::getConstraints() const
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=122428
    notImplemented();
    return 0;
}

RefPtr<MediaSourceStates> MediaStreamTrack::states() const
{
    return MediaSourceStates::create(m_private->states());
}

RefPtr<MediaStreamCapabilities> MediaStreamTrack::getCapabilities() const
{
    // The source may be shared by multiple tracks, so its states is not necessarily
    // in sync with the track state. A track that has ended always has a source
    // type of "none".
    RefPtr<RealtimeMediaSourceCapabilities> sourceCapabilities = m_private->capabilities();
    if (ended())
        sourceCapabilities->setSourceType(RealtimeMediaSourceStates::None);
    
    return MediaStreamCapabilities::create(sourceCapabilities.release());
}

void MediaStreamTrack::applyConstraints(const Dictionary& constraints)
{
    m_constraints->initialize(constraints);
    m_private->applyConstraints(*m_constraints);
}

void MediaStreamTrack::applyConstraints(const MediaConstraints&)
{
    // FIXME: apply the new constraints to the track
    // https://bugs.webkit.org/show_bug.cgi?id=122428
}

void MediaStreamTrack::addObserver(MediaStreamTrack::Observer* observer)
{
    m_observers.append(observer);
}

void MediaStreamTrack::removeObserver(MediaStreamTrack::Observer* observer)
{
    size_t pos = m_observers.find(observer);
    if (pos != notFound)
        m_observers.remove(pos);
}

void MediaStreamTrack::trackEnded()
{
    dispatchEvent(Event::create(eventNames().endedEvent, false, false));

    for (auto& observer : m_observers)
        observer->trackDidEnd();

    configureTrackRendering();
}
    
void MediaStreamTrack::trackMutedChanged()
{
    AtomicString eventType = muted() ? eventNames().muteEvent : eventNames().unmuteEvent;
    dispatchEvent(Event::create(eventType, false, false));

    configureTrackRendering();
}

void MediaStreamTrack::configureTrackRendering()
{
    // 4.3.1
    // ... media from the source only flows when a MediaStreamTrack object is both unmuted and enabled
}

void MediaStreamTrack::stop()
{
    stopProducingData();
}

const char* MediaStreamTrack::activeDOMObjectName() const
{
    return "MediaStreamTrack";
}

bool MediaStreamTrack::canSuspendForPageCache() const
{
    // FIXME: We should try and do better here.
    return false;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
