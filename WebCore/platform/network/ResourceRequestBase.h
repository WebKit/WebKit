/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ResourceRequestBase_h
#define ResourceRequestBase_h

#include "FormData.h"
#include "KURL.h"
#include "HTTPHeaderMap.h"

#include <wtf/OwnPtr.h>

namespace WebCore {

    enum ResourceRequestCachePolicy {
        UseProtocolCachePolicy, // normal load
        ReloadIgnoringCacheData, // reload
        ReturnCacheDataElseLoad, // back/forward or encoding change - allow stale data
        ReturnCacheDataDontLoad, // results of a post - allow stale data and only use cache
    };

    const int unspecifiedTimeoutInterval = INT_MAX;

    class ResourceRequest;
    struct CrossThreadResourceRequestData;

    // Do not use this type directly.  Use ResourceRequest instead.
    class ResourceRequestBase : public FastAllocBase {
    public:
        // The type of this ResourceRequest, based on how the resource will be used.
        enum TargetType {
            TargetIsMainFrame,
            TargetIsSubframe,
            TargetIsSubresource,  // Resource is a generic subresource.  (Generally a specific type should be specified)
            TargetIsStyleSheet,
            TargetIsScript,
            TargetIsFontResource,
            TargetIsImage,
            TargetIsObject,
            TargetIsMedia,
            TargetIsWorker,
            TargetIsSharedWorker
        };

        static PassOwnPtr<ResourceRequest> adopt(PassOwnPtr<CrossThreadResourceRequestData>);

        // Gets a copy of the data suitable for passing to another thread.
        PassOwnPtr<CrossThreadResourceRequestData> copyData() const;

        bool isNull() const;
        bool isEmpty() const;

        const KURL& url() const;
        void setURL(const KURL& url);

        void removeCredentials();

        ResourceRequestCachePolicy cachePolicy() const;
        void setCachePolicy(ResourceRequestCachePolicy cachePolicy);
        
        double timeoutInterval() const;
        void setTimeoutInterval(double timeoutInterval);
        
        const KURL& firstPartyForCookies() const;
        void setFirstPartyForCookies(const KURL& firstPartyForCookies);
        
        const String& httpMethod() const;
        void setHTTPMethod(const String& httpMethod);
        
        const HTTPHeaderMap& httpHeaderFields() const;
        String httpHeaderField(const AtomicString& name) const;
        String httpHeaderField(const char* name) const;
        void setHTTPHeaderField(const AtomicString& name, const String& value);
        void setHTTPHeaderField(const char* name, const String& value);
        void addHTTPHeaderField(const AtomicString& name, const String& value);
        void addHTTPHeaderFields(const HTTPHeaderMap& headerFields);
        
        String httpContentType() const { return httpHeaderField("Content-Type");  }
        void setHTTPContentType(const String& httpContentType) { setHTTPHeaderField("Content-Type", httpContentType); }
        
        String httpReferrer() const { return httpHeaderField("Referer"); }
        void setHTTPReferrer(const String& httpReferrer) { setHTTPHeaderField("Referer", httpReferrer); }
        void clearHTTPReferrer();
        
        String httpOrigin() const { return httpHeaderField("Origin"); }
        void setHTTPOrigin(const String& httpOrigin) { setHTTPHeaderField("Origin", httpOrigin); }
        void clearHTTPOrigin();

        String httpUserAgent() const { return httpHeaderField("User-Agent"); }
        void setHTTPUserAgent(const String& httpUserAgent) { setHTTPHeaderField("User-Agent", httpUserAgent); }

        String httpAccept() const { return httpHeaderField("Accept"); }
        void setHTTPAccept(const String& httpAccept) { setHTTPHeaderField("Accept", httpAccept); }

        void setResponseContentDispositionEncodingFallbackArray(const String& encoding1, const String& encoding2 = String(), const String& encoding3 = String());

        FormData* httpBody() const;
        void setHTTPBody(PassRefPtr<FormData> httpBody);
        
        bool allowCookies() const;
        void setAllowCookies(bool allowCookies);

        bool isConditional() const;

        // Whether the associated ResourceHandleClient needs to be notified of
        // upload progress made for that resource.
        bool reportUploadProgress() const { return m_reportUploadProgress; }
        void setReportUploadProgress(bool reportUploadProgress) { m_reportUploadProgress = reportUploadProgress; }

        // What this request is for.
        TargetType targetType() const { return m_targetType; }
        void setTargetType(TargetType type) { m_targetType = type; }

    protected:
        // Used when ResourceRequest is initialized from a platform representation of the request
        ResourceRequestBase()
            : m_resourceRequestUpdated(false)
            , m_platformRequestUpdated(true)
            , m_reportUploadProgress(false)
            , m_targetType(TargetIsSubresource)
        {
        }

        ResourceRequestBase(const KURL& url, ResourceRequestCachePolicy policy)
            : m_url(url)
            , m_cachePolicy(policy)
            , m_timeoutInterval(unspecifiedTimeoutInterval)
            , m_httpMethod("GET")
            , m_allowCookies(true)
            , m_resourceRequestUpdated(true)
            , m_platformRequestUpdated(false)
            , m_reportUploadProgress(false)
            , m_targetType(TargetIsSubresource)
        {
        }

        void updatePlatformRequest() const; 
        void updateResourceRequest() const; 

        KURL m_url;

        ResourceRequestCachePolicy m_cachePolicy;
        double m_timeoutInterval;
        KURL m_firstPartyForCookies;
        String m_httpMethod;
        HTTPHeaderMap m_httpHeaderFields;
        Vector<String> m_responseContentDispositionEncodingFallbackArray;
        RefPtr<FormData> m_httpBody;
        bool m_allowCookies;
        mutable bool m_resourceRequestUpdated;
        mutable bool m_platformRequestUpdated;
        bool m_reportUploadProgress;
        TargetType m_targetType;

    private:
        const ResourceRequest& asResourceRequest() const;
    };

    bool equalIgnoringHeaderFields(const ResourceRequestBase&, const ResourceRequestBase&);

    bool operator==(const ResourceRequestBase&, const ResourceRequestBase&);
    inline bool operator!=(ResourceRequestBase& a, const ResourceRequestBase& b) { return !(a == b); }

    struct CrossThreadResourceRequestData : Noncopyable {
        KURL m_url;

        ResourceRequestCachePolicy m_cachePolicy;
        double m_timeoutInterval;
        KURL m_firstPartyForCookies;

        String m_httpMethod;
        OwnPtr<CrossThreadHTTPHeaderMapData> m_httpHeaders;
        Vector<String> m_responseContentDispositionEncodingFallbackArray;
        RefPtr<FormData> m_httpBody;
        bool m_allowCookies;
        ResourceRequestBase::TargetType m_targetType;
    };
    
    unsigned initializeMaximumHTTPConnectionCountPerHost();

} // namespace WebCore

#endif // ResourceRequestBase_h
