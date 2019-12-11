/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#import <wtf/Platform.h>

#if PLATFORM(MAC)

#if USE(APPLE_INTERNAL_SDK) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500

#import <AppKit/NSMenu_Private.h>

#elif USE(APPLE_INTERNAL_SDK)

WTF_EXTERN_C_BEGIN
#import <AppKit/NSMenu_Private.h>
WTF_EXTERN_C_END

#else

typedef NS_ENUM(NSInteger, NSMenuType) {
    NSMenuTypeNone = 0,
    NSMenuTypeContextMenu,
};

enum {
    NSPopUpMenuTypeGeneric,
    NSPopUpMenuTypePopUp,
    NSPopUpMenuTypePullsDown,
    NSPopUpMenuTypeMainMenu,
    NSPopUpMenuTypeContext,
    NSPopUpMenuDefaultToPopUpControlFont = 0x10,
    NSPopUpMenuPositionRelativeToRightEdge = 0x20,
    NSPopUpMenuIsPopupButton = 0x40,
};

@interface NSMenu ()
+ (NSMenuType)menuTypeForEvent:(NSEvent *)event;
@end

@class QLPreviewMenuItem;

@interface NSMenuItem ()
+ (QLPreviewMenuItem *)standardQuickLookMenuItem;
+ (NSMenuItem *)standardShareMenuItemForItems:(NSArray *)items;
@end

#endif

typedef NSUInteger NSPopUpMenuFlags;

WTF_EXTERN_C_BEGIN

extern NSString * const NSPopUpMenuPopupButtonBounds;
extern NSString * const NSPopUpMenuPopupButtonLabelOffset;
extern NSString * const NSPopUpMenuPopupButtonSize;
extern NSString * const NSPopUpMenuPopupButtonWidget;

void _NSPopUpCarbonMenu3(NSMenu *, NSWindow *, NSView *ownerView, NSPoint screenPoint, NSInteger checkedItem, NSFont *, CGFloat minWidth, NSString *runLoopMode, NSPopUpMenuFlags, NSDictionary *options);

WTF_EXTERN_C_END

#endif // PLATFORM(MAC)
