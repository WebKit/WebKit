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

#ifndef SpeechRecognitionError_h
#define SpeechRecognitionError_h

#if ENABLE(SCRIPTED_SPEECH)

#include "PlatformString.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class SpeechRecognitionError : public RefCounted<SpeechRecognitionError> {
public:
    enum Code {
        OTHER = 0,
        NO_SPEECH = 1,
        ABORTED = 2,
        AUDIO_CAPTURE = 3,
        NETWORK = 4,
        NOT_ALLOWED = 5,
        SERVICE_NOT_ALLOWED = 6,
        BAD_GRAMMAR = 7,
        LANGUAGE_NOT_SUPPORTED = 8
    };

    static PassRefPtr<SpeechRecognitionError> create(Code, const String&);

    Code code() { return m_code; }
    const String& message() { return m_message; }

private:
    SpeechRecognitionError(Code, const String&);

    Code m_code;
    String m_message;
};

} // namespace WebCore

#endif // ENABLE(SCRIPTED_SPEECH)

#endif // SpeechRecognitionError_h
