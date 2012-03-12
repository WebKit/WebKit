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

#ifndef SpeechRecognitionEvent_h
#define SpeechRecognitionEvent_h

#if ENABLE(SCRIPTED_SPEECH)

#include "Event.h"
#include "SpeechRecognitionError.h"
#include "SpeechRecognitionResult.h"
#include "SpeechRecognitionResultList.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class SpeechRecognitionError;
class SpeechRecognitionResult;
class SpeechRecognitionResultList;

struct SpeechRecognitionEventInit : public EventInit {
    SpeechRecognitionEventInit();

    RefPtr<SpeechRecognitionResult> result;
    RefPtr<SpeechRecognitionError> error;
    short resultIndex;
    RefPtr<SpeechRecognitionResultList> resultHistory;
};

class SpeechRecognitionEvent : public Event {
public:
    static PassRefPtr<SpeechRecognitionEvent> create();
    static PassRefPtr<SpeechRecognitionEvent> create(const AtomicString&, const SpeechRecognitionEventInit&);

    static PassRefPtr<SpeechRecognitionEvent> createResult(PassRefPtr<SpeechRecognitionResult>, short resultIndex, PassRefPtr<SpeechRecognitionResultList> resultHistory);
    static PassRefPtr<SpeechRecognitionEvent> createNoMatch(PassRefPtr<SpeechRecognitionResult>);
    static PassRefPtr<SpeechRecognitionEvent> createResultDeleted(short resultIndex, PassRefPtr<SpeechRecognitionResultList> resultHistory);
    static PassRefPtr<SpeechRecognitionEvent> createError(PassRefPtr<SpeechRecognitionError>);

    SpeechRecognitionResult* result() const { return m_result.get(); }
    SpeechRecognitionError* error() const { return m_error.get(); }
    short resultIndex() const { return m_resultIndex; } // FIXME: Spec says this should be short, but other indices are unsigned ints.
    SpeechRecognitionResultList* resultHistory() const { return m_resultHistory.get(); }

    // Event
    virtual const AtomicString& interfaceName() const OVERRIDE;

private:
    SpeechRecognitionEvent();
    SpeechRecognitionEvent(const AtomicString&, const SpeechRecognitionEventInit&);
    SpeechRecognitionEvent(const AtomicString& eventName, PassRefPtr<SpeechRecognitionResult>, short resultIndex, PassRefPtr<SpeechRecognitionResultList> resultHistory);
    SpeechRecognitionEvent(PassRefPtr<SpeechRecognitionError>);

    RefPtr<SpeechRecognitionResult> m_result;
    RefPtr<SpeechRecognitionError> m_error;
    short m_resultIndex;
    RefPtr<SpeechRecognitionResultList> m_resultHistory;
};

} // namespace WebCore

#endif // ENABLE(SCRIPTED_SPEECH)

#endif // SpeechRecognitionEvent_h
