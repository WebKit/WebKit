/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "ResourceRequestCFNet.h"

#include "HTTPHeaderNames.h"
#include "RegistrableDomain.h"
#include "ResourceRequest.h"
#include <wtf/cf/TypeCastsCF.h>

#if ENABLE(PUBLIC_SUFFIX_LIST)
#include "PublicSuffix.h"
#endif

#if USE(CFURLCONNECTION)
#include "FormDataStreamCFNet.h"
#include <CFNetwork/CFURLRequestPriv.h>
#include <pal/spi/win/CFNetworkSPIWin.h>
#include <wtf/text/CString.h>
#endif

#if PLATFORM(COCOA)
#include "ResourceLoadPriority.h"
#include <dlfcn.h>
#include <pal/spi/cf/CFNetworkSPI.h>
#endif

WTF_DECLARE_CF_TYPE_TRAIT(CFURL);

namespace WebCore {

// FIXME: Make this a NetworkingContext property.
#if PLATFORM(IOS_FAMILY)
bool ResourceRequest::s_httpPipeliningEnabled = true;
#else
bool ResourceRequest::s_httpPipeliningEnabled = false;
#endif

#if USE(CFURLCONNECTION)

typedef void (*CFURLRequestSetContentDispositionEncodingFallbackArrayFunction)(CFMutableURLRequestRef, CFArrayRef);
typedef CFArrayRef (*CFURLRequestCopyContentDispositionEncodingFallbackArrayFunction)(CFURLRequestRef);

#if PLATFORM(WIN)
static HMODULE findCFNetworkModule()
{
#ifndef DEBUG_ALL
    return GetModuleHandleA("CFNetwork");
#else
    return GetModuleHandleA("CFNetwork_debug");
#endif
}

static CFURLRequestSetContentDispositionEncodingFallbackArrayFunction findCFURLRequestSetContentDispositionEncodingFallbackArrayFunction()
{
    return reinterpret_cast<CFURLRequestSetContentDispositionEncodingFallbackArrayFunction>(GetProcAddress(findCFNetworkModule(), "_CFURLRequestSetContentDispositionEncodingFallbackArray"));
}

static CFURLRequestCopyContentDispositionEncodingFallbackArrayFunction findCFURLRequestCopyContentDispositionEncodingFallbackArrayFunction()
{
    return reinterpret_cast<CFURLRequestCopyContentDispositionEncodingFallbackArrayFunction>(GetProcAddress(findCFNetworkModule(), "_CFURLRequestCopyContentDispositionEncodingFallbackArray"));
}
#endif

static void setContentDispositionEncodingFallbackArray(CFMutableURLRequestRef request, CFArrayRef fallbackArray)
{
    static CFURLRequestSetContentDispositionEncodingFallbackArrayFunction function = findCFURLRequestSetContentDispositionEncodingFallbackArrayFunction();
    if (function)
        function(request, fallbackArray);
}

static CFArrayRef copyContentDispositionEncodingFallbackArray(CFURLRequestRef request)
{
    static CFURLRequestCopyContentDispositionEncodingFallbackArrayFunction function = findCFURLRequestCopyContentDispositionEncodingFallbackArrayFunction();
    if (!function)
        return 0;
    return function(request);
}

CFURLRequestRef ResourceRequest::cfURLRequest(HTTPBodyUpdatePolicy bodyPolicy) const
{
    updatePlatformRequest(bodyPolicy);

    return m_cfRequest.get();
}

static inline void setHeaderFields(CFMutableURLRequestRef request, const HTTPHeaderMap& requestHeaders) 
{
    // Remove existing headers first, as some of them may no longer be present in the map.
    RetainPtr<CFDictionaryRef> oldHeaderFields = adoptCF(CFURLRequestCopyAllHTTPHeaderFields(request));
    CFIndex oldHeaderFieldCount = CFDictionaryGetCount(oldHeaderFields.get());
    if (oldHeaderFieldCount) {
        Vector<CFStringRef> oldHeaderFieldNames(oldHeaderFieldCount);
        CFDictionaryGetKeysAndValues(oldHeaderFields.get(), reinterpret_cast<const void**>(&oldHeaderFieldNames[0]), 0);
        for (CFIndex i = 0; i < oldHeaderFieldCount; ++i)
            CFURLRequestSetHTTPHeaderFieldValue(request, oldHeaderFieldNames[i], 0);
    }

    for (const auto& header : requestHeaders)
        CFURLRequestSetHTTPHeaderFieldValue(request, header.key.createCFString().get(), httpHeaderValueUsingSuitableEncoding(header).get());
}

static inline CFURLRequestCachePolicy toPlatformRequestCachePolicy(ResourceRequestCachePolicy policy)
{
    switch (policy) {
    case ResourceRequestCachePolicy::UseProtocolCachePolicy:
        return kCFURLRequestCachePolicyProtocolDefault;
    case ResourceRequestCachePolicy::ReturnCacheDataElseLoad:
        return kCFURLRequestCachePolicyReturnCacheDataElseLoad;
    case ResourceRequestCachePolicy::ReturnCacheDataDontLoad:
        return kCFURLRequestCachePolicyReturnCacheDataDontLoad;
    case ResourceRequestCachePolicy::ReloadIgnoringCacheData:
    case ResourceRequestCachePolicy::DoNotUseAnyCache:
    case ResourceRequestCachePolicy::RefreshAnyCacheData:
        return kCFURLRequestCachePolicyReloadIgnoringCache;
    }

    ASSERT_NOT_REACHED();
    return kCFURLRequestCachePolicyReloadIgnoringCache;
}

static inline ResourceRequestCachePolicy fromPlatformRequestCachePolicy(CFURLRequestCachePolicy policy)
{
    switch (policy) {
    case kCFURLRequestCachePolicyProtocolDefault:
        return ResourceRequestCachePolicy::UseProtocolCachePolicy;
    case kCFURLRequestCachePolicyReloadIgnoringCache:
        return ResourceRequestCachePolicy::ReloadIgnoringCacheData;
    case kCFURLRequestCachePolicyReturnCacheDataElseLoad:
        return ResourceRequestCachePolicy::ReturnCacheDataElseLoad;
    case kCFURLRequestCachePolicyReturnCacheDataDontLoad:
        return ResourceRequestCachePolicy::ReturnCacheDataDontLoad;
    default:
        return ResourceRequestCachePolicy::ReloadIgnoringCacheData;
    }
}

#if PLATFORM(IOS_FAMILY)
static CFURLRef siteForCookies(ResourceRequest::SameSiteDisposition disposition, CFURLRef url)
{
    switch (disposition) {
    case ResourceRequest::SameSiteDisposition::Unspecified:
        return { };
    case ResourceRequest::SameSiteDisposition::SameSite:
        return url;
    case ResourceRequest::SameSiteDisposition::CrossSite:
        static CFURLRef emptyURL = CFURLCreateWithString(nullptr, CFSTR(""), nullptr);
        return emptyURL;
    }
}
#endif

void ResourceRequest::doUpdatePlatformRequest()
{
    CFMutableURLRequestRef cfRequest;

    RetainPtr<CFURLRef> url = ResourceRequest::url().createCFURL();
    RetainPtr<CFURLRef> firstPartyForCookies = ResourceRequest::firstPartyForCookies().createCFURL();
    double timeoutInterval = ResourceRequestBase::timeoutInterval() ? ResourceRequestBase::timeoutInterval() : ResourceRequestBase::defaultTimeoutInterval();
    if (m_cfRequest) {
        cfRequest = CFURLRequestCreateMutableCopy(0, m_cfRequest.get());
        CFURLRequestSetURL(cfRequest, url.get());
        CFURLRequestSetMainDocumentURL(cfRequest, firstPartyForCookies.get());
        CFURLRequestSetCachePolicy(cfRequest, toPlatformRequestCachePolicy(cachePolicy()));
        CFURLRequestSetTimeoutInterval(cfRequest, timeoutInterval);
    } else
        cfRequest = CFURLRequestCreateMutable(0, url.get(), toPlatformRequestCachePolicy(cachePolicy()), timeoutInterval, firstPartyForCookies.get());

    CFURLRequestSetHTTPRequestMethod(cfRequest, httpMethod().createCFString().get());

    if (httpPipeliningEnabled())
        CFURLRequestSetShouldPipelineHTTP(cfRequest, true, true);

    if (resourcePrioritiesEnabled()) {
        CFURLRequestSetRequestPriority(cfRequest, toPlatformRequestPriority(priority()));

        // Used by PLT to ignore very low priority beacon and ping loads.
        if (priority() == ResourceLoadPriority::VeryLow)
            _CFURLRequestSetProtocolProperty(cfRequest, CFSTR("WKVeryLowLoadPriority"), kCFBooleanTrue);
    }

    setHeaderFields(cfRequest, httpHeaderFields());

    CFURLRequestSetShouldHandleHTTPCookies(cfRequest, allowCookies());

#if PLATFORM(IOS_FAMILY)
    _CFURLRequestSetProtocolProperty(cfRequest, CFSTR("_kCFHTTPCookiePolicyPropertySiteForCookies"), siteForCookies(m_sameSiteDisposition, url.get()));

    int isTopSite = m_isTopSite;
    RetainPtr<CFNumberRef> isTopSiteCF = adoptCF(CFNumberCreate(nullptr, kCFNumberIntType, &isTopSite));
    _CFURLRequestSetProtocolProperty(cfRequest, CFSTR("_kCFHTTPCookiePolicyPropertyisTopSite"), isTopSiteCF.get());
#endif

    unsigned fallbackCount = m_responseContentDispositionEncodingFallbackArray.size();
    RetainPtr<CFMutableArrayRef> encodingFallbacks = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, fallbackCount, 0));
    for (unsigned i = 0; i != fallbackCount; ++i) {
        RetainPtr<CFStringRef> encodingName = m_responseContentDispositionEncodingFallbackArray[i].createCFString();
        CFStringEncoding encoding = CFStringConvertIANACharSetNameToEncoding(encodingName.get());
        if (encoding != kCFStringEncodingInvalidId)
            CFArrayAppendValue(encodingFallbacks.get(), reinterpret_cast<const void*>(encoding));
    }
    setContentDispositionEncodingFallbackArray(cfRequest, encodingFallbacks.get());

#if ENABLE(CACHE_PARTITIONING)
    String partition = cachePartition();
    if (!partition.isNull() && !partition.isEmpty()) {
        CString utf8String = partition.utf8();
        RetainPtr<CFStringRef> partitionValue = adoptCF(CFStringCreateWithBytes(0, reinterpret_cast<const UInt8*>(utf8String.data()), utf8String.length(), kCFStringEncodingUTF8, false));
        _CFURLRequestSetProtocolProperty(cfRequest, _kCFURLCachePartitionKey, partitionValue.get());
    }
#endif

    m_cfRequest = adoptCF(cfRequest);
}

void ResourceRequest::doUpdatePlatformHTTPBody()
{
    CFMutableURLRequestRef cfRequest;

    RetainPtr<CFURLRef> url = ResourceRequest::url().createCFURL();
    RetainPtr<CFURLRef> firstPartyForCookies = ResourceRequest::firstPartyForCookies().createCFURL();
    double timeoutInterval = ResourceRequestBase::timeoutInterval() ? ResourceRequestBase::timeoutInterval() : ResourceRequestBase::defaultTimeoutInterval();
    if (m_cfRequest) {
        cfRequest = CFURLRequestCreateMutableCopy(0, m_cfRequest.get());
        CFURLRequestSetURL(cfRequest, url.get());
        CFURLRequestSetMainDocumentURL(cfRequest, firstPartyForCookies.get());
        CFURLRequestSetCachePolicy(cfRequest, toPlatformRequestCachePolicy(cachePolicy()));
        CFURLRequestSetTimeoutInterval(cfRequest, timeoutInterval);
    } else
        cfRequest = CFURLRequestCreateMutable(0, url.get(), toPlatformRequestCachePolicy(cachePolicy()), timeoutInterval, firstPartyForCookies.get());

    FormData* formData = httpBody();
    if (formData && !formData->isEmpty())
        WebCore::setHTTPBody(cfRequest, formData);

    if (RetainPtr<CFReadStreamRef> bodyStream = adoptCF(CFURLRequestCopyHTTPRequestBodyStream(cfRequest))) {
        // For streams, provide a Content-Length to avoid using chunked encoding, and to get accurate total length in callbacks.
        if (RetainPtr<CFStringRef> lengthString = adoptCF(static_cast<CFStringRef>(CFReadStreamCopyProperty(bodyStream.get(), formDataStreamLengthPropertyName())))) {
            CFURLRequestSetHTTPHeaderFieldValue(cfRequest, CFSTR("Content-Length"), lengthString.get());
            // Since resource request is already marked updated, we need to keep it up to date too.
            ASSERT(m_resourceRequestUpdated);
            m_httpHeaderFields.set(HTTPHeaderName::ContentLength, lengthString.get());
        }
    }

    m_cfRequest = adoptCF(cfRequest);
}

void ResourceRequest::doUpdateResourceRequest()
{
    if (!m_cfRequest) {
        *this = ResourceRequest();
        return;
    }

    m_url = CFURLRequestGetURL(m_cfRequest.get());

    if (m_cachePolicy == ResourceRequestCachePolicy::UseProtocolCachePolicy)
        m_cachePolicy = fromPlatformRequestCachePolicy(CFURLRequestGetCachePolicy(m_cfRequest.get()));
    m_timeoutInterval = CFURLRequestGetTimeoutInterval(m_cfRequest.get());
    m_firstPartyForCookies = CFURLRequestGetMainDocumentURL(m_cfRequest.get());
    if (CFStringRef method = CFURLRequestCopyHTTPRequestMethod(m_cfRequest.get())) {
        m_httpMethod = method;
        CFRelease(method);
    }
    m_allowCookies = CFURLRequestShouldHandleHTTPCookies(m_cfRequest.get());

    if (resourcePrioritiesEnabled())
        m_priority = toResourceLoadPriority(CFURLRequestGetRequestPriority(m_cfRequest.get()));

#if PLATFORM(IOS_FAMILY)
    RetainPtr<CFURLRef> siteForCookies = adoptCF(checked_cf_cast<CFURLRef>(_CFURLRequestCopyProtocolPropertyForKey(m_cfRequest.get(), CFSTR("_kCFHTTPCookiePolicyPropertySiteForCookies"))));
    m_sameSiteDisposition = !siteForCookies ? SameSiteDisposition::Unspecified : (areRegistrableDomainsEqual(siteForCookies.get(), m_url) ? SameSiteDisposition::SameSite : SameSiteDisposition::CrossSite);

    RetainPtr<CFNumberRef> isTopSiteCF = adoptCF(checked_cf_cast<CFNumber>(_CFURLRequestCopyProtocolPropertyForKey(m_cfRequest.get(), CFSTR("_kCFHTTPCookiePolicyPropertyisTopSite"))));
    if (!isTopSiteCF)
        m_isTopSite = false;
    else {
        int isTopSite = 0;
        CFNumberGetValue(isTopSiteCF.get(), kCFNumberIntType, &isTopSite);
        m_isTopSite = isTopSite;
    }
#endif

    m_httpHeaderFields.clear();
    if (CFDictionaryRef headers = CFURLRequestCopyAllHTTPHeaderFields(m_cfRequest.get())) {
        CFIndex headerCount = CFDictionaryGetCount(headers);
        Vector<const void*, 128> keys(headerCount);
        Vector<const void*, 128> values(headerCount);
        CFDictionaryGetKeysAndValues(headers, keys.data(), values.data());
        for (int i = 0; i < headerCount; ++i)
            m_httpHeaderFields.set((CFStringRef)keys[i], (CFStringRef)values[i]);
        CFRelease(headers);
    }

    m_responseContentDispositionEncodingFallbackArray.clear();
    RetainPtr<CFArrayRef> encodingFallbacks = adoptCF(copyContentDispositionEncodingFallbackArray(m_cfRequest.get()));
    if (encodingFallbacks) {
        CFIndex count = CFArrayGetCount(encodingFallbacks.get());
        for (CFIndex i = 0; i < count; ++i) {
            CFStringEncoding encoding = reinterpret_cast<CFIndex>(CFArrayGetValueAtIndex(encodingFallbacks.get(), i));
            if (encoding != kCFStringEncodingInvalidId)
                m_responseContentDispositionEncodingFallbackArray.append(CFStringConvertEncodingToIANACharSetName(encoding));
        }
    }

#if ENABLE(CACHE_PARTITIONING)
    RetainPtr<CFStringRef> cachePartition = adoptCF(static_cast<CFStringRef>(_CFURLRequestCopyProtocolPropertyForKey(m_cfRequest.get(), _kCFURLCachePartitionKey)));
    if (cachePartition)
        m_cachePartition = cachePartition.get();
#endif
}

void ResourceRequest::doUpdateResourceHTTPBody()
{
    if (!m_cfRequest) {
        m_httpBody = nullptr;
        return;
    }

    if (RetainPtr<CFDataRef> bodyData = adoptCF(CFURLRequestCopyHTTPRequestBody(m_cfRequest.get())))
        m_httpBody = FormData::create(CFDataGetBytePtr(bodyData.get()), CFDataGetLength(bodyData.get()));
    else if (RetainPtr<CFReadStreamRef> bodyStream = adoptCF(CFURLRequestCopyHTTPRequestBodyStream(m_cfRequest.get()))) {
        FormData* formData = httpBodyFromStream(bodyStream.get());
        // There is no FormData object if a client provided a custom data stream.
        // We shouldn't be looking at http body after client callbacks.
        ASSERT(formData);
        if (formData)
            m_httpBody = formData;
    }
}


void ResourceRequest::setStorageSession(CFURLStorageSessionRef storageSession)
{
    updatePlatformRequest();

    auto cfRequest = CFURLRequestCreateMutableCopy(0, m_cfRequest.get());
    if (storageSession)
        _CFURLRequestSetStorageSession(cfRequest, storageSession);
    m_cfRequest = adoptCF(cfRequest);
}

#endif // USE(CFURLCONNECTION)

void ResourceRequest::updateFromDelegatePreservingOldProperties(const ResourceRequest& delegateProvidedRequest)
{
    // These are things we don't want willSendRequest delegate to mutate or reset.
    ResourceLoadPriority oldPriority = priority();
    RefPtr<FormData> oldHTTPBody = httpBody();
    bool isHiddenFromInspector = hiddenFromInspector();
    auto oldRequester = requester();
    auto oldInitiatorIdentifier = initiatorIdentifier();
    auto oldInspectorInitiatorNodeIdentifier = inspectorInitiatorNodeIdentifier();

    *this = delegateProvidedRequest;

    setPriority(oldPriority);
    setHTTPBody(WTFMove(oldHTTPBody));
    setHiddenFromInspector(isHiddenFromInspector);
    setRequester(oldRequester);
    setInitiatorIdentifier(oldInitiatorIdentifier);
    if (oldInspectorInitiatorNodeIdentifier)
        setInspectorInitiatorNodeIdentifier(*oldInspectorInitiatorNodeIdentifier);
}

bool ResourceRequest::httpPipeliningEnabled()
{
    return s_httpPipeliningEnabled;
}

void ResourceRequest::setHTTPPipeliningEnabled(bool flag)
{
    s_httpPipeliningEnabled = flag;
}

// FIXME: It is confusing that this function both sets connection count and determines maximum request count at network layer. This can and should be done separately.
unsigned initializeMaximumHTTPConnectionCountPerHost()
{
    static const unsigned preferredConnectionCount = 6;
    static const unsigned unlimitedRequestCount = 10000;

    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPLoadWidth, preferredConnectionCount);
    unsigned maximumHTTPConnectionCountPerHost = _CFNetworkHTTPConnectionCacheGetLimit(kHTTPLoadWidth);

    Boolean keyExistsAndHasValidFormat = false;
    Boolean prefValue = CFPreferencesGetAppBooleanValue(CFSTR("WebKitEnableHTTPPipelining"), kCFPreferencesCurrentApplication, &keyExistsAndHasValidFormat);
    if (keyExistsAndHasValidFormat)
        ResourceRequest::setHTTPPipeliningEnabled(prefValue);

    // Use WebCore scheduler when we can't use request priorities with CFNetwork.
    if (!ResourceRequest::resourcePrioritiesEnabled())
        return maximumHTTPConnectionCountPerHost;

    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPPriorityNumLevels, resourceLoadPriorityCount);
#if !PLATFORM(WIN)
    // FIXME: <rdar://problem/9375609> Implement minimum fast lane priority setting on Windows
    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPMinimumFastLanePriority, toPlatformRequestPriority(ResourceLoadPriority::Medium));
#endif

    return unlimitedRequestCount;
}

#if PLATFORM(IOS_FAMILY)
void initializeHTTPConnectionSettingsOnStartup()
{
    // This need to be called from WebKitInitialize so the calls happen early enough, before any requests are made. <rdar://problem/9691871>
    // Desktop doesn't have early initialization so it is not clear how this should be done there. The CFNetwork SPI probably
    // needs to become more forgiving.
    // We can't read settings here as this is called too early for that. All values need to be constants.
    static const unsigned preferredConnectionCount = 6;
    static const unsigned fastLaneConnectionCount = 1;
    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPLoadWidth, preferredConnectionCount);
    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPPriorityNumLevels, resourceLoadPriorityCount);
    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPMinimumFastLanePriority, toPlatformRequestPriority(ResourceLoadPriority::Medium));
    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPNumFastLanes, fastLaneConnectionCount);
}
#endif

#if PLATFORM(COCOA)
CFStringRef ResourceRequest::isUserInitiatedKey()
{
    static CFStringRef key = CFSTR("ResourceRequestIsUserInitiatedKey");
    return key;
}
#endif

} // namespace WebCore
