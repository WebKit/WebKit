/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebURLResponse_h
#define WebURLResponse_h

#include "WebCommon.h"
#include "WebPrivateOwnPtr.h"

#if defined(WEBKIT_IMPLEMENTATION)
namespace WebCore { class ResourceResponse; }
#endif

namespace WebKit {

class WebCString;
class WebHTTPHeaderVisitor;
class WebHTTPLoadInfo;
class WebString;
class WebURL;
class WebURLLoadTiming;
class WebURLResponsePrivate;

class WebURLResponse {
public:
    ~WebURLResponse() { reset(); }

    WebURLResponse() : m_private(0) { }
    WebURLResponse(const WebURLResponse& r) : m_private(0) { assign(r); }
    WebURLResponse& operator=(const WebURLResponse& r)
    {
        assign(r);
        return *this;
    }

    explicit WebURLResponse(const WebURL& url) : m_private(0)
    {
        initialize();
        setURL(url);
    }

    WEBKIT_API void initialize();
    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebURLResponse&);

    WEBKIT_API bool isNull() const;

    WEBKIT_API WebURL url() const;
    WEBKIT_API void setURL(const WebURL&);

    WEBKIT_API unsigned connectionID() const;
    WEBKIT_API void setConnectionID(unsigned);

    WEBKIT_API bool connectionReused() const;
    WEBKIT_API void setConnectionReused(bool);

    WEBKIT_API WebURLLoadTiming loadTiming();
    WEBKIT_API void setLoadTiming(const WebURLLoadTiming&);

    WEBKIT_API WebHTTPLoadInfo httpLoadInfo();
    WEBKIT_API void setHTTPLoadInfo(const WebHTTPLoadInfo&);

    WEBKIT_API double responseTime() const;
    WEBKIT_API void setResponseTime(double);

    WEBKIT_API WebString mimeType() const;
    WEBKIT_API void setMIMEType(const WebString&);

    WEBKIT_API long long expectedContentLength() const;
    WEBKIT_API void setExpectedContentLength(long long);

    WEBKIT_API WebString textEncodingName() const;
    WEBKIT_API void setTextEncodingName(const WebString&);

    WEBKIT_API WebString suggestedFileName() const;
    WEBKIT_API void setSuggestedFileName(const WebString&);

    WEBKIT_API int httpStatusCode() const;
    WEBKIT_API void setHTTPStatusCode(int);

    WEBKIT_API WebString httpStatusText() const;
    WEBKIT_API void setHTTPStatusText(const WebString&);

    WEBKIT_API WebString httpHeaderField(const WebString& name) const;
    WEBKIT_API void setHTTPHeaderField(const WebString& name, const WebString& value);
    WEBKIT_API void addHTTPHeaderField(const WebString& name, const WebString& value);
    WEBKIT_API void clearHTTPHeaderField(const WebString& name);
    WEBKIT_API void visitHTTPHeaderFields(WebHTTPHeaderVisitor*) const;

    WEBKIT_API double lastModifiedDate() const;
    WEBKIT_API void setLastModifiedDate(double);

    WEBKIT_API bool isContentFiltered() const;
    WEBKIT_API void setIsContentFiltered(bool);

    WEBKIT_API long long appCacheID() const;
    WEBKIT_API void setAppCacheID(long long);

    WEBKIT_API WebURL appCacheManifestURL() const;
    WEBKIT_API void setAppCacheManifestURL(const WebURL&);

    // A consumer controlled value intended to be used to record opaque
    // security info related to this request.
    WEBKIT_API WebCString securityInfo() const;
    WEBKIT_API void setSecurityInfo(const WebCString&);

#if defined(WEBKIT_IMPLEMENTATION)
    WebCore::ResourceResponse& toMutableResourceResponse();
    const WebCore::ResourceResponse& toResourceResponse() const;
#endif

    // Flag whether this request was served from the disk cache entry.
    WEBKIT_API bool wasCached() const;
    WEBKIT_API void setWasCached(bool);

    // Flag whether this request was loaded via the SPDY protocol or not.
    // SPDY is an experimental web protocol, see http://dev.chromium.org/spdy
    WEBKIT_API bool wasFetchedViaSPDY() const;
    WEBKIT_API void setWasFetchedViaSPDY(bool);

    // Flag whether this request was loaded after the TLS/Next-Protocol-Negotiation was used.
    // This is related to SPDY.
    WEBKIT_API bool wasNpnNegotiated() const;
    WEBKIT_API void setWasNpnNegotiated(bool);

    // Flag whether this request was made when "Alternate-Protocol: xxx"
    // is present in server's response.
    WEBKIT_API bool wasAlternateProtocolAvailable() const;
    WEBKIT_API void setWasAlternateProtocolAvailable(bool);

    // Flag whether this request was loaded via an explicit proxy (HTTP, SOCKS, etc).
    WEBKIT_API bool wasFetchedViaProxy() const;
    WEBKIT_API void setWasFetchedViaProxy(bool);

    // Flag whether this request is part of a multipart response.
    WEBKIT_API bool isMultipartPayload() const;
    WEBKIT_API void setIsMultipartPayload(bool);

    // This indicates the location of a downloaded response if the
    // WebURLRequest had the downloadToFile flag set to true.  This file path
    // remains valid for the lifetime of the WebURLLoader used to create it.
    WEBKIT_API WebString downloadFilePath() const;
    WEBKIT_API void setDownloadFilePath(const WebString&);

protected:
    void assign(WebURLResponsePrivate*);

private:
    WebURLResponsePrivate* m_private;
};

} // namespace WebKit

#endif
