/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "SpeechSynthesis.h"

#if ENABLE(SPEECH_SYNTHESIS)

#include "ContextDestructionObserverInlines.h"
#include "DocumentPage.h"
#include "EventNames.h"
#include "EventTargetInlines.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PlatformSpeechSynthesisVoice.h"
#include "PlatformSpeechSynthesizer.h"
#include "ScriptTrackingPrivacyCategory.h"
#include "SpeechSynthesisErrorEvent.h"
#include "SpeechSynthesisEvent.h"
#include "SpeechSynthesisUtterance.h"
#include "UserGestureIndicator.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SpeechSynthesis);

Ref<SpeechSynthesis> SpeechSynthesis::create(ScriptExecutionContext& context)
{
    auto synthesis = adoptRef(*new SpeechSynthesis(context));
    synthesis->suspendIfNeeded();
    return synthesis;
}

SpeechSynthesis::SpeechSynthesis(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
    , m_currentSpeechUtterance(nullptr)
    , m_isPaused(false)
    , m_restrictions({ })
    , m_speechSynthesisClient(nullptr)
{
    if (RefPtr document = dynamicDowncast<Document>(context)) {
#if PLATFORM(IOS_FAMILY)
        if (document->requiresUserGestureForAudioPlayback())
            m_restrictions = BehaviorRestrictionFlags::RequireUserGestureForSpeechStart;
#endif
        m_speechSynthesisClient = document->frame()->page()->speechSynthesisClient();
    }

    if (RefPtr speechSynthesisClient = m_speechSynthesisClient.get()) {
        speechSynthesisClient->setObserver(*this);
        speechSynthesisClient->resetState();
    }
}

SpeechSynthesis::~SpeechSynthesis() = default;

void SpeechSynthesis::setPlatformSynthesizer(Ref<PlatformSpeechSynthesizer>&& synthesizer)
{
    m_platformSpeechSynthesizer = synthesizer.ptr();
    if (m_voiceList)
        m_voiceList = std::nullopt;
    m_utteranceQueue.clear();
    // Finish current utterance.
    speakingErrorOccurred();
    m_isPaused = false;
    m_speechSynthesisClient = nullptr;
}

void SpeechSynthesis::voicesDidChange()
{
    if (m_voiceList)
        m_voiceList = std::nullopt;

    dispatchEvent(Event::create(eventNames().voiceschangedEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

PlatformSpeechSynthesizer& SpeechSynthesis::ensurePlatformSpeechSynthesizer()
{
    if (!m_platformSpeechSynthesizer)
        m_platformSpeechSynthesizer = PlatformSpeechSynthesizer::create(*this);
    return *m_platformSpeechSynthesizer;
}

const Vector<Ref<SpeechSynthesisVoice>>& SpeechSynthesis::getVoices()
{
    if (RefPtr context = scriptExecutionContext()) {
        if (context->requiresScriptTrackingPrivacyProtection(ScriptTrackingPrivacyCategory::Speech)) {
            static NeverDestroyed<Vector<Ref<SpeechSynthesisVoice>>> emptyVoicesList;
            return emptyVoicesList.get();
        }
    }

    if (m_voiceList)
        return *m_voiceList;

    // If the voiceList is empty, that's the cue to get the voices from the platform again.
    RefPtr speechSynthesisClient = m_speechSynthesisClient.get();
    auto& voiceList = speechSynthesisClient ? speechSynthesisClient->voiceList() : protect(ensurePlatformSpeechSynthesizer())->voiceList();
    m_voiceList = voiceList.map([](auto& voice) {
        return SpeechSynthesisVoice::create(Ref { voice });
    });

    return *m_voiceList;
}

bool SpeechSynthesis::speaking() const
{
    // If we have a current speech utterance, then that means we're assumed to be in a speaking state.
    // This state is independent of whether the utterance happens to be paused.
    return !!m_currentSpeechUtterance;
}

bool SpeechSynthesis::pending() const
{
    // This is true if there are any utterances that have not started.
    // That means there will be more than one in the queue.
    return m_utteranceQueue.size() > 1;
}

bool SpeechSynthesis::paused() const
{
    return m_isPaused;
}

void SpeechSynthesis::startSpeakingImmediately(SpeechSynthesisUtterance& utterance)
{
    utterance.setStartTime(MonotonicTime::now());
    m_currentSpeechUtterance = makeUnique<SpeechSynthesisUtteranceActivity>(Ref { utterance });
    m_isPaused = false;

    if (RefPtr speechSynthesisClient = m_speechSynthesisClient.get())
        speechSynthesisClient->speak(&utterance.platformUtterance());
    else
        protect(ensurePlatformSpeechSynthesizer())->speak(&utterance.platformUtterance());
}

void SpeechSynthesis::speak(SpeechSynthesisUtterance& utterance)
{
    // Like Audio, we should require that the user interact to start a speech synthesis session.
#if PLATFORM(IOS_FAMILY)
    if (UserGestureIndicator::processingUserGesture())
        removeBehaviorRestriction(BehaviorRestrictionFlags::RequireUserGestureForSpeechStart);
    else if (userGestureRequiredForSpeechStart())
        return;
#endif

    m_utteranceQueue.append(utterance);
    // If the queue was empty, speak this immediately and add it to the queue.
    if (m_utteranceQueue.size() == 1)
        startSpeakingImmediately(m_utteranceQueue.first());
}

void SpeechSynthesis::cancel()
{
    // Remove all the items from the utterance queue.
    // Hold on to the current utterance so the platform synthesizer can have a chance to clean up.
    RefPtr current = currentSpeechUtterance();
    // Clear m_utteranceQueue before calling cancel to avoid picking up new utterances
    // on completion callback
    auto utteranceQueue = WTF::move(m_utteranceQueue);
    if (RefPtr speechSynthesisClient = m_speechSynthesisClient.get()) {
        speechSynthesisClient->cancel();
        // If we wait for cancel to callback speakingErrorOccurred, then m_currentSpeechUtterance will be null
        // and the event won't be processed. Instead we process the error immediately.
        speakingErrorOccurred();
        m_currentSpeechUtterance = nullptr;
    } else if (RefPtr platformSpeechSynthesizer = m_platformSpeechSynthesizer)
        platformSpeechSynthesizer->cancel();

    // Trigger canceled events for queued utterances
    while (!utteranceQueue.isEmpty()) {
        const auto utterance = utteranceQueue.takeFirst();
        // Current utterance is handled in platform cancel()
        if (current.get() != utterance.ptr())
            utterance.get().errorEventOccurred(eventNames().errorEvent, SpeechSynthesisErrorCode::Canceled);
    }

    current = nullptr;
}

void SpeechSynthesis::pause()
{
    if (!m_isPaused) {
        if (RefPtr speechSynthesisClient = m_speechSynthesisClient.get())
            speechSynthesisClient->pause();
        else if (RefPtr platformSpeechSynthesizer = m_platformSpeechSynthesizer)
            platformSpeechSynthesizer->pause();
    }
}

void SpeechSynthesis::resumeSynthesis()
{
    if (m_currentSpeechUtterance) {
        if (RefPtr speechSynthesisClient = m_speechSynthesisClient.get())
            speechSynthesisClient->resume();
        else if (RefPtr platformSpeechSynthesizer = m_platformSpeechSynthesizer)
            platformSpeechSynthesizer->resume();
    }
}

void SpeechSynthesis::handleSpeakingCompleted(SpeechSynthesisUtterance& utterance, bool errorOccurred)
{
    ASSERT(m_currentSpeechUtterance);
    Ref<SpeechSynthesisUtterance> protect(utterance);

    m_currentSpeechUtterance = nullptr;

    if (errorOccurred)
        utterance.errorEventOccurred(eventNames().errorEvent, SpeechSynthesisErrorCode::Canceled);
    else
        utterance.eventOccurred(eventNames().endEvent, 0, 0, String());
    
    if (m_utteranceQueue.size()) {
        Ref<SpeechSynthesisUtterance> firstUtterance = m_utteranceQueue.takeFirst();
        ASSERT(&utterance == firstUtterance.ptr());

        // Start the next job if there is one pending.
        if (!m_utteranceQueue.isEmpty())
            startSpeakingImmediately(m_utteranceQueue.first());
    }
}

void SpeechSynthesis::boundaryEventOccurred(PlatformSpeechSynthesisUtterance& platformUtterance, SpeechBoundary boundary, unsigned charIndex, unsigned charLength)
{
    static NeverDestroyed<const String> wordBoundaryString(MAKE_STATIC_STRING_IMPL("word"));
    static NeverDestroyed<const String> sentenceBoundaryString(MAKE_STATIC_STRING_IMPL("sentence"));

    RefPtr client = platformUtterance.client();
    ASSERT(client);
    switch (boundary) {
    case SpeechBoundary::SpeechWordBoundary:
        client->eventOccurred(eventNames().boundaryEvent, charIndex, charLength, wordBoundaryString);
        break;
    case SpeechBoundary::SpeechSentenceBoundary:
        client->eventOccurred(eventNames().boundaryEvent, charIndex, charLength, sentenceBoundaryString);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void SpeechSynthesis::didStartSpeaking()
{
    if (!m_currentSpeechUtterance)
        return;
    didStartSpeaking(protect(currentSpeechUtterance())->platformUtterance());
}

void SpeechSynthesis::didFinishSpeaking()
{
    if (!m_currentSpeechUtterance)
        return;
    didFinishSpeaking(protect(currentSpeechUtterance())->platformUtterance());
}

void SpeechSynthesis::didPauseSpeaking()
{
    if (!m_currentSpeechUtterance)
        return;
    didPauseSpeaking(protect(currentSpeechUtterance())->platformUtterance());
}

void SpeechSynthesis::didResumeSpeaking()
{
    if (!m_currentSpeechUtterance)
        return;
    didResumeSpeaking(protect(currentSpeechUtterance())->platformUtterance());
}

void SpeechSynthesis::speakingErrorOccurred()
{
    if (!m_currentSpeechUtterance)
        return;
    speakingErrorOccurred(protect(currentSpeechUtterance())->platformUtterance());
}

void SpeechSynthesis::boundaryEventOccurred(bool wordBoundary, unsigned charIndex, unsigned charLength)
{
    if (!m_currentSpeechUtterance)
        return;
    boundaryEventOccurred(protect(currentSpeechUtterance())->platformUtterance(), wordBoundary ? SpeechBoundary::SpeechWordBoundary : SpeechBoundary::SpeechSentenceBoundary, charIndex, charLength);
}

void SpeechSynthesis::voicesChanged()
{
    voicesDidChange();
}

void SpeechSynthesis::didStartSpeaking(PlatformSpeechSynthesisUtterance& utterance)
{
    if (utterance.client())
        downcast<SpeechSynthesisUtterance>(*utterance.client()).eventOccurred(eventNames().startEvent, 0, 0, String());
}

void SpeechSynthesis::didPauseSpeaking(PlatformSpeechSynthesisUtterance& utterance)
{
    m_isPaused = true;
    if (utterance.client())
        downcast<SpeechSynthesisUtterance>(*utterance.client()).eventOccurred(eventNames().pauseEvent, 0, 0, String());
}

void SpeechSynthesis::didResumeSpeaking(PlatformSpeechSynthesisUtterance& utterance)
{
    m_isPaused = false;
    if (utterance.client())
        downcast<SpeechSynthesisUtterance>(*utterance.client()).eventOccurred(eventNames().resumeEvent, 0, 0, String());
}

void SpeechSynthesis::didFinishSpeaking(PlatformSpeechSynthesisUtterance& utterance)
{
    if (utterance.client())
        handleSpeakingCompleted(downcast<SpeechSynthesisUtterance>(*utterance.client()), false);
}

void SpeechSynthesis::speakingErrorOccurred(PlatformSpeechSynthesisUtterance& utterance)
{
    if (utterance.client())
        handleSpeakingCompleted(downcast<SpeechSynthesisUtterance>(*utterance.client()), true);
}

SpeechSynthesisUtterance* SpeechSynthesis::currentSpeechUtterance()
{
    return m_currentSpeechUtterance ? &m_currentSpeechUtterance->utterance() : nullptr;
}

void SpeechSynthesis::simulateVoicesListChange()
{
    if (m_speechSynthesisClient) {
        voicesChanged();
        return;
    }

    if (m_platformSpeechSynthesizer)
        voicesDidChange();
}

bool SpeechSynthesis::virtualHasPendingActivity() const
{
    return m_voiceList && m_hasEventListener;
}

ScriptExecutionContext* SpeechSynthesis::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void SpeechSynthesis::eventListenersDidChange()
{
    m_hasEventListener = hasEventListeners(eventNames().voiceschangedEvent);
}

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)
