/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebURLLoadTiming_h
#define WebURLLoadTiming_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"

namespace WebCore { class ResourceLoadTiming; }

namespace WebKit {
class WebString;

class WebURLLoadTiming {
public:
    ~WebURLLoadTiming() { reset(); }

    WebURLLoadTiming() { }
    WebURLLoadTiming(const WebURLLoadTiming& d) { assign(d); }
    WebURLLoadTiming& operator=(const WebURLLoadTiming& d)
    {
        assign(d);
        return *this;
    }

    WEBKIT_API void initialize();
    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebURLLoadTiming&);

    bool isNull() const { return m_private.isNull(); }

    WEBKIT_API double requestTime() const;
    WEBKIT_API void setRequestTime(double time);

    WEBKIT_API double proxyDuration() const;
    WEBKIT_API void setProxyDuration(double duration);

    WEBKIT_API double dnsDuration() const;
    WEBKIT_API void setDNSDuration(double duration);

    WEBKIT_API double connectDuration() const;
    WEBKIT_API void setConnectDuration(double duration);

    WEBKIT_API double sendDuration() const;
    WEBKIT_API void setSendDuration(double duration);

    WEBKIT_API double receiveHeadersDuration() const;
    WEBKIT_API void setReceiveHeadersDuration(double duration);

    WEBKIT_API double sslDuration() const;
    WEBKIT_API void setSSLDuration(double duration);

#if WEBKIT_IMPLEMENTATION
    WebURLLoadTiming(const WTF::PassRefPtr<WebCore::ResourceLoadTiming>&);
    WebURLLoadTiming& operator=(const WTF::PassRefPtr<WebCore::ResourceLoadTiming>&);
    operator WTF::PassRefPtr<WebCore::ResourceLoadTiming>() const;
#endif

private:
    WebPrivatePtr<WebCore::ResourceLoadTiming> m_private;
};

} // namespace WebKit

#endif
