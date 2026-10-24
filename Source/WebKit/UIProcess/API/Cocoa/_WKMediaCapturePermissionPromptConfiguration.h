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

#import <Foundation/Foundation.h>
#import <WebKit/WKFoundation.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

typedef NS_ENUM(NSInteger, _WKMediaCapturePermissionActionDecision) {
    _WKMediaCapturePermissionActionDecisionDeny,
    _WKMediaCapturePermissionActionDecisionAllow,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

typedef NS_ENUM(NSInteger, _WKMediaCapturePermissionPromptOutcome) {
    _WKMediaCapturePermissionPromptOutcomeAllowed,
    _WKMediaCapturePermissionPromptOutcomeDenied,
    _WKMediaCapturePermissionPromptOutcomeDismissed,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

WK_EXTERN const NSUInteger _WKMediaCapturePermissionPromptMaximumAdditionalActions WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKMediaCapturePermissionAction : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

+ (instancetype)actionWithTitle:(NSString *)title decision:(_WKMediaCapturePermissionActionDecision)decision;

@property (nonatomic, readonly, copy) NSString *title;
@property (nonatomic, readonly) _WKMediaCapturePermissionActionDecision decision;

@end

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKMediaCapturePermissionPromptConfiguration : NSObject

/*! @abstract Text at the top of the prompt.
 */
@property (nonatomic, copy, nullable) NSString *messageText;

/*! @abstract Text shown below the message.
 @discussion Unlike the other strings this has no WebKit default, so the prompt shows no informative
 text unless the application sets one.
 */
@property (nonatomic, copy, nullable) NSString *informativeText;

@property (nonatomic, copy, nullable) NSString *allowButtonTitle;
@property (nonatomic, copy, nullable) NSString *denyButtonTitle;

/*! @abstract Buttons shown between the allow and deny buttons. Empty by default. */
@property (nonatomic, copy) NSArray<_WKMediaCapturePermissionAction *> *additionalActions;

@end

/*! @abstract What the user did with WebKit's capture permission prompt.
 @discussion outcome is Allowed when the user activated the built-in allow button or an additional
 action whose decision is allow, and Denied when they activated the deny button or an additional action
 whose decision is deny. It is Dismissed when the user did not answer: WebKit dismissed the prompt,
 could not show it, or found no request being decided.

 chosenAction is nil when the user activated the built-in allow or deny button, and is
 otherwise the object from additionalActions that was activated.
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKMediaCapturePermissionPromptResult : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@property (nonatomic, readonly) _WKMediaCapturePermissionPromptOutcome outcome;
@property (nonatomic, readonly, nullable) _WKMediaCapturePermissionAction *chosenAction;

@end

NS_HEADER_AUDIT_END(nullability, sendability)
