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
#import <WebCore/AuthenticatorResponse.h>
#import <WebCore/AuthenticatorResponseData.h>
#import <WebCore/CBORReader.h>
#import <WebCore/FidoConstants.h>
#import <WebCore/MockWebAuthenticationConfiguration.h>
#import <WebCore/PublicKeyCredentialCreationOptions.h>
#import <WebCore/PublicKeyCredentialRequestOptions.h>
#import <WebCore/WebAuthenticationConstants.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <pal/crypto/CryptoDigest.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/Base64.h>

#if ENABLE(WEB_AUTHN)
static RetainPtr<NSData> produceClientDataJson(_WKWebAuthenticationType type, NSData *challenge, NSString *origin)
{
    auto dictionary = adoptNS([[NSMutableDictionary alloc] init]);
    switch (type) {
    case _WKWebAuthenticationTypeCreate:
        [dictionary setObject:@"webauthn.create" forKey:@"type"];
        break;
    case _WKWebAuthenticationTypeGet:
        [dictionary setObject:@"webauthn.get" forKey:@"type"];
        break;
    }
    [dictionary setObject:WTF::base64URLEncode(challenge.bytes, challenge.length) forKey:@"challenge"];
    [dictionary setObject:origin forKey:@"origin"];

    return [NSJSONSerialization dataWithJSONObject:dictionary.get() options:NSJSONWritingSortedKeys error:nil];
}

static Vector<uint8_t> produceClientDataJsonHash(NSData *clientDataJson)
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(clientDataJson.bytes, clientDataJson.length);
    return crypto->computeHash();
}
#endif

NSString * const _WKLocalAuthenticatorCredentialNameKey = @"_WKLocalAuthenticatorCredentialNameKey";
NSString * const _WKLocalAuthenticatorCredentialIDKey = @"_WKLocalAuthenticatorCredentialIDKey";
NSString * const _WKLocalAuthenticatorCredentialRelyingPartyIDKey = @"_WKLocalAuthenticatorCredentialRelyingPartyIDKey";

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
    if (!_client)
        return nil;
    return _client->delegate().autorelease();
}

- (void)setDelegate:(id<_WKWebAuthenticationPanelDelegate>)delegate
{
    auto client = WTF::makeUniqueRef<WebKit::WebAuthenticationPanelClient>(self, delegate);
    _client = makeWeakPtr(client.get());
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
    case WebCore::AuthenticatorTransport::Internal:
        return _WKWebAuthenticationTransportInternal;
    default:
        ASSERT_NOT_REACHED();
        return _WKWebAuthenticationTransportUSB;
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
static RetainPtr<NSArray> getAllLocalAuthenticatorCredentialsImpl(NSString *accessGroup)
{
    NSDictionary *query = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassKey,
        (__bridge id)kSecAttrKeyClass: (__bridge id)kSecAttrKeyClassPrivate,
        (__bridge id)kSecAttrAccessGroup: accessGroup,
        (__bridge id)kSecReturnAttributes: @YES,
        (__bridge id)kSecMatchLimit: (__bridge id)kSecMatchLimitAll,
#if HAVE(DATA_PROTECTION_KEYCHAIN)
        (__bridge id)kSecUseDataProtectionKeychain: @YES
#else
        (__bridge id)kSecAttrNoLegacy: @YES
#endif
    };
    CFTypeRef attributesArrayRef = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &attributesArrayRef);
    if (status && status != errSecItemNotFound)
        return nullptr;
    auto retainAttributesArray = adoptCF(attributesArrayRef);

    auto result = adoptNS([[NSMutableArray alloc] init]);
    for (NSDictionary *attributes in (NSArray *)attributesArrayRef) {
        auto decodedResponse = cbor::CBORReader::read(vectorFromNSData(attributes[(__bridge id)kSecAttrApplicationTag]));
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

        [result addObject:@{
            _WKLocalAuthenticatorCredentialNameKey: username,
            _WKLocalAuthenticatorCredentialIDKey: attributes[(__bridge id)kSecAttrApplicationLabel],
            _WKLocalAuthenticatorCredentialRelyingPartyIDKey: attributes[(__bridge id)kSecAttrLabel]
        }];
    }

    return result;
}
#endif

+ (NSArray<NSDictionary *> *)getAllLocalAuthenticatorCredentials
{
#if ENABLE(WEB_AUTHN)
    return getAllLocalAuthenticatorCredentialsImpl(String(WebCore::LocalAuthenticatiorAccessGroup)).autorelease();
#else
    return nullptr;
#endif
}

+ (NSArray<NSDictionary *> *)getAllLocalAuthenticatorCredentialsWithAccessGroup:(NSString *)accessGroup
{
#if ENABLE(WEB_AUTHN)
    return getAllLocalAuthenticatorCredentialsImpl(accessGroup).autorelease();
#else
    return nullptr;
#endif
}

+ (void)deleteLocalAuthenticatorCredentialWithID:(NSData *)credentialID
{
#if ENABLE(WEB_AUTHN)
    NSDictionary* deleteQuery = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassKey,
        (__bridge id)kSecAttrApplicationLabel: credentialID,
#if HAVE(DATA_PROTECTION_KEYCHAIN)
        (__bridge id)kSecUseDataProtectionKeychain: @YES
#else
        (__bridge id)kSecAttrNoLegacy: @YES
#endif
    };
    SecItemDelete((__bridge CFDictionaryRef)deleteQuery);
#endif
}

+ (void)clearAllLocalAuthenticatorCredentials
{
#if ENABLE(WEB_AUTHN)
    WebKit::LocalAuthenticator::clearAllCredentials();
#endif
}

- (void)cancel
{
#if ENABLE(WEB_AUTHN)
    _panel->cancel();
#endif
}

#if ENABLE(WEB_AUTHN)
static Vector<uint8_t> vectorFromNSData(NSData* data)
{
    Vector<uint8_t> result;
    result.append((const uint8_t*)data.bytes, data.length);
    return result;
}

static WebCore::PublicKeyCredentialCreationOptions::RpEntity publicKeyCredentialRpEntity(_WKPublicKeyCredentialRelyingPartyEntity *rpEntity)
{
    WebCore::PublicKeyCredentialCreationOptions::RpEntity result;
    result.name = rpEntity.name;
    result.icon = rpEntity.icon;
    result.id = rpEntity.identifier;

    return result;
}

static WebCore::PublicKeyCredentialCreationOptions::UserEntity publicKeyCredentialUserEntity(_WKPublicKeyCredentialUserEntity *userEntity)
{
    WebCore::PublicKeyCredentialCreationOptions::UserEntity result;
    result.name = userEntity.name;
    result.icon = userEntity.icon;
    result.idVector = vectorFromNSData(userEntity.identifier);
    result.displayName = userEntity.displayName;

    return result;
}

static Vector<WebCore::PublicKeyCredentialCreationOptions::Parameters> publicKeyCredentialParameters(NSArray<_WKPublicKeyCredentialParameters *> *publicKeyCredentialParamaters)
{
    Vector<WebCore::PublicKeyCredentialCreationOptions::Parameters> result;
    result.reserveInitialCapacity(publicKeyCredentialParamaters.count);

    for (_WKPublicKeyCredentialParameters *param : publicKeyCredentialParamaters)
        result.uncheckedAppend({ WebCore::PublicKeyCredentialType::PublicKey, param.algorithm.longLongValue });

    return result;
}

static WebCore::AuthenticatorTransport authenticatorTransport(_WKWebAuthenticationTransport transport)
{
    switch (transport) {
    case _WKWebAuthenticationTransportUSB:
        return WebCore::AuthenticatorTransport::Usb;
    case _WKWebAuthenticationTransportNFC:
        return WebCore::AuthenticatorTransport::Nfc;
    case _WKWebAuthenticationTransportInternal:
        return WebCore::AuthenticatorTransport::Internal;
    default:
        ASSERT_NOT_REACHED();
        return WebCore::AuthenticatorTransport::Usb;
    }
}

static Vector<WebCore::AuthenticatorTransport> authenticatorTransports(NSArray<NSNumber *> *transports)
{
    Vector<WebCore::AuthenticatorTransport> result;
    result.reserveInitialCapacity(transports.count);

    for (NSNumber *transport : transports)
        result.uncheckedAppend(authenticatorTransport((_WKWebAuthenticationTransport)transport.intValue));

    return result;
}

static Vector<WebCore::PublicKeyCredentialDescriptor> publicKeyCredentialDescriptors(NSArray<_WKPublicKeyCredentialDescriptor *> *credentials)
{
    Vector<WebCore::PublicKeyCredentialDescriptor> result;
    result.reserveInitialCapacity(credentials.count);

    for (_WKPublicKeyCredentialDescriptor *credential : credentials)
        result.uncheckedAppend({ WebCore::PublicKeyCredentialType::PublicKey, { }, vectorFromNSData(credential.identifier), authenticatorTransports(credential.transports) });

    return result;
}

static Optional<WebCore::AuthenticatorAttachment> authenticatorAttachment(_WKAuthenticatorAttachment attachment)
{
    switch (attachment) {
    case _WKAuthenticatorAttachmentAll:
        return WTF::nullopt;
    case _WKAuthenticatorAttachmentPlatform:
        return WebCore::AuthenticatorAttachment::Platform;
    case _WKAuthenticatorAttachmentCrossPlatform:
        return WebCore::AuthenticatorAttachment::CrossPlatform;
    default:
        ASSERT_NOT_REACHED();
        return WTF::nullopt;
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

static WebCore::PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria authenticatorSelectionCriteria(_WKAuthenticatorSelectionCriteria *authenticatorSelection)
{
    WebCore::PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria result;
    result.authenticatorAttachment = authenticatorAttachment(authenticatorSelection.authenticatorAttachment);
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
    default:
        ASSERT_NOT_REACHED();
        return WebCore::AttestationConveyancePreference::None;
    }
}

static WebCore::AuthenticationExtensionsClientInputs authenticationExtensionsClientInputs(_WKAuthenticationExtensionsClientInputs *extensions)
{
    WebCore::AuthenticationExtensionsClientInputs result;
    result.appid = extensions.appid;
    result.googleLegacyAppidSupport = false;

    return result;
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
    result.extensions = authenticationExtensionsClientInputs(options.extensions);
#endif

    return result;
}

#if ENABLE(WEB_AUTHN)
static RetainPtr<_WKAuthenticatorAttestationResponse> wkAuthenticatorAttestationResponse(const WebCore::AuthenticatorResponseData& data, NSData *clientDataJSON)
{
    return adoptNS([[_WKAuthenticatorAttestationResponse alloc] initWithClientDataJSON:clientDataJSON rawId:[NSData dataWithBytes:data.rawId->data() length:data.rawId->byteLength()] extensions:nil attestationObject:[NSData dataWithBytes:data.attestationObject->data() length:data.attestationObject->byteLength()]]);
}
#endif

- (void)makeCredentialWithChallenge:(NSData *)challenge origin:(NSString *)origin options:(_WKPublicKeyCredentialCreationOptions *)options completionHandler:(void (^)(_WKAuthenticatorAttestationResponse *, NSError *))handler
{
#if ENABLE(WEB_AUTHN)
    auto clientDataJSON = produceClientDataJson(_WKWebAuthenticationTypeCreate, challenge, origin);
    auto hash = produceClientDataJsonHash(clientDataJSON.get());
    auto callback = [handler = makeBlockPtr(handler), clientDataJSON = WTFMove(clientDataJSON)] (Variant<Ref<WebCore::AuthenticatorResponse>, WebCore::ExceptionData>&& result) mutable {
        WTF::switchOn(result, [&](const Ref<WebCore::AuthenticatorResponse>& response) {
            handler(wkAuthenticatorAttestationResponse(response->data(), clientDataJSON.get()).get(), nil);
        }, [&](const WebCore::ExceptionData& exception) {
            handler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);
        });
    };
    _panel->handleRequest({ WTFMove(hash), [_WKWebAuthenticationPanel convertToCoreCreationOptionsWithOptions:options], nullptr, WebKit::WebAuthenticationPanelResult::Unavailable, nullptr, WTF::nullopt, { }, true, String(), nullptr }, WTFMove(callback));
#endif
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
    result.userVerification = userVerification(options.userVerification);
    result.authenticatorAttachment = authenticatorAttachment(options.authenticatorAttachment);
    result.extensions = authenticationExtensionsClientInputs(options.extensions);
#endif

    return result;
}

#if ENABLE(WEB_AUTHN)
static RetainPtr<_WKAuthenticatorAssertionResponse> wkAuthenticatorAssertionResponse(const WebCore::AuthenticatorResponseData& data, NSData *clientDataJSON)
{
    RetainPtr<_WKAuthenticationExtensionsClientOutputs> extensions;
    if (data.appid)
        extensions = adoptNS([[_WKAuthenticationExtensionsClientOutputs alloc] initWithAppid:data.appid.value()]);

    NSData *userHandle = nil;
    if (data.userHandle)
        userHandle = [NSData dataWithBytes:data.userHandle->data() length:data.userHandle->byteLength()];

    return adoptNS([[_WKAuthenticatorAssertionResponse alloc] initWithClientDataJSON:clientDataJSON rawId:[NSData dataWithBytes:data.rawId->data() length:data.rawId->byteLength()] extensions:WTFMove(extensions) authenticatorData:[NSData dataWithBytes:data.authenticatorData->data() length:data.authenticatorData->byteLength()] signature:[NSData dataWithBytes:data.signature->data() length:data.signature->byteLength()] userHandle:userHandle]);
}
#endif

- (void)getAssertionWithChallenge:(NSData *)challenge origin:(NSString *)origin options:(_WKPublicKeyCredentialRequestOptions *)options completionHandler:(void (^)(_WKAuthenticatorAssertionResponse *, NSError *))handler
{
#if ENABLE(WEB_AUTHN)
    auto clientDataJSON = produceClientDataJson(_WKWebAuthenticationTypeCreate, challenge, origin);
    auto hash = produceClientDataJsonHash(clientDataJSON.get());
    auto callback = [handler = makeBlockPtr(handler), clientDataJSON = WTFMove(clientDataJSON)] (Variant<Ref<WebCore::AuthenticatorResponse>, WebCore::ExceptionData>&& result) mutable {
        WTF::switchOn(result, [&](const Ref<WebCore::AuthenticatorResponse>& response) {
            handler(wkAuthenticatorAssertionResponse(response->data(), clientDataJSON.get()).get(), nil);
        }, [&](const WebCore::ExceptionData& exception) {
            handler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);
        });
    };
    _panel->handleRequest({ WTFMove(hash), [_WKWebAuthenticationPanel convertToCoreRequestOptionsWithOptions:options], nullptr, WebKit::WebAuthenticationPanelResult::Unavailable, nullptr, WTF::nullopt, { }, true, String(), nullptr }, WTFMove(callback));
#endif
}

+ (BOOL)isUserVerifyingPlatformAuthenticatorAvailable
{
#if ENABLE(WEB_AUTHN)
    return WebKit::LocalService::isAvailable();
#else
    return NO;
#endif
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
