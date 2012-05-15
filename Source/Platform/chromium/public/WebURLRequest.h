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

#if WEBKIT_IMPLEMENTATION
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
        UseProtocolCachePolicy, // normal load
        ReloadIgnoringCacheData, // reload
        ReturnCacheDataElseLoad, // back/forward or encoding change - allow stale data
        ReturnCacheDataDontLoad, // results of a post - allow stale data and only use cache
    };

    enum TargetType {
        TargetIsMainFrame = 0,
        TargetIsSubframe = 1,
        TargetIsSubresource = 2,
        TargetIsStyleSheet = 3,
        TargetIsScript = 4,
        TargetIsFontResource = 5,
        TargetIsImage = 6,
        TargetIsObject = 7,
        TargetIsMedia = 8,
        TargetIsWorker = 9,
        TargetIsSharedWorker = 10,
        TargetIsPrefetch = 11,
        TargetIsFavicon = 12,
        TargetIsXHR = 13,
        TargetIsTextTrack = 14,
        TargetIsUnspecified = 15,
        // FIXME: This old enum value is only being left in while prerendering is staging into chromium. After http://codereview.chromium.org/10244007/
        // lands, this should be removed.
        TargetIsPrerender = TargetIsUnspecified,
    };

    class ExtraData {
    public:
        virtual ~ExtraData() { }
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

    WEBKIT_EXPORT void initialize();
    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebURLRequest&);

    WEBKIT_EXPORT bool isNull() const;

    WEBKIT_EXPORT WebURL url() const;
    WEBKIT_EXPORT void setURL(const WebURL&);

    // Used to implement third-party cookie blocking.
    WEBKIT_EXPORT WebURL firstPartyForCookies() const;
    WEBKIT_EXPORT void setFirstPartyForCookies(const WebURL&);

    WEBKIT_EXPORT bool allowCookies() const;
    WEBKIT_EXPORT void setAllowCookies(bool);

    // Controls whether user name, password, and cookies may be sent with the
    // request. (If false, this overrides allowCookies.)
    WEBKIT_EXPORT bool allowStoredCredentials() const;
    WEBKIT_EXPORT void setAllowStoredCredentials(bool);

    WEBKIT_EXPORT CachePolicy cachePolicy() const;
    WEBKIT_EXPORT void setCachePolicy(CachePolicy);

    WEBKIT_EXPORT WebString httpMethod() const;
    WEBKIT_EXPORT void setHTTPMethod(const WebString&);

    WEBKIT_EXPORT WebString httpHeaderField(const WebString& name) const;
    WEBKIT_EXPORT void setHTTPHeaderField(const WebString& name, const WebString& value);
    WEBKIT_EXPORT void addHTTPHeaderField(const WebString& name, const WebString& value);
    WEBKIT_EXPORT void clearHTTPHeaderField(const WebString& name);
    WEBKIT_EXPORT void visitHTTPHeaderFields(WebHTTPHeaderVisitor*) const;

    WEBKIT_EXPORT WebHTTPBody httpBody() const;
    WEBKIT_EXPORT void setHTTPBody(const WebHTTPBody&);

    // Controls whether upload progress events are generated when a request
    // has a body.
    WEBKIT_EXPORT bool reportUploadProgress() const;
    WEBKIT_EXPORT void setReportUploadProgress(bool);

    // Controls whether load timing info is collected for the request.
    WEBKIT_EXPORT bool reportLoadTiming() const;
    WEBKIT_EXPORT void setReportLoadTiming(bool);

    // Controls whether actual headers sent and received for request are
    // collected and reported.
    WEBKIT_EXPORT bool reportRawHeaders() const;
    WEBKIT_EXPORT void setReportRawHeaders(bool);

    WEBKIT_EXPORT TargetType targetType() const;
    WEBKIT_EXPORT void setTargetType(TargetType);

    // True if the request was user initiated.
    WEBKIT_EXPORT bool hasUserGesture() const;
    WEBKIT_EXPORT void setHasUserGesture(bool);

    // A consumer controlled value intended to be used to identify the
    // requestor.
    WEBKIT_EXPORT int requestorID() const;
    WEBKIT_EXPORT void setRequestorID(int);

    // A consumer controlled value intended to be used to identify the
    // process of the requestor.
    WEBKIT_EXPORT int requestorProcessID() const;
    WEBKIT_EXPORT void setRequestorProcessID(int);

    // Allows the request to be matched up with its app cache host.
    WEBKIT_EXPORT int appCacheHostID() const;
    WEBKIT_EXPORT void setAppCacheHostID(int);

    // If true, the response body will be downloaded to a file managed by the
    // WebURLLoader. See WebURLResponse::downloadedFilePath.
    WEBKIT_EXPORT bool downloadToFile() const;
    WEBKIT_EXPORT void setDownloadToFile(bool);

    // Extra data associated with the underlying resource request. Resource
    // requests can be copied. If non-null, each copy of a resource requests
    // holds a pointer to the extra data, and the extra data pointer will be
    // deleted when the last resource request is destroyed. Setting the extra
    // data pointer will cause the underlying resource request to be
    // dissociated from any existing non-null extra data pointer.
    WEBKIT_EXPORT ExtraData* extraData() const;
    WEBKIT_EXPORT void setExtraData(ExtraData*);

#if WEBKIT_IMPLEMENTATION
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
