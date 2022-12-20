/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DOMAudioSession.h"

#if ENABLE(DOM_AUDIO_SESSION)

#include "AudioSession.h"
#include "Document.h"
#include "EventNames.h"
#include "PlatformMediaSessionManager.h"

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DOMAudioSession);

static inline AudioSessionCategory fromDOMAudioSessionType(DOMAudioSession::Type type)
{
    switch (type) {
    case DOMAudioSession::Type::Auto:
        return AudioSessionCategory::None;
    case DOMAudioSession::Type::Playback:
        return AudioSessionCategory::MediaPlayback;
    case DOMAudioSession::Type::Transient:
        return AudioSessionCategory::AmbientSound;
    case DOMAudioSession::Type::TransientSolo:
        return AudioSessionCategory::SoloAmbientSound;
    case DOMAudioSession::Type::Ambient:
        return AudioSessionCategory::AmbientSound;
    case DOMAudioSession::Type::PlayAndRecord:
        return AudioSessionCategory::PlayAndRecord;
        break;
    };

    ASSERT_NOT_REACHED();
    return AudioSessionCategory::None;
}

Ref<DOMAudioSession> DOMAudioSession::create(ScriptExecutionContext* context)
{
    auto audioSession = adoptRef(*new DOMAudioSession(context));
    audioSession->suspendIfNeeded();
    return audioSession;
}

DOMAudioSession::DOMAudioSession(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
{
    AudioSession::sharedSession().addInterruptionObserver(*this);
}

DOMAudioSession::~DOMAudioSession()
{
    AudioSession::sharedSession().removeInterruptionObserver(*this);
}

ExceptionOr<void> DOMAudioSession::setType(Type type)
{
    auto* document = downcast<Document>(scriptExecutionContext());
    if (!document)
        return Exception { InvalidStateError };

    document->topDocument().setAudioSessionType(type);

    auto categoryOverride = fromDOMAudioSessionType(type);
    AudioSession::sharedSession().setCategoryOverride(categoryOverride);

    if (categoryOverride == AudioSessionCategory::None)
        PlatformMediaSessionManager::updateAudioSessionCategoryIfNecessary();

    return { };
}

DOMAudioSession::Type DOMAudioSession::type() const
{
    auto* document = downcast<Document>(scriptExecutionContext());
    return document ? document->topDocument().audioSessionType() : DOMAudioSession::Type::Auto;
}

static DOMAudioSession::State computeAudioSessionState()
{
    if (AudioSession::sharedSession().isInterrupted())
        return DOMAudioSession::State::Interrupted;

    if (!AudioSession::sharedSession().isActive())
        return DOMAudioSession::State::Inactive;

    return DOMAudioSession::State::Active;
}

DOMAudioSession::State DOMAudioSession::state() const
{
    if (!m_state)
        m_state = computeAudioSessionState();
    return *m_state;
}

void DOMAudioSession::stop()
{
}

const char* DOMAudioSession::activeDOMObjectName() const
{
    return "AudioSession";
}

bool DOMAudioSession::virtualHasPendingActivity() const
{
    return hasEventListeners(eventNames().statechangeEvent);
}

void DOMAudioSession::beginAudioSessionInterruption()
{
    scheduleStateChangeEvent();
}

void DOMAudioSession::endAudioSessionInterruption(AudioSession::MayResume)
{
    scheduleStateChangeEvent();
}

void DOMAudioSession::activeStateChanged()
{
    scheduleStateChangeEvent();
}

void DOMAudioSession::scheduleStateChangeEvent()
{
    if (m_hasScheduleStateChangeEvent)
        return;

    m_hasScheduleStateChangeEvent = true;
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this] {
        if (isContextStopped())
            return;

        m_hasScheduleStateChangeEvent = false;
        auto newState = computeAudioSessionState();

        if (m_state && *m_state == newState)
            return;

        m_state = newState;
        dispatchEvent(Event::create(eventNames().statechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

} // namespace WebCore

#endif // ENABLE(DOM_AUDIO_SESSION)
