/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if HAVE(ASC_AUTH_UI) || HAVE(UNIFIED_ASC_AUTH_UI)

@interface ASCWebKitSPISupport : NSObject
@property (class, nonatomic) BOOL shouldUseAlternateCredentialStore;
+ (BOOL)arePasskeysDisallowedForRelyingParty:(nonnull NSString *)relyingParty;
@end

// FIXME: Most of the forward declarations below should be behind a non-Apple-internal SDK compile-time flag.
// When building with an Apple-internal SDK, we should instead import the private headers directly from the
// AuthenticationServicesCore framework.

NS_ASSUME_NONNULL_BEGIN

@class LAContext;

@protocol ASCLoginChoiceProtocol;
@protocol ASCCredentialProtocol;

@protocol ASCAuthorizationPresenterHostProtocol <NSObject>

- (void)authorizationRequestInitiatedWithLoginChoice:(id <ASCLoginChoiceProtocol>)loginChoice authenticatedContext:(nullable LAContext *)context completionHandler:(void (^)(id <ASCCredentialProtocol> credential, NSError *error))completionHandler;

- (void)authorizationRequestFinishedWithCredential:(nullable id<ASCCredentialProtocol>)credential error:(nullable NSError *)error completionHandler:(void (^)(void))completionHandler;

- (void)validateUserEnteredPIN:(NSString *)pin completionHandler:(void (^)(id <ASCCredentialProtocol> credential, NSError *error))completionHandler;

@end

@class ASCAuthorizationPresentationContext;
@class ASCAuthorizationPresenter;

@protocol ASCAuthorizationPresenterDelegate <NSObject>

- (void)authorizationPresenter:(ASCAuthorizationPresenter *)presenter credentialRequestedForLoginChoice:(id <ASCLoginChoiceProtocol>)loginChoice authenticatedContext:(nullable LAContext *)context completionHandler:(void (^)(id <ASCCredentialProtocol> _Nullable credential, NSError * _Nullable error))completionHandler;

- (void)authorizationPresenter:(ASCAuthorizationPresenter *)presenter validateUserEnteredPIN:(NSString *)pin completionHandler:(void (^)(id <ASCCredentialProtocol> credential, NSError *error))completionHandler;

@end

@interface ASCAuthorizationPresenter : NSObject <ASCAuthorizationPresenterHostProtocol>

- (void)presentAuthorizationWithContext:(ASCAuthorizationPresentationContext *)context completionHandler:(void (^)(id<ASCCredentialProtocol> _Nullable, NSError * _Nullable))completionHandler;
- (void)presentError:(NSError *)error forService:(NSString *)service completionHandler:(void (^)(void))completionHandler;
- (void)updateInterfaceWithLoginChoices:(NSArray<id <ASCLoginChoiceProtocol>> *)loginChoices;
- (void)presentPINEntryInterface;
- (void)updateInterfaceForUserVisibleError:(NSError *)userVisibleError;
- (void)dismissWithError:(nullable NSError *)error;

@property (nonatomic, weak) id <ASCAuthorizationPresenterDelegate> delegate;

@end

@interface ASCAuthorizationRemotePresenter : NSObject

#if TARGET_OS_OSX
- (void)presentWithWindow:(NSWindow *)window daemonEndpoint:(NSXPCListenerEndpoint *)daemonEndpoint completionHandler:(void (^)(id <ASCCredentialProtocol>, NSError *))completionHandler;
#endif

@end

@class ASCCredentialRequestContext;

extern NSString * const ASCAuthorizationPresentationContextDataKey;

@interface ASCAuthorizationPresentationContext : NSObject <NSSecureCoding>

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)initWithRequestContext:(nullable ASCCredentialRequestContext *)requestContext appIdentifier:(nullable NSString *)appIdentifier;

- (void)addLoginChoice:(id<ASCLoginChoiceProtocol>)loginChoice;

@property (nonatomic, readonly, copy) NSString *appIdentifier;
@property (nonatomic, readonly, copy) NSArray<id<ASCLoginChoiceProtocol>> *loginChoices;
@property (nonatomic, nullable, copy) NSString *serviceName;

@property (nonatomic, copy) NSString *proxiedAppName;
@property (nonatomic, copy) NSArray<NSString *> *proxiedAssociatedDomains;
@property (nonatomic, copy) NSData *proxiedIconData;
@property (nonatomic, copy) NSNumber *proxiedIconScale;

@end

@protocol ASCLoginChoiceProtocol <NSObject, NSSecureCoding>

@end

typedef NS_ENUM(NSInteger, ASCSecurityKeyPublicKeyCredentialKind) {
    ASCSecurityKeyPublicKeyCredentialKindRegistration,
    ASCSecurityKeyPublicKeyCredentialKindAssertion,
    ASCSecurityKeyPublicKeyCredentialKindAssertionPlaceholder,
};

@class ASCPublicKeyCredentialDescriptor;

@interface ASCPublicKeyCredentialDescriptor : NSObject <NSSecureCoding>

- (instancetype)initWithCredentialID:(NSData *)credentialID transports:(nullable NSArray<NSString *> *)allowedTransports;

@property (nonatomic, readonly) NSData *credentialID;
@property (nonatomic, nullable, readonly) NSArray<NSString *> *transports;

@end

@class ASCWebAuthenticationExtensionsClientInputs;

@interface ASCWebAuthenticationExtensionsClientInputs : NSObject <NSCopying, NSSecureCoding>

- (instancetype)initWithAppID:(NSString * _Nullable)appID isGoogleLegacyAppIDSupport:(BOOL)isGoogleLegacyAppIDSupport NS_DESIGNATED_INITIALIZER;

@property (nonatomic, nullable, copy) NSString *appID;
@property (nonatomic) BOOL isGoogleLegacyAppIDSupport;

@end

@class ASCPublicKeyCredentialDescriptor;

typedef NS_ENUM(NSUInteger, ASCPublicKeyCredentialKind) {
    ASCPublicKeyCredentialKindPlatform = 1,
    ASCPublicKeyCredentialKindSecurityKey,
};

@interface ASCPublicKeyCredentialAssertionOptions : NSObject <NSSecureCoding>

- (instancetype)initWithKind:(ASCPublicKeyCredentialKind)credentialKind relyingPartyIdentifier:(NSString *)relyingPartyIdentifier challenge:(NSData *)challenge userVerificationPreference:(nullable NSString *)userVerificationPreference allowedCredentials:(nullable NSArray<ASCPublicKeyCredentialDescriptor *> *)allowedCredentials;

- (instancetype)initWithKind:(ASCPublicKeyCredentialKind)credentialKind relyingPartyIdentifier:(NSString *)relyingPartyIdentifier clientDataHash:(NSData *)clientDataHash userVerificationPreference:(nullable NSString *)userVerificationPreference allowedCredentials:(nullable NSArray<ASCPublicKeyCredentialDescriptor *> *)allowedCredentials;

@property (nonatomic, readonly) ASCPublicKeyCredentialKind credentialKind;
@property (nonatomic, copy, readonly) NSString *relyingPartyIdentifier;
@property (nonatomic, nullable, copy, readonly) NSData *challenge;
// If clientDataHash is null, then gets generated from challenge and relyingPartyIdentifier.
@property (nonatomic, nullable, copy) NSData *clientDataHash;
@property (nonatomic, nullable, readonly, copy) NSString *userVerificationPreference;
@property (nonatomic, nullable, copy) ASCWebAuthenticationExtensionsClientInputs *extensions;
@property (nonatomic, nullable, copy) NSData *extensionsCBOR;
@property (nonatomic, nullable, copy) NSNumber *timeout;

@property (nonatomic, nullable, readonly, copy) NSArray<ASCPublicKeyCredentialDescriptor *> *allowedCredentials;

@property (nonatomic, nullable, copy) NSString *destinationSiteForCrossSiteAssertion;

@end


typedef NS_OPTIONS(NSUInteger, ASCCredentialRequestTypes) {
    ASCCredentialRequestTypeNone = 0,
    ASCCredentialRequestTypePassword = 1 << 0,
    ASCCredentialRequestTypeAppleID = 1 << 1,
    ASCCredentialRequestTypePlatformPublicKeyRegistration = 1 << 2,
    ASCCredentialRequestTypePlatformPublicKeyAssertion = 1 << 3,
    ASCCredentialRequestTypeSecurityKeyPublicKeyRegistration = 1 << 4,
    ASCCredentialRequestTypeSecurityKeyPublicKeyAssertion = 1 << 5,
};

typedef NS_ENUM(NSInteger, ASPublicKeyCredentialResidentKeyPreference) {
    ASPublicKeyCredentialResidentKeyPreferenceNotPresent,
    ASPublicKeyCredentialResidentKeyPreferenceDiscouraged,
    ASPublicKeyCredentialResidentKeyPreferencePreferred,
    ASPublicKeyCredentialResidentKeyPreferenceRequired,
};

@interface ASCPublicKeyCredentialCreationOptions : NSObject <NSSecureCoding>

@property (nonatomic, nullable, copy) NSData *challenge;
@property (nonatomic, nullable, copy) NSData *clientDataHash;
@property (nonatomic, copy) NSString *relyingPartyIdentifier;
@property (nonatomic, copy) NSString *userName;
@property (nonatomic, copy) NSData *userIdentifier;
@property (nonatomic, copy) NSString *userDisplayName;
@property (nonatomic, copy) NSArray<NSNumber *> *supportedAlgorithmIdentifiers;
@property (nonatomic, nullable, copy) NSString *userVerificationPreference;
@property (nonatomic, nullable, copy) NSString *attestationPreference;
@property (nonatomic, nullable, copy) ASCWebAuthenticationExtensionsClientInputs *extensions;
@property (nonatomic, nullable, copy) NSData *extensionsCBOR;
@property (nonatomic, nullable, copy) NSNumber *timeout;

@property (nonatomic) BOOL shouldRequireResidentKey;
@property (nonatomic) ASPublicKeyCredentialResidentKeyPreference residentKeyPreference;
@property (nonatomic, copy) NSArray<ASCPublicKeyCredentialDescriptor *> *excludedCredentials;

@end

@interface ASCSecurityKeyPublicKeyCredentialLoginChoice : NSObject <ASCLoginChoiceProtocol>

- (instancetype)initRegistrationChoiceWithOptions:(ASCPublicKeyCredentialCreationOptions *)options;
- (instancetype)initWithName:(NSString *)name displayName:(NSString *)displayName userHandle:(NSData *)userHandle;
- (instancetype)initAssertionPlaceholderChoice;

@property (nonatomic, nullable, readonly, copy) NSString *name;
@property (nonatomic, nullable, readonly, copy) NSString *displayName;
@property (nonatomic, nullable, readonly, copy) NSData *userHandle;
@property (nonatomic, readonly) ASCSecurityKeyPublicKeyCredentialKind credentialKind;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface ASCPlatformPublicKeyCredentialLoginChoice : NSObject <ASCLoginChoiceProtocol>

- (instancetype)initRegistrationChoiceWithOptions:(ASCPublicKeyCredentialCreationOptions *)options;
- (instancetype)initWithName:(NSString *)name displayName:(NSString *)displayName userHandle:(NSData *)userHandle;

@property (nonatomic, readonly, copy) NSString *name;
@property (nonatomic, readonly, copy) NSString *displayName;
@property (nonatomic, readonly, nullable, copy) NSData *userHandle;
@property (nonatomic, readonly) BOOL isRegistrationRequest;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

typedef NS_ENUM(NSInteger, ASCredentialRequestStyle) {
    ASCredentialRequestStyleModal,
    ASCredentialRequestStyleAutoFill,
};

@class ASGlobalFrameIdentifier;

@interface ASGlobalFrameIdentifier : NSObject <NSCopying, NSSecureCoding>
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithPageID:(NSNumber *)webPageID frameID:(NSNumber *)webFrameID;
@property (nonatomic, readonly) NSNumber *webPageID;
@property (nonatomic, readonly) NSNumber *webFrameID;
@end

@interface ASCCredentialRequestContext : NSObject <NSSecureCoding>

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)initWithRequestTypes:(ASCCredentialRequestTypes)requestTypes;

@property (nonatomic, readonly) NSUInteger requestTypes;
@property (nonatomic, nullable, copy) NSString *relyingPartyIdentifier;

@property (nonatomic, nullable, copy) ASCPublicKeyCredentialCreationOptions *platformKeyCredentialCreationOptions;
@property (nonatomic, nullable, copy) ASCPublicKeyCredentialCreationOptions *securityKeyCredentialCreationOptions;

@property (nonatomic, nullable, copy) ASCPublicKeyCredentialAssertionOptions *platformKeyCredentialAssertionOptions;
@property (nonatomic, nullable, copy) ASCPublicKeyCredentialAssertionOptions *securityKeyCredentialAssertionOptions;

@property (nonatomic) ASCredentialRequestStyle requestStyle;

@property (nonatomic, nullable, copy) ASGlobalFrameIdentifier *globalFrameID;
@end

@protocol ASCCredentialProtocol <NSObject, NSSecureCoding>

@end

@class _WKAuthenticatorAttestationResponse;

@interface ASCPlatformPublicKeyCredentialRegistration : NSObject <ASCCredentialProtocol>

- (instancetype)initWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier attestationObject:(NSData *)attestationObject rawClientDataJSON:(NSData *)rawClientDataJSON credentialID:(NSData *)credentialID;

- (instancetype)initWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier authenticatorAttestationResponse:(_WKAuthenticatorAttestationResponse *)attestationResponse rawClientDataJSON:(NSData *)rawClientDataJSON;

@property (nonatomic, copy, readonly) NSData *credentialID;
@property (nonatomic, copy, readonly) NSString *relyingPartyIdentifier;
@property (nonatomic, copy, readonly) NSData *attestationObject;
@property (nonatomic, copy, readonly) NSData *rawClientDataJSON;
@property (nonatomic, copy) NSArray<NSNumber *> *transports;
@property (nonatomic, copy, nullable) NSData *extensionOutputsCBOR;
@property (nonatomic, copy, readonly) NSString *attachment;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface ASCSecurityKeyPublicKeyCredentialRegistration : NSObject <ASCCredentialProtocol>

- (instancetype)initWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier authenticatorAttestationResponse:(_WKAuthenticatorAttestationResponse *)attestationResponse;

@property (nonatomic, copy, readonly) NSData *credentialID;
@property (nonatomic, copy, readonly) NSData *rawClientDataJSON;
@property (nonatomic, copy, readonly) NSString *relyingPartyIdentifier;
@property (nonatomic, copy, readonly) NSData *attestationObject;
@property (nonatomic, copy) NSArray<NSNumber *> *transports;
@property (nonatomic, copy, readonly, nullable) NSData *extensionOutputsCBOR;
@property (nonatomic, copy, readonly) NSString *attachment;

@end

@class _WKAuthenticatorAssertionResponse;

@interface ASCPlatformPublicKeyCredentialAssertion : NSObject <ASCCredentialProtocol>

- (instancetype)initWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier authenticatorAssertionResponse:(_WKAuthenticatorAssertionResponse *)assertionResponse;

@property (nonatomic, copy, readonly) NSData *credentialID;
@property (nonatomic, copy, readonly) NSData *rawClientDataJSON;
@property (nonatomic, copy, readonly) NSString *relyingPartyIdentifier;
@property (nonatomic, copy, readonly) NSData *authenticatorData;
@property (nonatomic, copy, readonly) NSData *signature;
@property (nonatomic, copy, readonly, nullable) NSData *userHandle;
@property (nonatomic, copy, readonly, nullable) NSData *extensionOutputsCBOR;
@property (nonatomic, copy, readonly) NSString *attachment;

@end

@interface ASCSecurityKeyPublicKeyCredentialAssertion : NSObject <ASCCredentialProtocol>

- (instancetype)initWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier authenticatorAssertionResponse:(_WKAuthenticatorAssertionResponse *)assertionResponse;

@property (nonatomic, copy, readonly) NSData *credentialID;
@property (nonatomic, copy, readonly) NSString *relyingPartyIdentifier;
@property (nonatomic, copy, readonly) NSData *authenticatorData;
@property (nonatomic, copy, readonly) NSData *signature;
@property (nonatomic, copy, readonly, nullable) NSData *userHandle;
@property (nonatomic, copy, readonly) NSData *rawClientDataJSON;
@property (nonatomic, copy, readonly, nullable) NSData *extensionOutputsCBOR;
@property (nonatomic, copy, readonly) NSString *attachment;

@end

@protocol ASCAgentProtocol <NSObject>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
- (void)performAuthorizationRequestsForContext:(ASCCredentialRequestContext *)context withCompletionHandler:(void (^)(id <ASCCredentialProtocol> _Nullable credential, NSError * _Nullable error))completionHandler;
#elif TARGET_OS_OSX || TARGET_OS_MACCATALYST
- (void)performAuthorizationRequestsForContext:(ASCCredentialRequestContext *)context withClearanceHandler:(void (^)(NSXPCListenerEndpoint * _Nullable daemonEndpoint, NSError * _Nullable error))clearanceHandler;

- (void)beginAuthorizationForApplicationIdentifier:(NSString *)applicationIdentifier fromEndpoint:(NSXPCListenerEndpoint *)listenerEndpoint;

- (void)userSelectedLoginChoice:(id <ASCLoginChoiceProtocol>)loginChoice authenticatedContext:(LAContext *)context completionHandler:(void (^)(id <ASCCredentialProtocol> _Nullable, NSError * _Nullable))completionHandler;

- (void)requestCompletedWithCredential:(nullable id<ASCCredentialProtocol>)credential error:(nullable NSError *)error;

- (void)performAutoFillAuthorizationRequestsForContext:(ASCCredentialRequestContext *)context withCompletionHandler:(void (^)(id <ASCCredentialProtocol> _Nullable credential, NSError * _Nullable error))completionHandler;
#endif

- (void)cancelCurrentRequest;

@end

@interface ASCAgentProxy : NSObject <ASCAgentProtocol>

- (instancetype)init;

#if TARGET_OS_OSX
- (instancetype)initWithEndpoint:(NSXPCListenerEndpoint *)endpoint;
#endif

- (void)invalidate;

- (void)reconnectIfNecessary;

@end

// FIXME(219767): Remove ASCAppleIDCredential.
@interface ASCAppleIDCredential : NSObject <ASCCredentialProtocol>

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)initWithUser:(NSString *)user identityToken:(nullable NSData *)identityToken state:(nullable NSString *)state;

@property (nonatomic, readonly, copy) NSString *user;
@property (nonatomic, readonly, copy, nullable) NSData *identityToken;

@end

extern NSErrorDomain const ASCAuthorizationErrorDomain;

typedef NS_ERROR_ENUM(ASCAuthorizationErrorDomain, ASCAuthorizationError) {
    ASCAuthorizationErrorUnknown,
    ASCAuthorizationErrorFailed,
    ASCAuthorizationErrorUserCanceled,
    ASCAuthorizationErrorPINRequired,
    ASCAuthorizationErrorMultipleNFCTagsPresent,
    ASCAuthorizationErrorNoCredentialsFound,
    ASCAuthorizationErrorLAError,
    ASCAuthorizationErrorLAExcludeCredentialsMatched,
    ASCAuthorizationErrorPINInvalid,
    ASCAuthorizationErrorAuthenticatorTemporarilyLocked,
    ASCAuthorizationErrorAuthenticatorPermanentlyLocked,
};

NS_ASSUME_NONNULL_END

#endif // HAVE(ASC_AUTH_UI) || HAVE(UNIFIED_ASC_AUTH_UI)
