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

#if HAVE(SAFE_BROWSING)

#import <Foundation/Foundation.h>

#if USE(APPLE_INTERNAL_SDK)

#import <SafariSafeBrowsing/SafariSafeBrowsing.h>

#else

typedef NSString * SSBProvider NS_STRING_ENUM;

WTF_EXTERN_C_BEGIN

extern SSBProvider const SSBProviderGoogle;
extern SSBProvider const SSBProviderTencent;

WTF_EXTERN_C_END

@interface SSBServiceLookupResult : NSObject <NSCopying, NSSecureCoding>

@property (nonatomic, readonly) SSBProvider provider;

@property (nonatomic, readonly, getter=isPhishing) BOOL phishing;
@property (nonatomic, readonly, getter=isMalware) BOOL malware;
@property (nonatomic, readonly, getter=isUnwantedSoftware) BOOL unwantedSoftware;

@end

@interface SSBLookupResult : NSObject <NSCopying, NSSecureCoding>

@property (nonatomic, readonly) NSArray<SSBServiceLookupResult *> *serviceLookupResults;

@end

@interface SSBLookupContext : NSObject

+ (SSBLookupContext *)sharedLookupContext;

- (void)lookUpURL:(NSURL *)URL completionHandler:(void (^)(SSBLookupResult *, NSError *))completionHandler;

@end

#endif

#endif
