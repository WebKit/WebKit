/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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


#include "config.h"
#include "VirtualAuthenticatorUtils.h"

#if ENABLE(WEB_AUTHN)

#include <WebCore/WebAuthenticationConstants.h>
#include <WebCore/WebAuthenticationUtils.h>
#include <pal/crypto/CryptoDigest.h>
#include <wtf/cocoa/TypeCastsCocoa.h>
#include <wtf/cocoa/VectorCocoa.h>

namespace WebKit {
using namespace WebCore;

uint8_t flagsForConfig(const VirtualAuthenticatorConfiguration& config)
{
    uint8_t flags = WebAuthn::attestedCredentialDataIncludedFlag;
    if (config.isUserConsenting)
        flags = flags | WebAuthn::userPresenceFlag;
    if (config.isUserVerified)
        flags = flags | WebAuthn::userVerifiedFlag;
    return flags;
}

RetainPtr<SecKeyRef> createPrivateKey()
{
    NSDictionary* options = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrKeySizeInBits: @256,
    };
    CFErrorRef errorRef = nullptr;
    auto key = adoptCF(SecKeyCreateRandomKey(
        (__bridge CFDictionaryRef)options,
        &errorRef
    ));
    if (errorRef)
        return nullptr;
    return key;
}

std::pair<Vector<uint8_t>, Vector<uint8_t>> credentialIdAndCosePubKeyForPrivateKey(RetainPtr<SecKeyRef> privateKey)
{
    RetainPtr<CFDataRef> publicKeyDataRef;
    {
        auto publicKey = adoptCF(SecKeyCopyPublicKey(privateKey.get()));
        CFErrorRef errorRef = nullptr;
        publicKeyDataRef = adoptCF(SecKeyCopyExternalRepresentation(publicKey.get(), &errorRef));
        auto retainError = adoptCF(errorRef);
        ASSERT(!errorRef);
        ASSERT(((NSData *)publicKeyDataRef.get()).length == (1 + 2 * ES256FieldElementLength)); // 04 | X | Y
    }
    NSData *nsPublicKeyData = (NSData *)publicKeyDataRef.get();

    Vector<uint8_t> credentialId;
    {
        auto digest = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_1);
        digest->addBytes(nsPublicKeyData.bytes, nsPublicKeyData.length);
        credentialId = digest->computeHash();
    }

    Vector<uint8_t> cosePublicKey;
    {
        // COSE Encoding
        Vector<uint8_t> x(ES256FieldElementLength);
        [nsPublicKeyData getBytes: x.data() range:NSMakeRange(1, ES256FieldElementLength)];
        Vector<uint8_t> y(ES256FieldElementLength);
        [nsPublicKeyData getBytes: y.data() range:NSMakeRange(1 + ES256FieldElementLength, ES256FieldElementLength)];
        cosePublicKey = encodeES256PublicKeyAsCBOR(WTFMove(x), WTFMove(y));
    }
    return std::pair { credentialId, cosePublicKey };
}

String base64PrivateKey(RetainPtr<SecKeyRef> privateKey)
{
    CFErrorRef errorRef = nullptr;
    auto privateKeyRep = adoptCF(SecKeyCopyExternalRepresentation((__bridge SecKeyRef)((id)privateKey.get()), &errorRef));
    auto retainError = adoptCF(errorRef);
    if (errorRef) {
        ASSERT_NOT_REACHED();
        return emptyString();
    }
    NSData *nsPrivateKeyRep = (NSData *)privateKeyRep.get();

    return String([nsPrivateKeyRep base64EncodedStringWithOptions:0]);
}

RetainPtr<SecKeyRef> privateKeyFromBase64(const String& base64PrivateKey)
{
    NSDictionary* options = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrKeySizeInBits: @256,
    };
    RetainPtr<NSData> privateKey = adoptNS([[NSData alloc] initWithBase64EncodedString:base64PrivateKey options:0]);
    CFErrorRef errorRef = nullptr;
    auto key = adoptCF(SecKeyCreateWithData(
        bridge_cast(privateKey.get()),
        bridge_cast(options),
        &errorRef
    ));
    ASSERT(!errorRef);
    return key;
}

Vector<uint8_t> signatureForPrivateKey(RetainPtr<SecKeyRef> privateKey, const Vector<uint8_t>& authData, const Vector<uint8_t>& clientDataHash)
{
    NSMutableData *dataToSign = [NSMutableData dataWithBytes:authData.data() length:authData.size()];
    [dataToSign appendBytes:clientDataHash.data() length:clientDataHash.size()];
    RetainPtr<CFDataRef> signature;
    {
        CFErrorRef errorRef = nullptr;
        signature = adoptCF(SecKeyCreateSignature((__bridge SecKeyRef)((id)privateKey.get()), kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)dataToSign, &errorRef));
        auto retainError = adoptCF(errorRef);
        ASSERT(!errorRef);
    }

    return vectorFromNSData((NSData *)signature.get());
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
