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

#include "Document.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameDestructionObserverInlines.h"
#include "Page.h"
#include "PlatformSpeechSynthesisVoice.h"
#include "PlatformSpeechSynthesizer.h"
#include "SpeechSynthesisErrorEvent.h"
#include "SpeechSynthesisEvent.h"
#include "SpeechSynthesisUtterance.h"
#include "UserGestureIndicator.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SpeechSynthesis);

Ref<SpeechSynthesis>SpeechSynthesis::create(ScriptExecutionContext& context)
{
    return adoptRef(*new SpeechSynthesis(context));
}

SpeechSynthesis::SpeechSynthesis(ScriptExecutionContext& context)
    : ContextDestructionObserver(&context)
    , m_currentSpeechUtterance(nullptr)
    , m_isPaused(false)
    , m_restrictions(NoRestrictions)
    , m_speechSynthesisClient(nullptr)
{
    if (context.isDocument()) {
#if PLATFORM(IOS_FAMILY)
        if (downcast<Document>(context).audioPlaybackRequiresUserGesture())
            m_restrictions = RequireUserGestureForSpeechStartRestriction;
#endif
        m_speechSynthesisClient = downcast<Document>(context).frame()->page()->speechSynthesisClient();
    }

    if (m_speechSynthesisClient) {
        m_speechSynthesisClient->setObserver(*this);
        m_speechSynthesisClient->resetState();
    }
}

SpeechSynthesis::~SpeechSynthesis() = default;

void SpeechSynthesis::setPlatformSynthesizer(std::unique_ptr<PlatformSpeechSynthesizer> synthesizer)
{
    m_platformSpeechSynthesizer = WTFMove(synthesizer);
    m_voiceList.clear();
    m_currentSpeechUtterance = nullptr;
    m_utteranceQueue.clear();
    m_isPaused = false;
    m_speechSynthesisClient = nullptr;
}

void SpeechSynthesis::voicesDidChange()
{
    m_voiceList.clear();
    dispatchEvent(Event::create(eventNames().voiceschangedEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

PlatformSpeechSynthesizer& SpeechSynthesis::ensurePlatformSpeechSynthesizer()
{
    if (!m_platformSpeechSynthesizer)
        m_platformSpeechSynthesizer = makeUnique<PlatformSpeechSynthesizer>(this);
    return *m_platformSpeechSynthesizer;
}

const Vector<Ref<SpeechSynthesisVoice>>& SpeechSynthesis::getVoices()
{
    if (!m_voiceList.isEmpty())
        return m_voiceList;

    // If the voiceList is empty, that's the cue to get the voices from the platform again.
    auto& voiceList = m_speechSynthesisClient ? m_speechSynthesisClient->voiceList() : ensurePlatformSpeechSynthesizer().voiceList();
    m_voiceList = voiceList.map([](auto& voice) {
        return SpeechSynthesisVoice::create(*voice);
    });
    return m_voiceList;
}

bool SpeechSynthesis::speaking() const
{
    // If we have a current speech utterance, then that means we're assumed to be in a speaking state.
    // This state is independent of whether the utterance happens to be paused.
    return m_currentSpeechUtterance;
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
    m_currentSpeechUtterance = &utterance;
    m_isPaused = false;

    if (m_speechSynthesisClient)
        m_speechSynthesisClient->speak(utterance.platformUtterance());
    else
        ensurePlatformSpeechSynthesizer().speak(utterance.platformUtterance());
}

void SpeechSynthesis::speak(SpeechSynthesisUtterance& utterance)
{
    // Like Audio, we should require that the user interact to start a speech synthesis session.
#if PLATFORM(IOS_FAMILY)
    if (UserGestureIndicator::processingUserGesture())
        removeBehaviorRestriction(RequireUserGestureForSpeechStartRestriction);
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
    RefPtr<SpeechSynthesisUtterance> current = m_currentSpeechUtterance;
    m_utteranceQueue.clear();
    if (m_speechSynthesisClient) {
        m_speechSynthesisClient->cancel();
        // If we wait for cancel to callback speakingErrorOccurred, then m_currentSpeechUtterance will be null
        // and the event won't be processed. Instead we process the error immediately.
        speakingErrorOccurred();
        m_currentSpeechUtterance = nullptr;
    } else if (m_platformSpeechSynthesizer) {
        m_platformSpeechSynthesizer->cancel();
        // The platform should have called back immediately and cleared the current utterance.
        ASSERT(!m_currentSpeechUtterance);
    }
    current = nullptr;
}

void SpeechSynthesis::pause()
{
    if (!m_isPaused) {
        if (m_speechSynthesisClient)
            m_speechSynthesisClient->pause();
        else if (m_platformSpeechSynthesizer)
            m_platformSpeechSynthesizer->pause();
    }
}

void SpeechSynthesis::resume()
{
    if (m_currentSpeechUtterance) {
        if (m_speechSynthesisClient)
            m_speechSynthesisClient->resume();
        else if (m_platformSpeechSynthesizer)
            m_platformSpeechSynthesizer->resume();
    }
}

void SpeechSynthesis::fireEvent(const AtomString& type, SpeechSynthesisUtterance& utterance, unsigned long charIndex, unsigned long charLength, const String& name) const
{
    utterance.dispatchEvent(SpeechSynthesisEvent::create(type, { &utterance, charIndex, charLength, static_cast<float>((MonotonicTime::now() - utterance.startTime()).seconds()), name }));
}

void SpeechSynthesis::fireErrorEvent(const AtomString& type, SpeechSynthesisUtterance& utterance, SpeechSynthesisErrorCode errorCode) const
{
    utterance.dispatchEvent(SpeechSynthesisErrorEvent::create(type, { { &utterance, 0, 0, static_cast<float>((MonotonicTime::now() - utterance.startTime()).seconds()), { } }, errorCode }));
}

void SpeechSynthesis::handleSpeakingCompleted(SpeechSynthesisUtterance& utterance, bool errorOccurred)
{
    ASSERT(m_currentSpeechUtterance);
    Ref<SpeechSynthesisUtterance> protect(utterance);

    m_currentSpeechUtterance = nullptr;

    if (errorOccurred)
        fireErrorEvent(eventNames().errorEvent, utterance, SpeechSynthesisErrorCode::Canceled);
    else
        fireEvent(eventNames().endEvent, utterance, 0, 0, String());
    
    if (m_utteranceQueue.size()) {
        Ref<SpeechSynthesisUtterance> firstUtterance = m_utteranceQueue.takeFirst();
        ASSERT(&utterance == firstUtterance.ptr());

        // Start the next job if there is one pending.
        if (!m_utteranceQueue.isEmpty())
            startSpeakingImmediately(m_utteranceQueue.first());
    }
}

void SpeechSynthesis::boundaryEventOccurred(PlatformSpeechSynthesisUtterance& utterance, SpeechBoundary boundary, unsigned charIndex, unsigned charLength)
{
    static NeverDestroyed<const String> wordBoundaryString(MAKE_STATIC_STRING_IMPL("word"));
    static NeverDestroyed<const String> sentenceBoundaryString(MAKE_STATIC_STRING_IMPL("sentence"));

    ASSERT(utterance.client());

    switch (boundary) {
    case SpeechBoundary::SpeechWordBoundary:
        fireEvent(eventNames().boundaryEvent, static_cast<SpeechSynthesisUtterance&>(*utterance.client()), charIndex, charLength, wordBoundaryString);
        break;
    case SpeechBoundary::SpeechSentenceBoundary:
        fireEvent(eventNames().boundaryEvent, static_cast<SpeechSynthesisUtterance&>(*utterance.client()), charIndex, charLength, sentenceBoundaryString);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void SpeechSynthesis::didStartSpeaking()
{
    if (!m_currentSpeechUtterance)
        return;
    didStartSpeaking(*m_currentSpeechUtterance->platformUtterance());
}

void SpeechSynthesis::didFinishSpeaking()
{
    if (!m_currentSpeechUtterance)
        return;
    didFinishSpeaking(*m_currentSpeechUtterance->platformUtterance());
}

void SpeechSynthesis::didPauseSpeaking()
{
    if (!m_currentSpeechUtterance)
        return;
    didPauseSpeaking(*m_currentSpeechUtterance->platformUtterance());
}

void SpeechSynthesis::didResumeSpeaking()
{
    if (!m_currentSpeechUtterance)
        return;
    didResumeSpeaking(*m_currentSpeechUtterance->platformUtterance());
}

void SpeechSynthesis::speakingErrorOccurred()
{
    if (!m_currentSpeechUtterance)
        return;
    speakingErrorOccurred(*m_currentSpeechUtterance->platformUtterance());
}

void SpeechSynthesis::boundaryEventOccurred(bool wordBoundary, unsigned charIndex, unsigned charLength)
{
    if (!m_currentSpeechUtterance)
        return;
    boundaryEventOccurred(*m_currentSpeechUtterance->platformUtterance(), wordBoundary ? SpeechBoundary::SpeechWordBoundary : SpeechBoundary::SpeechSentenceBoundary, charIndex, charLength);
}

void SpeechSynthesis::voicesChanged()
{
    voicesDidChange();
}

void SpeechSynthesis::didStartSpeaking(PlatformSpeechSynthesisUtterance& utterance)
{
    if (utterance.client())
        fireEvent(eventNames().startEvent, static_cast<SpeechSynthesisUtterance&>(*utterance.client()), 0, 0, String());
}

void SpeechSynthesis::didPauseSpeaking(PlatformSpeechSynthesisUtterance& utterance)
{
    m_isPaused = true;
    if (utterance.client())
        fireEvent(eventNames().pauseEvent, static_cast<SpeechSynthesisUtterance&>(*utterance.client()), 0, 0, String());
}

void SpeechSynthesis::didResumeSpeaking(PlatformSpeechSynthesisUtterance& utterance)
{
    m_isPaused = false;
    if (utterance.client())
        fireEvent(eventNames().resumeEvent, static_cast<SpeechSynthesisUtterance&>(*utterance.client()), 0, 0, String());
}

void SpeechSynthesis::didFinishSpeaking(PlatformSpeechSynthesisUtterance& utterance)
{
    if (utterance.client())
        handleSpeakingCompleted(static_cast<SpeechSynthesisUtterance&>(*utterance.client()), false);
}

void SpeechSynthesis::speakingErrorOccurred(PlatformSpeechSynthesisUtterance& utterance)
{
    if (utterance.client())
        handleSpeakingCompleted(static_cast<SpeechSynthesisUtterance&>(*utterance.client()), true);
}

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)
