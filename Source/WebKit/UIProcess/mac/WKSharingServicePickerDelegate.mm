/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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
#import "WKSharingServicePickerDelegate.h"

#if ENABLE(SERVICE_CONTROLS)

#import "APIAttachment.h"
#import "DataReference.h"
#import "WKObject.h"
#import "WebContextMenuProxyMac.h"
#import "WebPageProxy.h"
#import "_WKAttachmentInternal.h"
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#import <pal/spi/mac/NSSharingServiceSPI.h>
#import <wtf/text/WTFString.h>

// FIXME: We probably need to hang on the picker itself until the context menu operation is done, and this object will probably do that.
@implementation WKSharingServicePickerDelegate

+ (WKSharingServicePickerDelegate*)sharedSharingServicePickerDelegate
{
    static WKSharingServicePickerDelegate* delegate = [[WKSharingServicePickerDelegate alloc] init];
    return delegate;
}

- (WebKit::WebContextMenuProxyMac*)menuProxy
{
    return _menuProxy;
}

- (void)setMenuProxy:(WebKit::WebContextMenuProxyMac*)menuProxy
{
    _menuProxy = menuProxy;
}

- (void)setPicker:(NSSharingServicePicker *)picker
{
    _picker = picker;
}

- (void)setFiltersEditingServices:(BOOL)filtersEditingServices
{
    _filterEditingServices = filtersEditingServices;
}

- (void)setHandlesEditingReplacement:(BOOL)handlesEditingReplacement
{
    _handleEditingReplacement = handlesEditingReplacement;
}

- (void)setSourceFrame:(NSRect)sourceFrame
{
    _sourceFrame = sourceFrame;
}

- (void)setAttachmentID:(String)attachmentID
{
    _attachmentID = attachmentID;
}

- (NSArray *)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker sharingServicesForItems:(NSArray *)items mask:(NSSharingServiceMask)mask proposedSharingServices:(NSArray *)proposedServices
{
    if (!_filterEditingServices)
        return proposedServices;

    NSMutableArray *services = [NSMutableArray arrayWithCapacity:proposedServices.count];
    
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

- (NSRect)sharingService:(NSSharingService *)sharingService sourceFrameOnScreenForShareItem:(id <NSPasteboardWriting>)item
{
    return _sourceFrame;
}

- (void)sharingService:(NSSharingService *)sharingService willShareItems:(NSArray *)items
{
    _menuProxy->clearServicesMenu();
}

- (void)sharingService:(NSSharingService *)sharingService didShareItems:(NSArray *)items
{
    // We only care about what item was shared if we were interested in editor services
    // (i.e., if we plan on replacing the selection or controlled image with the returned item)
    if (!_handleEditingReplacement)
        return;

    if (!items.count)
        return;

    Vector<String> types;
    IPC::DataReference dataReference;

    id item = [items objectAtIndex:0];

    if ([item isKindOfClass:[NSAttributedString class]]) {
        NSData *data = [item RTFDFromRange:NSMakeRange(0, [item length]) documentAttributes:@{ }];
        dataReference = IPC::DataReference(static_cast<const uint8_t*>([data bytes]), [data length]);

        types.append(NSPasteboardTypeRTFD);
        types.append(WebCore::legacyRTFDPasteboardType());
    } else if ([item isKindOfClass:[NSData class]]) {
        NSData *data = (NSData *)item;
        RetainPtr<CGImageSourceRef> source = adoptCF(CGImageSourceCreateWithData((CFDataRef)data, NULL));
        RetainPtr<CGImageRef> image = adoptCF(CGImageSourceCreateImageAtIndex(source.get(), 0, NULL));

        if (!image)
            return;

        dataReference = IPC::DataReference(static_cast<const uint8_t*>([data bytes]), [data length]);
        types.append(NSPasteboardTypeTIFF);
    } else if ([item isKindOfClass:[NSItemProvider class]]) {
        NSItemProvider *itemProvider = (NSItemProvider *)item;
        
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        WeakPtr weakPage = _menuProxy->page();
        NSString *itemUTI = itemProvider.registeredTypeIdentifiers.firstObject;
        [itemProvider loadDataRepresentationForTypeIdentifier:itemUTI completionHandler:[weakPage, attachmentID = _attachmentID, itemUTI](NSData *data, NSError *error) {
            RefPtr webPage = weakPage.get();
            
            if (!webPage)
                return;
            
            if (error)
                return;
            
            auto apiAttachment = webPage->attachmentForIdentifier(attachmentID);
            if (!apiAttachment)
                return;
            
            auto attachment = wrapper(apiAttachment);
            [attachment setData:data newContentType:itemUTI];
            webPage->didInvalidateDataForAttachment(*apiAttachment.get());
        }];
ALLOW_DEPRECATED_DECLARATIONS_END
        return;
    } else {
        LOG_ERROR("sharingService:didShareItems: - Unknown item type returned\n");
        return;
    }

    // FIXME: We should adopt replaceSelectionWithAttributedString instead of bouncing through the (fake) pasteboard.
    _menuProxy->page()->replaceSelectionWithPasteboardData(types, dataReference);
}

- (NSWindow *)sharingService:(NSSharingService *)sharingService sourceWindowForShareItems:(NSArray *)items sharingContentScope:(NSSharingContentScope *)sharingContentScope
{
    return _menuProxy->window();
}

- (void)removeBackground
{
    _menuProxy->removeBackgroundFromControlledImage();
}

@end

#endif // ENABLE(SERVICE_CONTROLS)
