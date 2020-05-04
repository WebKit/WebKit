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

#import <WebCore/LocalizedStrings.h>
#import <WebCore/WebAuthenticationConstants.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/LocalConnectionAdditions.h>
#endif

#import "LocalAuthenticationSoftLink.h"

namespace WebKit {
using namespace WebCore;

namespace {
#if PLATFORM(MAC)
static inline String bundleName()
{
    return [[NSRunningApplication currentApplication] localizedName];
}
#endif
} // namespace

LocalConnection::~LocalConnection()
{
    // Dismiss any showing LocalAuthentication dialogs.
    [m_context invalidate];
}

void LocalConnection::verifyUser(const String& rpId, ClientDataType type, SecAccessControlRef accessControl, UserVerificationCallback&& completionHandler)
{
    String title = genericTouchIDPromptTitle();
#if PLATFORM(MAC)
    switch (type) {
    case ClientDataType::Create:
        title = makeCredentialTouchIDPromptTitle(bundleName(), rpId);
        break;
    case ClientDataType::Get:
        title = getAssertionTouchIDPromptTitle(bundleName(), rpId);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
#endif

    m_context = [allocLAContextInstance() init];

    auto options = adoptNS([[NSMutableDictionary alloc] init]);
    if ([m_context biometryType] == LABiometryTypeTouchID) {
        [options setObject:title forKey:@(LAOptionAuthenticationTitle)];
        [options setObject:@NO forKey:@(LAOptionFallbackVisible)];
    }

    auto reply = makeBlockPtr([context = m_context, completionHandler = WTFMove(completionHandler)] (NSDictionary *, NSError *error) mutable {
        UserVerification verification = UserVerification::Yes;
        if (error) {
            LOG_ERROR("Couldn't authenticate with biometrics: %@", error);
            verification = UserVerification::No;
            if (error.code == LAErrorUserCancel)
                verification = UserVerification::Cancel;
        }

        // This block can be executed in another thread.
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), verification, context = WTFMove(context)] () mutable {
            completionHandler(verification, context.get());
        });
    });

    [m_context evaluateAccessControl:accessControl operation:LAAccessControlOperationUseKeySign options:options.get() reply:reply.get()];
}

RetainPtr<SecKeyRef> LocalConnection::createCredentialPrivateKey(LAContext *context, SecAccessControlRef accessControlRef, const String& secAttrLabel, NSData *secAttrApplicationTag) const
{
    NSDictionary *attributes = @{
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDSecureEnclave,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecPrivateKeyAttrs: @{
            (id)kSecUseAuthenticationContext: context,
            (id)kSecAttrAccessControl: (id)accessControlRef,
            (id)kSecAttrIsPermanent: @YES,
            (id)kSecAttrAccessGroup: (id)String(LocalAuthenticatiorAccessGroup),
            (id)kSecAttrLabel: secAttrLabel,
            (id)kSecAttrApplicationTag: secAttrApplicationTag,
        }};
    CFErrorRef errorRef = nullptr;
    auto credentialPrivateKey = adoptCF(SecKeyCreateRandomKey((__bridge CFDictionaryRef)attributes, &errorRef));
    auto retainError = adoptCF(errorRef);
    if (errorRef) {
        LOG_ERROR("Couldn't create private key: %@", (NSError *)errorRef);
        return nullptr;
    }
    return credentialPrivateKey;
}

void LocalConnection::getAttestation(SecKeyRef privateKey, NSData *authData, NSData *hash, AttestationCallback&& completionHandler) const
{
#if defined(LOCALCONNECTION_ADDITIONS)
LOCALCONNECTION_ADDITIONS
#endif
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
