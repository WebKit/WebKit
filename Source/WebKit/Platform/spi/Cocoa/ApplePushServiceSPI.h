/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if USE(APPLE_INTERNAL_SDK)

#import <ApplePushService/ApplePushService.h>

#else // if !USE(APPLE_INTERNAL_SDK)

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

WTF_EXTERN_C_BEGIN
extern NSString *const APSEnvironmentProduction;

typedef NS_ENUM(NSInteger, APSURLTokenErrorCode) {
    APSURLTokenErrorCodeInvalidArguments = 100,
    APSURLTokenErrorCodeXPCError = 101,
    APSURLTokenErrorCodeTopicSaltingFailed = 102,
    APSURLTokenErrorCodeTopicAlreadyInFilter = 103,
};
WTF_EXTERN_C_END

@class APSConnection;
@protocol APSConnectionDelegate;

@interface APSMessage : NSObject<NSCoding>
@property (nonatomic, retain) NSString *topic;
@property (nonatomic, retain) NSDictionary *userInfo;
@property (nonatomic, assign) NSUInteger identifier;
@end

@interface APSIncomingMessage : APSMessage
@end

#if HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)

@interface APSURLToken : NSObject<NSSecureCoding, NSCopying>
@property (nonatomic, strong) NSString *tokenURL;
@end

@interface APSURLTokenInfo : NSObject<NSSecureCoding, NSCopying>
- (instancetype)initWithTopic:(NSString *)topic vapidPublicKey:(nullable NSData *)vapidPublicKey;
@end

typedef void(^APSConnectionRequestURLTokenCompletion)(APSURLToken * __nullable token, NSError * __nullable error);
typedef void(^APSConnectionInvalidateURLTokenCompletion)(BOOL success, NSError * __nullable error);

#endif // HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)

@interface APSConnection : NSObject

- (instancetype)initWithEnvironmentName:(NSString *)environmentName namedDelegatePort:(nullable NSString *)namedDelegatePort queue:(dispatch_queue_t)queue;

#if HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)
- (void)requestURLTokenForInfo:(APSURLTokenInfo *)info completion:(APSConnectionRequestURLTokenCompletion)completion;
- (void)invalidateURLTokenForInfo:(APSURLTokenInfo *)info completion:(APSConnectionInvalidateURLTokenCompletion)completion;
#endif // HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)

@property (nonatomic, readwrite, assign, nullable) id<APSConnectionDelegate> delegate;

@property (nonatomic, readwrite, strong, setter=_setEnabledTopics:, nullable) NSArray<NSString *> *enabledTopics;
@property (nonatomic, readwrite, strong, setter=_setIgnoredTopics:, nullable) NSArray<NSString *> *ignoredTopics;
@property (nonatomic, readwrite, strong, setter=_setOpportunisticTopics:, nullable) NSArray<NSString *> *opportunisticTopics;
@property (nonatomic, readwrite, strong, setter=_setNonWakingTopics:, nullable) NSArray<NSString *> *nonWakingTopics;

- (void)setEnabledTopics:(NSArray<NSString *> *)enabledTopics ignoredTopics:(NSArray<NSString *> *)ignoredTopics opportunisticTopics:(NSArray<NSString *> *)opportunisticTopics nonWakingTopics:(NSArray<NSString *> *)nonWakingTopics;

@end

@protocol APSConnectionDelegate<NSObject>
- (void)connection:(APSConnection *)connection didReceivePublicToken:(NSData *)publicToken;
@optional
- (void)connection:(APSConnection *)connection didReceiveIncomingMessage:(APSIncomingMessage *)message;
@end

NS_ASSUME_NONNULL_END

#endif // !USE(APPLE_INTERNAL_SDK)
