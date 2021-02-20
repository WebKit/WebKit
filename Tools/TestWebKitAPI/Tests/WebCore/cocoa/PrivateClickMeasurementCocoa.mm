/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "Test.h"

#if HAVE(RSA_BSSA)

#include <Security/SecKeyPriv.h>
#include <WebCore/PrivateClickMeasurement.h>

extern "C" {
#include <corecrypto/cc_priv.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccrsa.h>
#include <corecrypto/ccrsabssa.h>
}

using namespace WebCore;

namespace TestWebKitAPI {

TEST(PrivateClickMeasurement, ValidBlindedSecret)
{
    auto ephemeralNonce = PrivateClickMeasurement::EphemeralSourceNonce { "ABCDEFabcdef0123456789"_s };
    EXPECT_TRUE(ephemeralNonce.isValid());

    PrivateClickMeasurement pcm;
    pcm.setEphemeralSourceNonce(WTFMove(ephemeralNonce));

    // Generate the server key pair.
    size_t modulusNBits = 4096;
    int error = 0;

    struct ccrng_state* rng = ccrng(&error);
    EXPECT_EQ(error, CCERR_OK);

    const uint8_t e[] = { 0x1, 0x00, 0x01 };
    ccrsa_full_ctx_decl(ccn_sizeof(modulusNBits), rsaPrivateKey);
    error = ccrsa_generate_key(modulusNBits, rsaPrivateKey, sizeof(e), e, rng);
    EXPECT_EQ(error, CCERR_OK);

    ccrsa_pub_ctx_t rsaPublicKey = ccrsa_ctx_public(rsaPrivateKey);
    size_t modulusNBytes = cc_ceiling(ccrsa_pubkeylength(rsaPublicKey), 8);

    const struct ccrsabssa_ciphersuite *ciphersuite = &ccrsabssa_ciphersuite_rsa4096_sha384;

    size_t exportSize = ccder_encode_rsa_pub_size(rsaPublicKey);
    auto publicKey = adoptNS([[NSMutableData alloc] initWithLength:exportSize]);
    ccder_encode_rsa_pub(rsaPublicKey, static_cast<uint8_t*>([publicKey mutableBytes]), static_cast<uint8_t*>([publicKey mutableBytes]) + [publicKey length]);

    auto secKey = adoptCF(SecKeyCreateWithData((__bridge CFDataRef)publicKey.get(), (__bridge CFDictionaryRef)@{
        (__bridge id)kSecAttrKeyType: (__bridge id)kSecAttrKeyTypeRSA,
        (__bridge id)kSecAttrKeyClass: (__bridge id)kSecAttrKeyClassPublic
    }, nil));
    EXPECT_NOT_NULL(secKey);

    auto spkiData = adoptCF(SecKeyCopySubjectPublicKeyInfo(secKey.get()));
    auto *nsSpkiData = (__bridge NSData *)spkiData.get();

    // Continue the test.
    EXPECT_TRUE(pcm.calculateAndUpdateSourceSecretToken(WTF::base64URLEncode(nsSpkiData.bytes, nsSpkiData.length)));
    auto sourceSecretToken = pcm.tokenSignatureJSON();
    EXPECT_EQ(sourceSecretToken->asObject()->size(), 4ul);
    EXPECT_STREQ(sourceSecretToken->getString("source_engagement_type"_s).utf8().data(), "click");
    EXPECT_STREQ(sourceSecretToken->getString("source_nonce"_s).utf8().data(), "ABCDEFabcdef0123456789");
    EXPECT_FALSE(sourceSecretToken->getString("source_secret_token"_s).isEmpty());
    EXPECT_EQ(sourceSecretToken->getInteger("version"_s), 2);

    // Generate the signature.
    Vector<uint8_t> blindedMessage;
    base64URLDecode(sourceSecretToken->getString("source_secret_token"_s), blindedMessage);

    auto blindedSignature = adoptNS([[NSMutableData alloc] initWithLength: modulusNBytes]);
    ccrsabssa_sign_blinded_message(ciphersuite, rsaPrivateKey, blindedMessage.data(), blindedMessage.size(), static_cast<uint8_t *>([blindedSignature mutableBytes]), [blindedSignature length], rng);

    // Continue the test.
    EXPECT_TRUE(pcm.calculateAndUpdateSourceUnlinkableToken(WTF::base64URLEncode([blindedSignature bytes], [blindedSignature length])));
    auto& persistentToken = pcm.sourceUnlinkableToken();
    EXPECT_TRUE(persistentToken);
    EXPECT_FALSE(persistentToken->tokenBase64URL.isEmpty());
    EXPECT_FALSE(persistentToken->keyIDBase64URL.isEmpty());
    EXPECT_FALSE(persistentToken->signatureBase64URL.isEmpty());
}

} // namespace TestWebKitAPI

#endif // EHAVE(RSA_BSSA)
