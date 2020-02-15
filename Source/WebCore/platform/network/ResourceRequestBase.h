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
#include "FrameLoaderTypes.h"
#include "HTTPHeaderMap.h"
#include "IntRect.h"
#include "ResourceLoadPriority.h"
#include <wtf/URL.h>

namespace WebCore {

enum class ResourceRequestCachePolicy : uint8_t {
    UseProtocolCachePolicy, // normal load, equivalent to fetch "default" cache mode.
    ReloadIgnoringCacheData, // reload, equivalent to fetch "reload"cache mode.
    ReturnCacheDataElseLoad, // back/forward or encoding change - allow stale data, equivalent to fetch "force-cache" cache mode.
    ReturnCacheDataDontLoad, // results of a post - allow stale data and only use cache, equivalent to fetch "only-if-cached" cache mode.
    DoNotUseAnyCache, // Bypass the cache entirely, equivalent to fetch "no-store" cache mode.
    RefreshAnyCacheData, // Serve cache data only if revalidated, equivalent to fetch "no-cache" mode.
};

enum HTTPBodyUpdatePolicy : uint8_t {
    DoNotUpdateHTTPBody,
    UpdateHTTPBody
};

class ResourceRequest;
class ResourceResponse;

// Do not use this type directly.  Use ResourceRequest instead.
class ResourceRequestBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT ResourceRequest isolatedCopy() const;
    WEBCORE_EXPORT void setAsIsolatedCopy(const ResourceRequest&);

    WEBCORE_EXPORT bool isNull() const;
    WEBCORE_EXPORT bool isEmpty() const;

    WEBCORE_EXPORT const URL& url() const;
    WEBCORE_EXPORT void setURL(const URL& url);

    WEBCORE_EXPORT ResourceRequest redirectedRequest(const ResourceResponse&, bool shouldClearReferrerOnHTTPSToHTTPRedirect) const;

    WEBCORE_EXPORT void removeCredentials();

    WEBCORE_EXPORT ResourceRequestCachePolicy cachePolicy() const;
    WEBCORE_EXPORT void setCachePolicy(ResourceRequestCachePolicy cachePolicy);
    
    WEBCORE_EXPORT double timeoutInterval() const; // May return 0 when using platform default.
    void setTimeoutInterval(double timeoutInterval);
    
    WEBCORE_EXPORT const URL& firstPartyForCookies() const;
    WEBCORE_EXPORT void setFirstPartyForCookies(const URL&);

    // Same-Site cookies; see <https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-00#section-2.1>
    // and <https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-00#section-5.2>.
    // FIXME: For some reason the main resource request may be updated more than once. We start off as Unspecified
    // to detect if we need to compute the same-site and top-site state or not. See FIXME in DocumentLoader::startLoadingMainResource().
    enum class SameSiteDisposition : uint8_t { Unspecified, SameSite, CrossSite };
    bool isSameSiteUnspecified() const { return m_sameSiteDisposition == SameSiteDisposition::Unspecified; }
    WEBCORE_EXPORT bool isSameSite() const; // Whether this request's registrable domain matches the request's initiator's "site for cookies".
    WEBCORE_EXPORT void setIsSameSite(bool);
    WEBCORE_EXPORT bool isTopSite() const; // Whether this request is for a top-level navigation.
    WEBCORE_EXPORT void setIsTopSite(bool);

    WEBCORE_EXPORT const String& httpMethod() const;
    WEBCORE_EXPORT void setHTTPMethod(const String& httpMethod);
    
    WEBCORE_EXPORT const HTTPHeaderMap& httpHeaderFields() const;
    WEBCORE_EXPORT void setHTTPHeaderFields(HTTPHeaderMap);

    WEBCORE_EXPORT String httpHeaderField(const String& name) const;
    WEBCORE_EXPORT String httpHeaderField(HTTPHeaderName) const;
    WEBCORE_EXPORT void setHTTPHeaderField(const String& name, const String& value);
    WEBCORE_EXPORT void setHTTPHeaderField(HTTPHeaderName, const String& value);
    WEBCORE_EXPORT void addHTTPHeaderField(HTTPHeaderName, const String& value);
    WEBCORE_EXPORT void addHTTPHeaderField(const String& name, const String& value);
    WEBCORE_EXPORT void addHTTPHeaderFieldIfNotPresent(HTTPHeaderName, const String&);

    WEBCORE_EXPORT bool hasHTTPHeaderField(HTTPHeaderName) const;

    // Instead of passing a string literal to any of these functions, just use a HTTPHeaderName instead.
    template<size_t length> String httpHeaderField(const char (&)[length]) const = delete;
    template<size_t length> void setHTTPHeaderField(const char (&)[length], const String&) = delete;
    template<size_t length> void addHTTPHeaderField(const char (&)[length], const String&) = delete;

    WEBCORE_EXPORT void clearHTTPAuthorization();

    WEBCORE_EXPORT String httpContentType() const;
    WEBCORE_EXPORT void setHTTPContentType(const String&);
    WEBCORE_EXPORT void clearHTTPContentType();

    bool hasHTTPHeader(HTTPHeaderName) const;

    WEBCORE_EXPORT String httpReferrer() const;
    bool hasHTTPReferrer() const;
    WEBCORE_EXPORT void setHTTPReferrer(const String&);
    WEBCORE_EXPORT void setExistingHTTPReferrerToOriginString();
    WEBCORE_EXPORT void clearHTTPReferrer();

    WEBCORE_EXPORT String httpOrigin() const;
    bool hasHTTPOrigin() const;
    void setHTTPOrigin(const String&);
    WEBCORE_EXPORT void clearHTTPOrigin();

    WEBCORE_EXPORT String httpUserAgent() const;
    WEBCORE_EXPORT void setHTTPUserAgent(const String&);
    void clearHTTPUserAgent();

    String httpAccept() const;
    void setHTTPAccept(const String&);
    void clearHTTPAccept();

    void clearHTTPAcceptEncoding();

    WEBCORE_EXPORT void clearPurpose();

    const Vector<String>& responseContentDispositionEncodingFallbackArray() const { return m_responseContentDispositionEncodingFallbackArray; }
    WEBCORE_EXPORT void setResponseContentDispositionEncodingFallbackArray(const String& encoding1, const String& encoding2 = String(), const String& encoding3 = String());

    WEBCORE_EXPORT FormData* httpBody() const;
    WEBCORE_EXPORT bool hasUpload() const;
    WEBCORE_EXPORT void setHTTPBody(RefPtr<FormData>&&);

    bool allowCookies() const;
    void setAllowCookies(bool allowCookies);

    WEBCORE_EXPORT ResourceLoadPriority priority() const;
    WEBCORE_EXPORT void setPriority(ResourceLoadPriority);

    WEBCORE_EXPORT static String partitionName(const String& domain);
    const String& cachePartition() const { return m_cachePartition; }
    WEBCORE_EXPORT void setCachePartition(const String&);
    void setDomainForCachePartition(const String& domain) { setCachePartition(partitionName(domain)); }

    WEBCORE_EXPORT bool isConditional() const;
    WEBCORE_EXPORT void makeUnconditional();

    // Whether this request should be hidden from the Inspector.
    bool hiddenFromInspector() const { return m_hiddenFromInspector; }
    void setHiddenFromInspector(bool hiddenFromInspector) { m_hiddenFromInspector = hiddenFromInspector; }

    enum class Requester : uint8_t { Unspecified, Main, XHR, Fetch, Media, ImportScripts };
    Requester requester() const { return m_requester; }
    void setRequester(Requester requester) { m_requester = requester; }

    // Who initiated the request so the Inspector can associate it with a context. E.g. a Web Worker.
    String initiatorIdentifier() const { return m_initiatorIdentifier; }
    void setInitiatorIdentifier(const String& identifier) { m_initiatorIdentifier = identifier; }

    // Additional information for the Inspector to be able to identify the node that initiated this request.
    const Optional<int>& inspectorInitiatorNodeIdentifier() const { return m_inspectorInitiatorNodeIdentifier; }
    void setInspectorInitiatorNodeIdentifier(int inspectorInitiatorNodeIdentifier) { m_inspectorInitiatorNodeIdentifier = inspectorInitiatorNodeIdentifier; }

#if USE(SYSTEM_PREVIEW)
    WEBCORE_EXPORT bool isSystemPreview() const;

    WEBCORE_EXPORT SystemPreviewInfo systemPreviewInfo() const;
    WEBCORE_EXPORT void setSystemPreviewInfo(const SystemPreviewInfo&);
#endif

#if !PLATFORM(COCOA)
    bool encodingRequiresPlatformData() const { return true; }
#endif
    template<class Encoder> void encodeWithoutPlatformData(Encoder&) const;
    template<class Decoder> bool decodeWithoutPlatformData(Decoder&);

    WEBCORE_EXPORT static double defaultTimeoutInterval(); // May return 0 when using platform default.
    WEBCORE_EXPORT static void setDefaultTimeoutInterval(double);

    WEBCORE_EXPORT static bool equal(const ResourceRequest&, const ResourceRequest&);

protected:
    // Used when ResourceRequest is initialized from a platform representation of the request
    ResourceRequestBase()
        : m_allowCookies(false)
        , m_resourceRequestUpdated(false)
        , m_platformRequestUpdated(true)
        , m_resourceRequestBodyUpdated(false)
        , m_platformRequestBodyUpdated(true)
        , m_hiddenFromInspector(false)
        , m_isTopSite(false)
    {
    }

    ResourceRequestBase(const URL& url, ResourceRequestCachePolicy policy)
        : m_url(url)
        , m_timeoutInterval(s_defaultTimeoutInterval)
        , m_httpMethod("GET"_s)
        , m_cachePolicy(policy)
        , m_allowCookies(true)
        , m_resourceRequestUpdated(true)
        , m_platformRequestUpdated(false)
        , m_resourceRequestBodyUpdated(true)
        , m_platformRequestBodyUpdated(false)
        , m_hiddenFromInspector(false)
        , m_isTopSite(false)
    {
    }

    void updatePlatformRequest(HTTPBodyUpdatePolicy = HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) const;
    void updateResourceRequest(HTTPBodyUpdatePolicy = HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) const;

    template<class Encoder> void encodeBase(Encoder&) const;
    template<class Decoder> bool decodeBase(Decoder&);

    // The ResourceRequest subclass may "shadow" this method to compare platform specific fields
    static bool platformCompare(const ResourceRequest&, const ResourceRequest&) { return true; }

    URL m_url;
    double m_timeoutInterval; // 0 is a magic value for platform default on platforms that have one.
    URL m_firstPartyForCookies;
    String m_httpMethod;
    String m_initiatorIdentifier;
    String m_cachePartition { emptyString() };
    HTTPHeaderMap m_httpHeaderFields;
    Vector<String> m_responseContentDispositionEncodingFallbackArray;
    RefPtr<FormData> m_httpBody;
    ResourceRequestCachePolicy m_cachePolicy { ResourceRequestCachePolicy::UseProtocolCachePolicy };
    SameSiteDisposition m_sameSiteDisposition { SameSiteDisposition::Unspecified };
    ResourceLoadPriority m_priority { ResourceLoadPriority::Low };
    Requester m_requester { Requester::Unspecified };
    Optional<int> m_inspectorInitiatorNodeIdentifier;
    bool m_allowCookies : 1;
    mutable bool m_resourceRequestUpdated : 1;
    mutable bool m_platformRequestUpdated : 1;
    mutable bool m_resourceRequestBodyUpdated : 1;
    mutable bool m_platformRequestBodyUpdated : 1;
    bool m_hiddenFromInspector : 1;
    bool m_isTopSite : 1;
#if USE(SYSTEM_PREVIEW)
    Optional<SystemPreviewInfo> m_systemPreviewInfo;
#endif

private:
    const ResourceRequest& asResourceRequest() const;

    WEBCORE_EXPORT static double s_defaultTimeoutInterval;
};

bool equalIgnoringHeaderFields(const ResourceRequestBase&, const ResourceRequestBase&);

inline bool operator==(const ResourceRequest& a, const ResourceRequest& b) { return ResourceRequestBase::equal(a, b); }
inline bool operator!=(ResourceRequest& a, const ResourceRequest& b) { return !(a == b); }

WEBCORE_EXPORT unsigned initializeMaximumHTTPConnectionCountPerHost();
#if PLATFORM(IOS_FAMILY)
WEBCORE_EXPORT void initializeHTTPConnectionSettingsOnStartup();
#endif

template<class Encoder>
ALWAYS_INLINE void ResourceRequestBase::encodeBase(Encoder& encoder) const
{
    encoder << m_url;
    encoder << m_timeoutInterval;
    encoder << m_firstPartyForCookies.string();
    encoder << m_httpMethod;
    encoder << m_httpHeaderFields;
    encoder << m_responseContentDispositionEncodingFallbackArray;
    encoder.encodeEnum(m_cachePolicy);
    encoder << m_allowCookies;
    encoder.encodeEnum(m_sameSiteDisposition);
    encoder << m_isTopSite;
    encoder.encodeEnum(m_priority);
    encoder.encodeEnum(m_requester);
}

template<class Decoder>
ALWAYS_INLINE bool ResourceRequestBase::decodeBase(Decoder& decoder)
{
    if (!decoder.decode(m_url))
        return false;

    if (!decoder.decode(m_timeoutInterval))
        return false;

    String firstPartyForCookies;
    if (!decoder.decode(firstPartyForCookies))
        return false;
    m_firstPartyForCookies = URL({ }, firstPartyForCookies);

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

    SameSiteDisposition sameSiteDisposition;
    if (!decoder.decodeEnum(sameSiteDisposition))
        return false;
    m_sameSiteDisposition = sameSiteDisposition;

    bool isTopSite;
    if (!decoder.decode(isTopSite))
        return false;
    m_isTopSite = isTopSite;

    ResourceLoadPriority priority;
    if (!decoder.decodeEnum(priority))
        return false;
    m_priority = priority;

    if (!decoder.decodeEnum(m_requester))
        return false;

    return true;
}

template<class Encoder>
void ResourceRequestBase::encodeWithoutPlatformData(Encoder& encoder) const
{
    ASSERT(!m_httpBody);
    ASSERT(!m_platformRequestUpdated);
    encodeBase(encoder);
}

template<class Decoder>
bool ResourceRequestBase::decodeWithoutPlatformData(Decoder& decoder)
{
    return decodeBase(decoder);
}

} // namespace WebCore

#endif // ResourceRequestBase_h
