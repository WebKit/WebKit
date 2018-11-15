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
#import "LocalConnection.h"

#if ENABLE(WEB_AUTHN)

#import "DeviceIdentitySPI.h"
#import <WebCore/ExceptionData.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>

#import "LocalAuthenticationSoftLink.h"

namespace WebKit {

void LocalConnection::getUserConsent(const String& reason, UserConsentCallback&& completionHandler) const
{
    // FIXME(182772)
#if PLATFORM(IOS_FAMILY)
    auto context = adoptNS([allocLAContextInstance() init]);
    auto reply = BlockPtr<void(BOOL, NSError *)>::fromCallable([completionHandler = WTFMove(completionHandler)] (BOOL success, NSError *error) mutable {
        ASSERT(!RunLoop::isMain());

        UserConsent consent = UserConsent::Yes;
        if (!success || error) {
            LOG_ERROR("Couldn't authenticate with biometrics: %@", error);
            consent = UserConsent::No;
        }
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), consent]() mutable {
            completionHandler(consent);
        });
    });
    [context evaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics localizedReason:reason reply:reply.get()];
#endif
}

void LocalConnection::getUserConsent(const String& reason, SecAccessControlRef accessControl, UserConsentContextCallback&& completionHandler) const
{
    // FIXME(182772)
#if PLATFORM(IOS_FAMILY)
    auto context = adoptNS([allocLAContextInstance() init]);
    auto reply = BlockPtr<void(BOOL, NSError *)>::fromCallable([context, completionHandler = WTFMove(completionHandler)] (BOOL success, NSError *error) mutable {
        ASSERT(!RunLoop::isMain());

        UserConsent consent = UserConsent::Yes;
        if (!success || error) {
            LOG_ERROR("Couldn't authenticate with biometrics: %@", error);
            consent = UserConsent::No;
        }
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), consent, context = WTFMove(context)]() mutable {
            completionHandler(consent, context.get());
        });
    });
    [context evaluateAccessControl:accessControl operation:LAAccessControlOperationUseKeySign localizedReason:reason reply:reply.get()];
#endif
}

void LocalConnection::getAttestation(const String& rpId, const String& username, const Vector<uint8_t>& hash, AttestationCallback&& completionHandler) const
{
    // DeviceIdentity.Framework is not avaliable in iOS simulator.
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    // Apple Attestation
    ASSERT(hash.size() <= 32);

    RetainPtr<SecAccessControlRef> accessControlRef;
    {
        CFErrorRef errorRef = nullptr;
        accessControlRef = adoptCF(SecAccessControlCreateWithFlags(NULL, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, kSecAccessControlPrivateKeyUsage | kSecAccessControlUserPresence, &errorRef));
        auto retainError = adoptCF(errorRef);
        if (errorRef) {
            LOG_ERROR("Couldn't create ACL: %@", (NSError *)errorRef);
            completionHandler(NULL, NULL, [NSError errorWithDomain:@"com.apple.WebKit.WebAuthN" code:1 userInfo:nil]);
            return;
        }
    }

    String label = makeString(username, "@", rpId);
    NSDictionary *options = @{
        kMAOptionsBAAKeychainLabel: label,
        // FIXME(rdar://problem/38489134): Need a formal name.
        kMAOptionsBAAKeychainAccessGroup: @"com.apple.safari.WebAuthN.credentials",
        kMAOptionsBAAIgnoreExistingKeychainItems: @(YES),
        // FIXME(rdar://problem/38489134): Determine a proper lifespan.
        kMAOptionsBAAValidity: @(1440), // Last one day.
        kMAOptionsBAASCRTAttestation: @(NO),
        kMAOptionsBAANonce: [NSData dataWithBytes:hash.data() length:hash.size()],
        kMAOptionsBAAAccessControls: (id)accessControlRef.get(),
        kMAOptionsBAAOIDSToInclude: @[kMAOptionsBAAOIDNonce]
    };

    // FIXME(183652): Reduce prompt for biometrics
    DeviceIdentityIssueClientCertificateWithCompletion(dispatch_get_main_queue(), options, BlockPtr<void(SecKeyRef, NSArray *, NSError *)>::fromCallable(WTFMove(completionHandler)).get());
#endif
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
