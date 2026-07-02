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
#import <wtf/Forward.h>
#import <wtf/ObjectIdentifier.h>
#import <wtf/Vector.h>

namespace WebCore {
class Color;
class FloatQuad;
class IntPoint;
class IntSize;
}

namespace WebKit {
class WebPageProxy;
class WebViewImpl;
struct InteractionInformationAtPosition;
}

OBJC_CLASS NSPanGestureRecognizer;
OBJC_CLASS WKWebView;

#if __has_include(<WebKitAdditions/WKAppKitGestureControllerAdditionsBefore.mm>) && !__has_feature(modules)
#import <WebKitAdditions/WKAppKitGestureControllerAdditionsBefore.mm>
#endif

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

NS_SWIFT_UI_ACTOR
@interface WKAppKitGestureController : NSObject <NSGestureRecognizerDelegate>

- (instancetype)initWithPage:(std::reference_wrapper<WebKit::WebPageProxy>)page viewImpl:(std::reference_wrapper<WebKit::WebViewImpl>)viewImpl;
- (void)enableGesturesIfNeeded;
- (void)beginSuppressingSingleClickGestureForTextSelection;
- (void)endSuppressingSingleClickGestureForTextSelection;
- (NSGestureRecognizer *)activeDragGestureRecognizer;
- (void)setGestureDraggingSession:(NSDraggingSession *)session;
- (void)clearGestureDragState;
- (void)setTextSelectionDragGesture:(NSGestureRecognizer *)gesture completionHandler:(void (^)(NSDraggingSession *))completionHandler;
- (void)positionInformationDidChange:(const WebKit::InteractionInformationAtPosition&)info;
- (void)didCommitLoadForMainFrame;
- (void)didEndSyntheticMomentumScrolling;
- (void)reset;

#if ENABLE(TWO_PHASE_CLICKS)

@property (nonatomic, readonly, getter=isPotentialClickInProgress) BOOL potentialClickInProgress;

#if !__has_feature(modules)
- (void)didGetClickHighlightForRequest:(WebKit::ClickIdentifier)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius nodeHasBuiltInClickHandling:(BOOL)nodeHasBuiltInClickHandling;
- (void)disableDoubleClickGesturesDuringClickIfNecessary:(WebKit::ClickIdentifier)requestID;
- (void)commitPotentialClickFailed;
- (void)didCompleteSyntheticClick;
- (void)didHandleClickAsHover;
- (void)didNotHandleClickAsClick:(const WebCore::IntPoint&)point;
#endif // !__has_feature(modules)

#endif // ENABLE(TWO_PHASE_CLICKS)

// Exposed for Swift
@property (nonatomic, readonly, nullable) WKWebView *webView;
@property (nonatomic, strong, nullable) NSPanGestureRecognizer *panGestureRecognizer;
- (void)configureForScrolling:(NSPanGestureRecognizer *)gesture;
- (void)panGestureRecognized:(NSGestureRecognizer *)gesture;

@end

@interface WKAppKitGestureController (Swift)

- (void)setUpPanGestureRecognizer;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // HAVE(APPKIT_GESTURES_SUPPORT)
