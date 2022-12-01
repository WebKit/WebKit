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

#if PLATFORM(IOS_FAMILY)

#import "WKPDFView.h"
#import "WKUSDPreviewView.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <WebCore/MIMETypeRegistry.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <wtf/FixedVector.h>
#import <wtf/HashCountedSet.h>
#import <wtf/HashMap.h>
#import <wtf/text/StringHash.h>

@implementation WKWebViewContentProviderRegistry {
    HashMap<String, Class <WKWebViewContentProvider>, ASCIICaseInsensitiveHash> _contentProviderForMIMEType;
    HashCountedSet<WebKit::WebPageProxy*> _pages;
}

- (instancetype)initWithConfiguration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

#if ENABLE(WKPDFVIEW)
    for (auto& type : WebCore::MIMETypeRegistry::pdfMIMETypes())
        [self registerProvider:[WKPDFView class] forMIMEType:@(type.characters())];
#endif

#if USE(SYSTEM_PREVIEW)
    if (configuration._systemPreviewEnabled && !configuration.preferences._modelDocumentEnabled) {
        for (auto& type : WebCore::MIMETypeRegistry::usdMIMETypes())
            [self registerProvider:[WKUSDPreviewView class] forMIMEType:@(type.characters())];
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
    if (mimeType.isEmpty())
        return nil;

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
