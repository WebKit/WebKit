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

    WEBKIT_API int proxyStart() const;
    WEBKIT_API void setProxyStart(int start);

    WEBKIT_API int proxyEnd() const;
    WEBKIT_API void setProxyEnd(int end);

    WEBKIT_API int dnsStart() const;
    WEBKIT_API void setDNSStart(int start);

    WEBKIT_API int dnsEnd() const;
    WEBKIT_API void setDNSEnd(int end);

    WEBKIT_API int connectStart() const;
    WEBKIT_API void setConnectStart(int start);

    WEBKIT_API int connectEnd() const;
    WEBKIT_API void setConnectEnd(int end);

    WEBKIT_API int sendStart() const;
    WEBKIT_API void setSendStart(int start);

    WEBKIT_API int sendEnd() const;
    WEBKIT_API void setSendEnd(int end);

    WEBKIT_API int receiveHeadersEnd() const;
    WEBKIT_API void setReceiveHeadersEnd(int end);

    WEBKIT_API int sslStart() const;
    WEBKIT_API void setSSLStart(int start);

    WEBKIT_API int sslEnd() const;
    WEBKIT_API void setSSLEnd(int end);

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
