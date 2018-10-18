/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import <NetworkExtension/NEFilterSource.h>

#else

typedef NS_ENUM(NSInteger, NEFilterSourceStatus) {
    NEFilterSourceStatusPass = 1,
    NEFilterSourceStatusBlock = 2,
    NEFilterSourceStatusNeedsMoreData = 3,
    NEFilterSourceStatusError = 4,
    NEFilterSourceStatusWhitelisted = 5,
    NEFilterSourceStatusBlacklisted = 6,
};

typedef NS_ENUM(NSInteger, NEFilterSourceDirection) {
    NEFilterSourceDirectionOutbound = 1,
    NEFilterSourceDirectionInbound = 2,
};

@interface NEFilterSource : NSObject
@end

@interface NEFilterSource (WKLegacyDetails)
+ (BOOL)filterRequired;
- (id)initWithURL:(NSURL *)url direction:(NEFilterSourceDirection)direction socketIdentifier:(uint64_t)socketIdentifier;
- (void)addData:(NSData *)data withCompletionQueue:(dispatch_queue_t)queue completionHandler:(void (^)(NEFilterSourceStatus, NSData *))completionHandler;
- (void)dataCompleteWithCompletionQueue:(dispatch_queue_t)queue completionHandler:(void (^)(NEFilterSourceStatus, NSData *))completionHandler;
@property (readonly) NEFilterSourceStatus status;
@property (readonly) NSURL *url;
@property (readonly) NEFilterSourceDirection direction;
@property (readonly) uint64_t socketIdentifier;
@end

#define NEFilterSourceOptionsPageData @"PageData"
#define NEFilterSourceOptionsRedirectURL @"RedirectURL"

typedef void (^NEFilterSourceDecisionHandler)(NEFilterSourceStatus, NSDictionary *);

@interface NEFilterSource (WKModernDetails)
- (id)initWithDecisionQueue:(dispatch_queue_t)queue;
- (void)willSendRequest:(NSURLRequest *)request decisionHandler:(NEFilterSourceDecisionHandler)decisionHandler;
- (void)receivedResponse:(NSURLResponse *)response decisionHandler:(NEFilterSourceDecisionHandler)decisionHandler;
- (void)receivedData:(NSData *)data decisionHandler:(NEFilterSourceDecisionHandler)decisionHandler;
- (void)finishedLoadingWithDecisionHandler:(NEFilterSourceDecisionHandler)decisionHandler;
- (void)remediateWithDecisionHandler:(NEFilterSourceDecisionHandler)decisionHandler;
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)
@property (copy) NSString *sourceAppIdentifier;
@property (assign) pid_t sourceAppPid;
#endif
@end

#endif // !USE(APPLE_INTERNAL_SDK)
