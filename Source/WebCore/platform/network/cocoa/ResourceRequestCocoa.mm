/*
 * Copyright (C) 2014-2022 Apple, Inc.  All rights reserved.
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

#import "config.h"
#import "ResourceRequest.h"

#if PLATFORM(COCOA)

#import "FormDataStreamMac.h"
#import "HTTPHeaderNames.h"
#import "RegistrableDomain.h"
#import "ResourceRequestCFNet.h"
#import "RuntimeApplicationChecks.h"
#import <Foundation/Foundation.h>
#import <Foundation/NSURLRequest.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/FileSystem.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/CString.h>

namespace WebCore {

ResourceRequest::ResourceRequest(NSURLRequest *nsRequest)
    : m_nsRequest(nsRequest)
{
#if ENABLE(APP_PRIVACY_REPORT)
    setIsAppInitiated(nsRequest.attribution == NSURLRequestAttributionDeveloper);
#endif
}

ResourceRequest::ResourceRequest(ResourceRequestPlatformData&& platformData, const String& cachePartition, bool hiddenFromInspector
#if USE(SYSTEM_PREVIEW)
    , const std::optional<SystemPreviewInfo>& systemPreviewInfo
#endif
    )
{
    if (platformData.m_urlRequest) {
        setRequester(*platformData.m_requester);
        m_nsRequest = platformData.m_urlRequest;
        setIsAppInitiated(*platformData.m_isAppInitiated);
    }

    setCachePartition(cachePartition);
    setHiddenFromInspector(hiddenFromInspector);
#if USE(SYSTEM_PREVIEW)
    m_systemPreviewInfo = systemPreviewInfo;
#endif
}

ResourceRequestData ResourceRequest::getRequestDataToSerialize() const
{
    if (encodingRequiresPlatformData())
        return getResourceRequestPlatformData();
    return m_requestData;
}

ResourceRequest ResourceRequest::fromResourceRequestData(ResourceRequestData requestData, const String& cachePartition, bool hiddenFromInspector
#if USE(SYSTEM_PREVIEW)
    , const std::optional<SystemPreviewInfo>& systemPreviewInfo
#endif
    )
{
    if (std::holds_alternative<RequestData>(requestData)) {
        
#if USE(SYSTEM_PREVIEW)
        return ResourceRequest(WTFMove(std::get<RequestData>(requestData)), cachePartition, hiddenFromInspector, systemPreviewInfo);
#else
        return ResourceRequest(WTFMove(std::get<RequestData>(requestData)), cachePartition, hiddenFromInspector);
#endif
    }
#if USE(SYSTEM_PREVIEW)
    return ResourceRequest(WTFMove(std::get<ResourceRequestPlatformData>(requestData)), cachePartition, hiddenFromInspector, systemPreviewInfo);
#else
    return ResourceRequest(WTFMove(std::get<ResourceRequestPlatformData>(requestData)), cachePartition, hiddenFromInspector);
#endif
}

NSURLRequest *ResourceRequest::nsURLRequest(HTTPBodyUpdatePolicy bodyPolicy) const
{
    updatePlatformRequest(bodyPolicy);
    auto requestCopy = m_nsRequest;
    return requestCopy.autorelease();
}

ResourceRequestPlatformData ResourceRequest::getResourceRequestPlatformData() const
{
    RELEASE_ASSERT(m_httpBody || m_nsRequest);
    
    auto requestToSerialize = retainPtr(this->nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody));

    if (Class requestClass = [requestToSerialize class]; UNLIKELY(requestClass != [NSURLRequest class] && requestClass != [NSMutableURLRequest class])) {
        WebCore::ResourceRequest request(requestToSerialize.get());
        request.replacePlatformRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
        requestToSerialize = retainPtr(request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody));
    }
    ASSERT([requestToSerialize class] == [NSURLRequest class] || [requestToSerialize class] == [NSMutableURLRequest class]);

    if (!requestToSerialize)
        return ResourceRequestPlatformData { NULL, std::nullopt, std::nullopt };

    // We don't send HTTP body over IPC for better performance.
    // Also, it's not always possible to do, as streams can only be created in process that does networking.
    if ([requestToSerialize HTTPBody] || [requestToSerialize HTTPBodyStream]) {
        auto mutableRequest = adoptNS([requestToSerialize mutableCopy]);
        [mutableRequest setHTTPBody:nil];
        [mutableRequest setHTTPBodyStream:nil];
        requestToSerialize = WTFMove(mutableRequest);
    }
    return ResourceRequestPlatformData { WTFMove(requestToSerialize), isAppInitiated(), requester() };
}

CFURLRequestRef ResourceRequest::cfURLRequest(HTTPBodyUpdatePolicy bodyPolicy) const
{
    return [nsURLRequest(bodyPolicy) _CFURLRequest];
}

static inline ResourceRequestCachePolicy fromPlatformRequestCachePolicy(NSURLRequestCachePolicy policy)
{
    switch (policy) {
    case NSURLRequestUseProtocolCachePolicy:
        return ResourceRequestCachePolicy::UseProtocolCachePolicy;
    case NSURLRequestReturnCacheDataElseLoad:
        return ResourceRequestCachePolicy::ReturnCacheDataElseLoad;
    case NSURLRequestReturnCacheDataDontLoad:
        return ResourceRequestCachePolicy::ReturnCacheDataDontLoad;
    default:
        return ResourceRequestCachePolicy::ReloadIgnoringCacheData;
    }
}

static inline NSURLRequestCachePolicy toPlatformRequestCachePolicy(ResourceRequestCachePolicy policy)
{
    switch (policy) {
    case ResourceRequestCachePolicy::UseProtocolCachePolicy:
        return NSURLRequestUseProtocolCachePolicy;
    case ResourceRequestCachePolicy::ReturnCacheDataElseLoad:
        return NSURLRequestReturnCacheDataElseLoad;
    case ResourceRequestCachePolicy::ReturnCacheDataDontLoad:
        return NSURLRequestReturnCacheDataDontLoad;
    default:
        return NSURLRequestReloadIgnoringLocalCacheData;
    }
}

void ResourceRequest::doUpdateResourceRequest()
{
    m_requestData.m_url = [m_nsRequest URL];

    if (m_requestData.m_cachePolicy == ResourceRequestCachePolicy::UseProtocolCachePolicy)
        m_requestData.m_cachePolicy = fromPlatformRequestCachePolicy([m_nsRequest cachePolicy]);
    m_requestData.m_timeoutInterval = [m_nsRequest timeoutInterval];
    m_requestData.m_firstPartyForCookies = [m_nsRequest mainDocumentURL];

    URL siteForCookies { [m_nsRequest _propertyForKey:@"_kCFHTTPCookiePolicyPropertySiteForCookies"] };
    m_requestData.m_sameSiteDisposition = siteForCookies.isNull() ? SameSiteDisposition::Unspecified : (areRegistrableDomainsEqual(siteForCookies, m_requestData.m_url) ? SameSiteDisposition::SameSite : SameSiteDisposition::CrossSite);

    m_requestData.m_isTopSite = static_cast<NSNumber*>([m_nsRequest _propertyForKey:@"_kCFHTTPCookiePolicyPropertyIsTopLevelNavigation"]).boolValue;

    if (NSString* method = [m_nsRequest HTTPMethod])
        m_requestData.m_httpMethod = method;
    m_requestData.m_allowCookies = [m_nsRequest HTTPShouldHandleCookies];

    if (resourcePrioritiesEnabled())
        m_requestData.m_priority = toResourceLoadPriority(m_nsRequest ? CFURLRequestGetRequestPriority([m_nsRequest _CFURLRequest]) : 0);

    m_requestData.m_httpHeaderFields.clear();
    [[m_nsRequest allHTTPHeaderFields] enumerateKeysAndObjectsUsingBlock: ^(NSString *name, NSString *value, BOOL *) {
        m_requestData.m_httpHeaderFields.set(name, value);
    }];

    m_requestData.m_responseContentDispositionEncodingFallbackArray.clear();
    NSArray *encodingFallbacks = [m_nsRequest contentDispositionEncodingFallbackArray];
    m_requestData.m_responseContentDispositionEncodingFallbackArray.reserveCapacity([encodingFallbacks count]);
    for (NSNumber *encodingFallback in [m_nsRequest contentDispositionEncodingFallbackArray]) {
        CFStringEncoding encoding = CFStringConvertNSStringEncodingToEncoding([encodingFallback unsignedLongValue]);
        if (encoding != kCFStringEncodingInvalidId)
            m_requestData.m_responseContentDispositionEncodingFallbackArray.uncheckedAppend(CFStringConvertEncodingToIANACharSetName(encoding));
    }

    if (m_nsRequest) {
        NSString* cachePartition = [NSURLProtocol propertyForKey:(NSString *)_kCFURLCachePartitionKey inRequest:m_nsRequest.get()];
        if (cachePartition)
            m_cachePartition = cachePartition;
    }
}

void ResourceRequest::doUpdateResourceHTTPBody()
{
    if (NSData* bodyData = [m_nsRequest HTTPBody])
        m_httpBody = FormData::create([bodyData bytes], [bodyData length]);
    else if (NSInputStream* bodyStream = [m_nsRequest HTTPBodyStream]) {
        FormData* formData = httpBodyFromStream(bodyStream);
        // There is no FormData object if a client provided a custom data stream.
        // We shouldn't be looking at http body after client callbacks.
        ASSERT(formData);
        if (formData)
            m_httpBody = formData;
    }
}

static NSURL *siteForCookies(ResourceRequest::SameSiteDisposition disposition, NSURL *url)
{
    switch (disposition) {
    case ResourceRequest::SameSiteDisposition::Unspecified:
        return { };
    case ResourceRequest::SameSiteDisposition::SameSite:
        return url;
    case ResourceRequest::SameSiteDisposition::CrossSite:
        static NSURL *emptyURL = [[NSURL alloc] initWithString:@""];
        return emptyURL;
    }
}

void ResourceRequest::replacePlatformRequest(HTTPBodyUpdatePolicy policy)
{
    updateResourceRequest();
    m_nsRequest = nil;
    m_platformRequestUpdated = false;
    updatePlatformRequest(policy);
}

void ResourceRequest::doUpdatePlatformRequest()
{
    if (isNull()) {
        m_nsRequest = nil;
        return;
    }

    auto nsRequest = adoptNS<NSMutableURLRequest *>([m_nsRequest mutableCopy]);

    if (nsRequest)
        [nsRequest setURL:url()];
    else
        nsRequest = adoptNS([[NSMutableURLRequest alloc] initWithURL:url()]);

#if ENABLE(APP_PRIVACY_REPORT)
    nsRequest.get().attribution = m_requestData.m_isAppInitiated ? NSURLRequestAttributionDeveloper : NSURLRequestAttributionUser;
#endif

    if (ResourceRequest::httpPipeliningEnabled())
        CFURLRequestSetShouldPipelineHTTP([nsRequest _CFURLRequest], true, true);

    if (ResourceRequest::resourcePrioritiesEnabled()) {
        CFURLRequestSetRequestPriority([nsRequest _CFURLRequest], toPlatformRequestPriority(priority()));

        // Used by PLT to ignore very low priority beacon and ping loads.
        if (priority() == ResourceLoadPriority::VeryLow)
            _CFURLRequestSetProtocolProperty([nsRequest _CFURLRequest], CFSTR("WKVeryLowLoadPriority"), kCFBooleanTrue);
    }

    [nsRequest setCachePolicy:toPlatformRequestCachePolicy(cachePolicy())];
    _CFURLRequestSetProtocolProperty([nsRequest _CFURLRequest], kCFURLRequestAllowAllPOSTCaching, kCFBooleanTrue);

    double timeoutInterval = ResourceRequestBase::timeoutInterval();
    if (timeoutInterval)
        [nsRequest setTimeoutInterval:timeoutInterval];
    // Otherwise, respect NSURLRequest default timeout.

    [nsRequest setMainDocumentURL:firstPartyForCookies()];
    if (!httpMethod().isEmpty())
        [nsRequest setHTTPMethod:httpMethod()];
    [nsRequest setHTTPShouldHandleCookies:allowCookies()];

    [nsRequest _setProperty:siteForCookies(m_requestData.m_sameSiteDisposition, [nsRequest URL]) forKey:@"_kCFHTTPCookiePolicyPropertySiteForCookies"];
    [nsRequest _setProperty:m_requestData.m_isTopSite ? @YES : @NO forKey:@"_kCFHTTPCookiePolicyPropertyIsTopLevelNavigation"];

    // Cannot just use setAllHTTPHeaderFields here, because it does not remove headers.
    for (NSString *oldHeaderName in [nsRequest allHTTPHeaderFields])
        [nsRequest setValue:nil forHTTPHeaderField:oldHeaderName];
    for (const auto& header : httpHeaderFields()) {
        auto encodedValue = httpHeaderValueUsingSuitableEncoding(header);
        [nsRequest setValue:(__bridge NSString *)encodedValue.get() forHTTPHeaderField:header.key];
    }

    [nsRequest setContentDispositionEncodingFallbackArray:createNSArray(m_requestData.m_responseContentDispositionEncodingFallbackArray, [] (auto& name) -> NSNumber * {
        auto encoding = CFStringConvertEncodingToNSStringEncoding(CFStringConvertIANACharSetNameToEncoding(name.createCFString().get()));
        if (encoding == kCFStringEncodingInvalidId)
            return nil;
        return @(encoding);
    }).get()];

    String partition = cachePartition();
    if (!partition.isNull() && !partition.isEmpty()) {
        NSString *partitionValue = [NSString stringWithUTF8String:partition.utf8().data()];
        [NSURLProtocol setProperty:partitionValue forKey:(NSString *)_kCFURLCachePartitionKey inRequest:nsRequest.get()];
    }

#if PLATFORM(MAC)
    if (m_requestData.m_url.isLocalFile()) {
        auto filePath = m_requestData.m_url.fileSystemPath();
        if (!filePath.isNull()) {
            auto fileDevice = FileSystem::getFileDeviceId(filePath);
            if (fileDevice && fileDevice.value())
                [nsRequest _setProperty:[NSNumber numberWithInteger:fileDevice.value()] forKey:@"NSURLRequestFileProtocolExpectedDevice"];
        }
    }
#endif

    m_nsRequest = WTFMove(nsRequest);
}

void ResourceRequest::doUpdatePlatformHTTPBody()
{
    if (isNull()) {
        ASSERT(!m_nsRequest);
        return;
    }

    auto nsRequest = adoptNS<NSMutableURLRequest *>([m_nsRequest mutableCopy]);

    if (nsRequest)
        [nsRequest setURL:url()];
    else
        nsRequest = adoptNS([[NSMutableURLRequest alloc] initWithURL:url()]);

#if ENABLE(APP_PRIVACY_REPORT)
    nsRequest.get().attribution = m_requestData.m_isAppInitiated ? NSURLRequestAttributionDeveloper : NSURLRequestAttributionUser;
#endif

    FormData* formData = httpBody();
    if (formData && !formData->isEmpty())
        WebCore::setHTTPBody(nsRequest.get(), formData);

    if (NSInputStream *bodyStream = [nsRequest HTTPBodyStream]) {
        // For streams, provide a Content-Length to avoid using chunked encoding, and to get accurate total length in callbacks.
        NSString *lengthString = [bodyStream propertyForKey:(__bridge NSString *)formDataStreamLengthPropertyName()];
        if (lengthString) {
            [nsRequest setValue:lengthString forHTTPHeaderField:@"Content-Length"];
            // Since resource request is already marked updated, we need to keep it up to date too.
            ASSERT(m_resourceRequestUpdated);
            m_requestData.m_httpHeaderFields.set(HTTPHeaderName::ContentLength, lengthString);
        }
    }

    m_nsRequest = WTFMove(nsRequest);
}

void ResourceRequest::setStorageSession(CFURLStorageSessionRef storageSession)
{
    updatePlatformRequest();
    m_nsRequest = copyRequestWithStorageSession(storageSession, m_nsRequest.get());
}

RetainPtr<NSURLRequest> copyRequestWithStorageSession(CFURLStorageSessionRef storageSession, NSURLRequest *request)
{
    if (!storageSession || !request)
        return adoptNS([request copy]);

    auto cfRequest = adoptCF(CFURLRequestCreateMutableCopy(kCFAllocatorDefault, [request _CFURLRequest]));
    _CFURLRequestSetStorageSession(cfRequest.get(), storageSession);
    auto nsRequest = adoptNS([[NSMutableURLRequest alloc] _initWithCFURLRequest:cfRequest.get()]);
#if ENABLE(APP_PRIVACY_REPORT)
    nsRequest.get().attribution = request.attribution;
#endif
    return nsRequest;
}

NSCachedURLResponse *cachedResponseForRequest(CFURLStorageSessionRef storageSession, NSURLRequest *request)
{
    if (!storageSession)
        return [[NSURLCache sharedURLCache] cachedResponseForRequest:request];

    auto cache = adoptCF(_CFURLStorageSessionCopyCache(kCFAllocatorDefault, storageSession));
    auto cachedResponse = adoptCF(CFURLCacheCopyResponseForRequest(cache.get(), [request _CFURLRequest]));
    if (!cachedResponse)
        return nil;

    return adoptNS([[NSCachedURLResponse alloc] _initWithCFCachedURLResponse:cachedResponse.get()]).autorelease();
}

} // namespace WebCore

#endif // PLATFORM(COCOA)

