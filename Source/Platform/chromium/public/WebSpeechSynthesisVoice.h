/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef WebSpeechSynthesisVoice_h
#define WebSpeechSynthesisVoice_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"
#include "WebString.h"

namespace WebCore {
class PlatformSpeechSynthesisVoice;
}

namespace WebKit {

class WebSpeechSynthesisVoice {
public:
    WebSpeechSynthesisVoice() { }
    WebSpeechSynthesisVoice(const WebSpeechSynthesisVoice& other) { assign(other); }
    ~WebSpeechSynthesisVoice() { reset(); }

    WebSpeechSynthesisVoice& operator=(const WebSpeechSynthesisVoice& other)
    {
        assign(other);
        return *this;
    }

    WEBKIT_EXPORT void assign(const WebSpeechSynthesisVoice&);
    WEBKIT_EXPORT void reset();

    WEBKIT_EXPORT void setVoiceURI(const WebString&);
    WEBKIT_EXPORT void setName(const WebString&);
    WEBKIT_EXPORT void setLanguage(const WebString&);
    WEBKIT_EXPORT void setIsLocalService(bool);
    WEBKIT_EXPORT void setIsDefault(bool);

#if WEBKIT_IMPLEMENTATION
    operator WTF::PassRefPtr<WebCore::PlatformSpeechSynthesisVoice>() const;
#endif

private:
    WebPrivatePtr<WebCore::PlatformSpeechSynthesisVoice> m_private;
};

} // namespace WebKit

#endif // WebSpeechSynthesisVoice_h
