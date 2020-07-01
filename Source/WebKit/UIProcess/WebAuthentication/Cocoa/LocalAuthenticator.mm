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

#import <Security/SecItem.h>
#import <WebCore/AuthenticatorAssertionResponse.h>
#import <WebCore/AuthenticatorAttestationResponse.h>
#import <WebCore/CBORReader.h>
#import <WebCore/CBORWriter.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/FidoConstants.h>
#import <WebCore/PublicKeyCredentialCreationOptions.h>
#import <WebCore/PublicKeyCredentialRequestOptions.h>
#import <WebCore/WebAuthenticationConstants.h>
#import <WebCore/WebAuthenticationUtils.h>
#import <pal/crypto/CryptoDigest.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/Base64.h>
#import <wtf/text/StringHash.h>

namespace WebKit {
using namespace WebCore;
using CBOR = cbor::CBORValue;

namespace LocalAuthenticatorInternal {

// See https://www.w3.org/TR/webauthn/#flags.
const uint8_t makeCredentialFlags = 0b01000101; // UP, UV and AT are set.
const uint8_t getAssertionFlags = 0b00000101; // UP and UV are set.
// Credential ID is currently SHA-1 of the corresponding public key.
const uint16_t credentialIdLength = 20;
const uint64_t counter = 0;

static inline bool emptyTransportsOrContain(const Vector<AuthenticatorTransport>& transports, AuthenticatorTransport target)
{
    return transports.isEmpty() ? true : transports.contains(target);
}

// A Base64 encoded string of the Credential ID is used as the key of the hash set.
static inline HashSet<String> produceHashSet(const Vector<PublicKeyCredentialDescriptor>& credentialDescriptors)
{
    HashSet<String> result;
    for (auto& credentialDescriptor : credentialDescriptors) {
        if (emptyTransportsOrContain(credentialDescriptor.transports, AuthenticatorTransport::Internal)
            && credentialDescriptor.type == PublicKeyCredentialType::PublicKey
            && credentialDescriptor.idVector.size() == credentialIdLength)
            result.add(base64Encode(credentialDescriptor.idVector.data(), credentialDescriptor.idVector.size()));
    }
    return result;
}

static inline Vector<uint8_t> toVector(NSData *data)
{
    Vector<uint8_t> result;
    result.append(reinterpret_cast<const uint8_t*>(data.bytes), data.length);
    return result;
}

static inline RetainPtr<NSData> toNSData(const Vector<uint8_t>& data)
{
    return adoptNS([[NSData alloc] initWithBytes:data.data() length:data.size()]);
}

static inline RetainPtr<NSData> toNSData(ArrayBuffer* buffer)
{
    ASSERT(buffer);
    return adoptNS([[NSData alloc] initWithBytes:buffer->data() length:buffer->byteLength()]);
}

static inline Ref<ArrayBuffer> toArrayBuffer(NSData *data)
{
    return ArrayBuffer::create(reinterpret_cast<const uint8_t*>(data.bytes), data.length);
}

static inline Ref<ArrayBuffer> toArrayBuffer(const Vector<uint8_t>& data)
{
    return ArrayBuffer::create(data.data(), data.size());
}

static Optional<Vector<Ref<AuthenticatorAssertionResponse>>> getExistingCredentials(const String& rpId)
{
    // Search Keychain for existing credential matched the RP ID.
    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrLabel: rpId,
        (id)kSecReturnAttributes: @YES,
        (id)kSecMatchLimit: (id)kSecMatchLimitAll,
#if HAVE(DATA_PROTECTION_KEYCHAIN)
        (id)kSecUseDataProtectionKeychain: @YES
#else
        (id)kSecAttrNoLegacy: @YES
#endif
    };
    CFTypeRef attributesArrayRef = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &attributesArrayRef);
    if (status && status != errSecItemNotFound)
        return WTF::nullopt;
    auto retainAttributesArray = adoptCF(attributesArrayRef);
    NSArray *sortedAttributesArray = [(NSArray *)attributesArrayRef sortedArrayUsingComparator:^(NSDictionary *a, NSDictionary *b) {
        return [b[(id)kSecAttrModificationDate] compare:a[(id)kSecAttrModificationDate]];
    }];

    Vector<Ref<AuthenticatorAssertionResponse>> result;
    result.reserveInitialCapacity(sortedAttributesArray.count);
    for (NSDictionary *attributes in sortedAttributesArray) {
        auto decodedResponse = cbor::CBORReader::read(toVector(attributes[(id)kSecAttrApplicationTag]));
        if (!decodedResponse || !decodedResponse->isMap()) {
            ASSERT_NOT_REACHED();
            return WTF::nullopt;
        }
        auto& responseMap = decodedResponse->getMap();

        auto it = responseMap.find(CBOR(kEntityIdMapKey));
        if (it == responseMap.end() || !it->second.isByteString()) {
            ASSERT_NOT_REACHED();
            return WTF::nullopt;
        }
        auto& userHandle = it->second.getByteString();

        it = responseMap.find(CBOR(kEntityNameMapKey));
        if (it == responseMap.end() || !it->second.isString()) {
            ASSERT_NOT_REACHED();
            return WTF::nullopt;
        }
        auto& username = it->second.getString();

        result.uncheckedAppend(AuthenticatorAssertionResponse::create(toArrayBuffer(attributes[(id)kSecAttrApplicationLabel]), toArrayBuffer(userHandle), String(username), (__bridge SecAccessControlRef)attributes[(id)kSecAttrAccessControl]));
    }
    return result;
}

} // LocalAuthenticatorInternal

void LocalAuthenticator::clearAllCredentials()
{
    // FIXME<rdar://problem/57171201>: We should guard the method with a first party entitlement once WebAuthn is avaliable for third parties.
    NSDictionary* deleteQuery = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrAccessGroup: (id)String(LocalAuthenticatiorAccessGroup),
#if HAVE(DATA_PROTECTION_KEYCHAIN)
        (id)kSecUseDataProtectionKeychain: @YES
#else
        (id)kSecAttrNoLegacy: @YES
#endif
    };
    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)deleteQuery);
    if (status && status != errSecItemNotFound)
        LOG_ERROR(makeString("Couldn't clear all credential: "_s, status).utf8().data());
}

LocalAuthenticator::LocalAuthenticator(UniqueRef<LocalConnection>&& connection)
    : m_connection(WTFMove(connection))
{
}

void LocalAuthenticator::makeCredential()
{
    using namespace LocalAuthenticatorInternal;
    ASSERT(m_state == State::Init);
    m_state = State::RequestReceived;
    auto& creationOptions = WTF::get<PublicKeyCredentialCreationOptions>(requestData().options);

    // The following implements https://www.w3.org/TR/webauthn/#op-make-cred as of 5 December 2017.
    // Skip Step 4-5 as requireResidentKey and requireUserVerification are enforced.
    // Skip Step 9 as extensions are not supported yet.
    // Step 8 is implicitly captured by all UnknownError exception receiveResponds.
    // Skip Step 10 as counter is constantly 0.
    // Step 2.
    if (notFound == creationOptions.pubKeyCredParams.findMatching([] (auto& pubKeyCredParam) {
        return pubKeyCredParam.type == PublicKeyCredentialType::PublicKey && pubKeyCredParam.alg == COSE::ES256;
    })) {
        receiveException({ NotSupportedError, "The platform attached authenticator doesn't support any provided PublicKeyCredentialParameters."_s });
        return;
    }

    // Step 3.
    auto existingCredentials = getExistingCredentials(creationOptions.rp.id);
    if (!existingCredentials) {
        receiveException({ UnknownError, makeString("Couldn't get existing credentials") });
        return;
    }
    m_existingCredentials = WTFMove(*existingCredentials);

    auto excludeCredentialIds = produceHashSet(creationOptions.excludeCredentials);
    if (!excludeCredentialIds.isEmpty()) {
        if (notFound != m_existingCredentials.findMatching([&excludeCredentialIds] (auto& credential) {
            auto* rawId = credential->rawId();
            ASSERT(rawId);
            return excludeCredentialIds.contains(base64Encode(rawId->data(), rawId->byteLength()));
        })) {
            receiveException({ NotAllowedError, "At least one credential matches an entry of the excludeCredentials list in the platform attached authenticator."_s }, WebAuthenticationStatus::LAExcludeCredentialsMatched);
            return;
        }
    }

    // Step 6.
    // Get user consent.
    if (auto* observer = this->observer()) {
        auto callback = [weakThis = makeWeakPtr(*this)] (LocalAuthenticatorPolicy policy) {
            ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;

            weakThis->continueMakeCredentialAfterDecidePolicy(policy);
        };
        observer->decidePolicyForLocalAuthenticator(WTFMove(callback));
    }
}

void LocalAuthenticator::continueMakeCredentialAfterDecidePolicy(LocalAuthenticatorPolicy policy)
{
    ASSERT(m_state == State::RequestReceived);
    m_state = State::PolicyDecided;

    auto& creationOptions = WTF::get<PublicKeyCredentialCreationOptions>(requestData().options);

    if (policy == LocalAuthenticatorPolicy::Disallow) {
        receiveRespond(ExceptionData { UnknownError, "Disallow local authenticator."_s });
        return;
    }

    RetainPtr<SecAccessControlRef> accessControl;
    {
        CFErrorRef errorRef = nullptr;
        accessControl = adoptCF(SecAccessControlCreateWithFlags(NULL, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, kSecAccessControlPrivateKeyUsage | kSecAccessControlUserPresence, &errorRef));
        auto retainError = adoptCF(errorRef);
        if (errorRef) {
            receiveException({ UnknownError, makeString("Couldn't create access control: ", String(((NSError*)errorRef).localizedDescription)) });
            return;
        }
    }

    SecAccessControlRef accessControlRef = accessControl.get();
    auto callback = [accessControl = WTFMove(accessControl), weakThis = makeWeakPtr(*this)] (LocalConnection::UserVerification verification, LAContext *context) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;

        weakThis->continueMakeCredentialAfterUserVerification(accessControl.get(), verification, context);
    };
    m_connection->verifyUser(creationOptions.rp.id, getClientDataType(requestData().options), accessControlRef, WTFMove(callback));
}

void LocalAuthenticator::continueMakeCredentialAfterUserVerification(SecAccessControlRef accessControlRef, LocalConnection::UserVerification verification, LAContext *context)
{
    using namespace LocalAuthenticatorInternal;

    ASSERT(m_state == State::PolicyDecided);
    m_state = State::UserVerified;
    auto& creationOptions = WTF::get<PublicKeyCredentialCreationOptions>(requestData().options);

    if (!validateUserVerification(verification))
        return;

    // Here is the keychain schema.
    // kSecAttrLabel: RP ID
    // kSecAttrApplicationLabel: Credential ID (auto-gen by Keychain)
    // kSecAttrApplicationTag: { "id": UserEntity.id, "name": UserEntity.name } (CBOR encoded)
    // Noted, the vale of kSecAttrApplicationLabel is automatically generated by the Keychain, which is a SHA-1 hash of
    // the public key.
    const auto& secAttrLabel = creationOptions.rp.id;

    cbor::CBORValue::MapValue userEntityMap;
    userEntityMap[cbor::CBORValue(kEntityIdMapKey)] = cbor::CBORValue(creationOptions.user.idVector);
    userEntityMap[cbor::CBORValue(kEntityNameMapKey)] = cbor::CBORValue(creationOptions.user.name);
    auto userEntity = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(userEntityMap)));
    ASSERT(userEntity);
    auto secAttrApplicationTag = toNSData(*userEntity);

    // Step 7.
    // The above-to-create private key will be inserted into keychain while using SEP.
    auto privateKey = m_connection->createCredentialPrivateKey(context, accessControlRef, secAttrLabel, secAttrApplicationTag.get());
    if (!privateKey) {
        receiveException({ UnknownError, "Couldn't create private key."_s });
        return;
    }

    RetainPtr<CFDataRef> publicKeyDataRef;
    {
        auto publicKey = adoptCF(SecKeyCopyPublicKey(privateKey.get()));
        CFErrorRef errorRef = nullptr;
        publicKeyDataRef = adoptCF(SecKeyCopyExternalRepresentation(publicKey.get(), &errorRef));
        auto retainError = adoptCF(errorRef);
        if (errorRef) {
            receiveException({ UnknownError, makeString("Couldn't export the public key: ", String(((NSError*)errorRef).localizedDescription)) });
            return;
        }
        ASSERT(((NSData *)publicKeyDataRef.get()).length == (1 + 2 * ES256FieldElementLength)); // 04 | X | Y
    }
    NSData *nsPublicKeyData = (NSData *)publicKeyDataRef.get();

    // Query credentialId in the keychain could be racy as it is the only unique identifier
    // of the key item. Instead we calculate that, and examine its equaity in DEBUG build.
    Vector<uint8_t> credentialId;
    {
        auto digest = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_1);
        digest->addBytes(nsPublicKeyData.bytes, nsPublicKeyData.length);
        credentialId = digest->computeHash();
        m_provisionalCredentialId = toNSData(credentialId);

#ifndef NDEBUG
        NSDictionary *credentialIdQuery = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrLabel: secAttrLabel,
            (id)kSecAttrApplicationLabel: m_provisionalCredentialId.get(),
#if HAVE(DATA_PROTECTION_KEYCHAIN)
            (id)kSecUseDataProtectionKeychain: @YES
#else
            (id)kSecAttrNoLegacy: @YES
#endif
        };
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)credentialIdQuery, nullptr);
        ASSERT(!status);
#endif // NDEBUG
    }

    // Step 11. https://www.w3.org/TR/webauthn/#attested-credential-data
    // credentialPublicKey
    Vector<uint8_t> cosePublicKey;
    {
        // COSE Encoding
        Vector<uint8_t> x(ES256FieldElementLength);
        [nsPublicKeyData getBytes: x.data() range:NSMakeRange(1, ES256FieldElementLength)];
        Vector<uint8_t> y(ES256FieldElementLength);
        [nsPublicKeyData getBytes: y.data() range:NSMakeRange(1 + ES256FieldElementLength, ES256FieldElementLength)];
        cosePublicKey = encodeES256PublicKeyAsCBOR(WTFMove(x), WTFMove(y));
    }
    // FIXME(rdar://problem/38320512): Define Apple AAGUID.
    auto attestedCredentialData = buildAttestedCredentialData(Vector<uint8_t>(aaguidLength, 0), credentialId, cosePublicKey);

    // Step 12.
    auto authData = buildAuthData(creationOptions.rp.id, makeCredentialFlags, counter, attestedCredentialData);

    // Skip Apple Attestation for none attestation.
    if (creationOptions.attestation == AttestationConveyancePreference::None) {
        deleteDuplicateCredential();

        auto attestationObject = buildAttestationObject(WTFMove(authData), "", { }, AttestationConveyancePreference::None);
        receiveRespond(AuthenticatorAttestationResponse::create(credentialId, attestationObject));
        return;
    }

    // Step 13. Apple Attestation
    auto nsAuthData = toNSData(authData);
    auto callback = [credentialId = WTFMove(credentialId), authData = WTFMove(authData), weakThis = makeWeakPtr(*this)] (NSArray * _Nullable certificates, NSError * _Nullable error) mutable {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueMakeCredentialAfterAttested(WTFMove(credentialId), WTFMove(authData), certificates, error);
    };
    m_connection->getAttestation(privateKey.get(), nsAuthData.get(), toNSData(requestData().hash).get(), WTFMove(callback));
}

void LocalAuthenticator::continueMakeCredentialAfterAttested(Vector<uint8_t>&& credentialId, Vector<uint8_t>&& authData, NSArray *certificates, NSError *error)
{
    using namespace LocalAuthenticatorInternal;

    ASSERT(m_state == State::UserVerified);
    m_state = State::Attested;
    auto& creationOptions = WTF::get<PublicKeyCredentialCreationOptions>(requestData().options);

    if (error) {
        receiveException({ UnknownError, makeString("Couldn't attest: ", String(error.localizedDescription)) });
        return;
    }
    // Attestation Certificate and Attestation Issuing CA
    ASSERT(certificates && ([certificates count] == 2));

    // Step 13. Apple Attestation Cont'
    // Assemble the attestation object:
    // https://www.w3.org/TR/webauthn/#attestation-object
    cbor::CBORValue::MapValue attestationStatementMap;
    {
        attestationStatementMap[cbor::CBORValue("alg")] = cbor::CBORValue(COSE::ES256);
        Vector<cbor::CBORValue> cborArray;
        for (size_t i = 0; i < [certificates count]; i++)
            cborArray.append(cbor::CBORValue(toVector((NSData *)adoptCF(SecCertificateCopyData((__bridge SecCertificateRef)certificates[i])).get())));
        attestationStatementMap[cbor::CBORValue("x5c")] = cbor::CBORValue(WTFMove(cborArray));
    }
    auto attestationObject = buildAttestationObject(WTFMove(authData), "apple", WTFMove(attestationStatementMap), creationOptions.attestation);

    deleteDuplicateCredential();
    receiveRespond(AuthenticatorAttestationResponse::create(credentialId, attestationObject));
}

void LocalAuthenticator::getAssertion()
{
    using namespace LocalAuthenticatorInternal;
    ASSERT(m_state == State::Init);
    m_state = State::RequestReceived;
    auto& requestOptions = WTF::get<PublicKeyCredentialRequestOptions>(requestData().options);

    // The following implements https://www.w3.org/TR/webauthn/#op-get-assertion as of 5 December 2017.
    // Skip Step 2 as requireUserVerification is enforced.
    // Skip Step 8 as extensions are not supported yet.
    // Skip Step 9 as counter is constantly 0.
    // Step 12 is implicitly captured by all UnknownError exception callbacks.
    // Step 3-5. Unlike the spec, if an allow list is provided and there is no intersection between existing ones and the allow list, we always return NotAllowedError.
    auto allowCredentialIds = produceHashSet(requestOptions.allowCredentials);
    if (!requestOptions.allowCredentials.isEmpty() && allowCredentialIds.isEmpty()) {
        receiveException({ NotAllowedError, "No matched credentials are found in the platform attached authenticator."_s }, WebAuthenticationStatus::LANoCredential);
        return;
    }

    // Search Keychain for the RP ID.
    auto existingCredentials = getExistingCredentials(requestOptions.rpId);
    if (!existingCredentials) {
        receiveException({ UnknownError, makeString("Couldn't get existing credentials") });
        return;
    }
    m_existingCredentials = WTFMove(*existingCredentials);

    Vector<Ref<WebCore::AuthenticatorAssertionResponse>> assertionResponses;
    assertionResponses.reserveInitialCapacity(m_existingCredentials.size());
    for (auto& credential : m_existingCredentials) {
        if (allowCredentialIds.isEmpty()) {
            assertionResponses.uncheckedAppend(credential.copyRef());
            continue;
        }

        auto* rawId = credential->rawId();
        if (allowCredentialIds.contains(base64Encode(rawId->data(), rawId->byteLength())))
            assertionResponses.uncheckedAppend(credential.copyRef());
    }
    if (assertionResponses.isEmpty()) {
        receiveException({ NotAllowedError, "No matched credentials are found in the platform attached authenticator."_s }, WebAuthenticationStatus::LANoCredential);
        return;
    }

    // Step 6-7. User consent is implicitly acquired by selecting responses.
    m_connection->filterResponses(assertionResponses);

    if (auto* observer = this->observer()) {
        auto callback = [this, weakThis = makeWeakPtr(*this)] (AuthenticatorAssertionResponse* response) {
            ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;

            auto result = m_existingCredentials.findMatching([expectedResponse = response] (auto& response) {
                return response.ptr() == expectedResponse;
            });
            if (result == notFound)
                return;
            continueGetAssertionAfterResponseSelected(m_existingCredentials[result].copyRef());
        };
        observer->selectAssertionResponse(WTFMove(assertionResponses), WebAuthenticationSource::Local, WTFMove(callback));
    }
}

void LocalAuthenticator::continueGetAssertionAfterResponseSelected(Ref<WebCore::AuthenticatorAssertionResponse>&& response)
{
    ASSERT(m_state == State::RequestReceived);
    m_state = State::ResponseSelected;

    auto& requestOptions = WTF::get<PublicKeyCredentialRequestOptions>(requestData().options);

    auto accessControlRef = response->accessControl();
    auto callback = [
        weakThis = makeWeakPtr(*this),
        response = WTFMove(response)
    ] (LocalConnection::UserVerification verification, LAContext *context) mutable {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;

        weakThis->continueGetAssertionAfterUserVerification(WTFMove(response), verification, context);
    };
    m_connection->verifyUser(requestOptions.rpId, getClientDataType(requestData().options), accessControlRef, WTFMove(callback));
}

void LocalAuthenticator::continueGetAssertionAfterUserVerification(Ref<WebCore::AuthenticatorAssertionResponse>&& response, LocalConnection::UserVerification verification, LAContext *context)
{
    using namespace LocalAuthenticatorInternal;
    ASSERT(m_state == State::ResponseSelected);
    m_state = State::UserVerified;

    if (!validateUserVerification(verification))
        return;

    // Step 10.
    auto requestOptions = WTF::get<PublicKeyCredentialRequestOptions>(requestData().options);
    auto authData = buildAuthData(requestOptions.rpId, getAssertionFlags, counter, { });

    // Step 11.
    RetainPtr<CFDataRef> signature;
    auto nsCredentialId = toNSData(response->rawId());
    {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrApplicationLabel: nsCredentialId.get(),
            (id)kSecUseAuthenticationContext: context,
            (id)kSecReturnRef: @YES,
#if HAVE(DATA_PROTECTION_KEYCHAIN)
            (id)kSecUseDataProtectionKeychain: @YES
#else
            (id)kSecAttrNoLegacy: @YES
#endif
        };
        CFTypeRef privateKeyRef = nullptr;
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &privateKeyRef);
        if (status) {
            receiveException({ UnknownError, makeString("Couldn't get the private key reference: ", status) });
            return;
        }
        auto privateKey = adoptCF(privateKeyRef);

        NSMutableData *dataToSign = [NSMutableData dataWithBytes:authData.data() length:authData.size()];
        [dataToSign appendBytes:requestData().hash.data() length:requestData().hash.size()];

        CFErrorRef errorRef = nullptr;
        // FIXME: Converting CFTypeRef to SecKeyRef is quite subtle here.
        signature = adoptCF(SecKeyCreateSignature((__bridge SecKeyRef)((id)privateKeyRef), kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)dataToSign, &errorRef));
        auto retainError = adoptCF(errorRef);
        if (errorRef) {
            receiveException({ UnknownError, makeString("Couldn't generate the signature: ", String(((NSError*)errorRef).localizedDescription)) });
            return;
        }
    }

    // Extra step: update the Keychain item with the same value to update its modification date such that LRU can be used
    // for selectAssertionResponse
    NSDictionary *updateQuery = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrApplicationLabel: nsCredentialId.get(),
#if HAVE(DATA_PROTECTION_KEYCHAIN)
        (id)kSecUseDataProtectionKeychain: @YES
#else
        (id)kSecAttrNoLegacy: @YES
#endif
    };
    NSDictionary *updateParams = @{
        (id)kSecAttrLabel: requestOptions.rpId,
    };
    auto status = SecItemUpdate((__bridge CFDictionaryRef)updateQuery, (__bridge CFDictionaryRef)updateParams);
    if (status)
        LOG_ERROR("Couldn't update the Keychain item: %d", status);

    // Step 13.
    response->setAuthenticatorData(WTFMove(authData));
    response->setSignature(toArrayBuffer((NSData *)signature.get()));
    receiveRespond(WTFMove(response));
}

void LocalAuthenticator::receiveException(ExceptionData&& exception, WebAuthenticationStatus status) const
{
    LOG_ERROR(exception.message.utf8().data());

    // Roll back the just created credential.
    if (m_provisionalCredentialId) {
        NSDictionary* deleteQuery = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrApplicationLabel: m_provisionalCredentialId.get(),
#if HAVE(DATA_PROTECTION_KEYCHAIN)
            (id)kSecUseDataProtectionKeychain: @YES
#else
            (id)kSecAttrNoLegacy: @YES
#endif
        };
        OSStatus status = SecItemDelete((__bridge CFDictionaryRef)deleteQuery);
        if (status)
            LOG_ERROR(makeString("Couldn't delete provisional credential while handling error: "_s, status).utf8().data());
    }

    if (auto* observer = this->observer())
        observer->authenticatorStatusUpdated(status);

    receiveRespond(WTFMove(exception));
    return;
}

void LocalAuthenticator::deleteDuplicateCredential() const
{
    using namespace LocalAuthenticatorInternal;

    auto& creationOptions = WTF::get<PublicKeyCredentialCreationOptions>(requestData().options);
    m_existingCredentials.findMatching([creationOptions] (auto& credential) {
        auto* userHandle = credential->userHandle();
        ASSERT(userHandle);
        if (userHandle->byteLength() != creationOptions.user.idVector.size())
            return false;
        if (memcmp(userHandle->data(), creationOptions.user.idVector.data(), userHandle->byteLength()))
            return false;

        NSDictionary* deleteQuery = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrApplicationLabel: toNSData(credential->rawId()).get(),
#if HAVE(DATA_PROTECTION_KEYCHAIN)
            (id)kSecUseDataProtectionKeychain: @YES
#else
            (id)kSecAttrNoLegacy: @YES
#endif
        };
        OSStatus status = SecItemDelete((__bridge CFDictionaryRef)deleteQuery);
        if (status && status != errSecItemNotFound)
            LOG_ERROR(makeString("Couldn't delete older credential: "_s, status).utf8().data());
        return true;
    });
}

bool LocalAuthenticator::validateUserVerification(LocalConnection::UserVerification verification) const
{
    if (verification == LocalConnection::UserVerification::Cancel) {
        if (auto* observer = this->observer())
            observer->cancelRequest();
        return false;
    }

    if (verification == LocalConnection::UserVerification::No) {
        receiveException({ NotAllowedError, "Couldn't verify user."_s });
        return false;
    }

    return true;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
