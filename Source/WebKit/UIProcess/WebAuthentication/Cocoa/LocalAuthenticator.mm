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

#include "config.h"
#include "LocalAuthenticator.h"

#if ENABLE(WEB_AUTHN)

#import <Security/SecItem.h>
#import <WebCore/CBORWriter.h>
#import <WebCore/COSEConstants.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/PublicKeyCredentialCreationOptions.h>
#import <WebCore/PublicKeyCredentialData.h>
#import <WebCore/PublicKeyCredentialRequestOptions.h>
#import <pal/crypto/CryptoDigest.h>
#import <wtf/HashSet.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>
#import <wtf/text/StringHash.h>

namespace WebKit {
using namespace WebCore;

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

#if PLATFORM(IOS_FAMILY)
// https://www.w3.org/TR/webauthn/#sec-authenticator-data
static Vector<uint8_t> buildAuthData(const String& rpId, const uint8_t flags, uint32_t counter, const Vector<uint8_t>& optionalAttestedCredentialData)
{
    Vector<uint8_t> authData;
    authData.reserveInitialCapacity(authDataPrefixFixedSize + optionalAttestedCredentialData.size());

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

static inline bool emptyTransportsOrContain(const Vector<AuthenticatorTransport>& transports, AuthenticatorTransport target)
{
    return transports.isEmpty() ? true : transports.contains(target);
}

static inline HashSet<String> produceHashSet(const Vector<PublicKeyCredentialDescriptor>& credentialDescriptors)
{
    HashSet<String> result;
    for (auto& credentialDescriptor : credentialDescriptors) {
        if (emptyTransportsOrContain(credentialDescriptor.transports, AuthenticatorTransport::Internal)
            && credentialDescriptor.type == PublicKeyCredentialType::PublicKey
            && credentialDescriptor.idVector.size() == credentialIdLength.integer)
            result.add(String(reinterpret_cast<const char*>(credentialDescriptor.idVector.data()), credentialDescriptor.idVector.size()));
    }
    return result;
}

static inline Vector<uint8_t> toVector(NSData *data)
{
    Vector<uint8_t> result;
    result.append(reinterpret_cast<const uint8_t*>(data.bytes), data.length);
    return result;
}
#endif // !PLATFORM(IOS_FAMILY)

} // LocalAuthenticatorInternal

LocalAuthenticator::LocalAuthenticator(UniqueRef<LocalConnection>&& connection)
    : m_connection(WTFMove(connection))
{
}

void LocalAuthenticator::makeCredential()
{
    // FIXME(182772)
    using namespace LocalAuthenticatorInternal;
    ASSERT(m_state == State::Init);
    m_state = State::RequestReceived;

#if PLATFORM(IOS_FAMILY)
    // The following implements https://www.w3.org/TR/webauthn/#op-make-cred as of 5 December 2017.
    // Skip Step 4-5 as requireResidentKey and requireUserVerification are enforced.
    // Skip Step 9 as extensions are not supported yet.
    // Step 8 is implicitly captured by all UnknownError exception receiveResponds.
    // Step 2.
    bool canFullfillPubKeyCredParams = false;
    for (auto& pubKeyCredParam : requestData().creationOptions.pubKeyCredParams) {
        if (pubKeyCredParam.type == PublicKeyCredentialType::PublicKey && pubKeyCredParam.alg == COSE::ES256) {
            canFullfillPubKeyCredParams = true;
            break;
        }
    }
    if (!canFullfillPubKeyCredParams) {
        receiveRespond(ExceptionData { NotSupportedError, "The platform attached authenticator doesn't support any provided PublicKeyCredentialParameters."_s });
        return;
    }

    // Step 3.
    HashSet<String> excludeCredentialIds = produceHashSet(requestData().creationOptions.excludeCredentials);
    if (!excludeCredentialIds.isEmpty()) {
        // Search Keychain for the RP ID.
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrLabel: requestData().creationOptions.rp.id,
            (id)kSecReturnAttributes: @YES,
            (id)kSecMatchLimit: (id)kSecMatchLimitAll,
        };
        CFTypeRef attributesArrayRef = nullptr;
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &attributesArrayRef);
        if (status && status != errSecItemNotFound) {
            LOG_ERROR("Couldn't query Keychain: %d", status);
            receiveRespond(ExceptionData { UnknownError, "Unknown internal error."_s });
            return;
        }
        auto retainAttributesArray = adoptCF(attributesArrayRef);

        for (NSDictionary *nsAttributes in (NSArray *)attributesArrayRef) {
            NSData *nsCredentialId = nsAttributes[(id)kSecAttrApplicationLabel];
            if (excludeCredentialIds.contains(String(reinterpret_cast<const char*>(nsCredentialId.bytes), nsCredentialId.length))) {
                receiveRespond(ExceptionData { NotAllowedError, "At least one credential matches an entry of the excludeCredentials list in the platform attached authenticator."_s });
                return;
            }
        }
    }

    // Step 6.
    // FIXME(rdar://problem/35900593): Update to a formal UI.
    // Get user consent.
    auto callback = [weakThis = makeWeakPtr(*this)](LocalConnection::UserConsent consent) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;

        weakThis->continueMakeCredentialAfterUserConsented(consent);
    };
    m_connection->getUserConsent(
        "Allow " + requestData().creationOptions.rp.id + " to create a public key credential for " + requestData().creationOptions.user.name,
        WTFMove(callback));
#endif // !PLATFORM(IOS_FAMILY)
}

void LocalAuthenticator::continueMakeCredentialAfterUserConsented(LocalConnection::UserConsent consent)
{
    // FIXME(182772)
    ASSERT(m_state == State::RequestReceived);
    m_state = State::UserConsented;

#if PLATFORM(IOS_FAMILY)
    if (consent == LocalConnection::UserConsent::No) {
        receiveRespond(ExceptionData { NotAllowedError, "Couldn't get user consent."_s });
        return;
    }

    // Step 7.5.
    // Userhandle is stored in kSecAttrApplicationTag attribute.
    // Failures after this point could block users' accounts forever. Should we follow the spec?
    NSDictionary* deleteQuery = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: requestData().creationOptions.rp.id,
        (id)kSecAttrApplicationTag: [NSData dataWithBytes:requestData().creationOptions.user.idVector.data() length:requestData().creationOptions.user.idVector.size()],
    };
    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)deleteQuery);
    if (status && status != errSecItemNotFound) {
        LOG_ERROR("Couldn't detele older credential: %d", status);
        receiveRespond(ExceptionData { UnknownError, "Unknown internal error."_s });
        return;
    }

    // Step 7.1, 13. Apple Attestation
    auto callback = [weakThis = makeWeakPtr(*this)](SecKeyRef _Nullable privateKey, NSArray * _Nullable certificates, NSError * _Nullable error) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueMakeCredentialAfterAttested(privateKey, certificates, error);
    };
    m_connection->getAttestation(requestData().creationOptions.rp.id, requestData().creationOptions.user.name, requestData().hash, WTFMove(callback));
#endif // !PLATFORM(IOS_FAMILY)
}

void LocalAuthenticator::continueMakeCredentialAfterAttested(SecKeyRef privateKey, NSArray *certificates, NSError *error)
{
    // FIXME(182772)
    using namespace LocalAuthenticatorInternal;

    ASSERT(m_state == State::UserConsented);
    m_state = State::Attested;

#if PLATFORM(IOS_FAMILY)
    if (error) {
        LOG_ERROR("Couldn't attest: %@", error);
        receiveRespond(ExceptionData { UnknownError, "Unknown internal error."_s });
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
        // -rk is added by DeviceIdentity.Framework.
        String label = makeString(requestData().creationOptions.user.name, "@", requestData().creationOptions.rp.id, "-rk");
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
            receiveRespond(ExceptionData { UnknownError, "Unknown internal error."_s });
            return;
        }
        auto retainAttributes = adoptCF(attributesRef);

        NSDictionary *nsAttributes = (NSDictionary *)attributesRef;
        credentialId = toVector(nsAttributes[(id)kSecAttrApplicationLabel]);

        NSDictionary *updateQuery = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrApplicationLabel: nsAttributes[(id)kSecAttrApplicationLabel],
        };
        NSDictionary *updateParams = @{
            (id)kSecAttrLabel: requestData().creationOptions.rp.id,
            (id)kSecAttrApplicationTag: [NSData dataWithBytes:requestData().creationOptions.user.idVector.data() length:requestData().creationOptions.user.idVector.size()],
        };
        status = SecItemUpdate((__bridge CFDictionaryRef)updateQuery, (__bridge CFDictionaryRef)updateParams);
        if (status) {
            LOG_ERROR("Couldn't update the Keychain item: %d", status);
            receiveRespond(ExceptionData { UnknownError, "Unknown internal error."_s });
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
        RetainPtr<CFDataRef> publicKeyDataRef;
        {
            auto publicKey = adoptCF(SecKeyCopyPublicKey(privateKey));
            CFErrorRef errorRef = nullptr;
            publicKeyDataRef = adoptCF(SecKeyCopyExternalRepresentation(publicKey.get(), &errorRef));
            auto retainError = adoptCF(errorRef);
            if (errorRef) {
                LOG_ERROR("Couldn't export the public key: %@", (NSError*)errorRef);
                receiveRespond(ExceptionData { UnknownError, "Unknown internal error."_s });
                return;
            }
            ASSERT(((NSData *)publicKeyDataRef.get()).length == (1 + 2 * ES256KeySizeInBytes)); // 04 | X | Y
        }

        // COSE Encoding
        // FIXME(183535): Improve CBOR encoder to work with bytes directly.
        Vector<uint8_t> x(ES256KeySizeInBytes);
        [(NSData *)publicKeyDataRef.get() getBytes: x.data() range:NSMakeRange(1, ES256KeySizeInBytes)];
        Vector<uint8_t> y(ES256KeySizeInBytes);
        [(NSData *)publicKeyDataRef.get() getBytes: y.data() range:NSMakeRange(1 + ES256KeySizeInBytes, ES256KeySizeInBytes)];
        cbor::CBORValue::MapValue publicKeyMap;
        publicKeyMap[cbor::CBORValue(COSE::kty)] = cbor::CBORValue(COSE::EC2);
        publicKeyMap[cbor::CBORValue(COSE::alg)] = cbor::CBORValue(COSE::ES256);
        publicKeyMap[cbor::CBORValue(COSE::crv)] = cbor::CBORValue(COSE::P_256);
        publicKeyMap[cbor::CBORValue(COSE::x)] = cbor::CBORValue(WTFMove(x));
        publicKeyMap[cbor::CBORValue(COSE::y)] = cbor::CBORValue(WTFMove(y));
        auto cosePublicKey = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(publicKeyMap)));
        if (!cosePublicKey) {
            LOG_ERROR("Couldn't encode the public key into COSE binaries.");
            receiveRespond(ExceptionData { UnknownError, "Unknown internal error."_s });
            return;
        }
        attestedCredentialData.appendVector(cosePublicKey.value());
    }

    // Step 12.
    auto authData = buildAuthData(requestData().creationOptions.rp.id, makeCredentialFlags, counter, attestedCredentialData);

    // Step 13. Apple Attestation Cont'
    // Assemble the attestation object:
    // https://www.w3.org/TR/webauthn/#attestation-object
    cbor::CBORValue::MapValue attestationStatementMap;
    {
        Vector<uint8_t> signature;
        {
            CFErrorRef errorRef = nullptr;
            // FIXME(183652): Reduce prompt for biometrics
            auto signatureRef = adoptCF(SecKeyCreateSignature(privateKey, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)[NSData dataWithBytes:authData.data() length:authData.size()], &errorRef));
            auto retainError = adoptCF(errorRef);
            if (errorRef) {
                LOG_ERROR("Couldn't generate the signature: %@", (NSError*)errorRef);
                receiveRespond(ExceptionData { UnknownError, "Unknown internal error."_s });
                return;
            }
            signature = toVector((NSData *)signatureRef.get());
        }
        attestationStatementMap[cbor::CBORValue("alg")] = cbor::CBORValue(COSE::ES256);
        attestationStatementMap[cbor::CBORValue("sig")] = cbor::CBORValue(signature);
        Vector<cbor::CBORValue> cborArray;
        for (size_t i = 0; i < [certificates count]; i++)
            cborArray.append(cbor::CBORValue(toVector((NSData *)adoptCF(SecCertificateCopyData((__bridge SecCertificateRef)certificates[i])).get())));
        attestationStatementMap[cbor::CBORValue("x5c")] = cbor::CBORValue(WTFMove(cborArray));
    }

    cbor::CBORValue::MapValue attestationObjectMap;
    attestationObjectMap[cbor::CBORValue("authData")] = cbor::CBORValue(authData);
    attestationObjectMap[cbor::CBORValue("fmt")] = cbor::CBORValue("Apple");
    attestationObjectMap[cbor::CBORValue("attStmt")] = cbor::CBORValue(WTFMove(attestationStatementMap));
    auto attestationObject = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(attestationObjectMap)));
    if (!attestationObject) {
        LOG_ERROR("Couldn't encode the attestation object.");
        receiveRespond(ExceptionData { UnknownError, "Unknown internal error."_s });
        return;
    }

    receiveRespond(PublicKeyCredentialData { ArrayBuffer::create(credentialId.data(), credentialId.size()), true, nullptr, ArrayBuffer::create(attestationObject.value().data(), attestationObject.value().size()), nullptr, nullptr, nullptr });
#endif // !PLATFORM(IOS_FAMILY)
}

void LocalAuthenticator::getAssertion()
{
    // FIXME(182772)
    using namespace LocalAuthenticatorInternal;
    ASSERT(m_state == State::Init);
    m_state = State::RequestReceived;

#if PLATFORM(IOS_FAMILY)
    // The following implements https://www.w3.org/TR/webauthn/#op-get-assertion as of 5 December 2017.
    // Skip Step 2 as requireUserVerification is enforced.
    // Skip Step 8 as extensions are not supported yet.
    // Step 12 is implicitly captured by all UnknownError exception callbacks.
    // Step 3-5. Unlike the spec, if an allow list is provided and there is no intersection between existing ones and the allow list, we always return NotAllowedError.
    HashSet<String> allowCredentialIds = produceHashSet(requestData().requestOptions.allowCredentials);
    if (!requestData().requestOptions.allowCredentials.isEmpty() && allowCredentialIds.isEmpty()) {
        receiveRespond(ExceptionData { NotAllowedError, "No matched credentials are found in the platform attached authenticator."_s });
        return;
    }

    // Search Keychain for the RP ID.
    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrLabel: requestData().requestOptions.rpId,
        (id)kSecReturnAttributes: @YES,
        (id)kSecMatchLimit: (id)kSecMatchLimitAll
    };
    CFTypeRef attributesArrayRef = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &attributesArrayRef);
    if (status && status != errSecItemNotFound) {
        LOG_ERROR("Couldn't query Keychain: %d", status);
        receiveRespond(ExceptionData { UnknownError, "Unknown internal error."_s });
        return;
    }
    auto retainAttributesArray = adoptCF(attributesArrayRef);

    NSArray *intersectedCredentialsAttributes = nil;
    if (requestData().requestOptions.allowCredentials.isEmpty())
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
        receiveRespond(ExceptionData { NotAllowedError, "No matched credentials are found in the platform attached authenticator."_s });
        return;
    }

    // Step 6.
    // FIXME(rdar://problem/35900534): We don't have an UI to prompt users for selecting intersectedCredentials, and therefore we always use the first one for now.
    NSDictionary *selectedCredentialAttributes = intersectedCredentialsAttributes[0];

    // Step 7. Get user consent.
    // FIXME(rdar://problem/35900593): Update to a formal UI.
    auto callback = [
        weakThis = makeWeakPtr(*this),
        credentialId = toVector(selectedCredentialAttributes[(id)kSecAttrApplicationLabel]),
        userhandle = toVector(selectedCredentialAttributes[(id)kSecAttrApplicationTag])
    ](LocalConnection::UserConsent consent, LAContext *context) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;

        weakThis->continueGetAssertionAfterUserConsented(consent, context, credentialId, userhandle);
    };
    m_connection->getUserConsent(
        String::format("Log into %s with %s.", requestData().requestOptions.rpId.utf8().data(), selectedCredentialAttributes[(id)kSecAttrApplicationTag]),
        (__bridge SecAccessControlRef)selectedCredentialAttributes[(id)kSecAttrAccessControl],
        WTFMove(callback));
#endif // !PLATFORM(IOS_FAMILY)
}

void LocalAuthenticator::continueGetAssertionAfterUserConsented(LocalConnection::UserConsent consent, LAContext *context, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& userhandle)
{
    // FIXME(182772)
    using namespace LocalAuthenticatorInternal;
    ASSERT(m_state == State::RequestReceived);
    m_state = State::UserConsented;

#if PLATFORM(IOS_FAMILY)
    if (consent == LocalConnection::UserConsent::No) {
        receiveRespond(ExceptionData { NotAllowedError, "Couldn't get user consent."_s });
        return;
    }

    // Step 9-10.
    // FIXME(183533): Due to the stated Keychain limitations, we can't save the counter value.
    // Therefore, it is always zero.
    uint32_t counter = 0;
    auto authData = buildAuthData(requestData().requestOptions.rpId, getAssertionFlags, counter, { });

    // Step 11.
    Vector<uint8_t> signature;
    {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrApplicationLabel: [NSData dataWithBytes:credentialId.data() length:credentialId.size()],
            (id)kSecUseAuthenticationContext: context,
            (id)kSecReturnRef: @YES,
        };
        CFTypeRef privateKeyRef = nullptr;
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &privateKeyRef);
        if (status) {
            LOG_ERROR("Couldn't get the private key reference: %d", status);
            receiveRespond(ExceptionData { UnknownError, "Unknown internal error."_s });
            return;
        }
        auto privateKey = adoptCF(privateKeyRef);

        NSMutableData *dataToSign = [NSMutableData dataWithBytes:authData.data() length:authData.size()];
        [dataToSign appendBytes:requestData().hash.data() length:requestData().hash.size()];

        CFErrorRef errorRef = nullptr;
        // FIXME: Converting CFTypeRef to SecKeyRef is quite subtle here.
        auto signatureRef = adoptCF(SecKeyCreateSignature((__bridge SecKeyRef)((id)privateKeyRef), kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)dataToSign, &errorRef));
        auto retainError = adoptCF(errorRef);
        if (errorRef) {
            LOG_ERROR("Couldn't generate the signature: %@", (NSError*)errorRef);
            receiveRespond(ExceptionData { UnknownError, "Unknown internal error."_s });
            return;
        }
        signature = toVector((NSData *)signatureRef.get());
    }

    // Step 13.
    receiveRespond(PublicKeyCredentialData { ArrayBuffer::create(credentialId.data(), credentialId.size()), false, nullptr, nullptr, ArrayBuffer::create(authData.data(), authData.size()), ArrayBuffer::create(signature.data(), signature.size()), ArrayBuffer::create(userhandle.data(), userhandle.size()) });
#endif // !PLATFORM(IOS_FAMILY)
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
