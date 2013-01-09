/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebIntentRequest_h
#define WebIntentRequest_h

#include <public/WebCommon.h>
#include <public/WebPrivatePtr.h>
#include <public/WebString.h>

namespace WebCore { class IntentRequest; }

namespace WebKit {

class WebIntent;
class WebSerializedScriptValue;

// Holds data passed through a Web Intents invocation call from the Javascript
// Intent object.
// See spec at http://www.chromium.org/developers/design-documents/webintentsapi
class WebIntentRequest {
public:
    WebIntentRequest() { }
    WebIntentRequest(const WebIntentRequest& other) { assign(other); }
    ~WebIntentRequest() { reset(); }

    WebIntentRequest& operator=(const WebIntentRequest& other)
    {
       assign(other);
       return *this;
    }
    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT bool isNull() const;
    WEBKIT_EXPORT bool equals(const WebIntentRequest&) const;
    WEBKIT_EXPORT void assign(const WebIntentRequest&);

    WEBKIT_EXPORT void postResult(const WebSerializedScriptValue&);
    WEBKIT_EXPORT void postFailure(const WebSerializedScriptValue&);

    WEBKIT_EXPORT WebIntent intent() const;

#if WEBKIT_IMPLEMENTATION
    WebIntentRequest(const WTF::PassRefPtr<WebCore::IntentRequest>&);
#endif

private:
    WebPrivatePtr<WebCore::IntentRequest> m_private;
};

} // namespace WebKit

#endif // WebIntentRequest_h
