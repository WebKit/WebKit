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

#if HAVE(ASC_AUTH_UI)

// FIXME<rdar://problem/72406585>: Uncomment the headers after the SDK becomes stable.
//#if USE(APPLE_INTERNAL_SDK)
//
//// FIXME(219767): Remove ASCAppleIDCredential.h.
//#import <AuthenticationServicesCore/ASCAppleIDCredential.h>
//#import <AuthenticationServicesCore/ASCAuthorizationPresentationContext.h>
//#import <AuthenticationServicesCore/ASCAuthorizationPresenter.h>
//#import <AuthenticationServicesCore/ASCPlatformPublicKeyCredentialLoginChoice.h>
//#import <AuthenticationServicesCore/ASCSecurityKeyPublicKeyCredentialLoginChoice.h>
//
//#else

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

typedef NS_ENUM(NSInteger, ASCSecurityKeyPublicKeyCredentialLoginChoiceKind) {
    ASCSecurityKeyPublicKeyCredentialLoginChoiceKindRegistration,
    ASCSecurityKeyPublicKeyCredentialLoginChoiceKindAssertion,
    ASCSecurityKeyPublicKeyCredentialLoginChoiceKindAssertionPlaceholder,
};

@interface ASCSecurityKeyPublicKeyCredentialLoginChoice : NSObject <ASCLoginChoiceProtocol>

- (instancetype)initRegistrationChoice;
- (instancetype)initWithName:(NSString *)name displayName:(NSString *)displayName userHandle:(NSData *)userHandle;
- (instancetype)initAssertionPlaceholderChoice;

@property (nonatomic, nullable, readonly, copy) NSString *name;
@property (nonatomic, nullable, readonly, copy) NSString *displayName;
@property (nonatomic, nullable, readonly, copy) NSData *userHandle;
@property (nonatomic, readonly) ASCSecurityKeyPublicKeyCredentialLoginChoiceKind loginChoiceKind;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface ASCPlatformPublicKeyCredentialLoginChoice : NSObject <ASCLoginChoiceProtocol>

- (instancetype)initRegistrationChoice;
- (instancetype)initWithName:(NSString *)name displayName:(NSString *)displayName userHandle:(NSData *)userHandle;

@property (nonatomic, readonly, copy) NSString *name;
@property (nonatomic, readonly, copy) NSString *displayName;
@property (nonatomic, readonly, copy) NSData *userHandle;
@property (nonatomic, readonly) BOOL isRegistrationRequest;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

@protocol ASCCredentialProtocol <NSObject, NSSecureCoding>

@end

// FIXME(219767): Remove ASCAppleIDCredential.
@interface ASCAppleIDCredential : NSObject <ASCCredentialProtocol>

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)initWithUser:(NSString *)user identityToken:(NSData *)identityToken;

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

//#endif // USE(APPLE_INTERNAL_SDK)

#endif // HAVE(ASC_AUTH_UI)
