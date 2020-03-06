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
#import <WebCore/AuthenticatorAssertionResponse.h>
#import <WebCore/AuthenticatorAttestationResponse.h>
#import <WebCore/CBORWriter.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/PublicKeyCredentialCreationOptions.h>
#import <WebCore/PublicKeyCredentialRequestOptions.h>
#import <WebCore/WebAuthenticationConstants.h>
#import <WebCore/WebAuthenticationUtils.h>
#import <pal/crypto/CryptoDigest.h>
#import <wtf/HashSet.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/StringHash.h>

namespace WebKit {
using namespace WebCore;

namespace LocalAuthenticatorInternal {

// See https://www.w3.org/TR/webauthn/#flags.
const uint8_t makeCredentialFlags = 0b01000101; // UP, UV and AT are set.
const uint8_t getAssertionFlags = 0b00000101; // UP and UV are set.
// Credential ID is currently SHA-1 of the corresponding public key.
const uint16_t credentialIdLength = 20;

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
            && credentialDescriptor.idVector.size() == credentialIdLength)
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

static inline RetainPtr<NSData> toNSData(const Vector<uint8_t>& data)
{
    // FIXME(183534): Consider using initWithBytesNoCopy.
    return adoptNS([[NSData alloc] initWithBytes:data.data() length:data.size()]);
}

static inline RetainPtr<NSData> toNSData(ArrayBuffer* buffer)
{
    ASSERT(buffer);
    // FIXME(183534): Consider using initWithBytesNoCopy.
    return adoptNS([[NSData alloc] initWithBytes:buffer->data() length:buffer->byteLength()]);
}

static inline Ref<ArrayBuffer> toArrayBuffer(NSData *data)
{
    return ArrayBuffer::create(reinterpret_cast<const uint8_t*>(data.bytes), data.length);
}

// FIXME(<rdar://problem/60108131>): Remove this whitelist once testing is complete.
static const HashSet<String>& whitelistedRpId()
{
    static NeverDestroyed<HashSet<String>> whitelistedRpId = std::initializer_list<String> {
        "",
        "localhost",
        "tlstestwebkit.org",
    };
    return whitelistedRpId;
}

} // LocalAuthenticatorInternal

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
    // Step 2.
    if (notFound == creationOptions.pubKeyCredParams.findMatching([] (auto& pubKeyCredParam) {
        return pubKeyCredParam.type == PublicKeyCredentialType::PublicKey && pubKeyCredParam.alg == COSE::ES256;
    })) {
        receiveException({ NotSupportedError, "The platform attached authenticator doesn't support any provided PublicKeyCredentialParameters."_s });
        return;
    }

    // Step 3.
    auto excludeCredentialIds = produceHashSet(creationOptions.excludeCredentials);
    if (!excludeCredentialIds.isEmpty()) {
        // Search Keychain for the RP ID.
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrLabel: creationOptions.rp.id,
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
        if (status && status != errSecItemNotFound) {
            receiveException({ UnknownError, makeString("Couldn't query Keychain: ", status) });
            return;
        }
        auto retainAttributesArray = adoptCF(attributesArrayRef);

        // FIXME: Need to obtain user consent and then return different error according to the result.
        for (NSDictionary *nsAttributes in (NSArray *)attributesArrayRef) {
            NSData *nsCredentialId = nsAttributes[(id)kSecAttrApplicationLabel];
            if (excludeCredentialIds.contains(String(reinterpret_cast<const char*>(nsCredentialId.bytes), nsCredentialId.length))) {
                receiveException({ NotAllowedError, "At least one credential matches an entry of the excludeCredentials list in the platform attached authenticator."_s }, WebAuthenticationStatus::LAExcludeCredentialsMatched);
                return;
            }
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
    m_connection->verifyUser(accessControlRef, WTFMove(callback));
}

void LocalAuthenticator::continueMakeCredentialAfterUserVerification(SecAccessControlRef accessControlRef, LocalConnection::UserVerification verification, LAContext *context)
{
    using namespace LocalAuthenticatorInternal;

    ASSERT(m_state == State::PolicyDecided);
    m_state = State::UserVerified;
    auto& creationOptions = WTF::get<PublicKeyCredentialCreationOptions>(requestData().options);

    if (verification == LocalConnection::UserVerification::No) {
        receiveException({ NotAllowedError, "Couldn't verify user."_s });
        return;
    }

    // FIXME(183533): A single kSecClassKey item couldn't store all meta data. The following schema is a tentative solution
    // to accommodate the most important meta data, i.e. RP ID, Credential ID, and userhandle.
    // kSecAttrLabel: RP ID
    // kSecAttrApplicationLabel: Credential ID (auto-gen by Keychain)
    // kSecAttrApplicationTag: userhandle
    // Noted, the vale of kSecAttrApplicationLabel is automatically generated by the Keychain, which is a SHA-1 hash of
    // the public key. We borrow it directly for now to workaround the stated limitations.
    const auto& secAttrLabel = creationOptions.rp.id;
    auto secAttrApplicationTag = toNSData(creationOptions.user.idVector);

    // Step 7.5.
    // Failures after this point could block users' accounts forever. Should we follow the spec?
    NSDictionary* deleteQuery = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: secAttrLabel,
        (id)kSecAttrApplicationTag: secAttrApplicationTag.get(),
#if HAVE(DATA_PROTECTION_KEYCHAIN)
        (id)kSecUseDataProtectionKeychain: @YES
#else
        (id)kSecAttrNoLegacy: @YES
#endif
    };
    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)deleteQuery);
    if (status && status != errSecItemNotFound) {
        receiveException({ UnknownError, makeString("Couldn't delete older credential: ", status) });
        return;
    }

    // Step 7.1-7.4.
    // The above-to-create private key will be inserted into keychain while using SEP.
    auto privateKey = m_connection->createCredentialPrivateKey(context, accessControlRef, secAttrLabel, secAttrApplicationTag.get());
    if (!privateKey) {
        receiveException({ UnknownError, "Couldn't create private key."_s });
        return;
    }

    Vector<uint8_t> credentialId;
    {
        NSDictionary *credentialIdQuery = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrLabel: secAttrLabel,
            (id)kSecAttrApplicationTag: secAttrApplicationTag.get(),
            (id)kSecReturnAttributes: @YES,
#if HAVE(DATA_PROTECTION_KEYCHAIN)
            (id)kSecUseDataProtectionKeychain: @YES
#else
            (id)kSecAttrNoLegacy: @YES
#endif
        };
        CFTypeRef attributesRef = nullptr;
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)credentialIdQuery, &attributesRef);
        if (status) {
            receiveException({ UnknownError, makeString("Couldn't get Credential ID: ", status) });
            return;
        }
        auto retainAttributes = adoptCF(attributesRef);

        NSDictionary *nsAttributes = (NSDictionary *)attributesRef;
        credentialId = toVector(nsAttributes[(id)kSecAttrApplicationLabel]);
    }

    // Step 10.
    // FIXME(183533): store the counter.
    uint32_t counter = 0;

    // Step 11. https://www.w3.org/TR/webauthn/#attested-credential-data
    // credentialPublicKey
    Vector<uint8_t> cosePublicKey;
    {
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

        // COSE Encoding
        Vector<uint8_t> x(ES256FieldElementLength);
        [(NSData *)publicKeyDataRef.get() getBytes: x.data() range:NSMakeRange(1, ES256FieldElementLength)];
        Vector<uint8_t> y(ES256FieldElementLength);
        [(NSData *)publicKeyDataRef.get() getBytes: y.data() range:NSMakeRange(1 + ES256FieldElementLength, ES256FieldElementLength)];
        cosePublicKey = encodeES256PublicKeyAsCBOR(WTFMove(x), WTFMove(y));
    }
    // FIXME(rdar://problem/38320512): Define Apple AAGUID.
    auto attestedCredentialData = buildAttestedCredentialData(Vector<uint8_t>(aaguidLength, 0), credentialId, cosePublicKey);

    // Step 12.
    auto authData = buildAuthData(creationOptions.rp.id, makeCredentialFlags, counter, attestedCredentialData);

    // Skip Apple Attestation for none attestation, and non whitelisted RP ID for now.
    if (creationOptions.attestation == AttestationConveyancePreference::None || !whitelistedRpId().contains(creationOptions.rp.id)) {
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
    // Step 12 is implicitly captured by all UnknownError exception callbacks.
    // Step 3-5. Unlike the spec, if an allow list is provided and there is no intersection between existing ones and the allow list, we always return NotAllowedError.
    auto allowCredentialIds = produceHashSet(requestOptions.allowCredentials);
    if (!requestOptions.allowCredentials.isEmpty() && allowCredentialIds.isEmpty()) {
        receiveException({ NotAllowedError, "No matched credentials are found in the platform attached authenticator."_s }, WebAuthenticationStatus::LANoCredential);
        return;
    }

    // Search Keychain for the RP ID.
    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrLabel: requestOptions.rpId,
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
    if (status && status != errSecItemNotFound) {
        receiveException({ UnknownError, makeString("Couldn't query Keychain: ", status) });
        return;
    }
    auto retainAttributesArray = adoptCF(attributesArrayRef);

    NSArray *intersectedCredentialsAttributes = nil;
    if (requestOptions.allowCredentials.isEmpty())
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
        receiveException({ NotAllowedError, "No matched credentials are found in the platform attached authenticator."_s }, WebAuthenticationStatus::LANoCredential);
        return;
    }

    // Step 6-7. User consent is implicitly acquired by selecting responses.
    for (NSDictionary *attribute : intersectedCredentialsAttributes) {
        auto addResult = m_assertionResponses.add(AuthenticatorAssertionResponse::create(
            toArrayBuffer(attribute[(id)kSecAttrApplicationLabel]),
            toArrayBuffer(attribute[(id)kSecAttrApplicationTag]),
            (__bridge SecAccessControlRef)attribute[(id)kSecAttrAccessControl]));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
    m_connection->filterResponses(m_assertionResponses);

    if (auto* observer = this->observer()) {
        auto callback = [this, weakThis = makeWeakPtr(*this)] (AuthenticatorAssertionResponse* response) {
            ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;

            auto returnResponse = m_assertionResponses.take(response);
            if (!returnResponse)
                return;
            continueGetAssertionAfterResponseSelected(WTFMove(*returnResponse));
        };
        observer->selectAssertionResponse(m_assertionResponses, WebAuthenticationSource::Local, WTFMove(callback));
    }
}

void LocalAuthenticator::continueGetAssertionAfterResponseSelected(Ref<WebCore::AuthenticatorAssertionResponse>&& response)
{
    ASSERT(m_state == State::RequestReceived);
    m_state = State::ResponseSelected;

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
    m_connection->verifyUser(accessControlRef, WTFMove(callback));
}

void LocalAuthenticator::continueGetAssertionAfterUserVerification(Ref<WebCore::AuthenticatorAssertionResponse>&& response, LocalConnection::UserVerification verification, LAContext *context)
{
    using namespace LocalAuthenticatorInternal;
    ASSERT(m_state == State::ResponseSelected);
    m_state = State::UserVerified;

    if (verification == LocalConnection::UserVerification::No) {
        receiveException({ NotAllowedError, "Couldn't verify user."_s });
        return;
    }

    // Step 9-10.
    // FIXME(183533): Due to the stated Keychain limitations, we can't save the counter value.
    // Therefore, it is always zero.
    uint32_t counter = 0;
    auto authData = buildAuthData(WTF::get<PublicKeyCredentialRequestOptions>(requestData().options).rpId, getAssertionFlags, counter, { });

    // Step 11.
    RetainPtr<CFDataRef> signature;
    {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrApplicationLabel: toNSData(response->rawId()).get(),
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

    // Step 13.
    response->setAuthenticatorData(WTFMove(authData));
    response->setSignature(toArrayBuffer((NSData *)signature.get()));
    receiveRespond(WTFMove(response));
}

void LocalAuthenticator::receiveException(ExceptionData&& exception, WebAuthenticationStatus status) const
{
    LOG_ERROR(exception.message.utf8().data());
    if (auto* observer = this->observer())
        observer->authenticatorStatusUpdated(status);
    receiveRespond(WTFMove(exception));
    return;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
