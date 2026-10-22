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

#if ENABLE(TWO_PHASE_CLICKS)

#import "IdentifierTypes.h"
#import <WebCore/Color.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FloatRect.h>
#import <WebCore/IntSize.h>
#import <wtf/Vector.h>

@class WKClickHighlightController;
@class WKClickHighlightView;

@protocol WKClickHighlightControllerDelegate <NSObject>
- (void)clickHighlightController:(WKClickHighlightController *)controller installHighlightView:(WKClickHighlightView *)highlightView;
- (CGFloat)clickHighlightControllerContentScale:(WKClickHighlightController *)controller;
- (CGFloat)clickHighlightControllerCornerRadiusScale:(WKClickHighlightController *)controller;
- (WebCore::FloatRect)clickHighlightControllerUnobscuredRect:(WKClickHighlightController *)controller;
- (WebCore::FloatRect)clickHighlightControllerBoundaryRect:(WKClickHighlightController *)controller;
- (BOOL)clickHighlightControllerIsPotentialClickInProgress:(WKClickHighlightController *)controller;
@end

@interface WKClickHighlightController : NSObject

- (instancetype)initWithDelegate:(id<WKClickHighlightControllerDelegate>)delegate NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)beginRequest:(WebKit::TapIdentifier)requestID expectsFastCommit:(BOOL)expectsFastCommit;

- (void)didReceiveHighlightForRequest:(WebKit::TapIdentifier)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)quads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius nodeHasBuiltInClickHandling:(BOOL)nodeHasBuiltInClickHandling;

- (void)showPendingHighlightForCommittedClick;
- (void)finishUnlessExpectingFastCommit;
- (void)fadeUnlessAwaitingHighlight;

- (void)finish;
- (void)cancel;
- (void)cleanUp;

- (void)updateHighlight;
- (void)setHighlightColor:(const WebCore::Color&)color;

@property (nonatomic) BOOL usesDebugAppearance;
@property (nonatomic, readonly) CGRect highlightViewFrame;

@end

#endif // ENABLE(TWO_PHASE_CLICKS)
