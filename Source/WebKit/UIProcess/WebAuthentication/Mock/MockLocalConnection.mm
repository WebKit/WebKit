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

#import <WebCore/ExceptionData.h>
#import <wtf/RunLoop.h>
#import <wtf/text/WTFString.h>

#import "LocalAuthenticationSoftLink.h"

namespace WebKit {

MockLocalConnection::MockLocalConnection(const MockWebAuthenticationConfiguration& configuration)
    : m_configuration(configuration)
{
}

void MockLocalConnection::getUserConsent(const String&, UserConsentCallback&& callback) const
{
    // Mock async operations.
    RunLoop::main().dispatch([configuration = m_configuration, callback = WTFMove(callback)]() mutable {
        ASSERT(configuration.local);
        if (!configuration.local->acceptAuthentication) {
            callback(UserConsent::No);
            return;
        }
        callback(UserConsent::Yes);
    });
}

void MockLocalConnection::getUserConsent(const String&, SecAccessControlRef, UserConsentContextCallback&& callback) const
{
    // Mock async operations.
    RunLoop::main().dispatch([configuration = m_configuration, callback = WTFMove(callback)]() mutable {
        ASSERT(configuration.local);
        if (!configuration.local->acceptAuthentication) {
            callback(UserConsent::No, nil);
            return;
        }
        callback(UserConsent::Yes, adoptNS([allocLAContextInstance() init]).get());
    });
}

void MockLocalConnection::getAttestation(const String& rpId, const String& username, const Vector<uint8_t>& hash, AttestationCallback&& callback) const
{
    // Mock async operations.
    RunLoop::main().dispatch([configuration = m_configuration, rpId, username, hash, callback = WTFMove(callback)]() mutable {
        ASSERT(configuration.local);
        if (!configuration.local->acceptAttestation) {
            callback(NULL, NULL, [NSError errorWithDomain:NSOSStatusErrorDomain code:-1 userInfo:nil]);
            return;
        }

        // Get Key and add it to Keychain.
        NSDictionary* options = @{
            (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrKeySizeInBits: @256,
        };
        CFErrorRef errorRef = nullptr;
        auto key = adoptCF(SecKeyCreateWithData(
            (__bridge CFDataRef)adoptNS([[NSData alloc] initWithBase64EncodedString:configuration.local->privateKeyBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get(),
            (__bridge CFDictionaryRef)options,
            &errorRef
        ));
        ASSERT(!errorRef);

        // Mock what DeviceIdentity would do.
        String label = makeString(username, "@", rpId, "-rk");
        NSDictionary* addQuery = @{
            (id)kSecValueRef: (id)key.get(),
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrLabel: (id)label,
        };
        OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery, NULL);
        ASSERT_UNUSED(status, !status);

        auto attestationCertificate = adoptCF(SecCertificateCreateWithData(NULL, (__bridge CFDataRef)adoptNS([[NSData alloc] initWithBase64EncodedString:configuration.local->userCertificateBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get()));
        auto attestationIssuingCACertificate = adoptCF(SecCertificateCreateWithData(NULL, (__bridge CFDataRef)adoptNS([[NSData alloc] initWithBase64EncodedString:configuration.local->intermediateCACertificateBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get()));

        callback(key.get(), [NSArray arrayWithObjects: (__bridge id)attestationCertificate.get(), (__bridge id)attestationIssuingCACertificate.get(), nil], NULL);
    });
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
