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

#import <AppKit/AppKit.h>

#if USE(APPLE_INTERNAL_SDK)

#define CGCOLORTAGGEDPOINTER_H_

#import <AppKit/NSInspectorBar.h>
#import <AppKit/NSTextInputClient_Private.h>
#import <AppKit/NSWindow_Private.h>

#if HAVE(NSSCROLLVIEW_SEPARATOR_TRACKING_ADAPTER)
#import <AppKit/NSScrollViewSeparatorTrackingAdapter_Private.h>
#endif

#else

@interface NSInspectorBar : NSObject
@property (getter=isVisible) BOOL visible;
@end

#if HAVE(NSSCROLLVIEW_SEPARATOR_TRACKING_ADAPTER)
@protocol NSScrollViewSeparatorTrackingAdapter
@property (readonly) NSRect scrollViewFrame;
@property (readonly) BOOL hasScrolledContentsUnderTitlebar;
@end
#endif

@protocol NSTextInputClient_Async
@end

typedef NS_OPTIONS(NSUInteger, NSWindowShadowOptions) {
    NSWindowShadowSecondaryWindow = 0x2,
};

@interface NSWindow ()
- (NSInspectorBar *)inspectorBar;
- (void)setInspectorBar:(NSInspectorBar *)bar;

@property (readonly) NSWindowShadowOptions shadowOptions;
@property CGFloat titlebarAlphaValue;

#if HAVE(NSSCROLLVIEW_SEPARATOR_TRACKING_ADAPTER)
- (BOOL)registerScrollViewSeparatorTrackingAdapter:(NSObject<NSScrollViewSeparatorTrackingAdapter> *)adapter;
- (void)unregisterScrollViewSeparatorTrackingAdapter:(NSObject<NSScrollViewSeparatorTrackingAdapter> *)adapter;
#endif

@end

#endif

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
