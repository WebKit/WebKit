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

#ifndef WebSpeechRecognitionHandle_h
#define WebSpeechRecognitionHandle_h

#include "platform/WebCommon.h"
#include "platform/WebPrivatePtr.h"

namespace WebCore {
class SpeechRecognition;
}

namespace WebKit {

class WebSpeechRecognitionResult;
class WebString;

// WebSpeechRecognitionHandle is used by WebSpeechRecognizer to identify a
// recognition session, and by WebSpeechRecognizerClient to route
// recognition events.
class WebSpeechRecognitionHandle {
public:
    ~WebSpeechRecognitionHandle() { reset(); }
    WebSpeechRecognitionHandle() { }

    WebSpeechRecognitionHandle(const WebSpeechRecognitionHandle& other) { assign(other); }
    WebSpeechRecognitionHandle& operator=(const WebSpeechRecognitionHandle& other)
    {
        assign(other);
        return *this;
    }

    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebSpeechRecognitionHandle&);

    // Comparison functions are provided so that WebSpeechRecognitionHandle objects
    // can be stored in a hash map.
    WEBKIT_EXPORT bool equals(const WebSpeechRecognitionHandle&) const;
    WEBKIT_EXPORT bool lessThan(const WebSpeechRecognitionHandle&) const;

#if WEBKIT_IMPLEMENTATION
    WebSpeechRecognitionHandle(const WTF::PassRefPtr<WebCore::SpeechRecognition>&);
    WebSpeechRecognitionHandle& operator=(const WTF::PassRefPtr<WebCore::SpeechRecognition>&);
    operator WTF::PassRefPtr<WebCore::SpeechRecognition>() const;
#endif

private:
    WebPrivatePtr<WebCore::SpeechRecognition> m_private;
};

inline bool operator==(const WebSpeechRecognitionHandle& a, const WebSpeechRecognitionHandle& b)
{
    return a.equals(b);
}

inline bool operator!=(const WebSpeechRecognitionHandle& a, const WebSpeechRecognitionHandle& b)
{
    return !(a == b);
}

inline bool operator<(const WebSpeechRecognitionHandle& a, const WebSpeechRecognitionHandle& b)
{
    return a.lessThan(b);
}

} // namespace WebKit

#endif // WebSpeechRecognitionHandle_h
