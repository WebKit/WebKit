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

#include "Dictionary.h"
#include "Event.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "ExceptionCodePlaceholder.h"
#include "MediaConstraintsImpl.h"
#include "MediaSourceSettings.h"
#include "MediaStream.h"
#include "MediaStreamPrivate.h"
#include "MediaTrackConstraints.h"
#include "NotImplemented.h"
#include "ScriptExecutionContext.h"
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

    m_private->addObserver(*this);
}

MediaStreamTrack::~MediaStreamTrack()
{
    m_private->removeObserver(*this);
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

auto MediaStreamTrack::readyState() const -> State
{
    return ended() ? State::Ended : State::Live;
}

bool MediaStreamTrack::ended() const
{
    return m_ended || m_private->ended();
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

    // http://w3c.github.io/mediacapture-main/#widl-MediaStreamTrack-stop-void
    // 4.3.3.2 Methods
    // When a MediaStreamTrack object's stop() method is invoked, the User Agent must run following steps:
    // 1. Let track be the current MediaStreamTrack object.
    // 2. If track is sourced by a non-local source, then abort these steps.
    if (remote() || ended())
        return;

    // 3. Notify track's source that track is ended so that the source may be stopped, unless other
    // MediaStreamTrack objects depend on it.
    // 4. Set track's readyState attribute to ended.

    // Set m_ended to true before telling the private to stop so we do not fire an 'ended' event.
    m_ended = true;

    m_private->endTrack();
}

RefPtr<MediaTrackConstraints> MediaStreamTrack::getConstraints() const
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=122428
    notImplemented();
    return 0;
}

RefPtr<MediaSourceSettings> MediaStreamTrack::getSettings() const
{
    return MediaSourceSettings::create(m_private->settings());
}

RefPtr<RealtimeMediaSourceCapabilities> MediaStreamTrack::getCapabilities() const
{
    return m_private->capabilities();
}

void MediaStreamTrack::applyConstraints(const Dictionary& constraints)
{
    // FIXME: Implement correctly. https://bugs.webkit.org/show_bug.cgi?id=160579

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

void MediaStreamTrack::trackEnded(MediaStreamTrackPrivate&)
{
    // http://w3c.github.io/mediacapture-main/#life-cycle
    // When a MediaStreamTrack track ends for any reason other than the stop() method being invoked, the User Agent must queue a task that runs the following steps:
    // 1. If the track's readyState attribute has the value ended already, then abort these steps.
    if (m_ended)
        return;

    // 2. Set track's readyState attribute to ended.
    m_ended = true;

    if (scriptExecutionContext()->activeDOMObjectsAreSuspended() || scriptExecutionContext()->activeDOMObjectsAreStopped())
        return;

    // 3. Notify track's source that track is ended so that the source may be stopped, unless other MediaStreamTrack objects depend on it.
    // 4. Fire a simple event named ended at the object.
    dispatchEvent(Event::create(eventNames().endedEvent, false, false));

    for (auto& observer : m_observers)
        observer->trackDidEnd();

    configureTrackRendering();
}
    
void MediaStreamTrack::trackMutedChanged(MediaStreamTrackPrivate&)
{
    if (scriptExecutionContext()->activeDOMObjectsAreSuspended() || scriptExecutionContext()->activeDOMObjectsAreStopped())
        return;

    AtomicString eventType = muted() ? eventNames().muteEvent : eventNames().unmuteEvent;
    dispatchEvent(Event::create(eventType, false, false));

    configureTrackRendering();
}

void MediaStreamTrack::trackSettingsChanged(MediaStreamTrackPrivate&)
{
    configureTrackRendering();
}

void MediaStreamTrack::trackEnabledChanged(MediaStreamTrackPrivate&)
{
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

bool MediaStreamTrack::canSuspendForDocumentSuspension() const
{
    // FIXME: We should try and do better here.
    return false;
}

AudioSourceProvider* MediaStreamTrack::audioSourceProvider()
{
    return m_private->audioSourceProvider();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
