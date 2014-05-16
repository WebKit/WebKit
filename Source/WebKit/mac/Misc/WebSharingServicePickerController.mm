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

#if ENABLE(SERVICE_CONTROLS)

#import "WebContextMenuClient.h"
#import "WebViewInternal.h"
#import <AppKit/NSSharingService.h>
#import <WebCore/BitmapImage.h>
#import <WebCore/Document.h>
#import <WebCore/Editor.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameSelection.h>
#import <WebCore/ContextMenuController.h>
#import <WebCore/Page.h>

#if __has_include(<AppKit/NSSharingService_Private.h>)
#import <AppKit/NSSharingService_Private.h>
#else
typedef enum {
    NSSharingServicePickerStyleRollover = 1
} NSSharingServicePickerStyle;

@interface NSSharingServicePicker (Private)
@property NSSharingServicePickerStyle style;
- (NSMenu *)menu;
@end
#endif

#if __has_include(<AppKit/NSItemProvider.h>)
#import <AppKit/NSItemProvider.h>
#else
// FIXME: We should properly disable code that interacts with NSItemProvider/NSSharingServicePicker in 32-bit.
@interface NSItemProvider : NSObject
@property (copy, readonly) NSArray *registeredTypeIdentifiers;
- (instancetype)initWithItem:(id <NSSecureCoding>)item typeIdentifier:(NSString *)typeIdentifier;
- (void)loadItemForTypeIdentifier:(NSString *)typeIdentifier options:(NSDictionary *)options completionHandler:(void (^)(id <NSSecureCoding> item, NSError *error))completionHandler;
@end
#endif

static NSString *serviceControlsPasteboardName = @"WebKitServiceControlsPasteboard";

using namespace WebCore;

@implementation WebSharingServicePickerController

- (instancetype)initWithData:(NSData *)data includeEditorServices:(BOOL)includeEditorServices menuClient:(WebContextMenuClient*)menuClient
{
#ifndef __LP64__
    return nil;
#else
    if (!(self = [super init]))
        return nil;

    RetainPtr<NSItemProvider> itemProvider = adoptNS([[NSItemProvider alloc] initWithItem:data typeIdentifier:@"public.data"]);

    _picker = adoptNS([[NSSharingServicePicker alloc] initWithItems:@[ itemProvider.get() ]]);
    [_picker setStyle:NSSharingServicePickerStyleRollover];
    [_picker setDelegate:self];

    _includeEditorServices = includeEditorServices;
    _menuClient = menuClient;

    return self;
#endif
}

- (void)clear
{
    // Protect self from being dealloc'ed partway through this method.
    RetainPtr<WebSharingServicePickerController> protector(self);

    if (_menuClient)
        _menuClient->clearSharingServicePickerController();

    _picker = nullptr;
    _menuClient = nullptr;
}

- (NSMenu *)menu
{
    return [_picker menu];
}

- (void)didShareImageData:(NSData *)data confirmDataIsValidTIFFData:(BOOL)confirmData
{
    Page* page = [_menuClient->webView() page];
    if (!page)
        return;

    if (confirmData) {
        RetainPtr<NSImage> nsImage = adoptNS([[NSImage alloc] initWithData:data]);
        if (!nsImage) {
            LOG_ERROR("Shared image data cannot create a valid NSImage");
            return;
        }

        data = [nsImage TIFFRepresentation];
    }

    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:serviceControlsPasteboardName];
    [pasteboard declareTypes:@[ NSPasteboardTypeTIFF ] owner:nil];
    [pasteboard setData:data forType:NSPasteboardTypeTIFF];

    if (Node* node = page->contextMenuController().context().hitTestResult().innerNode()) {
        if (Frame* frame = node->document().frame())
            frame->editor().replaceNodeFromPasteboard(node, serviceControlsPasteboardName);
    }

    [self clear];
}

#pragma mark NSSharingServicePickerDelegate methods

- (NSArray *)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker sharingServicesForItems:(NSArray *)items mask:(NSSharingServiceMask)mask proposedSharingServices:(NSArray *)proposedServices
{
    if (_includeEditorServices)
        return proposedServices;
        
    NSMutableArray *services = [[NSMutableArray alloc] initWithCapacity:[proposedServices count]];
    
    for (NSSharingService *service in proposedServices) {
        if (service.type != NSSharingServiceTypeEditor)
            [services addObject:service];
    }
    
    return services;
}

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

    id item = [items objectAtIndex:0];

    if ([item isKindOfClass:[NSImage class]])
        [self didShareImageData:[item TIFFRepresentation] confirmDataIsValidTIFFData:NO];
#ifdef __LP64__
    else if ([item isKindOfClass:[NSItemProvider class]]) {
        NSItemProvider *itemProvider = (NSItemProvider *)item;
        NSString *itemUTI = itemProvider.registeredTypeIdentifiers.firstObject;
        
        [itemProvider loadItemForTypeIdentifier:itemUTI options:nil completionHandler:^(id receivedData, NSError *dataError) {
            if (!receivedData) {
                LOG_ERROR("Did not receive data from NSItemProvider");
                return;
            }

            if (![receivedData isKindOfClass:[NSData class]]) {
                LOG_ERROR("Data received from NSItemProvider is not of type NSData");
                return;
            }

            [[NSOperationQueue mainQueue] addOperationWithBlock:^{
                [self didShareImageData:receivedData confirmDataIsValidTIFFData:YES];
            }];

        }];
    }
#endif
    else
        LOG_ERROR("sharingService:didShareItems: - Unknown item type returned");
}

- (void)sharingService:(NSSharingService *)sharingService didFailToShareItems:(NSArray *)items error:(NSError *)error
{
    [self clear];
}

- (NSRect)sharingService:(NSSharingService *)sharingService sourceFrameOnScreenForShareItem:(id <NSPasteboardWriting>)item
{
    if (!_menuClient)
        return NSZeroRect;

    return _menuClient->screenRectForHitTestNode();
}

- (NSImage *)sharingService:(NSSharingService *)sharingService transitionImageForShareItem:(id <NSPasteboardWriting>)item contentRect:(NSRect *)contentRect
{
    if (!_menuClient)
        return nil;

    return _menuClient->renderedImageForControlledImage();
}

- (NSWindow *)sharingService:(NSSharingService *)sharingService sourceWindowForShareItems:(NSArray *)items sharingContentScope:(NSSharingContentScope *)sharingContentScope
{
    return [_menuClient->webView() window];
}

@end

#endif
