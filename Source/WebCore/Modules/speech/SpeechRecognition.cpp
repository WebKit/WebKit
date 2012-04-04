/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SCRIPTED_SPEECH)

#include "SpeechRecognition.h"

#include "Document.h"
#include "Page.h"
#include "SpeechRecognitionController.h"
#include "SpeechRecognitionError.h"
#include "SpeechRecognitionEvent.h"

namespace WebCore {

PassRefPtr<SpeechRecognition> SpeechRecognition::create(ScriptExecutionContext* context)
{
    RefPtr<SpeechRecognition> speechRecognition(adoptRef(new SpeechRecognition(context)));
    speechRecognition->suspendIfNeeded();
    return speechRecognition.release();
}

void SpeechRecognition::start()
{
    ASSERT(m_controller); // FIXME: Spec should say what to do if we are already started.
    m_controller->start(this, m_grammars.get(), m_lang, m_continuous);
}

void SpeechRecognition::stopFunction()
{
    ASSERT(m_controller);
    m_controller->stop(this); // FIXME: Spec should say what to do if we are not started.
}

void SpeechRecognition::abort()
{
    ASSERT(m_controller);
    m_controller->abort(this); // FIXME: Spec should say what to do if we are not started.
}

void SpeechRecognition::didStartAudio()
{
    // FIXME: The spec should specify whether these events can bubble and are cancelable.
    dispatchEvent(Event::create(eventNames().audiostartEvent, /*canBubble=*/false, /*cancelable=*/false));
}

void SpeechRecognition::didStartSound()
{
    dispatchEvent(Event::create(eventNames().soundstartEvent, /*canBubble=*/false, /*cancelable=*/false));
}

void SpeechRecognition::didStartSpeech()
{
    dispatchEvent(Event::create(eventNames().speechstartEvent, /*canBubble=*/false, /*cancelable=*/false));
}

void SpeechRecognition::didEndSpeech()
{
    dispatchEvent(Event::create(eventNames().speechendEvent, /*canBubble=*/false, /*cancelable=*/false));
}

void SpeechRecognition::didEndSound()
{
    dispatchEvent(Event::create(eventNames().soundendEvent, /*canBubble=*/false, /*cancelable=*/false));
}

void SpeechRecognition::didEndAudio()
{
    dispatchEvent(Event::create(eventNames().audioendEvent, /*canBubble=*/false, /*cancelable=*/false));
}

void SpeechRecognition::didReceiveResult(PassRefPtr<SpeechRecognitionResult> result, unsigned long resultIndex, PassRefPtr<SpeechRecognitionResultList> resultHistory)
{
    dispatchEvent(SpeechRecognitionEvent::createResult(result, resultIndex, resultHistory));
}

void SpeechRecognition::didReceiveNoMatch(PassRefPtr<SpeechRecognitionResult> result)
{
    dispatchEvent(SpeechRecognitionEvent::createNoMatch(result));
}

void SpeechRecognition::didDeleteResult(unsigned resultIndex, PassRefPtr<SpeechRecognitionResultList> resultHistory)
{
    dispatchEvent(SpeechRecognitionEvent::createResultDeleted(resultIndex, resultHistory));
}

void SpeechRecognition::didReceiveError(PassRefPtr<SpeechRecognitionError> error)
{
    dispatchEvent(SpeechRecognitionEvent::createError(error));
}

void SpeechRecognition::didStart()
{
    dispatchEvent(Event::create(eventNames().startEvent, /*canBubble=*/false, /*cancelable=*/false));
}

void SpeechRecognition::didEnd()
{
    dispatchEvent(Event::create(eventNames().endEvent, /*canBubble=*/false, /*cancelable=*/false));
}

const AtomicString& SpeechRecognition::interfaceName() const
{
    return eventNames().interfaceForSpeechRecognition;
}

ScriptExecutionContext* SpeechRecognition::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

SpeechRecognition::SpeechRecognition(ScriptExecutionContext* context)
    : ActiveDOMObject(context, this)
    , m_grammars(SpeechGrammarList::create()) // FIXME: The spec is not clear on the default value for the grammars attribute.
    , m_continuous(false)
    , m_controller(0)
{
    ASSERT(scriptExecutionContext()->isDocument());
    Document* document = static_cast<Document*>(scriptExecutionContext());

    Page* page = document->page();
    ASSERT(page);

    m_controller = SpeechRecognitionController::from(page);
    ASSERT(m_controller);

    // FIXME: Need to hook up with Page to get notified when the visibility changes.
}

SpeechRecognition::~SpeechRecognition()
{
}

} // namespace WebCore

#endif // ENABLE(SCRIPTED_SPEECH)
