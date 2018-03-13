/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "WebCredentialsMessengerProxy.h"

#if ENABLE(WEB_AUTHN)

#import <LocalAuthentication/LocalAuthentication.h>
#import <WebCore/NotImplemented.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(LocalAuthentication)
SOFT_LINK_CLASS(LocalAuthentication, LAContext);

namespace WebKit {

void WebCredentialsMessengerProxy::platformIsUserVerifyingPlatformAuthenticatorAvailable(uint64_t messageId)
{
#if defined(__i386__)
    ASSERT_UNUSED(messageId, messageId);
    notImplemented();
#else
    auto context = adoptNS([allocLAContextInstance() init]);
    NSError *error = nil;

    if ([context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:&error])
        isUserVerifyingPlatformAuthenticatorAvailableReply(messageId, true);
    else {
        LOG_ERROR("Couldn't evaluate authentication with biometrics policy: %@", error);
        isUserVerifyingPlatformAuthenticatorAvailableReply(messageId, false);
    }
#endif // defined(__i386__)
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
