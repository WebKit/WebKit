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

#import "AppKitSPI.h"
#import "InteractionInformationAtPosition.h"
#import "TransactionID.h"
#import "WKAppKitGestureController.h"
#import "WKClickHighlightController.h"
#import "WKDeferringGestureRecognizer.h"
#import "WKPointerTrackingGestureRecognizer.h"
#import "WKPressGestureRecognizer.h"
#import <WebCore/ElementContext.h>
#import <optional>

namespace WebKit {

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
class AppKitGestureMomentumEventGenerator;
#endif

using OptionalTransactionID = std::optional<TransactionID>;

namespace CxxInteropSupport {

// ENABLE(MODEL_ELEMENT_STAGE_MODE) is an expression, which Swift's compilation conditions do not include.
inline bool isInteractiveModel(const InteractionInformationAtPosition& info)
{
#if ENABLE(MODEL_ELEMENT_STAGE_MODE)
    return info.isInteractiveModel;
#else
    UNUSED_PARAM(info);
    return false;
#endif
}

} // namespace CxxInteropSupport

} // namespace WebKit

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

// The C++ that WKAppKitGestureController uses and Swift cannot call: Live Text's image analysis and the page's drawing
// area. It also owns the momentum event generator and hands the page's position information to the controller.
NS_SWIFT_UI_ACTOR
@interface WKAppKitGestureControllerCxxSupport : NSObject

@property (nonatomic, weak, nullable) WKAppKitGestureController *controller;

// Position information from the page, which -positionInformationDidChange: hands to the handler: an Objective-C method
// that Swift implements cannot take the C++ struct.
- (void)setPositionInformationDidChangeHandler:(NS_SWIFT_UI_ACTOR void (^)(WebKit::InteractionInformationAtPosition info))handler;
#if !__has_feature(modules)
- (void)positionInformationDidChange:(const WebKit::InteractionInformationAtPosition&)info;
#endif

// Live Text, for the position information of a press. The completion is not called when the press is not over an image
// Live Text can analyze, or when the controller or its web view goes away first.
- (void)preflightImageAnalysis:(WebKit::InteractionInformationAtPosition)info completion:(NS_SWIFT_UI_ACTOR void (^)(BOOL foundText, WebCore::ElementContext element))completion;

// Momentum. -resetMomentum destroys the momentum event generator.
- (void)resetMomentum;

// The page. doAfterProcessingAllPendingMouseEvents: drops its completion when the controller has no web view.
- (uint8_t)touchActionsAtPoint:(NSPoint)point;
@property (nonatomic, readonly) WebKit::OptionalTransactionID lastCommittedMainFrameLayerTreeTransactionID;
- (void)doAfterProcessingAllPendingMouseEvents:(NS_SWIFT_UI_ACTOR void (^)(void))completion NS_SWIFT_NAME(doAfterProcessingAllPendingMouseEvents(_:));

@end

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
namespace WebKit::CxxInteropSupport {

// The support object's momentum event generator, if it has one, and made if it has none. In Swift, the result of a
// main-actor function is isolated to the main actor; an Objective-C method's result could be sent to another thread.
NS_SWIFT_UI_ACTOR AppKitGestureMomentumEventGenerator* WTF_NULLABLE momentumEventGenerator(WKAppKitGestureControllerCxxSupport *);
NS_SWIFT_UI_ACTOR AppKitGestureMomentumEventGenerator& ensureMomentumEventGenerator(WKAppKitGestureControllerCxxSupport *);

} // namespace WebKit::CxxInteropSupport
#endif

@interface WKAppKitGestureController ()
@property (nonatomic, readonly) WKAppKitGestureControllerCxxSupport *cxxSupport;
@end

@interface WKAppKitGestureController (WKClickHighlightControllerDelegate) <WKClickHighlightControllerDelegate>
@end

@interface WKAppKitGestureController (NSGestureRecognizerDelegate) <NSGestureRecognizerDelegatePrivate>
@end

@interface WKAppKitGestureController (WKDeferringGestureRecognizerDelegate) <WKDeferringGestureRecognizerDelegate>
@end

@interface WKAppKitGestureController (WKPointerTrackingGestureRecognizerDelegate) <WKPointerTrackingGestureRecognizerDelegate>
@end

@interface WKAppKitGestureController (WKPressGestureRecognizerDelegate) <WKPressGestureRecognizerDelegate>
@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // HAVE(APPKIT_GESTURES_SUPPORT)
