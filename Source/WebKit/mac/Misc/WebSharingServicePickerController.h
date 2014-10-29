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

#if ENABLE(SERVICE_CONTROLS)

#import <wtf/RetainPtr.h>

@class NSSharingServicePicker;
@class WebSharingServicePickerController;
@protocol NSSharingServiceDelegate;
@protocol NSSharingServicePickerDelegate;

#if __has_include(<AppKit/NSSharingService_Private.h>)
#import <AppKit/NSSharingService_Private.h>
#else
typedef NS_ENUM(NSInteger, NSSharingServicePickerStyle) {
    NSSharingServicePickerStyleMenu = 0,
    NSSharingServicePickerStyleRollover = 1,
    NSSharingServicePickerStyleTextSelection = 2,
    NSSharingServicePickerStyleDataDetector = 3
} NS_ENUM_AVAILABLE_MAC(10_10);

typedef NS_ENUM(NSInteger, NSSharingServiceType) {
    NSSharingServiceTypeShare = 0,
    NSSharingServiceTypeViewer = 1,
    NSSharingServiceTypeEditor = 2
} NS_ENUM_AVAILABLE_MAC(10_10);

typedef NSUInteger NSSharingServiceMask;
#endif

namespace WebCore {
class FloatRect;
class Page;
}

class WebContextMenuClient;

class WebSharingServicePickerClient {
public:
    virtual ~WebSharingServicePickerClient() { }

    virtual void sharingServicePickerWillBeDestroyed(WebSharingServicePickerController &) = 0;
    virtual WebCore::Page* pageForSharingServicePicker(WebSharingServicePickerController &) = 0;
    virtual RetainPtr<NSWindow> windowForSharingServicePicker(WebSharingServicePickerController &) = 0;

    virtual WebCore::FloatRect screenRectForCurrentSharingServicePickerItem(WebSharingServicePickerController &) = 0;
    virtual RetainPtr<NSImage> imageForCurrentSharingServicePickerItem(WebSharingServicePickerController &) = 0;
};

@interface WebSharingServicePickerController : NSObject <NSSharingServiceDelegate, NSSharingServicePickerDelegate> {
    WebSharingServicePickerClient* _pickerClient;
    RetainPtr<NSSharingServicePicker> _picker;
    BOOL _includeEditorServices;
}

- (instancetype)initWithItems:(NSArray *)items includeEditorServices:(BOOL)includeEditorServices client:(WebSharingServicePickerClient*)pickerClient style:(NSSharingServicePickerStyle)style;
- (NSMenu *)menu;
- (void)didShareImageData:(NSData *)data confirmDataIsValidTIFFData:(BOOL)confirmData;
- (void)clear;

@end

#endif
