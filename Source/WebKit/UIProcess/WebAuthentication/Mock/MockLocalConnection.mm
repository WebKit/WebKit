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
#import "MockLocalConnection.h"

#if ENABLE(WEB_AUTHN)

#import <Security/SecItem.h>
#import <WebCore/AuthenticatorAssertionResponse.h>
#import <WebCore/ExceptionData.h>
#import <wtf/RunLoop.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/Base64.h>
#import <wtf/text/WTFString.h>

#import "LocalAuthenticationSoftLink.h"

namespace WebKit {
using namespace WebCore;

MockLocalConnection::MockLocalConnection(const MockWebAuthenticationConfiguration& configuration)
    : m_configuration(configuration)
{
}

void MockLocalConnection::verifyUser(const String&, ClientDataType, SecAccessControlRef, UserVerificationCallback&& callback)
{
    // Mock async operations.
    RunLoop::main().dispatch([configuration = m_configuration, callback = WTFMove(callback)]() mutable {
        ASSERT(configuration.local);

        UserVerification userVerification = UserVerification::No;
        switch (configuration.local->userVerification) {
        case MockWebAuthenticationConfiguration::UserVerification::No:
            break;
        case MockWebAuthenticationConfiguration::UserVerification::Yes:
            userVerification = UserVerification::Yes;
            break;
        case MockWebAuthenticationConfiguration::UserVerification::Cancel:
            userVerification = UserVerification::Cancel;
        }

        callback(userVerification, adoptNS([allocLAContextInstance() init]).get());
    });
}

RetainPtr<SecKeyRef> MockLocalConnection::createCredentialPrivateKey(LAContext *, SecAccessControlRef, const String& secAttrLabel, NSData *secAttrApplicationTag) const
{
    ASSERT(m_configuration.local);

    // Get Key and add it to Keychain.
    NSDictionary* options = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrKeySizeInBits: @256,
    };
    CFErrorRef errorRef = nullptr;
    auto key = adoptCF(SecKeyCreateWithData(
        (__bridge CFDataRef)adoptNS([[NSData alloc] initWithBase64EncodedString:m_configuration.local->privateKeyBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get(),
        (__bridge CFDictionaryRef)options,
        &errorRef
    ));
    if (errorRef)
        return nullptr;

    NSDictionary* addQuery = @{
        (id)kSecValueRef: (id)key.get(),
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: secAttrLabel,
        (id)kSecAttrApplicationTag: secAttrApplicationTag,
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
#if HAVE(DATA_PROTECTION_KEYCHAIN)
        (id)kSecUseDataProtectionKeychain: @YES
#else
        (id)kSecAttrNoLegacy: @YES
#endif
    };
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery, NULL);
    if (status) {
        LOG_ERROR("Couldn't add the key to the keychain. %d", status);
        return nullptr;
    }

    return key;
}

void MockLocalConnection::getAttestation(SecKeyRef, NSData *, NSData *, AttestationCallback&& callback) const
{
    // Mock async operations.
    RunLoop::main().dispatch([configuration = m_configuration, callback = WTFMove(callback)]() mutable {
        ASSERT(configuration.local);
        if (!configuration.local->acceptAttestation) {
            callback(NULL, [NSError errorWithDomain:@"WebAuthentication" code:-1 userInfo:@{ NSLocalizedDescriptionKey: @"The operation couldn't complete." }]);
            return;
        }

        auto attestationCertificate = adoptCF(SecCertificateCreateWithData(NULL, (__bridge CFDataRef)adoptNS([[NSData alloc] initWithBase64EncodedString:configuration.local->userCertificateBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get()));
        auto attestationIssuingCACertificate = adoptCF(SecCertificateCreateWithData(NULL, (__bridge CFDataRef)adoptNS([[NSData alloc] initWithBase64EncodedString:configuration.local->intermediateCACertificateBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get()));
        callback(@[ (__bridge id)attestationCertificate.get(), (__bridge id)attestationIssuingCACertificate.get()], NULL);
    });
}

void MockLocalConnection::filterResponses(Vector<Ref<AuthenticatorAssertionResponse>>& responses) const
{
    const auto& preferredCredentialIdBase64 = m_configuration.local->preferredCredentialIdBase64;
    if (preferredCredentialIdBase64.isEmpty())
        return;

    auto itr = responses.begin();
    for (; itr != responses.end(); ++itr) {
        auto* rawId = itr->get().rawId();
        ASSERT(rawId);
        auto rawIdBase64 = base64Encode(rawId->data(), rawId->byteLength());
        if (rawIdBase64 == preferredCredentialIdBase64)
            break;
    }
    auto response = itr->copyRef();
    responses.clear();
    responses.append(WTFMove(response));
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
