/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "WKNSURLAuthenticationChallenge.h"

#if WK_API_ENABLED

#import "AuthenticationDecisionListener.h"
#import "WebCertificateInfo.h"
#import "WebCredential.h"
#import <WebCore/AuthenticationMac.h>

using namespace WebCore;
using namespace WebKit;

@interface WKNSURLAuthenticationChallengeSender : NSObject <NSURLAuthenticationChallengeSender>
@end

@implementation WKNSURLAuthenticationChallenge

- (NSObject *)_web_createTarget
{
    AuthenticationChallengeProxy& challenge = *reinterpret_cast<AuthenticationChallengeProxy*>(&self._apiObject);

    static dispatch_once_t token;
    static WKNSURLAuthenticationChallengeSender *sender;
    dispatch_once(&token, ^{
        sender = [[WKNSURLAuthenticationChallengeSender alloc] init];
    });

    return [[NSURLAuthenticationChallenge alloc] initWithAuthenticationChallenge:mac(challenge.core()) sender:sender];
}

- (AuthenticationChallengeProxy&)_web_authenticationChallengeProxy
{
    return *reinterpret_cast<AuthenticationChallengeProxy*>(&self._apiObject);
}

@end

@implementation WKNSURLAuthenticationChallengeSender

static void checkChallenge(NSURLAuthenticationChallenge *challenge)
{
    if ([challenge class] != [WKNSURLAuthenticationChallenge class])
        [NSException raise:NSInvalidArgumentException format:@"The challenge was not sent by the receiver."];
}

- (void)cancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    checkChallenge(challenge);
    AuthenticationChallengeProxy& webChallenge = ((WKNSURLAuthenticationChallenge *)challenge)._web_authenticationChallengeProxy;
    webChallenge.listener()->cancel();
}

- (void)continueWithoutCredentialForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    checkChallenge(challenge);
    AuthenticationChallengeProxy& webChallenge = ((WKNSURLAuthenticationChallenge *)challenge)._web_authenticationChallengeProxy;
    webChallenge.listener()->useCredential(nullptr);
}

- (void)useCredential:(NSURLCredential *)credential forAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    checkChallenge(challenge);
    AuthenticationChallengeProxy& webChallenge = ((WKNSURLAuthenticationChallenge *)challenge)._web_authenticationChallengeProxy;
    webChallenge.listener()->useCredential(WebCredential::create(core(credential)).get());
}

- (void)performDefaultHandlingForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    checkChallenge(challenge);
    AuthenticationChallengeProxy& webChallenge = ((WKNSURLAuthenticationChallenge *)challenge)._web_authenticationChallengeProxy;
    webChallenge.listener()->performDefaultHandling();
}

- (void)rejectProtectionSpaceAndContinueWithChallenge:(NSURLAuthenticationChallenge *)challenge
{
    checkChallenge(challenge);
    AuthenticationChallengeProxy& webChallenge = ((WKNSURLAuthenticationChallenge *)challenge)._web_authenticationChallengeProxy;
    webChallenge.listener()->rejectProtectionSpaceAndContinue();
}

@end

#endif // WK_API_ENABLED
