// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#ifndef ResourceRequest_H_
#define ResourceRequest_H_

#include "FormData.h"
#include "KURL.h"
#include "HTTPHeaderMap.h"

#if PLATFORM(MAC)
#include "RetainPtr.h"
#ifdef __OBJC__
@class NSURLRequest;
#else
class NSURLRequest;
#endif
#elif USE(CFNETWORK)
#include "RetainPtr.h"
typedef const struct _CFURLRequest* CFURLRequestRef;
#endif

namespace WebCore {

    enum ResourceRequestCachePolicy {
        UseProtocolCachePolicy, // normal load
        ReloadIgnoringCacheData, // reload
        ReturnCacheDataElseLoad, // back/forward or encoding change - allow stale data
        ReturnCacheDataDontLoad, // results of a post - allow stale data and only use cache
    };

    struct ResourceRequest {

        
        ResourceRequest(const String& url) 
            : m_url(url.deprecatedString())
            , m_cachePolicy(UseProtocolCachePolicy)
            , m_timeoutInterval(defaultTimeoutInterval)
            , m_httpMethod("GET")
            , m_allowHTTPCookies(true)
            , m_resourceRequestUpdated(true)
            , m_platformRequestUpdated(false)
        {
        }

        ResourceRequest(const KURL& url) 
            : m_url(url)
            , m_cachePolicy(UseProtocolCachePolicy)
            , m_timeoutInterval(defaultTimeoutInterval)
            , m_httpMethod("GET")
            , m_allowHTTPCookies(true)
            , m_resourceRequestUpdated(true)
            , m_platformRequestUpdated(false)
        {
        }

        ResourceRequest(const KURL& url, const String& referrer, ResourceRequestCachePolicy policy = UseProtocolCachePolicy) 
            : m_url(url)
            , m_cachePolicy(policy)
            , m_timeoutInterval(defaultTimeoutInterval)
            , m_httpMethod("GET")
            , m_allowHTTPCookies(true)
            , m_resourceRequestUpdated(true)
            , m_platformRequestUpdated(false)
        {
            setHTTPReferrer(referrer);
        }
        
        ResourceRequest()
            : m_cachePolicy(UseProtocolCachePolicy)
            , m_timeoutInterval(defaultTimeoutInterval)
            , m_httpMethod("GET")
            , m_allowHTTPCookies(true)
            , m_resourceRequestUpdated(true)
            , m_platformRequestUpdated(false)
        {
        }

        bool isEmpty() const;

        const KURL& url() const;
        void setURL(const KURL& url);

        const ResourceRequestCachePolicy cachePolicy() const;
        void setCachePolicy(ResourceRequestCachePolicy cachePolicy);
        
        double timeoutInterval() const;
        void setTimeoutInterval(double timeoutInterval);
        
        const KURL& mainDocumentURL() const;
        void setMainDocumentURL(const KURL& mainDocumentURL);
        
        const String& httpMethod() const;
        void setHTTPMethod(const String& httpMethod);
        
        const HTTPHeaderMap& httpHeaderFields() const;
        String httpHeaderField(const String& name) const;
        void setHTTPHeaderField(const String& name, const String& value);
        void addHTTPHeaderField(const String& name, const String& value);
        void addHTTPHeaderFields(const HTTPHeaderMap& headerFields);
        
        String httpContentType() const { return httpHeaderField("Content-Type");  }
        void setHTTPContentType(const String& httpContentType) { setHTTPHeaderField("Content-Type", httpContentType); }
        
        String httpReferrer() const { return httpHeaderField("Referer"); }
        void setHTTPReferrer(const String& httpReferrer) { setHTTPHeaderField("Referer", httpReferrer); }
        
        String httpUserAgent() const { return httpHeaderField("User-Agent"); }
        void setHTTPUserAgent(const String& httpUserAgent) { setHTTPHeaderField("User-Agent", httpUserAgent); }

        String httpAccept() const { return httpHeaderField("Accept"); }
        void setHTTPAccept(const String& httpUserAgent) { setHTTPHeaderField("Accept", httpUserAgent); }

        FormData* httpBody() const;
        void setHTTPBody(PassRefPtr<FormData> httpBody);
        
        bool allowHTTPCookies() const;
        void setAllowHTTPCookies(bool allowHTTPCookies);

        bool isConditional() const;
        
#if PLATFORM(MAC)
        ResourceRequest(NSURLRequest* nsRequest)
            : m_resourceRequestUpdated(false)
            , m_platformRequestUpdated(true)
            , m_nsRequest(nsRequest) { }
        
        NSURLRequest* nsURLRequest() const;
#elif USE(CFNETWORK)
        ResourceRequest(CFURLRequestRef cfRequest)
            : m_resourceRequestUpdated(false)
            , m_platformRequestUpdated(true)
            , m_cfRequest(cfRequest) { }
        
        CFURLRequestRef cfURLRequest() const;       
#endif
    private:
        void updatePlatformRequest() const; 
        void updateResourceRequest() const; 

#if PLATFORM(MAC) || USE(CFNETWORK)
        void doUpdatePlatformRequest();
        void doUpdateResourceRequest();
#endif
        
        static const int defaultTimeoutInterval = 60;

        KURL m_url;

        ResourceRequestCachePolicy m_cachePolicy;
        double m_timeoutInterval;
        KURL m_mainDocumentURL;
        String m_httpMethod;
        HTTPHeaderMap m_httpHeaderFields;
        RefPtr<FormData> m_httpBody;
        bool m_allowHTTPCookies;
        mutable bool m_resourceRequestUpdated;
        mutable bool m_platformRequestUpdated;
#if PLATFORM(MAC)
        RetainPtr<NSURLRequest> m_nsRequest;
#elif USE(CFNETWORK)
        RetainPtr<CFURLRequestRef> m_cfRequest;      
#endif
    };

    bool operator==(const ResourceRequest& a, const ResourceRequest& b);

} // namespace WebCore

#endif // ResourceRequest_H_
