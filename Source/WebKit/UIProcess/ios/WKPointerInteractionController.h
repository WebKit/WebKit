/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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

#if HAVE(UI_POINTER_INTERACTION)

#import <UIKit/UIKit.h>

@class WKContentView;

namespace WebKit {
struct InteractionInformationAtPosition;
struct InteractionInformationRequest;
}

// WKContentView is the pointer interaction's delegate and forwards region and style requests to the controller, which
// reads position information through these methods.
@protocol WKPointerInteractionControllerDelegate <UIPointerInteractionDelegate>
@property (nonatomic, readonly) const WebKit::InteractionInformationAtPosition& positionInformation;
- (BOOL)_currentPositionInformationIsValidForRequest:(const WebKit::InteractionInformationRequest&)request;
- (void)doAfterPositionInformationUpdate:(void (^)(WebKit::InteractionInformationAtPosition))action forRequest:(WebKit::InteractionInformationRequest)request;
@end

@interface WKPointerInteractionController : NSObject

- (instancetype)initWithContentView:(WKContentView<WKPointerInteractionControllerDelegate> *)contentView NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)setUpPointerInteraction;
- (void)cleanUp;

@property (nonatomic, readonly) UIPointerInteraction *pointerInteraction;

- (UIPointerRegion *)regionForRequest:(UIPointerRegionRequest *)request;
- (UIPointerStyle *)styleForRegion:(UIPointerRegion *)region;

@end

#endif // HAVE(UI_POINTER_INTERACTION)
