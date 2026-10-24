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

#import "config.h"
#import "_WKMediaCapturePermissionPromptConfigurationInternal.h"

#if ENABLE(MEDIA_STREAM)
#import "Logging.h"
#endif

const NSUInteger _WKMediaCapturePermissionPromptMaximumAdditionalActions = WebKit::maximumPromptAdditionalActions;

@implementation _WKMediaCapturePermissionAction {
    RetainPtr<NSString> _title;
    _WKMediaCapturePermissionActionDecision _decision;
}

+ (instancetype)actionWithTitle:(NSString *)title decision:(_WKMediaCapturePermissionActionDecision)decision
{
    RetainPtr action = adoptNS([[_WKMediaCapturePermissionAction alloc] init]);
    action->_title = adoptNS([title copy]);
    action->_decision = decision;
    return action.autorelease();
}

- (NSString *)title
{
    return _title.get();
}

- (_WKMediaCapturePermissionActionDecision)decision
{
    return _decision;
}

@end

@implementation _WKMediaCapturePermissionPromptConfiguration {
    RetainPtr<NSString> _messageText;
    RetainPtr<NSString> _informativeText;
    RetainPtr<NSString> _allowButtonTitle;
    RetainPtr<NSString> _denyButtonTitle;
    RetainPtr<NSArray<_WKMediaCapturePermissionAction *>> _additionalActions;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _additionalActions = @[ ];

    return self;
}

- (NSString *)messageText
{
    return _messageText.get();
}

- (void)setMessageText:(NSString *)messageText
{
    _messageText = adoptNS([messageText copy]);
}

- (NSString *)informativeText
{
    return _informativeText.get();
}

- (void)setInformativeText:(NSString *)informativeText
{
    _informativeText = adoptNS([informativeText copy]);
}

- (NSString *)allowButtonTitle
{
    return _allowButtonTitle.get();
}

- (void)setAllowButtonTitle:(NSString *)allowButtonTitle
{
    _allowButtonTitle = adoptNS([allowButtonTitle copy]);
}

- (NSString *)denyButtonTitle
{
    return _denyButtonTitle.get();
}

- (void)setDenyButtonTitle:(NSString *)denyButtonTitle
{
    _denyButtonTitle = adoptNS([denyButtonTitle copy]);
}

- (NSArray<_WKMediaCapturePermissionAction *> *)additionalActions
{
    return _additionalActions.get();
}

- (void)setAdditionalActions:(NSArray<_WKMediaCapturePermissionAction *> *)additionalActions
{
    _additionalActions = adoptNS([additionalActions copy]);
}

@end

@implementation _WKMediaCapturePermissionPromptResult {
    _WKMediaCapturePermissionPromptOutcome _outcome;
    RetainPtr<_WKMediaCapturePermissionAction> _chosenAction;
}

- (instancetype)_initWithOutcome:(_WKMediaCapturePermissionPromptOutcome)outcome chosenAction:(_WKMediaCapturePermissionAction *)chosenAction
{
    if (!(self = [super init]))
        return nil;

    _outcome = outcome;
    _chosenAction = chosenAction;

    return self;
}

- (_WKMediaCapturePermissionPromptOutcome)outcome
{
    return _outcome;
}

- (_WKMediaCapturePermissionAction *)chosenAction
{
    return _chosenAction.get();
}

@end

#if ENABLE(MEDIA_STREAM)

namespace WebKit {

MediaPermissionPromptCustomization customizationFromConfiguration(_WKMediaCapturePermissionPromptConfiguration *configuration)
{
    if (!configuration)
        return { };

    MediaPermissionPromptCustomization customization {
        .messageText = [configuration messageText],
        .informativeText = [configuration informativeText],
        .allowButtonTitle = [configuration allowButtonTitle],
        .denyButtonTitle = [configuration denyButtonTitle],
    };

    NSArray<_WKMediaCapturePermissionAction *> *actions = [configuration additionalActions];
    NSUInteger count = [actions count];
    if (count > maximumPromptAdditionalActions) {
        RELEASE_LOG_ERROR(WebRTC, "_WKMediaCapturePermissionPromptConfiguration: %lu additional actions supplied, only the first %zu are rendered", static_cast<unsigned long>(count), maximumPromptAdditionalActions);
        count = maximumPromptAdditionalActions;
    }

    customization.additionalActions = Vector<MediaPermissionPromptAdditionalAction>(count, [&](size_t index) {
        _WKMediaCapturePermissionAction *action = [actions objectAtIndex:index];
        return MediaPermissionPromptAdditionalAction {
            .title = [action title],
            .decision = [action decision] == _WKMediaCapturePermissionActionDecisionAllow ? MediaPermissionPromptAdditionalAction::Decision::Allow : MediaPermissionPromptAdditionalAction::Decision::Deny,
            .source = action,
        };
    });

    return customization;
}

}

#endif // ENABLE(MEDIA_STREAM)
