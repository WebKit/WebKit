/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "WKImageExtractionPreviewController.h"

#if ENABLE(IMAGE_EXTRACTION) && PLATFORM(MAC)

#import "WebPageProxy.h"
#import <wtf/FileSystem.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakPtr.h>

@implementation WKImageExtractionPreviewItem {
    RetainPtr<NSURL> _url;
    RetainPtr<NSString> _title;
}

- (instancetype)initWithURL:(NSURL *)url title:(NSString *)title
{
    if (!(self = [super init]))
        return nil;

    _url = url;
    _title = adoptNS([title copy]);

    return self;
}

- (void)dealloc
{
    FileSystem::deleteFile([_url path]);

    [super dealloc];
}

#pragma mark - QLPreviewItem

- (NSURL *)previewItemURL
{
    return _url.get();
}

- (NSString *)previewItemTitle
{
    return _title.get();
}

@end

@implementation WKImageExtractionPreviewController {
    WeakPtr<WebKit::WebPageProxy> _page;
    RetainPtr<WKImageExtractionPreviewItem> _previewItem;
}

- (instancetype)initWithPage:(WebKit::WebPageProxy&)page url:(NSURL *)url title:(NSString *)title
{
    if (!(self = [super init]))
        return nil;

    _page = makeWeakPtr(page);
    _previewItem = adoptNS([[WKImageExtractionPreviewItem alloc] initWithURL:url title:title]);

    return self;
}

#pragma mark - QLPreviewPanelDataSource

- (NSInteger)numberOfPreviewItemsInPreviewPanel:(QLPreviewPanel *)panel
{
    return 1;
}

- (id <QLPreviewItem>)previewPanel:(QLPreviewPanel *)panel previewItemAtIndex:(NSInteger)index
{
    ASSERT(!index);
    return _previewItem.get();
}

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WKImageExtractionPreviewControllerAdditions.mm>
#endif

@end

#endif // ENABLE(IMAGE_EXTRACTION) && PLATFORM(MAC)
