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
#import "_WKDownloadInternal.h"

#import "APIDownloadClient.h"
#import "DownloadProxy.h"
#import "WKDownloadInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKNSData.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/VectorCocoa.h>


ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static NSMapTable<WKDownload *, _WKDownload *> *downloadWrapperMapSingleton()
{
    static NeverDestroyed<RetainPtr<NSMapTable>> table;
    if (!table.get())
        table.get() = [NSMapTable weakToWeakObjectsMapTable];
    return table.get().get();
}
ALLOW_DEPRECATED_DECLARATIONS_END

// FIXME: Remove when rdar://133558571, rdar://133558520, rdar://133498655, rdar://133498564, rdar://133498491, rdar://133495572, and rdar://125569813 are complete.

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
@implementation _WKDownload
IGNORE_WARNINGS_END

- (instancetype)initWithDownload2:(WKDownload *)download
{
    if (!(self = [super init]))
        return nil;
    lazyInitialize(_download, retainPtr(download));
    return self;
}

+ (instancetype)downloadWithDownload:(WKDownload *)download
{
    if (RetainPtr<_WKDownload> wrapper = [downloadWrapperMapSingleton() objectForKey:download])
        return wrapper.autorelease();
    auto wrapper = adoptNS([[_WKDownload alloc] initWithDownload2:download]);
    [downloadWrapperMapSingleton() setObject:wrapper.get() forKey:download];
    return wrapper.autorelease();
}

- (void)cancel
{
    Ref { *_download->_download }->cancel([download = Ref { *_download->_download }] (auto*) {
        protect(download->client())->legacyDidCancel(download.get());
    });
}

- (void)publishProgressAtURL:(NSURL *)URL
{
    Ref { *_download->_download }->publishProgress(URL);
}

- (NSURLRequest *)request
{
    return _download->_download->request().protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody).autorelease();
}

- (WKWebView *)originatingWebView
{
    RefPtr page = Ref { *_download->_download }->originatingPage();
    return page ? page->cocoaView().autorelease() : nil;
}

-(NSArray<NSURL *> *)redirectChain
{
    return createNSArray(_download->_download->redirectChain(), [] (auto& url) {
        return url.createNSURL();
    }).autorelease();
}

- (BOOL)wasUserInitiated
{
    return _download->_download->wasUserInitiated();
}

- (NSData *)resumeData
{
    return WebKit::wrapper(_download->_download->legacyResumeData());
}

- (WKFrameInfo *)originatingFrame
{
    return WebKit::wrapper(&_download->_download->frameInfo());
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_download->_download;
}

@end
