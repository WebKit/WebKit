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
#import "WKWebViewContentProviderRegistry.h"

#if WK_API_ENABLED

#if PLATFORM(IOS_FAMILY)

#import "WKLegacyPDFView.h"
#import "WKPDFView.h"
#import "WKSystemPreviewView.h"
#import "WKWebViewConfigurationPrivate.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <WebCore/MIMETypeRegistry.h>
#import <wtf/HashCountedSet.h>
#import <wtf/HashMap.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

@implementation WKWebViewContentProviderRegistry {
    HashMap<String, Class <WKWebViewContentProvider>, ASCIICaseInsensitiveHash> _contentProviderForMIMEType;
    HashCountedSet<WebKit::WebPageProxy*> _pages;
}

- (instancetype)initWithConfiguration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

#if ENABLE(WKPDFVIEW)
    for (auto& mimeType : WebCore::MIMETypeRegistry::getPDFMIMETypes())
        [self registerProvider:[WKPDFView class] forMIMEType:mimeType];
#elif ENABLE(WKLEGACYPDFVIEW)
    for (auto& mimeType : WebCore::MIMETypeRegistry::getPDFMIMETypes())
        [self registerProvider:[WKLegacyPDFView class] forMIMEType:mimeType];
#endif

#if USE(SYSTEM_PREVIEW)
    if (configuration._systemPreviewEnabled) {
        for (auto& mimeType : WebCore::MIMETypeRegistry::getSystemPreviewMIMETypes())
            [self registerProvider:[WKSystemPreviewView class] forMIMEType:mimeType];
    }
#endif

    return self;
}

- (void)addPage:(WebKit::WebPageProxy&)page
{
    ASSERT(!_pages.contains(&page));
    _pages.add(&page);
}

- (void)removePage:(WebKit::WebPageProxy&)page
{
    ASSERT(_pages.contains(&page));
    _pages.remove(&page);
}

- (void)registerProvider:(Class <WKWebViewContentProvider>)contentProvider forMIMEType:(const String&)mimeType
{
    _contentProviderForMIMEType.set(mimeType, contentProvider);

    for (auto& page : _pages)
        page.key->addMIMETypeWithCustomContentProvider(mimeType);
}

- (Class <WKWebViewContentProvider>)providerForMIMEType:(const String&)mimeType
{
    const auto& representation = _contentProviderForMIMEType.find(mimeType);

    if (representation == _contentProviderForMIMEType.end())
        return nil;

    return representation->value;
}

- (Vector<String>)_mimeTypesWithCustomContentProviders
{
    return copyToVector(_contentProviderForMIMEType.keys());
}

@end

#endif // PLATFORM(IOS_FAMILY)

#endif // WK_API_ENABLED
