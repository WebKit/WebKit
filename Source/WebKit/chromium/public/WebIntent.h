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

#ifndef WebIntent_h
#define WebIntent_h

#include "WebMessagePortChannel.h"
#include "platform/WebCommon.h"
#include "platform/WebPrivatePtr.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "platform/WebVector.h"

namespace WebCore { class Intent; }

namespace WebKit {

// Holds data passed through a Web Intents invocation call from the Javascript
// Intent object.
// See spec at http://www.chromium.org/developers/design-documents/webintentsapi
class WebIntent {
public:
    WebIntent() { }
    WebIntent(const WebIntent& other) { assign(other); }
    ~WebIntent() { reset(); }

    WebIntent& operator=(const WebIntent& other)
    {
       assign(other);
       return *this;
    }

    WEBKIT_EXPORT static WebIntent create(const WebString& action, const WebString& type, const WebString& data,
                                          const WebVector<WebString>& extrasNames, const WebVector<WebString>& extrasValues);

    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT bool isNull() const;
    WEBKIT_EXPORT bool equals(const WebIntent&) const;
    WEBKIT_EXPORT void assign(const WebIntent&);

    WEBKIT_EXPORT WebString action() const;
    WEBKIT_EXPORT WebString type() const;
    WEBKIT_EXPORT WebString data() const;
    WEBKIT_EXPORT WebURL service() const;
    WEBKIT_EXPORT WebVector<WebURL> suggestions() const;

    // Retrieve a list of the names of extra metadata associated with the
    // intent.
    WEBKIT_EXPORT WebVector<WebString> extrasNames() const;

    // Retrieve the value of an extra metadata element. The argument should
    // be one of the names retrieved with |extrasNames|. Returns an empty
    // string if the name is invalid.
    WEBKIT_EXPORT WebString extrasValue(const WebString&) const;

    // Caller takes ownership of the ports.
    WEBKIT_EXPORT WebMessagePortChannelArray* messagePortChannelsRelease() const;

#if WEBKIT_IMPLEMENTATION
    WebIntent(const WTF::PassRefPtr<WebCore::Intent>&);
    operator WebCore::Intent*() const;
#endif

private:
    WebPrivatePtr<WebCore::Intent> m_private;
};

} // namespace WebKit

#endif // WebIntent_h
