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
#import <pal/mac/QuickLookUISoftLink.h>
#import <wtf/RetainPtr.h>

@implementation WKImageExtractionPreviewController {
    RetainPtr<QLItem> _item;
    RetainPtr<NSData> _imageData;
}

- (instancetype)initWithPage:(WebKit::WebPageProxy&)page imageData:(NSData *)imageData title:(NSString *)title imageURL:(NSURL *)imageURL
{
    if (!(self = [super init]))
        return nil;

    _imageData = imageData;
    _item = adoptNS([PAL::allocQLItemInstance() initWithDataProvider:(id)self contentType:UTTypePNG previewTitle:title]);
    if ([_item respondsToSelector:@selector(setPreviewOptions:)]) {
        auto previewOptions = adoptNS([[NSMutableDictionary alloc] initWithCapacity:2]);
        if (imageURL)
            [previewOptions setObject:imageURL forKey:@"imageURL"];
        if (NSURL *pageURL = URL { URL { }, page.currentURL() })
            [previewOptions setObject:pageURL forKey:@"pageURL"];
        [_item setPreviewOptions:previewOptions.get()];
    }

    return self;
}

#pragma mark - QLPreviewItemDataProvider

- (NSData *)provideDataForItem:(QLItem *)item
{
    ASSERT(item == _item);
    return _imageData.get();
}

#pragma mark - QLPreviewPanelDataSource

- (NSInteger)numberOfPreviewItemsInPreviewPanel:(QLPreviewPanel *)panel
{
    return 1;
}

- (id <QLPreviewItem>)previewPanel:(QLPreviewPanel *)panel previewItemAtIndex:(NSInteger)index
{
    ASSERT(!index);
    return _item.get();
}

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WKImageExtractionPreviewControllerAdditions.mm>
#endif

@end

#endif // ENABLE(IMAGE_EXTRACTION) && PLATFORM(MAC)
