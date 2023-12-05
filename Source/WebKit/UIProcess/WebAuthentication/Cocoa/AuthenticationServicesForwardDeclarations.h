/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

OBJC_CLASS NSString;
typedef NSString *ASAuthorizationPublicKeyCredentialUserVerificationPreference;
typedef NSString *ASAuthorizationCustomMethod;

#if PLATFORM(MAC)
@class NSWindow;
typedef NSWindow *ASPresentationAnchor;
#else
@class UIWindow;
typedef UIWindow *ASPresentationAnchor;
#endif

@class ASAuthorizationController;

NS_ASSUME_NONNULL_BEGIN

@protocol ASAuthorizationControllerPresentationContextProviding <NSObject>
@required

#if !TARGET_OS_WATCH
- (ASPresentationAnchor)presentationAnchorForAuthorizationController:(ASAuthorizationController *)controller;
#endif

@end

@protocol ASAuthorizationProvider <NSObject>

@end

@protocol ASAuthorizationCredential <NSObject, NSCopying, NSSecureCoding>

@end

@interface ASAuthorization : NSObject

@property (nonatomic, readonly, strong) id<ASAuthorizationProvider> provider;

@property (nonatomic, readonly, strong) id<ASAuthorizationCredential> credential;

+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

@protocol ASAuthorizationControllerDelegate <NSObject>
@optional

- (void)authorizationController:(ASAuthorizationController *)controller didCompleteWithAuthorization:(ASAuthorization *)authorization NS_SWIFT_NAME(authorizationController(controller:didCompleteWithAuthorization:));
- (void)authorizationController:(ASAuthorizationController *)controller didCompleteWithError:(NSError *)error NS_SWIFT_NAME(authorizationController(controller:didCompleteWithError:));

- (void)authorizationController:(ASAuthorizationController *)controller didCompleteWithCustomMethod:(ASAuthorizationCustomMethod)method;
@end

@interface ASAuthorizationRequest : NSObject <NSCopying, NSSecureCoding>
@property (nonatomic, readonly, strong) id<ASAuthorizationProvider> provider;
@end
typedef NSString *ASAuthorizationPublicKeyCredentialAttestationKind;

@interface ASAuthorizationController : NSObject
@property (nonatomic, weak, nullable) id<ASAuthorizationControllerDelegate> delegate;
@property (nonatomic, weak, nullable) id<ASAuthorizationControllerPresentationContextProviding> presentationContextProvider;
- (void)performRequests;

- (void)cancel;

+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithAuthorizationRequests:(NSArray<ASAuthorizationRequest *> *)authorizationRequests;
@end

@protocol ASAuthorizationPublicKeyCredentialRegistrationRequest <NSObject, NSSecureCoding, NSCopying>

@property (nonatomic, readonly, copy) NSString *relyingPartyIdentifier;

@property (nonatomic, copy) NSData *userID;

@property (nonatomic, copy) NSString *name;

@property (nonatomic, nullable, copy) NSString *displayName;

@property (nonatomic, copy) NSData *challenge;

@property (nonatomic, assign) ASAuthorizationPublicKeyCredentialUserVerificationPreference userVerificationPreference;

@property (nonatomic, assign) ASAuthorizationPublicKeyCredentialAttestationKind attestationPreference;

@end

typedef NS_ENUM(NSInteger, ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement) {
    ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirementRequired,
    ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirementPreferred,
};

@interface ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput : NSObject
- (instancetype)initWithSupportRequirement:(ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement)requirement;
@property (nonatomic, assign) ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement supportRequirement;
@end

@interface ASAuthorizationPublicKeyCredentialLargeBlobRegistrationOutput : NSObject

@property (nonatomic, readonly) BOOL isSupported;

@end

typedef NS_ENUM(NSInteger, ASAuthorizationPublicKeyCredentialAttachment) {
    ASAuthorizationPublicKeyCredentialAttachmentPlatform,
    ASAuthorizationPublicKeyCredentialAttachmentCrossPlatform,
};

@protocol ASPublicKeyCredential <ASAuthorizationCredential>

@property (nonatomic, readonly, copy) NSData *rawClientDataJSON;

@property (nonatomic, readonly, copy) NSData *credentialID;

@end

@protocol ASAuthorizationPublicKeyCredentialRegistration <ASPublicKeyCredential>

@property (nonatomic, readonly, nullable, copy) NSData *rawAttestationObject;

@end

@interface ASAuthorizationPlatformPublicKeyCredentialRegistration : NSObject <ASAuthorizationPublicKeyCredentialRegistration>
@property (nonatomic, readonly) ASAuthorizationPublicKeyCredentialAttachment attachment;
@property (nonatomic, readonly, nullable) ASAuthorizationPublicKeyCredentialLargeBlobRegistrationOutput *largeBlob NS_REFINED_FOR_SWIFT;
@end

@interface ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest : ASAuthorizationRequest <ASAuthorizationPublicKeyCredentialRegistrationRequest>

@property (nonatomic, nullable, copy) ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput *largeBlob;

@end

typedef NS_ENUM(NSInteger, ASPublicKeyCredentialClientDataCrossOriginValue) {
    ASPublicKeyCredentialClientDataCrossOriginValueNotSet,
    ASPublicKeyCredentialClientDataCrossOriginValueCrossOrigin,
    ASPublicKeyCredentialClientDataCrossOriginValueSameOriginWithAncestors,
};

@interface ASPublicKeyCredentialClientData : NSObject
- (instancetype)initWithChallenge:(NSData *)challenge origin:(NSString *)origin;

@property (nonatomic, copy) NSData *challenge;

@property (nonatomic, copy) NSString *origin;

@property (nonatomic, copy, nullable) NSString *topOrigin;

@property (nonatomic) ASPublicKeyCredentialClientDataCrossOriginValue crossOrigin;

@end

@protocol ASAuthorizationPublicKeyCredentialDescriptor <NSObject, NSSecureCoding, NSCopying>

@property (nonatomic, copy) NSData *credentialID;

@end

@protocol ASAuthorizationPublicKeyCredentialAssertionRequest <NSObject, NSSecureCoding, NSCopying>

@property (nonatomic, copy) NSData *challenge;

@property (nonatomic, copy) NSString *relyingPartyIdentifier;

@property (nonatomic, copy) NSArray<id<ASAuthorizationPublicKeyCredentialDescriptor>> *allowedCredentials;

@property (nonatomic, copy) ASAuthorizationPublicKeyCredentialUserVerificationPreference userVerificationPreference;

@end

@interface ASAuthorizationPlatformPublicKeyCredentialDescriptor : NSObject <ASAuthorizationPublicKeyCredentialDescriptor>

- (instancetype)initWithCredentialID:(NSData *)credentialID NS_DESIGNATED_INITIALIZER;

@end

typedef NS_ENUM(NSInteger, ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperation) {
    ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperationRead,
    ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperationWrite,
};

@interface ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput : NSObject
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithOperation:(ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperation)operation;

@property (nonatomic, readonly) ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperation operation;
@property (nonatomic, nullable, copy) NSData *dataToWrite;

@end

@protocol ASAuthorizationPublicKeyCredentialAssertion <ASPublicKeyCredential>

@property (nonatomic, readonly, copy) NSData *rawAuthenticatorData;

@property (nonatomic, readonly, copy) NSData *userID;

@property (nonatomic, readonly, copy) NSData *signature;

@end

@interface ASAuthorizationPublicKeyCredentialLargeBlobAssertionOutput : NSObject

@property (nonatomic, nullable, readonly) NSData *readData;
@property (nonatomic, readonly) BOOL didWrite;

@end

@interface ASAuthorizationPlatformPublicKeyCredentialAssertion : NSObject <ASAuthorizationPublicKeyCredentialAssertion>
@property (nonatomic, readonly) ASAuthorizationPublicKeyCredentialAttachment attachment;

@property (nonatomic, readonly, nullable) ASAuthorizationPublicKeyCredentialLargeBlobAssertionOutput *largeBlob NS_REFINED_FOR_SWIFT;
@end

@interface ASAuthorizationPlatformPublicKeyCredentialAssertionRequest : ASAuthorizationRequest <ASAuthorizationPublicKeyCredentialAssertionRequest>
@property (nonatomic, copy) NSArray<ASAuthorizationPlatformPublicKeyCredentialDescriptor *> *allowedCredentials;
@property (nonatomic, nullable, copy) ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput *largeBlob;
@end

@interface ASAuthorizationPlatformPublicKeyCredentialProvider : NSObject <ASAuthorizationProvider>

- (instancetype)initWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier NS_DESIGNATED_INITIALIZER;

- (ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest *)createCredentialRegistrationRequestWithChallenge:(NSData *)challenge name:(NSString *)name userID:(NSData *)userID NS_SWIFT_NAME(createCredentialRegistrationRequest(challenge:name:userID:));

- (ASAuthorizationPlatformPublicKeyCredentialAssertionRequest *)createCredentialAssertionRequestWithChallenge:(NSData *)challenge NS_SWIFT_NAME(createCredentialAssertionRequest(challenge:));

@property (nonatomic, readonly, copy) NSString *relyingPartyIdentifier;
@end

@protocol ASAuthorizationWebBrowserPlatformPublicKeyCredentialProvider

- (ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest *)createCredentialRegistrationRequestWithClientData:(ASPublicKeyCredentialClientData *)clientData name:(NSString *)name userID:(NSData *)userID;

- (ASAuthorizationPlatformPublicKeyCredentialAssertionRequest *)createCredentialAssertionRequestWithClientData:(ASPublicKeyCredentialClientData *)clientData;

@end

typedef NSString *ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport;

@interface ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor : NSObject <ASAuthorizationPublicKeyCredentialDescriptor>

- (instancetype)initWithCredentialID:(NSData *)credentialID transports:(NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport> *)allowedTransports;

@property (nonatomic, copy) NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport> *transports;
@end

@interface ASAuthorizationSecurityKeyPublicKeyCredentialRegistration : NSObject <ASAuthorizationPublicKeyCredentialRegistration>
@end

typedef NSInteger ASCOSEAlgorithmIdentifier NS_TYPED_EXTENSIBLE_ENUM;

@interface ASAuthorizationPublicKeyCredentialParameters : NSObject <NSSecureCoding, NSCopying>

- (instancetype)initWithAlgorithm:(ASCOSEAlgorithmIdentifier)algorithm;

@property (nonatomic, readonly) ASCOSEAlgorithmIdentifier algorithm;

@end

typedef NSString *ASAuthorizationPublicKeyCredentialResidentKeyPreference;

@interface ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest : ASAuthorizationRequest <ASAuthorizationPublicKeyCredentialRegistrationRequest>

@property (nonatomic, copy) NSArray<ASAuthorizationPublicKeyCredentialParameters *> *credentialParameters;

@property (nonatomic, copy) NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor *> *excludedCredentials;

@property (nonatomic, copy) ASAuthorizationPublicKeyCredentialResidentKeyPreference residentKeyPreference;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface ASAuthorizationSecurityKeyPublicKeyCredentialAssertion : NSObject <ASAuthorizationPublicKeyCredentialAssertion>
@end

@interface ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest : ASAuthorizationRequest <ASAuthorizationPublicKeyCredentialAssertionRequest>

@property (nonatomic, copy) NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor *> *allowedCredentials;

@end

@interface ASAuthorizationSecurityKeyPublicKeyCredentialProvider : NSObject <ASAuthorizationProvider>

- (instancetype)initWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier NS_DESIGNATED_INITIALIZER;

- (ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest *)createCredentialRegistrationRequestWithChallenge:(NSData *)challenge displayName:(NSString *)displayName name:(NSString *)name userID:(NSData *)userID NS_SWIFT_NAME(createCredentialRegistrationRequest(challenge:displayName:name:userID:));

- (ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest *)createCredentialAssertionRequestWithChallenge:(NSData *)challenge NS_SWIFT_NAME(createCredentialAssertionRequest(challenge:));

@property (nonatomic, readonly, copy) NSString *relyingPartyIdentifier;

@end

NS_ASSUME_NONNULL_END
