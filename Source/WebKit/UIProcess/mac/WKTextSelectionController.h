/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import <AppKit/AppKit.h>

@class WKWebView;

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

NS_SWIFT_UI_ACTOR
@interface WKTextSelectionController : NSObject

- (instancetype)initWithView:(WKWebView *)view;

- (void)addTextSelectionManager;

- (void)selectionDidChange;

@end

@interface WKTextSelectionController (NSTextSelectionManagerDelegate)

@property (nonatomic, readonly) NSRect insertionCursorRect;
@property (nonatomic, readonly) BOOL selectionIsInsertionPoint;

- (BOOL)isTextSelectedAtPoint:(NSPoint)point;
- (void)showContextMenuAtPoint:(NSPoint)point;

- (void)dragSelectionWithGesture:(NSGestureRecognizer *)gesture completionHandler:(void(^)(NSDraggingSession *))completionHandler;

- (void)beginRangeSelectionAtPoint:(NSPoint)point withGranularity:(NSTextSelectionGranularity)granularity;
- (void)continueRangeSelectionAtPoint:(NSPoint)point;
- (void)endRangeSelectionAtPoint:(NSPoint)point;

- (void)moveInsertionCursorToPoint:(NSPoint)point placeAtWordBoundary:(BOOL)wordBoundary completionHandler:(void(^)(BOOL))completionHandler;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // HAVE(APPKIT_GESTURES_SUPPORT)
