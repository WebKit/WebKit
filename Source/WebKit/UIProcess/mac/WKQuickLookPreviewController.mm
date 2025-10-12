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
#import "WKQuickLookPreviewController.h"

#if HAVE(QUICKLOOK_PREVIEW_ITEM_DATA_PROVIDER)

#import "WebPageProxy.h"
#import <wtf/RetainPtr.h>
#import <wtf/URL.h>
#import <pal/mac/QuickLookUISoftLink.h>

@interface WKQuickLookPreviewController () <QLPreviewPanelDelegate, QLPreviewPanelDataSource>
@end

@implementation WKQuickLookPreviewController {
    RetainPtr<QLItem> _item;
    RetainPtr<NSData> _imageData;
    WebKit::QuickLookPreviewActivity _activity;
}

- (instancetype)initWithPage:(WebKit::WebPageProxy&)page imageData:(NSData *)imageData title:(NSString *)title imageURL:(NSURL *)imageURL activity:(WebKit::QuickLookPreviewActivity)activity
{
    if (!(self = [super init]))
        return nil;

    _activity = activity;
    _imageData = imageData;
    _item = adoptNS([PAL::allocQLItemInstance() initWithDataProvider:(id)self contentType:UTTypePNG previewTitle:title]);
    if ([_item respondsToSelector:@selector(setPreviewOptions:)]) {
        auto previewOptions = adoptNS([[NSMutableDictionary alloc] initWithCapacity:2]);
        if (imageURL)
            [previewOptions setObject:imageURL forKey:@"imageURL"];
        if (RetainPtr pageURL = URL { page.currentURL() }.createNSURL())
            [previewOptions setObject:pageURL.get() forKey:@"pageURL"];
        [_item setPreviewOptions:previewOptions.get()];
    }

    return self;
}

- (void)beginControl:(QLPreviewPanel *)panel
{
    panel.dataSource = self;
    panel.delegate = self;
}

- (void)endControl:(QLPreviewPanel *)panel
{
    if (panel.dataSource == self)
        panel.dataSource = nil;

    if (panel.delegate == self)
        panel.delegate = nil;
}

- (void)closePanelIfNecessary
{
    if (!PAL::isQuickLookUIFrameworkAvailable() || ![PAL::getQLPreviewPanelClassSingleton() sharedPreviewPanelExists])
        return;

    if (RetainPtr panel = [PAL::getQLPreviewPanelClassSingleton() sharedPreviewPanel]; [self isControlling:panel.get()])
        [panel close];
}

- (BOOL)isControlling:(QLPreviewPanel *)panel
{
    return panel.dataSource == self && panel.delegate == self;
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

#pragma mark - QLPreviewPanelDelegate

- (QLPreviewActivity)previewPanel:(QLPreviewPanel *)previewPanel initialActivityForItem:(id <QLPreviewItem>)item
{
    return _activity == WebKit::QuickLookPreviewActivity::VisualSearch ? QLPreviewActivityVisualSearch : QLPreviewActivityNone;
}

@end

#endif // HAVE(QUICKLOOK_PREVIEW_ITEM_DATA_PROVIDER)
