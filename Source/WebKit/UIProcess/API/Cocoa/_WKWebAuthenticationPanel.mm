/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#import "_WKWebAuthenticationPanelInternal.h"


#import "LocalAuthenticator.h"
#import "LocalService.h"
#import "Logging.h"
#import "WKError.h"
#import "WebAuthenticationPanelClient.h"
#import "_WKAuthenticationExtensionsClientInputs.h"
#import "_WKAuthenticationExtensionsClientOutputsInternal.h"
#import "_WKAuthenticatorAssertionResponseInternal.h"
#import "_WKAuthenticatorAttestationResponseInternal.h"
#import "_WKAuthenticatorSelectionCriteria.h"
#import "_WKPublicKeyCredentialCreationOptions.h"
#import "_WKPublicKeyCredentialDescriptor.h"
#import "_WKPublicKeyCredentialParameters.h"
#import "_WKPublicKeyCredentialRequestOptions.h"
#import "_WKPublicKeyCredentialRelyingPartyEntity.h"
#import "_WKPublicKeyCredentialUserEntity.h"
#import <WebCore/AuthenticationExtensionsClientInputs.h>
#import <WebCore/AuthenticatorAttachment.h>
#import <WebCore/AuthenticatorResponse.h>
#import <WebCore/AuthenticatorResponseData.h>
#import <WebCore/AuthenticatorSelectionCriteria.h>
#import <WebCore/BufferSource.h>
#import <WebCore/CBORReader.h>
#import <WebCore/CBORWriter.h>
#import <WebCore/CredentialRequestOptions.h>
#import <WebCore/DeviceRequestConverter.h>
#import <WebCore/FidoConstants.h>
#import <WebCore/MediationRequirement.h>
#import <WebCore/MockWebAuthenticationConfiguration.h>
#import <WebCore/PublicKeyCredentialCreationOptions.h>
#import <WebCore/PublicKeyCredentialRequestOptions.h>
#import <WebCore/WebAuthenticationConstants.h>
#import <WebCore/WebAuthenticationUtils.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <objc/runtime.h>
#import <pal/crypto/CryptoDigest.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/Base64.h>

#if ENABLE(WEB_AUTHN)

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/LocalAuthenticatorAdditions.h>
#else
static void updateQueryIfNecessary(NSMutableDictionary *)
{
}
static inline void updateCredentialIfNecessary(NSMutableDictionary *credential, NSDictionary *attributes)
{
}
static inline void updateQueryForGroupIfNecessary(NSMutableDictionary *dictionary, NSString *group)
{
}
#endif

static RetainPtr<NSData> produceClientDataJson(_WKWebAuthenticationType type, NSData *challenge, NSString *origin, NSString *topOrigin = nil, WebAuthn::Scope scope = WebAuthn::Scope::SameOrigin)
{
    WebCore::ClientDataType clientDataType;
    switch (type) {
    case _WKWebAuthenticationTypeCreate:
        clientDataType = WebCore::ClientDataType::Create;
        break;
    case _WKWebAuthenticationTypeGet:
        clientDataType = WebCore::ClientDataType::Get;
        break;
    }
    auto challengeBuffer = ArrayBuffer::tryCreate(span(challenge));
    auto securityOrigin = WebCore::SecurityOrigin::createFromString(origin);

    auto clientDataJson = buildClientDataJson(clientDataType, WebCore::BufferSource(challengeBuffer), securityOrigin, scope, topOrigin);
    return toNSData(clientDataJson->span());
}

static Vector<uint8_t> produceClientDataJsonHash(NSData *clientDataJson)
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(span(clientDataJson));
    return crypto->computeHash();
}
#endif

NSString * const _WKLocalAuthenticatorCredentialNameKey = @"_WKLocalAuthenticatorCredentialNameKey";
NSString * const _WKLocalAuthenticatorCredentialDisplayNameKey = @"_WKLocalAuthenticatorCredentialDisplayNameKey";
NSString * const _WKLocalAuthenticatorCredentialIDKey = @"_WKLocalAuthenticatorCredentialIDKey";
NSString * const _WKLocalAuthenticatorCredentialRelyingPartyIDKey = @"_WKLocalAuthenticatorCredentialRelyingPartyIDKey";
NSString * const _WKLocalAuthenticatorCredentialLastModificationDateKey = @"_WKLocalAuthenticatorCredentialLastModificationDateKey";
NSString * const _WKLocalAuthenticatorCredentialCreationDateKey = @"_WKLocalAuthenticatorCredentialCreationDateKey";
NSString * const _WKLocalAuthenticatorCredentialGroupKey = @"_WKLocalAuthenticatorCredentialGroupKey";
NSString * const _WKLocalAuthenticatorCredentialSynchronizableKey = @"_WKLocalAuthenticatorCredentialSynchronizableKey";
NSString * const _WKLocalAuthenticatorCredentialUserHandleKey = @"_WKLocalAuthenticatorCredentialUserHandleKey";
NSString * const _WKLocalAuthenticatorCredentialLastUsedDateKey = @"_WKLocalAuthenticatorCredentialLastUsedDateKey";

@implementation _WKWebAuthenticationPanel {
#if ENABLE(WEB_AUTHN)
    WeakPtr<WebKit::WebAuthenticationPanelClient> _client;
    RetainPtr<NSMutableSet> _transports;
#endif
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

#if ENABLE(WEB_AUTHN)
    API::Object::constructInWrapper<API::WebAuthenticationPanel>(self);
#endif
    return self;
}

#if ENABLE(WEB_AUTHN)

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKWebAuthenticationPanel.class, self))
        return;

    _panel->~WebAuthenticationPanel();

    [super dealloc];
}

- (id <_WKWebAuthenticationPanelDelegate>)delegate
{
    RefPtr client = _client.get();
    if (!client)
        return nil;
    return client->delegate().autorelease();
}

- (void)setDelegate:(id<_WKWebAuthenticationPanelDelegate>)delegate
{
    Ref client = WebKit::WebAuthenticationPanelClient::create(self, delegate);
    _client = client.ptr();
    _panel->setClient(WTFMove(client));
}


- (NSString *)relyingPartyID
{
    return _panel->rpId();
}

static _WKWebAuthenticationTransport wkWebAuthenticationTransport(WebCore::AuthenticatorTransport transport)
{
    switch (transport) {
    case WebCore::AuthenticatorTransport::Usb:
        return _WKWebAuthenticationTransportUSB;
    case WebCore::AuthenticatorTransport::Nfc:
        return _WKWebAuthenticationTransportNFC;
    case WebCore::AuthenticatorTransport::Ble:
        return _WKWebAuthenticationTransportBLE;
    case WebCore::AuthenticatorTransport::Internal:
        return _WKWebAuthenticationTransportInternal;
    case WebCore::AuthenticatorTransport::Cable:
        return _WKWebAuthenticationTransportCaBLE;
    case WebCore::AuthenticatorTransport::Hybrid:
        return _WKWebAuthenticationTransportHybrid;
    case WebCore::AuthenticatorTransport::SmartCard:
        return _WKWebAuthenticationTransportSmartCard;
    }
}

- (NSSet *)transports
{
    if (_transports)
        return _transports.get();

    auto& transports = _panel->transports();
    _transports = [[NSMutableSet alloc] initWithCapacity:transports.size()];
    for (auto& transport : transports)
        [_transports addObject:adoptNS([[NSNumber alloc] initWithInt:wkWebAuthenticationTransport(transport)]).get()];
    return _transports.get();
}

static _WKWebAuthenticationType wkWebAuthenticationType(WebCore::ClientDataType type)
{
    switch (type) {
    case WebCore::ClientDataType::Create:
        return _WKWebAuthenticationTypeCreate;
    case WebCore::ClientDataType::Get:
        return _WKWebAuthenticationTypeGet;
    default:
        ASSERT_NOT_REACHED();
        return _WKWebAuthenticationTypeCreate;
    }
}

static fido::AuthenticatorSupportedOptions::UserVerificationAvailability coreUserVerificationAvailability(_WKWebAuthenticationUserVerificationAvailability wkAvailability)
{
    switch (wkAvailability) {
    case _WKWebAuthenticationUserVerificationAvailabilitySupportedAndConfigured:
        return fido::AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured;
    case _WKWebAuthenticationUserVerificationAvailabilitySupportedButNotConfigured:
        return fido::AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedButNotConfigured;
    case _WKWebAuthenticationUserVerificationAvailabilityNotSupported:
        return fido::AuthenticatorSupportedOptions::UserVerificationAvailability::kNotSupported;
    }

    ASSERT_NOT_REACHED();
    return fido::AuthenticatorSupportedOptions::UserVerificationAvailability::kNotSupported;
}

- (_WKWebAuthenticationType)type
{
    return wkWebAuthenticationType(_panel->clientDataType());
}

- (NSString *)userName
{
    return _panel->userName();
}

#else // ENABLE(WEB_AUTHN)
- (id <_WKWebAuthenticationPanelDelegate>)delegate
{
    return nil;
}
- (void)setDelegate:(id<_WKWebAuthenticationPanelDelegate>)delegate
{
}
#endif // ENABLE(WEB_AUTHN)

#if ENABLE(WEB_AUTHN)
static RetainPtr<NSArray> getAllLocalAuthenticatorCredentialsImpl(NSString *accessGroup, NSString *byRpId, NSData *byCredentialId)
{
    BOOL useAlternateKeychainAttribute = WebKit::shouldUseAlternateKeychainAttribute();
    auto query = adoptNS([[NSMutableDictionary alloc] init]);
    [query setDictionary:@{
        (__bridge id)kSecClass: bridge_id_cast(kSecClassKey),
        (__bridge id)kSecAttrKeyClass: bridge_id_cast(kSecAttrKeyClassPrivate),
        (__bridge id)kSecAttrAccessGroup: accessGroup,
        (__bridge id)kSecReturnAttributes: @YES,
        (__bridge id)kSecMatchLimit: bridge_id_cast(kSecMatchLimitAll),
        (__bridge id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
        (__bridge id)kSecUseDataProtectionKeychain: @YES
    }];
    if (byRpId)
        [query setObject:byRpId forKey:bridge_cast(kSecAttrLabel)];
    if (byCredentialId) {
        CFStringRef credentialIdKey = useAlternateKeychainAttribute ? kSecAttrAlias : kSecAttrApplicationLabel;
        [query setObject:byCredentialId forKey:bridge_cast(credentialIdKey)];
    }

    CFTypeRef attributesArrayRef = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query.get(), &attributesArrayRef);
    if (byCredentialId && useAlternateKeychainAttribute && status == errSecItemNotFound) {
        [query removeObjectForKey:bridge_cast(kSecAttrAlias)];
        [query setObject:byCredentialId forKey:bridge_cast(kSecAttrApplicationLabel)];
        status = SecItemCopyMatching((__bridge CFDictionaryRef)query.get(), &attributesArrayRef);
    }

    if (status && status != errSecItemNotFound)
        return nullptr;

    auto retainAttributesArray = adoptCF(attributesArrayRef);

    auto result = adoptNS([[NSMutableArray alloc] init]);
    for (NSDictionary *attributes in (NSArray *)attributesArrayRef) {
        auto decodedResponse = cbor::CBORReader::read(makeVector(attributes[bridge_id_cast(kSecAttrApplicationTag)]));
        if (!decodedResponse || !decodedResponse->isMap()) {
            ASSERT_NOT_REACHED();
            return nullptr;
        }
        auto& responseMap = decodedResponse->getMap();

        auto it = responseMap.find(cbor::CBORValue(fido::kEntityNameMapKey));
        if (it == responseMap.end() || !it->second.isString()) {
            ASSERT_NOT_REACHED();
            return nullptr;
        }
        auto& username = it->second.getString();

        id credentialID = attributes[bridge_cast(kSecAttrAlias)];
        if (!credentialID)
            credentialID = attributes[bridge_cast(kSecAttrApplicationLabel)];
        auto credential = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:
            username, _WKLocalAuthenticatorCredentialNameKey,
            credentialID, _WKLocalAuthenticatorCredentialIDKey,
            attributes[bridge_cast(kSecAttrLabel)], _WKLocalAuthenticatorCredentialRelyingPartyIDKey,
            attributes[bridge_cast(kSecAttrModificationDate)], _WKLocalAuthenticatorCredentialLastModificationDateKey,
            attributes[bridge_cast(kSecAttrCreationDate)], _WKLocalAuthenticatorCredentialCreationDateKey,
            nil
        ]);
        it = responseMap.find(cbor::CBORValue(fido::kEntityIdMapKey));
        if (it != responseMap.end() && it->second.isByteString()) {
            auto& userHandle = it->second.getByteString();
            [credential setObject:toNSData(userHandle.span()).get() forKey:_WKLocalAuthenticatorCredentialUserHandleKey];
        } else
            [credential setObject:[NSNull null] forKey:_WKLocalAuthenticatorCredentialUserHandleKey];

        updateCredentialIfNecessary(credential.get(), attributes);

        it = responseMap.find(cbor::CBORValue(fido::kDisplayNameMapKey));
        if (it != responseMap.end() && it->second.isString())
            [credential setObject:it->second.getString() forKey:_WKLocalAuthenticatorCredentialDisplayNameKey];

        [result addObject:credential.get()];
    }

    return result;
}
#endif

+ (NSArray<NSDictionary *> *)getAllLocalAuthenticatorCredentials
{
#if ENABLE(WEB_AUTHN)
    return getAllLocalAuthenticatorCredentialsImpl(WebCore::LocalAuthenticatorAccessGroup, nil, nil).autorelease();
#else
    return nullptr;
#endif
}

+ (NSArray<NSDictionary *> *)getAllLocalAuthenticatorCredentialsWithAccessGroup:(NSString *)accessGroup
{
#if ENABLE(WEB_AUTHN)
    return getAllLocalAuthenticatorCredentialsImpl(accessGroup, nil, nil).autorelease();
#else
    return nullptr;
#endif
}

+ (NSArray<NSDictionary *> *)getAllLocalAuthenticatorCredentialsWithRPID:(NSString *)rpID
{
#if ENABLE(WEB_AUTHN)
    return getAllLocalAuthenticatorCredentialsImpl(WebCore::LocalAuthenticatorAccessGroup, rpID, nil).autorelease();
#else
    return nullptr;
#endif
}

+ (NSArray<NSDictionary *> *)getAllLocalAuthenticatorCredentialsWithCredentialID:(NSData *)credentialID
{
#if ENABLE(WEB_AUTHN)
    return getAllLocalAuthenticatorCredentialsImpl(WebCore::LocalAuthenticatorAccessGroup, nil, credentialID).autorelease();
#else
    return nullptr;
#endif
}

+ (NSArray<NSDictionary *> *)getAllLocalAuthenticatorCredentialsWithRPIDAndAccessGroup:(NSString *)accessGroup rpID:(NSString *)rpID
{
#if ENABLE(WEB_AUTHN)
    return getAllLocalAuthenticatorCredentialsImpl(accessGroup, rpID, nil).autorelease();
#else
    return nullptr;
#endif
}

+ (NSArray<NSDictionary *> *)getAllLocalAuthenticatorCredentialsWithCredentialIDAndAccessGroup:(NSString *)accessGroup credentialID:(NSData *)credentialID
{
#if ENABLE(WEB_AUTHN)
    return getAllLocalAuthenticatorCredentialsImpl(accessGroup, nil, credentialID).autorelease();
#else
    return nullptr;
#endif
}


+ (void)deleteLocalAuthenticatorCredentialWithID:(NSData *)credentialID
{
    [self deleteLocalAuthenticatorCredentialWithGroupAndID:nil credential:credentialID];
}

+ (void)deleteLocalAuthenticatorCredentialWithGroupAndID:(NSString *)group credential:(NSData *)credentialID
{
#if ENABLE(WEB_AUTHN)
    BOOL useAlternateKeychainAttribute = WebKit::shouldUseAlternateKeychainAttribute();
    auto deleteQuery = adoptNS([[NSMutableDictionary alloc] init]);
    [deleteQuery setDictionary:@{
        (__bridge id)kSecClass: bridge_id_cast(kSecClassKey),
        (__bridge id)kSecUseDataProtectionKeychain: @YES,
        (__bridge id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny
    }];
    updateQueryForGroupIfNecessary(deleteQuery.get(), group);

    CFStringRef credentialIdKey = useAlternateKeychainAttribute ? kSecAttrAlias : kSecAttrApplicationLabel;
    [deleteQuery setObject:credentialID forKey:(__bridge id)credentialIdKey];

    SecItemDelete((__bridge CFDictionaryRef)deleteQuery.get());

    if (useAlternateKeychainAttribute) {
        [deleteQuery removeObjectForKey:bridge_cast(kSecAttrAlias)];
        [deleteQuery setObject:credentialID forKey:bridge_cast(kSecAttrApplicationLabel)];
        SecItemDelete((__bridge CFDictionaryRef)deleteQuery.get());
    }
#endif
}

+ (void)clearAllLocalAuthenticatorCredentials
{
#if ENABLE(WEB_AUTHN)
    WebKit::LocalAuthenticator::clearAllCredentials();
#endif
}

+ (void)setDisplayNameForLocalCredentialWithGroupAndID:(NSString *)group credential:(NSData *)credentialID displayName: (NSString *)displayName
{
#if ENABLE(WEB_AUTHN)
    BOOL useAlternateKeychainAttribute = WebKit::shouldUseAlternateKeychainAttribute();
    auto query = adoptNS([[NSMutableDictionary alloc] init]);
    [query setDictionary:@{
        (__bridge id)kSecClass: bridge_id_cast(kSecClassKey),
        (__bridge id)kSecReturnAttributes: @YES,
        (__bridge id)kSecReturnPersistentRef : bridge_id_cast(kCFBooleanTrue),
        (__bridge id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
        (__bridge id)kSecUseDataProtectionKeychain: @YES
    }];
    updateQueryForGroupIfNecessary(query.get(), group);

    CFStringRef credentialIdKey = useAlternateKeychainAttribute ? kSecAttrAlias : kSecAttrApplicationLabel;
    [query setObject:credentialID forKey:(__bridge id)credentialIdKey];

    CFTypeRef attributesArrayRef = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query.get(), &attributesArrayRef);
    if (useAlternateKeychainAttribute && status == errSecItemNotFound) {
        [query removeObjectForKey:bridge_cast(kSecAttrAlias)];
        [query setObject:credentialID forKey:bridge_cast(kSecAttrApplicationLabel)];
        status = SecItemCopyMatching((__bridge CFDictionaryRef)query.get(), &attributesArrayRef);
    }

    if (status && status != errSecItemNotFound) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto attributes = adoptNS((__bridge NSDictionary *)attributesArrayRef);
    auto decodedResponse = cbor::CBORReader::read(makeVector(attributes.get()[bridge_id_cast(kSecAttrApplicationTag)]));
    if (!decodedResponse || !decodedResponse->isMap()) {
        ASSERT_NOT_REACHED();
        return;
    }
    auto& previousUserMap = decodedResponse->getMap();

    bool nameSet = false;
    cbor::CBORValue::MapValue updatedUserMap;
    for (auto it = previousUserMap.begin(); it != previousUserMap.end(); ++it) {
        if (it->first.isString() && it->first.getString() == fido::kDisplayNameMapKey) {
            if (displayName)
                updatedUserMap[it->first.clone()] = cbor::CBORValue(String(displayName));
            nameSet = true;
        } else
            updatedUserMap[it->first.clone()] = it->second.clone();
    }
    if (!nameSet && displayName)
        updatedUserMap[cbor::CBORValue(fido::kDisplayNameMapKey)] = cbor::CBORValue(String(displayName));
    auto updatedTag = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(updatedUserMap)));

    RetainPtr secAttrApplicationTag = toNSData(updatedTag->span());

    NSDictionary *updateParams = @{
        (__bridge id)kSecAttrApplicationTag: secAttrApplicationTag.get(),
    };

    [query setDictionary:@{
        (__bridge id)kSecValuePersistentRef: [attributes objectForKey:bridge_id_cast(kSecValuePersistentRef)],
        (__bridge id)kSecClass: bridge_id_cast(kSecClassKey),
        (__bridge id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
    }];
    updateQueryForGroupIfNecessary(query.get(), group);

    status = SecItemUpdate((__bridge CFDictionaryRef)query.get(), (__bridge CFDictionaryRef)updateParams);
    if (status && status != errSecItemNotFound) {
        ASSERT_NOT_REACHED();
        return;
    }
#endif
}

+ (void)setNameForLocalCredentialWithGroupAndID:(NSString *)group credential:(NSData *)credentialID name:(NSString *)name
{
#if ENABLE(WEB_AUTHN)
    BOOL useAlternateKeychainAttribute = WebKit::shouldUseAlternateKeychainAttribute();
    auto query = adoptNS([[NSMutableDictionary alloc] init]);
    [query setDictionary:@{
        (__bridge id)kSecClass: bridge_id_cast(kSecClassKey),
        (__bridge id)kSecReturnAttributes: @YES,
        (__bridge id)kSecReturnPersistentRef : bridge_id_cast(kCFBooleanTrue),
        (__bridge id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
        (__bridge id)kSecUseDataProtectionKeychain: @YES
    }];
    updateQueryForGroupIfNecessary(query.get(), group);

    CFStringRef credentialIdKey = useAlternateKeychainAttribute ? kSecAttrAlias : kSecAttrApplicationLabel;
    [query setObject:credentialID forKey:(__bridge id)credentialIdKey];

    CFTypeRef attributesArrayRef = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query.get(), &attributesArrayRef);
    if (useAlternateKeychainAttribute && status == errSecItemNotFound) {
        [query removeObjectForKey:(__bridge id)kSecAttrAlias];
        [query setObject:credentialID forKey:(__bridge id)kSecAttrApplicationLabel];
        status = SecItemCopyMatching((__bridge CFDictionaryRef)query.get(), &attributesArrayRef);
    }

    if (status && status != errSecItemNotFound) {
        ASSERT_NOT_REACHED();
        return;
    }
    auto attributes = adoptNS((__bridge NSDictionary *)attributesArrayRef);
    auto decodedResponse = cbor::CBORReader::read(makeVector(attributes.get()[bridge_id_cast(kSecAttrApplicationTag)]));
    if (!decodedResponse || !decodedResponse->isMap()) {
        ASSERT_NOT_REACHED();
        return;
    }
    auto& previousUserMap = decodedResponse->getMap();

    bool nameSet = false;
    cbor::CBORValue::MapValue updatedUserMap;
    for (auto it = previousUserMap.begin(); it != previousUserMap.end(); ++it) {
        if (it->first.isString() && it->first.getString() == fido::kEntityNameMapKey) {
            if (name)
                updatedUserMap[it->first.clone()] = cbor::CBORValue(String(name));
            nameSet = true;
        } else
            updatedUserMap[it->first.clone()] = it->second.clone();
    }
    if (!nameSet && name)
        updatedUserMap[cbor::CBORValue(fido::kEntityNameMapKey)] = cbor::CBORValue(String(name));
    auto updatedTag = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(updatedUserMap)));

    RetainPtr secAttrApplicationTag = toNSData(updatedTag->span());

    NSDictionary *updateParams = @{
        (__bridge id)kSecAttrApplicationTag: secAttrApplicationTag.get(),
    };

    [query setDictionary:@{
        (__bridge id)kSecValuePersistentRef: [attributes objectForKey:bridge_id_cast(kSecValuePersistentRef)],
        (__bridge id)kSecClass: bridge_id_cast(kSecClassKey),
        (__bridge id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
    }];
    updateQueryForGroupIfNecessary(query.get(), group);

    status = SecItemUpdate((__bridge CFDictionaryRef)query.get(), (__bridge CFDictionaryRef)updateParams);
    if (status && status != errSecItemNotFound) {
        ASSERT_NOT_REACHED();
        return;
    }
#endif
}

#if ENABLE(WEB_AUTHN)
static void createNSErrorFromWKErrorIfNecessary(NSError **error, WKErrorCode errorCode)
{
    if (error)
        *error = [NSError errorWithDomain:WKErrorDomain code: errorCode userInfo:nil];
}
#endif // ENABLE(WEB_AUTHN)

+ (NSData *)importLocalAuthenticatorCredential:(NSData *)credentialBlob error:(NSError **)error
{
    return [self importLocalAuthenticatorWithAccessGroup:WebCore::LocalAuthenticatorAccessGroup credential:credentialBlob error:error];
}

+ (NSData *)importLocalAuthenticatorWithAccessGroup:(NSString *)accessGroup credential:(NSData *)credentialBlob error:(NSError **)error
{
#if ENABLE(WEB_AUTHN)
    auto credential = cbor::CBORReader::read(makeVector(credentialBlob));
    if (!credential || !credential->isMap()) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorMalformedCredential);
        return nullptr;
    }

    auto& credentialMap = credential->getMap();
    auto it = credentialMap.find(cbor::CBORValue(WebCore::privateKeyKey));
    if (it == credentialMap.end() || !it->second.isByteString()) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorMalformedCredential);
        return nullptr;
    }
    RetainPtr privateKey = toNSData(it->second.getByteString().span());

    it = credentialMap.find(cbor::CBORValue(WebCore::keyTypeKey));
    if (it == credentialMap.end() || !it->second.isInteger()) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorMalformedCredential);
        return nullptr;
    }
    auto keyType = it->second.getInteger();

    it = credentialMap.find(cbor::CBORValue(WebCore::keySizeKey));
    if (it == credentialMap.end() || !it->second.isInteger()) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorMalformedCredential);
        return nullptr;
    }
    auto keySize = it->second.getInteger();

    it = credentialMap.find(cbor::CBORValue(WebCore::relyingPartyKey));
    if (it == credentialMap.end() || !it->second.isString()) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorMalformedCredential);
        return nullptr;
    }
    auto rp = it->second.getString();

    it = credentialMap.find(cbor::CBORValue(WebCore::applicationTagKey));
    if (it == credentialMap.end() || !it->second.isMap()) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorMalformedCredential);
        return nullptr;
    }
    auto keyTag = cbor::CBORWriter::write(cbor::CBORValue(it->second.getMap()));

    NSDictionary *options = @{
        // Key type values are string values of numbers, stored as kCFNumberSInt64Type in attributes, but must be passed as string here
        (id)kSecAttrKeyType: (id)[NSString stringWithFormat:@"%i", (int)keyType],
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrKeySizeInBits: @(keySize),
    };
    CFErrorRef errorRef = nullptr;
    auto key = adoptCF(SecKeyCreateWithData(
        bridge_cast(privateKey.get()),
        bridge_cast(options),
        &errorRef
    ));
    if (errorRef) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorMalformedCredential);
        return nullptr;
    }

    auto publicKey = adoptCF(SecKeyCopyPublicKey(key.get()));
    auto publicKeyDataRep = adoptCF(SecKeyCopyExternalRepresentation(publicKey.get(), &errorRef));
    if (errorRef) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorMalformedCredential);
        return nullptr;
    }

    NSData *nsPublicKeyData = (NSData *)publicKeyDataRep.get();
    auto digest = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_1);
    digest->addBytes(span(nsPublicKeyData));
    auto credentialId = digest->computeHash();
    RetainPtr nsCredentialId = toNSData(credentialId.span());

    BOOL useAlternateKeychainAttribute = WebKit::shouldUseAlternateKeychainAttribute();
    auto query = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:
        (id)kSecClassKey, (id)kSecClass,
        (id)kSecAttrKeyClassPrivate, (id)kSecAttrKeyClass,
        (id)rp, (id)kSecAttrLabel,
        @YES, (id)kSecUseDataProtectionKeychain,
        nil
    ]);
    updateQueryIfNecessary(query.get());

    CFStringRef credentialIdKey = useAlternateKeychainAttribute ? kSecAttrAlias : kSecAttrApplicationLabel;
    [query setObject:nsCredentialId.get() forKey:(id)credentialIdKey];

    if (accessGroup != nil)
        [query setObject:accessGroup forKey:(__bridge id)kSecAttrAccessGroup];

    OSStatus status = SecItemCopyMatching(bridge_cast(query.get()), nullptr);
    if (useAlternateKeychainAttribute && status == errSecItemNotFound) {
        [query removeObjectForKey:(id)kSecAttrAlias];
        [query setObject:nsCredentialId.get() forKey:(id)kSecAttrApplicationLabel];
        status = SecItemCopyMatching(bridge_cast(query.get()), nullptr);
    }

    if (!status) {
        // Credential with same id already exists, duplicate key.
        createNSErrorFromWKErrorIfNecessary(error, WKErrorDuplicateCredential);
        return nullptr;
    }

    RetainPtr secAttrApplicationTag = toNSData(keyTag->span());

    auto addQuery = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:
        (id)key.get(), (id)kSecValueRef,
        (id)kSecAttrKeyClassPrivate, (id)kSecAttrKeyClass,
        (id)rp, (id)kSecAttrLabel,
        secAttrApplicationTag.get(), (id)kSecAttrApplicationTag,
        @YES, (id)kSecUseDataProtectionKeychain,
        (id)kSecAttrAccessibleAfterFirstUnlock, (id)kSecAttrAccessible,
        nil
    ]);
    updateQueryIfNecessary(addQuery.get());

    if (accessGroup != nil)
        [addQuery setObject:accessGroup forKey:(__bridge id)kSecAttrAccessGroup];

    status = SecItemAdd(bridge_cast(addQuery.get()), NULL);
    if (status) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorUnknown);
        return nullptr;
    }

    return nsCredentialId.autorelease();
#else
    return nullptr;
#endif // ENABLE(WEB_AUTHN)
}

+ (NSData *)exportLocalAuthenticatorCredentialWithID:(NSData *)credentialID error:(NSError **)error
{
    return [self exportLocalAuthenticatorCredentialWithGroupAndID:nil credential:credentialID error:error];
}

+ (NSData *)exportLocalAuthenticatorCredentialWithGroupAndID:(NSString *)group credential:(NSData *)credentialID error:(NSError **)error
{
#if ENABLE(WEB_AUTHN)
    BOOL useAlternateKeychainAttribute = WebKit::shouldUseAlternateKeychainAttribute();
    auto query = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:
        (id)kSecClassKey, (id)kSecClass,
        (id)kSecAttrKeyClassPrivate, (id)kSecAttrKeyClass,
        @YES, (id)kSecReturnRef,
        @YES, (id)kSecUseDataProtectionKeychain,
        (id)kSecAttrSynchronizableAny, (id)kSecAttrSynchronizable,
        nil
    ]);
    updateQueryForGroupIfNecessary(query.get(), group);

    CFStringRef credentialIdKey = useAlternateKeychainAttribute ? kSecAttrAlias : kSecAttrApplicationLabel;
    [query setObject:credentialID forKey:(id)credentialIdKey];

    CFTypeRef privateKeyRef = nullptr;
    OSStatus status = SecItemCopyMatching(bridge_cast(query.get()), &privateKeyRef);
    if (useAlternateKeychainAttribute && status == errSecItemNotFound) {
        [query removeObjectForKey:bridge_cast(kSecAttrAlias)];
        [query setObject:credentialID forKey:bridge_cast(kSecAttrApplicationLabel)];
        status = SecItemCopyMatching(bridge_cast(query.get()), &privateKeyRef);
    }

    if (status && status != errSecItemNotFound) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorCredentialNotFound);
        return nullptr;
    }

    auto privateKey = adoptCF(privateKeyRef);
    CFErrorRef errorRef = nullptr;
    auto privateKeyRep = adoptCF(SecKeyCopyExternalRepresentation((__bridge SecKeyRef)((id)privateKeyRef), &errorRef));
    auto retainError = adoptCF(errorRef);
    if (errorRef) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorCredentialNotFound);
        return nullptr;
    }
        
    [query removeObjectForKey:(id)kSecReturnRef];
    [query setObject: @YES forKey:(id)kSecReturnAttributes];
    CFTypeRef attributesArrayRef = nullptr;
    status = SecItemCopyMatching(bridge_cast(query.get()), &attributesArrayRef);
    if (status && status != errSecItemNotFound) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorCredentialNotFound);
        return nullptr;
    }
    auto attributes = adoptNS((__bridge NSDictionary *)attributesArrayRef);

    int64_t keyType, keySize;
    if (!CFNumberGetValue((__bridge CFNumberRef)attributes.get()[bridge_id_cast(kSecAttrKeyType)], kCFNumberSInt64Type, &keyType)) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorMalformedCredential);
        return nullptr;
    }
    if (!CFNumberGetValue((__bridge CFNumberRef)attributes.get()[bridge_id_cast(kSecAttrKeySizeInBits)], kCFNumberSInt64Type, &keySize)) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorMalformedCredential);
        return nullptr;
    }
    
    cbor::CBORValue::MapValue credentialMap;
    credentialMap[cbor::CBORValue(WebCore::privateKeyKey)] = cbor::CBORValue(WebCore::toBufferSource(bridge_id_cast(privateKeyRep.get())));
    credentialMap[cbor::CBORValue(WebCore::keyTypeKey)] = cbor::CBORValue(keyType);
    credentialMap[cbor::CBORValue(WebCore::keySizeKey)] = cbor::CBORValue(keySize);
    credentialMap[cbor::CBORValue(WebCore::relyingPartyKey)] = cbor::CBORValue(String(attributes.get()[bridge_id_cast(kSecAttrLabel)]));
    auto decodedResponse = cbor::CBORReader::read(makeVector(attributes.get()[bridge_id_cast(kSecAttrApplicationTag)]));
    if (!decodedResponse || !decodedResponse->isMap()) {
        createNSErrorFromWKErrorIfNecessary(error, WKErrorMalformedCredential);
        return nullptr;
    }
    credentialMap[cbor::CBORValue(WebCore::applicationTagKey)] = cbor::CBORValue(WTFMove(*decodedResponse));
    auto serializedCredential = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(credentialMap)));
    return toNSData(serializedCredential.value().span()).autorelease();
#else
    return nullptr;
#endif // ENABLE(WEB_AUTHN)
}

- (void)cancel
{
#if ENABLE(WEB_AUTHN)
    _panel->cancel();
#endif
}

#if ENABLE(WEB_AUTHN)

static WebCore::PublicKeyCredentialRpEntity publicKeyCredentialRpEntity(_WKPublicKeyCredentialRelyingPartyEntity *rpEntity)
{
    WebCore::PublicKeyCredentialRpEntity result;
    result.name = rpEntity.name;
    result.icon = rpEntity.icon;
    result.id = rpEntity.identifier;

    return result;
}

static WebCore::PublicKeyCredentialUserEntity publicKeyCredentialUserEntity(_WKPublicKeyCredentialUserEntity *userEntity)
{
    WebCore::PublicKeyCredentialUserEntity result;
    result.name = userEntity.name;
    result.icon = userEntity.icon;
    result.id = WebCore::toBufferSource(userEntity.identifier).variant();
    result.displayName = userEntity.displayName;

    return result;
}

static Vector<WebCore::PublicKeyCredentialParameters> publicKeyCredentialParameters(NSArray<_WKPublicKeyCredentialParameters *> *publicKeyCredentialParamaters)
{
    return Vector<WebCore::PublicKeyCredentialParameters>(publicKeyCredentialParamaters.count, [publicKeyCredentialParamaters](size_t i) {
        _WKPublicKeyCredentialParameters *param = publicKeyCredentialParamaters[i];
        return WebCore::PublicKeyCredentialParameters { WebCore::PublicKeyCredentialType::PublicKey, param.algorithm.longLongValue };
    });
}

static WebCore::AuthenticatorTransport authenticatorTransport(_WKWebAuthenticationTransport transport)
{
    switch (transport) {
    case _WKWebAuthenticationTransportUSB:
        return WebCore::AuthenticatorTransport::Usb;
    case _WKWebAuthenticationTransportNFC:
        return WebCore::AuthenticatorTransport::Nfc;
    case _WKWebAuthenticationTransportBLE:
        return WebCore::AuthenticatorTransport::Ble;
    case _WKWebAuthenticationTransportInternal:
        return WebCore::AuthenticatorTransport::Internal;
    case _WKWebAuthenticationTransportCaBLE:
        return WebCore::AuthenticatorTransport::Cable;
    case _WKWebAuthenticationTransportHybrid:
        return WebCore::AuthenticatorTransport::Hybrid;
    case _WKWebAuthenticationTransportSmartCard:
        return WebCore::AuthenticatorTransport::SmartCard;
    }
}

static Vector<WebCore::AuthenticatorTransport> authenticatorTransports(NSArray<NSNumber *> *transports)
{
    return Vector<WebCore::AuthenticatorTransport>(transports.count, [transports](size_t i) {
        NSNumber *transport = transports[i];
        return authenticatorTransport((_WKWebAuthenticationTransport)transport.intValue);
    });
}

static Vector<WebCore::PublicKeyCredentialDescriptor> publicKeyCredentialDescriptors(NSArray<_WKPublicKeyCredentialDescriptor *> *credentials)
{
    return Vector<WebCore::PublicKeyCredentialDescriptor>(credentials.count, [credentials](size_t i) {
        _WKPublicKeyCredentialDescriptor *credential = credentials[i];
        return WebCore::PublicKeyCredentialDescriptor { WebCore::PublicKeyCredentialType::PublicKey, WebCore::toBufferSource(credential.identifier).variant(), authenticatorTransports(credential.transports) };
    });
}

static std::optional<WebCore::AuthenticatorAttachment> authenticatorAttachment(_WKAuthenticatorAttachment attachment)
{
    switch (attachment) {
    case _WKAuthenticatorAttachmentAll:
        return std::nullopt;
    case _WKAuthenticatorAttachmentPlatform:
        return WebCore::AuthenticatorAttachment::Platform;
    case _WKAuthenticatorAttachmentCrossPlatform:
        return WebCore::AuthenticatorAttachment::CrossPlatform;
    default:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
}

static WebCore::UserVerificationRequirement userVerification(_WKUserVerificationRequirement uv)
{
    switch (uv) {
    case _WKUserVerificationRequirementRequired:
        return WebCore::UserVerificationRequirement::Required;
    case _WKUserVerificationRequirementPreferred:
        return WebCore::UserVerificationRequirement::Preferred;
    case _WKUserVerificationRequirementDiscouraged:
        return WebCore::UserVerificationRequirement::Discouraged;
    default:
        ASSERT_NOT_REACHED();
        return WebCore::UserVerificationRequirement::Preferred;
    }
}

static std::optional<WebCore::ResidentKeyRequirement> toWebCore(_WKResidentKeyRequirement uv)
{
    switch (uv) {
    case _WKResidentKeyRequirementNotPresent:
        return std::nullopt;
    case _WKResidentKeyRequirementRequired:
        return WebCore::ResidentKeyRequirement::Required;
    case _WKResidentKeyRequirementPreferred:
        return WebCore::ResidentKeyRequirement::Preferred;
    case _WKResidentKeyRequirementDiscouraged:
        return WebCore::ResidentKeyRequirement::Discouraged;
    default:
        ASSERT_NOT_REACHED();
        return WebCore::ResidentKeyRequirement::Preferred;
    }
}

static WebCore::AuthenticatorSelectionCriteria authenticatorSelectionCriteria(_WKAuthenticatorSelectionCriteria *authenticatorSelection)
{
    WebCore::AuthenticatorSelectionCriteria result;
    result.authenticatorAttachment = authenticatorAttachment(authenticatorSelection.authenticatorAttachment);
    result.residentKey = toWebCore(authenticatorSelection.residentKey);
    result.requireResidentKey = authenticatorSelection.requireResidentKey;
    result.userVerification = userVerification(authenticatorSelection.userVerification);

    return result;
}

static WebCore::AttestationConveyancePreference attestationConveyancePreference(_WKAttestationConveyancePreference attestation)
{
    switch (attestation) {
    case _WKAttestationConveyancePreferenceNone:
        return WebCore::AttestationConveyancePreference::None;
    case _WKAttestationConveyancePreferenceIndirect:
        return WebCore::AttestationConveyancePreference::Indirect;
    case _WKAttestationConveyancePreferenceDirect:
        return WebCore::AttestationConveyancePreference::Direct;
    case _WKAttestationConveyancePreferenceEnterprise:
        return WebCore::AttestationConveyancePreference::Enterprise;
    default:
        ASSERT_NOT_REACHED();
        return WebCore::AttestationConveyancePreference::None;
    }
}

static WebCore::AuthenticationExtensionsClientInputs authenticationExtensionsClientInputs(_WKAuthenticationExtensionsClientInputs *extensions)
{
    WebCore::AuthenticationExtensionsClientInputs result;
    result.appid = extensions.appid;

    return result;
}

static WebCore::MediationRequirement toWebCore(_WKWebAuthenticationMediationRequirement mediation)
{
    switch (mediation) {
    case _WKWebAuthenticationMediationRequirementSilent:
        return WebCore::MediationRequirement::Silent;
    case _WKWebAuthenticationMediationRequirementOptional:
        return WebCore::MediationRequirement::Optional;
    case _WKWebAuthenticationMediationRequirementRequired:
        return WebCore::MediationRequirement::Required;
    case _WKWebAuthenticationMediationRequirementConditional:
        return WebCore::MediationRequirement::Conditional;
    default:
        ASSERT_NOT_REACHED();
        return WebCore::MediationRequirement::Optional;
    }
}
#endif

+ (WebCore::PublicKeyCredentialCreationOptions)convertToCoreCreationOptionsWithOptions:(_WKPublicKeyCredentialCreationOptions *)options
{
    WebCore::PublicKeyCredentialCreationOptions result;

#if ENABLE(WEB_AUTHN)
    result.rp = publicKeyCredentialRpEntity(options.relyingParty);
    result.user = publicKeyCredentialUserEntity(options.user);

    result.pubKeyCredParams = publicKeyCredentialParameters(options.publicKeyCredentialParamaters);

    if (options.timeout)
        result.timeout = options.timeout.unsignedIntValue;
    if (options.excludeCredentials)
        result.excludeCredentials = publicKeyCredentialDescriptors(options.excludeCredentials);
    if (options.authenticatorSelection)
        result.authenticatorSelection = authenticatorSelectionCriteria(options.authenticatorSelection);
    result.attestation = attestationConveyancePreference(options.attestation);
    if (options.extensionsCBOR)
        result.extensions = WebCore::AuthenticationExtensionsClientInputs::fromCBOR(span(options.extensionsCBOR));
    else
        result.extensions = authenticationExtensionsClientInputs(options.extensions);
#endif

    return result;
}

#if ENABLE(WEB_AUTHN)
static _WKAuthenticatorAttachment authenticatorAttachmentToWKAuthenticatorAttachment(WebCore::AuthenticatorAttachment attachment)
{
    switch (attachment) {
    case WebCore::AuthenticatorAttachment::Platform:
        return _WKAuthenticatorAttachmentPlatform;
    case WebCore::AuthenticatorAttachment::CrossPlatform:
        return _WKAuthenticatorAttachmentCrossPlatform;
    }
}

static RetainPtr<NSArray<NSNumber *>> wkTransports(const Vector<WebCore::AuthenticatorTransport>& transports)
{
    auto wkTransports = adoptNS([NSMutableArray<NSNumber *> new]);
    for (auto transport : transports)
        [wkTransports addObject:[NSNumber numberWithInt:(int)transport]];
    return wkTransports;
}


static RetainPtr<_WKAuthenticatorAttestationResponse> wkAuthenticatorAttestationResponse(const WebCore::AuthenticatorResponseData& data, NSData *clientDataJSON, WebCore::AuthenticatorAttachment attachment)
{
    auto value = adoptNS([[_WKAuthenticatorAttestationResponse alloc] initWithClientDataJSON:clientDataJSON rawId:toNSData(data.rawId->span()).get() extensionOutputsCBOR:toNSData(data.extensionOutputs->toCBOR()).autorelease() attestationObject:toNSData(data.attestationObject->span()).get() attachment: authenticatorAttachmentToWKAuthenticatorAttachment(attachment) transports:wkTransports(data.transports).autorelease()]);
    
    return value;
}
#endif

- (void)makeCredentialWithChallenge:(NSData *)challenge origin:(NSString *)origin options:(_WKPublicKeyCredentialCreationOptions *)options completionHandler:(void (^)(_WKAuthenticatorAttestationResponse *, NSError *))handler
{
#if ENABLE(WEB_AUTHN)
    auto clientDataJSON = produceClientDataJson(_WKWebAuthenticationTypeCreate, challenge, origin);
    auto hash = produceClientDataJsonHash(clientDataJSON.get());
    auto callback = [handler = makeBlockPtr(handler), clientDataJSON = WTFMove(clientDataJSON)] (std::variant<Ref<WebCore::AuthenticatorResponse>, WebCore::ExceptionData>&& result) mutable {
        WTF::switchOn(result, [&](const Ref<WebCore::AuthenticatorResponse>& response) {
            handler(wkAuthenticatorAttestationResponse(response->data(), clientDataJSON.get(), response->attachment()).get(), nil);
        }, [&](const WebCore::ExceptionData& exception) {
            handler(nil, [NSError errorWithDomain:WKErrorDomain code:static_cast<NSInteger>(exception.code) userInfo:@{ NSLocalizedDescriptionKey: exception.message }]);
        });
    };
    _panel->handleRequest({ WTFMove(hash), [_WKWebAuthenticationPanel convertToCoreCreationOptionsWithOptions:options], nullptr, WebKit::WebAuthenticationPanelResult::Unavailable, nullptr, std::nullopt, { }, String(), nullptr, std::nullopt, std::nullopt }, WTFMove(callback));
#endif
}

- (void)makeCredentialWithMediationRequirement:(_WKWebAuthenticationMediationRequirement)mediation clientDataHash:(NSData *)clientDataHash options:(_WKPublicKeyCredentialCreationOptions *)options completionHandler:(void (^)(_WKAuthenticatorAttestationResponse *, NSError *))handler
{
#if ENABLE(WEB_AUTHN)
    auto callback = [handler = makeBlockPtr(handler)] (std::variant<Ref<WebCore::AuthenticatorResponse>, WebCore::ExceptionData>&& result) mutable {
        WTF::switchOn(result, [&](const Ref<WebCore::AuthenticatorResponse>& response) {
            handler(wkAuthenticatorAttestationResponse(response->data(), nullptr, response->attachment()).get(), nil);
        }, [&](const WebCore::ExceptionData& exception) {
            handler(nil, [NSError errorWithDomain:WKErrorDomain code:static_cast<NSInteger>(exception.code) userInfo:@{ NSLocalizedDescriptionKey: exception.message }]);
        });
    };
    _panel->handleRequest({ makeVector(clientDataHash), [_WKWebAuthenticationPanel convertToCoreCreationOptionsWithOptions:options], nullptr, WebKit::WebAuthenticationPanelResult::Unavailable, nullptr, std::nullopt, { }, String(), nullptr, toWebCore(mediation), std::nullopt }, WTFMove(callback));
#endif
}

- (void)makeCredentialWithClientDataHash:(NSData *)clientDataHash options:(_WKPublicKeyCredentialCreationOptions *)options completionHandler:(void (^)(_WKAuthenticatorAttestationResponse *, NSError *))handler
{
    [self makeCredentialWithMediationRequirement:_WKWebAuthenticationMediationRequirementOptional clientDataHash:clientDataHash options:options completionHandler:handler];
}

+ (WebCore::PublicKeyCredentialRequestOptions)convertToCoreRequestOptionsWithOptions:(_WKPublicKeyCredentialRequestOptions *)options
{
    WebCore::PublicKeyCredentialRequestOptions result;

#if ENABLE(WEB_AUTHN)
    if (options.timeout)
        result.timeout = options.timeout.unsignedIntValue;
    if (options.relyingPartyIdentifier)
        result.rpId = options.relyingPartyIdentifier;
    if (options.allowCredentials)
        result.allowCredentials = publicKeyCredentialDescriptors(options.allowCredentials);
    if (options.extensionsCBOR)
        result.extensions = WebCore::AuthenticationExtensionsClientInputs::fromCBOR(span(options.extensionsCBOR));
    else
        result.extensions = authenticationExtensionsClientInputs(options.extensions);
    result.userVerification = userVerification(options.userVerification);
    result.authenticatorAttachment = authenticatorAttachment(options.authenticatorAttachment);
#endif

    return result;
}

#if ENABLE(WEB_AUTHN)
static RetainPtr<_WKAuthenticatorAssertionResponse> wkAuthenticatorAssertionResponse(const WebCore::AuthenticatorResponseData& data, NSData *clientDataJSON, WebCore::AuthenticatorAttachment attachment)
{
    RetainPtr<NSData> userHandle;
    if (data.userHandle)
        userHandle = toNSData(data.userHandle->span());

    return adoptNS([[_WKAuthenticatorAssertionResponse alloc] initWithClientDataJSON:clientDataJSON rawId:toNSData(data.rawId->span()).get() extensionOutputsCBOR:toNSData(data.extensionOutputs->toCBOR()).autorelease() authenticatorData:toNSData(data.authenticatorData->span()).get() signature:toNSData(data.signature->span()).get() userHandle:userHandle.get() attachment:authenticatorAttachmentToWKAuthenticatorAttachment(attachment)]);
}
#endif

- (void)getAssertionWithChallenge:(NSData *)challenge origin:(NSString *)origin options:(_WKPublicKeyCredentialRequestOptions *)options completionHandler:(void (^)(_WKAuthenticatorAssertionResponse *, NSError *))handler
{
#if ENABLE(WEB_AUTHN)
    auto clientDataJSON = produceClientDataJson(_WKWebAuthenticationTypeGet, challenge, origin);
    auto hash = produceClientDataJsonHash(clientDataJSON.get());
    auto callback = [handler = makeBlockPtr(handler), clientDataJSON = WTFMove(clientDataJSON)] (std::variant<Ref<WebCore::AuthenticatorResponse>, WebCore::ExceptionData>&& result) mutable {
        WTF::switchOn(result, [&](const Ref<WebCore::AuthenticatorResponse>& response) {
            handler(wkAuthenticatorAssertionResponse(response->data(), clientDataJSON.get(), response->attachment()).get(), nil);
        }, [&](const WebCore::ExceptionData& exception) {
            handler(nil, [NSError errorWithDomain:WKErrorDomain code:static_cast<NSInteger>(exception.code) userInfo:@{ NSLocalizedDescriptionKey: exception.message }]);
        });
    };
    _panel->handleRequest({ WTFMove(hash), [_WKWebAuthenticationPanel convertToCoreRequestOptionsWithOptions:options], nullptr, WebKit::WebAuthenticationPanelResult::Unavailable, nullptr, std::nullopt, { }, String(), nullptr, std::nullopt, std::nullopt }, WTFMove(callback));
#endif
}

- (void)getAssertionWithMediationRequirement:(_WKWebAuthenticationMediationRequirement)mediation clientDataHash:(NSData *)clientDataHash options:(_WKPublicKeyCredentialRequestOptions *)options completionHandler:(void (^)(_WKAuthenticatorAssertionResponse *, NSError *))handler
{
#if ENABLE(WEB_AUTHN)
    auto callback = [handler = makeBlockPtr(handler)] (std::variant<Ref<WebCore::AuthenticatorResponse>, WebCore::ExceptionData>&& result) mutable {
        WTF::switchOn(result, [&](const Ref<WebCore::AuthenticatorResponse>& response) {
            handler(wkAuthenticatorAssertionResponse(response->data(), nullptr, response->attachment()).get(), nil);
        }, [&](const WebCore::ExceptionData& exception) {
            handler(nil, [NSError errorWithDomain:WKErrorDomain code:static_cast<NSInteger>(exception.code) userInfo:@{ NSLocalizedDescriptionKey: exception.message }]);
        });
    };
    _panel->handleRequest({ makeVector(clientDataHash), [_WKWebAuthenticationPanel convertToCoreRequestOptionsWithOptions:options], nullptr, WebKit::WebAuthenticationPanelResult::Unavailable, nullptr, std::nullopt, { }, String(), nullptr, toWebCore(mediation), std::nullopt }, WTFMove(callback));
#endif
}

- (void)getAssertionWithClientDataHash:(NSData *)clientDataHash options:(_WKPublicKeyCredentialRequestOptions *)options completionHandler:(void (^)(_WKAuthenticatorAssertionResponse *, NSError *))handler
{
    [self getAssertionWithMediationRequirement:_WKWebAuthenticationMediationRequirementOptional clientDataHash:clientDataHash options:options completionHandler:handler];
}

+ (BOOL)isUserVerifyingPlatformAuthenticatorAvailable
{
#if ENABLE(WEB_AUTHN)
    return WebKit::LocalService::isAvailable();
#else
    return NO;
#endif
}

+ (NSData *)getClientDataJSONForAuthenticationType:(_WKWebAuthenticationType)type challenge:(NSData *)challenge origin:(NSString *)origin
{
    RetainPtr<NSData> clientDataJSON;

#if ENABLE(WEB_AUTHN)
    clientDataJSON = produceClientDataJson(type, challenge, origin);
#endif

    return clientDataJSON.autorelease();
}

+ (NSData *)getClientDataJSONWithTopOrigin:(_WKWebAuthenticationType)type challenge:(NSData *)challenge origin:(NSString *)origin topOrigin:(NSString *)topOrigin crossOrigin:(BOOL)crossOrigin
{
    RetainPtr<NSData> clientDataJSON;

#if ENABLE(WEB_AUTHN)
    clientDataJSON = produceClientDataJson(type, challenge, origin, topOrigin, crossOrigin ? WebAuthn::Scope::CrossOrigin: WebAuthn::Scope::SameOrigin);
#endif

    return clientDataJSON.autorelease();
}

+ (NSData *)encodeMakeCredentialCommandWithClientDataJSON:(NSData *)clientDataJSON options:(_WKPublicKeyCredentialCreationOptions *)options userVerificationAvailability:(_WKWebAuthenticationUserVerificationAvailability)userVerificationAvailability
{
    return [self encodeMakeCredentialCommandWithClientDataJSON:clientDataJSON options:options userVerificationAvailability:userVerificationAvailability authenticatorSupportedExtensions:nil];
}

+ (NSData *)encodeMakeCredentialCommandWithClientDataJSON:(NSData *)clientDataJSON options:(_WKPublicKeyCredentialCreationOptions *)options userVerificationAvailability:(_WKWebAuthenticationUserVerificationAvailability)userVerificationAvailability authenticatorSupportedExtensions:(NSArray<NSString *> *)authenticatorSupportedExtensions
{
    RetainPtr<NSData> encodedCommand;
#if ENABLE(WEB_AUTHN)
    auto hash = produceClientDataJsonHash(clientDataJSON);
    auto encodedVector = fido::encodeMakeCredentialRequestAsCBOR(hash, [_WKWebAuthenticationPanel convertToCoreCreationOptionsWithOptions:options], coreUserVerificationAvailability(userVerificationAvailability), fido::AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported, makeVector<String>(authenticatorSupportedExtensions), std::nullopt);
    encodedCommand = toNSData(encodedVector);
#endif

    return encodedCommand.autorelease();
}

+ (NSData *)encodeGetAssertionCommandWithClientDataJSON:(NSData *)clientDataJSON options:(_WKPublicKeyCredentialRequestOptions *)options userVerificationAvailability:(_WKWebAuthenticationUserVerificationAvailability)userVerificationAvailability
{
    return [self encodeGetAssertionCommandWithClientDataJSON:clientDataJSON options:options userVerificationAvailability:userVerificationAvailability authenticatorSupportedExtensions:nil];
}

+ (NSData *)encodeGetAssertionCommandWithClientDataJSON:(NSData *)clientDataJSON options:(_WKPublicKeyCredentialRequestOptions *)options userVerificationAvailability:(_WKWebAuthenticationUserVerificationAvailability)userVerificationAvailability authenticatorSupportedExtensions:(NSArray<NSString *> *)authenticatorSupportedExtensions
{
    RetainPtr<NSData> encodedCommand;
#if ENABLE(WEB_AUTHN)
    auto hash = produceClientDataJsonHash(clientDataJSON);
    auto encodedVector = fido::encodeGetAssertionRequestAsCBOR(hash, [_WKWebAuthenticationPanel convertToCoreRequestOptionsWithOptions:options], coreUserVerificationAvailability(userVerificationAvailability), makeVector<String>(authenticatorSupportedExtensions), std::nullopt);
    encodedCommand = toNSData(encodedVector);
#endif

    return encodedCommand.autorelease();
}

+ (NSData *)encodeMakeCredentialCommandWithClientDataHash:(NSData *)clientDataHash options:(_WKPublicKeyCredentialCreationOptions *)options userVerificationAvailability:(_WKWebAuthenticationUserVerificationAvailability)userVerificationAvailability
{
    return [self encodeMakeCredentialCommandWithClientDataHash:clientDataHash options:options userVerificationAvailability:userVerificationAvailability authenticatorSupportedExtensions:nil];
}

+ (NSData *)encodeMakeCredentialCommandWithClientDataHash:(NSData *)clientDataHash options:(_WKPublicKeyCredentialCreationOptions *)options userVerificationAvailability:(_WKWebAuthenticationUserVerificationAvailability)userVerificationAvailability authenticatorSupportedExtensions:(NSArray<NSString *> *)authenticatorSupportedExtensions
{
    RetainPtr<NSData> encodedCommand;
#if ENABLE(WEB_AUTHN)
    auto encodedVector = fido::encodeMakeCredentialRequestAsCBOR(makeVector(clientDataHash), [_WKWebAuthenticationPanel convertToCoreCreationOptionsWithOptions:options], coreUserVerificationAvailability(userVerificationAvailability), fido::AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported, makeVector<String>(authenticatorSupportedExtensions), std::nullopt);
    encodedCommand = toNSData(encodedVector);
#endif

    return encodedCommand.autorelease();
}

+ (NSData *)encodeGetAssertionCommandWithClientDataHash:(NSData *)clientDataHash options:(_WKPublicKeyCredentialRequestOptions *)options userVerificationAvailability:(_WKWebAuthenticationUserVerificationAvailability)userVerificationAvailability
{
    return [self encodeGetAssertionCommandWithClientDataHash:clientDataHash options:options userVerificationAvailability:userVerificationAvailability authenticatorSupportedExtensions:nil];
}

+ (NSData *)encodeGetAssertionCommandWithClientDataHash:(NSData *)clientDataHash options:(_WKPublicKeyCredentialRequestOptions *)options userVerificationAvailability:(_WKWebAuthenticationUserVerificationAvailability)userVerificationAvailability authenticatorSupportedExtensions:(NSArray<NSString *> *)authenticatorSupportedExtensions
{
    RetainPtr<NSData> encodedCommand;
#if ENABLE(WEB_AUTHN)
    auto encodedVector = fido::encodeGetAssertionRequestAsCBOR(makeVector(clientDataHash), [_WKWebAuthenticationPanel convertToCoreRequestOptionsWithOptions:options], coreUserVerificationAvailability(userVerificationAvailability), makeVector<String>(authenticatorSupportedExtensions), std::nullopt);
    encodedCommand = toNSData(encodedVector);
#endif

    return encodedCommand.autorelease();
}

- (void)setMockConfiguration:(NSDictionary *)configuration
{
#if ENABLE(WEB_AUTHN)
    WebCore::MockWebAuthenticationConfiguration::LocalConfiguration localConfiguration;
    localConfiguration.userVerification = WebCore::MockWebAuthenticationConfiguration::UserVerification::Yes;
    if (configuration[@"privateKeyBase64"])
        localConfiguration.privateKeyBase64 = configuration[@"privateKeyBase64"];

    WebCore::MockWebAuthenticationConfiguration mockConfiguration;
    mockConfiguration.local = WTFMove(localConfiguration);

    _panel->setMockConfiguration(WTFMove(mockConfiguration));
#endif
}

#if ENABLE(WEB_AUTHN)
#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_panel;
}
#endif

@end
