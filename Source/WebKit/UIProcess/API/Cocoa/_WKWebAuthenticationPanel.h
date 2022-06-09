/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKit/WKDeclarationSpecifiers.h>
#import <WebKit/WKFoundation.h>

NS_ASSUME_NONNULL_BEGIN

@class _WKAuthenticatorAttestationResponse;
@class _WKAuthenticatorAssertionResponse;
@class _WKPublicKeyCredentialCreationOptions;
@class _WKPublicKeyCredentialRequestOptions;
@class _WKWebAuthenticationAssertionResponse;
@class _WKWebAuthenticationPanel;
@class LAContext;

typedef NS_ENUM(NSInteger, _WKWebAuthenticationPanelResult) {
    _WKWebAuthenticationPanelResultUnavailable,
    _WKWebAuthenticationPanelResultPresented,
    _WKWebAuthenticationPanelResultDidNotPresent,
} WK_API_AVAILABLE(macos(10.15.4), ios(13.4));

typedef NS_ENUM(NSInteger, _WKWebAuthenticationPanelUpdate) {
    _WKWebAuthenticationPanelUpdateMultipleNFCTagsPresent,
    _WKWebAuthenticationPanelUpdateNoCredentialsFound,
    _WKWebAuthenticationPanelUpdatePINBlocked,
    _WKWebAuthenticationPanelUpdatePINAuthBlocked,
    _WKWebAuthenticationPanelUpdatePINInvalid,
    _WKWebAuthenticationPanelUpdateLAError,
    _WKWebAuthenticationPanelUpdateLAExcludeCredentialsMatched,
    _WKWebAuthenticationPanelUpdateLANoCredential,
} WK_API_AVAILABLE(macos(10.15.4), ios(13.4));

typedef NS_ENUM(NSInteger, _WKWebAuthenticationResult) {
    _WKWebAuthenticationResultSucceeded,
    _WKWebAuthenticationResultFailed,
} WK_API_AVAILABLE(macos(10.15.4), ios(13.4));

typedef NS_ENUM(NSInteger, _WKWebAuthenticationTransport) {
    _WKWebAuthenticationTransportUSB,
    _WKWebAuthenticationTransportNFC,
    _WKWebAuthenticationTransportInternal,
} WK_API_AVAILABLE(macos(10.15.4), ios(13.4));

typedef NS_ENUM(NSInteger, _WKWebAuthenticationType) {
    _WKWebAuthenticationTypeCreate,
    _WKWebAuthenticationTypeGet,
} WK_API_AVAILABLE(macos(10.15.4), ios(13.4));

typedef NS_ENUM(NSInteger, _WKLocalAuthenticatorPolicy) {
    _WKLocalAuthenticatorPolicyAllow,
    _WKLocalAuthenticatorPolicyDisallow,
} WK_API_AVAILABLE(macos(11.0), ios(14.0));

typedef NS_ENUM(NSInteger, _WKWebAuthenticationSource) {
    _WKWebAuthenticationSourceLocal,
    _WKWebAuthenticationSourceExternal,
} WK_API_AVAILABLE(macos(11.0), ios(14.0));

typedef NS_ENUM(NSInteger, _WKWebAuthenticationUserVerificationAvailability) {
    _WKWebAuthenticationUserVerificationAvailabilitySupportedAndConfigured,
    _WKWebAuthenticationUserVerificationAvailabilitySupportedButNotConfigured,
    _WKWebAuthenticationUserVerificationAvailabilityNotSupported,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

WK_EXPORT extern NSString * const _WKLocalAuthenticatorCredentialNameKey;
WK_EXPORT extern NSString * const _WKLocalAuthenticatorCredentialDisplayNameKey;
WK_EXPORT extern NSString * const _WKLocalAuthenticatorCredentialIDKey;
WK_EXPORT extern NSString * const _WKLocalAuthenticatorCredentialRelyingPartyIDKey;
WK_EXPORT extern NSString * const _WKLocalAuthenticatorCredentialLastModificationDateKey;
WK_EXPORT extern NSString * const _WKLocalAuthenticatorCredentialCreationDateKey;

@protocol _WKWebAuthenticationPanelDelegate <NSObject>

@optional

- (void)panel:(_WKWebAuthenticationPanel *)panel updateWebAuthenticationPanel:(_WKWebAuthenticationPanelUpdate)update WK_API_AVAILABLE(macos(11.0), ios(14.0));
- (void)panel:(_WKWebAuthenticationPanel *)panel requestPINWithRemainingRetries:(NSUInteger)retries completionHandler:(void (^)(NSString *))completionHandler WK_API_AVAILABLE(macos(11.0), ios(14.0));
- (void)panel:(_WKWebAuthenticationPanel *)panel selectAssertionResponse:(NSArray < _WKWebAuthenticationAssertionResponse *> *)responses source:(_WKWebAuthenticationSource)source completionHandler:(void (^)(_WKWebAuthenticationAssertionResponse * _Nullable))completionHandler WK_API_AVAILABLE(macos(11.0), ios(14.0));
- (void)panel:(_WKWebAuthenticationPanel *)panel requestLAContextForUserVerificationWithCompletionHandler:(void (^)(LAContext *context))completionHandler WK_API_AVAILABLE(macos(12.0), ios(15.0));

// FIXME: <rdar://problem/71509848> Deprecate the following delegates.
- (void)panel:(_WKWebAuthenticationPanel *)panel decidePolicyForLocalAuthenticatorWithCompletionHandler:(void (^)(_WKLocalAuthenticatorPolicy policy))completionHandler WK_API_AVAILABLE(macos(11.0), ios(14.0));
- (void)panel:(_WKWebAuthenticationPanel *)panel dismissWebAuthenticationPanelWithResult:(_WKWebAuthenticationResult)result WK_API_AVAILABLE(macos(11.0), ios(14.0));
@end

WK_CLASS_AVAILABLE(macos(10.15.4), ios(13.4))
@interface _WKWebAuthenticationPanel : NSObject

@property (nullable, nonatomic, weak) id <_WKWebAuthenticationPanelDelegate> delegate;

+ (NSArray<NSDictionary *> *)getAllLocalAuthenticatorCredentials WK_API_AVAILABLE(macos(12.0), ios(15.0));
+ (void)deleteLocalAuthenticatorCredentialWithID:(NSData *)credentialID WK_API_AVAILABLE(macos(12.0), ios(15.0));
+ (void)clearAllLocalAuthenticatorCredentials WK_API_AVAILABLE(macos(12.0), ios(15.0));
+ (void)setUsernameForLocalCredentialWithID:(NSData *)credentialID username: (NSString *)username WK_API_AVAILABLE(macos(12.0), ios(15.0));

+ (BOOL)isUserVerifyingPlatformAuthenticatorAvailable WK_API_AVAILABLE(macos(12.0), ios(15.0));

+ (NSData *)getClientDataJSONForAuthenticationType:(_WKWebAuthenticationType)type challenge:(NSData *)challenge origin:(NSString *)origin WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
+ (NSData *)encodeMakeCredentialCommandWithClientDataJSON:(NSData *)clientDataJSON options:(_WKPublicKeyCredentialCreationOptions *)options userVerificationAvailability:(_WKWebAuthenticationUserVerificationAvailability)userVerificationAvailability WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
+ (NSData *)encodeGetAssertionCommandWithClientDataJSON:(NSData *)clientDataJSON options:(_WKPublicKeyCredentialRequestOptions *)options userVerificationAvailability:(_WKWebAuthenticationUserVerificationAvailability)userVerificationAvailability WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

+ (NSData *)encodeMakeCredentialCommandWithClientDataHash:(NSData *)clientDataHash options:(_WKPublicKeyCredentialCreationOptions *)options userVerificationAvailability:(_WKWebAuthenticationUserVerificationAvailability)userVerificationAvailability WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
+ (NSData *)encodeGetAssertionCommandWithClientDataHash:(NSData *)clientDataHash options:(_WKPublicKeyCredentialRequestOptions *)options userVerificationAvailability:(_WKWebAuthenticationUserVerificationAvailability)userVerificationAvailability WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

- (instancetype)init;

// FIXME: <rdar://problem/71509485> Adds detailed NSError.
- (void)makeCredentialWithChallenge:(NSData *)challenge origin:(NSString *)origin options:(_WKPublicKeyCredentialCreationOptions *)options completionHandler:(void (^)(_WKAuthenticatorAttestationResponse *, NSError *))handler WK_API_AVAILABLE(macos(12.0), ios(15.0));
- (void)makeCredentialWithClientDataHash:(NSData *)clientDataHash options:(_WKPublicKeyCredentialCreationOptions *)options completionHandler:(void (^)(_WKAuthenticatorAttestationResponse *, NSError *))handler WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
- (void)getAssertionWithChallenge:(NSData *)challenge origin:(NSString *)origin options:(_WKPublicKeyCredentialRequestOptions *)options completionHandler:(void (^)(_WKAuthenticatorAssertionResponse *, NSError *))handler WK_API_AVAILABLE(macos(12.0), ios(15.0));
- (void)getAssertionWithClientDataHash:(NSData *)clientDataHash options:(_WKPublicKeyCredentialRequestOptions *)options completionHandler:(void (^)(_WKAuthenticatorAssertionResponse *, NSError *))handler WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
- (void)cancel;

// FIXME: <rdar://problem/71509848> Deprecate the following properties.
@property (nonatomic, readonly, copy) NSString *relyingPartyID;
@property (nonatomic, readonly, copy) NSSet *transports;
@property (nonatomic, readonly) _WKWebAuthenticationType type;
@property (nonatomic, readonly, copy, nullable) NSString *userName;

@end

NS_ASSUME_NONNULL_END
