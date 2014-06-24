/*
 * Copyright (C) 2003, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "HTTPHeaderMap.h"
#include "URL.h"
#include "ResourceLoadPriority.h"

namespace WebCore {

    enum ResourceRequestCachePolicy {
        UseProtocolCachePolicy, // normal load
        ReloadIgnoringCacheData, // reload
        ReturnCacheDataElseLoad, // back/forward or encoding change - allow stale data
        ReturnCacheDataDontLoad  // results of a post - allow stale data and only use cache
    };

    enum HTTPBodyUpdatePolicy {
        DoNotUpdateHTTPBody,
        UpdateHTTPBody
    };

    class ResourceRequest;
    struct CrossThreadResourceRequestData;

    // Do not use this type directly.  Use ResourceRequest instead.
    class ResourceRequestBase {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static PassOwnPtr<ResourceRequest> adopt(PassOwnPtr<CrossThreadResourceRequestData>);

        // Gets a copy of the data suitable for passing to another thread.
        PassOwnPtr<CrossThreadResourceRequestData> copyData() const;

        bool isNull() const;
        bool isEmpty() const;

        const URL& url() const;
        void setURL(const URL& url);

        void removeCredentials();

        ResourceRequestCachePolicy cachePolicy() const;
        void setCachePolicy(ResourceRequestCachePolicy cachePolicy);
        
        double timeoutInterval() const; // May return 0 when using platform default.
        void setTimeoutInterval(double timeoutInterval);
        
        const URL& firstPartyForCookies() const;
        void setFirstPartyForCookies(const URL& firstPartyForCookies);
        
        const String& httpMethod() const;
        void setHTTPMethod(const String& httpMethod);
        
        const HTTPHeaderMap& httpHeaderFields() const;
        void setHTTPHeaderFields(HTTPHeaderMap);

        String httpHeaderField(const String& name) const;
        String httpHeaderField(HTTPHeaderName) const;
        void setHTTPHeaderField(const String& name, const String& value);
        void setHTTPHeaderField(HTTPHeaderName, const String& value);
        void addHTTPHeaderField(const String& name, const String& value);

        // Instead of passing a string literal to any of these functions, just use a HTTPHeaderName instead.
        template<size_t length> String httpHeaderField(const char (&)[length]) const = delete;
        template<size_t length> void setHTTPHeaderField(const char (&)[length], const String&) = delete;
        template<size_t length> void addHTTPHeaderField(const char (&)[length], const String&) = delete;

        void clearHTTPAuthorization();

        String httpContentType() const;
        void setHTTPContentType(const String&);
        void clearHTTPContentType();

        String httpReferrer() const;
        void setHTTPReferrer(const String&);
        void clearHTTPReferrer();
        
        String httpOrigin() const;
        void setHTTPOrigin(const String&);
        void clearHTTPOrigin();

        String httpUserAgent() const;
        void setHTTPUserAgent(const String&);
        void clearHTTPUserAgent();

        String httpAccept() const;
        void setHTTPAccept(const String&);
        void clearHTTPAccept();

        const Vector<String>& responseContentDispositionEncodingFallbackArray() const { return m_responseContentDispositionEncodingFallbackArray; }
        void setResponseContentDispositionEncodingFallbackArray(const String& encoding1, const String& encoding2 = String(), const String& encoding3 = String());

        FormData* httpBody() const;
        void setHTTPBody(PassRefPtr<FormData> httpBody);
        
        bool allowCookies() const;
        void setAllowCookies(bool allowCookies);

        ResourceLoadPriority priority() const;
        void setPriority(ResourceLoadPriority);

        bool isConditional() const;
        void makeUnconditional();

        // Whether the associated ResourceHandleClient needs to be notified of
        // upload progress made for that resource.
        bool reportUploadProgress() const { return m_reportUploadProgress; }
        void setReportUploadProgress(bool reportUploadProgress) { m_reportUploadProgress = reportUploadProgress; }

        // Whether the timing information should be collected for the request.
        bool reportLoadTiming() const { return m_reportLoadTiming; }
        void setReportLoadTiming(bool reportLoadTiming) { m_reportLoadTiming = reportLoadTiming; }

        // Whether actual headers being sent/received should be collected and reported for the request.
        bool reportRawHeaders() const { return m_reportRawHeaders; }
        void setReportRawHeaders(bool reportRawHeaders) { m_reportRawHeaders = reportRawHeaders; }

#if ENABLE(INSPECTOR)
        // Whether this request should be hidden from the Inspector.
        bool hiddenFromInspector() const { return m_hiddenFromInspector; }
        void setHiddenFromInspector(bool hiddenFromInspector) { m_hiddenFromInspector = hiddenFromInspector; }
#endif

#if !PLATFORM(COCOA)
        bool encodingRequiresPlatformData() const { return true; }
#endif
        template<class Encoder> void encodeWithoutPlatformData(Encoder&) const;
        template<class Decoder> bool decodeWithoutPlatformData(Decoder&);

        static double defaultTimeoutInterval(); // May return 0 when using platform default.
        static void setDefaultTimeoutInterval(double);

#if PLATFORM(IOS)
        static bool defaultAllowCookies();
        static void setDefaultAllowCookies(bool);
#endif

        static bool compare(const ResourceRequest&, const ResourceRequest&);

    protected:
        // Used when ResourceRequest is initialized from a platform representation of the request
        ResourceRequestBase()
            : m_resourceRequestUpdated(false)
            , m_platformRequestUpdated(true)
            , m_resourceRequestBodyUpdated(false)
            , m_platformRequestBodyUpdated(true)
            , m_reportUploadProgress(false)
            , m_reportLoadTiming(false)
            , m_reportRawHeaders(false)
#if ENABLE(INSPECTOR)
            , m_hiddenFromInspector(false)
#endif
            , m_priority(ResourceLoadPriorityLow)
        {
        }

        ResourceRequestBase(const URL& url, ResourceRequestCachePolicy policy)
            : m_url(url)
            , m_timeoutInterval(s_defaultTimeoutInterval)
            , m_httpMethod(ASCIILiteral("GET"))
            , m_cachePolicy(policy)
#if !PLATFORM(IOS)
            , m_allowCookies(true)
#else
            , m_allowCookies(ResourceRequestBase::defaultAllowCookies())
#endif
            , m_resourceRequestUpdated(true)
            , m_platformRequestUpdated(false)
            , m_resourceRequestBodyUpdated(true)
            , m_platformRequestBodyUpdated(false)
            , m_reportUploadProgress(false)
            , m_reportLoadTiming(false)
            , m_reportRawHeaders(false)
#if ENABLE(INSPECTOR)
            , m_hiddenFromInspector(false)
#endif
            , m_priority(ResourceLoadPriorityLow)
        {
        }

        void updatePlatformRequest(HTTPBodyUpdatePolicy = DoNotUpdateHTTPBody) const;
        void updateResourceRequest(HTTPBodyUpdatePolicy = DoNotUpdateHTTPBody) const;

        // The ResourceRequest subclass may "shadow" this method to compare platform specific fields
        static bool platformCompare(const ResourceRequest&, const ResourceRequest&) { return true; }

        URL m_url;
        double m_timeoutInterval; // 0 is a magic value for platform default on platforms that have one.
        URL m_firstPartyForCookies;
        String m_httpMethod;
        HTTPHeaderMap m_httpHeaderFields;
        Vector<String> m_responseContentDispositionEncodingFallbackArray;
        RefPtr<FormData> m_httpBody;
        unsigned m_cachePolicy : 3;
        bool m_allowCookies : 1;
        mutable bool m_resourceRequestUpdated : 1;
        mutable bool m_platformRequestUpdated : 1;
        mutable bool m_resourceRequestBodyUpdated : 1;
        mutable bool m_platformRequestBodyUpdated : 1;
        bool m_reportUploadProgress : 1;
        bool m_reportLoadTiming : 1;
        bool m_reportRawHeaders : 1;
#if ENABLE(INSPECTOR)
        bool m_hiddenFromInspector : 1;
#endif
        ResourceLoadPriority m_priority : 4; // not unsigned because ResourceLoadPriority has negative values

    private:
        const ResourceRequest& asResourceRequest() const;

        static double s_defaultTimeoutInterval;
#if PLATFORM(IOS)
        static bool s_defaultAllowCookies;
#endif
    };

    bool equalIgnoringHeaderFields(const ResourceRequestBase&, const ResourceRequestBase&);

    inline bool operator==(const ResourceRequest& a, const ResourceRequest& b) { return ResourceRequestBase::compare(a, b); }
    inline bool operator!=(ResourceRequest& a, const ResourceRequest& b) { return !(a == b); }

    struct CrossThreadResourceRequestDataBase {
        WTF_MAKE_NONCOPYABLE(CrossThreadResourceRequestDataBase); WTF_MAKE_FAST_ALLOCATED;
    public:
        CrossThreadResourceRequestDataBase() { }
        URL m_url;

        ResourceRequestCachePolicy m_cachePolicy;
        double m_timeoutInterval;
        URL m_firstPartyForCookies;

        String m_httpMethod;
        std::unique_ptr<CrossThreadHTTPHeaderMapData> m_httpHeaders;
        Vector<String> m_responseContentDispositionEncodingFallbackArray;
        RefPtr<FormData> m_httpBody;
        bool m_allowCookies;
        ResourceLoadPriority m_priority;
    };
    
    unsigned initializeMaximumHTTPConnectionCountPerHost();
#if PLATFORM(IOS)
    void initializeHTTPConnectionSettingsOnStartup();
#endif

template<class Encoder>
void ResourceRequestBase::encodeWithoutPlatformData(Encoder& encoder) const
{
    ASSERT(!m_httpBody);
    ASSERT(!m_platformRequestUpdated);
    encoder << m_url.string();
    encoder << m_timeoutInterval;
    encoder << m_firstPartyForCookies.string();
    encoder << m_httpMethod;
    encoder << m_httpHeaderFields;
    encoder << m_responseContentDispositionEncodingFallbackArray;
    encoder.encodeEnum(m_cachePolicy);
    encoder << m_allowCookies;
    encoder.encodeEnum(m_priority);
}

template<class Decoder>
bool ResourceRequestBase::decodeWithoutPlatformData(Decoder& decoder)
{
    String url;
    if (!decoder.decode(url))
        return false;
    m_url = URL(ParsedURLString, url);

    if (!decoder.decode(m_timeoutInterval))
        return false;

    String firstPartyForCookies;
    if (!decoder.decode(firstPartyForCookies))
        return false;
    m_firstPartyForCookies = URL(ParsedURLString, firstPartyForCookies);

    if (!decoder.decode(m_httpMethod))
        return false;

    if (!decoder.decode(m_httpHeaderFields))
        return false;

    if (!decoder.decode(m_responseContentDispositionEncodingFallbackArray))
        return false;

    ResourceRequestCachePolicy cachePolicy;
    if (!decoder.decodeEnum(cachePolicy))
        return false;
    m_cachePolicy = cachePolicy;

    bool allowCookies;
    if (!decoder.decode(allowCookies))
        return false;
    m_allowCookies = allowCookies;

    ResourceLoadPriority priority;
    if (!decoder.decodeEnum(priority))
        return false;
    m_priority = priority;

    return true;
}

} // namespace WebCore

#endif // ResourceRequestBase_h
