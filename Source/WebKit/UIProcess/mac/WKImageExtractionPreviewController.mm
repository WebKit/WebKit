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
    RetainPtr<NSURL> _fileURL;
    RetainPtr<NSString> _title;
    RetainPtr<NSURL> _imageURL;
    RetainPtr<NSURL> _pageURL;
}

- (instancetype)initWithFileURL:(NSURL *)fileURL title:(NSString *)title imageURL:(NSURL *)imageURL pageURL:(NSURL *)pageURL
{
    if (!(self = [super init]))
        return nil;

    _fileURL = fileURL;
    _title = adoptNS([title copy]);
    _imageURL = imageURL;
    _pageURL = pageURL;

    return self;
}

- (void)dealloc
{
    FileSystem::deleteFile([_fileURL path]);

    [super dealloc];
}

#pragma mark - QLPreviewItem

- (NSURL *)previewItemURL
{
    return _fileURL.get();
}

- (NSString *)previewItemTitle
{
    return _title.get();
}

- (NSDictionary *)previewOptions
{
    if (!_imageURL && !_pageURL)
        return nil;

    auto previewOptions = adoptNS([[NSMutableDictionary alloc] initWithCapacity:2]);
    if (_imageURL)
        [previewOptions setObject:_imageURL.get() forKey:@"imageURL"];
    if (_pageURL)
        [previewOptions setObject:_pageURL.get() forKey:@"pageURL"];
    return previewOptions.autorelease();
}

@end

@implementation WKImageExtractionPreviewController {
    WeakPtr<WebKit::WebPageProxy> _page;
    RetainPtr<WKImageExtractionPreviewItem> _previewItem;
}

- (instancetype)initWithPage:(WebKit::WebPageProxy&)page fileURL:(NSURL *)fileURL title:(NSString *)title imageURL:(NSURL *)imageURL
{
    if (!(self = [super init]))
        return nil;

    _page = makeWeakPtr(page);

    // FIXME: We should turn `_previewItem` into a QLItem once the fix for rdar://74299451 is available.
    _previewItem = adoptNS([[WKImageExtractionPreviewItem alloc] initWithFileURL:fileURL title:title imageURL:imageURL pageURL:URL { URL { }, page.currentURL() }]);

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
