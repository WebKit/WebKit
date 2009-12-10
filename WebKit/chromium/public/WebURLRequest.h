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

#ifndef WebURLRequest_h
#define WebURLRequest_h

#include "WebCommon.h"
#include "WebHTTPBody.h"

#if defined(WEBKIT_IMPLEMENTATION)
namespace WebCore { class ResourceRequest; }
#endif

namespace WebKit {

class WebCString;
class WebHTTPBody;
class WebHTTPHeaderVisitor;
class WebString;
class WebURL;
class WebURLRequestPrivate;

class WebURLRequest {
public:
    enum CachePolicy {
        UseProtocolCachePolicy,  // normal load
        ReloadIgnoringCacheData, // reload
        ReturnCacheDataElseLoad, // back/forward or encoding change - allow stale data
        ReturnCacheDataDontLoad, // results of a post - allow stale data and only use cache
    };

    enum TargetType {
        TargetIsMainFrame = 0,
        TargetIsSubFrame = 1,   // Temporary for backward compatibility.
        TargetIsSubframe = 1,
        TargetIsSubResource = 2,  // Temporary for backward comptibility.
        TargetIsSubresource = 2,
        TargetIsStyleSheet = 3,
        TargetIsScript = 4,
        TargetIsFontResource = 5,
        TargetIsImage = 6,
        TargetIsObject = 7,
        TargetIsMedia = 8
    };

    ~WebURLRequest() { reset(); }

    WebURLRequest() : m_private(0) { }
    WebURLRequest(const WebURLRequest& r) : m_private(0) { assign(r); }
    WebURLRequest& operator=(const WebURLRequest& r)
    {
        assign(r);
        return *this;
    }

    explicit WebURLRequest(const WebURL& url) : m_private(0)
    {
        initialize();
        setURL(url);
    }

    WEBKIT_API void initialize();
    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebURLRequest&);

    WEBKIT_API bool isNull() const;

    WEBKIT_API WebURL url() const;
    WEBKIT_API void setURL(const WebURL&);

    // Used to implement third-party cookie blocking.
    WEBKIT_API WebURL firstPartyForCookies() const;
    WEBKIT_API void setFirstPartyForCookies(const WebURL&);

    WEBKIT_API bool allowCookies() const;
    WEBKIT_API void setAllowCookies(bool allowCookies);

    // Controls whether user name, password, and cookies may be sent with the
    // request. (If false, this overrides allowCookies.)
    WEBKIT_API bool allowStoredCredentials() const;
    WEBKIT_API void setAllowStoredCredentials(bool allowStoredCredentials);

    WEBKIT_API CachePolicy cachePolicy() const;
    WEBKIT_API void setCachePolicy(CachePolicy);

    WEBKIT_API WebString httpMethod() const;
    WEBKIT_API void setHTTPMethod(const WebString&);

    WEBKIT_API WebString httpHeaderField(const WebString& name) const;
    WEBKIT_API void setHTTPHeaderField(const WebString& name, const WebString& value);
    WEBKIT_API void addHTTPHeaderField(const WebString& name, const WebString& value);
    WEBKIT_API void clearHTTPHeaderField(const WebString& name);
    WEBKIT_API void visitHTTPHeaderFields(WebHTTPHeaderVisitor*) const;

    WEBKIT_API WebHTTPBody httpBody() const;
    WEBKIT_API void setHTTPBody(const WebHTTPBody&);

    // Controls whether upload progress events are generated when a request
    // has a body.
    WEBKIT_API bool reportUploadProgress() const;
    WEBKIT_API void setReportUploadProgress(bool);

    WEBKIT_API TargetType targetType() const;
    WEBKIT_API void setTargetType(TargetType);

    // A consumer controlled value intended to be used to identify the
    // requestor.
    WEBKIT_API int requestorID() const;
    WEBKIT_API void setRequestorID(int);

    // A consumer controlled value intended to be used to identify the
    // process of the requestor.
    WEBKIT_API int requestorProcessID() const;
    WEBKIT_API void setRequestorProcessID(int);

    // Allows the request to be matched up with its app cache host.
    WEBKIT_API int appCacheHostID() const;
    WEBKIT_API void setAppCacheHostID(int id);

#if defined(WEBKIT_IMPLEMENTATION)
    WebCore::ResourceRequest& toMutableResourceRequest();
    const WebCore::ResourceRequest& toResourceRequest() const;
#endif

protected:
    void assign(WebURLRequestPrivate*);

private:
    WebURLRequestPrivate* m_private;
};

} // namespace WebKit

#endif
