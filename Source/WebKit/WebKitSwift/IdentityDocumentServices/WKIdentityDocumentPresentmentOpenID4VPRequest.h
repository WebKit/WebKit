/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#import "WKIdentityDocumentPresentmentMobileDocumentRequest.h"
#import <Foundation/Foundation.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@interface WKIdentityDocumentPresentmentOpenID4VPTrustedAuthoritiesQuery : NSObject

@property (nonatomic, strong, readonly) NSString *type;
@property (nonatomic, strong, readonly) NSArray<NSString *> *values;

- (instancetype)initWithType:(NSString *)type values:(NSArray<NSString *> *)values NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

// A key, an array index, or, when both are nil, every element of an array.
@interface WKIdentityDocumentPresentmentOpenID4VPClaimPathComponent : NSObject

@property (nonatomic, strong, readonly, nullable) NSString *key;
@property (nonatomic, strong, readonly, nullable) NSNumber *index;

- (instancetype)initWithKey:(nullable NSString *)key index:(nullable NSNumber *)index NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

// Exactly one of the values is set.
@interface WKIdentityDocumentPresentmentOpenID4VPClaimValue : NSObject

@property (nonatomic, strong, readonly, nullable) NSString *stringValue;
@property (nonatomic, strong, readonly, nullable) NSNumber *integerValue;
@property (nonatomic, strong, readonly, nullable) NSNumber *booleanValue;

- (instancetype)initWithStringValue:(nullable NSString *)stringValue integerValue:(nullable NSNumber *)integerValue booleanValue:(nullable NSNumber *)booleanValue NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface WKIdentityDocumentPresentmentOpenID4VPClaimsQuery : NSObject

@property (nonatomic, strong, readonly, nullable) NSString *identifier;
@property (nonatomic, strong, readonly) NSArray<WKIdentityDocumentPresentmentOpenID4VPClaimPathComponent *> *path;
@property (nonatomic, strong, readonly) NSArray<WKIdentityDocumentPresentmentOpenID4VPClaimValue *> *values;
@property (nonatomic, strong, readonly, nullable) NSNumber *intentToRetain;

- (instancetype)initWithIdentifier:(nullable NSString *)identifier path:(NSArray<WKIdentityDocumentPresentmentOpenID4VPClaimPathComponent *> *)path values:(NSArray<WKIdentityDocumentPresentmentOpenID4VPClaimValue *> *)values intentToRetain:(nullable NSNumber *)intentToRetain NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface WKIdentityDocumentPresentmentOpenID4VPCredentialQuery : NSObject

@property (nonatomic, strong, readonly) NSString *identifier;
// The DCQL format identifier, or nil for a format the platform does not support.
@property (nonatomic, strong, readonly, nullable) NSString *format;
@property (nonatomic, readonly) BOOL allowsMultiple;
@property (nonatomic, strong, readonly, nullable) NSString *documentType;
@property (nonatomic, strong, readonly) NSArray<NSString *> *verifiableCredentialTypes;
@property (nonatomic, strong, readonly) NSArray<WKIdentityDocumentPresentmentOpenID4VPTrustedAuthoritiesQuery *> *trustedAuthorities;
@property (nonatomic, readonly) BOOL requiresCryptographicHolderBinding;
@property (nonatomic, strong, readonly) NSArray<WKIdentityDocumentPresentmentOpenID4VPClaimsQuery *> *claims;
@property (nonatomic, strong, readonly) NSArray<NSArray<NSString *> *> *claimSets;

- (instancetype)initWithIdentifier:(NSString *)identifier format:(nullable NSString *)format allowsMultiple:(BOOL)allowsMultiple documentType:(nullable NSString *)documentType verifiableCredentialTypes:(NSArray<NSString *> *)verifiableCredentialTypes trustedAuthorities:(NSArray<WKIdentityDocumentPresentmentOpenID4VPTrustedAuthoritiesQuery *> *)trustedAuthorities requiresCryptographicHolderBinding:(BOOL)requiresCryptographicHolderBinding claims:(NSArray<WKIdentityDocumentPresentmentOpenID4VPClaimsQuery *> *)claims claimSets:(NSArray<NSArray<NSString *> *> *)claimSets NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface WKIdentityDocumentPresentmentOpenID4VPCredentialSetQuery : NSObject

@property (nonatomic, strong, readonly) NSArray<NSArray<NSString *> *> *options;
@property (nonatomic, readonly) BOOL isRequired;

- (instancetype)initWithOptions:(NSArray<NSArray<NSString *> *> *)options isRequired:(BOOL)isRequired NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface WKIdentityDocumentPresentmentOpenID4VPVerifierIdentity : NSObject

@property (nonatomic, strong, readonly, nullable) NSString *clientIdentifierPrefix;
@property (nonatomic, strong, readonly) NSString *identifier;
@property (nonatomic, strong, readonly) NSArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *> *certificateChain;

- (instancetype)initWithClientIdentifierPrefix:(nullable NSString *)clientIdentifierPrefix identifier:(NSString *)identifier certificateChain:(NSArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *> *)certificateChain NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface WKIdentityDocumentPresentmentOpenID4VPRequest : NSObject

@property (nonatomic, strong, readonly) NSString *requestType;
@property (nonatomic, strong, readonly) NSArray<WKIdentityDocumentPresentmentOpenID4VPCredentialQuery *> *credentials;
@property (nonatomic, strong, readonly) NSArray<WKIdentityDocumentPresentmentOpenID4VPCredentialSetQuery *> *credentialSets;
@property (nonatomic, strong, readonly) NSArray<WKIdentityDocumentPresentmentOpenID4VPVerifierIdentity *> *verifierIdentities;

- (instancetype)initWithRequestType:(NSString *)requestType credentials:(NSArray<WKIdentityDocumentPresentmentOpenID4VPCredentialQuery *> *)credentials credentialSets:(NSArray<WKIdentityDocumentPresentmentOpenID4VPCredentialSetQuery *> *)credentialSets verifierIdentities:(NSArray<WKIdentityDocumentPresentmentOpenID4VPVerifierIdentity *> *)verifierIdentities NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // ENABLE(WEB_AUTHN)
