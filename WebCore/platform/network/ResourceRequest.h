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
        {
        }

        ResourceRequest(const KURL& url) 
            : m_url(url)
            , m_cachePolicy(UseProtocolCachePolicy)
            , m_timeoutInterval(defaultTimeoutInterval)
            , m_httpMethod("GET")
            , m_allowHTTPCookies(true)
        {
        }

        ResourceRequest(const KURL& url, const String& referrer, ResourceRequestCachePolicy policy = UseProtocolCachePolicy) 
            : m_url(url)
            , m_cachePolicy(policy)
            , m_timeoutInterval(defaultTimeoutInterval)
            , m_httpMethod("GET")
            , m_allowHTTPCookies(true)
        {
            setHTTPReferrer(referrer);
        }
        
        ResourceRequest()
            : m_cachePolicy(UseProtocolCachePolicy)
            , m_timeoutInterval(defaultTimeoutInterval)
            , m_allowHTTPCookies(true)
        {
        }

        bool isEmpty() const { return m_url.isEmpty(); }

        const KURL& url() const { return m_url; }
        void setURL(const KURL& url) { m_url = url; }

        const ResourceRequestCachePolicy cachePolicy() const { return m_cachePolicy; }
        void setCachePolicy(ResourceRequestCachePolicy cachePolicy) { m_cachePolicy = cachePolicy; }
        
        double timeoutInterval() const { return m_timeoutInterval; }
        void setTimeoutInterval(double timeoutInterval) { m_timeoutInterval = timeoutInterval; }
        
        const KURL& mainDocumentURL() const { return m_mainDocumentURL; }
        void setMainDocumentURL(const KURL& mainDocumentURL) { m_mainDocumentURL = mainDocumentURL; }
        
        const String& httpMethod() const { return m_httpMethod; }
        void setHTTPMethod(const String& httpMethod) { m_httpMethod = httpMethod; }
        
        const HTTPHeaderMap& httpHeaderFields() const { return m_httpHeaderFields; }
        String httpHeaderField(const String& name) const { return m_httpHeaderFields.get(name); }
        void setHTTPHeaderField(const String& name, const String& value) { m_httpHeaderFields.set(name, value); }
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

        const FormData& httpBody() const { return m_httpBody; }
        FormData& httpBody() { return m_httpBody; }
        void setHTTPBody(const FormData& httpBody) { m_httpBody = httpBody; } 
        
        bool allowHTTPCookies() const { return m_allowHTTPCookies; }
        void setAllowHTTPCookies(bool allowHTTPCookies) { m_allowHTTPCookies = allowHTTPCookies; }

    private:
        static const int defaultTimeoutInterval = 60;

        KURL m_url;

        ResourceRequestCachePolicy m_cachePolicy;
        double m_timeoutInterval;
        KURL m_mainDocumentURL;
        String m_httpMethod;
        HTTPHeaderMap m_httpHeaderFields;
        FormData m_httpBody;
        bool m_allowHTTPCookies;
    };

    inline void ResourceRequest::addHTTPHeaderField(const String& name, const String& value) 
    {
        pair<HTTPHeaderMap::iterator, bool> result = m_httpHeaderFields.add(name, value); 
        if (!result.second)
            result.first->second += "," + value;
    }

    // FIXME: probably shouldn't inline this
    inline void ResourceRequest::addHTTPHeaderFields(const HTTPHeaderMap& headerFields)
    {
        HTTPHeaderMap::const_iterator end = headerFields.end();
        for (HTTPHeaderMap::const_iterator it = headerFields.begin(); it != end; ++it)
            addHTTPHeaderField(it->first, it->second);
    }

} // namespace WebCore

#endif // ResourceRequest_H_
