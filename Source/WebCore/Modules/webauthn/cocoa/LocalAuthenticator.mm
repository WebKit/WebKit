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
#import "PublicKeyCredentialRequestOptions.h"
#import <Security/SecItem.h>
#import <pal/crypto/CryptoDigest.h>
#import <pal/spi/cocoa/DeviceIdentitySPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashSet.h>
#import <wtf/MainThread.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/StringHash.h>

#import "LocalAuthenticationSoftLink.h"

namespace WebCore {

namespace LocalAuthenticatorInternal {

// See https://www.w3.org/TR/webauthn/#flags.
const uint8_t makeCredentialFlags = 0b01000101; // UP, UV and AT are set.
const uint8_t getAssertionFlags = 0b00000101; // UP and UV are set.
// FIXME(rdar://problem/38320512): Define Apple AAGUID.
const uint8_t AAGUID[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // 16 bytes
// Credential ID is currently SHA-1 of the corresponding public key.
// FIXME(183534): Assume little endian here.
const union {
    uint16_t integer;
    uint8_t bytes[2];
} credentialIdLength = {0x0014};
const size_t ES256KeySizeInBytes = 32;
const size_t authDataPrefixFixedSize = 37; // hash(32) + flags(1) + counter(4)

#if PLATFORM(IOS)
// https://www.w3.org/TR/webauthn/#sec-authenticator-data
static Vector<uint8_t> buildAuthData(const String& rpId, const uint8_t flags, const uint32_t counter, const Vector<uint8_t>& optionalAttestedCredentialData)
{
    Vector<uint8_t> authData;
    authData.reserveCapacity(authDataPrefixFixedSize + optionalAttestedCredentialData.size());

    // RP ID hash
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    // FIXME(183534): Test IDN.
    ASSERT(rpId.isAllASCII());
    auto asciiRpId = rpId.ascii();
    crypto->addBytes(asciiRpId.data(), asciiRpId.length());
    authData = crypto->computeHash();

    // FLAGS
    authData.append(flags);

    // COUNTER
    // FIXME(183534): Assume little endian here.
    union {
        uint32_t integer;
        uint8_t bytes[4];
    } counterUnion;
    counterUnion.integer = counter;
    authData.append(counterUnion.bytes, sizeof(counterUnion.bytes));

    // ATTESTED CRED. DATA
    authData.appendVector(optionalAttestedCredentialData);

    return authData;
}

inline HashSet<String> produceHashSet(const Vector<PublicKeyCredentialDescriptor>& credentialDescriptors)
{
    HashSet<String> result;
    for (auto& credentialDescriptor : credentialDescriptors) {
        if (credentialDescriptor.transports.isEmpty() && credentialDescriptor.type == PublicKeyCredentialType::PublicKey && credentialDescriptor.idVector.size() == credentialIdLength.integer)
            result.add(String(reinterpret_cast<const char*>(credentialDescriptor.idVector.data()), credentialDescriptor.idVector.size()));
    }
    return result;
}
#endif // !PLATFORM(IOS)

} // LocalAuthenticatorInternal

LocalAuthenticator::LocalAuthenticator()
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));
}

void LocalAuthenticator::makeCredential(const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions& options, CreationCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    using namespace LocalAuthenticatorInternal;

#if !PLATFORM(IOS)
    // FIXME(182772)
    ASSERT_UNUSED(hash, hash == hash);
    ASSERT_UNUSED(options, !options.rp.id.isEmpty());
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
    HashSet<String> excludeCredentialIds = produceHashSet(options.excludeCredentials);
    if (!excludeCredentialIds.isEmpty()) {
        // Search Keychain for the RP ID.
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrLabel: options.rp.id,
            (id)kSecReturnAttributes: @YES,
            (id)kSecMatchLimit: (id)kSecMatchLimitAll,
        };
        CFTypeRef attributesArrayRef = nullptr;
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &attributesArrayRef);
        if (status && status != errSecItemNotFound) {
            LOG_ERROR("Couldn't query Keychain: %d", status);
            exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
            return;
        }
        auto retainAttributesArray = adoptCF(attributesArrayRef);

        for (NSDictionary *nsAttributes in (NSArray *)attributesArrayRef) {
            NSData *nsCredentialId = nsAttributes[(id)kSecAttrApplicationLabel];
            if (excludeCredentialIds.contains(String(reinterpret_cast<const char*>(nsCredentialId.bytes), nsCredentialId.length))) {
                exceptionCallback({ NotAllowedError, ASCIILiteral("At least one credential matches an entry of the excludeCredentials list in the platform attached authenticator.") });
                return;
            }
        }
    }

    // Step 6.
    // FIXME(rdar://problem/35900593): Update to a formal UI.
    // Get user consent.
    auto context = adoptNS([allocLAContextInstance() init]);
    NSError *error = nil;
    if (![context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:&error]) {
        LOG_ERROR("Couldn't evaluate authentication with biometrics policy: %@", error);
        // FIXME(182767)
        exceptionCallback({ NotAllowedError, ASCIILiteral("No avaliable authenticators.") });
        return;
    }

    NSString *reason = [NSString stringWithFormat:@"Allow %@ to create a public key credential for %@", (id)options.rp.id, (id)options.user.name];
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

            // Step 7.2-7.4.
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
                CFTypeRef attributesRef = nullptr;
                OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)credentialIdQuery, &attributesRef);
                if (status) {
                    LOG_ERROR("Couldn't get Credential ID: %d", status);
                    exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
                    return;
                }
                auto retainAttributes = adoptCF(attributesRef);

                NSDictionary *nsAttributes = (NSDictionary *)attributesRef;
                NSData *nsCredentialId = nsAttributes[(id)kSecAttrApplicationLabel];
                credentialId.append(reinterpret_cast<const uint8_t*>(nsCredentialId.bytes), nsCredentialId.length);

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

            // Step 10.
            // FIXME(183533): store the counter.
            uint32_t counter = 0;

            // FIXME(183534): attestedCredentialData could throttle.
            // Step 11. https://www.w3.org/TR/webauthn/#attested-credential-data
            Vector<uint8_t> attestedCredentialData;
            {
                // aaguid
                attestedCredentialData.append(AAGUID, sizeof(AAGUID));

                // credentialIdLength
                ASSERT(credentialId.size() == credentialIdLength.integer);
                // FIXME(183534): Assume little endian here.
                attestedCredentialData.append(credentialIdLength.bytes, sizeof(uint16_t));

                // credentialId
                attestedCredentialData.appendVector(credentialId);

                // credentialPublicKey
                CFDataRef publicKeyDataRef = nullptr;
                {
                    auto publicKey = adoptCF(SecKeyCopyPublicKey(privateKey));
                    CFErrorRef errorRef = nullptr;
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
                attestedCredentialData.appendVector(cosePublicKey.value());
            }

            // Step 12.
            auto authData = buildAuthData(options.rp.id, makeCredentialFlags, counter, attestedCredentialData);

            // Step 13. Apple Attestation Cont'
            // Assemble the attestation object:
            // https://www.w3.org/TR/webauthn/#attestation-object
            cbor::CBORValue::MapValue attestationStatementMap;
            {
                Vector<uint8_t> signature;
                {
                    CFErrorRef errorRef = nullptr;
                    // FIXME(183652): Reduce prompt for biometrics
                    CFDataRef signatureRef = SecKeyCreateSignature(privateKey, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)[NSData dataWithBytes:authData.data() length:authData.size()], &errorRef);
                    auto retainError = adoptCF(errorRef);
                    if (errorRef) {
                        LOG_ERROR("Couldn't generate the signature: %@", (NSError*)errorRef);
                        exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
                        return;
                    }
                    auto retainSignature = adoptCF(signatureRef);
                    NSData *nsSignature = (NSData *)signatureRef;
                    signature.append(reinterpret_cast<const uint8_t*>(nsSignature.bytes), nsSignature.length);
                }
                attestationStatementMap[cbor::CBORValue("alg")] = cbor::CBORValue(COSE::ES256);
                attestationStatementMap[cbor::CBORValue("sig")] = cbor::CBORValue(signature);
                Vector<cbor::CBORValue> cborArray;
                for (size_t i = 0; i < [certificates count]; i++) {
                    CFDataRef dataRef = SecCertificateCopyData((__bridge SecCertificateRef)certificates[i]);
                    auto retainData = adoptCF(dataRef);
                    NSData *nsData = (NSData *)dataRef;
                    Vector<uint8_t> data;
                    data.append(reinterpret_cast<const uint8_t*>(nsData.bytes), nsData.length);
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

void LocalAuthenticator::getAssertion(const Vector<uint8_t>& hash, const PublicKeyCredentialRequestOptions& options, RequestCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    using namespace LocalAuthenticatorInternal;

#if !PLATFORM(IOS)
    // FIXME(182772)
    ASSERT_UNUSED(hash, hash == hash);
    ASSERT_UNUSED(options, !options.rpId.isEmpty());
    ASSERT_UNUSED(callback, callback);
    exceptionCallback({ NotAllowedError, ASCIILiteral("No avaliable authenticators.") });
#else
    // The following implements https://www.w3.org/TR/webauthn/#op-get-assertion as of 5 December 2017.
    // Skip Step 2 as requireUserVerification is enforced.
    // Skip Step 8 as extensions are not supported yet.
    // Step 12 is implicitly captured by all UnknownError exception callbacks.
    // Step 3-5. Unlike the spec, if an allow list is provided and there is no intersection between existing ones and the allow list, we always return NotAllowedError.
    HashSet<String> allowCredentialIds = produceHashSet(options.allowCredentials);
    if (!options.allowCredentials.isEmpty() && allowCredentialIds.isEmpty()) {
        exceptionCallback({ NotAllowedError, ASCIILiteral("No matched credentials are found in the platform attached authenticator.") });
        return;
    }

    // Search Keychain for the RP ID.
    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrLabel: options.rpId,
        (id)kSecReturnAttributes: @YES,
        (id)kSecMatchLimit: (id)kSecMatchLimitAll,
    };
    CFTypeRef attributesArrayRef = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &attributesArrayRef);
    if (status && status != errSecItemNotFound) {
        LOG_ERROR("Couldn't query Keychain: %d", status);
        exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
        return;
    }
    auto retainAttributesArray = adoptCF(attributesArrayRef);

    NSArray *intersectedCredentialsAttributes = nil;
    if (options.allowCredentials.isEmpty())
        intersectedCredentialsAttributes = (NSArray *)attributesArrayRef;
    else {
        NSMutableArray *result = [NSMutableArray arrayWithCapacity:allowCredentialIds.size()];
        for (NSDictionary *nsAttributes in (NSArray *)attributesArrayRef) {
            NSData *nsCredentialId = nsAttributes[(id)kSecAttrApplicationLabel];
            if (allowCredentialIds.contains(String(reinterpret_cast<const char*>(nsCredentialId.bytes), nsCredentialId.length)))
                [result addObject:nsAttributes];
        }
        intersectedCredentialsAttributes = result;
    }
    if (!intersectedCredentialsAttributes.count) {
        exceptionCallback({ NotAllowedError, ASCIILiteral("No matched credentials are found in the platform attached authenticator.") });
        return;
    }

    // Step 6.
    // FIXME(rdar://problem/35900534): We don't have an UI to prompt users for selecting intersectedCredentials, and therefore we always use the first one for now.
    NSDictionary *selectedCredentialAttributes = intersectedCredentialsAttributes[0];

    // Step 7. Get user consent.
    // FIXME(183534): The lifetime of context is managed by reply and the early return, which is a bit subtle.
    LAContext *context = [allocLAContextInstance() init];
    NSError *error = nil;
    if (![context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:&error]) {
        auto retainContext = adoptNS(context);
        LOG_ERROR("Couldn't evaluate authentication with biometrics policy: %@", error);
        // FIXME(182767)
        exceptionCallback({ NotAllowedError, ASCIILiteral("No avaliable authenticators.") });
        return;
    }

    Vector<uint8_t> credentialId;
    NSData *nsCredentialId = selectedCredentialAttributes[(id)kSecAttrApplicationLabel];
    credentialId.append(reinterpret_cast<const uint8_t*>(nsCredentialId.bytes), nsCredentialId.length);
    Vector<uint8_t> userhandle;
    NSData *nsUserhandle = selectedCredentialAttributes[(id)kSecAttrApplicationTag];
    userhandle.append(reinterpret_cast<const uint8_t*>(nsUserhandle.bytes), nsUserhandle.length);
    auto reply = BlockPtr<void(BOOL, NSError *)>::fromCallable([callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), rpId = options.rpId.isolatedCopy(), hash, credentialId = WTFMove(credentialId), userhandle = WTFMove(userhandle), context = adoptNS(context)] (BOOL success, NSError *error) mutable {
        ASSERT(!isMainThread());
        if (!success || error) {
            LOG_ERROR("Couldn't authenticate with biometrics: %@", error);
            exceptionCallback({ NotAllowedError, ASCIILiteral("Couldn't get user consent.") });
            return;
        }

        // Step 9-10.
        // FIXME(183533): Due to the stated Keychain limitations, we can't save the counter value.
        // Therefore, it is always zero.
        uint32_t counter = 0;
        auto authData = buildAuthData(rpId, getAssertionFlags, counter, { });

        // Step 11.
        Vector<uint8_t> signature;
        {
            NSDictionary *query = @{
                (id)kSecClass: (id)kSecClassKey,
                (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
                (id)kSecAttrApplicationLabel: [NSData dataWithBytes:credentialId.data() length:credentialId.size()],
                (id)kSecUseAuthenticationContext: context.get(),
                (id)kSecReturnRef: @YES,
            };
            CFTypeRef privateKeyRef = nullptr;
            OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &privateKeyRef);
            if (status) {
                LOG_ERROR("Couldn't get the private key reference: %d", status);
                exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
                return;
            }
            auto privateKey = adoptCF(privateKeyRef);

            NSMutableData *dataToSign = [NSMutableData dataWithBytes:authData.data() length:authData.size()];
            [dataToSign appendBytes:hash.data() length:hash.size()];

            CFErrorRef errorRef = nullptr;
            // FIXME: Converting CFTypeRef to SecKeyRef is quite subtle here.
            CFDataRef signatureRef = SecKeyCreateSignature((__bridge SecKeyRef)((id)privateKeyRef), kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)dataToSign, &errorRef);
            auto retainError = adoptCF(errorRef);
            if (errorRef) {
                LOG_ERROR("Couldn't generate the signature: %@", (NSError*)errorRef);
                exceptionCallback({ UnknownError, ASCIILiteral("Unknown internal error.") });
                return;
            }
            auto retainSignature = adoptCF(signatureRef);
            NSData *nsSignature = (NSData *)signatureRef;
            signature.append(reinterpret_cast<const uint8_t*>(nsSignature.bytes), nsSignature.length);
        }

        // Step 13.
        callback(credentialId, authData, signature, userhandle);
    });

    // FIXME(183533): Use userhandle instead of username due to the stated Keychain limitations.
    NSString *reason = [NSString stringWithFormat:@"Log into %@ with %@.", (id)options.rpId, selectedCredentialAttributes[(id)kSecAttrApplicationTag]];
    [context evaluateAccessControl:(__bridge SecAccessControlRef)selectedCredentialAttributes[(id)kSecAttrAccessControl] operation:LAAccessControlOperationUseKeySign localizedReason:reason reply:reply.get()];
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
        CFErrorRef errorRef = nullptr;
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
