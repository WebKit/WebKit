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

#ifndef WebSpeechRecognitionParams_h
#define WebSpeechRecognitionParams_h

#include "WebSecurityOrigin.h"
#include "WebSpeechGrammar.h"
#include "platform/WebString.h"
#include "platform/WebVector.h"

namespace WebKit {

class WebSpeechGrammar;

class WebSpeechRecognitionParams {
public:
    WebSpeechRecognitionParams(const WebVector<WebSpeechGrammar>& grammars, const WebString& language, bool continuous, const WebSecurityOrigin& origin)
        : m_grammars(grammars)
        , m_language(language)
        , m_continuous(continuous)
        , m_origin(origin)
    {
    }

    const WebVector<WebSpeechGrammar>& grammars() const { return m_grammars; }
    const WebString& language() const { return m_language; }
    bool continuous() const { return m_continuous; }
    const WebSecurityOrigin& origin() const { return m_origin; }

private:
    WebVector<WebSpeechGrammar> m_grammars;
    WebString m_language;
    bool m_continuous;
    WebSecurityOrigin m_origin;
};

} // namespace WebKit

#endif // WebSpeechRecognitionParams_h
