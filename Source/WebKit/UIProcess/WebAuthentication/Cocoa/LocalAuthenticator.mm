/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

#import "Logging.h"
#import "MockLocalConnection.h"
#import <Security/SecItem.h>
#import <WebCore/AuthenticatorAssertionResponse.h>
#import <WebCore/AuthenticatorAttachment.h>
#import <WebCore/AuthenticatorAttestationResponse.h>
#import <WebCore/CBORReader.h>
#import <WebCore/CBORWriter.h>
#import <WebCore/CredentialPropertiesOutput.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/FidoConstants.h>
#import <WebCore/MediationRequirement.h>
#import <WebCore/PublicKeyCredentialCreationOptions.h>
#import <WebCore/PublicKeyCredentialRequestOptions.h>
#import <WebCore/WebAuthenticationConstants.h>
#import <WebCore/WebAuthenticationUtils.h>
#import <pal/crypto/CryptoDigest.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/Base64.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringHash.h>

#import "AuthenticationServicesCoreSoftLink.h"

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/LocalAuthenticatorAdditions.h>
#else
static void updateQueryIfNecessary(NSMutableDictionary *)
{
}
static inline String groupForAttributes(NSDictionary *attributes)
{
    return nullString();
}
static bool shouldUpdateQuery()
{
    return false;
}
static inline RefPtr<ArrayBuffer> alternateBlobIfNecessary(const WebKit::WebAuthenticationRequestData requestData)
{
    return nullptr;
}

#define LOCAL_AUTHENTICATOR_ADDITIONS
#endif

namespace WebKit {
using namespace fido;
using namespace WebCore;
using namespace WebAuthn;
using CBOR = cbor::CBORValue;

BOOL shouldUseAlternateKeychainAttribute()
{
#if HAVE(UNIFIED_ASC_AUTH_UI)
    if (![WebKit::getASCWebKitSPISupportClass() respondsToSelector:@selector(shouldUseAlternateKeychainAttribute)])
        return NO;

    return [WebKit::getASCWebKitSPISupportClass() shouldUseAlternateKeychainAttribute];
#else
    return NO;
#endif
}

namespace LocalAuthenticatorInternal {

constexpr uint64_t counter = 0;
// This aaguid is unattested.
constexpr std::array<uint8_t, 16> aaguid = { 0xFB, 0xFC, 0x30, 0x07, 0x15, 0x4E, 0x4E, 0xCC, 0x8C, 0x0B, 0x6E, 0x02, 0x05, 0x57, 0xD7, 0xBD }; // Randomly generated.

constexpr char kLargeBlobMapKey[] = "largeBlob";

static inline bool emptyTransportsOrContain(const Vector<AuthenticatorTransport>& transports, AuthenticatorTransport target)
{
    return transports.isEmpty() ? true : transports.contains(target);
}

// A Base64 encoded string of the Credential ID is used as the key of the hash set.
static inline HashSet<String> produceHashSet(const Vector<PublicKeyCredentialDescriptor>& credentialDescriptors)
{
    HashSet<String> result;
    for (auto& credentialDescriptor : credentialDescriptors) {
        if (emptyTransportsOrContain(credentialDescriptor.transports, AuthenticatorTransport::Internal) && credentialDescriptor.type == PublicKeyCredentialType::PublicKey)
            result.add(base64EncodeToString(BufferSource { credentialDescriptor.id }.span()));
    }
    return result;
}

static inline uint8_t authDataFlags(ClientDataType type, LocalConnection::UserVerification verification, bool synchronizable, std::optional<MediationRequirement> mediation)
{
    auto flags = 0;
    if (type != ClientDataType::Create || mediation != MediationRequirement::Conditional)
        flags |= userPresenceFlag;
    if (verification != LocalConnection::UserVerification::Presence)
        flags |= userVerifiedFlag;
    if (type == ClientDataType::Create)
        flags |= attestedCredentialDataIncludedFlag;
    if (synchronizable)
        flags |= backupEligibilityFlag | backupStateFlag;
    return flags;
}

static inline Vector<uint8_t> aaguidVector()
{
    static NeverDestroyed<Vector<uint8_t>> aaguidVector = { aaguid };
    return aaguidVector;
}

static inline RetainPtr<NSData> toNSData(ArrayBuffer* buffer)
{
    ASSERT(buffer);
    return WTF::toNSData(buffer->span());
}

static inline Ref<ArrayBuffer> toArrayBuffer(NSData *data)
{
    return ArrayBuffer::create(span(data));
}

static inline Ref<ArrayBuffer> toArrayBuffer(std::span<const uint8_t> data)
{
    return ArrayBuffer::create(data);
}

static Vector<AuthenticatorTransport> transports()
{
    Vector<WebCore::AuthenticatorTransport> transports = { WebCore::AuthenticatorTransport::Internal };
    if (shouldUpdateQuery())
        transports.append(WebCore::AuthenticatorTransport::Hybrid);
    return transports;
}

} // LocalAuthenticatorInternal

void LocalAuthenticator::clearAllCredentials()
{
    // FIXME<rdar://problem/57171201>: We should guard the method with a first party entitlement once WebAuthn is avaliable for third parties.
    auto query = adoptNS([[NSMutableDictionary alloc] init]);
    [query setDictionary:@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrAccessGroup: LocalAuthenticatorAccessGroup,
        (id)kSecUseDataProtectionKeychain: @YES
    }];
    updateQueryIfNecessary(query.get());

    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query.get());
    if (status && status != errSecItemNotFound)
        LOG_ERROR(makeString("Couldn't clear all credential: "_s, status).utf8().data());
}

LocalAuthenticator::LocalAuthenticator(Ref<LocalConnection>&& connection)
    : m_connection(WTFMove(connection))
{
}

std::optional<Vector<Ref<AuthenticatorAssertionResponse>>> LocalAuthenticator::getExistingCredentials(const String& rpId)
{
    RetainPtr sortedAttributesArray = protectedConnection()->getExistingCredentials(rpId);
    Vector<Ref<AuthenticatorAssertionResponse>> result;
    result.reserveInitialCapacity([sortedAttributesArray count]);
    for (NSDictionary *attributes in sortedAttributesArray.get()) {
        auto decodedResponse = cbor::CBORReader::read(makeVector(attributes[(id)kSecAttrApplicationTag]));
        if (!decodedResponse || !decodedResponse->isMap()) {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
        auto& responseMap = decodedResponse->getMap();

        RefPtr<ArrayBuffer> userHandle;
        auto it = responseMap.find(CBOR(fido::kEntityIdMapKey));
        if (it != responseMap.end() && it->second.isByteString()) {
            userHandle = LocalAuthenticatorInternal::toArrayBuffer(it->second.getByteString());
        }

        it = responseMap.find(CBOR(fido::kEntityNameMapKey));
        if (it == responseMap.end() || !it->second.isString()) {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
        auto& username = it->second.getString();

        id credentialID;
        if (shouldUseAlternateKeychainAttribute()) {
            credentialID = attributes[(id)kSecAttrAlias];
            if (!credentialID)
                credentialID = attributes[(id)kSecAttrApplicationLabel];
        } else
            credentialID = attributes[(id)kSecAttrApplicationLabel];

        auto response = AuthenticatorAssertionResponse::create(LocalAuthenticatorInternal::toArrayBuffer(credentialID), WTFMove(userHandle), String(username), (__bridge SecAccessControlRef)attributes[(id)kSecAttrAccessControl], AuthenticatorAttachment::Platform);

        auto group = groupForAttributes(attributes);
        if (!group.isNull()) {
            response->setGroup(group);
            response->setSynchronizable(true);
        } else if ([[attributes allKeys] containsObject:bridge_cast(kSecAttrSynchronizable)])
            response->setSynchronizable([attributes[(id)kSecAttrSynchronizable] isEqual:@YES]);
        it = responseMap.find(CBOR(fido::kDisplayNameMapKey));
        if (it != responseMap.end() && it->second.isString())
            response->setDisplayName(it->second.getString());

        it = responseMap.find(CBOR(LocalAuthenticatorInternal::kLargeBlobMapKey));
        if (it != responseMap.end() && it->second.isByteString())
            response->setLargeBlob(ArrayBuffer::create(it->second.getByteString()));

        response->setAccessGroup(attributes[(id)kSecAttrAccessGroup]);

        result.append(WTFMove(response));
    }
    return result;
}


void LocalAuthenticator::makeCredential()
{
    using namespace LocalAuthenticatorInternal;
    ASSERT(m_state == State::Init);
    m_state = State::RequestReceived;
    auto& creationOptions = std::get<PublicKeyCredentialCreationOptions>(requestData().options);

    // The following implements https://www.w3.org/TR/webauthn/#op-make-cred as of 5 December 2017.
    // Skip Step 4-5 as requireResidentKey and requireUserVerification are enforced.
    // Skip Step 9 as extensions are not supported yet.
    // Step 8 is implicitly captured by all UnknownError exception receiveResponds.
    // Skip Step 10 as counter is constantly 0.
    // Step 2.
    if (notFound == creationOptions.pubKeyCredParams.findIf([] (auto& pubKeyCredParam) {
        return pubKeyCredParam.type == PublicKeyCredentialType::PublicKey && pubKeyCredParam.alg == COSE::ES256;
    })) {
        receiveException({ ExceptionCode::NotSupportedError, "The platform attached authenticator doesn't support any provided PublicKeyCredentialParameters."_s });
        return;
    }

    // Step 3.
    ASSERT(creationOptions.rp.id);
    auto existingCredentials = getExistingCredentials(creationOptions.rp.id);
    if (!existingCredentials) {
        receiveException({ ExceptionCode::UnknownError, "Couldn't get existing credentials"_s });
        return;
    }
    m_existingCredentials = WTFMove(*existingCredentials);

    auto excludeCredentialIds = produceHashSet(creationOptions.excludeCredentials);
    if (!excludeCredentialIds.isEmpty()) {
        if (notFound != m_existingCredentials.findIf([&excludeCredentialIds] (auto& credential) {
            auto* rawId = credential->rawId();
            ASSERT(rawId);
            return excludeCredentialIds.contains(base64EncodeToString(rawId->span()));
        })) {
            receiveException({ ExceptionCode::InvalidStateError, "At least one credential matches an entry of the excludeCredentials list in the platform attached authenticator."_s }, WebAuthenticationStatus::LAExcludeCredentialsMatched);
            return;
        }
    }

    if (RefPtr observer = this->observer()) {
        auto callback = [weakThis = WeakPtr { *this }] (LAContext *context) {
            ASSERT(RunLoop::isMain());
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->continueMakeCredentialAfterReceivingLAContext(context);
        };
        observer->requestLAContextForUserVerification(WTFMove(callback));
    }
}

void LocalAuthenticator::continueMakeCredentialAfterReceivingLAContext(LAContext *context)
{
    ASSERT(m_state == State::RequestReceived);
    m_state = State::PolicyDecided;

    RetainPtr<SecAccessControlRef> accessControl;
    {
        CFErrorRef errorRef = nullptr;
        accessControl = adoptCF(SecAccessControlCreateWithFlags(NULL, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, kSecAccessControlPrivateKeyUsage | kSecAccessControlUserPresence, &errorRef));
        auto retainError = adoptCF(errorRef);
        if (errorRef) {
            receiveException({ ExceptionCode::UnknownError, makeString("Couldn't create access control: "_s, String(((NSError*)errorRef).localizedDescription)) });
            return;
        }
    }

    SecAccessControlRef accessControlRef = accessControl.get();
    auto callback = [accessControl = WTFMove(accessControl), context = retainPtr(context), weakThis = WeakPtr { *this }] (LocalConnection::UserVerification verification) {
        ASSERT(RunLoop::isMain());
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->continueMakeCredentialAfterUserVerification(accessControl.get(), verification, context.get());
    };
    protectedConnection()->verifyUser(accessControlRef, context, WTFMove(callback));
}

std::optional<WebCore::ExceptionData> LocalAuthenticator::processLargeBlobExtension(const WebCore::PublicKeyCredentialCreationOptions& options, WebCore::AuthenticationExtensionsClientOutputs& extensionOutputs)
{
    if (!options.extensions || !options.extensions->largeBlob)
        return std::nullopt;

    auto largeBlobInput = options.extensions->largeBlob;

    // Step 1.
    if (largeBlobInput->read || largeBlobInput->write)
        return WebCore::ExceptionData { ExceptionCode::NotSupportedError, "Cannot use `read` or `write` with largeBlob during create."_s };

    // Step 2-3.
    extensionOutputs.largeBlob = AuthenticationExtensionsClientOutputs::LargeBlobOutputs {
        .supported = true,
    };

    return std::nullopt;
}

std::optional<WebCore::ExceptionData> LocalAuthenticator::processLargeBlobExtension(const WebCore::PublicKeyCredentialRequestOptions& options, WebCore::AuthenticationExtensionsClientOutputs& extensionOutputs, const Ref<WebCore::AuthenticatorAssertionResponse>& response)
{
    using namespace LocalAuthenticatorInternal;
    if (!options.extensions || !options.extensions->largeBlob)
        return std::nullopt;

    auto largeBlobInput = options.extensions->largeBlob;
    AuthenticationExtensionsClientOutputs::LargeBlobOutputs largeBlobOutput;

    // Step 1.
    if (!largeBlobInput->support.isNull())
        return WebCore::ExceptionData { ExceptionCode::NotSupportedError, "Cannot use `support` with largeBlob during get."_s };

    // Step 2.
    if (largeBlobInput->read && largeBlobInput->write)
        return WebCore::ExceptionData { ExceptionCode::NotSupportedError, "Cannot use `read` and `write` simultaneously with largeBlob."_s };

    // Step 3.
    if (largeBlobInput->read && largeBlobInput->read.value()) {
        auto blob = alternateBlobIfNecessary(requestData());

        if (!blob)
            blob = response->largeBlob();

        if (blob)
            largeBlobOutput.blob = blob;
    }

    // Step 4.
    if (largeBlobInput->write) {
        auto nsCredentialId = toNSData(response->rawId());
        BOOL useAlternateKeychainAttribute = shouldUseAlternateKeychainAttribute();
        auto fetchQuery = adoptNS([[NSMutableDictionary alloc] init]);
        [fetchQuery setDictionary:@{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
            (id)kSecUseDataProtectionKeychain: @YES,
            (id)kSecReturnAttributes: @YES,
            (id)kSecReturnPersistentRef: @YES,
        }];

        CFStringRef credentialIdKey = useAlternateKeychainAttribute ? kSecAttrAlias : kSecAttrApplicationLabel;
        [fetchQuery setObject:nsCredentialId.get() forKey:(id)credentialIdKey];

        CFTypeRef attributesArrayRef = nullptr;
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)fetchQuery.get(), &attributesArrayRef);
        if (useAlternateKeychainAttribute && status == errSecItemNotFound) {
            [fetchQuery removeObjectForKey:(id)kSecAttrAlias];
            [fetchQuery setObject:nsCredentialId.get() forKey:(id)kSecAttrApplicationLabel];
            status = SecItemCopyMatching((__bridge CFDictionaryRef)fetchQuery.get(), &attributesArrayRef);
        }

        if (status && status != errSecItemNotFound) {
            ASSERT_NOT_REACHED();
            return WebCore::ExceptionData { ExceptionCode::UnknownError, "Attempted to update unknown credential."_s };
        }

        auto retainAttributesArray = adoptCF(attributesArrayRef);
        NSDictionary *dict = (NSDictionary *)attributesArrayRef;

        auto decodedResponse = cbor::CBORReader::read(makeVector(dict[(id)kSecAttrApplicationTag]));
        if (!decodedResponse || !decodedResponse->isMap()) {
            ASSERT_NOT_REACHED();
            return WebCore::ExceptionData { ExceptionCode::UnknownError, "Could not read credential."_s };
        }

        CBOR::MapValue responseMap;
        for (auto it = decodedResponse->getMap().begin(); it != decodedResponse->getMap().end(); it++)
            responseMap[it->first.clone()] = it->second.clone();

        responseMap[CBOR(kLargeBlobMapKey)] = CBOR(largeBlobInput->write.value());

        auto outputTag = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(responseMap)));
        auto nsOutputTag = toNSData(*outputTag);
        NSDictionary *updateQuery = @{
            (id)kSecValuePersistentRef: dict[(id)kSecValuePersistentRef],
        };

        NSDictionary *updateParams = @{
            (id)kSecAttrApplicationTag: nsOutputTag.get(),
        };

        status = SecItemUpdate((__bridge CFDictionaryRef)updateQuery, (__bridge CFDictionaryRef)updateParams);
        largeBlobOutput.written = status == errSecSuccess;
    }

    extensionOutputs.largeBlob = largeBlobOutput;
    return std::nullopt;
}

std::optional<WebCore::ExceptionData> LocalAuthenticator::processClientExtensions(std::variant<Ref<AuthenticatorAttestationResponse>, Ref<AuthenticatorAssertionResponse>> response)
{
    using namespace LocalAuthenticatorInternal;
    return WTF::switchOn(response, [&](const Ref<AuthenticatorAttestationResponse>& response) -> std::optional<WebCore::ExceptionData> {
        auto& creationOptions = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
        if (!creationOptions.extensions)
            return std::nullopt;

        auto extensionOutputs = response->extensions();
        if (creationOptions.extensions->credProps)
            extensionOutputs.credProps = CredentialPropertiesOutput { true /* rk */ };

        auto exception = processLargeBlobExtension(creationOptions, extensionOutputs);
        if (exception)
            return exception;

        response->setExtensions(WTFMove(extensionOutputs));
        return std::nullopt;
    }, [&](const Ref<AuthenticatorAssertionResponse>& response) -> std::optional<WebCore::ExceptionData> {
        auto& assertionOptions = std::get<PublicKeyCredentialRequestOptions>(requestData().options);
        if (!assertionOptions.extensions)
            return std::nullopt;

        auto extensionOutputs = response->extensions();
        auto exception = processLargeBlobExtension(assertionOptions, extensionOutputs, response);
        if (exception)
            return exception;

        response->setExtensions(WTFMove(extensionOutputs));
        return std::nullopt;
    });
}

void LocalAuthenticator::continueMakeCredentialAfterUserVerification(SecAccessControlRef accessControlRef, LocalConnection::UserVerification verification, LAContext *context)
{
    using namespace LocalAuthenticatorInternal;

    ASSERT(m_state == State::PolicyDecided);
    m_state = State::UserVerified;
    auto& creationOptions = std::get<PublicKeyCredentialCreationOptions>(requestData().options);

    if (!validateUserVerification(verification))
        return;

    // Here is the keychain schema.
    // kSecAttrLabel: RP ID
    // kSecAttrApplicationLabel: Credential ID (auto-gen by Keychain)
    // kSecAttrApplicationTag: { "id": UserEntity.id, "name": UserEntity.name, "displayName": UserEntity.name} (CBOR encoded)
    // Noted, the vale of kSecAttrApplicationLabel is automatically generated by the Keychain, which is a SHA-1 hash of
    // the public key.
    ASSERT(creationOptions.rp.id);
    const auto& secAttrLabel = creationOptions.rp.id;

    // id, name, and displayName are required in PublicKeyCredentialUserEntity
    // https://www.w3.org/TR/webauthn-2/#dictdef-publickeycredentialuserentity
    cbor::CBORValue::MapValue userEntityMap;
    userEntityMap[cbor::CBORValue(fido::kEntityIdMapKey)] = cbor::CBORValue(creationOptions.user.id);
    userEntityMap[cbor::CBORValue(fido::kEntityNameMapKey)] = cbor::CBORValue(creationOptions.user.name);
    userEntityMap[cbor::CBORValue(fido::kDisplayNameMapKey)] = cbor::CBORValue(creationOptions.user.displayName);
    auto userEntity = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(userEntityMap)));
    ASSERT(userEntity);
    auto secAttrApplicationTag = toNSData(*userEntity);

    // Step 7.
    // The above-to-create private key will be inserted into keychain while using SEP.
    auto privateKey = protectedConnection()->createCredentialPrivateKey(context, accessControlRef, secAttrLabel, secAttrApplicationTag.get());
    if (!privateKey) {
        receiveException({ ExceptionCode::UnknownError, "Couldn't create private key."_s });
        return;
    }

    RetainPtr<CFDataRef> publicKeyDataRef;
    {
        auto publicKey = adoptCF(SecKeyCopyPublicKey(privateKey.get()));
        CFErrorRef errorRef = nullptr;
        publicKeyDataRef = adoptCF(SecKeyCopyExternalRepresentation(publicKey.get(), &errorRef));
        auto retainError = adoptCF(errorRef);
        if (errorRef) {
            receiveException({ ExceptionCode::UnknownError, makeString("Couldn't export the public key: "_s, String(((NSError*)errorRef).localizedDescription)) });
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
        digest->addBytes(span(nsPublicKeyData));
        credentialId = digest->computeHash();
        m_provisionalCredentialId = toNSData(credentialId);

        auto query = adoptNS([[NSMutableDictionary alloc] init]);
        [query setDictionary:@{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrLabel: secAttrLabel,
            (id)kSecAttrApplicationLabel: m_provisionalCredentialId.get(),
            (id)kSecUseDataProtectionKeychain: @YES
        }];
        updateQueryIfNecessary(query.get());

#if ASSERT_ENABLED
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query.get(), nullptr);
        ASSERT(!status);
#endif

        NSDictionary *updateAttributes = @{
            (id)kSecAttrAlias: m_provisionalCredentialId.get()
        };
        SecItemUpdate((__bridge CFDictionaryRef)query.get(), (__bridge CFDictionaryRef)updateAttributes);
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

    auto flags = authDataFlags(ClientDataType::Create, verification, shouldUpdateQuery(), requestData().mediation);
    // Skip attestation.
    auto authData = buildAuthData(creationOptions.rp.id, flags, counter, buildAttestedCredentialData(aaguidVector(), credentialId, cosePublicKey));

    LOCAL_AUTHENTICATOR_ADDITIONS

    auto attestationObject = buildAttestationObject(WTFMove(authData), String { emptyString() }, { }, AttestationConveyancePreference::None);

    finishMakeCredential(WTFMove(credentialId), WTFMove(attestationObject), std::nullopt);
}

void LocalAuthenticator::continueMakeCredentialAfterAttested(Vector<uint8_t>&& credentialId, Vector<uint8_t>&& authData, NSArray *certificates, NSError *error)
{
    using namespace LocalAuthenticatorInternal;

    ASSERT(m_state == State::UserVerified);
    m_state = State::Attested;
    auto& creationOptions = std::get<PublicKeyCredentialCreationOptions>(requestData().options);

    if (error) {
        LOG_ERROR("Couldn't attest: %s", String(error.localizedDescription).utf8().data());
        auto attestationObject = buildAttestationObject(WTFMove(authData), String { emptyString() }, { }, AttestationConveyancePreference::None);
        finishMakeCredential(WTFMove(credentialId), WTFMove(attestationObject), std::nullopt);
        return;
    }
    // Attestation Certificate and Attestation Issuing CA
    ASSERT(certificates && ([certificates count] == 2));

    // Step 13. Apple Attestation Cont'
    // Assemble the attestation object:
    // https://www.w3.org/TR/webauthn/#attestation-object
    cbor::CBORValue::MapValue attestationStatementMap;
    {
        Vector<cbor::CBORValue> cborArray;
        for (size_t i = 0; i < [certificates count]; i++)
            cborArray.append(cbor::CBORValue(makeVector((NSData *)adoptCF(SecCertificateCopyData((__bridge SecCertificateRef)certificates[i])).get())));
        attestationStatementMap[cbor::CBORValue("x5c")] = cbor::CBORValue(WTFMove(cborArray));
    }
    auto attestationObject = buildAttestationObject(WTFMove(authData), "apple"_s, WTFMove(attestationStatementMap), creationOptions.attestation);

    finishMakeCredential(WTFMove(credentialId), WTFMove(attestationObject), std::nullopt);
}

void LocalAuthenticator::finishMakeCredential(Vector<uint8_t>&& credentialId, Vector<uint8_t>&& attestationObject, std::optional<ExceptionData> exception)
{
    if (exception) {
        receiveException(WTFMove(exception.value()));
        return;
    }

    deleteDuplicateCredential();
    auto response = AuthenticatorAttestationResponse::create(credentialId, attestationObject, AuthenticatorAttachment::Platform, LocalAuthenticatorInternal::transports());
    exception = processClientExtensions(response);
    if (exception)
        receiveException(WTFMove(exception.value()));
    else
        receiveRespond(WTFMove(response));
}

void LocalAuthenticator::getAssertion()
{
    using namespace LocalAuthenticatorInternal;
    ASSERT(m_state == State::Init);
    m_state = State::RequestReceived;
    auto& requestOptions = std::get<PublicKeyCredentialRequestOptions>(requestData().options);

    // The following implements https://www.w3.org/TR/webauthn/#op-get-assertion as of 5 December 2017.
    // Skip Step 2 as requireUserVerification is enforced.
    // Skip Step 9 as counter is constantly 0.
    // Step 12 is implicitly captured by all UnknownError exception callbacks.
    // Step 3-5. Unlike the spec, if an allow list is provided and there is no intersection between existing ones and the allow list, we always return NotAllowedError.
    auto allowCredentialIds = produceHashSet(requestOptions.allowCredentials);
    if (!requestOptions.allowCredentials.isEmpty() && allowCredentialIds.isEmpty()) {
        receiveException({ ExceptionCode::NotAllowedError, "No matched credentials are found in the platform attached authenticator."_s }, WebAuthenticationStatus::LANoCredential);
        RELEASE_LOG_ERROR(WebAuthn, "No matched credentials are found in the platform attached authenticator.");
        return;
    }

    // Search Keychain for the RP ID.
    auto existingCredentials = getExistingCredentials(requestOptions.rpId);
    if (!existingCredentials) {
        receiveException({ ExceptionCode::UnknownError, "Couldn't get existing credentials"_s });
        RELEASE_LOG_ERROR(WebAuthn, "Couldn't get existing credentials");
        return;
    }
    m_existingCredentials = WTFMove(*existingCredentials);

    auto assertionResponses = WTF::compactMap(m_existingCredentials, [&](auto& credential) -> RefPtr<WebCore::AuthenticatorAssertionResponse> {
        if (allowCredentialIds.isEmpty())
            return credential.copyRef();
        auto* rawId = credential->rawId();
        if (allowCredentialIds.contains(base64EncodeToString(rawId->span())))
            return credential.copyRef();
        return nullptr;
    });
    if (assertionResponses.isEmpty()) {
        receiveException({ ExceptionCode::NotAllowedError, "No matched credentials are found in the platform attached authenticator."_s }, WebAuthenticationStatus::LANoCredential);
        RELEASE_LOG_ERROR(WebAuthn, "No matched credentials are found in the platform attached authenticator.");
        return;
    }

    // Step 6-7. User consent is implicitly acquired by selecting responses.
    protectedConnection()->filterResponses(assertionResponses);

    if (RefPtr observer = this->observer()) {
        auto callback = [weakThis = WeakPtr { *this }] (AuthenticatorAssertionResponse* response) {
            RELEASE_ASSERT(RunLoop::isMain());
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            auto result = protectedThis->m_existingCredentials.findIf([expectedResponse = response] (auto& response) {
                return response.ptr() == expectedResponse;
            });
            if (result == notFound)
                return;
            protectedThis->continueGetAssertionAfterResponseSelected(protectedThis->m_existingCredentials[result].copyRef());
        };
        observer->selectAssertionResponse(WTFMove(assertionResponses), WebAuthenticationSource::Local, WTFMove(callback));
    }
}

void LocalAuthenticator::continueGetAssertionAfterResponseSelected(Ref<WebCore::AuthenticatorAssertionResponse>&& response)
{
    ASSERT(m_state == State::RequestReceived);
    m_state = State::ResponseSelected;

    auto accessControlRef = response->accessControl();
    LAContext *context = response->laContext();
    auto callback = [weakThis = WeakPtr { *this }, response = WTFMove(response)] (LocalConnection::UserVerification verification) mutable {
        ASSERT(RunLoop::isMain());
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->continueGetAssertionAfterUserVerification(WTFMove(response), verification, response->laContext());
    };

    protectedConnection()->verifyUser(accessControlRef, context, WTFMove(callback));
}

void LocalAuthenticator::continueGetAssertionAfterUserVerification(Ref<WebCore::AuthenticatorAssertionResponse>&& response, LocalConnection::UserVerification verification, LAContext *context)
{
    using namespace LocalAuthenticatorInternal;
    ASSERT(m_state == State::ResponseSelected);
    m_state = State::UserVerified;

    if (!validateUserVerification(verification))
        return;

    // Step 10.
    auto requestOptions = std::get<PublicKeyCredentialRequestOptions>(requestData().options);
    auto flags = authDataFlags(ClientDataType::Get, verification, response->synchronizable(), requestData().mediation);
    auto authData = buildAuthData(requestOptions.rpId, flags, counter, { });

    // Step 11.
    RetainPtr<CFDataRef> signature;
    auto nsCredentialId = toNSData(response->rawId());
    {
        BOOL useAlternateKeychainAttribute = shouldUseAlternateKeychainAttribute();
        NSMutableDictionary *queryDictionary = [@{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
            (id)kSecReturnRef: @YES,
            (id)kSecUseDataProtectionKeychain: @YES
        } mutableCopy];

        CFStringRef credentialIdKey = useAlternateKeychainAttribute ? kSecAttrAlias : kSecAttrApplicationLabel;
        queryDictionary[(id)credentialIdKey] = nsCredentialId.get();

        if (context)
            queryDictionary[(id)kSecUseAuthenticationContext] = context;

        auto query = adoptNS(queryDictionary);

        CFTypeRef privateKeyRef = nullptr;
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query.get(), &privateKeyRef);
        if (useAlternateKeychainAttribute && status == errSecItemNotFound) {
            queryDictionary[(id)kSecAttrAlias] = nil;
            queryDictionary[(id)kSecAttrApplicationLabel] = nsCredentialId.get();
            status = SecItemCopyMatching((__bridge CFDictionaryRef)query.get(), &privateKeyRef);
        }

        if (status) {
            receiveException({ ExceptionCode::UnknownError, makeString("Couldn't get the private key reference: "_s, status) });
            RELEASE_LOG_ERROR(WebAuthn, "Couldn't get the private key reference: %d", status);
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
            RELEASE_LOG_ERROR(WebAuthn, "Couldn't generate signature: %@", ((NSError*)errorRef).localizedDescription);
            receiveException({ ExceptionCode::UnknownError, makeString("Couldn't generate the signature: "_s, String(((NSError*)errorRef).localizedDescription)) });
            return;
        }
    }

    // Extra step: update the Keychain item with the same value to update its modification date such that LRU can be used
    // for selectAssertionResponse
    BOOL useAlternateKeychainAttribute = shouldUseAlternateKeychainAttribute();
    auto query = adoptNS([[NSMutableDictionary alloc] init]);
    [query setDictionary:@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
        (id)kSecUseDataProtectionKeychain: @YES
    }];

    CFStringRef credentialIdKey = useAlternateKeychainAttribute ? kSecAttrAlias : kSecAttrApplicationLabel;
    [query setObject:nsCredentialId.get() forKey:(id)credentialIdKey];

    NSDictionary *updateParams = @{
        (id)kSecAttrAlias: nsCredentialId.get(),
    };
    auto status = SecItemUpdate((__bridge CFDictionaryRef)query.get(), (__bridge CFDictionaryRef)updateParams);
    if (useAlternateKeychainAttribute && status == errSecItemNotFound) {
        [query removeObjectForKey:(id)kSecAttrAlias];
        [query setObject:nsCredentialId.get() forKey:(id)kSecAttrApplicationLabel];
        status = SecItemUpdate((__bridge CFDictionaryRef)query.get(), (__bridge CFDictionaryRef)updateParams);
    }

    if (status)
        RELEASE_LOG_ERROR(WebAuthn, "Couldn't update the Keychain item: %d", status);

    // Step 13.
    response->setAuthenticatorData(WTFMove(authData));
    response->setSignature(toArrayBuffer((NSData *)signature.get()));
    auto exception = processClientExtensions(response);
    if (exception)
        receiveException(WTFMove(exception.value()));
    else
        receiveRespond(WTFMove(response));
}

void LocalAuthenticator::receiveException(ExceptionData&& exception, WebAuthenticationStatus status) const
{
    LOG_ERROR(exception.message.utf8().data());

    // Roll back the just created credential.
    if (m_provisionalCredentialId) {
        BOOL useAlternateKeychainAttribute = shouldUseAlternateKeychainAttribute();
        auto query = adoptNS([[NSMutableDictionary alloc] init]);
        [query setDictionary:@{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecUseDataProtectionKeychain: @YES
        }];
        updateQueryIfNecessary(query.get());

        CFStringRef credentialIdKey = useAlternateKeychainAttribute ? kSecAttrAlias : kSecAttrApplicationLabel;
        [query setObject:m_provisionalCredentialId.get() forKey:(id)credentialIdKey];

        OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query.get());
        if (useAlternateKeychainAttribute && status == errSecItemNotFound) {
            [query removeObjectForKey:(id)kSecAttrAlias];
            [query setObject:m_provisionalCredentialId.get() forKey:(id)kSecAttrApplicationLabel];
            status = SecItemDelete((__bridge CFDictionaryRef)query.get());
        }

        if (status)
            RELEASE_LOG_ERROR(WebAuthn, "Couldn't delete provisional credential while handling error: %d", status);
    }

    if (RefPtr observer = this->observer())
        observer->authenticatorStatusUpdated(status);

    receiveRespond(WTFMove(exception));
    return;
}

void LocalAuthenticator::deleteDuplicateCredential() const
{
    using namespace LocalAuthenticatorInternal;

    auto& creationOptions = std::get<PublicKeyCredentialCreationOptions>(requestData().options);
    m_existingCredentials.findIf([creationOptions] (auto& credential) {
        auto* userHandle = credential->userHandle();
        ASSERT(userHandle);
        if (!equalSpans(userHandle->span(), BufferSource { creationOptions.user.id } .span()))
            return false;

        BOOL useAlternateKeychainAttribute = shouldUseAlternateKeychainAttribute();
        auto query = adoptNS([[NSMutableDictionary alloc] init]);
        [query setDictionary:@{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrAlias: toNSData(credential->rawId()).get(),
            (id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
            (id)kSecUseDataProtectionKeychain: @YES
        }];

        CFStringRef credentialIdKey = useAlternateKeychainAttribute ? kSecAttrAlias : kSecAttrApplicationLabel;
        [query setObject:toNSData(credential->rawId()).get() forKey:(id)credentialIdKey];

        OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query.get());
        if (useAlternateKeychainAttribute && status == errSecItemNotFound) {
            [query removeObjectForKey:(id)kSecAttrAlias];
            [query setObject:toNSData(credential->rawId()).get() forKey:(id)kSecAttrApplicationLabel];
            status = SecItemDelete((__bridge CFDictionaryRef)query.get());
        }

        if (status && status != errSecItemNotFound)
            RELEASE_LOG_ERROR(WebAuthn, "Couldn't delete older credential: %d", status);
        return true;
    });
}

bool LocalAuthenticator::validateUserVerification(LocalConnection::UserVerification verification) const
{
    if (verification == LocalConnection::UserVerification::Cancel) {
        if (RefPtr observer = this->observer())
            observer->cancelRequest();
        return false;
    }

    if (verification == LocalConnection::UserVerification::No) {
        receiveException({ ExceptionCode::NotAllowedError, "Couldn't verify user."_s });
        RELEASE_LOG_ERROR(WebAuthn, "Could not verify user.");
        return false;
    }

    return true;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
