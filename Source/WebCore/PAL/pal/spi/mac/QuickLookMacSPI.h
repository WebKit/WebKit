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

#import <Quartz/Quartz.h>

#if USE(APPLE_INTERNAL_SDK)

#if HAVE(CPP20_INCOMPATIBLE_INTERNAL_HEADERS)
#define CGCOLORTAGGEDPOINTER_H_
#endif

#import <Quartz/QuartzPrivate.h>

#else

@protocol QLPreviewMenuItemDelegate <NSObject>
@optional

- (NSView *)menuItem:(NSMenuItem *)menuItem viewAtScreenPoint:(NSPoint)screenPoint;
- (id<QLPreviewItem>)menuItem:(NSMenuItem *)menuItem previewItemAtPoint:(NSPoint)point;
- (NSRectEdge)menuItem:(NSMenuItem *)menuItem preferredEdgeForPoint:(NSPoint)point;
- (void)menuItemDidClose:(NSMenuItem *)menuItem;
- (NSRect)menuItem:(NSMenuItem *)menuItem itemFrameForPoint:(NSPoint)point;
- (NSSize)menuItem:(NSMenuItem *)menuItem maxSizeForPoint:(NSPoint)point;

@end

@interface QLPreviewMenuItem : NSObject
@end

@interface QLPreviewMenuItem ()
typedef NS_ENUM(NSInteger, QLPreviewStyle) {
    QLPreviewStyleStandaloneWindow,
    QLPreviewStylePopover
};

- (void)close;

@property (assign) id<QLPreviewMenuItemDelegate> delegate;
@property QLPreviewStyle previewStyle;
@end

@interface QLItem : NSObject <QLPreviewItem>
@property (nonatomic, copy) NSDictionary* previewOptions;
@end

#if HAVE(QUICKLOOK_PREVIEW_ACTIVITY)

typedef NS_ENUM(NSInteger, QLPreviewActivity) {
    QLPreviewActivityNone,
    QLPreviewActivityMarkup,
    QLPreviewActivityTrim,
    QLPreviewActivityVisualSearch
};

#endif // HAVE(QUICKLOOK_PREVIEW_ACTIVITY)

#endif // USE(APPLE_INTERNAL_SDK)

#if HAVE(QUICKLOOK_PREVIEW_ITEM_DATA_PROVIDER)

@class UTType;
@interface QLItem (Staging_74299451)
- (instancetype)initWithDataProvider:(id /* <QLPreviewItemDataProvider> */)data contentType:(UTType *)contentType previewTitle:(NSString *)previewTitle;
@end

#endif // HAVE(QUICKLOOK_PREVIEW_ITEM_DATA_PROVIDER)

#if HAVE(QUICKLOOK_ITEM_PREVIEW_OPTIONS)

@interface QLItem (Staging_77864637)
@property (nonatomic, copy) NSDictionary *previewOptions;
@end

#endif // HAVE(QUICKLOOK_ITEM_PREVIEW_OPTIONS)
