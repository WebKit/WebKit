/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

DECLARE_SYSTEM_HEADER

#if PLATFORM(MAC)

#import <AppKit/AppKit.h>

#if USE(APPLE_INTERNAL_SDK)

#define CGCOLORTAGGEDPOINTER_H_

#import <AppKit/NSApplication_Private.h>
#import <AppKit/NSInspectorBar.h>
#import <AppKit/NSMenu_Private.h>
#import <AppKit/NSPreviewRepresentingActivityItem_Private.h>
#import <AppKit/NSTextInputClient_Private.h>
#import <AppKit/NSView_Layout.h>
#import <AppKit/NSWindow_Private.h>
#import <AppKit/NSScrollViewSeparatorTrackingAdapter_Private.h>

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
#import <AppKit/NSScrollPocket_Private.h>
#endif

#import <AppKit/NSGestureRecognizer_Private.h>
#import <AppKit/NSPanGestureRecognizer_Private.h>

#if HAVE(NSVIEW_CORNER_CONFIGURATION)
#import <AppKit/NSViewCornerConfiguration_Private.h>
#endif

#else

@interface NSInspectorBar : NSObject
@property (getter=isVisible) BOOL visible;
@end

@interface NSKeyboardShortcut
+ (id)shortcutWithKeyEquivalent:(NSString *)keyEquivalent modifierMask:(NSUInteger)modifierMask;
@property (readonly) NSString *localizedDisplayName;
@end

@protocol NSScrollViewSeparatorTrackingAdapter
@property (readonly) NSRect scrollViewFrame;
@property (readonly) BOOL hasScrolledContentsUnderTitlebar;
@end

@class NSTextPlaceholder;

@protocol NSTextInputClient_Async
@optional

- (void)insertTextPlaceholderWithSize:(CGSize)size completionHandler:(void (^)(NSTextPlaceholder *))completionHandler;

- (void)removeTextPlaceholder:(NSTextPlaceholder *)placeholder willInsertText:(BOOL)willInsertText completionHandler:(void (^)(void))completionHandler;
@end

typedef NS_OPTIONS(NSUInteger, NSWindowShadowOptions) {
    NSWindowShadowSecondaryWindow = 0x2,
};

static const NSWindowStyleMask NSWindowStyleMaskAlertWindow = (NSWindowStyleMask)(1ull << 33ull);

@interface NSWindow ()
- (NSInspectorBar *)inspectorBar;
- (void)setInspectorBar:(NSInspectorBar *)bar;

@property (readonly) NSWindowShadowOptions shadowOptions;
@property CGFloat titlebarAlphaValue;

- (BOOL)registerScrollViewSeparatorTrackingAdapter:(NSObject<NSScrollViewSeparatorTrackingAdapter> *)adapter;
- (void)unregisterScrollViewSeparatorTrackingAdapter:(NSObject<NSScrollViewSeparatorTrackingAdapter> *)adapter;

- (void)_setSharesParentFirstResponder:(BOOL)sharesParentFirstResponder;

@end

@class LPLinkMetadata;

@interface NSPreviewRepresentingActivityItem ()
- (instancetype)initWithItem:(id)item linkMetadata:(LPLinkMetadata *)linkMetadata;
@end

typedef NS_ENUM(NSInteger, NSScrollPocketStyle) {
    NSScrollPocketStyleAutomatic,
    NSScrollPocketStyleSoft,
    NSScrollPocketStyleHard,
};

typedef NS_ENUM(NSInteger, NSScrollPocketEdge) {
    NSScrollPocketEdgeTop    = 0,
    NSScrollPocketEdgeBottom = 1,
    NSScrollPocketEdgeLeft   = 2,
    NSScrollPocketEdgeRight  = 3,
};

@interface NSScrollPocket : NSView
- (void)addElementContainer:(NSView *)elementContainer;
- (void)removeElementContainer:(NSView *)elementContainer;
@property (nonatomic) BOOL prefersSolidColorHardPocket;
@property NSScrollPocketEdge edge;
@property NSScrollPocketStyle style;
@property (copy, nullable) NSColor *captureColor;
@property (readonly, strong) NSView *captureView;
@end

@interface NSView (NSConstraintBasedLayout)
- (void)_setHostsAutolayoutEngine:(BOOL)flag;
@end

@interface NSPanGestureRecognizer (SPI)
@property (readonly) NSTimeInterval timestamp;
@end

#if HAVE(NSVIEW_CORNER_CONFIGURATION)

@interface _NSCornerRadius : NSObject
@property (class, copy, readonly) _NSCornerRadius *containerConcentricRadius;
+ (_NSCornerRadius *)fixedRadius:(CGFloat)radius;
@end

@interface NSViewCornerRadii : NSObject
@property CGFloat topLeft;
@property CGFloat topRight;
@property CGFloat bottomLeft;
@property CGFloat bottomRight;
@property (copy) CALayerCornerCurve cornerCurve;
@end

@interface NSViewCornerConfiguration : NSObject
+ (NSViewCornerConfiguration *)configurationWithRadius:(_NSCornerRadius *)radius;
+ (instancetype)configurationWithTopLeftRadius:(nullable _NSCornerRadius *)topLeftRadius topRightRadius:(nullable _NSCornerRadius *)topRightRadius bottomLeftRadius:(nullable _NSCornerRadius *)bottomLeftRadius bottomRightRadius:(nullable _NSCornerRadius *)bottomRightRadius;
@end

@interface NSView (NSViewCornerConfiguration)
@property (nullable, readonly) NSViewCornerRadii *_effectiveCornerRadii;
@property (readonly, nullable, copy) NSViewCornerConfiguration *_cornerConfiguration;
- (void)_viewDidChangeEffectiveCornerRadii;
- (void)_invalidateCornerConfiguration;
@end

#endif

#if !HAVE(NSGESTURERECOGNIZER_MODIFIER_FLAGS)
@interface NSGestureRecognizer (SPI)
- (NSEventModifierFlags)modifierFlags;
@end
#endif

#endif

@interface NSPopover (IPI)
@property (readonly) NSView *positioningView;
@end

@interface NSWorkspace (NSWorkspaceAccessibilityDisplayInternal_IPI)
+ (void)_invalidateAccessibilityDisplayValues;
@end

@interface NSInspectorBar (IPI)
- (void)_update;
@end

// FIXME: Move this above once <rdar://problem/70224980> is in an SDK.
@interface NSCursor ()
+ (void)hideUntilChanged;
@end

#if HAVE(NSWINDOW_SNAPSHOT_READINESS_HANDLER)
// FIXME: Move this above once <rdar://problem/112554759> is in an SDK.
@interface NSWindow (Staging_112554759)
typedef void (^NSWindowSnapshotReadinessHandler) (void);
- (NSWindowSnapshotReadinessHandler)_holdResizeSnapshotWithReason:(NSString *)reason;
@end
#endif

#endif // PLATFORM(MAC)
