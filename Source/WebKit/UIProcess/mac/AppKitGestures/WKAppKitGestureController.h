/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import <wtf/Platform.h>

#if HAVE(APPKIT_GESTURES_SUPPORT)

#import <AppKit/NSGestureRecognizer.h>

OBJC_CLASS WKClickHighlightController;
OBJC_CLASS WKWebView;

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

NS_SWIFT_UI_ACTOR
@interface WKAppKitGestureController : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)initWithView:(WKWebView *)view NS_DESIGNATED_INITIALIZER;
- (void)setUp;
- (void)preferencesDidChange;
- (void)didCommitLoadForMainFrame;
- (void)reset;

@property (nonatomic, readonly, nullable) WKWebView *webView;

@end

@interface WKAppKitGestureController (Clicks)
- (void)beginSuppressingSingleClickGestureForTextSelection;
- (void)endSuppressingSingleClickGestureForTextSelection;
- (void)cancelClickHighlight;

@property (nonatomic, readonly, getter=isPotentialTapInProgress) BOOL potentialTapInProgress;
@property (nonatomic, readonly, nullable) WKClickHighlightController *clickHighlightController;
- (void)commitPotentialTapFailed;
- (void)didCompleteSyntheticClick;
- (void)didHandleTapAsHover;
- (void)disableDoubleTapGesturesDuringTapIfNecessaryForRequest:(uint64_t)requestID;
- (void)didNotHandleTapAsClickAtPoint:(NSPoint)point;
@end

@interface WKAppKitGestureController (Drag)
@property (nonatomic, readonly, nullable) NSGestureRecognizer *activeDragGestureRecognizer;
- (void)setTextSelectionDragGesture:(NSGestureRecognizer *)gesture completionHandler:(void (^)(NSDraggingSession *))completionHandler;
- (void)didReceiveDragStart;
- (void)setGestureDraggingSession:(nullable NSDraggingSession *)session;
- (void)clearGestureDragState;
@end

@interface WKAppKitGestureController (Scrolling)
- (void)interruptMomentumForScrollWheelEvent;
@property (nonatomic, readonly) BOOL everMagnifiedDuringCurrentGesture;
@end

@interface WKAppKitGestureController (TransformGestures)
- (void)transformGestureWasNotHandledByContent;
@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // HAVE(APPKIT_GESTURES_SUPPORT)
