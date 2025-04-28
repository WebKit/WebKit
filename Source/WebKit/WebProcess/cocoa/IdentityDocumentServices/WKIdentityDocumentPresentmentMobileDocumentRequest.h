/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <Security/Security.h>

NS_ASSUME_NONNULL_BEGIN

@interface WKIdentityDocumentPresentmentRequestAuthenticationCertificate : NSObject

@property (nonatomic) SecCertificateRef certificate;

- (instancetype)initWithCertificate:(SecCertificateRef)certificate NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface WKIdentityDocumentPresentmentMobileDocumentElementInfo : NSObject

@property (nonatomic) BOOL isRetaining;

- (instancetype)initWithIsRetaining:(BOOL)isRetaining NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest : NSObject

@property (nonatomic, strong) NSString *documentType;

@property (nonatomic, strong) NSDictionary<NSString *, NSDictionary<NSString *, WKIdentityDocumentPresentmentMobileDocumentElementInfo *> *> *namespaces;

- (instancetype)initWithDocumentType:(NSString *)documentType namespaces:(NSDictionary<NSString *, NSDictionary<NSString *, WKIdentityDocumentPresentmentMobileDocumentElementInfo *> *> *)namespaces NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest : NSObject

@property (nonatomic, strong) NSArray<NSArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *> *> *documentSets;

@property (nonatomic) BOOL isMandatory;

- (instancetype)initWithDocumentSets:(NSArray<NSArray<WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest *> *> *)documentSets isMandatory:(BOOL)isMandatory NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface WKIdentityDocumentPresentmentMobileDocumentRequest : NSObject

@property (nonatomic, strong) NSArray<WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest *> *presentmentRequests;

@property (nonatomic, strong) NSArray<NSArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *> *> *authenticationCertificates;

- (instancetype)initWithPresentmentRequests:(NSArray<WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest *> *)presentmentRequests authenticationCertificates:(NSArray<NSArray<WKIdentityDocumentPresentmentRequestAuthenticationCertificate *> *> *)authenticationCertificates NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
