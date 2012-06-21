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

#include "SpeechRecognitionError.h"

namespace WebCore {

PassRefPtr<SpeechRecognitionError> SpeechRecognitionError::create(Code code, const String& message)
{
    return adoptRef(new SpeechRecognitionError(code, message));
}

PassRefPtr<SpeechRecognitionError> SpeechRecognitionError::create(const AtomicString& eventName, const SpeechRecognitionErrorInit& initializer)
{
    return adoptRef(new SpeechRecognitionError(eventName, initializer));
}

SpeechRecognitionError::SpeechRecognitionError(Code code, const String& message)
    : Event(eventNames().errorEvent, /*canBubble=*/false, /*cancelable=*/false)
    , m_code(static_cast<unsigned short>(code))
    , m_message(message)
{
}

SpeechRecognitionError::SpeechRecognitionError(const AtomicString& eventName, const SpeechRecognitionErrorInit& initializer)
    : Event(eventName, initializer)
    , m_code(initializer.code)
    , m_message(initializer.message)
{
}

const AtomicString& SpeechRecognitionError::interfaceName() const
{
    return eventNames().interfaceForSpeechRecognitionError;
}

SpeechRecognitionErrorInit::SpeechRecognitionErrorInit()
    : code(0)
{
}

} // namespace WebCore

#endif // ENABLE(SCRIPTED_SPEECH)
