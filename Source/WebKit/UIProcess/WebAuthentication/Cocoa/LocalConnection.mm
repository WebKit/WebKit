/*
 * Copyright (C) 2018=2021 Apple Inc. All rights reserved.
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
#import <WebCore/UserVerificationRequirement.h>
#import <WebCore/WebAuthenticationConstants.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/SpanCocoa.h>

#import "AppAttestInternalSoftLink.h"
#import "LocalAuthenticationSoftLink.h"

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/LocalConnectionAdditions.h>
#else
#define LOCAL_CONNECTION_ADDITIONS
#endif

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

WTF_MAKE_TZONE_ALLOCATED_IMPL(LocalConnection);

Ref<LocalConnection> LocalConnection::create()
{
    return adoptRef(*new LocalConnection);
}

LocalConnection::~LocalConnection()
{
    // Dismiss any showing LocalAuthentication dialogs.
    [m_context invalidate];
}

void LocalConnection::verifyUser(const String& rpId, ClientDataType type, SecAccessControlRef accessControl, UserVerificationRequirement uv, UserVerificationCallback&& completionHandler)
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
#if HAVE(UNIFIED_ASC_AUTH_UI)
    if ([m_context biometryType] == LABiometryTypeTouchID) {
        [options setObject:title forKey:@(LAOptionAuthenticationTitle)];
        [options setObject:@NO forKey:@(LAOptionFallbackVisible)];
    }
#endif

    auto reply = makeBlockPtr([context = m_context, completionHandler = WTFMove(completionHandler)] (NSDictionary *information, NSError *error) mutable {
        UserVerification verification = UserVerification::Yes;
        if (error) {
            LOG_ERROR("Couldn't authenticate with biometrics: %@", error);
            verification = UserVerification::No;
            if (error.code == LAErrorUserCancel)
                verification = UserVerification::Cancel;
        }
        if (information[@"UserPresence"])
            verification = UserVerification::Presence;

        // This block can be executed in another thread.
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), verification, context = WTFMove(context)] () mutable {
            completionHandler(verification, context.get());
        });
    });

#if USE(APPLE_INTERNAL_SDK)
    // Depending on certain internal requirements, accessControl might not require user verifications.
    // Hence, here introduces a quirk to force the compatible mode to require user verifications if necessary.
    if (shouldUseAlternateAttributes()) {
        NSError *error = nil;
        auto canEvaluatePolicy = [m_context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:&error];
        if (error.code == LAErrorBiometryLockout)
            canEvaluatePolicy = true;

        if (uv == UserVerificationRequirement::Required || canEvaluatePolicy) {
            [m_context evaluatePolicy:LAPolicyDeviceOwnerAuthentication options:options.get() reply:reply.get()];
            return;
        }

        reply(@{ @"UserPresence": @YES }, nullptr);
        return;
    }
#endif

    [m_context evaluateAccessControl:accessControl operation:LAAccessControlOperationUseKeySign options:options.get() reply:reply.get()];
}

void LocalConnection::verifyUser(SecAccessControlRef accessControl, LAContext *context, CompletionHandler<void(UserVerification)>&& completionHandler)
{
    auto options = adoptNS([[NSMutableDictionary alloc] init]);
    [options setObject:@YES forKey:@(LAOptionNotInteractive)];

    auto reply = makeBlockPtr([completionHandler = WTFMove(completionHandler)] (NSDictionary *information, NSError *error) mutable {
        UserVerification verification = UserVerification::Yes;
        if (error) {
            LOG_ERROR("Couldn't authenticate with biometrics: %@", error);
            verification = UserVerification::No;
            if (error.code == LAErrorUserCancel)
                verification = UserVerification::Cancel;
        }
        if (information[@"UserPresence"])
            verification = UserVerification::Presence;

        // This block can be executed in another thread.
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), verification] () mutable {
            completionHandler(verification);
        });
    });

#if USE(APPLE_INTERNAL_SDK)
    // Depending on certain internal requirements, context might be nil. In that case, just check user presence.
    if (shouldUseAlternateAttributes() && !context) {
        reply(@{ @"UserPresence": @YES }, nullptr);
        return;
    }

#if PLATFORM(IOS_FAMILY_SIMULATOR)
    // Simulator doesn't support LAAccessControlOperationUseKeySign, but does support alternate attributes.
    if (shouldUseAlternateAttributes()) {
        reply(@{ }, nullptr);
        return;
    }
#endif
#endif // USE(APPLE_INTERNAL_SDK)

    [context evaluateAccessControl:accessControl operation:LAAccessControlOperationUseKeySign options:options.get() reply:reply.get()];
}

RetainPtr<SecKeyRef> LocalConnection::createCredentialPrivateKey(LAContext *context, SecAccessControlRef accessControlRef, const String& secAttrLabel, NSData *secAttrApplicationTag) const
{
    RetainPtr privateKeyAttributes = @{
        (id)kSecAttrAccessControl: (id)accessControlRef,
        (id)kSecAttrIsPermanent: @YES,
        (id)kSecAttrAccessGroup: LocalAuthenticatorAccessGroup,
        (id)kSecAttrLabel: secAttrLabel,
        (id)kSecAttrApplicationTag: secAttrApplicationTag,
    };

    if (context) {
        auto mutableCopy = adoptNS([privateKeyAttributes mutableCopy]);
        mutableCopy.get()[(id)kSecUseAuthenticationContext] = context;
        privateKeyAttributes = WTFMove(mutableCopy);
    }

    NSDictionary *attributes = @{
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDSecureEnclave,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecPrivateKeyAttrs: privateKeyAttributes.get(),
    };

    LOCAL_CONNECTION_ADDITIONS
    CFErrorRef errorRef = nullptr;
    auto credentialPrivateKey = adoptCF(SecKeyCreateRandomKey((__bridge CFDictionaryRef)attributes, &errorRef));
    auto retainError = adoptCF(errorRef);
    if (errorRef) {
        LOG_ERROR("Couldn't create private key: %@", (NSError *)errorRef);
        return nullptr;
    }
    return credentialPrivateKey;
}

RetainPtr<NSArray> LocalConnection::getExistingCredentials(const String& rpId)
{
    // Search Keychain for existing credential matched the RP ID.
    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
        (id)kSecAttrAccessGroup: LocalAuthenticatorAccessGroup,
        (id)kSecAttrLabel: rpId,
        (id)kSecReturnAttributes: @YES,
        (id)kSecMatchLimit: (id)kSecMatchLimitAll,
        (id)kSecUseDataProtectionKeychain: @YES
    };

    CFTypeRef attributesArrayRef = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &attributesArrayRef);
    if (status && status != errSecItemNotFound)
        return nullptr;
    auto retainAttributesArray = adoptCF(attributesArrayRef);
    NSArray *sortedAttributesArray = [(NSArray *)attributesArrayRef sortedArrayUsingComparator:^(NSDictionary *a, NSDictionary *b) {
        return [b[(id)kSecAttrModificationDate] compare:a[(id)kSecAttrModificationDate]];
    }];
    return retainPtr(sortedAttributesArray);
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
