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

#pragma once

DECLARE_SYSTEM_HEADER

#if HAVE(SAFE_BROWSING)

#import <Foundation/Foundation.h>

#if 0 && USE(APPLE_INTERNAL_SDK)

#import <SafariSafeBrowsing/SafariSafeBrowsing.h>

#else
NS_ASSUME_NONNULL_BEGIN

typedef NSString * SSBProvider NS_STRING_ENUM;

WTF_EXTERN_C_BEGIN

extern SSBProvider const SSBProviderGoogle;
extern SSBProvider const SSBProviderTencent;
extern SSBProvider const SSBProviderApple;

WTF_EXTERN_C_END

@interface SSBServiceLookupResult : NSObject <NSCopying, NSSecureCoding>

@property (nonatomic, readonly) SSBProvider provider;

@property (nonatomic, readonly, getter=isPhishing) BOOL phishing;
@property (nonatomic, readonly, getter=isMalware) BOOL malware;
@property (nonatomic, readonly, getter=isUnwantedSoftware) BOOL unwantedSoftware;

@property (nonatomic, readonly) NSString *malwareDetailsBaseURLString;
@property (nonatomic, readonly) NSURL *learnMoreURL;
@property (nonatomic, readonly) NSString *reportAnErrorBaseURLString;
@property (nonatomic, readonly) NSString *localizedProviderDisplayName;
@property (nonatomic, readonly) NSString *localizedProviderShortName;

@end

@interface SSBLookupResult : NSObject <NSCopying, NSSecureCoding>

@property (nonatomic, readonly) NSArray<SSBServiceLookupResult *> *serviceLookupResults;

@end

@interface SSBLookupContext : NSObject

+ (SSBLookupContext *)sharedLookupContext;

- (void)lookUpURL:(NSURL *)URL completionHandler:(void (^)(SSBLookupResult *, NSError *))completionHandler;
- (void)lookUpURL:(NSURL *)URL isMainFrame:(bool)isMainFrame hasHighConfidenceOfSafety:(BOOL)hasHighConfidenceOfSafety completionHandler:(void (^)(SSBLookupResult *, NSError * _Nullable))completionHandler;
- (void)lookUpURL:(NSURL *)URL isMainFrame:(bool)isMainFrame hasHighConfidenceOfSafety:(BOOL)hasHighConfidenceOfSafety cachedResult:(SSBLookupResult * _Nullable)cachedResult completionHandler:(void (^)(SSBLookupResult *, NSError * _Nullable))completionHandler;
- (void)getListsForNamespace:(NSString *)listNamespace completionHandler:(void(^)(NSDictionary<NSString *, NSArray<NSString *> *> * _Nullable, NSError * _Nullable))completionHandler;

@end

NS_ASSUME_NONNULL_END

#endif

#endif
