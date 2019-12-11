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

#import <Security/SecAsn1Coder.h>
#import <Security/SecAsn1Templates.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SSLKeyGenerator.h>
#import <wtf/MainThread.h>
#import <wtf/Scope.h>
#import <wtf/URL.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/Base64.h>

#if USE(APPLE_INTERNAL_SDK)
#include <Security/SecKeyPriv.h>
#else
extern const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5;
#endif

namespace TestWebKitAPI {

struct PublicKeyAndChallenge {
    SecAsn1PubKeyInfo subjectPublicKeyInfo;
    SecAsn1Item challenge;
};

struct SignedPublicKeyAndChallenge {
    PublicKeyAndChallenge publicKeyAndChallenge;
    SecAsn1AlgId algorithmIdentifier;
    SecAsn1Item signature;
};

const SecAsn1Template publicKeyAndChallengeTemplate[] {
    { SEC_ASN1_SEQUENCE, 0, nullptr, sizeof(PublicKeyAndChallenge) },
    { SEC_ASN1_INLINE, offsetof(PublicKeyAndChallenge, subjectPublicKeyInfo), kSecAsn1SubjectPublicKeyInfoTemplate, 0},
    { SEC_ASN1_INLINE, offsetof(PublicKeyAndChallenge, challenge), kSecAsn1IA5StringTemplate, 0 },
    { 0, 0, 0, 0}
};

const SecAsn1Template signedPublicKeyAndChallengeTemplate[] {
    { SEC_ASN1_SEQUENCE, 0, nullptr, sizeof(SignedPublicKeyAndChallenge) },
    { SEC_ASN1_INLINE, offsetof(SignedPublicKeyAndChallenge, publicKeyAndChallenge), publicKeyAndChallengeTemplate, 0 },
    { SEC_ASN1_INLINE, offsetof(SignedPublicKeyAndChallenge, algorithmIdentifier), kSecAsn1AlgorithmIDTemplate, 0 },
    { SEC_ASN1_BIT_STRING, offsetof(SignedPublicKeyAndChallenge, signature), 0, 0 },
    { 0, 0, 0, 0 }
};

const URL url = URL(URL(), "http://www.webkit.org/");

class SSLKeyGeneratorTest : public testing::Test {
public:
    virtual void SetUp()
    {
        WTF::initializeMainThread();
    }

    virtual void TearDown()
    {
        SecItemDelete((__bridge CFDictionaryRef) @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrLabel: WebCore::keygenKeychainItemName(url.host().toString()),
        });
        SecItemDelete((__bridge CFDictionaryRef) @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic,
            (id)kSecAttrLabel: WebCore::keygenKeychainItemName(url.host().toString()),
        });
    }
};

TEST_F(SSLKeyGeneratorTest, DefaultTest)
{
    char challenge[] = "0123456789";
    auto rawResult = WebCore::signedPublicKeyAndChallengeString(0, challenge, url);
    ASSERT_FALSE(rawResult.isEmpty());
    Vector<uint8_t> derResult;
    ASSERT_TRUE(base64Decode(rawResult, derResult));

    SecAsn1CoderRef coder = nullptr;
    ASSERT_EQ(errSecSuccess, SecAsn1CoderCreate(&coder));
    auto releaseCoder = makeScopeExit([&coder] {
        SecAsn1CoderRelease(coder);
    });

    SignedPublicKeyAndChallenge decodedResult { };
    SecAsn1Item derResultItem { derResult.size(), derResult.data() };
    ASSERT_EQ(errSecSuccess, SecAsn1DecodeData(coder, &derResultItem, signedPublicKeyAndChallengeTemplate, &decodedResult));

    // Check challenge
    EXPECT_FALSE(memcmp(challenge, decodedResult.publicKeyAndChallenge.challenge.Data, sizeof(challenge)));

    // Check signature
    RetainPtr<SecKeyRef> publicKey = nullptr;
    {
        NSDictionary* options = @{
            (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic,
            (id)kSecAttrKeySizeInBits: @2048,
        };
        CFErrorRef errorRef = nullptr;
        publicKey = adoptCF(SecKeyCreateWithData(
            adoptCF(CFDataCreate(NULL, decodedResult.publicKeyAndChallenge.subjectPublicKeyInfo.subjectPublicKey.Data, decodedResult.publicKeyAndChallenge.subjectPublicKeyInfo.subjectPublicKey.Length)).get(),
            (__bridge CFDictionaryRef)options,
            &errorRef
        ));
        ASSERT_FALSE(errorRef);
    }

    SecAsn1Item dataToVerify { 0, nullptr };
    ASSERT_EQ(errSecSuccess, SecAsn1EncodeItem(coder, &decodedResult.publicKeyAndChallenge, publicKeyAndChallengeTemplate, &dataToVerify));

    // Signature's Length is in bits, we need it in bytes.
    EXPECT_TRUE(SecKeyVerifySignature(publicKey.get(), kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5, adoptCF(CFDataCreate(NULL, dataToVerify.Data, dataToVerify.Length)).get(), adoptCF(CFDataCreate(NULL, decodedResult.signature.Data, decodedResult.signature.Length / 8)).get(), NULL));

    // Check OIDs
    EXPECT_FALSE(memcmp(oidMd5Rsa.data, decodedResult.algorithmIdentifier.algorithm.Data, oidMd5Rsa.length));
    EXPECT_FALSE(memcmp(oidRsa.data, decodedResult.publicKeyAndChallenge.subjectPublicKeyInfo.algorithm.algorithm.Data, oidRsa.length));

}

} // namespace TestWebKitAPI
