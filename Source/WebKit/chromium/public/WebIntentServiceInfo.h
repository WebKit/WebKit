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

#ifndef WebIntentServiceInfo_h
#define WebIntentServiceInfo_h

#include "platform/WebCommon.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"

namespace WebKit {

// Holds data used to initialize a Web Intents service (handler).
// See spec at http://www.chromium.org/developers/design-documents/webintentsapi
class WebIntentServiceInfo {
public:
    ~WebIntentServiceInfo() { }

    // The location of the handler page registered by the service.
    WEBKIT_EXPORT WebURL url() const;
    WEBKIT_EXPORT void setURL(const WebURL&);

    // The short name the service will be known by when the user
    // initiates an intent.
    WEBKIT_EXPORT WebString title() const;
    WEBKIT_EXPORT void setTitle(const WebString&);

    // The kind of intent the service will handle.
    WEBKIT_EXPORT WebString action() const;
    WEBKIT_EXPORT void setAction(const WebString&);

    // The type of payload data which the service will handle.
    WEBKIT_EXPORT WebString type() const;
    WEBKIT_EXPORT void setType(const WebString&);

    // A hint to the client about whether the service can be run within
    // an "inline" context within the calling page, or in a new tab
    // context (the default).
    WEBKIT_EXPORT WebString disposition() const;
    WEBKIT_EXPORT void setDisposition(const WebString&);

#if WEBKIT_IMPLEMENTATION
    WebIntentServiceInfo();
#endif

private:
    WebString m_action;
    WebString m_type;
    WebURL m_href;
    WebString m_title;
    WebString m_disposition;
};

} // namespace WebKit

#endif // WebIntentServiceInfo_h
