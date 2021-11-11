/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(APPLE_PAY_AMS_UI)

#if USE(APPLE_INTERNAL_SDK)

#import <AppleMediaServices/AMSBag.h>
#import <AppleMediaServices/AMSBagConsumer.h>
#import <AppleMediaServices/AMSBagProtocol.h>
#import <AppleMediaServices/AMSEngagementRequest.h>
#import <AppleMediaServices/AMSEngagementResult.h>
#import <AppleMediaServices/AMSPromise.h>
#import <AppleMediaServices/AMSTask.h>

#else // if !USE(APPLE_INTERNAL_SDK)

NS_ASSUME_NONNULL_BEGIN

@interface AMSPromise<ResultType> : NSObject
- (void)addFinishBlock:(void(^)(ResultType _Nullable result, NSError * _Nullable error))finishBlock;
@end

@protocol AMSBagProtocol <NSObject>
@end

@interface AMSBag : NSObject <AMSBagProtocol>
@end

@protocol AMSBagConsumer <NSObject>
@optional
+ (AMSBag *)createBagForSubProfile;
@end

@interface AMSEngagementRequest : NSObject <NSSecureCoding>
@end

@interface AMSEngagementResult : NSObject <NSSecureCoding>
@end

@interface AMSTask : NSObject
- (BOOL)cancel;
@end

NS_ASSUME_NONNULL_END

#endif // !USE(APPLE_INTERNAL_SDK)

NS_ASSUME_NONNULL_BEGIN

@interface AMSEngagementRequest (Staging_84159382)
- (instancetype)initWithRequestDictionary:(NSDictionary *)dictionary;
@property (NS_NONATOMIC_IOSONLY, strong, nullable) NSURL *originatingURL;
@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(APPLE_PAY_AMS_UI)
