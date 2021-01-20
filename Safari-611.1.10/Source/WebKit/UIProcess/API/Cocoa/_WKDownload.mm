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
#import <wtf/WeakObjCPtr.h>

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static NSMapTable<WKDownload *, _WKDownload *> *downloadWrapperMap()
{
    static NeverDestroyed<RetainPtr<NSMapTable>> table;
    if (!table.get())
        table.get() = [NSMapTable weakToWeakObjectsMapTable];
    return table.get().get();
}
ALLOW_DEPRECATED_DECLARATIONS_END

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
@implementation _WKDownload
IGNORE_WARNINGS_END

- (instancetype)initWithDownload2:(WKDownload *)download
{
    if (!(self = [super init]))
        return nil;
    _download = download;
    return self;
}

+ (instancetype)downloadWithDownload:(WKDownload *)download
{
    if (_WKDownload *wrapper = [downloadWrapperMap() objectForKey:download])
        return wrapper;
    _WKDownload *wrapper = [[[_WKDownload alloc] initWithDownload2:download] autorelease];
    [downloadWrapperMap() setObject:wrapper forKey:download];
    return wrapper;
}

- (void)cancel
{
    _download->_download->cancel([download = makeRef(*_download->_download)] (auto*) {
        download->client().legacyDidCancel(download.get());
    });
}

- (void)publishProgressAtURL:(NSURL *)URL
{
    _download->_download->publishProgress(URL);
}

- (NSURLRequest *)request
{
    return _download->_download->request().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
}

- (WKWebView *)originatingWebView
{
    if (auto* originatingPage = _download->_download->originatingPage())
        return [[fromWebPageProxy(*originatingPage) retain] autorelease];
    return nil;
}

-(NSArray<NSURL *> *)redirectChain
{
    return createNSArray(_download->_download->redirectChain(), [] (auto& url) -> NSURL * {
        return url;
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
