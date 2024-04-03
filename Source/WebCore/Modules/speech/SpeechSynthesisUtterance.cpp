/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "SpeechSynthesisUtterance.h"

#if ENABLE(SPEECH_SYNTHESIS)

#include "ContextDestructionObserverInlines.h"
#include "EventNames.h"
#include "SpeechSynthesisErrorEvent.h"
#include "SpeechSynthesisEvent.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SpeechSynthesisUtterance);
    
Ref<SpeechSynthesisUtterance> SpeechSynthesisUtterance::create(ScriptExecutionContext& context, const String& text)
{
    auto utterance = adoptRef(*new SpeechSynthesisUtterance(context, text, { }));
    utterance->suspendIfNeeded();
    return utterance;
}

Ref<SpeechSynthesisUtterance> SpeechSynthesisUtterance::create(ScriptExecutionContext& context, const String& text, SpeechSynthesisUtterance::UtteranceCompletionHandler&& completion)
{
    auto utterance = adoptRef(*new SpeechSynthesisUtterance(context, text, WTFMove(completion)));
    utterance->suspendIfNeeded();
    return utterance;
}

SpeechSynthesisUtterance::SpeechSynthesisUtterance(ScriptExecutionContext& context, const String& text, UtteranceCompletionHandler&& completion)
    : ActiveDOMObject(&context)
    , m_platformUtterance(PlatformSpeechSynthesisUtterance::create(*this))
    , m_completionHandler(WTFMove(completion))
{
    m_platformUtterance->setText(text);
}

SpeechSynthesisUtterance::~SpeechSynthesisUtterance()
{
    m_platformUtterance->setClient(nullptr);
}

SpeechSynthesisVoice* SpeechSynthesisUtterance::voice() const
{
    return m_voice.get();
}

void SpeechSynthesisUtterance::setVoice(SpeechSynthesisVoice* voice)
{
    if (!voice)
        return;
    
    // Cache our own version of the SpeechSynthesisVoice so that we don't have to do some lookup
    // to go from the platform voice back to the speech synthesis voice in the read property.
    m_voice = voice;
    
    if (voice)
        m_platformUtterance->setVoice(voice->platformVoice());
}

void SpeechSynthesisUtterance::eventOccurred(const AtomString& type, unsigned long charIndex, unsigned long charLength, const String& name)
{
    if (m_completionHandler) {
        if (type == eventNames().endEvent)
            m_completionHandler(*this);

        return;
    }

    dispatchEvent(SpeechSynthesisEvent::create(type, { this, charIndex, charLength, static_cast<float>((MonotonicTime::now() - startTime()).seconds()), name }));
}

void SpeechSynthesisUtterance::errorEventOccurred(const AtomString& type, SpeechSynthesisErrorCode errorCode)
{
    if (m_completionHandler) {
        m_completionHandler(*this);
        return;
    }

    dispatchEvent(SpeechSynthesisErrorEvent::create(type, { { this, 0, 0, static_cast<float>((MonotonicTime::now() - startTime()).seconds()), { } }, errorCode }));
}

void SpeechSynthesisUtterance::incrementActivityCountForEventDispatch()
{
    ++m_activityCountForEventDispatch;
}

void SpeechSynthesisUtterance::decrementActivityCountForEventDispatch()
{
    --m_activityCountForEventDispatch;
}

const char* SpeechSynthesisUtterance::activeDOMObjectName() const
{
    return "SpeechSynthesisUtterance";
}

bool SpeechSynthesisUtterance::virtualHasPendingActivity() const
{
    return m_activityCountForEventDispatch && hasEventListeners();
}


} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)
