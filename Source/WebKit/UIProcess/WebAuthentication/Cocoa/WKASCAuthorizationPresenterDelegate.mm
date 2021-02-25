/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WKASCAuthorizationPresenterDelegate.h"

#if ENABLE(WEB_AUTHN) && HAVE(ASC_AUTH_UI)

#import "AuthenticatorPresenterCoordinator.h"

#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>
#import "AuthenticationServicesCoreSoftLink.h"

NS_ASSUME_NONNULL_BEGIN

@implementation WKASCAuthorizationPresenterDelegate {
    WeakPtr<WebKit::AuthenticatorPresenterCoordinator> _coordinator;
}

- (instancetype)initWithCoordinator:(WebKit::AuthenticatorPresenterCoordinator&)coordinator
{
    if ((self = [super init]))
        _coordinator = makeWeakPtr(coordinator);
    return self;
}

- (void)authorizationPresenter:(ASCAuthorizationPresenter *)presenter credentialRequestedForLoginChoice:(id <ASCLoginChoiceProtocol>)loginChoice authenticatedContext:(nullable LAContext *)context completionHandler:(void (^)(id <ASCCredentialProtocol> _Nullable credential, NSError * _Nullable error))completionHandler
{
    auto requestHandler = [completionHandler = makeBlockPtr(completionHandler)] (ASCAppleIDCredential *credential, NSError *error) {
        completionHandler(credential, error);
    };
    [self dispatchCoordinatorCallback:[requestHandler = WTFMove(requestHandler)] (WebKit::AuthenticatorPresenterCoordinator& coordinator) mutable {
        coordinator.setCredentialRequestHandler(WTFMove(requestHandler));
    }];

    if ([loginChoice isKindOfClass:WebKit::getASCPlatformPublicKeyCredentialLoginChoiceClass()]) {
        auto *platformLoginChoice = (ASCPlatformPublicKeyCredentialLoginChoice *)loginChoice;

        if ([platformLoginChoice isRegistrationRequest]) {
            [self dispatchCoordinatorCallback:[context = retainPtr(context)] (WebKit::AuthenticatorPresenterCoordinator& coordinator) mutable {
                coordinator.setLAContext(context.get());
            }];

            return;
        }

        String loginChoiceName = platformLoginChoice.name;
        [self dispatchCoordinatorCallback:[loginChoiceName = WTFMove(loginChoiceName), context = retainPtr(context)] (WebKit::AuthenticatorPresenterCoordinator& coordinator) mutable {
            coordinator.didSelectAssertionResponse(loginChoiceName, context.get());
        }];

        return;
    }

    if ([loginChoice isKindOfClass:WebKit::getASCSecurityKeyPublicKeyCredentialLoginChoiceClass()]) {
        auto *securityKeyLoginChoice = (ASCSecurityKeyPublicKeyCredentialLoginChoice *)loginChoice;

        if ([securityKeyLoginChoice loginChoiceKind] == ASCSecurityKeyPublicKeyCredentialLoginChoiceKindAssertion) {
            String loginChoiceName = securityKeyLoginChoice.name;
            [self dispatchCoordinatorCallback:[loginChoiceName = WTFMove(loginChoiceName)] (WebKit::AuthenticatorPresenterCoordinator& coordinator) mutable {
                coordinator.didSelectAssertionResponse(loginChoiceName, nil);
            }];

            return;
        }
    }
}

- (void)authorizationPresenter:(ASCAuthorizationPresenter *)presenter validateUserEnteredPIN:(NSString *)pin completionHandler:(void (^)(id <ASCCredentialProtocol> credential, NSError *error))completionHandler
{
    auto requestHandler = [completionHandler = makeBlockPtr(completionHandler)] (ASCAppleIDCredential *credential, NSError *error) {
        completionHandler(credential, error);
    };
    [self dispatchCoordinatorCallback:[requestHandler = WTFMove(requestHandler)] (WebKit::AuthenticatorPresenterCoordinator& coordinator) mutable {
        coordinator.setCredentialRequestHandler(WTFMove(requestHandler));
    }];

    String pinString = pin;
    [self dispatchCoordinatorCallback:[pinString = WTFMove(pinString)] (WebKit::AuthenticatorPresenterCoordinator& coordinator) mutable {
        coordinator.setPin(pinString);
    }];
}

- (void)dispatchCoordinatorCallback:(Function<void(WebKit::AuthenticatorPresenterCoordinator&)>&&)callback
{
    ASSERT(!RunLoop::isMain());
    RunLoop::main().dispatch([coordinator = _coordinator, callback = WTFMove(callback)] {
        if (!coordinator)
            return;
        callback(*coordinator);
    });
}

@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(WEB_AUTHN) && HAVE(ASC_AUTH_UI)
