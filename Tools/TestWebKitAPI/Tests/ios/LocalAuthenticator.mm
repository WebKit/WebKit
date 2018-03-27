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

#if ENABLE(WEB_AUTHN)

#if PLATFORM(IOS)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/SecItem.h>
#import <WebCore/CBORReader.h>
#import <WebCore/COSEConstants.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/LocalAuthenticator.h>
#import <WebCore/PublicKeyCredentialCreationOptions.h>
#import <WebCore/PublicKeyCredentialRequestOptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/text/Base64.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

const String testAttestationCertificateBase64 = String() +
    "MIIB6jCCAZCgAwIBAgIGAWHAxcjvMAoGCCqGSM49BAMCMFMxJzAlBgNVBAMMHkJh" +
    "c2ljIEF0dGVzdGF0aW9uIFVzZXIgU3ViIENBMTETMBEGA1UECgwKQXBwbGUgSW5j" +
    "LjETMBEGA1UECAwKQ2FsaWZvcm5pYTAeFw0xODAyMjMwMzM3MjJaFw0xODAyMjQw" +
    "MzQ3MjJaMGoxIjAgBgNVBAMMGTAwMDA4MDEwLTAwMEE0OUEyMzBBMDIxM0ExGjAY" +
    "BgNVBAsMEUJBQSBDZXJ0aWZpY2F0aW9uMRMwEQYDVQQKDApBcHBsZSBJbmMuMRMw" +
    "EQYDVQQIDApDYWxpZm9ybmlhMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEvCje" +
    "Pzr6Sg76XMoHuGabPaG6zjpLFL8Zd8/74Hh5PcL2Zq+o+f7ENXX+7nEXXYt0S8Ux" +
    "5TIRw4hgbfxXQbWLEqM5MDcwDAYDVR0TAQH/BAIwADAOBgNVHQ8BAf8EBAMCBPAw" +
    "FwYJKoZIhvdjZAgCBAowCKEGBAR0ZXN0MAoGCCqGSM49BAMCA0gAMEUCIAlK8A8I" +
    "k43TbvKuYGHZs1DTgpTwmKTBvIUw5bwgZuYnAiEAtuJjDLKbGNJAJFMi5deEBqno" +
    "pBTCqbfbDJccfyQpjnY=";
const String testAttestationIssuingCACertificateBase64 = String() +
    "MIICIzCCAaigAwIBAgIIeNjhG9tnDGgwCgYIKoZIzj0EAwIwUzEnMCUGA1UEAwwe" +
    "QmFzaWMgQXR0ZXN0YXRpb24gVXNlciBSb290IENBMRMwEQYDVQQKDApBcHBsZSBJ" +
    "bmMuMRMwEQYDVQQIDApDYWxpZm9ybmlhMB4XDTE3MDQyMDAwNDIwMFoXDTMyMDMy" +
    "MjAwMDAwMFowUzEnMCUGA1UEAwweQmFzaWMgQXR0ZXN0YXRpb24gVXNlciBTdWIg" +
    "Q0ExMRMwEQYDVQQKDApBcHBsZSBJbmMuMRMwEQYDVQQIDApDYWxpZm9ybmlhMFkw" +
    "EwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEoSZ/1t9eBAEVp5a8PrXacmbGb8zNC1X3" +
    "StLI9YO6Y0CL7blHmSGmjGWTwD4Q+i0J2BY3+bPHTGRyA9jGB3MSbaNmMGQwEgYD" +
    "VR0TAQH/BAgwBgEB/wIBADAfBgNVHSMEGDAWgBSD5aMhnrB0w/lhkP2XTiMQdqSj" +
    "8jAdBgNVHQ4EFgQU5mWf1DYLTXUdQ9xmOH/uqeNSD80wDgYDVR0PAQH/BAQDAgEG" +
    "MAoGCCqGSM49BAMCA2kAMGYCMQC3M360LLtJS60Z9q3vVjJxMgMcFQ1roGTUcKqv" +
    "W+4hJ4CeJjySXTgq6IEHn/yWab4CMQCm5NnK6SOSK+AqWum9lL87W3E6AA1f2TvJ" +
    "/hgok/34jr93nhS87tOQNdxDS8zyiqw=";
const String testCredentialIdBase64 = "SMSXHngF7hEOsElA73C3RY+8bR4=";
const String testES256PrivateKeyBase64 = String() +
    "BDj/zxSkzKgaBuS3cdWDF558of8AaIpgFpsjF/Qm1749VBJPgqUIwfhWHJ91nb7U" +
    "PH76c0+WFOzZKslPyyFse4goGIW2R7k9VHLPEZl5nfnBgEVFh5zev+/xpHQIvuq6" +
    "RQ==";
const String testRpId = "localhost";
const String testUsername = "username";
const uint8_t testUserhandle[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x9};

RetainPtr<SecKeyRef> getTestKey()
{
    NSDictionary* options = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrKeySizeInBits: @256,
    };
    CFErrorRef errorRef = nullptr;
    auto key = adoptCF(SecKeyCreateWithData(
        (__bridge CFDataRef)adoptNS([[NSData alloc] initWithBase64EncodedString:testES256PrivateKeyBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get(),
        (__bridge CFDictionaryRef)options,
        &errorRef
    ));
    EXPECT_FALSE(errorRef);

    return key;
}

void addTestKeyToKeychain()
{
    auto key = getTestKey();
    NSDictionary* addQuery = @{
        (id)kSecValueRef: (id)key.get(),
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: testRpId,
        (id)kSecAttrApplicationTag: [NSData dataWithBytes:testUserhandle length:sizeof(testUserhandle)],
    };
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery, NULL);
    EXPECT_FALSE(status);
}

void cleanUpKeychain()
{
    // Cleanup the keychain.
    NSDictionary* deleteQuery = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: testRpId,
    };
    SecItemDelete((__bridge CFDictionaryRef)deleteQuery);
}

class LACantEvaluatePolicySwizzler {
public:
    LACantEvaluatePolicySwizzler()
        : m_swizzler([LAContext class], @selector(canEvaluatePolicy:error:), reinterpret_cast<IMP>(cantEvaluatePolicy))
    {
    }
private:
    static BOOL cantEvaluatePolicy()
    {
        return NO;
    }
    InstanceMethodSwizzler m_swizzler;
};

class LACanEvaluatePolicySwizzler {
public:
    LACanEvaluatePolicySwizzler()
        : m_swizzler([LAContext class], @selector(canEvaluatePolicy:error:), reinterpret_cast<IMP>(canEvaluatePolicy))
    {
    }
private:
    static BOOL canEvaluatePolicy()
    {
        return YES;
    }
    InstanceMethodSwizzler m_swizzler;
};

class LAEvaluatePolicyFailedSwizzler {
public:
    LAEvaluatePolicyFailedSwizzler()
        : m_swizzler([LAContext class], @selector(evaluatePolicy:localizedReason:reply:), reinterpret_cast<IMP>(evaluatePolicyFailed))
    {
    }
private:
    static void evaluatePolicyFailed(id, SEL, LAPolicy, NSString *, void (^reply)(BOOL, NSError *))
    {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            Util::sleep(1); // mimic user interaction delay
            reply(NO, nil);
        });
    }
    InstanceMethodSwizzler m_swizzler;
};

class LAEvaluatePolicyPassedSwizzler {
public:
    LAEvaluatePolicyPassedSwizzler()
        : m_swizzler([LAContext class], @selector(evaluatePolicy:localizedReason:reply:), reinterpret_cast<IMP>(evaluatePolicyPassed))
    {
    }
private:
    static void evaluatePolicyPassed(id, SEL, LAPolicy, NSString *, void (^reply)(BOOL, NSError *))
    {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            Util::sleep(1); // mimic user interaction delay
            reply(YES, nil);
        });
    }
    InstanceMethodSwizzler m_swizzler;
};

class LAEvaluateAccessControlFailedSwizzler {
public:
    LAEvaluateAccessControlFailedSwizzler()
        : m_swizzler([LAContext class], @selector(evaluateAccessControl:operation:localizedReason:reply:), reinterpret_cast<IMP>(evaluateAccessControlFailed))
    {
    }
private:
    static void evaluateAccessControlFailed(id, SEL, SecAccessControlRef, LAAccessControlOperation, NSString *, void (^reply)(BOOL, NSError *))
    {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            Util::sleep(1); // mimic user interaction delay
            reply(NO, nil);
        });
    }
    InstanceMethodSwizzler m_swizzler;
};

class LAEvaluateAccessControlPassedSwizzler {
public:
    LAEvaluateAccessControlPassedSwizzler()
        : m_swizzler([LAContext class], @selector(evaluateAccessControl:operation:localizedReason:reply:), reinterpret_cast<IMP>(evaluateAccessControlPassed))
    {
    }
private:
    static void evaluateAccessControlPassed(id, SEL, SecAccessControlRef, LAAccessControlOperation, NSString *, void (^reply)(BOOL, NSError *))
    {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            Util::sleep(1); // mimic user interaction delay
            reply(YES, nil);
        });
    }
    InstanceMethodSwizzler m_swizzler;
};

class TestLocalAuthenticator : public WebCore::LocalAuthenticator {
public:
    void setFailureFlag() { m_failureFlag = true; }

protected:
    void issueClientCertificate(const String& rpId, const String& username, const Vector<uint8_t>& hash, WebCore::CompletionBlock _Nonnull completion) const final
    {
        if (m_failureFlag) {
            completion(NULL, NULL, [NSError errorWithDomain:NSOSStatusErrorDomain code:-1 userInfo:nil]);
            return;
        }

        ASSERT_EQ(32ul, hash.size());
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), BlockPtr<void()>::fromCallable([rpId = rpId.isolatedCopy(), username = username.isolatedCopy(), completion = makeBlockPtr(completion)] {
            Util::sleep(1); // mimic network delay

            // Get Key and add it to Keychain
            auto key = getTestKey();
            String label(username);
            label.append("@" + rpId + "-rk"); // mimic what DeviceIdentity would do.
            NSDictionary* addQuery = @{
                (id)kSecValueRef: (id)key.get(),
                (id)kSecClass: (id)kSecClassKey,
                (id)kSecAttrLabel: (id)label,
            };
            OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery, NULL);
            ASSERT_FALSE(status);

            // Construct dummy certificates
            auto attestationCertificate = adoptCF(SecCertificateCreateWithData(NULL, (__bridge CFDataRef)adoptNS([[NSData alloc] initWithBase64EncodedString:testAttestationCertificateBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get()));
            auto attestationIssuingCACertificate = adoptCF(SecCertificateCreateWithData(NULL, (__bridge CFDataRef)adoptNS([[NSData alloc] initWithBase64EncodedString:testAttestationIssuingCACertificateBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get()));

            // Do self-attestation instead.
            completion(key.get(), [NSArray arrayWithObjects: (__bridge id)attestationCertificate.get(), (__bridge id)attestationIssuingCACertificate.get(), nil], NULL);
        }).get());
    }

private:
    bool m_failureFlag { false };
};

// FIXME(182769): Convert the followings to proper API tests.
TEST(LocalAuthenticator, MakeCredentialNotSupportedPubKeyCredParams)
{
    WebCore::PublicKeyCredentialCreationOptions creationOptions;
    creationOptions.pubKeyCredParams.append({ WebCore::PublicKeyCredentialType::PublicKey, -35 }); // ES384
    creationOptions.pubKeyCredParams.append({ WebCore::PublicKeyCredentialType::PublicKey, -257 }); // RS256

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    auto callback = [&done] (const Vector<uint8_t>&, const Vector<uint8_t>&) {
        EXPECT_FALSE(true);
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData& exception) mutable {
        EXPECT_EQ(WebCore::NotSupportedError, exception.code);
        EXPECT_STREQ("The platform attached authenticator doesn't support any provided PublicKeyCredentialParameters.", exception.message.ascii().data());
        done = true;
    };
    authenticator->makeCredential({ }, creationOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

TEST(LocalAuthenticator, MakeCredentialExcludeCredentialsMatch)
{
    addTestKeyToKeychain();

    WebCore::PublicKeyCredentialDescriptor descriptor;
    descriptor.type = WebCore::PublicKeyCredentialType::PublicKey;
    WTF::base64Decode(testCredentialIdBase64, descriptor.idVector);
    WebCore::PublicKeyCredentialCreationOptions creationOptions;
    creationOptions.rp.id = testRpId;
    creationOptions.pubKeyCredParams.append({ WebCore::PublicKeyCredentialType::PublicKey, COSE::ES256 });
    creationOptions.excludeCredentials.append(WTFMove(descriptor));

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    auto callback = [&done] (const Vector<uint8_t>&, const Vector<uint8_t>&) {
        EXPECT_FALSE(true);
        cleanUpKeychain();
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData& exception) mutable {
        EXPECT_EQ(WebCore::NotAllowedError, exception.code);
        EXPECT_STREQ("At least one credential matches an entry of the excludeCredentials list in the platform attached authenticator.", exception.message.ascii().data());
        cleanUpKeychain();
        done = true;
    };
    authenticator->makeCredential({ }, creationOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

TEST(LocalAuthenticator, MakeCredentialBiometricsNotEnrolled)
{
    LACantEvaluatePolicySwizzler swizzler;

    WebCore::PublicKeyCredentialCreationOptions creationOptions;
    creationOptions.pubKeyCredParams.append({ WebCore::PublicKeyCredentialType::PublicKey, COSE::ES256 });

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    auto callback = [&done] (const Vector<uint8_t>&, const Vector<uint8_t>&) {
        EXPECT_FALSE(true);
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData& exception) mutable {
        EXPECT_EQ(WebCore::NotAllowedError, exception.code);
        EXPECT_STREQ("No avaliable authenticators.", exception.message.ascii().data());
        done = true;
    };
    authenticator->makeCredential({ }, creationOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

TEST(LocalAuthenticator, MakeCredentialBiometricsNotAuthenticated)
{
    LACanEvaluatePolicySwizzler canEvaluatePolicySwizzler;
    LAEvaluatePolicyFailedSwizzler evaluatePolicyFailedSwizzler;

    WebCore::PublicKeyCredentialCreationOptions creationOptions;
    creationOptions.pubKeyCredParams.append({ WebCore::PublicKeyCredentialType::PublicKey, COSE::ES256 });

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    auto callback = [&done] (const Vector<uint8_t>&, const Vector<uint8_t>&) {
        EXPECT_FALSE(true);
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData& exception) mutable {
        EXPECT_EQ(WebCore::NotAllowedError, exception.code);
        EXPECT_STREQ("Couldn't get user consent.", exception.message.ascii().data());
        done = true;
    };
    authenticator->makeCredential({ }, creationOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

TEST(LocalAuthenticator, MakeCredentialNotAttestated)
{
    LACanEvaluatePolicySwizzler canEvaluatePolicySwizzler;
    LAEvaluatePolicyPassedSwizzler evaluatePolicyPassedSwizzler;

    WebCore::PublicKeyCredentialCreationOptions creationOptions;
    creationOptions.pubKeyCredParams.append({ WebCore::PublicKeyCredentialType::PublicKey, COSE::ES256 });

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    authenticator->setFailureFlag();
    auto callback = [&done] (const Vector<uint8_t>&, const Vector<uint8_t>&) {
        EXPECT_FALSE(true);
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData& exception) mutable {
        EXPECT_EQ(WebCore::UnknownError, exception.code);
        EXPECT_STREQ("Unknown internal error.", exception.message.ascii().data());
        done = true;
    };
    authenticator->makeCredential({ }, creationOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

TEST(LocalAuthenticator, MakeCredentialDeleteOlderCredenital)
{
    LACanEvaluatePolicySwizzler canEvaluatePolicySwizzler;
    LAEvaluatePolicyPassedSwizzler evaluatePolicyPassedSwizzler;

    // Insert the older credential
    addTestKeyToKeychain();

    WebCore::PublicKeyCredentialCreationOptions creationOptions;
    creationOptions.rp.id = testRpId;
    creationOptions.user.name = testUsername;
    creationOptions.user.idVector.append(testUserhandle, sizeof(testUserhandle));
    creationOptions.pubKeyCredParams.append({ WebCore::PublicKeyCredentialType::PublicKey, COSE::ES256 });

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    authenticator->setFailureFlag();
    auto callback = [&done] (const Vector<uint8_t>&, const Vector<uint8_t>&) {
        EXPECT_FALSE(true);
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData&) mutable {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrLabel: testRpId,
            (id)kSecAttrApplicationTag: [NSData dataWithBytes:testUserhandle length:sizeof(testUserhandle)],
        };
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL);
        EXPECT_EQ(errSecItemNotFound, status);
        done = true;
    };
    authenticator->makeCredential({ }, creationOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

TEST(LocalAuthenticator, MakeCredentialPassedWithSelfAttestation)
{
    LACanEvaluatePolicySwizzler canEvaluatePolicySwizzler;
    LAEvaluatePolicyPassedSwizzler evaluatePolicyPassedSwizzler;

    WebCore::PublicKeyCredentialCreationOptions creationOptions;
    creationOptions.rp.id = testRpId;
    creationOptions.user.name = testUsername;
    creationOptions.user.idVector.append(testUserhandle, sizeof(testUserhandle));
    creationOptions.pubKeyCredParams.append({ WebCore::PublicKeyCredentialType::PublicKey, COSE::ES256 });

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    auto callback = [&done] (const Vector<uint8_t>& credentialId, const Vector<uint8_t>& attestationObjet) {
        // Check Keychain
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrLabel: testRpId,
            (id)kSecAttrApplicationLabel: adoptNS([[NSData alloc] initWithBase64EncodedString:testCredentialIdBase64 options:NSDataBase64DecodingIgnoreUnknownCharacters]).get(),
            (id)kSecAttrApplicationTag: [NSData dataWithBytes:testUserhandle length:sizeof(testUserhandle)],
        };
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL);
        EXPECT_FALSE(status);

        // Check Credential ID
        EXPECT_TRUE(WTF::base64Encode(credentialId.data(), credentialId.size()) == testCredentialIdBase64);

        // Check Attestation Object
        auto attestationObjectMap = cbor::CBORReader::read(attestationObjet);
        ASSERT_TRUE(!!attestationObjectMap);

        // Check Authenticator Data.
        auto& authData = attestationObjectMap->getMap().find(cbor::CBORValue("authData"))->second.getByteString();
        size_t pos = 0;
        uint8_t expectedRpIdHash[] = {
            0x49, 0x96, 0x0d, 0xe5, 0x88, 0x0e, 0x8c, 0x68,
            0x74, 0x34, 0x17, 0x0f, 0x64, 0x76, 0x60, 0x5b,
            0x8f, 0xe4, 0xae, 0xb9, 0xa2, 0x86, 0x32, 0xc7,
            0x99, 0x5c, 0xf3, 0xba, 0x83, 0x1d, 0x97, 0x63
        };
        EXPECT_FALSE(memcmp(authData.data() + pos, expectedRpIdHash, sizeof(expectedRpIdHash)));
        pos += sizeof(expectedRpIdHash);

        // FLAGS
        EXPECT_EQ(69, authData[pos]);
        pos++;

        uint32_t counter = -1;
        memcpy(&counter, authData.data() + pos, sizeof(uint32_t));
        EXPECT_EQ(0u, counter);
        pos += sizeof(uint32_t);

        uint8_t expectedAAGUID[] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        EXPECT_FALSE(memcmp(authData.data() + pos, expectedAAGUID, sizeof(expectedAAGUID)));
        pos += sizeof(expectedAAGUID);

        uint16_t l = -1;
        memcpy(&l, authData.data() + pos, sizeof(uint16_t));
        EXPECT_EQ(20u, l);
        pos += sizeof(uint16_t);

        EXPECT_FALSE(memcmp(authData.data() + pos, credentialId.data(), l));
        pos += l;

        // Credential Public Key
        // FIXME(183536): The CBOR reader doesn't support negative integer as map key. Thus we couldn't utilzie it.
        EXPECT_STREQ("pQECAyYgASFYIDj/zxSkzKgaBuS3cdWDF558of8AaIpgFpsjF/Qm1749IlggVBJPgqUIwfhWHJ91nb7UPH76c0+WFOzZKslPyyFse4g=", WTF::base64Encode(authData.data() + pos, authData.size() - pos).ascii().data());

        // Check Self Attestation
        EXPECT_STREQ("Apple", attestationObjectMap->getMap().find(cbor::CBORValue("fmt"))->second.getString().ascii().data());

        auto& attStmt = attestationObjectMap->getMap().find(cbor::CBORValue("attStmt"))->second.getMap();
        EXPECT_EQ(COSE::ES256, attStmt.find(cbor::CBORValue("alg"))->second.getNegative());

        auto& sig = attStmt.find(cbor::CBORValue("sig"))->second.getByteString();
        auto privateKey = getTestKey();
        EXPECT_TRUE(SecKeyVerifySignature(SecKeyCopyPublicKey(privateKey.get()), kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)[NSData dataWithBytes:authData.data() length:authData.size()], (__bridge CFDataRef)[NSData dataWithBytes:sig.data() length:sig.size()], NULL));

        // Check certificates
        auto& x5c = attStmt.find(cbor::CBORValue("x5c"))->second.getArray();
        auto& attestationCertificateData = x5c[0].getByteString();
        auto attestationCertificate = adoptCF(SecCertificateCreateWithData(NULL, (__bridge CFDataRef)[NSData dataWithBytes:attestationCertificateData.data() length:attestationCertificateData.size()]));
        CFStringRef commonName = nullptr;
        status = SecCertificateCopyCommonName(attestationCertificate.get(), &commonName);
        auto retainCommonName = adoptCF(commonName);
        ASSERT(!status);
        EXPECT_STREQ("00008010-000A49A230A0213A", [(NSString *)commonName cStringUsingEncoding: NSASCIIStringEncoding]);

        auto& attestationIssuingCACertificateData = x5c[1].getByteString();
        auto attestationIssuingCACertificate = adoptCF(SecCertificateCreateWithData(NULL, (__bridge CFDataRef)[NSData dataWithBytes:attestationIssuingCACertificateData.data() length:attestationIssuingCACertificateData.size()]));
        commonName = nullptr;
        status = SecCertificateCopyCommonName(attestationIssuingCACertificate.get(), &commonName);
        retainCommonName = adoptCF(commonName);
        ASSERT(!status);
        EXPECT_STREQ("Basic Attestation User Sub CA1", [(NSString *)commonName cStringUsingEncoding: NSASCIIStringEncoding]);

        cleanUpKeychain();
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData&) mutable {
        EXPECT_FALSE(true);
        cleanUpKeychain();
        done = true;
    };
    Vector<uint8_t> hash(32);
    authenticator->makeCredential(hash, creationOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

TEST(LocalAuthenticator, GetAssertionAllowCredentialsMismatch1)
{
    // Transports mismatched
    WebCore::PublicKeyCredentialDescriptor descriptor;
    descriptor.type = WebCore::PublicKeyCredentialType::PublicKey;
    descriptor.transports.append(WebCore::PublicKeyCredentialDescriptor::AuthenticatorTransport::Usb);
    WebCore::PublicKeyCredentialRequestOptions requestOptions;
    requestOptions.allowCredentials.append(WTFMove(descriptor));

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    auto callback = [&done] (const Vector<uint8_t>&, const Vector<uint8_t>&, const Vector<uint8_t>&, const Vector<uint8_t>&) {
        EXPECT_FALSE(true);
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData& exception) mutable {
        EXPECT_EQ(WebCore::NotAllowedError, exception.code);
        EXPECT_STREQ("No matched credentials are found in the platform attached authenticator.", exception.message.ascii().data());
        done = true;
    };
    authenticator->getAssertion({ }, requestOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

TEST(LocalAuthenticator, GetAssertionAllowCredentialsMismatch2)
{
    // No existing credential
    WebCore::PublicKeyCredentialRequestOptions requestOptions;
    requestOptions.rpId = testRpId;

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    auto callback = [&done] (const Vector<uint8_t>&, const Vector<uint8_t>&, const Vector<uint8_t>&, const Vector<uint8_t>&) {
        EXPECT_FALSE(true);
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData& exception) mutable {
        EXPECT_EQ(WebCore::NotAllowedError, exception.code);
        EXPECT_STREQ("No matched credentials are found in the platform attached authenticator.", exception.message.ascii().data());
        done = true;
    };
    authenticator->getAssertion({ }, requestOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

TEST(LocalAuthenticator, GetAssertionAllowCredentialsMismatch3)
{
    // Credential ID mismatched
    addTestKeyToKeychain();

    WebCore::PublicKeyCredentialDescriptor descriptor;
    descriptor.type = WebCore::PublicKeyCredentialType::PublicKey;
    WTF::base64Decode(testCredentialIdBase64, descriptor.idVector);
    descriptor.idVector[19] = 0; // nuke the last byte.
    WebCore::PublicKeyCredentialRequestOptions requestOptions;
    requestOptions.rpId = testRpId;
    requestOptions.allowCredentials.append(descriptor);

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    auto callback = [&done] (const Vector<uint8_t>&, const Vector<uint8_t>&, const Vector<uint8_t>&, const Vector<uint8_t>&) {
        EXPECT_FALSE(true);
        cleanUpKeychain();
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData& exception) mutable {
        EXPECT_EQ(WebCore::NotAllowedError, exception.code);
        EXPECT_STREQ("No matched credentials are found in the platform attached authenticator.", exception.message.ascii().data());
        cleanUpKeychain();
        done = true;
    };
    authenticator->getAssertion({ }, requestOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

TEST(LocalAuthenticator, GetAssertionBiometricsNotEnrolled)
{
    LACantEvaluatePolicySwizzler swizzler;

    addTestKeyToKeychain();

    WebCore::PublicKeyCredentialRequestOptions requestOptions;
    requestOptions.rpId = testRpId;

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    auto callback = [&done] (const Vector<uint8_t>&, const Vector<uint8_t>&, const Vector<uint8_t>&, const Vector<uint8_t>&) {
        EXPECT_FALSE(true);
        cleanUpKeychain();
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData& exception) mutable {
        EXPECT_EQ(WebCore::NotAllowedError, exception.code);
        EXPECT_STREQ("No avaliable authenticators.", exception.message.ascii().data());
        cleanUpKeychain();
        done = true;
    };
    authenticator->getAssertion({ }, requestOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

TEST(LocalAuthenticator, GetAssertionBiometricsNotAuthenticated)
{
    LACanEvaluatePolicySwizzler canEvaluatePolicySwizzler;
    LAEvaluateAccessControlFailedSwizzler evaluateAccessControlFailedSwizzler;

    addTestKeyToKeychain();

    WebCore::PublicKeyCredentialRequestOptions requestOptions;
    requestOptions.rpId = testRpId;

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    auto callback = [&done] (const Vector<uint8_t>&, const Vector<uint8_t>&, const Vector<uint8_t>&, const Vector<uint8_t>&) {
        EXPECT_FALSE(true);
        cleanUpKeychain();
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData& exception) mutable {
        EXPECT_EQ(WebCore::NotAllowedError, exception.code);
        EXPECT_STREQ("Couldn't get user consent.", exception.message.ascii().data());
        cleanUpKeychain();
        done = true;
    };
    authenticator->getAssertion({ }, requestOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

TEST(LocalAuthenticator, GetAssertionPassed)
{
    LACanEvaluatePolicySwizzler canEvaluatePolicySwizzler;
    LAEvaluateAccessControlPassedSwizzler evaluateAccessControlPassedSwizzler;

    addTestKeyToKeychain();

    WebCore::PublicKeyCredentialRequestOptions requestOptions;
    requestOptions.rpId = testRpId;

    Vector<uint8_t> hash(32);

    bool done = false;
    std::unique_ptr<TestLocalAuthenticator> authenticator = std::make_unique<TestLocalAuthenticator>();
    auto callback = [&done, hash] (const Vector<uint8_t>& credentialId, const Vector<uint8_t>& authData, const Vector<uint8_t>& signature, const Vector<uint8_t>& userhandle) {
        // Check Credential ID
        EXPECT_TRUE(WTF::base64Encode(credentialId.data(), credentialId.size()) == testCredentialIdBase64);

        // Check Authenticator Data.
        size_t pos = 0;
        uint8_t expectedRpIdHash[] = {
            0x49, 0x96, 0x0d, 0xe5, 0x88, 0x0e, 0x8c, 0x68,
            0x74, 0x34, 0x17, 0x0f, 0x64, 0x76, 0x60, 0x5b,
            0x8f, 0xe4, 0xae, 0xb9, 0xa2, 0x86, 0x32, 0xc7,
            0x99, 0x5c, 0xf3, 0xba, 0x83, 0x1d, 0x97, 0x63
        };
        EXPECT_FALSE(memcmp(authData.data() + pos, expectedRpIdHash, sizeof(expectedRpIdHash)));
        pos += sizeof(expectedRpIdHash);

        // FLAGS
        EXPECT_EQ(5, authData[pos]);
        pos++;

        uint32_t counter = -1;
        memcpy(&counter, authData.data() + pos, sizeof(uint32_t));
        EXPECT_EQ(0u, counter);

        // Check signature
        auto privateKey = getTestKey();
        Vector<uint8_t> dataToSign(authData);
        dataToSign.appendVector(hash);
        EXPECT_TRUE(SecKeyVerifySignature(SecKeyCopyPublicKey(privateKey.get()), kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)[NSData dataWithBytes:dataToSign.data() length:dataToSign.size()], (__bridge CFDataRef)[NSData dataWithBytes:signature.data() length:signature.size()], NULL));

        // Check User Handle
        EXPECT_EQ(userhandle.size(), sizeof(testUserhandle));
        EXPECT_FALSE(memcmp(userhandle.data(), testUserhandle, sizeof(testUserhandle)));

        cleanUpKeychain();
        done = true;
    };
    auto exceptionCallback = [&done] (const WebCore::ExceptionData& exception) mutable {
        EXPECT_FALSE(true);
        cleanUpKeychain();
        done = true;
    };
    authenticator->getAssertion(hash, requestOptions, WTFMove(callback), WTFMove(exceptionCallback));

    TestWebKitAPI::Util::run(&done);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS)

#endif // ENABLE(WEB_AUTHN)
