/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WebResourceLoader.h"

#if ENABLE(NETWORK_PROCESS)
#include "DataReference.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoader.h>

using namespace WebCore;

#if USE(QUICK_LOOK)

@interface WKWebResourceQuickLookDelegate : NSObject <NSURLConnectionDelegate> {
    RefPtr<WebKit::WebResourceLoader> _webResourceLoader;
}
@end

@implementation WKWebResourceQuickLookDelegate

- (id)initWithWebResourceLoader:(PassRefPtr<WebKit::WebResourceLoader>)loader
{
    self = [super init];
    if (!self)
        return nil;

    _webResourceLoader = loader;
    return self;
}

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
- (void)connection:(NSURLConnection *)connection didReceiveDataArray:(NSArray *)dataArray
{
    UNUSED_PARAM(connection);
    if (!_webResourceLoader)
        return;
    _webResourceLoader->resourceLoader()->didReceiveDataArray(reinterpret_cast<CFArrayRef>(dataArray));
}
#endif

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    UNUSED_PARAM(connection);
    if (!_webResourceLoader)
        return;

    // QuickLook code sends us a nil data at times. The check below is the same as the one in
    // ResourceHandleMac.cpp added for a different bug.
    if (![data length])
        return;
    _webResourceLoader->resourceLoader()->didReceiveData(reinterpret_cast<const char*>([data bytes]), [data length], lengthReceived, DataPayloadBytes);
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    UNUSED_PARAM(connection);
    if (!_webResourceLoader)
        return;

    _webResourceLoader->resourceLoader()->didFinishLoading(0);
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    UNUSED_PARAM(connection);

    _webResourceLoader->resourceLoader()->didFail(ResourceError(error));
}

- (void)clearHandle
{
    _webResourceLoader = nullptr;
}

@end

namespace WebKit {

void WebResourceLoader::setUpQuickLookHandleIfNeeded(const ResourceResponse& response)
{
    RetainPtr<WKWebResourceQuickLookDelegate> delegate = adoptNS([[WKWebResourceQuickLookDelegate alloc] initWithWebResourceLoader:this]);
    m_quickLookHandle = QuickLookHandle::create(resourceLoader(), response.nsURLResponse(), delegate.get());
}

} // namespace WebKit

#endif

#endif // ENABLE(NETWORK_PROCESS)
