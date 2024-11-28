/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "LegacyCustomProtocolManagerClient.h"

#import "CacheStoragePolicy.h"
#import "LegacyCustomProtocolManagerProxy.h"
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ResourceResponse.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/SpanCocoa.h>

@interface WKCustomProtocolLoader : NSObject <NSURLConnectionDelegate> {
@private
    WeakPtr<WebKit::LegacyCustomProtocolManagerProxy> _customProtocolManagerProxy;
    Markable<WebKit::LegacyCustomProtocolID> _customProtocolID;
    NSURLCacheStoragePolicy _storagePolicy;
    RetainPtr<NSURLConnection> _urlConnection;
}
- (id)initWithLegacyCustomProtocolManagerProxy:(WebKit::LegacyCustomProtocolManagerProxy&)customProtocolManagerProxy customProtocolID:(WebKit::LegacyCustomProtocolID)customProtocolID request:(NSURLRequest *)request;
- (void)cancel;
@end

@implementation WKCustomProtocolLoader

- (id)initWithLegacyCustomProtocolManagerProxy:(WebKit::LegacyCustomProtocolManagerProxy&)customProtocolManagerProxy customProtocolID:(WebKit::LegacyCustomProtocolID)customProtocolID request:(NSURLRequest *)request
{
    self = [super init];
    if (!self)
        return nil;

    ASSERT(request);
    _customProtocolManagerProxy = customProtocolManagerProxy;
    _customProtocolID = customProtocolID;
    _storagePolicy = NSURLCacheStorageNotAllowed;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    _urlConnection = adoptNS([[NSURLConnection alloc] initWithRequest:request delegate:self startImmediately:NO]);
    [_urlConnection scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    [_urlConnection start];
ALLOW_DEPRECATED_DECLARATIONS_END

    return self;
}

- (void)dealloc
{
    [_urlConnection cancel];
    [super dealloc];
}

- (void)cancel
{
    ASSERT(_customProtocolManagerProxy);
    _customProtocolManagerProxy = nullptr;
    [_urlConnection cancel];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    RefPtr customProtocolManagerProxy = _customProtocolManagerProxy.get();
    if (!customProtocolManagerProxy)
        return;

    WebCore::ResourceError coreError(error);
    customProtocolManagerProxy->didFailWithError(*_customProtocolID, coreError);
    customProtocolManagerProxy->stopLoading(*_customProtocolID);
}

- (NSCachedURLResponse *)connection:(NSURLConnection *)connection willCacheResponse:(NSCachedURLResponse *)cachedResponse
{
    ASSERT(_storagePolicy == NSURLCacheStorageNotAllowed);
    _storagePolicy = [cachedResponse storagePolicy];
    return cachedResponse;
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
    RefPtr customProtocolManagerProxy = _customProtocolManagerProxy.get();
    if (!customProtocolManagerProxy)
        return;

    WebCore::ResourceResponse coreResponse(response);
    customProtocolManagerProxy->didReceiveResponse(*_customProtocolID, coreResponse, WebKit::toCacheStoragePolicy(_storagePolicy));
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
    RefPtr customProtocolManagerProxy = _customProtocolManagerProxy.get();
    if (!customProtocolManagerProxy)
        return;

    customProtocolManagerProxy->didLoadData(*_customProtocolID, span(data));
}

- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse
{
    RefPtr customProtocolManagerProxy = _customProtocolManagerProxy.get();
    if (!customProtocolManagerProxy)
        return nil;

    if (redirectResponse) {
        customProtocolManagerProxy->wasRedirectedToRequest(*_customProtocolID, request, redirectResponse);
        return nil;
    }
    return request;
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    RefPtr customProtocolManagerProxy = _customProtocolManagerProxy.get();
    if (!customProtocolManagerProxy)
        return;

    customProtocolManagerProxy->didFinishLoading(*_customProtocolID);
    customProtocolManagerProxy->stopLoading(*_customProtocolID);
}

@end

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LegacyCustomProtocolManagerClient);

using namespace WebCore;

void LegacyCustomProtocolManagerClient::startLoading(LegacyCustomProtocolManagerProxy& manager, WebKit::LegacyCustomProtocolID customProtocolID, const ResourceRequest& coreRequest)
{
    NSURLRequest *request = coreRequest.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
    if (!request)
        return;

    auto loader = adoptNS([[WKCustomProtocolLoader alloc] initWithLegacyCustomProtocolManagerProxy:manager customProtocolID:customProtocolID request:request]);
    ASSERT(loader);
    ASSERT(!m_loaderMap.contains(customProtocolID));
    m_loaderMap.add(customProtocolID, WTFMove(loader));
}

void LegacyCustomProtocolManagerClient::stopLoading(LegacyCustomProtocolManagerProxy&, WebKit::LegacyCustomProtocolID customProtocolID)
{
    m_loaderMap.remove(customProtocolID);
}

void LegacyCustomProtocolManagerClient::invalidate(LegacyCustomProtocolManagerProxy&)
{
    while (!m_loaderMap.isEmpty()) {
        auto loader = m_loaderMap.take(m_loaderMap.begin()->key);
        ASSERT(loader);
        [loader cancel];
    }
}

} // namespace WebKit
