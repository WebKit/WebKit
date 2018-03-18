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
#import "LocalAuthenticator.h"

#if ENABLE(WEB_AUTHN)

#import "CBORWriter.h"
#import "COSEConstants.h"
#import "ExceptionData.h"
#import "PublicKeyCredentialCreationOptions.h"
#import <Security/SecItem.h>
#import <pal/crypto/CryptoDigest.h>
#import <pal/spi/cocoa/DeviceIdentitySPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#import "LocalAuthenticationSoftLink.h"

namespace WebCore {

namespace LocalAuthenticatorInternal {

// UP, UV and AT are set. See https://www.w3.org/TR/webauthn/#flags.
const uint8_t authenticatorDataFlags = 69;
// FIXME(rdar://problem/38320512): Define Apple AAGUID.
const uint8_t AAGUID[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // 16 bytes
const size_t maxCredentialIdLength = 0xffff; // 2 bytes
const size_t ES256KeySizeInBytes = 32;

} // LocalAuthenticatorInternal

void LocalAuthenticator::makeCredential(const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions& options, CreationCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    using namespace LocalAuthenticatorInternal;

#if !PLATFORM(IOS)
    // FIXME(182772)
    ASSERT_UNUSED(hash, hash == hash);
    ASSERT_UNUSED(options, options.rp.name.isEmpty());
    ASSERT_UNUSED(callback, callback);
    exceptionCallback({ NotAllowedError, ASCIILiteral("No avaliable authenticators.") });
#else
    // The following implements https://www.w3.org/TR/webauthn/#op-make-cred as of 5 December 2017.
    // Skip Step 4-5 as requireResidentKey and requireUserVerification are enforced.
    // Skip Step 9 as extensions are not supported yet.
    // Step 8 is implicitly captured by all UnknownError exception callbacks.
    // Step 2.
    bool canFullfillPubKeyCredParams = false;
    for (auto& pubKeyCredParam : options.pubKeyCredParams) {
        if (pubKeyCredParam.type == PublicKeyCredentialType::PublicKey && pubKeyCredParam.alg == COSE::ES256) {
            canFullfillPubKeyCredParams = true;
            break;
        }
    }
    if (!canFullfillPubKeyCredParams) {
        exceptionCallback({ NotSupportedError, ASCIILiteral("The platform attached authenticator doesn't support any provided PublicKeyCredentialParameters.") });
        return;
    }

    // Step 3.
    for (auto& excludeCredential : options.excludeCredentials) {
        if (excludeCredential.type == PublicKeyCredentialType::PublicKey && excludeCredential.transports.isEmpty()) {
            // Search Keychain for the Credential ID and RP ID, which is stored in the kSecAttrApplicationLabel and kSecAttrLabel attribute respectively.
            NSDictionary *query = @{
                (id)kSecClass: (id)kSecClassKey,
                (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
                (id)kSecAttrApplicationLabel: [NSData dataWithBytes:excludeCredential.idVector.data() length:excludeCredential.idVector.size()],
                (id)kSecAttrLabel: options.rp.id
            };
            OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL);
            if (!status) {
                exceptionCallback({ NotAllowedError, ASCIILiteral("At least one credential matches an entry of the excludeCredentials list in the platform attached authenticator.") });
                return;
            }
            if (status != errSecItemNotFound) {
                LOG_ERROR("Couldn't query Keychain: %d", status);
                exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
                return;
            }
        }
    }

    // Step 6.
    // FIXME(rdar://problem/35900593): Update to a formal UI.
    // Get user consent.
    auto context = adoptNS([allocLAContextInstance() init]);
    NSError *error = nil;
    NSString *reason = [NSString stringWithFormat:@"Allow %@ to create a public key credential for %@", (id)options.rp.id, (id)options.user.name];
    if (![context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:&error]) {
        LOG_ERROR("Couldn't evaluate authentication with biometrics policy: %@", error);
        // FIXME(182767)
        exceptionCallback({ NotAllowedError, ASCIILiteral("No avaliable authenticators.") });
        return;
    }

    // FIXME(183534): Optimize the following nested callbacks and threading.
    [context evaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics localizedReason:reason reply:BlockPtr<void(BOOL, NSError *)>::fromCallable([weakThis = m_weakFactory.createWeakPtr(*this), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), options = crossThreadCopy(options), hash] (BOOL success, NSError *error) mutable {
        ASSERT(!isMainThread());
        if (!success || error) {
            LOG_ERROR("Couldn't authenticate with biometrics: %@", error);
            exceptionCallback({ NotAllowedError, ASCIILiteral("Couldn't get user consent.") });
            return;
        }

        // Step 7.5.
        // Userhandle is stored in kSecAttrApplicationTag attribute.
        // Failures after this point could block users' accounts forever. Should we follow the spec?
        NSDictionary* deleteQuery = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrLabel: options.rp.id,
            (id)kSecAttrApplicationTag: [NSData dataWithBytes:options.user.idVector.data() length:options.user.idVector.size()],
        };
        OSStatus status = SecItemDelete((__bridge CFDictionaryRef)deleteQuery);
        if (status && status != errSecItemNotFound) {
            LOG_ERROR("Couldn't detele older credential: %d", status);
            exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
            return;
        }

        // Step 7.1, 13. Apple Attestation
        // FIXME(183534)
        if (!weakThis)
            return;
        weakThis->issueClientCertificate(options.rp.id, options.user.name, hash, BlockPtr<void(SecKeyRef, NSArray *, NSError *)>::fromCallable([callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), options = crossThreadCopy(options)] (_Nullable SecKeyRef privateKey, NSArray * _Nullable certificates, NSError * _Nullable error) {
            ASSERT(!isMainThread());
            if (error) {
                LOG_ERROR("Couldn't attest: %@", error);
                exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
                return;
            }
            // Attestation Certificate and Attestation Issuing CA
            ASSERT(certificates && ([certificates count] == 2));

            // Step 7.2 - 7.4.
            // FIXME(183533): A single kSecClassKey item couldn't store all meta data. The following schema is a tentative solution
            // to accommodate the most important meta data, i.e. RP ID, Credential ID, and userhandle.
            // kSecAttrLabel: RP ID
            // kSecAttrApplicationLabel: Credential ID (auto-gen by Keychain)
            // kSecAttrApplicationTag: userhandle
            // Noted, the current DeviceIdentity.Framework would only allow us to pass the kSecAttrLabel as the inital attribute
            // for the Keychain item. Since that's the only clue we have to locate the unique item, we use the pattern username@rp.id
            // as the initial value.
            // Also noted, the vale of kSecAttrApplicationLabel is automatically generated by the Keychain, which is a SHA-1 hash of
            // the public key. We borrow it directly for now to workaround the stated limitations.
            // Update the Keychain item to the above schema.
            // FIXME(183533): DeviceIdentity.Framework would insert certificates into Keychain as well. We should update those as well.
            Vector<uint8_t> credentialId;
            {
                String label(options.user.name);
                label.append("@" + options.rp.id + "-rk"); // -rk is added by DeviceIdentity.Framework.
                NSDictionary *credentialIdQuery = @{
                    (id)kSecClass: (id)kSecClassKey,
                    (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
                    (id)kSecAttrLabel: label,
                    (id)kSecReturnAttributes: @YES
                };
                CFTypeRef attributesRef = NULL;
                OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)credentialIdQuery, &attributesRef);
                if (status) {
                    LOG_ERROR("Couldn't get Credential ID: %d", status);
                    exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
                    return;
                }
                auto retainAttributes = adoptCF(attributesRef);

                NSDictionary *nsAttributes = (NSDictionary *)attributesRef;
                NSData *nsCredentialId = nsAttributes[(id)kSecAttrApplicationLabel];
                credentialId.append(static_cast<const uint8_t*>(nsCredentialId.bytes), nsCredentialId.length);

                NSDictionary *updateQuery = @{
                    (id)kSecClass: (id)kSecClassKey,
                    (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
                    (id)kSecAttrApplicationLabel: nsCredentialId,
                };
                NSDictionary *updateParams = @{
                    (id)kSecAttrLabel: options.rp.id,
                    (id)kSecAttrApplicationTag: [NSData dataWithBytes:options.user.idVector.data() length:options.user.idVector.size()],
                };
                status = SecItemUpdate((__bridge CFDictionaryRef)updateQuery, (__bridge CFDictionaryRef)updateParams);
                if (status) {
                    LOG_ERROR("Couldn't update the Keychain item: %d", status);
                    exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
                    return;
                }
            }

            // Step 12.
            // Apple Attestation Cont'
            // Assemble the attestation object:
            // https://www.w3.org/TR/webauthn/#attestation-object
            // FIXME(183534): authData could throttle.
            Vector<uint8_t> authData;
            {
                // RP ID hash
                auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
                // FIXME(183534): Test IDN.
                ASSERT(options.rp.id.isAllASCII());
                auto asciiRpId = options.rp.id.ascii();
                crypto->addBytes(asciiRpId.data(), asciiRpId.length());
                authData = crypto->computeHash();

                // FLAGS
                authData.append(authenticatorDataFlags);

                // Step. 10.
                // COUNTER
                // FIXME(183533): store the counter.
                union {
                    uint32_t integer;
                    uint8_t bytes[4];
                } counter = {0x00000000};
                authData.append(counter.bytes, sizeof(counter.bytes));

                // Step 11.
                // AAGUID
                authData.append(AAGUID, sizeof(AAGUID));

                // L
                ASSERT(credentialId.size() <= maxCredentialIdLength);
                // Assume little endian here.
                union {
                    uint16_t integer;
                    uint8_t bytes[2];
                } credentialIdLength;
                credentialIdLength.integer = static_cast<uint16_t>(credentialId.size());
                authData.append(credentialIdLength.bytes, sizeof(uint16_t));

                // CREDENTIAL ID
                authData.appendVector(credentialId);

                // CREDENTIAL PUBLIC KEY
                CFDataRef publicKeyDataRef = NULL;
                {
                    auto publicKey = adoptCF(SecKeyCopyPublicKey(privateKey));
                    CFErrorRef errorRef = NULL;
                    publicKeyDataRef = SecKeyCopyExternalRepresentation(publicKey.get(), &errorRef);
                    auto retainError = adoptCF(errorRef);
                    if (errorRef) {
                        LOG_ERROR("Couldn't export the public key: %@", (NSError*)errorRef);
                        exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
                        return;
                    }
                    ASSERT(((NSData *)publicKeyDataRef).length == (1 + 2*ES256KeySizeInBytes)); // 04 | X | Y
                }
                auto retainPublicKeyData = adoptCF(publicKeyDataRef);

                // COSE Encoding
                // FIXME(183535): Improve CBOR encoder to work with bytes directly.
                Vector<uint8_t> x(ES256KeySizeInBytes);
                [(NSData *)publicKeyDataRef getBytes: x.data() range:NSMakeRange(1, ES256KeySizeInBytes)];
                Vector<uint8_t> y(ES256KeySizeInBytes);
                [(NSData *)publicKeyDataRef getBytes: y.data() range:NSMakeRange(1 + ES256KeySizeInBytes, ES256KeySizeInBytes)];
                cbor::CBORValue::MapValue publicKeyMap;
                publicKeyMap[cbor::CBORValue(COSE::kty)] = cbor::CBORValue(COSE::EC2);
                publicKeyMap[cbor::CBORValue(COSE::alg)] = cbor::CBORValue(COSE::ES256);
                publicKeyMap[cbor::CBORValue(COSE::crv)] = cbor::CBORValue(COSE::P_256);
                publicKeyMap[cbor::CBORValue(COSE::x)] = cbor::CBORValue(WTFMove(x));
                publicKeyMap[cbor::CBORValue(COSE::y)] = cbor::CBORValue(WTFMove(y));
                auto cosePublicKey = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(publicKeyMap)));
                if (!cosePublicKey) {
                    LOG_ERROR("Couldn't encode the public key into COSE binaries.");
                    exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
                    return;
                }
                authData.appendVector(cosePublicKey.value());
            }

            cbor::CBORValue::MapValue attestationStatementMap;
            {
                Vector<uint8_t> signature;
                {
                    CFErrorRef errorRef = NULL;
                    // FIXME(183652): Reduce prompt for biometrics
                    CFDataRef signatureRef = SecKeyCreateSignature(privateKey, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)[NSData dataWithBytes:authData.data() length:authData.size()], &errorRef);
                    auto retainError = adoptCF(errorRef);
                    if (errorRef) {
                        LOG_ERROR("Couldn't export the public key: %@", (NSError*)errorRef);
                        exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
                        return;
                    }
                    auto retainSignature = adoptCF(signatureRef);
                    NSData *nsSignature = (NSData *)signatureRef;
                    signature.append(static_cast<const uint8_t*>(nsSignature.bytes), nsSignature.length);
                }
                attestationStatementMap[cbor::CBORValue("alg")] = cbor::CBORValue(COSE::ES256);
                attestationStatementMap[cbor::CBORValue("sig")] = cbor::CBORValue(signature);
                Vector<cbor::CBORValue> cborArray;
                for (size_t i = 0; i < [certificates count]; i++) {
                    CFDataRef dataRef = SecCertificateCopyData((__bridge SecCertificateRef)certificates[i]);
                    auto retainData = adoptCF(dataRef);
                    NSData *nsData = (NSData *)dataRef;
                    Vector<uint8_t> data;
                    data.append(static_cast<const uint8_t*>(nsData.bytes), nsData.length);
                    cborArray.append(cbor::CBORValue(WTFMove(data)));
                }
                attestationStatementMap[cbor::CBORValue("x5c")] = cbor::CBORValue(WTFMove(cborArray));
            }

            cbor::CBORValue::MapValue attestationObjectMap;
            attestationObjectMap[cbor::CBORValue("authData")] = cbor::CBORValue(authData);
            attestationObjectMap[cbor::CBORValue("fmt")] = cbor::CBORValue("Apple");
            attestationObjectMap[cbor::CBORValue("attStmt")] = cbor::CBORValue(WTFMove(attestationStatementMap));
            auto attestationObject = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(attestationObjectMap)));
            if (!attestationObject) {
                LOG_ERROR("Couldn't encode the attestation object.");
                exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
                return;
            }

            callback(credentialId, attestationObject.value());
        }).get());
    }).get()];
#endif // !PLATFORM(IOS)
}

bool LocalAuthenticator::isAvailable() const
{
#if !PLATFORM(IOS)
    // FIXME(182772)
    return false;
#else
    auto context = adoptNS([allocLAContextInstance() init]);
    NSError *error = nil;

    if (![context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:&error]) {
        LOG_ERROR("Couldn't evaluate authentication with biometrics policy: %@", error);
        return true;
    }
    return true;
#endif // !PLATFORM(IOS)
}

void LocalAuthenticator::issueClientCertificate(const String& rpId, const String& username, const Vector<uint8_t>& hash, CompletionBlock _Nonnull completionHandler) const
{
// DeviceIdentity.Framework is not avaliable in iOS simulator.
#if !PLATFORM(IOS) || PLATFORM(IOS_SIMULATOR)
    // FIXME(182772)
    ASSERT_UNUSED(rpId, !rpId.isEmpty());
    ASSERT_UNUSED(username, !username.isEmpty());
    ASSERT_UNUSED(hash, !hash.isEmpty());
    completionHandler(NULL, NULL, [NSError errorWithDomain:@"com.apple.WebKit.WebAuthN" code:1 userInfo:nil]);
#else
    // Apple Attestation
    ASSERT(hash.size() <= 32);

    SecAccessControlRef accessControlRef;
    {
        CFErrorRef errorRef = NULL;
        accessControlRef = SecAccessControlCreateWithFlags(NULL, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, kSecAccessControlPrivateKeyUsage | kSecAccessControlUserPresence, &errorRef);
        auto retainError = adoptCF(errorRef);
        if (errorRef) {
            LOG_ERROR("Couldn't create ACL: %@", (NSError *)errorRef);
            completionHandler(NULL, NULL, [NSError errorWithDomain:@"com.apple.WebKit.WebAuthN" code:1 userInfo:nil]);
            return;
        }
    }
    auto retainAccessControl = adoptCF(accessControlRef);

    String label(username);
    label.append("@" + rpId);
    NSDictionary *options = @{
        kMAOptionsBAAKeychainLabel: label,
        // FIXME(rdar://problem/38489134): Need a formal name.
        kMAOptionsBAAKeychainAccessGroup: @"com.apple.safari.WebAuthN.credentials",
        kMAOptionsBAAIgnoreExistingKeychainItems: @(YES),
        // FIXME(rdar://problem/38489134): Determine a proper lifespan.
        kMAOptionsBAAValidity: @(1440), // Last one day.
        kMAOptionsBAASCRTAttestation: @(NO),
        kMAOptionsBAANonce: [NSData dataWithBytes:hash.data() length:hash.size()],
        kMAOptionsBAAAccessControls: (id)accessControlRef,
        kMAOptionsBAAOIDSToInclude: @[kMAOptionsBAAOIDNonce]
    };

    // FIXME(183652): Reduce prompt for biometrics
    DeviceIdentityIssueClientCertificateWithCompletion(NULL, options, completionHandler);
#endif
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
