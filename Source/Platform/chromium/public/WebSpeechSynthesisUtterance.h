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

#ifndef WebSpeechSynthesisUtterance_h
#define WebSpeechSynthesisUtterance_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"
#include "WebString.h"

namespace WebCore { class PlatformSpeechSynthesisUtterance; }

namespace WebKit {

class WebSpeechSynthesisUtterance {
public:
    WebSpeechSynthesisUtterance() { }
    WebSpeechSynthesisUtterance(const WebSpeechSynthesisUtterance& other) { assign(other); }
    ~WebSpeechSynthesisUtterance() { reset(); }

    WebSpeechSynthesisUtterance& operator=(const WebSpeechSynthesisUtterance& other)
    {
        assign(other);
        return *this;
    }

    WEBKIT_EXPORT void assign(const WebSpeechSynthesisUtterance&);
    WEBKIT_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    WEBKIT_EXPORT WebString text() const;
    WEBKIT_EXPORT WebString lang() const;
    WEBKIT_EXPORT WebString voice() const;

    // As defined in: https://dvcs.w3.org/hg/speech-api/raw-file/tip/speechapi.html
    WEBKIT_EXPORT float volume() const; // 0...1, 1 is default
    WEBKIT_EXPORT float rate() const; // 0.1...10, 1 is default
    WEBKIT_EXPORT float pitch() const; // 0...2, 1 is default

    WEBKIT_EXPORT double startTime() const; // In seconds.

#if WEBKIT_IMPLEMENTATION
    WebSpeechSynthesisUtterance(const PassRefPtr<WebCore::PlatformSpeechSynthesisUtterance>&);
    WebSpeechSynthesisUtterance& operator=(WebCore::PlatformSpeechSynthesisUtterance*);
    operator PassRefPtr<WebCore::PlatformSpeechSynthesisUtterance>() const;
    operator WebCore::PlatformSpeechSynthesisUtterance*() const;
#endif

private:
    WebPrivatePtr<WebCore::PlatformSpeechSynthesisUtterance> m_private;
};

} // namespace WebKit

#endif // WebSpeechSynthesisUtterance_h
