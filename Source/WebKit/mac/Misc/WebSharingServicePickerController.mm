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

#import "WebSharingServicePickerController.h"

#if ENABLE(IMAGE_CONTROLS)

#import "WebContextMenuClient.h"
#import "WebViewInternal.h"
#import <AppKit/NSSharingService.h>
#import <WebCore/BitmapImage.h>
#import <WebCore/ContextMenuController.h>
#import <WebCore/Page.h>

#if __has_include(<AppKit/NSSharingService_Private.h>)
#import <AppKit/NSSharingService_Private.h>
#else
typedef enum {
    NSSharingServicePickerStyleRollover = 1
} NSSharingServicePickerStyle;
#endif

using namespace WebCore;

@implementation WebSharingServicePickerController

- (instancetype)initWithImage:(NSImage *)image menuClient:(WebContextMenuClient*)menuClient
{
    if (!(self = [super init]))
        return nil;

    _picker = adoptNS([[NSSharingServicePicker alloc] initWithItems:@[ image ]]);
    [_picker setStyle:NSSharingServicePickerStyleRollover];
    [_picker setDelegate:self];

    _menuClient = menuClient;

    return self;
}

- (void)clear
{
    _menuClient->clearSharingServicePickerController();

    _picker = nullptr;
    _menuClient = nullptr;
}

- (NSMenu *)menu
{
    return [_picker menu];
}

#pragma mark NSSharingServicePickerDelegate methods

- (id <NSSharingServiceDelegate>)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker delegateForSharingService:(NSSharingService *)sharingService
{
    return self;
}

- (void)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker didChooseSharingService:(NSSharingService *)service
{
    if (!service)
        _menuClient->clearSharingServicePickerController();
}

#pragma mark NSSharingServiceDelegate methods

- (void)sharingService:(NSSharingService *)sharingService didShareItems:(NSArray *)items
{
    // We only send one item, so we should only get one item back.
    if ([items count] != 1)
        return;

    RetainPtr<CGImageSourceRef> source = adoptCF(CGImageSourceCreateWithData((CFDataRef)[items objectAtIndex:0], NULL));
    RetainPtr<CGImageRef> cgImage = adoptCF(CGImageSourceCreateImageAtIndex(source.get(), 0, NULL));
    RefPtr<Image> image = BitmapImage::create(cgImage.get());

    Page* page = [_menuClient->webView() page];
    if (!page)
        return;

    page->contextMenuController().replaceControlledImage(image.get());

    [self clear];
}

- (void)sharingService:(NSSharingService *)sharingService didFailToShareItems:(NSArray *)items error:(NSError *)error
{
    [self clear];
}

- (NSWindow *)sharingService:(NSSharingService *)sharingService sourceWindowForShareItems:(NSArray *)items sharingContentScope:(NSSharingContentScope *)sharingContentScope
{
    return [_menuClient->webView() window];
}

@end

#endif
