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
#include "Page.h"
#include "PermissionsPolicy.h"
#include "PlatformMediaSessionManager.h"

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(DOMAudioSession);

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

    if (RefPtr document = downcast<Document>(context))
        document->setDOMAudioSession(audioSession.get());

    return audioSession;
}

DOMAudioSession::DOMAudioSession(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
    , m_applyTypeTimer(*this, &DOMAudioSession::applyType)
{
    AudioSession::protectedSharedSession()->addInterruptionObserver(*this);
}

DOMAudioSession::~DOMAudioSession()
{
    AudioSession::protectedSharedSession()->removeInterruptionObserver(*this);
}

static inline bool shouldUseDocumentType(DOMAudioSession::Type overrideType, DOMAudioSession::Type documentType)
{
    switch (documentType) {
    case DOMAudioSession::Type::Auto:
        return false;
    case DOMAudioSession::Type::Playback:
        return true;
    case DOMAudioSession::Type::Transient:
        return overrideType == DOMAudioSession::Type::Auto;
    case DOMAudioSession::Type::TransientSolo:
        return overrideType == DOMAudioSession::Type::Auto;
    case DOMAudioSession::Type::Ambient:
        return overrideType == DOMAudioSession::Type::Auto;
    case DOMAudioSession::Type::PlayAndRecord:
        return true;
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

ExceptionOr<void> DOMAudioSession::setType(Type type)
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (!document)
        return Exception { ExceptionCode::InvalidStateError };

    if (!PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Microphone, *document, PermissionsPolicy::ShouldReportViolation::No))
        return { };

    m_typeToApply = type;

    if (!isTypeBeingApplied())
        m_applyTypeTimer.startOneShot(0_s);

    return { };
}

void DOMAudioSession::applyType()
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (!document)
        return;

    if (document->audioSessionType() == m_typeToApply) {
        m_typeToApply = DOMAudioSession::Type::Auto;
        return;
    }

    document->setAudioSessionType(m_typeToApply);
    m_typeToApply = DOMAudioSession::Type::Auto;

    update();
}

void DOMAudioSession::update()
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (!document)
        return;
    RefPtr page = document->page();
    if (!page)
        return;

    if ((document->mediaState() & MediaProducer::MicrophoneCaptureMask)
        && document->audioSessionType() != DOMAudioSession::Type::Auto
        && document->audioSessionType() != DOMAudioSession::Type::PlayAndRecord)
        document->stopMediaCapture(MediaProducerMediaCaptureKind::Microphone);

    // FIXME: Handle out-of-process iframes.
    DOMAudioSession::Type overrideType = DOMAudioSession::Type::Auto;
    page->forEachDocument([&] (auto& document) {
        if (!(document.mediaState() & MediaProducerMediaState::IsPlayingAudio)
            && !(document.mediaState() & MediaProducer::MicrophoneCaptureMask))
            return;

        auto documentType = document.audioSessionType();
        if (shouldUseDocumentType(overrideType, documentType))
            overrideType = documentType;
    });


    auto categoryOverride = fromDOMAudioSessionType(overrideType);
    AudioSession::sharedSession().setCategoryOverride(categoryOverride);

    if (categoryOverride == AudioSessionCategory::None)
        PlatformMediaSessionManager::updateAudioSessionCategoryIfNecessary();
}

DOMAudioSession::Type DOMAudioSession::type() const
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (!document || !PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Microphone, *document, PermissionsPolicy::ShouldReportViolation::No))
        return DOMAudioSession::Type::Auto;

    return isTypeBeingApplied() ? m_typeToApply : document->audioSessionType();
}

static DOMAudioSession::State computeAudioSessionState(const Document& document)
{
    if (AudioSession::sharedSession().isInterrupted())
        return DOMAudioSession::State::Interrupted;

    if (!AudioSession::sharedSession().isActive())
        return DOMAudioSession::State::Inactive;

    auto mediaState = document.mediaState();
    return (mediaState & MediaProducerMediaState::HasActiveAudioCaptureDevice) || (mediaState & MediaProducerMediaState::IsPlayingAudio) ? DOMAudioSession::State::Active : DOMAudioSession::State::Inactive;
}

DOMAudioSession::State DOMAudioSession::state() const
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (!document || !PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Microphone, *document, PermissionsPolicy::ShouldReportViolation::No))
        return DOMAudioSession::State::Inactive;

    if (!m_state)
        m_state = computeAudioSessionState(*document);
    return *m_state;
}

void DOMAudioSession::stop()
{
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

void DOMAudioSession::audioSessionActiveStateChanged()
{
    scheduleStateChangeEvent();
}

void DOMAudioSession::scheduleStateChangeEvent()
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (document && !PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Microphone, *document, PermissionsPolicy::ShouldReportViolation::No))
        return;

    if (m_hasScheduleStateChangeEvent)
        return;

    m_hasScheduleStateChangeEvent = true;
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [this] {
        if (isContextStopped())
            return;

        Ref document = *downcast<Document>(scriptExecutionContext());

        m_hasScheduleStateChangeEvent = false;
        auto newState = computeAudioSessionState(document.get());

        if (m_state && *m_state == newState)
            return;

        m_state = newState;
        dispatchEvent(Event::create(eventNames().statechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

} // namespace WebCore

#endif // ENABLE(DOM_AUDIO_SESSION)
